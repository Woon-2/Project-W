# 트리거 존(Zone) 시스템 — 설계 & As-Built

> CLAUDE.md "Major changes or design changes must be recorded in markdown documents" 지침에 따른 기록.
> 선행 인프라: 거점(Stronghold) 시스템(`RoomServer/docs/strongholdSystem.md`), 서버 지형 chunk(`serverTerrainChunk.md`).

## 1. Context

특정 지역 진입 시 보스전 발생 등 **연출 트리거**를 데이터 주도로 구성하기 위한 시스템.
- `MultiBoundingVolume.cs`처럼 **여러 입체(박스+구)의 조합**으로 존을 구성해 Unity에서 오서링.
- 추출 시 **대상 Faction을 복수 지정**(factionMask).
- 게임에서 지정 Faction이 진입한 상태의 **ZoneEnter/ZoneStay/ZoneLeave** 시점 동작을 지정.
- **동작 = 코드 콜백 + 태그**(이벤트 아님: 서버엔 EventBus가 없고, 보스 스폰/아레나 락은 Room 상태 동기 접근 필요).
- 범위: **서버 권위 게임플레이 존 + 클라 로컬 연출 존** 둘 다.

거점의 "Unity 마커 → `chunks_index.bin` 섹션 → 서버/클라 파서 → 런타임 → 프로토콜" 패턴을 재사용.
**핵심 원칙: 존은 물리 바디가 아니라 순수 질의 볼륨**(PhysicsWorld 미등록, contact 미생성).

## 2. 데이터 포맷 (`chunks_index.bin`, Stronghold 섹션 직후)

```
Int "ZoneCount" = Z
repeat Z:
  Head "Zone"
    Int   "Id"
    Text  "Tag"
    Int   "FactionMask"      // OR of factionBit(faction): Players=1<<1, Monsters=1<<2
    Int   "VolumeCount" = V
    repeat V:
      Int   "Shape"          // 0=Box, 1=Sphere
      Float "CenterX/Y/Z"    // world space
      Float "OrientX/Y/Z/W"  // box only
      Float "HalfX/Y/Z"      // box half extents
      Float "Radius"         // sphere
  Tail "Zone"
```
- **스트림 정렬 불변식:** 서버(`RoomServer/terrain.cpp`)·클라(`client/terrain.cpp`) 파서가 같은 위치·태그로 읽음.
- **데이터 의존:** 기존 `chunks_index.bin`은 Zone 섹션이 없으므로 **재추출 필수**(없으면 `ZoneCount` 읽기에서 스트림 깨짐). 거점 섹션 추가 때와 동일한 제약.

## 3. 공유 def — `common/zoneDef.hpp`

`ZoneShape{Box,Sphere}`, `ZoneVolumeDef{shape,center,orient,halfExtents,radius}`, `ZoneDef{id,tag,factionMask(uint32),volumes}`.
`mu::Vec3/NQuat`만 의존, `Faction` 비의존(raw 마스크) → 서버·클라 공용. 새 파일이라 **주석 영어만**(BOM/cp949 회피).

## 4. 서버 런타임 (RoomServer)

- **`terrain.{hpp,cpp}`**: `ChunkIndex::zones`, `TerrainChunkManager::zones()`. `parseChunkIndex`가 Stronghold 직후 Zone 섹션 파싱.
- **`zone.{hpp,cpp}`** (신규):
  - `Zone`: `contains(p)` = volume union point-in-OBB(`(~orient).rotate(p-center)` 후 `|local|≤half`) / point-in-sphere(`len2≤r²`). `factionAllowed(f)`, `armed`, `occupants`(object id set).
  - `ZoneHandler = std::function<void(Room&, Zone&, uint32 objId, Object* obj)>` (obj는 Leave 시 null 가능).
  - `ZoneSystem`: `build(defs)`, `on(tag, ZoneEvent, handler)`, `update(room, dt)`, `byId`.
  - `update`: armed zone마다 factionMask 후보(`getLivingPlayers()`의 player, mask에 Monsters면 `goblins()` 생존 개체) → `contains` → `newOcc`. 차분: enter=new\occ, stay=new∩occ, leave=occ\new. 핸들러 미바인딩 태그는 skip. **disarm 시 occupants clear**(재무장 시 안에 있으면 재진입 Enter).
- **`Room`**: 멤버 `ZoneSystem zoneSystem_`. `init()`에서 `build(worldTerrain_->zones())`+`bindZoneHandlers()`. `update()`에서 `physicsWorld_.step()` **직후** `zoneSystem_.update(*this, dtSec)`. 공개 접근자 `goblins()`/`resolveObject(id)` 추가. `bindZoneHandlers()`에 예시 핸들러("boss_arena_1" Enter → `S_ZoneState` broadcast + `setArmed(false)` + TODO 보스 스폰).
- **`PacketManager`**: `makeSZoneStatePacket(zoneId, state)`.

## 5. 프로토콜 — 신규 1개

`PacketType::S_ZoneState` + `SZoneStatePacket{ uint16 zoneId; uint8 state; }`. state 의미는 핸들러 정의.
보스 스폰은 **기존 `S_NpcRespawn` 재사용**(신규 패킷 최소화).

## 6. 클라이언트

- **`terrain.{hpp,cpp}`**: `ChunkIndex::zones` 추가, Stronghold와 달리 Zone 섹션을 **실제 파싱**(연출 존 사용). `TerrainChunkManager::zones()`.
- **`zone.{hpp,cpp}`** (신규): 경량 ZoneSystem. 후보 = **로컬 예측 플레이어 1개**, Zone은 bool `inside_`만 추적. `ZoneHandler = std::function<void(Zone&)>`. `update(playerPos)`: 핸들러 바인딩된 태그만 판정(서버 전용 태그 skip).
- **`online/onlineGame.{hpp,cpp}`**: 멤버 `clientZoneSystem_`, `zoneStates_`(S_ZoneState 캐시). `chunkManager_.init` 후 `build`+`bindZoneHandlers()`(연출 태그 예시 자리), update 루프에서 `chunkManager_.update` 직후 `clientZoneSystem_.update(player_->pos())`. `onZoneState(zoneId,state)`(현재 캐시; 배리어/컷신 연결은 확장 지점).
- **`PacketManager`**: `handleSZoneStatePacket` + dispatch case → `onZoneState`.

## 7. Unity 오서링

- **`ZoneMarker.cs`** (신규): `zoneTag`(Component.tag 회피), `[Flags] FactionFlags{Players=1<<1,Monsters=1<<2}`, `List<ZoneVolume>{shape,localCenter,size,rotationEuler,radius}`.
- **`ZoneEditor.cs`** (신규, `#if UNITY_EDITOR`): BVEditor 패턴 — 볼륨 리스트 편집 + 씬 핸들(박스 Position/Rotation/Scale+WireCube, 구 Position+RadiusHandle).
- **`TerrainExtractor.cs`**: Stronghold 직후 Zone 섹션 쓰기. 각 볼륨을 **월드 베이크**(center=`TransformPoint`, orient=`rotation*Euler`, half=`size*lossyScale*0.5`, radius=`radius*maxScale`).

## 8. 한계 / 주의

- **재추출 필수**(§2). 없으면 서버/클라 부팅 시 chunks_index 파싱 실패.
- **경계 떨림**: Enter/Leave는 집합 차분만; 히스테리시스/최소 체류시간 미구현(필요 시 추가).
- **사망/접속종료**: 서버 후보가 `getLivingPlayers()`(생존)라 사망 즉시 Leave; 접속종료 객체는 `resolveObject` null → obj=null Leave. "전원 진입" 게이트 거짓 충족 방지됨.
- **회전 존**: `contains`가 OBB narrow 판정하므로 회전 박스 정확. queryAABB 미사용(존·후보 소수라 직접 순회로 충분; 추후 최적화 여지).
- **N·M**: 몬스터 마스크 존은 매 틱 생존 고블린 전수 point-test — 존이 많고 몬스터가 많으면 존 AABB+broadPhase.queryAABB 최적화 고려.

## 9. 검증

- 빌드: `MSBuild RoomServer.vcxproj` / `client.vcxproj` Debug x64 **그린** (2026-06, RoomServer.exe + client.exe). 기존 무해 경고만(skillCompiler C4996, onlineGame C4244).
- 런타임(미완, 추출 데이터 필요): ZoneMarker 배치(예 "boss_arena_1", Players, 박스+구) → 추출 → 부팅 로그 zone 개수 → 존 진입 시 서버 콜백 발동 → 이탈/재진입 → 연출 태그 존은 클라 로컬 발동. 자동화 테스트 없음(수동).

## 10. 일반 마커(Marker) 채널 — Zone/Stronghold와 독립

벽 형상·보스 스폰 위치처럼 Zone/Stronghold가 과한 경량 배치는 **타입+이름+변환**만 추출하는 별도 채널을 둔다.
- **데이터(`chunks_index.bin`, Zone 섹션 직후):** `Int "MarkerCount"` → repeat `Head "Marker"` / `Text "Type"` / `Text "Name"` / `Float Pos*,Orient*,Scale*` / `Tail "Marker"`.
- **공유 def:** `common/markerDef.hpp` `MarkerDef{ type, name, pos, orient, scale }`(mu:: 의존, 플레인).
- **파싱:** 서버·클라 `parseChunkIndex` 모두 Zone 직후 파싱 → `ChunkIndex::markers` → `TerrainChunkManager::markers()`(양쪽). 스트림 정렬 불변식 동일.
- **Unity:** `LevelMarker.cs`(`markerType`/`markerName`, 변환은 transform). `TerrainExtractor`가 `FindObjectsByType<LevelMarker>`로 수집해 월드 TRS 기록(name 미입력 시 GameObject 이름).
- **소비:** 게임플레이 코드가 `markers()`를 type/name으로 필터해 사용(보스 스폰 좌표, 배리어 슬랩 등).
- **주의:** 기존 패턴과 동일하게 **재추출 필수**(Marker 섹션 없으면 파싱 깨짐).

## 11. As-Built — 중간보스 아레나 + 가상 벽 (2026-06)

존 `"midboss_arena"`(Players) 진입 시 집단 전투 시작 시나리오의 첫 통합.
- **서버(`Room`):** `bindZoneHandlers`가 `"midboss_arena"` Enter → `onMidbossArenaEnter`:
  - 디버그 로그(`[Zone] '...' ENTER by player N`).
  - `worldTerrain_->markers()`에서 `type=="Wall"` && name `WallHobgoblin_0`/`WallHobgoblin_1` → `spawnBarrierFromMarker`(cube 모델 Static 콜라이더, **id 없음·objectById_ 미등록·비네트워크**, `barriers_`에 보관, ~Room에서 unregister).
  - `type=="BossSpawn"` 마커 → 디버그 로그만(실제 스폰 TBD).
  - `S_ZoneState(zoneId, 1)` broadcast + `setArmed(false)`(1회성).
  - `assetManager_` backref를 init에서 캐시(런타임 배리어용 cube 모델).
- **클라(`Online::Game`):** `onZoneState(zoneId, 1)` → `chunkManager_.markers()`에서 같은 Wall 마커로 **로컬 Static Cube** 생성(`physicsWorld_.registerBody`, **렌더 안 함=가상의 벽**, `barriers_`). state==0 → 제거. 예측 로컬 플레이어가 벽에 막힘.
- **왜 양쪽:** online 플레이어 이동은 클라 예측+서버 검증이라, 실제 차단은 클라 배리어가, 권위/타 엔티티는 서버 배리어가 담당.
- **한계:** 중도 입장 클라는 S_ZoneState를 못 받아 벽 미생성(거점 중도입장 한계와 동일). zone 태그(`midboss_arena`)·마커 이름은 오서링과 정확히 일치해야 함.
  - ~~벽 제거(보스 처치 시) 미연결 — 현재 영구 유지.~~ → **해결됨(§12 참조).**
- **⚠️ 양방향 물리 벽(이 절)은 §13에서 후방 Wall 일방향 벽으로 대체됨** — 첫 진입자가 입구를 봉인해 다인 파티원이 입장 못 하던 버그 때문. 현재 `spawnBarrierFromMarker` 미호출(클라도 Cube 벽 미생성).

## 12. As-Built — 아레나 벽 해제 (전 NPC 처치 시, 2026-06)

전술 전투 종료 시 가상 벽을 내려 플레이어가 아레나 밖으로 후퇴할 수 있게 한다. §11의 "벽 제거 미연결" 한계 해소.
- **해제 조건:** 보스(`platoonLeader_`) 사망 **AND** 전 부대원(`tacticalNpcs_`) 사망. 보스가 먼저 죽고 부대원이 `Confused`로 살아남으면 그 전원을 마저 처치해야 해제(사용자 확정 정책).
- **감지 위치(`Room::updateTacticalAI` 말미):** `arenaWallsActive_ && allTacticalCombatantsDead()` → `teardownArenaWalls()`. 사망 NPC 시체는 `tacticalNpcs_`에 잔존해 매 틱 계속 검사되므로 별도 폴링 불필요.
- **`teardownArenaWalls()`:** 서버 `barriers_` 바디를 `physicsWorld_.unregisterBody` 후 clear(`~Room()`과 동일 패턴) + `S_ZoneState(activeArenaZoneId_, 0)` broadcast. **새 패킷·새 클라 코드 없음** — 클라 `onZoneState`의 기존 `state==0` 분기(`onlineGame.cpp`)가 로컬 벽을 제거.
- **활성 추적 상태(`Room`):** 진입 핸들러 3종이 `S_ZoneState(.,1)` 직후 `activeArenaZoneId_`/`arenaWallsActive_` 기록. 동시 인카운터 없음(진입 가드 + zone 1회성)이라 단일 상태로 충분. `arenaWallsActive_`는 해제 시 false로 내려 1회성 보장.
- **퇴화 데이터 안전:** 스폰 마커 부재로 인카운터 미생성(`platoonLeader_==nullptr`) 시 `allTacticalCombatantsDead()`가 false → 벽 유지(기존 동작과 동일).
- **§13 연동:** 물리 벽이 후방 Wall 일방향 벽으로 대체된 뒤에도 본 해제 로직은 그대로 — `teardownArenaWalls()`의 `S_ZoneState(.,0)`이 이제 "일방향 벽 off"를 의미해 전원 자유 후퇴를 허용한다(추가로 `arenaWalls_.clear()`). `barriers_` 정리 루프는 아레나 벽 미생성으로 no-op.

## 13. As-Built — 아레나 후방 Wall 일방향 벽(2026-06)

§11의 양방향 물리 벽은 Enter 트리거가 첫 진입자 1명에게 발동→즉시 봉인하므로 **2~4인 파티의 후발 주자가 입구에 막혀 입장 불가**한 버그가 있었다. → **일방향 벽**(들어오기 자유, 나가기 차단)으로 대체.

**1차 시도(폐기):** 경계를 트리거 Zone 볼륨으로 삼음. 그러나 `Arena_*` Zone은 *트리거*용으로 작게 author돼, 플레이어가 트리거 영역에 갇혀 **전투 구역까지 못 가는** 버그(런타임 확인). → Zone-경계 폐기, 아래 Wall-기반으로 전환.

**확정 기하(사용자 검증):** Hobgoblin 아레나는 **코리도형**으로 벽 마커가 **양 끝에 2개**(`WallHobgoblin_0`/`_1` = 입구·출구). 안쪽(전투 구역)은 두 벽 사이.

- **방식:** 각 후방 Wall 마커를 **수평(XZ) 일방향 슬랩**으로 변환. 두 벽 모두 "바깥(중점에서 멀어지는 쪽)으로 나가기"만 차단 → 플레이어는 두 벽 사이에 갇히고(전투 구역 포함) 입장·전진·측면은 자유. felt collision은 클라 예측(`Game::resolveArenaWallLeash`), 권위는 서버(`Room::move()`)가 미러.
- **interior 기준점 = 두 벽 중점**(`wallSum/wallCount`, 핸들러가 스폰 fallback으로도 쓰는 값). 코리도라 중점이 두 벽 사이 = 확실한 안쪽. outward = `dot(normal, center-mid) >= 0 ? normal : -normal`.
- **일방향(stateless, old→new):** old가 안쪽이고 new가 footprint 안에서 바깥으로 넘어가면 평면으로 클램프(XZ만, **Y 보존**). 밖→안(입장)·측면 통과. committed 래치 불필요(횡단 방향으로 일방향 성립).
- **공유 수학(`common/arenaWall.hpp`, 신규):** `OneWayWall{center,outward,widthDir,halfWidth}` + `makeOneWayWall(marker, interiorRef)`(얇은 수평축=법선, scale*0.5=half) + `clampOneWayWall(old,new,wall,radius)`.
- **서버:** 아레나 핸들러 3종이 Wall 마커 중점 계산 후 `arenaWalls_`(슬랩)에 캐시(+슬랩 outward/halfWidth 로그). `Room::move()` 텔레포트 클램프 직후 `arenaWallsActive_ && !isMoveClampExempt()`이면 각 슬랩에 `clampOneWayWall`. `teardownArenaWalls()`가 `arenaWalls_.clear()`.
- **클라(`Online::Game`):** `onZoneState(.,1)`이 zone tag(`Arena_X`) → Wall prefix(`WallX`) 도출 → 해당 마커들로 슬랩 빌드·캐시(`arenaWalls_`)(Hobgoblin 전용 `isHobgoblinBarrierMarker` 한계도 일반화). `resolveArenaWallLeash`를 물리 step 루프 **뒤** 프레임당 1회 호출(직전 프레임 `arenaPrevPlayerPos_` 대비, `setCurrPos`+`moveChange_`). 장식용 마법진은 유지.
- **신규 패킷 없음:** 기존 `S_ZoneState`(1=벽 on, 0=off) 재사용. §12 해제(전 NPC 처치)가 그대로 연동(0 → 양측 `arenaWalls_` clear).
- **⚠️ 부호 검증:** 진입 시 서버·클라가 각 슬랩 outward를 로그. 전진이 막히고 후퇴가 되면 outward 부호 반대 → interior 기준점 재확인.
- **빌드:** RoomServer + client Debug x64 그린(2026-06, 오류 0; 클라 기존 무해 C4244 3건 외 경고 0).

# 거점(Stronghold) 시스템 — 설계 & As-Built

> CLAUDE.md "Major changes or design changes must be recorded in markdown documents" 지침에 따른 기록.
> 선행 인프라: 서버 지형 chunk 전환(`RoomServer/docs/serverTerrainChunk.md`).

## 1. Context

구조물(거점) 주위 랜덤 위치에 몬스터가 스폰되고, 거점은 체력을 가져 플레이어가 파괴할 수 있으며,
파괴 후 일정 시간이 지나면 재건된다. 몬스터·거점은 **서버 권위**로 관리되고 클라이언트는 표현만 한다.
스폰 Y는 서버의 `Level::terrainChunks.heightAtWorld(x,z)`로 지면에 스냅한다.

## 2. 확정 결정
1. **데이터 위치:** 거점 정의를 `chunks_index.bin`의 신규 Stronghold 섹션에 저장. 서버가 권위로 소비, 클라 파서는 consume-and-discard.
2. **몬스터 생명주기 = 고정 풀 재부활:** 풀 크기 = 적정 개체수. 죽은 개체를 거점이 `respawnInterval`·`maxPerWave`에 따라 **새 랜덤 위치로 부활**. 기존 `S_NpcRespawn` 재사용(신규 몬스터 패킷/클라 팩토리 없음).
3. **거점 = 전투 타깃:** 플레이어 스킬이 직접 때려 HP 감소. HP≤0 → 파괴(부활 중단), `respawnDelay` 후 재건.
4. **파괴 시 소속 몬스터 유지**(부활만 중단). 구조물은 **placeholder(cube)** + **충돌 장애물**(Static body+collider).

## 3. 데이터 포맷 (`chunks_index.bin`, ChunkIndex tail 직전)
```
Int "StrongholdCount" = S
repeat S:
  Head "Stronghold"
    Int   "Id"
    Float "CenterX/Y/Z"  "OrientX/Y/Z/W"  "ScaleX/Y/Z"
    Float "ActivityRadius" "SpawnRadius"
    Int   "MaxHp"
    Float "RespawnDelaySec"
    Int   "PopulationCount" = P
    repeat P: Int "MonsterType" "TargetCount" "MaxPerWave"; Float "RespawnIntervalSec"
  Tail "Stronghold"
```
- 추출기: `client/unityScripts/StrongholdMarker.cs`(신규 컴포넌트) + `TerrainExtractor.cs`가 `FindObjectsByType<StrongholdMarker>`로 수집해 섹션 기록. id = export 인덱스.
- 서버 `parseChunkIndex`(`RoomServer/terrain.cpp`)가 `ChunkIndex::strongholds`로 저장 → `TerrainChunkManager::strongholds()`. 클라 `client/terrain.cpp parseChunkIndex`는 동일 태그를 읽고 버림(스트림 정렬).

## 4. 서버 런타임
- **`strongholdDef.hpp`**(신규, 플레인 데이터): `PopulationDef`, `StrongholdDef`. `terrain.hpp ChunkIndex`에 포함.
- **`Stronghold`**(`stronghold.{hpp,cpp}`, `: public Object`): 전투 타깃 + 인구 관리자.
  - `configure(def, groupId, poolStart, poolCount)`; Goblin population 파라미터(maxPerWave/interval) resolve.
  - `updatePopulation(dt, pool, room, outRevivedIds)`: 미파괴 시 interval마다 범위 내 죽은 고블린을 maxPerWave까지 `reviveAt(랜덤XZ, Y=heightAtWorld)`. 부활 id 반환.
  - `updateStructure(dt)`: hp≤0 → destroyed(부활 중단); destroyed면 rebuildTimer, `≥respawnDelay` → hp 복구·재건. 상태 전이 시 true.
- **`Npc`**(`Npc.{hpp,cpp}`): 개체별 self-respawn 제거(`updateDead`는 Dead 유지만). 신규 `reviveAt(pos)`(hp 복구 + spawnPos 갱신 + snapToCurrent + Idle). `NpcUpdateResult::respawned` 제거.
- **`Room`**(`Room.{hpp,cpp}`): `init(const Level*)` **시그니처 불변**.
  - `worldTerrain_ = &levelData->terrainChunks`. `goblins_` 풀 = Σ(Goblin targetCount), `strongholds_` 벡터. **reserve 후** 등록(주소 안정 불변식).
  - 거점별: NpcGroup 생성, 고블린 풀 구간 생성(`setupGoblin` + 랜덤 위치+Y스냅 + spawnPos/activityZone/groupId), `Stronghold` 생성(`setupStronghold`: placeholder 모델/Faction::Monsters/canReceiveDamage/Static).
  - `updateGoblinAI`: 고블린 update 후 거점 루프 — `updateStructure`(→`S_StrongholdState`), `updatePopulation`(→부활 id별 `S_NpcRespawn`). `result.respawned` 경로 제거.
  - `enter`: ObjectInfo 리스트에 거점 포함(type=Stronghold). 신규 `groundHeightAtWorld`/`randomSpawnInDisc`.
- **`Level`/`AssetManager`**: Level의 `goblins`/`goblinSpawners`/`GoblinSpawnerInfo`/`importGoblinSpawner`/`GoblinSpawner` 노드 제거. Level은 `const AssetManager* assetManager` backref 1개만 보유(`loadLevelFromFile`이 설정) → Room이 이를 통해 `modelGoblin()`/`goblinAnimations()`/`modelCube()` 접근(placeholder=cube). Room.cpp는 `AssetManager.hpp` include.

## 5. 프로토콜 (`ServerEngine/protocol.hpp`, `PacketManager`)
- `ObjectType::Stronghold` 추가. `PacketType::S_StrongholdState` + `SStrongholdStatePacket{ uint16 strongholdId; int32 hp; uint8 state /*0=Alive,1=Destroyed*/ }`.
- 서버 `makeSStrongholdStatePacket`. **HP 감소는 기존 `S_SkillHit`/`S_Hit`(targetId) 재사용**, 몬스터 부활은 `S_NpcRespawn` 재사용.

## 6. 클라이언트
- `terrain.cpp parseChunkIndex`: Stronghold 섹션 consume-discard.
- `PacketManager.cpp`: `S_Enter`에 `ObjectType::Stronghold → createStronghold`; `S_StrongholdState` 핸들러.
- `online/onlineGame.{hpp,cpp}`: `strongholds_`(placeholder Cube) + `strongholdHpBars_`. `createStronghold`(cube 모델, HP바), `onStrongholdState`(파괴/재건 + max-seen HP 학습), `applyHit`에 거점 라우팅. update/render 루프 + HP바 화면 투영 추가. 파괴 시 렌더 스킵.

## 7. 한계 / 주의
- ~~**중도 입장 HP 동기화:** `ObjectInfo`에 hp 없음 → 입장 시 거점/몬스터 풀피로 표시(기존 한계). 클라는 패킷 hp로 max-seen 학습.~~ → **§10에서 해소**(ObjectInfo/PlayerInfo에 hp/maxHp 추가).
- **풀 주소 안정성:** `goblins_`/`strongholds_` 등록 전 `reserve` 필수.
- **다종 대비:** 데이터/프로토콜은 `ObjectType` 키로 다종 가능. 실제 스폰·AI·모델은 **Goblin만**. 추가 시 데이터 + 클라 팩토리 분기 + Config.
- **데이터 의존:** chunks_index에 Stronghold 섹션이 추출돼 있어야 함. 없으면 strongholds 0개(서버 정상). 구조물 모델은 placeholder cube — 전용 에셋은 추후 교체.

## 8. 검증
- 빌드: `MSBuild GameServer.sln /t:RoomServer;client Debug x64` **그린**(2026-06, RoomServer.exe + client.exe).
- 런타임(미완, 추출 데이터 필요): 거점+고블린 추출 → 부팅 로그(거점/풀) → 클라 접속 시 구조물+고블린 표시(지형 위) → 고블린 처치 후 부활(인구 유지) → 거점 공격→파괴→재건. DummyClient/수동(자동화 테스트 없음).

## 9. As-Built — 테스트 중 수정·결정 (2026-06)

§4–6 계획 위에 실제 통합하며 내린 수정/결정. 아래는 함정(gotcha) 포함.

- **거점 id 부여 필수(크래시 수정):** `Room::init`에서 거점도 `setId(IdPool::pop())` 해야 함. 누락 시 id=-1로 `registerObject`가 `objectById_[-1]` 기록 → 힙 손상(룸 생성/스킬 컴파일 단계에서 크래시). 거점이 1개라도 있어야 재현. `~Room()`에서 거점 id도 `IdPool::push`.
- **고블린 `canReceiveDamage` 함정:** 풀을 `Goblin g{}`(기본 생성자)로 만들면 `Npc(Object&&,cfg)`의 `setCanReceiveDamage(true)`가 호출되지 않아 **스킬 시스템 타깃에서 제외**됨(레거시 `Room::attack`은 그 체크 없이 때려서 "스킬엔 안 맞고 레거시엔 맞는" 증상). → `setupGoblin`에서 `g.setCanReceiveDamage(true)` 명시.
- **`Level` → AssetManager backref:** Level이 개별 에셋 포인터 대신 `const AssetManager* assetManager`(loadLevelFromFile이 설정)만 보유. Room이 `levelData->assetManager->modelGoblin()/goblinAnimations()/modelCube()`로 접근. Room.cpp는 `AssetManager.hpp` include.
- **거점 placeholder 형상/배치:** `setupStronghold`가 authored `sd.scale` 대신 고정 세로 막대 스케일 `(1.5,5,1.5)` 사용. 큐브 pivot이 중심이라, groundY에 놓고 **월드 BVH 높이 절반만큼 상승**시켜 바닥면을 지면에 세움(`AABB.size`=full extent). + **바닥을 0.5m 지면 아래로 묻어**(coplanar z-fighting 줄무늬 방지). 상승된 pos·스케일은 ObjectInfo로 전송돼 클라 시각 자동 일치.
- **클라 거점 충돌:** 클라는 거점을 렌더만 하면 로컬 player(클라 예측)가 통과함 → `createStronghold`에서 거점을 **클라 `physicsWorld_`에 Static body 등록**(setMotionType(Static)+snapToCurrent+registerBody). 큐브 모델 BVH가 충돌 형상 제공.
- **거점 HP바 타이머:** 고블린처럼 피격 후에만 잠깐 표시. `StrongholdHpEntry.hpBarVisibleSeconds`(applyHit에서 5s 설정, 프레임마다 감소, ≤0/파괴 시 숨김).
- **거점 데미지 넘버 누락 수정 [UI]:** (초기 인라인 수정은 §10 EventBus 재설계로 대체됨) 데미지 넘버는 `update()`의 EvHit/EvDeath 디스패치 루프에서만 생성되는데, 그 루프의 `resolveObject`가 거점을 못 찾고(`continue`) 거점은 EventBus가 없어 이벤트 자체가 안 만들어져 → 거점 피격 시 데미지 UI 미표시였다. **현재 해법은 §10 참조**(거점을 EventBus 보유 `Stronghold` 클래스로 만들고 `resolveObject`에 추가 → 고블린과 동일 경로로 데미지 넘버 생성).
- **그림자 acne(줄무늬) 수정 [렌더링]:** 정적 메시 CSM 그림자 PSO `createShadowMapCSMShader`(`shader.cpp`)를 **`CullMode=FRONT`(앞면 컬링)**로 변경 → 그림자맵에 뒷면 깊이 기록, 빛 받는 앞면이 자기 그림자 안 받음(닫힌 솔리드 메시 acne 표준 해법). 이 PSO는 정적 deferred 메시 전용(현재 거점뿐)이라 캐릭터(스킨드 PSO)·지형 그림자 불변. 향후 얇은/열린 정적 캐스터 추가 시 재검토. [[project_csm_shadow]] 참조.

## 10. enter 시점 HP 동기화 + 거점 EventBus 통합 (2026-06)

거점 피격 시 데미지 넘버 미표시 + 중도 입장 HP 풀피 오표시(§7)를 함께 해결한 설계 변경. 빌드 그린(RoomServer.exe + client.exe).

### 10.1 enter 시점 HP 전송 (전 객체, 플레이어 포함)
- **프로토콜**(`protocol.hpp`): `ObjectInfo`·`PlayerInfo`에 `int32 hp; int32 maxHp;` 추가. `constexpr int32 kPlayerMaxHp = 100;`(클라/서버 단일 출처).
- **서버**: `Room::enter`가 플레이어를 `setHp(kPlayerMaxHp)`로 권위 초기화(기존 base `Object` 기본 hp_=1,000,000) → **플레이어가 100 HP로 죽을 수 있게 됨**(의도된 동작 변경). 전 객체 ObjectInfo에 hp/maxHp 채움(플레이어=`hp()`/`kPlayerMaxHp`, 고블린=`hp()`/`Npc::maxHp()`[신규 getter], 거점=`hp()`/`strongholdMaxHp()`). `makeSEnterPacket` 복사부에 hp/maxHp 추가. `PlayerInfo`는 통째 복사라 `S_Enter myInfo`·`S_Enter_Other` 자동 전파.
- **클라**: `setupPlayer`/`createOtherPlayer`(ObjectInfo·PlayerInfo 2종)/`createGoblin`/`createStronghold`의 하드코딩 HP(100/90/1) → `info.hp`/`info.maxHp`. 거점 "max-seen 학습" 해킹 제거. → **§7 중도 입장 한계 해소**(이미 깎인 대상도 정확한 부분 HP로 표시).

### 10.2 거점을 EventBus 보유 Object 클래스로
- **`client/object.hpp`**: 신규 `class Stronghold : public Object` + 중첩 `EventBus`(`Goblin`/`Player` 패턴). **AnimBlender 없음**(`setAnimBlender` 미오버라이드 → 애니메이션 안 함). 기존 placeholder는 `Cube`였음(`Cube`는 standalone 등에서 계속 사용하므로 유지).
- **`client/object.cpp`** `Stronghold::EventBus::receive`: 고블린 핸들러에서 AnimBlender 포워딩·래그돌만 제거 — `Hit`→`hp_=max(hp,0)`, `Death`→`if(!isDead_){isDead_=true;hp_=0;}`(멱등 가드), `Respawn`→`isDead_=false`.
- **`onlineGame.hpp`**: `strongholds_` `vector<shared_ptr<Cube>>`→`vector<shared_ptr<Stronghold>>`, `StrongholdHpEntry.obj` `Cube*`→`Stronghold*`, `bool destroyed` 제거(파괴상태=`obj->isDead()`로 단일화).

### 10.3 Hit/데미지/HP바 통합 경로 (`onlineGame.cpp`)
- `applyHit`: 거점 전용 인라인 분기 삭제 → 고블린/거점 HP바 가시성 5s 설정 후 `EvHit`/`EvDeath` 발행(통합). 데미지 넘버는 디스패치 루프가 생성.
- 디스패치 루프 `resolveObject`에 거점 추가(`strongholdHpBars_`에서 `obj` 반환, `Stronghold*`→`Object*` 업캐스트) → 데미지 넘버·HP갱신이 고블린과 동일 경로. 킬카운트는 `idGoblinMap_` 체크라 거점 자동 제외.
- `onStrongholdState`: 상태 전이만 처리(전투 HP는 S_SkillHit/S_Hit). `state=1`(파괴)은 **setHp 호출 없이** `EvDeath`만 발행 — **함정**: 여기서 `setHp(0)`을 하면 같은 프레임 디스패치 전에 `prevHp`가 0이 되어 막타 데미지 넘버가 0으로 계산돼 사라진다. `state=0`(재건)은 `setHp(full)`+`EvRespawn`.
- 렌더/HP바 투영 루프: `entry.destroyed` → `obj->isDead()`.

### 10.4 동시성/엣지케이스
- **이중 EvDeath**: 막타 `S_SkillHit(≤0)`→`EvDeath`와 `S_StrongholdState(state=1)`→`EvDeath`. 핸들러 `if(!isDead_)` 가드로 멱등. TCP 순서상 hit 먼저 → 데미지 넘버는 첫 EvDeath(applyHit)에서만(prevHp 보존), 둘째는 hp=0/isDead라 dmg≤0·가드로 무시. §10.3대로 onStrongholdState가 setHp(0)을 안 하므로 prevHp 안전.
- **포인터 안정성**: `strongholds_`는 shared_ptr 벡터, `StrongholdHpEntry.obj`/resolveObject 반환은 raw지만 shared_ptr가 수명 보장.

## 11. 몬스터 스폰 시 prop 겹침 회피 (결정론적 bounded retry, 2026-06)

기존 `randomSpawnInDisc`/`Stronghold::randomSpawnPos`는 디스크 균등 샘플 후 Y만 지면에 스냅할 뿐, scatter prop(나무/바위 등)과의 겹침을 전혀 검사하지 않아 몬스터가 prop 메시 속에 박힌 채 스폰될 수 있었다. 물리 스텝마다 `ScatterCollider`(§ [[project_scatter_system]])가 사후에 밀어내긴 하지만(`staticDepenetration`), 스폰 순간의 겹침 자체는 막지 못했다.

- **신규 질의 API**: `WorldCollider::overlapsBVH(const BVH&)`(기본 false, `collision.hpp`) — body 없이 "이 world-BVH가 닿는가"만 묻는 경량 질의. `ScatterCollider::overlapsBVH`가 기존 `forEachCandidate` grid + `collides(BVH,BVH)`를 재사용해 구현(`collision.cpp`). `PhysicsWorld::overlapsAnyScatterProp(pos, BVH)`가 등록된 모든 `worldColliders_`를 `footprintReject`로 먼저 거르고 질의(`physicsWorld.hpp/.cpp`).
- **Bounded retry 스폰**: `Room::randomSpawnInDiscAvoidingProps(center, radius, const Object& footprintSource)`(`Room.hpp/.cpp`, public)가 `randomSpawnInDisc`를 **최대 8회(kMaxSpawnAttempts, 고정 상수)** 재시도하며, 매 시도마다 `footprintSource`의 `model()`/`body().orient()`/`body().scale()`로 후보 위치의 가상 world-BVH를 `makeWorldBVH`(기존 scatter 베이킹 함수 재사용)로 만들어 `overlapsAnyScatterProp`로 검사. 겹치지 않는 첫 후보를 즉시 채택, 8회 모두 실패하면(드묾) 마지막 후보로 fallback — 잔류 겹침은 기존 `staticDepenetration`이 다음 몇 스텝 내 자동 해소하므로 영구 박힘은 없다.
- **결정론적 시간**: 매 시도 비용(RNG 샘플 O(1) + 높이 조회 O(1) + `makeWorldBVH`<로컬 BVH 노드 수> + `overlapsAnyScatterProp`<grid-bounded 후보만>)이 prop 밀도/씬 규모와 무관한 상수이므로, 전체 비용은 항상 "8 × 상수"로 상한 고정. "겹치지 않을 때까지 무한 재시도" 방식이 아니다.
- **호출부**: `Room::init`의 `spawnMonster`(초기 스폰)와 `Stronghold::updatePopulation`의 `tryRevive`(리바이브) 양쪽 모두 `randomSpawnInDiscAvoidingProps`를 사용. 초기 스폰에서도 검사가 동작하도록 `registerScatterColliders` 호출을 `Room::init` 앞부분(거점 스폰 루프 이전)으로 이동(기존엔 terrain chunk 등록 이후라 초기 스폰 시점엔 콜라이더가 비어있었음).
- **단순화**: `Stronghold::randomSpawnPos`(자체 `s_strongholdRng`로 동일 디스크 샘플링을 중복 구현하던 private 메서드)를 삭제하고 `room.randomSpawnInDiscAvoidingProps`를 직접 호출하도록 통합.
- **범위**: scatter prop만 검사 대상(몬스터-몬스터/몬스터-플레이어 겹침은 의도적으로 허용, 검사 경로 자체에 없음). 거점 구조물 자체(Static body, `WorldCollider` 경로 아님)와의 겹침은 이번 범위 밖.

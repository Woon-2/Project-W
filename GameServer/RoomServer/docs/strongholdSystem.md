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
- **중도 입장 HP 동기화:** `ObjectInfo`에 hp 없음 → 입장 시 거점/몬스터 풀피로 표시(기존 한계). 클라는 패킷 hp로 max-seen 학습.
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
- **그림자 acne(줄무늬) 수정 [렌더링]:** 정적 메시 CSM 그림자 PSO `createShadowMapCSMShader`(`shader.cpp`)를 **`CullMode=FRONT`(앞면 컬링)**로 변경 → 그림자맵에 뒷면 깊이 기록, 빛 받는 앞면이 자기 그림자 안 받음(닫힌 솔리드 메시 acne 표준 해법). 이 PSO는 정적 deferred 메시 전용(현재 거점뿐)이라 캐릭터(스킨드 PSO)·지형 그림자 불변. 향후 얇은/열린 정적 캐스터 추가 시 재검토. [[project_csm_shadow]] 참조.

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

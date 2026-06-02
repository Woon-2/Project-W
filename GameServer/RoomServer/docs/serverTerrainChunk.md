# 서버 지형 Chunk 전환 — 설계 & As-Built

> CLAUDE.md "Major changes or design changes must be recorded in markdown documents" 지침에 따른 기록.
> 클라이언트 Chunk 스트리밍 문서: `client/docs/terrainChunkStreaming.md`.

## 1. Context

클라이언트는 단일 Terrain → 다중 Chunk **실시간 스트리밍**으로 전환했지만(`client/docs/terrainChunkStreaming.md`),
서버(RoomServer)는 단일 terrain(`loadTerrainHeightFieldFromFiles` + `terrain_manifest.bin`/`terrain_meta.bin`)에
머물러 있었다. 향후 거점(monster spawner) 시스템이 **서버 권위로 몬스터를 지형 위에 스폰**하려면
서버가 **월드 전역의 지형 높이를 질의**할 수 있어야 하고, 월드가 다중 chunk이므로 서버도 chunk 기반으로
전환해야 한다.

## 2. 전략 결정 (점검 결론)

**서버는 클라이언트처럼 스트리밍하지 않는다.**

- 스트리밍의 비용 동인(GPU 자원/텍스처/디스크립터/CommandList/draw call)이 서버엔 **존재하지 않음**.
- 서버가 chunk에서 쓰는 건 CPU `TerrainHeightField`(`vector<float>`)뿐 — 청크당 ~0.26~1MB로 작음.
- 몬스터·거점은 **항상 존재**하므로 그들이 선 지형은 항상 쿼리 가능해야 함. 언로드는 곧 버그(Y=0 추락).
  스트리밍 전제(플레이어 주변만 유지)와 충돌.

**확정 결정**
1. 실시간 로드/언로드 **없음** — 부팅 시 전 chunk heightfield를 1회 로드해 전부 상주.
2. heightfield 데이터는 **read-only 단일 인스턴스를 전 룸 공유**(룸별 복사 제거).
3. 각 룸은 init에서 **전 chunk collider를 일괄 등록**(동적 토글 없음).
4. **클라이언트의 클래스/함수 명명·구조를 최대한 미러링**(GPU/스트리밍/카메라 부분만 제거).

## 3. 공유 vs 룸별 소유

| 계층 | 소유자 | 내용 |
|------|--------|------|
| 공유(부팅 1회, read-only) | `Level::terrainChunks` (`TerrainChunkManager`) | `ChunkIndex` + `unordered_map<int64, TerrainHeightField>` + 셀 크기 + 월드 라우팅 |
| 룸별(저렴) | `Room::terrainChunks_` (`vector<TerrainObject>`) | chunk당 정적 body(pos=worldOffset) + collider. heightField는 공유를 **포인터 참조** |

**`TerrainChunkManager`는 `Level`이 멤버로 소유한다.** `RoomManager`는 기존처럼 `const Level* pLevel_`
하나만 전 룸에 공유하므로(`Room::init(const Level*)` 시그니처 불변), 별도의 인터페이스 추가 없이
지형 매니저도 Level을 통해 함께 공유된다. `Level` 인스턴스는 `AssetManager::level_` 단 하나뿐이라
heightfield 데이터는 룸 수와 무관하게 1벌만 존재한다.

## 4. 명명 미러링 (client → server)

| 클라이언트 | 서버 | 비고 |
|-----------|------|------|
| `ChunkIndexEntry` / `ChunkIndex` | 동일(서버는 `palette` 필드 생략) | |
| `parseChunkIndex(terrainDir)` | 동일 시그니처 | palette 태그는 **읽되 버림**(스트림 정렬). index 파일 부재 시 빈 결과(지형 비활성, 안전) |
| `packCoord(col,row)` = `(int64)col<<32 \| (uint32)(int32)row` | 동일 | 부호 있는 임의 좌표 |
| `worldOffset(col,row)` = `(col·sizeX, 0, row·sizeZ)` | 동일 | |
| `chunkCoordAtWorld` / `heightAtWorld` / `normalAtWorld` | 동일 | 월드 라우팅 → 해당 chunk hf 로컬 질의 |
| `TerrainChunkManager` (streaming) | **load-all 변종**(동명) | `init`/`heightAtWorld`/`normalAtWorld`/`chunkCoordAtWorld`/`empty`/`chunkCount`/`forEachChunk`. 스트리밍·GPU·카메라 API 없음 |
| `PhysicsWorld::TerrainEntry/TerrainHandle/registerTerrain/unregisterTerrain` | 동일 | slot 재사용 |
| `TerrainCollider` (collision.*) | **무수정** | 이미 클라와 동일. testVertex가 `terrainBody_->pos()`를 origin으로 로컬 reject |

> 신규: `loadChunkHeightField(entry, terrainDir)` — 클라의 `buildChunkCpu` 중 height 파싱 부분만 추출한 CPU 전용 로더.

## 5. 변경된 파일

| 파일 | 변경 |
|------|------|
| `terrain.hpp`/`terrain.cpp` | `ChunkIndexEntry`/`ChunkIndex`/`parseChunkIndex`/`loadChunkHeightField`/`TerrainChunkManager` 추가. 단일 경로(`parseManifest`/`parseMeta`/`TerrainManifest`/`TerrainMeta`/`loadTerrainHeightFieldFromFiles`) 제거 |
| `object.hpp` | `TerrainObject`: `TerrainHeightField` 소유 → `const TerrainHeightField*` 참조(+`setHeightField`) |
| `physicsWorld.hpp`/`.cpp` | 단일 `terrainCollider_` → `vector<TerrainEntry> terrains_` + `freeTerrainSlots_`. `registerTerrain`→`TerrainHandle` 반환, `unregisterTerrain(handle)`. `generateContacts` terrain 루프를 `for(terrains_)` + `kPad=4` XZ reject |
| `Level.hpp` | `TerrainChunkManager terrainChunks` 멤버 추가(Level이 소유) |
| `AssetManager.cpp` | `loadAssets()`에서 level 로드 후 `level_.terrainChunks.init("../resources/terrains/")` |
| `RoomManager`/`roomServerMain` | **변경 없음** — 기존 `const Level*` 공유 그대로 |
| `Room.hpp`/`.cpp` | `TerrainObject terrain_` → `vector<TerrainObject> terrainChunks_`. **`init(const Level*)` 시그니처 불변**. `levelData->terrainChunks`에서 **`reserve(chunkCount)` 후** `forEachChunk`로 chunk별 정적 body+collider 일괄 등록 |
| `Level.hpp`/`.cpp` | `TerrainObject terrain` 필드 + `importTerrain()` 제거. `type=="Terrain"` 노드는 레거시 `ManifestPath` consume만 |

## 6. 불변식 / 주의사항

- **body 포인터 안정성:** `Room::terrainChunks_`는 `registerTerrain(&t.body(), ...)`로 body 주소를 물리에 넘기므로,
  등록 전에 `reserve(chunkCount)` 필수(재할당 금지). `goblins_`의 "no reallocation risk"와 동일 원칙.
- **Room 풀링:** `ObjectPool<Room>`은 push에서 `~Room()`, pop에서 placement-new → 매 `makeRoom`마다 새 인스턴스.
  따라서 `terrainChunks_`/`physicsWorld_`는 재사용 누적 없음.
- **collider origin = body pos = worldOffset.** `TerrainCollider::testVertex`가 이를 origin으로 로컬 좌표
  계산·범위 reject하므로 chunk별 라우팅이 자동 성립.
- **데이터 부재 안전:** `chunks_index.bin`이 없으면 `TerrainChunkManager`가 `empty()`로 남아 지형 contact 없음
  (서버는 정상 부팅, 명시 로그).

## 7. 검증

- 빌드: `MSBuild GameServer.sln /t:RoomServer /p:Configuration=Debug /p:Platform=x64` — **그린**(2026-06, RoomServer.exe 생성).
- 런타임(미완, 추출 데이터 필요): 부팅 로그의 로드된 chunk 수, 플레이어/고블린이 chunk 경계를 넘어도 지면
  contact 끊김 없음, 룸 다중 생성 시 heightfield 메모리가 룸 수에 비례 증가하지 않음(공유).

## 8. 후속(Out of scope)

- 거점(Stronghold) 시스템(인구수 모델/HP/풀/프로토콜) — 별도 설계.
- `heightAtWorld`의 신규 소비처(거점 스폰 Y 스냅 등). 본 작업은 인프라(파서 + 공유 매니저 + 다중 물리 + 룸 배선)까지.

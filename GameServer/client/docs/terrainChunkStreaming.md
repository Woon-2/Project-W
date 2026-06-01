# Terrain Chunk Streaming — 설계 문서

> 이 문서는 단일 Terrain → 다중 Chunk 스트리밍 전환의 설계 근거와 구현 계획을 기록한다.
> (CLAUDE.md "Major changes or design changes must be recorded in markdown documents" 지침)
>
> **구현 상태 (2026-06, client 브랜치):** Phase 0–7 구현 완료. 비동기 멀티스레드 로딩 + hop≤3 BFS
> 스트리밍(load/unload + grace + graveyard) 동작. 단일 terrain 경로(`loadTerrainFromFiles`, manifest/meta
> 파서, `RequestTerrainLoad`, `AssetManager::terrain_`) 제거됨. `GameServer.sln /t:client Debug x64` 빌드 그린.
> **남은 검증:** Unity batch export로 실제 멀티 청크 데이터를 추출해 런타임 육안/물리 검증(§13) 필요.

---

## 1. Context (왜 이 작업을 하는가)

기존 클라이언트는 **단일 Terrain 객체**만 다룬다. Unity Terrain Tool로 지형을 디자인하고
`TerrainExtractor.cs`로 바이너리 추출 → `loadTerrainFromFiles()`로 한 개의 `TerrainData`를 로드한다.
이를 **Chunk 개념으로 확장**하여 넓은 월드를 여러 청크로 나누고, 플레이어 주변만 **실시간 seamless
스트리밍**(load/unload)한다.

요구사항:
- 추출 시 청크의 **그리드 좌표(col,row)**, **Neighbor Chunk IDs**를 기록한다.
- 모든 청크 데이터를 **한 폴더**에 저장하되 파일명에 청크 ID를 포함해 충돌을 막는다.
- **Layer 팔레트는 전 청크가 공유**(통합)한다. 동일 팔레트는 한 번만 추출/로드한다.
- 개별 청크의 **로드+메시 구축은 ThreadPool로 멀티스레드** 처리한다. 공유 Layer는 **개별 청크 로드 전
  싱글스레드로 먼저** 로드한다.
- TerrainObject / TerrainPipeline / TerrainDeferredPipeline 가 여러 청크를 그리도록 확장한다.
- **Seamless 스트리밍**: 플레이어가 선 청크 기준 **hop ≤ 3** 청크는 로드, 벗어난 청크는 **pending(grace)
  time** 후 여전히 hop > 3이면 언로드. pending 동안 다시 3-hop 이내로 돌아오면 언로드 취소.
- `ChunkManager`(`TerrainChunkManager`)로 구현한다.

설계 결정:
1. 청크 위치 = 인덱스의 **그리드 좌표(col,row)** (절대·부호 있음·임의 범위). **셀 크기만 균일**, 청크 집합은
   **희소 가능**(모든 col,row가 채워질 필요 없음, (0,0) 비전제). `worldOffset = (col·sizeX, 0, row·sizeZ)`.
2. **단일 전역 인덱스 파일**(`chunks_index.bin`)이 모든 청크의 {col, row, neighbor IDs, 파일 경로}와 공유
   팔레트를 담는다.
3. 레이어 **최대 4개, splat 1장**(현 셰이더/CB 레이아웃 유지).
4. 씬 파일의 `Terrain` 노드 경로와 `AssetManager`의 단일 terrain 로드를 **제거**하고 ChunkManager가
   전적으로 소유.
5. 추출은 **부모 GameObject(예: `TerrainRoot`) 일괄 export** — 자식 Terrain 전체를 한 번에 보고 grid/이웃/
   팔레트/인덱스를 1패스로 결정.
6. 이웃 연결은 **4-이웃(상하좌우)**. 스트리밍 hop은 explicit neighbor 그래프 BFS.

---

## 2. 현재 코드 사실 (탐색 결과)

**렌더링은 이미 사실상 multi-chunk ready**:
- `TerrainPipeline`/`TerrainDeferredPipeline` Dispatcher는 모든 패스에서 `std::vector<DrawEvent>`를 순회
  (`terrainPipeline.cpp` mainPass/shadowPass, `terrainDeferredPipeline.cpp` gBufferPass). `DrawEvent`는 이미
  `{ const TerrainData*, world }`(`terrainPipeline.hpp:47-50`).
- per-drawcall ConstantBufferArray가 **1000 슬롯**(`gfx.cpp:599-622`) → 청크 수 제약 없음.
- `gfx.cpp:335-336` `drawEventsTerrain*Pipeline_.reserve(4u)`만 상향. **셰이더/PSO 변경 불필요.**
- `TerrainObject`(`object.hpp:403-420`, `object.cpp:930-951`)는 `const TerrainData*`+`world`로 DrawEvent/
  Occluder 제출. 청크당 하나면 그대로 동작.

**로딩 인프라 — 스레드 안전 경계**:
- `ThreadPool`(`threadPool.hpp`): `addJob(fn,args)` fire-and-forget, lock-free 큐, **future 없음** →
  완료 동기화는 atomic flag/fence로.
- `loadTerrainFromFiles()`/`buildTerrainMesh()`(`terrain.cpp`)는 `cmdList`+`Fence&`로 GPU copy 기록, upload
  buffer를 fence에 연관.
- `GFX::loadAssets()`(`gfx.cpp:1054-1231`)는 **단일 ResourceLoading CommandContext**에 모든 로드 기록 → 1회
  실행 → signal/wait.
- **스레드 비안전**: `DescriptorPool::alloc/free`(bare std::list), `texHashMap`(unordered_map), 단일 CommandList
  기록. → 멀티스레드는 **CPU 작업에만**, GPU 자원/디스크립터/커맨드 기록은 **메인 스레드**.

**물리/게임 — 단일 terrain 전제**:
- `PhysicsWorld`: 단일 `terrainCollider_`/`terrainHF_`(`physicsWorld.hpp:143-147`).
  `registerTerrain()` 교체(`physicsWorld.cpp:64-69`), `generateContacts()` terrain 루프(`308-338`),
  `queryCameraArm()`(`89-133`) 모두 단일 가정.
- `TerrainCollider`(`collision.hpp:102-114`, `collision.cpp:378-477`) 단일 `terrainBody_`+`heightField_`.
- 높이 직접 조회 호출부:
  - standalone `game.cpp`: 비 `2982-2987`, 폭발 `2997-3002`, importTerrain `2232-2245`, render `2788`,
    importNode `2166-2169`.
  - online `onlineGame.cpp`: 비 `2697-2702`, 폭발 `2712`, importTerrain `225`, render `2383`,
    importNode `194-196`.
  - 카메라: `camera.cpp:38` → `queryCameraArm()`.
- `AssetManager`: 단일 `TerrainData terrain_`(`AssetManager.hpp:16,77`), `addRequestTerrainLoad`
  (`AssetManager.cpp:67-71`).
- `Game::update()`(standalone `game.cpp` ~2330-2641): physics step → `player_->update` → `camera_.update`.
  스트리밍 훅 = **player update 후, camera update 전**. `player_` = `game.hpp:115`.

---

## 3. 데이터 포맷

### 3.1 파일 레이아웃 (모두 한 폴더, 예: `resources/terrains/`)

| 파일 | 범위 | 내용 |
|------|------|------|
| `chunks_index.bin` | 전역 1개 | 공유 팔레트 + 모든 청크 레코드(좌표/이웃/경로) |
| `layer_{i}_diffuse.dds`, `layer_{i}_normal.dds` | 공유(1회) | 통합 Layer 팔레트 (i=0..L-1, L≤4) |
| `chunk_{col}_{row}_height.raw` | 청크별 | ushort N×N 높이맵 (기존 포맷) |
| `chunk_{col}_{row}_splat0.dds` | 청크별 | RGBA splat (4채널=4레이어 weight) |

청크 ID = 그리드 좌표 `(col,row)`. Neighbor ID = `(col,row)` 리스트. per-chunk meta 파일은 **인덱스에 흡수**.

### 3.2 `chunks_index.bin` (tag 기반 — `binaryImport.hpp`의 `readHeadTag/readText/readInteger/readFloat` 재사용)

```
Head "ChunkIndex"
  Int   "Version"        = 1
  # ---- 공유 팔레트 ----
  Int   "LayerCount"     = L           # ≤ 4
  repeat L:
    Text  "DiffusePath"
    Text  "NormalPath"
    Float "TileSizeX" "TileSizeY" "TileOffsetX" "TileOffsetY"
    Float "Metallic" "Roughness"        # roughness = 1 - smoothness (추출 시 변환)
  # ---- 청크 레코드 ----
  Int   "ChunkCount"     = C
  repeat C:
    Head "Chunk"
      Int   "Col" "Row"
      Float "SizeX" "SizeY" "SizeZ"
      Int   "Resolution"                # N (heightmap)
      Int   "AlphamapResolution"
      Int   "NeighborCount" = K          # 4-이웃 (상하좌우), 실재하는 것만
      repeat K: Int "NCol" "NRow"
      Text  "HeightPath"                # chunk_{col}_{row}_height.raw
      Text  "SplatPath"                 # chunk_{col}_{row}_splat0.dds
    Tail "Chunk"
Tail "ChunkIndex"
```

`SizeX/Z`는 청크 공통이지만 청크별로 적어 검증/확장 여지를 둔다(불일치 시 경고).

### 3.3 Extractor (`TerrainExtractor.cs`) — 부모 GameObject batch export

- UI: `Terrain` 단일 필드 → **`TerrainRoot`(Transform/GameObject)**. `GetComponentsInChildren<Terrain>()`로 청크 수집.
- **grid 좌표 자동**: `chunkSizeX=size.x`, `chunkSizeZ=size.z`,
  `col=round(transform.position.x/chunkSizeX)`, `row=round(position.z/chunkSizeZ)`. 좌표 충돌/사이즈 불일치 → 중단+에러.
- **이웃 자동(4-이웃)**: `(col±1,row)`,`(col,row±1)` 중 실재 청크만 neighbor 기록.
- **팔레트 dedup**: 첫 청크 `terrainLayers`를 기준 팔레트로 `layer_{i}_*` 1회 작성. 이후 청크가 동일 시그니처면
  통과, 다르면 중단+경고(통합 팔레트 전제 강제).
- 청크별: `chunk_{col}_{row}_height.raw`, `chunk_{col}_{row}_splat0.dds`. 마지막에 `chunks_index.bin` 1회 작성.
- 기존 규약 유지: height ushort 양자화(y외곽·x내곽), splat RGBA, roughness=1-smoothness, normal Linear/diffuse sRGB.
- PNG→DDS 변환은 기존처럼 외부 단계(인덱스 경로는 `.dds`).

---

## 4. 엔진 데이터 구조 (`terrain.hpp`/`terrain.cpp`)

핵심: **`TerrainData` 외형(필드 접근 패턴) 유지** → 두 파이프라인 무수정. 팔레트는 ChunkManager가 1회 로드,
각 청크 `TerrainData.layers`에 팔레트의 **BindlessIndex 핸들 복사**(GPU 텍스처 1개, 핸들만 공유).
splat/mesh/heightField만 청크별 소유.

```cpp
struct TerrainLayerPalette {            // 신규: 공유 팔레트 (ChunkManager 소유, 1회 로드)
    int layerCount = 0;
    std::vector<TerrainLayer> layers;
    bool loaded = false;
};
struct ChunkIndexEntry {                // 신규: 전역 인덱스 항목
    int col = 0, row = 0;
    float sizeX = 0, sizeY = 0, sizeZ = 0;
    int   resolution = 0, alphamapResolution = 0;
    std::vector<std::pair<int,int>> neighbors;
    std::string heightPath, splatPath;
};
struct ChunkIndex {
    int version = 0;
    TerrainLayerPalette paletteInfo;     // 경로/스칼라 (텍스처 로드는 별도)
    std::vector<ChunkIndexEntry> chunks;
};
struct ChunkCpuBuild {                  // 신규: ThreadPool 산출(GPU 자원 없음)
    int col = 0, row = 0;
    float sizeX = 0, sizeY = 0, sizeZ = 0;
    int   resolution = 0;
    std::vector<XMFLOAT3> positions, normals, tangents, bitangents;
    std::vector<XMFLOAT2> uvs;
    std::vector<u32t>     indices;
    std::vector<float>    normalizedHeights;   // 물리용
    std::atomic<bool>     done{false};
    bool                  failed = false;
};
// TerrainData: 기존 필드 유지 + int chunkCol, chunkRow 추가
```

신규 함수:
- `ChunkIndex parseChunkIndex(path)`
- `TerrainLayerPalette loadLayerPalette(index, device, cmdList, texHashMap, texPool, fence)` — 메인 1회
- `void buildChunkCpu(const ChunkIndexEntry&, ChunkCpuBuild& out)` — **순수 CPU**(D3D 호출 없음)
- `TerrainData finalizeChunkGpu(cpu, palette, splatPath, device, cmdList, texHashMap, texPool, fence)` — **메인**
- 기존 `getHeightAt/getNormalAt` 재사용.

---

## 5. ChunkManager (`client/terrainChunkManager.hpp`/`.cpp`, **영문 주석**)

### 5.1 책임
공유 팔레트/인덱스 로드, 청크 생명주기 상태기계, ThreadPool CPU 빌드 등록 + 메인 GPU 마감/fence 게이팅,
매 프레임 DrawEvent/Occluder 제출, 월드→청크 라우팅 높이/법선 질의, PhysicsWorld collider 등록/해제.

### 5.2 상태기계 (청크별)
```
Unloaded → Loading(CPU)  : ThreadPool 작업 등록, ChunkCpuBuild 채우는 중
Loading(CPU) → Uploading : done==true, 메인 GPU 마감 + fence signal
Uploading → Ready        : fence 완료 → TerrainObject 활성 + 물리 등록
Ready → Expiring         : desired 이탈, grace timer 시작
Expiring → Ready         : grace 내 desired 재진입(취소)
Expiring → Unloading     : grace 만료 → 물리 해제 + GPU 자원/디스크립터 해제 → Unloaded
```
불변식: **플레이어가 선 청크는 절대 언로드/expire 하지 않는다.**

### 5.3 API (개략)
```cpp
class TerrainChunkManager {
public:
    void init(ThreadPool&, GFX&, PhysicsWorld&, const std::filesystem::path& terrainDir);
    void update(mu::Vec3 playerWorldPos, Seconds dt);
    void submitDrawEvents(GFX&);
    float    heightAtWorld(float x, float z) const;
    mu::Vec3 normalAtWorld(float x, float z) const;
    std::optional<std::pair<int,int>> chunkCoordAtWorld(float x, float z) const;
private:
    TerrainLayerPalette palette_;
    ChunkIndex          index_;
    std::unordered_map<int64_t, LoadedChunk> chunks_;          // key = pack(col,row)
    std::unordered_map<int64_t, const ChunkIndexEntry*> indexByCoord_;
    int     maxHop_ = 3;
    Seconds graceTime_ = 5s;
    int     maxGpuFinalizePerFrame_ = 1;
};
```

### 5.4 좌표 라우팅 (희소 · 임의 좌표 범위)

**불변식**:
- col,row는 **절대 그리드 인덱스**(부호 있음, 임의 범위). (0,0)/연속 범위 비전제. 예) (15,15)~(21,21), 음수 OK.
- 청크 집합 **희소** 가능. 인덱스/맵에는 **존재 청크만**, 이웃은 실재 인접만 기록. 스트리밍은 **그래프 BFS**라
  빈 셀 자연 스킵.
- Unity terrain `transform.position` = 청크 min XZ 코너, terrain은 `[position, position+size]` →
  추출 `col=round(position.x/sizeX)`, 런타임 포함 `col=floor(x/sizeX)` 정합.

**구현**:
- `worldOffset(col,row) = (col·sizeX, 0, row·sizeZ)` — col 절대값, origin 가정 없음.
- 키: `int64 pack(col,row) = ((int64)(int32)col << 32) | (uint32)row`.
- `chunkCoordAtWorld(x,z)`: `col=floor(x/sizeX)`, `row=floor(z/sizeZ)`. `indexByCoord_`에 없으면 nullopt.
- `heightAtWorld(x,z)`: 청크 찾고 `localX=x-col·sizeX`, `localZ=z-row·sizeZ` → `getHeightAt(localX,localZ)` +
  `worldOffset.y`. 미로드/경계 밖/구멍이면 0 또는 인접 청크 폴백.

### 5.5 스트리밍 틱 (`update`)
1. `cur = chunkCoordAtWorld(player)`; 없으면 마지막 유효 유지.
2. **neighbor 그래프 BFS**(4-이웃, explicit neighbor), depth ≤ `maxHop_` → `desired`. (hop≤3 → 최대 25 청크)
3. `desired` 중 Unloaded → CPU 작업 등록(Loading). in-flight 상한.
4. Ready/Uploading이며 desired 밖 → Expiring(타이머). `cur` 제외.
5. Expiring이며 desired 재진입 → Ready 복귀(취소).
6. Expiring 타이머 ≥ `graceTime_` → Unload.
7. **GPU 마감 드레인**: Loading & `cpu->done` 인 청크를 프레임당 `maxGpuFinalizePerFrame_`개까지 finalize →
   fence signal → Uploading.
8. Uploading 중 fence 완료 → Ready(TerrainObject 배치 + 물리 등록).

---

## 6. 비동기 멀티스레드 로딩 (data-race-free)

멀티스레드는 **CPU 작업으로 한정**(무거운 비용은 N×N 지오메트리). 공유 Layer 선로드만으로는 splat/DescriptorPool/
texHashMap/CommandList 경합이 남으므로 CPU/GPU 경계를 분리한다.

- **Phase A (init, 메인 1회)**: `loadLayerPalette()` — 팔레트 텍스처 + SRV 단일 스레드.
- **Stage 1 (ThreadPool, `addJob`)**: `buildChunkCpu()` — height 파싱 + 정점/법선/탄젠트/인덱스/heightField 생성.
  공유 가변 상태 미접근. `cpu->done=true`(atomic).
- **Stage 2 (메인, `update` 드레인)**: `finalizeChunkGpu()` — VB/IB GPU 자원 + copy 기록(cmdListPool alloc),
  splat `loadDDS`+SRV, execute + fence signal.
- **Stage 3 (메인, 게이팅)**: fence 완료 후에만 Ready 승격 → 미완성 자원 미렌더(seamless).

향후 최적화(기록만): per-thread CommandContext + DescriptorPool/texHashMap 뮤텍스화로 GPU 마감 병렬화.

---

## 7. 물리 다중 지형 (`physicsWorld.*`, `collision.*`)

- 단일 `terrainCollider_`/`terrainHF_` → **collider 컬렉션** + `TerrainHandle`(register 반환/unregister 입력).
  `unordered_map<int64 pack(col,row), TerrainEntry>`(collider + worldXZ AABB + col/row).
- `generateContacts()`: Dynamic body마다 **월드 XZ로 겹치는 청크만** 라우팅(보통 1, 경계 2~4). uniform grid →
  `col=floor(x/sizeX)` O(1) 후보. 전체 M 순회 회피.
- `queryCameraArm()`: 6 샘플마다 속한 청크 heightField로 높이 조회.
- PhysicsWorld는 **grid 셀 크기(sizeX/Z)만** 보유. populated origin/연속 범위 가정 없음 — 미등록 (col,row)는
  지형 contact 없음(공중/구멍).
- `TerrainCollider`는 단일 청크 기준 유지(변경 최소; 필요 시 worldAABB 노출).

---

## 8. 렌더링 통합

- `gfx.cpp:335-336` `reserve(4u)` → `reserve(32u)`(hop≤3 최대 25 + 여유). perDrawcall 1000 그대로. **셰이더/PSO/
  Dispatcher 무수정.**
- `TerrainObject` 무수정. 청크당 1개를 ChunkManager가 소유, `setPos(worldOffset)`.
- game `render()`: `if(terrain_) terrain_->render(gfx_)` → `chunkManager_.submitDrawEvents(gfx_)`. `addFrameData`는 1회 유지.
- Hi-Z occluder: 청크별 `TerrainObject::render`가 willOcclude+Deferred 시 OccluderInfo 제출(기존 그대로).
  terrain은 renderObjectId 비사용 → `setMaxRenderObjectId` 무관.

---

## 9. 게임 통합 & 기존 경로 제거

- `StandAlone::Game`/`Online::Game`에 `TerrainChunkManager chunkManager_`.
- setup: `chunkManager_.init(threadPool_, gfx_, physicsWorld_, "../resources/terrains/")`.
- `update()`: player update 후, camera update 전에 `chunkManager_.update(player_->pos(), deltaTime)`.
- 높이 직접 조회 → `chunkManager_.heightAtWorld(x,z)`로 교체(standalone 비/폭발, online 비/폭발).
- **제거**: `importNode` `type=="Terrain"` 분기 + `importTerrain()`(양 게임), `AssetManager`의 `terrain_`/
  `terrain()`/`addRequestTerrainLoad`, gfx 단일 terrain 로드 경로(또는 팔레트 로드로 용도 변경).
- 카메라 `camera.cpp:38`은 물리 라우팅으로 자동 대응(무수정).

---

## 10. 구현 Phase

- **Phase 0** — 본 문서 작성 + `chunks_index.bin` 레이아웃 확정.
- **Phase 1** — Extractor batch export(자식 순회, 좌표/이웃 자동, 팔레트 dedup, 인덱스 1패스).
- **Phase 2** — 로더 리팩토링(팔레트/인덱스/CPU빌드 구조체, CPU/GPU 분리 함수, TerrainData grid 좌표).
- **Phase 3** — ChunkManager 동기 baseline(전 청크 동기 로드, 라우팅, submitDrawEvents, 게임 배선, 단일 제거).
- **Phase 4** — 다중 지형 물리(collider 컬렉션, register/unregister, contact·camera 라우팅).
- **Phase 5** — 비동기 멀티스레드 로딩(ThreadPool buildCpu + 메인 finalizeGpu + fence 게이팅).
- **Phase 6** — 스트리밍 생명주기(플레이어 청크 검출, BFS hop≤3, load/unload, grace timer; 양 게임 배선).
- **Phase 7** — 정리(죽은 코드 제거), CODE_INDEX 갱신, 본 문서 최종화, 메모리 갱신.

---

## 11. 주요 파일별 변경 요약

| 파일 | 변경 |
|------|------|
| `unityScripts/TerrainExtractor.cs` | batch export, 청크 ID/이웃/그리드, 팔레트 dedup, 전역 인덱스 |
| `terrain.hpp`/`terrain.cpp` | 팔레트/인덱스/CPU빌드 구조체, CPU/GPU 분리 로더 |
| `terrainChunkManager.hpp`/`.cpp` | **신규** ChunkManager (영문 주석) |
| `physicsWorld.hpp`/`.cpp` | collider 컬렉션, register/unregister, contact·camera 라우팅 |
| `collision.hpp`/`.cpp` | TerrainCollider 변경 최소(필요 시 worldAABB 노출) |
| `gfx.cpp` | `reserve(4u)`→`reserve(32u)`; 단일 terrain 로드 경로 정리 |
| `object.*` | TerrainObject 무수정(확인) |
| `standalone/game.*`, `online/onlineGame.*` | ChunkManager 배선, 높이 조회 교체, 단일 terrain 제거 |
| `AssetManager.*` | 단일 terrain 로드 제거(팔레트는 ChunkManager로 이관) |
| `client.vcxproj`(.filters) | 신규 파일 등록 |
| `docs/CODE_INDEX.md` | 인덱스 갱신 |

---

## 12. 위험 요소 & 대응

- **GPU 마감 hitch**: 프레임당 finalize 수 제한 + fence 게이팅. CPU 빌드 선행 → 마감만 분산.
- **스레드 안전**: 멀티스레드 CPU 전용. DescriptorPool/texHashMap/CommandList 메인 전용.
- **청크 경계 seam**: 가장자리 높이/법선 연속성은 추출 단계 보장. 엔진 법선은 청크 내부 central-difference라 경계
  불연속 가능 → 필요 시 경계 정점 법선 이웃 평균(후속 개선).
- **sparse/비사각 맵**: neighbor 그래프 BFS로 처리.
- **in-flight 폭주**: 동시 로드 상한 + grace로 thrashing 방지.
- **online 권위**: 지형은 클라 표현 → 스트리밍 로컬, 서버 동기화 무관.

---

## 13. 검증 (end-to-end)

1. **Phase 1**: 2×2 청크 추출 → `chunks_index.bin` + 파일 생성, 팔레트 1벌만 확인.
2. **Phase 3**: 실행 → 4청크 올바른 grid 위치 인접 렌더, `heightAtWorld`로 비/폭발 지면 스냅 정상.
3. **Phase 4**: 청크 경계 너머 이동 → 지면 충돌 끊김 없음, 카메라 spring arm 정상.
4. **Phase 5**: 로깅으로 CPU 빌드 워커 스레드 수행, 메인 마감/fence 게이팅 확인, hitch 측정.
5. **Phase 6**: hop>3 grace 후 언로드, 복귀 시 재로드/취소 로그 확인, 메모리 상한 유지.
6. 실행: `/run` 또는 VS2022 빌드 후 `client`(standalone, online).

> 자동화 테스트 없음(프로젝트 정책) — DummyClient/수동 검증.

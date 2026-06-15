# Scatter 시스템 (Unity Terrain Tree / Detail 추출·렌더링)

Unity Terrain Tool의 **Paint Tree**(나무)와 **Paint Detail**(Rock/Bush/Grass 등 디테일 메시·빌보드 풀)로
칠한 정적 식생을 terrain chunk에 담아 추출하고, 클라이언트에서 동일한 화면으로 렌더링한다.
heightmap terrain만 스트리밍하던 기존 chunk 파이프라인에 "흩뿌려진(scattered) 정적 식생" 개념을 chunk 단위로 추가한 것.

## 데이터 흐름

```
Unity Terrain  ──(TerrainExtractor.cs)──▶  chunks_index.bin (v3)
                                            + scatter_*.dds (빌보드 텍스처)
prop 프리팹     ──(ModelExtractor.cs)─────▶  ../resources/models/props/{Name}.bin
                                            (LOD0만, FindAlbedoTexture로 albedo 견고 추출)
                                                    │
클라이언트:  parseChunkIndex ──▶ TerrainChunkManager.loadScatterAssets()
             (prop .bin 로드 + 빌보드 메시/머티리얼 빌드 + 프로토타입 resolve)
                 chunk 활성화 ──▶ resolveChunkScatter() (인스턴스 world/AABB 상주)
                 매 프레임    ──▶ submitScatterDrawEvents() (PBR/PBRDeferred 자동 인스턴싱)
```

## 바이너리 포맷 (chunks_index.bin, version 3)

버전 이력: **v1**(scatter 없음) → **v2**(ScatterPrototypes + Chunk별 Scatter 추가) → **v3**(per-instance 회전을
`Yaw` float → `Rot` 쿼터니언으로 확장). reader는 `version >= 2` 게이트로 scatter 섹션 유무를, `version >= 3`
게이트로 회전 표현을 분기한다(v1은 통째로 건너뜀, v2 파일은 `Yaw`를 Y축 쿼터니언으로 변환해 읽음).

- **Palette** 직후 전역 섹션 `ScatterPrototypes`(중복 제거된 프로토타입 테이블):
  - `PrototypeCount`
  - per prototype: `Name`(text=매핑 키), `Kind`(0=TreeMesh,1=MeshDetail,2=BillboardGrass),
    `TexturePath`(빌보드 전용), `BillboardSize`(Vec2), `Tint`(Color 4f)
- 각 `Chunk` 레코드 내부(`</Chunk>` 직전) `Scatter` 블록:
  - `InstanceCount`
  - per instance: `ProtoIdx`(int), `PosLocal`(Vec3, chunk worldOffset 기준), `Rot`(Quaternion 4f, Unity x,y,z,w), `Scale`(Vec2 width/height)

> **버전 3**: per-instance 회전을 단일 `Yaw`(float)에서 **전체 쿼터니언 `Rot`**으로 바꿨다. 디테일 메시의
> "Align To Ground(%)" 지면 정렬 틸트를 추출 시점에 **베이크**하기 위함이다(아래 "디테일 회전" 참고).
> reader는 `version >= 3`이면 `Rot`, `version == 2`(레거시)면 `Yaw`를 읽어 Y축 쿼터니언으로 변환한다(스트림 정렬 유지).
> 엔진은 Unity 쿼터니언 (x,y,z,w)을 `mu::NQuat(x,y,z,w).mat4()`로 그대로 사용한다(zone/marker Orient와 동일 규약).

작성: `client/unityScripts/TerrainExtractor.cs::WriteIndex`
읽기: `client/terrain.cpp::parseChunkIndex`, `RoomServer/terrain.cpp::parseChunkIndex`(scatter 인스턴스 저장 — 몬스터-prop 충돌)

> 포맷 변경 시 `chunks_index.bin` **전체 재추출 필수**. 서버는 scatter를 쓰지 않으므로 정렬 유지를 위해 읽고 버린다.

### 디테일(Paint Detail) 산포 — Unity 권위 API 사용 (중요)
디테일은 `GetDetailLayer` 셀값을 직접 개수로 쓰면 **안 된다**. Unity 2022.2+/Unity 6은 Detail Scatter Mode가
`CoverageMode`(기본)일 때 셀값이 **개수가 아니라 커버리지(0~255)** 라, 그대로 곱하면 수백~수천 배 과다 산포된다
(과거 16억 인스턴스 버그의 원인). 그래서 추출기는 `TerrainData.ComputeDetailInstanceTransforms(patchX, patchY,
layer, density, out bounds)`로 **Unity가 실제 렌더하는 인스턴스(terrain-local 위치·yaw(rad)·scaleXZ/scaleY)** 를
그대로 받아온다. 패치 수 = `detailResolution / detailResolutionPerPatch`, density = `terrain.detailObjectDensity ×
Detail Density Scale`. 트리는 `treeInstances`(개별)라 그대로 정확.

### 디테일 회전 — "Align To Ground (%)" 베이크 (중요)
`ComputeDetailInstanceTransforms`는 **yaw(`rotationY`)만** 반환하고, 디테일의 지면 정렬 틸트는 Unity가 **렌더 시점에**
지형 노멀로 적용한다(transform에 없음). 따라서 추출기가 이를 직접 재현한다(`DetailPrototype.alignToGround` 0~1):
`n = GetInterpolatedNormal(u,v)` → `alignedUp = Slerp(up, n, alignToGround)` →
`rot = FromToRotation(up, alignedUp) * AngleAxis(rotationY, up)` 를 per-instance 쿼터니언으로 베이크한다.
- 메시 디테일(kind=1)만 틸트한다. **빌보드 풀(kind=2)은 직립 유지**, **트리(kind=0)는 yaw만**(지면 정렬 안 함).
- 정렬 안 함 ⇒ `rot`은 순수 yaw 쿼터니언이라 구버전(rotateY)과 동일 결과.

## 모델 이름 매핑

`ModelExtractor.cs`가 `targetName`으로 모델 이름을 직접 지정하듯, `TerrainExtractor`도 **Scan Prototypes** 버튼으로
발견된 prototype 목록을 편집 가능한 이름 필드로 노출한다. 사용자가 입력한 이름이 `Name`으로 기록되고,
클라이언트는 `propModels_[Name]`(= `../resources/models/props/{Name}.bin`)으로 해석한다.
**추출 시 지정한 이름 == ModelExtractor targetName** 이면 매핑된다(prefab 이름 자동 의존 X).
이름이 일치하는 prop `.bin`이 없으면 경고 로그 후 해당 프로토타입은 건너뛴다.

## 렌더링

- 신규 인스턴싱 파이프라인 없음. 기존 PBR/PBRDeferred 파이프라인이 동일 `(mesh, subMesh, material)` DrawEvent를
  자동으로 묶어 단일 instanced 드로우콜로 처리한다(`pbrPipeline.cpp` 정렬+`upper_bound`).
- 인스턴스는 무거운 `Object`가 아니라 `gfx.addDrawEvent(...)`로 직접 emit한다(1만 개 풀 대응).
- 같은 prototype 인스턴스는 같은 mesh/material을 가리키므로 chunk 경계를 넘어 전역적으로 배칭된다.
- 렌더 경로는 `gfx.renderPath()`를 따라 PBRDeferred/PBR 중 선택(object.cpp 미러).
- **거리 컬링(필수)**: `perInstanceData` StructuredBuffer는 **DrawEvent(=인스턴스)당 1칸**이고 고정 용량이라(초과 시
  `ShaderInputBuffer::stage`가 통째로 스킵→해당 패스 렌더 실패), 디테일/풀은 `submitScatterDrawEvents`에서
  **플레이어 반경 `kDetailCullRadius`(80m) 안만 emit**한다(Unity Detail Distance와 동일 개념). 트리는 랜드마크라 전부 emit.
  cullCenter는 `update(playerWorldPos)`에서 갱신. 이로써 칠한 총량과 무관하게 매 프레임 emit 수가 근거리로 한정된다.
- **버퍼 용량**: 안전마진으로 PBRDeferred shadow/gBuffer `perInstanceData` = 32,768, forward PBR = 16,384 (gfx.cpp).
  거리 컬링이 1차 방어, 용량 상향이 2차 방어.

### 폴리지 알파 컷아웃
나뭇잎·풀은 alpha test가 필요하다. 신규 PSO 대신:
- `Material::constantAlphaCutoff`(기본 0=불투명) 추가. 셰이더 머티리얼 cbuffer의 `padding0`을 `cAlphaCutoff`로 재활용(레이아웃 불변).
- `pbrDeferred.hlsl`/`pbr.hlsl` PSMain에서 `clip(albedo.a - cAlphaCutoff)`. cutoff 0이면 전 모델 무동작(회귀 없음).
- scatter prop 머티리얼은 로드 시 전부 cutoff=`kFoliageAlphaCutoff`(0.33). 불투명 줄기(albedo a≈1)는 영향 없음.
- **함정(트리 안 보임 버그)**: 머티리얼이 albedo 맵 없이 임포트되면 `constantAlbedo=(0,0,0,0)`가 되어 컷오프로 **전부 클리핑(=투명)**된다.
  원인은 트리 셰이더의 albedo 텍스처 프로퍼티 이름이 `ModelExtractor`의 후보(`_MainTex/_BaseMap/...`) 밖이었던 것.
  → `ModelExtractor.FindAlbedoTexture`가 후보 실패 시 셰이더 텍스처 프로퍼티 전체를 훑어 albedo 슬롯을 찾고,
  `cAlbedo`는 `_Color`/`_BaseColor` 없으면 흰색으로 기본값. **프롭 `.bin` 재추출 필요**.
- **양면**: 빌보드 cross-quad는 뒤집힌 삼각형을 중복 생성해 단면 컬링 PSO로도 양면 렌더(별도 PSO 불필요).
- **한계**: 그림자 패스는 position-only라 잎/풀 그림자는 컷아웃되지 않는다(solid 실루엣). 추후 alpha shadow로 개선 가능.

### 빌보드 풀
- 부팅 시 단위 cross-quad 메시 1개 생성(`buildCrossQuadMesh`, x/z∈[-0.5,0.5]·y∈[0,1], 2평면×양면=8삼각형).
- prototype별 `Material`(mapAlbedo=grass 텍스처, 나머지 맵 비활성 idxRange=-1, cutoff 적용).
- 인스턴스 크기는 `ComputeDetailInstanceTransforms`의 `scaleXZ/scaleY`가 **월드 크기 그대로** 담으므로, 프로토타입 `BillboardSize`는 1×1로 두고 unit quad에 scale만 적용한다(고정 quad, 카메라 추종은 미구현).

## 충돌(Physics) — 구현됨 (`ScatterCollider`)

`ModelExtractor`로 추출한 Tree/Rock 모델은 `Model::bvh`(model-space)를 갖는다. 이를 `TerrainCollider`/`registerTerrain`
슬롯 패턴을 일반화한 `WorldCollider` 추상화로 처리한다(`collision.hpp`, `common/slotVector.hpp`):
- **클라**: `activateChunkRenderAndPhysics`가 collidable 인스턴스(`ResolvedScatterProto.collidable`)의 `(model->bvh, pos/rot/scale)`로
  `ScatterCollider`를 만들어 `PhysicsWorld::registerScatter`, 핸들을 `LoadedChunk::scatterHandle`에 보관(`unloadChunk`에서 해제).
- `ScatterCollider`는 인스턴스 world BVH를 `makeWorldBVH`로 **1회 베이크**(정적→재빌드 없음)하고 XZ uniform grid로 인덱싱.
  Dynamic body마다 `collides(BVH,BVH)` → `StaticContact`(push-out, 회전·분리속도 없음) → `resolveStaticPenetration`.
  카메라 암은 `queryArm`이 `RaycastBVH`로 차단.
- **권위 분리(무분기)**: Dynamic body만 질의 → 클라=로컬 플레이어(player-vs-prop), 서버=몬스터(monster-vs-prop).
  플레이어 위치는 `C_Move`, 몬스터 위치는 `S_NpcMoveBatch`로 복제 → 다중 체인에서도 위치 동기화.
- **결정론**: 클라/서버가 `common/scatterTransform.hpp::makeScatterWorld` + 동일 ground-snap으로 prop world를 동일하게 계산.
  형상 소스도 동일(`MultiBoundingVolume`→`ModelExtractor(ForServer)` BoundingVolumes).
- **서버**: `parseChunkIndex`가 scatter prototype 이름 + per-chunk 인스턴스를 저장, `loadPropBVHs`가 `<name>Server.bin`(BV-only,
  `ModelExtractorForServer`로 추출)을 로드, `Room::init`이 `registerScatterColliders`로 룸 PhysicsWorld에 등록.
  서버에 `staticDepenetration`를 헤더 전용으로 포팅. **collidable prop은 `<name>Server.bin` 재추출 필요**(없으면 비충돌로 skip).
- 빌보드 풀은 BVH 없음 → 비충돌.
- (향후 최적화) 서버는 룸마다 world BVH를 베이크한다(터레인 콜라이더와 동일 per-room). 룸 수가 많으면 베이크 데이터를 공유로 전환 가능.

## 확장 대비 / 향후 작업 (의도된 단순화와 끼워넣을 자리)

현재 구현은 "시각 일치 + 인스턴싱"이 우선이라 아래는 **의도적으로 단순화**했고, 데이터/구조는 확장이 쉽도록 남겨 두었다.

1. **렌더 LOD 전환** — `ModelExtractor`는 트리/식생 프리팹의 **LOD0만** 추출한다(LODGroup의 LOD1+ 렌더러는 노드만 남기고 메시/재질 skip). 거리별 LOD 전환·임포스터·페이드는 미구현. 향후 prototype에 LOD 메시 배열을 담고 `submitScatterDrawEvents`에서 거리로 mesh/subMesh를 골라 emit하면 된다(자동 인스턴싱은 그대로 재사용).
2. **트리 거리/임포스터 컬링** — 디테일/풀만 `kDetailCullRadius`로 거리 컬링하고 **트리는 항상 emit**한다(랜드마크). 트리 수가 매우 많아지면 `perInstanceData` 용량(32768)을 넘길 수 있으니, 그 전에 트리도 LOD/임포스터 + 거리 컬을 도입해야 한다. (지금은 `worldAABB` 상주로 인스턴스 단위 컬 준비만 됨.)
3. **인스턴스 단위 frustum 컬링** — 현재는 chunk 단위 frustum(hop 범위) + 디테일 거리 컬만. `ScatterInstanceResolved::worldAABB`가 이미 있으므로 view frustum 교차로 per-instance 컬을 추가하는 비용은 작다.
4. **머티리얼 알파 모드(불투명 vs 컷아웃)** — 지금은 scatter 프롭 **모든** 머티리얼에 `kFoliageAlphaCutoff`(0.33)를 일괄 적용한다. 불투명 줄기(albedo a≈1)는 무해하지만 의미상 부정확하다. 향후 `ModelExtractor`에서 Unity 머티리얼의 surface type / `_Cutoff` / `_ALPHATEST_ON`을 읽어 **per-material cutoff**를 추출하면, 줄기는 0(불투명), 잎은 실제 컷오프값으로 분리된다. (`Material::constantAlphaCutoff`가 이미 per-material 필드라 받을 준비는 됨.)
5. **그림자 알파 컷아웃** — 그림자 패스는 position-only라 잎/풀 그림자가 solid 실루엣. alpha-test 그림자 변형 PSO로 개선 가능.
6. **카메라 추종 빌보드 / 바람** — 빌보드는 고정 cross-quad(카메라 추종 X), 바람 애니메이션 없음. 정점 셰이더 빌보딩·wind 노이즈로 확장 가능.

(충돌은 구현 완료 — 위 "충돌(Physics)" 절 참조.)

## 핵심 파일
- Unity: `client/unityScripts/TerrainExtractor.cs`(인덱스/산포 추출), `ModelExtractor.cs`(프롭 `.bin` 추출 — LOD0-only, `FindAlbedoTexture` 견고 albedo)
- 포맷/자료구조: `client/terrain.hpp`(ScatterPrototype/ScatterInstance), `client/terrain.cpp`(parseChunkIndex)
- chunk 통합: `client/terrainChunkManager.hpp/.cpp`(loadScatterAssets/resolveChunkScatter/submitScatterDrawEvents, billboardMesh_/propModels_/resolvedProtos_, `kDetailCullRadius`)
- 머티리얼/셰이더: `client/mesh.hpp`(constantAlphaCutoff), `shader.hpp`, `pbr.hlsl`, `pbrDeferred.hlsl`, `pbrPipeline.cpp`, `pbrDeferredPipeline.cpp`
- 인스턴스 버퍼 용량: `client/gfx.cpp`(perInstanceData PBRDeferred 32768 / forward 16384)
- 서버 정렬: `RoomServer/terrain.cpp`(scatter 인스턴스 저장 + `loadPropBVHs`, v3 분기)

## 검증 체크리스트
1. Unity에서 Scan Prototypes → 이름 지정 → Export. `chunks_index.bin`(v3)에 ScatterPrototypes/Scatter 기록 + scatter_*.png 출력. PNG→DDS 변환.
2. prop 프리팹을 ModelExtractor로 동일 이름(.bin)으로 추출, `../resources/models/props/`에 배치. (포맷/머티리얼 변경 시 **재추출 필수**)
3. 클라 진입 시 트리/락/풀이 Unity와 동일 배치로 렌더, 풀 알파 경계 깨끗한지 확인.
4. **트리가 검정/투명이 아닌 정상 색**으로 보이는지(=albedo 맵 추출 정상). 안 보이면 `.bin` 머티리얼에 AlbedoMap이 있는지 확인.
5. **디테일(바위 등)이 Unity와 같은 지면 정렬 각도**로 누워 있는지(Align To Ground 베이크 확인).
6. PIX로 동일 prototype이 단일 instanced 드로우콜로 묶이는지 확인.
7. chunk 언로드 시 잔상·누수 없는지, 먼 chunk는 emit 안 되는지 확인.

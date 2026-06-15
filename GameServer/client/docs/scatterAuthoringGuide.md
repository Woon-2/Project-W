# 지형에 Tree / Rock / Flower / Bush / Plant 띄우기 — 작성 가이드

Unity Terrain에 **Paint Tree / Paint Detail**로 그린 식생을 우리 게임에 띄우는 전체 절차.
시스템 설계는 [`scatterSystem.md`](scatterSystem.md) 참고. 이 문서는 "무엇을 어떤 순서로 하면 되는가"에 집중한다.

> 전제: Unity에서 Tree와 Detail Mesh를 이미 다 그려 둔 상태.

---

## 0. 먼저 이해할 것 — Unity 페인트 방식 ↔ 추출 종류(Kind)

| Unity에서 그린 방식 | Kind | 모델 소스 | 별도 `.bin` 추출 필요? |
|---|---|---|---|
| **Paint Trees** (나무) | TreeMesh | 프리팹 메시 | **필요** (ModelExtractor) |
| **Paint Details → Mesh** (Rock/Bush/Plant를 메시로) | MeshDetail | 프리팹 메시 | **필요** (ModelExtractor) |
| **Paint Details → Texture/Billboard** (잔디·꽃 등 2D 풀) | BillboardGrass | 텍스처(자동 cross-quad) | 불필요 |

핵심 규칙:
- **메시로 그린 것**(나무·바위·덤불·식물)은 그 프리팹을 `ModelExtractor`로 `.bin` 모델로 따로 추출해야 한다.
- **빌보드(텍스처)로 그린 풀/꽃**은 텍스처만 있으면 되고, 엔진이 자동으로 양면 십자 quad에 그린다.
- 어떤 detail이 mesh인지 billboard인지는 Unity의 Terrain → Paint Details에서 각 프로토타입의 *Render Mode*(또는 `usePrototypeMesh`)로 확인.

---

## 1. 메시 프롭(나무·바위 등) `.bin` 추출 — ModelExtractor

메시로 그린 **모든 종류의 프리팹마다** 1회씩:

1. Unity 메뉴 **Tools ▸ Model Extractor** 열기.
2. **Target Object**에 프리팹(나무/바위 등)을 지정.
   - LOD가 있는 프리팹이면 자동으로 **LOD0만** 추출된다(LODGroup 컴포넌트 필요). → [`skillCreationGuide`/LOD 메모 참고]
3. **Object Name**에 이름을 정한다. ⚠️ **이 이름을 기억**해 둘 것 — 3단계에서 똑같이 입력해야 매핑된다.
   - 권장: 공백 없는 식별자 (예: `Tree_Oak`, `Rock_01`, `Bush_A`).
4. (선택, 충돌용) 프리팹 루트에 **MultiBoundingVolume** 컴포넌트를 붙여 충돌 박스를 작성하면 `Model::bvh`가 추출된다.
   - 렌더만 필요하면 없어도 화면엔 나오지만, **나무/바위 충돌**(구현됨)에는 이 단계가 필수다(자세히는 scatterSystem.md "충돌(Physics)" 절). 서버 충돌까지 쓰려면 `<name>Server.bin`도 재추출.
5. **Export as binary (.bin)** → 출력된 `.bin`을 `../resources/models/props/{Object Name}.bin` 으로 배치.
   - 즉 `Object Name`이 `Tree_Oak`이면 파일은 `../resources/models/props/Tree_Oak.bin`.
6. 모델이 참조하는 **텍스처들**도 기존 모델(플레이어/고블린) 추출과 동일하게 엔진의 텍스처 경로에 배치.

> 폴더 `../resources/models/props/`가 없으면 새로 만든다.

---

## 2. (빌보드 풀/꽃) — 별도 추출 불필요

빌보드 텍스처는 다음 단계(TerrainExtractor Export)에서 `scatter_*.png`로 함께 출력된다. 여기서 따로 할 일 없음.

---

## 3. 지형 + 산포 데이터 추출 — TerrainExtractor

1. Unity 메뉴 **Tools ▸ Terrain Extractor** 열기.
2. **Terrain Root**: 모든 Terrain chunk를 자식으로 가진 부모 오브젝트 지정(기존 지형 추출과 동일).
3. **Scatter** 설정:
   - `Include Trees` / `Include Details` 체크.
   - `Detail Density Scale`(0~1): `terrain.detailObjectDensity`에 곱해지는 **추가** 배율. `1.0`이면 Unity에서 보이는 디테일 양과 동일. 더 솎고 싶을 때만 낮춘다.
   - `Max / Detail Type / Chunk`: **디테일 종류별·chunk별** 안전 상한(기본 20000). 보통 실제 개수가 이보다 작아 걸리지 않는다.
   > 디테일 개수는 Unity의 `ComputeDetailInstanceTransforms`로 **Unity 실제 렌더 인스턴스를 그대로** 가져온다(Coverage 모드·해상도·perPatch·density 자동 반영). 따라서 **Unity 에디터에서 보이는 수와 일치**한다. **Unity 2022.2 이상(현재 Unity 6)** 필요.
4. **Scan Prototypes** 클릭 → 발견된 프로토타입 목록이 뜬다.
   - 각 행: `[종류] [이름 입력칸] [원본 prefab/텍스처 이름]`.
   - ⚠️ **각 메시 프로토타입(Tree/MeshDtl)의 이름 입력칸을 1단계에서 정한 `Object Name`과 정확히 동일하게** 입력.
     - 예: 1단계에서 `Tree_Oak`로 추출했으면 여기서도 `Tree_Oak`.
   - 빌보드(Billboard) 행의 이름은 자유(텍스처 파일명에 쓰임).
5. **Export All Chunks** 클릭.
   - 출력물(기존 지형 출력 + 추가):
     - `chunks_index.bin` (버전 3, 산포 데이터 + per-instance 회전 쿼터니언 포함)
     - `scatter_{i}_{이름}.png` (빌보드 텍스처)
     - 기존 `layer_*_*.png`, `chunk_*_height.raw`, `chunk_*_splat0.png`
   - 콘솔에 `Scatter: N prototypes, M instances ...` 로그가 찍히는지 확인.

---

## 4. PNG → DDS 변환 + 파일 배치

엔진은 `.dds`를 읽는다(인덱스에 `.dds` 경로가 기록됨). 기존 지형 텍스처 변환 과정에 **scatter 텍스처만 추가**된다.

1. 다음 PNG들을 모두 **DDS로 변환**(기존에 쓰던 변환 도구/스크립트 사용):
   - `layer_*_diffuse.png`, `layer_*_normal.png` (팔레트)
   - `chunk_*_splat0.png` (스플랫)
   - `scatter_*_*.png` (**신규** — 빌보드 텍스처)
   - 빌보드 텍스처는 **알파 채널 보존** 포맷으로 변환(BC3/BC7). 알파가 컷아웃 마스크다.
2. 변환된 파일 + `chunks_index.bin` + `chunk_*_height.raw`를 모두 게임의 지형 디렉터리에 배치:
   - **`../resources/terrains/`** (client `init()`이 이 경로를 사용; 파일명으로 해석되므로 평면 폴더면 됨).
3. 메시 프롭 `.bin`은 1단계대로 **`../resources/models/props/`**.

최종 배치 요약:
```
../resources/terrains/      chunks_index.bin, layer_*.dds, chunk_*_height.raw,
                            chunk_*_splat0.dds, scatter_*_*.dds
../resources/models/props/  Tree_Oak.bin, Rock_01.bin, Bush_A.bin ... (+ 각 텍스처)
```

---

## 5. 실행 & 확인

1. 클라이언트 실행(standalone 또는 online) → 해당 chunk에 진입.
2. 부팅 로그 확인:
   - `[Terrain] Chunk index parsed: ... , N scatter prototypes`
   - `[ChunkManager] Scatter assets loaded: P prop models, B billboard prototypes.`
3. 나무/바위/덤불/풀이 Unity와 동일한 위치·밀도로 보이면 성공.

---

## 6. 트러블슈팅

| 증상 | 원인 / 해결 |
|---|---|
| 특정 나무/바위가 **안 보임**, 로그에 `Scatter prop model not found: .../{이름}.bin` | 3단계 이름 ≠ 1단계 `Object Name`, 또는 `.bin`이 `../resources/models/props/`에 없음. 이름을 정확히 일치시키고 파일 배치. |
| 풀/꽃이 **흰 사각형** | 빌보드 텍스처 DDS 누락/경로 불일치. `scatter_*.dds`가 `../resources/terrains/`에 있는지 확인. |
| 풀/잎 가장자리에 **검은 테/불투명 사각형** | DDS에 알파 채널이 없음(BC1로 변환됨). BC3/BC7로 재변환. |
| 나뭇잎이 **한쪽 면만** 보임 | 트리 `.bin`의 잎 메시가 단면. (빌보드는 자동 양면) 트리 프리팹의 잎을 양면 메시로 author하거나 감수. |
| 잎/풀 **그림자가 사각형(컷아웃 안 됨)** | 알려진 한계: 그림자 패스는 알파 테스트 미적용. 현재는 정상 동작. |
| 풀이 **너무 빽빽/렉** | `Detail Density Scale` 낮추거나 `Max / Detail Type / Chunk` 줄여 재추출. |
| **한 종류만 수만 개, 나머지 0개** | (구버전 버그) 종류별 독립 예산으로 수정됨. Export 로그의 `raw density sum`으로 종류별 실제 칠한 양 확인. |
| 지형 갱신했는데 그대로 | 포맷 변경(현재 v3) 시 **chunks_index.bin 전체 재추출** 필요. 옛 파일 교체 확인. |
| 나무가 지면에 **뜨거나 박힘** | 보통 ground-snap으로 보정되지만, height map과 다른 지형을 썼다면 height.raw도 같이 재추출했는지 확인. |
| **나무(LOD 프리팹)가 통째로 안 보임** | 커스텀/SpeedTree 셰이더의 albedo 슬롯이 `_MainTex/_BaseMap`이 아니라 추출 시 albedo 맵이 비었고, 폴리지 컷오프로 전부 클리핑된 것. `ModelExtractor`가 셰이더 텍스처 프로퍼티를 훑어 자동 탐색하도록 수정됨 → **해당 프롭 `.bin` 재추출**. (LOD0 지오메트리 자체는 정상 추출됨) |
| 디테일(바위 등) **기울기가 Unity와 다름/안 기울어짐** | "Align To Ground(%)" 틸트는 v3부터 추출 시 베이크된다. **chunks_index.bin을 v3로 재추출**(Unity 2022.2+ 필요). |

---

## 6.5 디테일 그리기 거리 (중요)

디테일/풀은 **플레이어 반경 `TerrainChunkManager::kDetailCullRadius`(기본 80m) 안에서만** 렌더된다(Unity의 Detail Distance와 동일 개념). 인스턴싱 버퍼(`perInstanceData`)가 DrawEvent당 1칸 고정 용량이라, 칠한 총량이 많아도 매 프레임 그리는 수를 근거리로 한정해 버퍼 오버플로(→해당 패스 렌더 실패)를 막기 위함이다. **트리는 랜드마크라 거리 무관하게 항상 렌더**된다.
- 더 멀리 보이게 하려면 `kDetailCullRadius`를 키운다(단, 근거리 인스턴스 수↑ → 버퍼 용량 `gfx.cpp`의 PBRDeferred/forward `perInstanceData`도 함께 키워야 안전).

## 7. 참고 / 한계

- 컷오프 임계값은 `TerrainChunkManager::kFoliageAlphaCutoff`(0.33). 잎/풀이 과도하게 깎이거나 덜 깎이면 이 값을 조정 후 재빌드.
- 빌보드는 **고정 십자 quad**(카메라 추종 회전 미구현).
- **충돌은 구현됨**(`ScatterCollider`): 1단계 `MultiBoundingVolume`로 작성한 BV가 prop 모델 BVH로 추출되어 정적 충돌(플레이어/몬스터·카메라 차단)에 사용된다. 충돌이 필요한 prop은 BV를 반드시 작성하고, 서버용 `<name>Server.bin`(BV-only)을 재추출할 것. 상세: `docs/scatterSystem.md` 충돌 절.
- 빌보드(디테일/풀)는 BV를 추출하지 않아 비충돌이다(렌더 전용). 충돌은 트리/바위 같은 메시 prop만.
- 서버(`RoomServer`)도 같은 `chunks_index.bin`을 읽어 산포 인스턴스를 **저장**(몬스터-prop 충돌 권위). v2/v3 포맷 모두 정상 동작(버전별 분기).

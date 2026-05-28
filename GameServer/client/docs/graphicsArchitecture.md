### 그래픽스 아키텍처
`GFX` - 렌더링을 총괄 책임지는 클래스

- 코어: `gfx.hpp`, `gfxUtil.hpp`, `mesh.hpp`, `shader.hpp`, `font.hpp`, `collision.hpp`
- 파이프라인: `pbrPipeline.hpp`, `pbrSkinnedPipeline.hpp`, `pbrDeferredPipeline.hpp`, `pbrDeferredSkinnedPipeline.hpp`, `billboardPipeline.hpp`, `bvPipeline.hpp`, `samplePipeline.hpp`, `skyboxPipeline.hpp`, `uiPipeline.hpp`, `terrainPipeline.hpp`, `terrainDeferredPipeline.hpp`, `sharedResources.hpp`

#### 장치 초기화
- `GFX::setupDXGI`
- `GFX::init`
- `GFX::createSwapChain`

#### d3d12단 리소스 생성
1. `게임` --[addRequest...]--> `GFX`
2. `GFX::loadAssets`: `CommandContext`를 할당해 요청들을 한 번에 처리

#### 객체 그리기
1. `게임` --[DrawEvent 제공/부수 정보(CameraData, LightData 등) 제공]--> `GFX`
2. `GFX::render`: `CommandContext`들을 할당해 스레드 풀 활성화 여부에 따라 멀티스레드 렌더링/싱글스레드 렌더링을 수행
   a. 멀티스레드 렌더링 시 master/slave 구조로 렌더링이 이루어짐
   b. 3개의 백버퍼를 활용, 가용 백버퍼가 있다면 present를 wait하지 않고 곧바로 다음 렌더링을 수행

`GFX`에 객체 그리기를 요청할 때에는 어떤 파이프라인을 통할지를 정해야 함
함수 오버로딩을 통해 어떤 파이프라인에 종속된 인자를 전달하느냐에 따라 결정
`GFX::renderPath()` 로 Forward / Deferred 경로를 런타임에 선택 가능

파이프라인은 모두 개별 네임스페이스를 가지고 있음
파이프라인은 여러 개의 렌더링 패스를 가질 수 있으며, 각 패스의 실행에 대한 함수를 public하게 제공

**파이프라인별 리소스 구조체 작성 규칙:**
- `LightData`, `CameraData`, `FrameData`, `Resources` 등은 내용이 동일하더라도 `using` alias 대신 각 파이프라인 namespace에 직접 작성
- 파이프라인이 독립적으로 진화할 수 있도록 하기 위함

**Root Parameter 레지스터 규약 (전 파이프라인 공통):**
- `PerDrawcallData` → b0
- `PerFrameData` → b1
- `PerInstanceData` → t0, `LightData` → t1, `BoneData` → t2
- 새 셰이더 cbuffer 선언 시 반드시 이 규약을 따를 것 (잘못된 register 사용 시 데이터 미전달)

#### 셰이더의 추가/수정

- `.hlsl`의 확장자를 가진 셰이더 파일에 대해 추가/수정
- `shader.hpp`, `shader.cpp`에 `create...Shader`와 같이 셰이더 생성 함수 추가
- `shader.hpp`에 cpu-gpu 메모리 레이아웃이 같도록 각 셰이더의 네임스페이스에 리소스 구조체들 추가
- 해당 셰이더와 연관된 파이프라인 수정: 파이프라인 리소스 구조체들과 셰이더의 구조체들간 수정사항 동기화 필요, 렌더링 패스들 업데이트 필요

#### 렌더 패스 실행 순서 (gfx.cpp render())

**Forward Path (기본):**
1. shadowPass(PBR) → shadowPass(PBRSkinned) → **shadowPass(Terrain)**
2. mainPass(PBR) → mainPass(PBRSkinned)
3. **mainPass(Terrain)** — shadow map SRV 상태에서 실행, 그림자 수신 O, 단일 스레드
4. mainPass(Skybox) → mainPass(BV) → mainPass(Billboard)
5. mainPass(UI)

**Deferred Path (`GFX::RenderPath::Deferred`):**
1. GBuffer clear (GB0~GB3 RTV + GBuffer DSV)
2. shadowPass(PBRDeferred) → shadowPass(PBRDeferredSkinned) → **shadowPass(Terrain)**
3. gBufferPass(PBRDeferred) → **gBufferIndirectPass(PBRDeferredSkinned)** → **gBufferPass(Terrain)** — MRT 4개(GB0~GB3) + GBuffer DSV에 기록
   - PBRDeferredSkinned는 Hi-Z 5단계 compute(Clear→Cull→PrefixSum→Compact→Command) 후 indirect draw 실행
   - Hi-Z Cull Pass에서 visibleFlags 생성 → Compact Pass 이후 CPU readback 복사 (1-frame delay)
4. GBuffer 상태 전환: RTV→SRV, GBuffer DSV→SRV (`transitionToRead`)
5. Lighting Pass — fullscreen triangle `DrawInstanced(3,1,0,0)`, GBuffer SRV 읽기, backbuffer RTV 출력
6. **GBuffer depth → backbuffer DSV 복사** (`copyResource`) — 이후 Forward 패스가 올바른 깊이 기준으로 렌더링하도록
7. mainPass(Skybox) → mainPass(BV) → mainPass(Billboard) ← Forward-always 패스
8. mainPass(UI)

Skybox / Billboard / UI는 renderPath에 관계없이 항상 Forward로 실행 (GBuffer에 기록하지 않음).
Terrain은 Deferred path에서 gBufferPass를 통해 GBuffer에 기록하고, Forward path에서만 mainPass로 실행한다.

#### CSM (Cascaded Shadow Mapping)

4-cascade CSM이 PBR / PBRSkinned / Terrain 파이프라인에 모두 적용되어 있다.

**설계 방식:** GS + Texture2DArray 대신 **cascade별 개별 Texture2D + 4-pass draw** 방식.

**cascade split:** `light.cpp::updateCSMCascades()` — Practical Split Scheme
```
C_i = lambda * nearZ*(farZ/nearZ)^((i+1)/N) + (1-lambda)*(nearZ + (farZ-nearZ)*(i+1)/N)
```
- 기본 파라미터: nearZ=0.1, farZ=500, lambda=0.75
- texel snapping으로 shadow swimming 제거 (cascade별 독립 해상도 사용)

**Normal Offset Shadow Bias:** (`pbrLighting.hlsli::sampleCascadePCF`)
- world-space normal 방향으로 `offset * sinTheta` 만큼 샘플 위치를 오프셋
- `sinTheta = sqrt(1 - NdotL²)` — 법선이 광원에 수직일수록 오프셋 최대
- back-lit 면(`rawNdotl < 0`)은 sinTheta=0 → 오프셋 없음 (잘못 lit 마킹 방지)
- offset 크기: `worldUnitsPerTexel * 2.0f` (cascade별 독립, `cascadeNormalOffsets[4]`)

**Cascade Blending:** (`pbrLighting.hlsli::calcCSMShadow`)
- cascade 경계 15% 구간에서 현재·다음 cascade를 `smoothstep` lerp
- 경계 팝핑(popping) 없이 부드러운 전환

**cbuffer 데이터 흐름 (PBR/PBRSkinned/Terrain 공통):**
```
Light::updateCSMCascades()
  → cascadeViews_[], cascadeProjs_[], cascadeSplitsFarV_, cascadeNormalOffsets_[]
  → render() → Pipeline::mainUpdate() → PerFrameData { lightVP[4], cascadeSplitsFarV, cascadeNormalOffsets }
```

**CSM 디버그:** `#define CSM_DEBUG_VIS` — cascade별 색상 오버레이 (R/G/B/Y)

**주의사항:**
- cascade DSV clear는 `gfx.cpp::clearCSMAllShadowMaps()`에서만 수행 (각 파이프라인 shadowDraw에서 개별 클리어 금지)
- MT shadow draw latch count = `cascadeCount * jobCnt` (cascadeCount만이면 버그)
- `cascadeNormalOffsets`의 rawNdotl은 **saturate 금지** — back-lit 감지에 음수 값이 필요

#### Deferred Shading (PBRDeferredPipeline / PBRDeferredSkinnedPipeline)

**파일:**
- `pbrDeferredPipeline.hpp/cpp`, `pbrDeferredSkinnedPipeline.hpp/cpp`
- `pbrDeferred.hlsl` (정적 메시 GBuffer geometry pass)
- `pbrDeferredSkinned.hlsl` (스킨드 메시 GBuffer geometry pass)
- `pbrDeferredLighting.hlsl` (fullscreen triangle deferred lighting pass)
- `sharedResources.hpp/cpp` — `SharedResources::GBuffer` 네임스페이스

**GBuffer 레이아웃 (`SharedResources::GBufferData`):**

| 슬롯 | 포맷 | 내용 |
|------|------|------|
| GB0 | R8G8B8A8_UNORM | Albedo.rgb (linear) + AO.a |
| GB1 | R16G16_FLOAT | NormalV oct-encoded (view-space), 클리어값 (0.5, 0.5) → (0,0,1) |
| GB2 | R8G8B8A8_UNORM | LightAccum.rgb (ambient+emissive 선계산) + Roughness.a |
| GB3 | R8_UNORM | Metallic |
| Depth | R32_TYPELESS (DSV=D32_FLOAT, SRV=R32_FLOAT) | Scene depth |

**Normal Oct Encoding (`pbrLighting.hlsli`):**
- `octEncode(float3 n)` — view-space unit normal → float2 [0,1]
- `octDecode(float2 oct)` — float2 [0,1] → view-space unit normal
- GB1 클리어 색상 (0.5, 0.5, 0, 0) → octDecode → 정면 법선 (0,0,1)

**pbrLighting.hlsli include 방식:**
- `pbrDeferred.hlsl`, `pbrDeferredSkinned.hlsl`: 일반 `#include` (geometry pass용)
- `pbrDeferredLighting.hlsl`: `#define DEFERRED_LIGHTING_PASS` 후 `#include` — `illuminateCSM()` guard 활성화

**Lighting Pass 설계:**
- VS: SV_VertexID 기반 fullscreen triangle (VB 없음), `DrawInstanced(3, 1, 0, 0)`
- PS: GB0~Depth SRV 샘플링 → depth+invProj→posV, posV+invView→posW, octDecode→normalV, invView→normalW
- LightData: 별도 StructuredBuffer (t1)로 전달 (`deferredLightingLightData_`)
- PerFrameData: 단일 ConstantBuffer (b1) — CSM 데이터 + invView/invProj + GBuffer SRV bindless 인덱스 + debugMode

**GBuffer depth → backbuffer DSV 복사:**
- Lighting pass cmdList와 동일 batch에서 `copyResource` 실행
- GBuffer DSV (D32_FLOAT) 내용을 backbuffer depth buffer에 복사
- 이후 Skybox/Terrain/Billboard 등 Forward-always 패스가 올바른 깊이를 기준으로 렌더링 가능

**GBuffer 디버그 뷰:**
- 'G' 키 → `GFX::cycleGBufferDebugMode()` → `gBufferDebugMode_` (0~7) 순환
- 순서: None → Albedo → Normal → AO → Roughness → Metallic → LightAccum → Depth
- Lighting PSO의 `debugMode` (cbuffer b1 내 uint) 로 전달, PSO permutation 불필요

**주의사항:**
- GBuffer DSV와 backbuffer DSV는 별개 리소스 — Deferred path에서 geometry pass는 GBuffer DSV 사용, depth 복사 없이 Forward 패스를 이어 실행하면 깊이가 초기화된 상태로 모든 Forward 오브젝트가 GBuffer 위에 그려짐
- `PBRDeferredSkinnedPipeline`의 Lighting pass는 직접 담당하지 않음 — `PBRDeferredPipeline`의 Lighting pass가 정적/스킨드 GBuffer를 모두 처리

#### Hi-Z Occlusion Culling (PBRDeferredSkinnedPipeline)

파일: `pbrDeferredSkinnedPipeline.hpp/cpp`

**5단계 GPU 파이프라인 (`hiZPassCompute()`):**
1. **Clear** — perGroupCnt / groupOffsets / visibleFlags 초기화
2. **Cull** — HiZ depth map으로 각 DrawEvent visibility 판정 → `visibleFlags[]` (u32t per DrawEvent, 0=culled, 1=visible)
3. **PrefixSum** — groupOffsets 계산
4. **Compact** — visibleFlags(SRV)를 읽어 visibleIndices 작성
5. **Command** — indirect draw args 생성 → `gBufferIndirectDraw()`에서 소비

**visibleFlags readback (1-frame delay):**
- Compact Pass 이후, `CopyBufferRegion`으로 `visibilityReadback`(READBACK heap)에 복사
- 다음 프레임 `hiZPassUpdate()`에서 CPU 읽기 → `objectVisibility[]` 집계 (OR)
- `DrawEvent::renderObjectId` 쿠키로 GPU→CPU 역매핑 (GFX/game 레이어 분리 유지)

**readback 타이밍 규칙:** `visibleFlags`는 Compact Pass에서 SRV로 소비되므로, CopyBufferRegion은 반드시 Compact Pass 완료 이후에 수행. Cull Pass 직후 복사 시 상태 전환 충돌.

**visibleFlags readback 용도:** `objectVisibility[]` 집계를 통한 **애니메이션 최적화 및 디버그 전용**.
Draw call skip에는 사용하지 않는다 — GPU indirect draw가 직접 instance count를 결정하므로.

**컬링 아키텍처 — compact event array 설계:**

`Object::render()`는 항상 DrawEvent를 제출한다 (컬링 상태로 제출 차단 없음).
DrawEvent 내 플래그로 컬링 상태를 전달하며, `sortDrawEvents()` 시점에 compact 배열로 분리:

| compact 배열 | 조건 | 사용 패스 |
|---|---|---|
| `gBufferEvents_` | `!viewFrustumCulled` | gBuffer 패스 전체 (direct + Hi-Z indirect) |
| `shadowEvents_` | `!shadowCulled` | shadow 패스 전체 |

- `perInstanceData`는 항상 compact — culled 인스턴스는 삽입조차 안 됨 (zero-init placeholder 없음)
- `firstInstanceOffset = gFirst - gBufferEvents_.begin()` (또는 `shadowEvents_`) — compact 배열 기준 절대 오프셋
- Hi-Z 5단계 compute 및 indirect draw 도 `gBufferEvents_` 기준으로 동작
  - `hiZPassUpdate()`: `perInstanceDataCull/Compact`, `lastDrawEventObjectIds` 크기 = `gBufferEvents_.size()`
  - `hiZPassCompute()`: Cull/Compact dispatch = `gBufferEvents_.size()`, readback 복사 크기 동일

**컬링 플래그 분리 (self-reinforcing culling 방지):**

| 플래그 | 위치 | 역할 |
|---|---|---|
| `viewFrustumCulled` | `DrawEvent` 내 | gBuffer compact 배열 필터링 기준 |
| `shadowCulled` | `DrawEvent` 내 | shadow compact 배열 필터링 기준 |
| `hiZCulled_` | `Object` 멤버 | `Object::update()` + AnimBlender — 물리/애니 연산 스킵 |

Hi-Z culled 오브젝트도 `viewFrustumCulled`가 false인 한 DrawEvent를 계속 제출해야 함.
DrawEvent를 차단하면 Hi-Z 파이프라인이 visibility 변화를 감지할 수 없어 영구 culled 상태(self-reinforcing) 발생.

**새 오브젝트를 Hi-Z culling에 참여시킬 때 필수 체크리스트:**
1. `setupStage()`에서 `obj->setRenderObjectId(nextRenderObjId++)` 할당
2. `gfx_.setMaxRenderObjectId(nextRenderObjId - 1u)` 호출
3. `applyHiZCulling()` 내에 `applyToEntity(obj)` 추가
4. Hi-Z OFF(`isHiZCullEnabled() == false`) 상태에서도 `setHiZCulled(false)` + `animBlender->setCulled(isFrustumCulled())` 복원 필요

#### TerrainPipeline 특성

- 파일: `terrain.hpp/cpp`, `terrain.hlsl`, `terrainPipeline.hpp/cpp`, `terrainShadowMap.hlsl`
- **shadow pass 있음** — 지형 기하가 공유 shadow map("ShadowMap" DSV)에 기록되어 PBR 객체 위에 지형 그림자를 드리움
- shadow pass: `shadowPass()` / `shadowPassMT()` (MT는 단일 스레드 위임, draw event 수가 적어 MT 효과 없음)
- shadow shader: `terrainShadowMap.hlsl` — position-only VS, PS 없음, NumRenderTargets=0
- 리소스 로드: `loadTerrainFromFiles()` — manifest 파싱 → height.raw 메시 빌드 → 텍스처 로드
- **manifest 태그 순서**: `HeightMap → SplatPath(s) → DiffusePath(s) → MetaData` (MetaData가 마지막)
- VB 5슬롯: Position(0) / Normal(1) / Tangent(2) / Bitangent(3) / UV(4), IB 32-bit (513×513 정점 초과 가능)
- Tangent/Bitangent: `terrain.cpp buildTerrainMesh()`에서 중앙 차분 + Gram-Schmidt로 CPU 사전 계산
- Splat map: RGBA 채널 = 레이어 0~3 블렌딩 가중치, 각 레이어마다 diffuse + normal map
- Normal map 포맷: **Unity DXT5nm** — X는 Alpha 채널, Y는 Green 채널, R은 더미(1.0) → `nmSample.ag * 2 - 1`로 읽어야 함
- Normal mapping: `hasAnyNormal` 플래그(cbuffer b0)로 조건부 처리, `float3x3(tangentV, bitangentV, normalV)` 패턴 (pbr.hlsl과 동일)
- `terrain.hlsl`에서 `pbrLighting.hlsli` include 시 `#define TERRAIN_SHADER` 필수 — `illuminate()` 스킵 가드
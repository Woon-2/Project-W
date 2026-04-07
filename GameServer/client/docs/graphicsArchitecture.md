### 그래픽스 아키텍처
`GFX` - 렌더링을 총괄 책임지는 클래스

- 코어: `gfx.hpp`, `gfxUtil.hpp`, `mesh.hpp`, `shader.hpp`, `font.hpp`, `collision.hpp`
- 파이프라인: `pbrPipeline.hpp`, `pbrSkinnedPipeline.hpp`, `billboardPipeline.hpp`, `bvPipeline.hpp`, `samplePipeline.hpp`, `skyboxPipeline.hpp`, `uiPipeline.hpp`, `terrainPipeline.hpp`, `sharedResource.hpp`

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

파이프라인은 모두 개별 네임스페이스를 가지고 있음
파이프라인은 여러 개의 렌더링 패스를 가질 수 있으며, 각 패스의 실행에 대한 함수를 public하게 제공

#### 셰이더의 추가/수정

- `.hlsl`의 확장자를 가진 셰이더 파일에 대해 추가/수정
- `shader.hpp`, `shader.cpp`에 `create...Shader`와 같이 셰이더 생성 함수 추가
- `shader.hpp`에 cpu-gpu 메모리 레이아웃이 같도록 각 셰이더의 네임스페이스에 리소스 구조체들 추가
- 해당 셰이더와 연관된 파이프라인 수정: 파이프라인 리소스 구조체들과 셰이더의 구조체들간 수정사항 동기화 필요, 렌더링 패스들 업데이트 필요

#### 렌더 패스 실행 순서 (gfx.cpp render())

1. shadowPass(PBR) → shadowPass(PBRSkinned) → **shadowPass(Terrain)**
2. mainPass(PBR) → mainPass(PBRSkinned)
3. **mainPass(Terrain)** — shadow map SRV 상태에서 실행, 그림자 수신 O, 단일 스레드
4. mainPass(Skybox) → mainPass(BV) → mainPass(Billboard)
5. mainPass(UI)

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
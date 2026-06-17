### 그래픽스 아키텍처
`GFX` - 렌더링을 총괄 책임지는 클래스

- 코어: `gfx.hpp`, `gfxUtil.hpp`, `mesh.hpp`, `shader.hpp`, `font.hpp`, `collision.hpp`
- 파이프라인: `pbrPipeline.hpp`, `pbrSkinnedPipeline.hpp`, `pbrDeferredPipeline.hpp`, `pbrDeferredSkinnedPipeline.hpp`, `billboardPipeline.hpp`, `bvPipeline.hpp`, `samplePipeline.hpp`, `skyboxPipeline.hpp`, `uiPipeline.hpp`, `terrainPipeline.hpp`, `terrainDeferredPipeline.hpp`, `sharedResources.hpp`
- 후처리/IBL: `TonemapPipeline.hpp`(ACES+exposure resolve), `BloomPipeline.hpp`(HDR bloom), `iblPrecomputePipeline.hpp`(IBL 맵 프리컴퓨트) — 상세는 아래 "HDR + IBL + Bloom 파이프라인"

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

**Deferred Path (인게임 기본; 로비는 Forward):**
1. GBuffer + **SceneColorHDR** clear (GB0~GB4 RTV + GBuffer DSV + SceneColorHDR RTV)
1b. **(Hi-Z ON)** Occluder pass: `occluderPass(TerrainDeferred)` → `occluderPass(PBRDeferred)` — 지형 + 근거리 BVH prop을 position-only depth로 Hi-Z source depth에 기록 → Hi-Z mip pyramid build → `hiZPass(PBRDeferredSkinned)` + **`hiZPass(PBRDeferred)`** (cull/compact/command)
2. shadowPass(PBRDeferred) → shadowPass(PBRDeferredSkinned) → **shadowPass(Terrain)**
3. gBufferPass(PBRDeferred, direct) → **gBufferIndirectPass(PBRDeferred)** → **gBufferIndirectPass(PBRDeferredSkinned)** → **gBufferPass(Terrain)** — MRT 4개(GB0~GB3) + GBuffer DSV에 기록. **GB2.rgb = emissive 전용**(ambient는 lighting 패스 IBL로 이동)
   - PBRDeferredSkinned/PBRDeferred 모두 Hi-Z 5단계 compute(Clear→Cull→PrefixSum→Compact→Command) 후 indirect draw 실행. compute 셰이더 5종 + `cmdSig_`는 공유
   - **PBRDeferred Hi-Z**(정적 prop, 2026-06-15): `occludeeCandidate` DrawEvent(=VFC 통과 BVH prop)만 indirect 대상. visibility feedback ring/CPU readback **없음**(정적이라 anim/물리 스킵 불필요; cull u3 출력은 scratch로 폐기). 비-occludee는 `gBufferPass` direct
   - PBRDeferredSkinned Hi-Z: Cull→visibleFlags + visibility feedback 2-slot ring → CPU readback(1-frame delay, anim/물리 스킵용)
4. GBuffer 상태 전환: RTV→SRV, GBuffer DSV→SRV (`transitionToRead`)
5. Lighting Pass — fullscreen triangle `DrawInstanced(3,1,0,0)`, GBuffer SRV 읽기, **SceneColorHDR(R16G16B16A16_FLOAT)에 선형 HDR 출력**. `color = directLight + computeIBL + emissive`, 이후 fog 적용. **톤매핑은 여기서 안 함**(resolve 담당)
6. **GBuffer depth → backbuffer DSV 복사** (`copyResource`) — 이후 Forward 오버레이가 올바른 깊이 기준으로 렌더링하도록
7. SceneColorHDR 상태 전환: RTV→SRV (`SceneColor::transitionToRead`)
8. **Bloom** (`gBufferDebugMode_==0`일 때만) — SceneColorHDR → bloom 밉체인(prefilter→downsample→additive upsample), mip0 → SRV
9. **Tonemap resolve** — SceneColorHDR(+ bloom mip0 가산) → exposure → ACES Filmic → gamma → **backbuffer(LDR)**
10. Forward-always 오버레이(backbuffer, resolve 이후): Skybox(raw) → BV → Billboard → 파티클류
11. mainPass(UI)

Skybox / Billboard / UI / 파티클은 renderPath에 관계없이 항상 backbuffer에 직접 그린다(SceneColorHDR·GBuffer 미사용). Forward path(로비)는 HDR/Bloom/resolve를 거치지 않고 셰이더 내 inline tonemap으로 backbuffer에 직접 출력한다.
Terrain은 Deferred path에서 gBufferPass로 GBuffer에 기록, Forward path에서만 mainPass로 실행한다.

#### CSM (Cascaded Shadow Mapping)

4-cascade CSM이 PBR / PBRSkinned / Terrain 파이프라인에 모두 적용되어 있다.

**설계 방식:** GS + Texture2DArray 대신 **cascade별 개별 Texture2D + 4-pass draw** 방식.

**cascade split:** `light.cpp::updateCSMCascades()` — Practical Split Scheme
```
C_i = lambda * nearZ*(farZ/nearZ)^((i+1)/N) + (1-lambda)*(nearZ + (farZ-nearZ)*(i+1)/N)
```
- 기본 파라미터: nearZ=0.1, farZ=500, lambda=0.75
- texel snapping으로 shadow swimming 제거 (cascade별 독립 해상도 사용)

**Camera-relative 정밀도 — shadow shimmering 해결 (2026-06):** follow camera에서만 나타나던 그림자 떨림의
근본 원인은 청크 원점(~월드 5500,5500)의 큰 좌표가 CSM 행렬 연산을 거치며 float 정밀도를 잃은 것이었다
(CSM 맵 생성 버그 아님 — cascade 디버그 뷰에서 cascade map 자체가 흔들리는 것으로 확정). 카메라-상대 공간이 실제 해결책이며, 다음 표준 기법으로 해결:
- **카메라-상대 그림자 공간:** `updateCSMCascades`가 frustum corner를 카메라 eye 원점 기준(camera-relative)으로
  구성 → cascade ortho bounds·`lightVP`가 `posW - camPos`를 light NDC로 매핑. caster(depth 패스)·receiver(셰이딩)
  양쪽이 셰이더에서 `camPos`를 먼저 빼고 `lightVP`를 적용해 ~5500 대형 좌표가 light-space 연산에 진입하지 않음
  (**caster·receiver를 반드시 함께 rebase** — 한쪽만 하면 그림자 깨짐). `Light::cascadeCameraPos()`로 기준 eye 노출.
- **frustum corner 직접 생성:** `inverse(camView*camProj)` 제거. 카메라 basis(camView 상위 3x3 열 = 직교 transpose,
  exact)·fov(camProj `m11`,`m00`)·per-cascade near/far로 corner를 카메라-상대 공간에서 직접 계산(역행렬 정밀도 손실 제거).
- **texel center snap 유지:** sphere center XY를 `worldUnitsPerTexel(=2·radius/res)` 격자에 스냅(radius가
  rotation-invariant라 프레임 간 안정). per-cascade **radius 양자화는 시도 후 제거** — 이산 radius 스텝이 이동 중
  오히려 떨림을 유발했고, 카메라-상대 공간만으로 shimmer가 해소됨(2026-06 사용자 검증).
- 보조: deferred lighting은 GBuffer `gb4`(linear view-Z, R32F)로 posV를 정확 복원(NDC 깊이 양자화 제거) — 단독으론 소폭 개선.
- **주의:** cascade `lightVP`가 카메라-상대 공간이므로, 절대 월드 BVH로 cull/test하는 코드는 `cascadeCameraPos()`로
  rebase 필요. standalone `game.cpp::cullObjectsForShadow`는 AABB/OBB center를 `- cascadeCameraPos()`로 rebase함(online엔 이 패턴 없음).

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

#### Alpha-tested (masked) foliage shadows

기본 그림자 패스는 PS 없는 depth-only라 알파를 무시 → 나뭇잎/풀 등 alpha-cutout 객체가
**사각형 그림자**를 드리운다. 해결: cutout 캐스터만 별도 masked PSO로 분기(업계 표준).

- 셰이더: `shadowMapCSMMasked.hlsl` — Position+UV VS + bindless albedo.a 샘플 후 `clip(a - cAlphaCutoff)` PS.
  공용 `DefaultRootSig`(모든 셰이더 공유, bindless 텍스처/샘플러 풀 포함)를 사용해 그림자 패스에서도
  bindless 샘플링이 가능. PSO: `createShadowMapCSMMaskedShader`(`shader.cpp`), `CULL_NONE`(얇은 foliage 양면).
- 분기: `PBRDeferredPipeline::shadowDraw/shadowDrawMT`가 그룹별 `material->constantAlphaCutoff > 0`이면
  masked PSO + `"PBRDeferredPipeline_ShadowMasked"`(Position+UV) VB + 텍스처/샘플러 풀 바인딩 +
  `perDrawcallDataMasked`(b0: firstInstanceOffset@0 + idxAlbedo + cutoff). 나머지는 기존 고속 depth-only 경로 유지.
- 적용 범위: **정적 deferred 메시(PBRDeferred)만**. 스킨드/지형 foliage가 생기면 동일 패턴으로 변형 추가 필요.
- 전제: foliage 머티리얼이 `constantAlphaCutoff > 0`(`mesh.hpp`)이어야 분기됨(아니면 무동작).

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
| GB2 | R8G8B8A8_UNORM | **Emissive.rgb** (ambient/IBL는 lighting 패스로 이동) + Roughness.a |
| GB3 | R8_UNORM | Metallic |
| GB4 | R32_FLOAT | Linear view-space Z (posV.z) — deferred 복원이 NDC 깊이 양자화 대신 사용 |
| Depth | R32_TYPELESS (DSV=D32_FLOAT, SRV=R32_FLOAT) | Scene depth |

> **주의:** GB2.rgb는 emissive 전용이다. `pbrDeferred.hlsl`·`pbrDeferredSkinned.hlsl`·`terrainDeferred.hlsl` 모두 `lightAccum = emissive`(지형/스킨드 모두)로 기록해야 한다. 과거 스킨드 셰이더만 `globalAmbient*albedo`를 굽던 버그가 있었고(이중 ambient: GB2 상수 ambient + lighting 패스 IBL), 셋 다 emissive-only로 통일했다.

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
- 출력: `color = illuminateFromGBuffer(direct·shadow) + computeIBL(IBL split-sum) + precompLight(=GB2 emissive)` → fog lerp → **SceneColorHDR에 선형 그대로**(톤매핑 X)
- LightData: 별도 StructuredBuffer (t1)로 전달 (`deferredLightingLightData_`)
- PerFrameData: 단일 ConstantBuffer (b1) — CSM + invView/invProj + GBuffer SRV bindless + debugMode + fog(density/falloff/baseHeight) + camPos + **IBL(idxIrradiance/idxPrefiltered/idxBRDFLUT/prefilteredMipCount/iblIntensity)**

**GBuffer depth → backbuffer DSV 복사:**
- Lighting pass cmdList와 동일 batch에서 `copyResource` 실행
- GBuffer DSV (D32_FLOAT) 내용을 backbuffer depth buffer에 복사
- 이후 Skybox/Terrain/Billboard 등 Forward-always 패스가 올바른 깊이를 기준으로 렌더링 가능

**GBuffer / IBL 디버그 뷰:**
- 'G' 키 → `GFX::cycleGBufferDebugMode()` → `gBufferDebugMode_` (0~10) 순환
- 순서: 0 None → 1 Albedo → 2 Normal → 3 AO → 4 Roughness → 5 Metallic → 6 LightAccum(=emissive) → 7 Depth → **8 IBL diffuse → 9 IBL specular → 10 BRDF LUT**
- Lighting PSO의 `debugMode` (cbuffer b1 내 uint)로 전달, PSO permutation 불필요
- **디버그 패스스루:** resolve가 `debugMode!=0`이면 exposure/ACES/gamma를 건너뛰고 샘플값을 그대로 출력한다(디버그 분기가 이미 표시용으로 인코딩하므로 이중 톤매핑/감마 방지). 디버그 모드에서는 bloom도 스킵.

**주의사항:**
- GBuffer DSV와 backbuffer DSV는 별개 리소스 — Deferred path에서 geometry pass는 GBuffer DSV 사용, depth 복사 없이 Forward 패스를 이어 실행하면 깊이가 초기화된 상태로 모든 Forward 오브젝트가 GBuffer 위에 그려짐
- `PBRDeferredSkinnedPipeline`의 Lighting pass는 직접 담당하지 않음 — `PBRDeferredPipeline`의 Lighting pass가 정적/스킨드 GBuffer를 모두 처리
- **GGX NDF 0/0 → NaN (검은 사각형 원인, 해결됨):** `pbrLighting.hlsli::distribute()`는 roughness→0
  이고 NH→1(정반사 정렬)이면 `denom→0` → `D = a2/denom = 0/0 = NaN`(극소 roughness면 거대값→
  R16G16B16A16F Inf). 이 1픽셀이 SceneColorHDR에 박히면 bloom이 사각 영역 전체로 NaN을 번지게 하고
  ACES `saturate(NaN)=0`으로 **간헐적 검은 2D 사각형**(정반사 하이라이트 근처)이 된다. 메인 렌더링에서만
  보이고 디버그 뷰에선 안 보임(bloom/ACES를 거치는 건 메인 경로뿐). **수정:** `distribute()`에
  `roughness=max(roughness,0.045f)` + `nom/max(denom,1e-7f)`. 방어선으로 bloom `srcTap()`도 입력 sanitize.
  (전 렌더 경로의 bloom·SceneColor·deferred lighting 버퍼는 모두 per-room이라 트리플버퍼 race는 원인이 아니었음.)

#### HDR + IBL + Bloom 파이프라인

Deferred 경로의 라이팅/후처리 체계. 모든 불투명 lit 출력을 **선형 HDR**로 누적하고, 단일 resolve
지점에서 톤매핑한다. 금속은 환경을 반사하고 유전체는 환경 irradiance를 받는다.

**1) SceneColorHDR (`SharedResources::SceneColor`)**
- per-room `R16G16B16A16_FLOAT` RT(+ bindless SRV, BilinearClamp). deferred lighting·skybox(deferred 시)가
  선형 HDR을 누적, resolve가 읽어 backbuffer로 합성. `addSceneColor`/`transitionToWrite/Read`/`eraseSceneColor`.
- GBuffer와 동일 수명(triple-buffer roomIdx), resize 시 재생성.

**2) IBL 프리컴퓨트 (`SharedResources::IBL`, `iblPrecomputePipeline`)** — 로드 타임 1회(LoadFence)
- 스카이박스 큐브 상주 직후 `loadRequestedAssets`에서 `precomputeIBL()` 호출. 환경 교체 시 재호출 가능.
- 컴퓨트 셰이더 3종(D3D 큐브 basis, GL Y-flip 금지): `iblIrradiance.hlsl`(코사인 컨볼루션 → irradiance 큐브 RGBA16F 32²),
  `iblPrefilter.hlsl`(GGX importance, 256샘플 → prefiltered 큐브 RGBA16F 128² 5밉, mip=roughness),
  `iblBRDFLUT.hlsl`(split-sum, k=roughness²/2 → R16G16F 256²).
- `envIsLDR` 토글: LDR 환경맵의 역Reinhard 근사. **현재 false**(LDR 하늘의 1.0 채널 과증폭→파란 폭발 방지).
  진짜 HDR HDRI 확보 시 envIsLDR 무관하게 드롭인.
- 큐브 밉별 UAV(TEXTURE2DARRAY 6슬라이스) write → 단일 SRV(TEXTURECUBE) read.

**3) IBL 셰이딩 (`pbrLighting.hlsli::computeIBL`)** — split-sum
```
diffuse  = irradiance(N) * albedo
specular = prefiltered(R, roughness*maxMip) * (F0*brdf.x + brdf.y)
return (kD*diffuse + specular) * (1-ao) * iblIntensity   // kD=(1-kS)(1-metallic), kS=fresnelSchlickRoughness
```
- N/V는 **월드 공간**. `computeIBL`/`fresnelSchlickRoughness`는 hlsli 상단에 정의(forward illuminate*도 호출 가능).
- **Deferred**: `pbrDeferredLighting.hlsl`이 `#define IBL_ENABLED` 후 direct에 가산.
- **Forward 패리티**: `pbr.hlsl`/`pbrSkinned.hlsl`/`terrain.hlsl`도 `#define IBL_ENABLED` + cbuffer에 camPos+IBL 필드
  추가, `illuminateCSM`/terrain ambient에 **가산**(기존 globalAmbient 유지 위). inline tonemap은 forward 유지.
- **포트레이트 무회귀**: 로비 포트레이트는 `FrameData::iblIntensity=0`으로 스테이징(스튜디오 globalAmbient 유지, 환경광 미수신).
  메인 forward/deferred는 1.0.

**4) Tonemap resolve (`tonemapResolve.hlsl`, `TonemapPipeline`)** — 단일 톤매핑 지점
- fullscreen triangle: SceneColorHDR(+ bloom mip0 가산) → `color *= exposure` → **ACES Filmic(Narkowicz)** → gamma → backbuffer.
- `PerDrawcallData(b0)`: idxSceneColor, idxBloom, exposure, bloomIntensity, debugMode. (debugMode≠0 → 패스스루)
- 노브: `GFX::tonemapExposure_`(기본 1.0).

**5) Bloom (`bloom.hlsl`, `BloomPipeline`, `SharedResources::Bloom`)** — 픽셀 기반 HDR 밉체인
- per-room RGBA16F 밉체인(half-res base, 최대 6밉, `ALLOW_RENDER_TARGET`). **밉별 RTV + 밉별 단일밉 SRV**.
- 패스(단일 cmdlist): prefilter(soft-threshold + 13-tap 다운샘플; scene→mip0) → downsample 체인(mip i-1→i)
  → **additive upsample** 체인(3×3 tent, mip i+1→i). 총 2N-1 패스.
- **배리어 안무(서브리소스별 RTV↔PIXEL_SHADER_RESOURCE)**: 다운샘플은 src=SRV·dst=RT, 업샘플은 src(작은밉)=SRV·
  dst(큰밉)=RT(이전 내용 보존하며 가산). 마지막에 mip0→SRV(resolve 합성용). SceneColorHDR는 PSR 유지.
- resolve에서 `color += bloom_mip0 * bloomIntensity`(exposure/ACES 앞). 노브: `GFX::bloomThreshold_`(1.0)/`bloomIntensity_`(0.08).
- 디버그 모드(`gBufferDebugMode_≠0`)에서는 bloom 스킵.
- **입력 sanitize (NaN/Inf 방어):** `bloom.hlsl::srcTap()`이 모든 탭을 `max(c,0)`+`min(c,64000)`+
  `select(isnan(c),0,c)`로 정규화한다. SceneColorHDR에 NaN/Inf가 1픽셀이라도 박히면 `softThreshold`의
  `Inf/Inf` 등으로 NaN이 밉체인 전체로 번지고, resolve의 ACES `saturate(NaN)=0` → **검은 사각형**이 된다.
  진입 지점에서 막아 라이팅이 비정상값을 흘려도 bloom은 NaN/Inf를 산출하지 않는다(근본 수정과 별개 방어선).
  근본 원인은 GGX NDF `distribute()`의 0/0 — Deferred Shading 주의사항 참조.

**주의사항:**
- **디스크립터 풀 사이징:** bloom은 per-room×밉수(최대 6) RTV를 소비한다. `rtvPool_`/`rtvHeap_`는 64
  (백버퍼+GBuffer4+SceneColor+Portrait+Bloom6 = room당 13 × 최대 3 room ≈ 39 + 여유). per-room×N 형태의
  RT 리소스를 추가할 때 풀 용량 갱신 필수(고갈 시 `DescriptorPool::alloc`이 빈 컨테이너 front()=UB→크래시).
  bloom SRV는 `srvTexPool_`(1800)에서 충당.
- **MT/동시성:** lighting write → SceneColor transitionToRead → bloom → resolve는 모두 RenderingSlave cmdlist를
  `cmdQ_` 제출 순서로 직렬화(동일 큐 → 별도 fence 불필요). bloom/SceneColor 리소스는 per-room이라 프레임 레이스 없음.
- **resize:** SceneColor·Bloom은 eraseX→addX로 재생성(IBL 맵은 해상도 무관·정적 유지).
- 지형 청색기 등 "차가운 룩"은 IBL/fog/톤매퍼가 아니라 **조명 리그(중립 태양 + 파란 하늘 ambient)** 의
  아트디렉션 이슈다. 보정은 `dirLight_.color`를 warm하게(예: (1.0,0.95,0.86)) — Phase 2 노브로는 해결 안 됨.

#### Hi-Z Occlusion Culling (PBRDeferredSkinnedPipeline)

파일: `pbrDeferredSkinnedPipeline.hpp/cpp`

**5단계 GPU 파이프라인 (`hiZPassCompute()`):**
1. **Clear** — perGroupCnt / groupOffsets / visibleFlags 초기화
2. **Cull** — HiZ depth map으로 각 DrawEvent visibility 판정. 출력 2갈래:
   - `visibleFlags[]` (u32t per DrawEvent, 0/1) — indirect draw용(Compact가 소비)
   - `visibilityFeedback`(u3) — CPU feedback용 packed 엔트리 `(objId<<1 | visBit)`

   **Cull 입력 AABB (포즈/랙돌 추종):** 셰이더는 `world × aabbMin/aabbMax`를 스크린 투영해
   occlusion을 판정한다. 스킨드 메시는 `mesh->bounds`(rest-pose 로컬) × `e.world`(객체 루트)가
   본 변형(애니/랙돌)을 반영하지 못해, 특히 **랙돌 시 사망 시점 위치의 stale AABB**로 잘못
   컬링된다(몸/옷이 별도 메시·별도 bounds라 옷만 사라지는 증상). 따라서 `Object::worldCullBounds()`가
   `body_.worldBVH()`의 **본 부착 노드 합집합 AABB**(포즈를 따라가는 월드 공간 바운드, +15% 마진)를
   제공하고, 유효하면(`DrawEvent::hasWorldCullBounds`) cull 스테이징이 이를 **identity world**로
   투영한다(없으면 rest-pose × world로 폴백). 같은 객체의 모든 draw event가 동일 AABB를 공유해
   몸/옷이 일관되게 판정되며, frustum culling이 신뢰하는 `worldBVH` 소스와 동일하다.
3. **PrefixSum** — groupOffsets 계산
4. **Compact** — visibleFlags(SRV)를 읽어 visibleIndices 작성
5. **Command** — indirect draw args 생성 → `gBufferIndirectDraw()`에서 소비

**visibility feedback (2-slot ring buffer, objId 패킹):**
- 단일 `RWStructuredBuffer visibilityFeedback`(roomCnt=1, byteWidth = 2*slotBytes, +readback).
  byte offset으로 2슬롯 표현(슬롯 s = `s * slotBytes`). `parity = frameIdx % 2`가 슬롯 선택.
- Cull이 u3에 슬롯 offset으로 바인딩되어 `gVisibility[idx] = (objId<<1)|vis`를 dense하게 기록
  (모든 instance 한 엔트리, visible/culled 무관). 엔트리에 objId가 패킹돼 **self-describing** —
  프레임 간 DrawEvent 순서/개수와 무관해 슬롯을 자유롭게 교차 판독 가능.
- Cull 직후 `copyToReadback`으로 같은 슬롯 offset에 복사. **전용 fence 없음** —
  coherency는 전역 프레임 펜스(매 프레임 `FrameFence[N-2]` 대기)가 제공.
- 다음 프레임 `hiZPassUpdate()`가 **두 슬롯을 모두 읽어** `objectVisibility[oid]`를 OR 집계.
  same-parity 슬롯(=N-2, 이번에 덮어쓸 슬롯)은 펜스로 완성·coherent 보장, 나머지는 torn 가능하나
  **OR-only + objId bounds-check**로 최악이 보수적(과다 visible) → 안전. 렌더 누락 불가능.
- slot별 엔트리 수는 GPU readback이 아니라 **CPU가 직접 추적**(`slotEntryCount[2]` =
  그 슬롯을 쓴 프레임의 `gBufferEvents_.size()`). 시작 프레임(데이터 없음)은 전부 visible로 시작.

**room 비종속:** visibility는 한 프레임 종속 자원이 아니라 2-slot ring으로 충분하므로,
이 버퍼는 backbuffer 수(triple buffering)와 무관하게 항상 2슬롯이다(roomCnt=1).

**용도:** `objectVisibility[]` 집계를 통한 **애니메이션/물리 연산 스킵 및 디버그 전용**.
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
  - `hiZPassUpdate()`: `perInstanceDataCull/Compact` 크기 = `gBufferEvents_.size()`,
    `perInstanceDataCull[i].instanceObjId = gBufferEvents_[i].renderObjectId` 패킹
  - `hiZPassCompute()`: Cull/Compact dispatch = `gBufferEvents_.size()`, visibility 슬롯 복사 크기 동일

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
- 리소스 로드: **Chunk 스트리밍으로 전환됨** — 단일 `loadTerrainFromFiles()`/manifest·meta 파서는 제거되고
  `TerrainChunkManager`가 `chunks_index.bin` 기반으로 청크를 워커(CPU build)+메인(GPU finalize)로 스트리밍.
  상세는 `docs/terrainChunkStreaming.md`.
- VB 5슬롯: Position(0) / Normal(1) / Tangent(2) / Bitangent(3) / UV(4), IB 32-bit (513×513 정점 초과 가능)
- Tangent/Bitangent: `terrain.cpp genChunkGeometryCpu()`에서 중앙 차분 + Gram-Schmidt로 CPU 사전 계산
  (청크 build는 ThreadPool 워커 스레드에서 수행)
- Splat map: RGBA 채널 = 레이어 0~3 블렌딩 가중치, 각 레이어마다 diffuse + normal map
- Normal map 포맷: **Unity DXT5nm** — X는 Alpha 채널, Y는 Green 채널, R은 더미(1.0) → `nmSample.ag * 2 - 1`로 읽어야 함
- Normal mapping: `hasAnyNormal` 플래그(cbuffer b0)로 조건부 처리, `float3x3(tangentV, bitangentV, normalV)` 패턴 (pbr.hlsl과 동일)
- `terrain.hlsl`에서 `pbrLighting.hlsli` include 시 `#define TERRAIN_SHADER` 필수 — `illuminate()` 스킵 가드
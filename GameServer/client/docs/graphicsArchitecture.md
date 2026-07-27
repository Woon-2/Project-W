### 그래픽스 아키텍처
`GFX` - 렌더링을 총괄 책임지는 클래스

- 코어: `gfx.hpp`, `gfxUtil.hpp`, `mesh.hpp`, `shader.hpp`, `font.hpp`, `collision.hpp`
- 파이프라인: `pbrPipeline.hpp`, `pbrSkinnedPipeline.hpp`, `pbrDeferredPipeline.hpp`, `pbrDeferredSkinnedPipeline.hpp`, `billboardPipeline.hpp`, `bvPipeline.hpp`, `samplePipeline.hpp`, `skyboxPipeline.hpp`, `uiPipeline.hpp`, `terrainPipeline.hpp`, `terrainDeferredPipeline.hpp`, `sharedResources.hpp`
- 후처리/IBL: `TonemapPipeline.hpp`(ACES+exposure resolve + 3D LUT color grading), `BloomPipeline.hpp`(HDR bloom), `iblPrecomputePipeline.hpp`(IBL 맵 프리컴퓨트) — 상세는 아래 "HDR + IBL + Bloom 파이프라인"

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

#### 제출 스레드 (Render Submission Thread) — `RenderSubmitter`

`ExecuteCommandLists`(이하 ECL)/`Present`/`Signal`은 단일 `ID3D12CommandQueue`에서 드라이버가
직렬화하며, 메인 렌더 스레드의 가장 큰 CPU 비용이다. 이를 메인 스레드 임계 경로에서 빼내기 위해
**전용 제출 스레드(`client/renderSubmitter.*`, `RenderSubmitter`)** 를 둔다.

- 모든 Dispatcher / barrier·clear / present / signal / 리소스 로딩(IBL precompute 포함)은
  `cmdQ_->ExecuteCommandLists`를 직접 호출하지 않고 `submitter_->submit/present/signal`로 enqueue한다.
  `cmdQ_`는 **제출 스레드만** 만진다(생성/네이밍/스왑체인·폰트 연결 같은 비-ECL 호출 제외).
- 내부 큐는 **순서 보장 단일 소비자 FIFO**(mutex+deque+cv) → enqueue 순서 = GPU 제출 순서.
  기존 패스 간/배리어 순서가 그대로 보존된다. (ThreadPool에 ECL을 분산하지 않는 이유: 같은 큐는
  어느 스레드에서 호출해도 직렬화되어 병렬 이득이 없고 순서가 깨진다. 순서 무관 작업의 진짜 병렬화는
  별도 큐(async compute/copy)의 몫 — 후속 과제.)
- **불변식**: ① 커맨드 리스트는 `Close()` 후 enqueue. ② 리스트 객체(ComPtr) 수명은 프레임
  Fence의 `associatedCmdCtxs_`가 보장(submit은 raw 포인터만 보관). ③ `Fence::desiredValue`
  증가/대기는 메인 스레드 전용, 제출 스레드는 전달받은 값으로 Signal만. ④ `render()` 시작부의
  `waitOnFence`가 백프레셔(메인이 제출 스레드보다 최대 ~3프레임 앞섬).
- **수명**: 초기화 단계는 inline 모드(`start(cmdQ, async=false)`)로 메인 스레드에서 직접 실행,
  첫 `render()`에서 `goAsync()`로 전용 스레드 가동. 셧다운/`drainGpu()`는 `flushBlocking()` 후
  Fence 대기, 소멸 시 `stop()`으로 join(멤버 선언이 `cmdQ_` 뒤라 큐보다 먼저 파괴).
- **⚠ back buffer 인덱스는 결정론적으로**: Present가 제출 스레드에서 비동기로 일어나므로 메인
  스레드가 `swapChain_->GetCurrentBackBufferIndex()`를 질의하면 stale 값(직전 프레임 Present
  미처리)을 받아 잘못된 back buffer에 기록 → `EXECUTECOMMANDLISTS_WRONGSWAPCHAINBUFFERREFERENCE`
  → 디바이스 제거(크래시). 따라서 back buffer 인덱스는 반드시 `frameIdx_ % backBuffers_.size()`
  로 계산한다(매 프레임 1회 Present → current == presents%N == frameIdx_%N, render()의 roomIdx와 동일).
  `GFX::resize()`는 `ResizeBuffers`가 current 인덱스를 0으로 리셋하므로, `drainGpu()`로 idle 후
  `frameIdx_`를 N의 배수로 올림 정렬해 다음 프레임 인덱스를 0으로 재동기화한다.

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
3. gBufferPass(PBRDeferred, direct) → **gBufferIndirectPass(PBRDeferred)** → **gBufferIndirectPass(PBRDeferredSkinned)** → **gBufferPass(Terrain)** — MRT 4개(GB0~GB3) + GBuffer DSV에 기록. **GB2.rgb = emissive 전용**(ambient는 lighting 패스 IBL로 이동). 스킨드 GBuffer PS는 추가로 **흡수 물결(per-instance ripple)** emissive를 GB2에 가산(로컬 플레이어만; 아래 "몬스터 사망 에너지 오브 연출" 참조)
   - PBRDeferredSkinned/PBRDeferred 모두 Hi-Z 5단계 compute(Clear→Cull→PrefixSum→Compact→Command) 후 indirect draw 실행. compute 셰이더 5종 + `cmdSig_`는 공유
   - **PBRDeferred Hi-Z**(정적 prop, 2026-06-15): `occludeeCandidate` DrawEvent(=VFC 통과 BVH prop)만 indirect 대상. visibility feedback ring/CPU readback **없음**(정적이라 anim/물리 스킵 불필요; cull u3 출력은 scratch로 폐기). 비-occludee는 `gBufferPass` direct
   - PBRDeferredSkinned Hi-Z: Cull→visibleFlags + visibility feedback 2-slot ring → CPU readback(1-frame delay, anim/물리 스킵용)
4. GBuffer 상태 전환: RTV→SRV, GBuffer DSV→SRV (`transitionToRead`)
5. Lighting Pass — fullscreen triangle `DrawInstanced(3,1,0,0)`, GBuffer SRV 읽기, **SceneColorHDR(R16G16B16A16_FLOAT)에 선형 HDR 출력**. `color = directLight + computeIBL + emissive`, 이후 fog 적용. **톤매핑은 여기서 안 함**(resolve 담당)
6. **GBuffer depth → backbuffer DSV 복사** (`copyResource`) — 이후 SceneColorHDR 합성/Forward 오버레이가 올바른 깊이 기준으로 렌더링하도록
6a. **Skybox → SceneColorHDR 합성**(Deferred, `gBufferDebugMode_==0`) — SceneColorHDR가 **아직 RENDER_TARGET**일 때 backbuffer scene depth(reversed-Z far)로 depth-test해 **배경 픽셀만** raw 스카이박스로 채운다. 이후 가산 글로우/bloom/heat 워프가 하늘에도 적용되고 resolve까지 살아남는다(resolve가 배경 픽셀을 패스스루로 내보내 하늘 룩 보존). **Forward(로비) 경로는 종전대로 backbuffer에 직접**(아래 10) — `skyboxRtv`를 renderPath로 분기. **PSO RTV 포맷이 타깃과 일치해야 하므로 SceneColorHDR(R16G16B16A16F) 타깃엔 `SkyboxShaderHDR`, backbuffer(R8G8B8A8) 타깃엔 `SkyboxShader`를 선택**(한 빌더 `createSkyboxShaderImpl`에서 RTV 포맷만 다르게)
6b. **Energy orb 패스(EnergyOrbPipeline)** — `gBufferDebugMode_==0`일 때만. SceneColorHDR가 **아직 RENDER_TARGET 상태**일 때, 복사된 backbuffer scene depth(reversed-Z)로 depth-test하며 **가산(additive) HDR**로 렌더 → bloom 이전이라 발광/bloom이 산다. 몬스터 사망 시 서브메시별 에너지 오브(정점→구체 모핑, GS quad). 단일 스레드(`updateGPUDataSingleThreaded`/`drawSingleThreaded`)
6c. **Heat-haze 패스(HeatDistortionPipeline)** — `gBufferDebugMode_==0`일 때만(보스 위압 연출). EnergyOrb와 동일 슬롯(SceneColorHDR=RENDER_TARGET, bloom 이전)에서 **가산 HDR**로 보스별 틴트 글로우를 그려 bloom이 발광시킨다. depth는 GB4(linear view-Z)로 게이팅. 활성 heat source가 없으면 self-skip. 굴절 워프는 별도 패스가 아니라 resolve(아래 9)에 흡수. 아래 "보스 Heat Distortion" 참조
6d. **상호작용 실루엣 패스(OutlinePipeline)** — `gBufferDebugMode_==0`일 때만. 6b/6c와 같은 슬롯(SceneColorHDR=RENDER_TARGET, bloom 이전)에서 inverted hull(`CullMode=FRONT`)을 **가산 HDR**로 그려 조준된 월드 아이템의 테두리를 bloom으로 발광시킨다. depth test `GREATER`(reversed-Z) / depth write off. 드로우콜이 프레임당 1개 수준이라 단일 스레드이며 Hi-Z 대상이 아니다. 아래 "상호작용 강조 실루엣" 참조
7. SceneColorHDR 상태 전환: RTV→SRV (`SceneColor::transitionToRead`)
8. **Bloom** (`gBufferDebugMode_==0`일 때만) — SceneColorHDR → bloom 밉체인(prefilter→downsample→additive upsample), mip0 → SRV
9. **Tonemap resolve** — **배경(GB4==0, 하늘) 픽셀은 패스스루**(노출/ACES/감마/LUT 미적용, raw 하늘+bloom)로 스카이박스 룩 보존; **지오메트리 픽셀**은 SceneColorHDR(+ bloom mip0 가산) → exposure → ACES Filmic → gamma → **3D LUT** → backbuffer. **보스 heat distortion 굴절 워프**도 여기서: 샘플 UV를 보스 영역에서 오프셋(`heatField.hlsli`, GB4 깊이 게이팅; 패스스루/톤맵 분기는 **샘플된 픽셀**의 GB4로 결정해 인코딩 일치)
10. Forward-always 오버레이(backbuffer, resolve 이후): BV → Billboard → 파티클류. (**Deferred 경로의 skybox는 6a로 이동**; Forward 경로에서만 skybox를 backbuffer에 직접)
11. mainPass(UI)

Billboard / UI / 파티클은 renderPath에 관계없이 backbuffer에 직접 그린다. **Skybox는 Deferred 경로에서 SceneColorHDR(6a)에 합성되어 heat distortion/bloom이 하늘에도 적용**되고, Forward path(로비)에서만 backbuffer에 직접 raw로 그려진다(HDR/Bloom/resolve 미경유, 셰이더 inline tonemap).
Terrain은 Deferred path에서 gBufferPass로 GBuffer에 기록, Forward path에서만 mainPass로 실행한다.

#### Reversed-Z 깊이 버퍼

메인 카메라(퍼스펙티브)가 쓰는 depth buffer는 reversed-Z(near→NDC z=1.0, far→NDC z=0.0)다.

**Why:** `DXGI_FORMAT_D32_FLOAT`라도 퍼스펙티브의 쌍곡선 z 매핑 때문에 far 영역 depth가 1.0
근처(float가 표현값이 희소한 구간)에 뭉쳐 정밀도가 이중으로 손실된다. near=1.0/far=0.0으로
뒤집어 far를 float가 밀집한 0.0 근처로 보내 개선한다.

**적용 범위:**
- **O** — 메인 scene depth(forward/deferred 백버퍼), GBuffer depth, Hi-Z occluder depth+mip
  chain, Portrait depth. 전부 `Camera::setPerspective()` → `mu::perspReversedZ()`
  (`mathUtil.hpp`, client/common 사본 동일 유지)를 거치는 퍼스펙티브 카메라가 공급한다.
- **X** — CSM 그림자맵(직교투영)은 그대로 표준-Z. ortho는 z 매핑이 선형이라 reversed-Z의
  핵심 이득(쌍곡선 압축 보정)이 적용되지 않기 때문. 그림자 비교 샘플러(`LESS_EQUAL`),
  `mu::ortho()`, cascade split 계산은 변경하지 않았다.

**투영행렬:** depth(z) = A + B/z 형태에서 depth(nearZ)=1, depth(farZ)=0이 되도록 풀면
`A = nearZ/(nearZ-farZ)`, `B = nearZ*farZ/(farZ-nearZ)`. `XMMatrixPerspectiveFovLH`는
표준 매핑만 지원하므로 z/w 항을 직접 구성(`mu::perspReversedZ`).

**Depth clear / DepthFunc:** far=0.0이므로 클리어 값 1.0f→0.0f(메인/GBuffer/Hi-Z/Portrait).
`shader.cpp`의 그림자맵 PSO 7개(`createShadowMap*`, `createTerrainShadowMap*`)를 제외한
나머지 전부 `DepthFunc`를 `LESS`→`GREATER` / `LESS_EQUAL`→`GREATER_EQUAL`로 반전.
`HiZMap::clearDepth`(distance culling용 epsilon)도 `0.9999f`→`0.0001f`로 대칭 변환.

**Hi-Z 비교 방향 반전:** `hiZMap.hlsl` 다운샘플 `max`→`min`(2x2 셀 중 가장 먼 occluder
depth 보존), `hiZCull.hlsl`의 `ProjectAABBToScreen`(`min`→`max`, AABB 8코너 중 카메라에
가장 가까운 코너)·`OcclusionTest`(`max`→`min` 집계, `depth <= maxDepth` → `depth >= minDepth`).

**기타 반영 지점:**
- soft particle depth linearization(`blendCGMesh.hlsl`/`smokeBlendCG.hlsl`):
  `linearZ = (nearZ*farZ)/(nearZ + depth01*(farZ-nearZ))`
- 스카이박스 far-plane trick(`skybox.hlsl`): `clipPos.xyww`(NDC z=1=far, 표준-Z) →
  `float4(clipPos.xy, 0.f, clipPos.w)`(NDC z=0=far, reversed-Z)
- `pbrDeferredLighting.hlsl`의 위치 재구성은 raw NDC depth가 아니라 GB4의 선형 view-Z
  (`posV.z`)를 쓰므로 무관(`rawDepth` 디버그 뷰만 반전되어 보임 — 정상)
- 뷰포트 `MinDepth`/`MaxDepth`(0~1)는 변경 없음 — NDC→뷰포트 depth 범위 매핑이라
  reversed-Z(투영행렬이 NDC z를 만드는 방식)와는 별개

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
  첫 cascade의 near 경계(`prevFarV` 초기값)는 `camProj`의 A/B 항(`NDC_z = A + B/viewZ`)에서 역산하는데, 이 공식은
  카메라 투영의 Z 매핑 방향에 종속적이다 — **Reversed-Z 도입(2026-06) 시 한 번 깨졌던 지점**: 표준-Z는 `NDC_z=0`이
  near라 `viewZ=-B/A`였지만, reversed-Z는 `NDC_z=1`이 near이므로 `viewZ=B/(1-A)`로 풀어야 한다. 누락 시 첫 cascade의
  near가 farZ로 잘못 계산되어 cascade 0의 바운딩 구가 거대해지고(거의 전체 depth range를 덮음) 가까운 오브젝트가
  저해상도 그림자를 받는 증상이 나타난다(`light.cpp::updateCSMCascades`). 카메라 투영 행렬의 Z 매핑을 바꿀 때마다
  이 추출 공식도 같이 점검해야 한다.
- **texel center snap 유지:** sphere center XY를 `worldUnitsPerTexel(=2·radius/res)` 격자에 스냅(radius가
  rotation-invariant라 프레임 간 안정). per-cascade **radius 양자화는 시도 후 제거** — 이산 radius 스텝이 이동 중
  오히려 떨림을 유발했고, 카메라-상대 공간만으로 shimmer가 해소됨(2026-06 사용자 검증).
- 보조: deferred lighting은 GBuffer `gb4`(linear view-Z, R32F)로 posV를 정확 복원(NDC 깊이 양자화 제거) — 단독으론 소폭 개선.
- **주의:** cascade `lightVP`가 카메라-상대 공간이므로, 절대 월드 BVH로 cull/test하는 코드는 `cascadeCameraPos()`로
  rebase 필요 — 이 rebase는 이제 `Light::shadowVisible`(아래) 내부에서 일괄 처리된다.
- **ortho z-pad = `2·radius` (유지):** `mu::ortho(...,minZ - 2·radius, maxZ)`. near 평면을 frustum slice보다 충분히
  뒤로 빼서, 빛과 slice 사이에 있는 캐스터(slice 바로 밖 나무/풀 등)도 깊이를 기록한다. `radius`로 줄이면 일부
  foliage가 near 평면 뒤로 사라져, 2x 패딩을 의도적으로 유지(2026-06-17 사용자 검증). chunk의 `shadowVisible(expand=3)`은
  z축소와 무관하게 보수적 컬링으로 유지.

**Shadow(light) frustum culling — 단일 진입점 (2026-06-17):**
그림자 캐스터 컬링은 **메인 카메라가 아니라 광원 cascade 기준**으로 수행해야 한다(메인 frustum으로 컬링하면
화면 밖 캐스터의 그림자가 사라지는 popping 발생). 모든 light-frustum 컬링은 `Light::shadowVisible(...)` 하나로 통일:
- `updateCSMCascades`가 cascade마다 `cascadeFrusta_[i] = extractFrustum(view·proj)`를 캐시(ortho 투영이라 6평면=OBB).
- `Light::shadowVisible(AABB/OBB/variant, expand=1)` — bounds를 `cascadeCameraPos_`로 rebase + `expand`로 half-extent
  확장 후 `intersects(Frustum, ·)`(`frustumCull.hpp`, AABB·OBB 오버로드)로 테스트, 어느 cascade에라도 보이면 visible.
  cascade 0개면 항상 true(미컬, 안전 폴백).
- 호출 위치 3곳: ① 엔티티 `game.cpp::cullObjectsForShadow`(인라인 SAT ~60줄 → 한 줄로 축약) ② 지형 chunk
  `TerrainChunkManager::submitDrawEvents`(`expand=3`, 대형 캐스터 보존) ③ scatter BVH prop `submitScatterDrawEvents`(`expand=1`).
- **메인 vs 그림자 가시성 분리:** scatter prop은 메인카메라 `frustum_` VFC와 `shadowVisible`를 독립 평가해 DrawEvent의
  `viewFrustumCulled`/`shadowCulled`를 따로 설정 → 화면 밖이지만 그림자 frustum 안인 나무는 shadow 패스에만 제출(gbuffer/Hi-Z 제외).
  지형 chunk DrawEvent에도 `shadowCulled` 필드 추가, terrain `shadowDraw` 루프에서 skip(gbuffer는 무영향).
- Hi-Z occlusion은 메인 패스 개념 → occludee/occluder 선정은 `mainVisible`일 때만(그림자에는 Hi-Z 미적용).

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
| GB2 | R11G11B10_FLOAT | **Emissive.rgb (HDR)** — intensity>1 보존(Unity 발광·블룸) |
| GB3 | R8G8_UNORM | Metallic.r + Roughness.g |
| GB4 | R32_FLOAT | Linear view-space Z (posV.z) — deferred 복원이 NDC 깊이 양자화 대신 사용 |
| Depth | R32_TYPELESS (DSV=D32_FLOAT, SRV=R32_FLOAT) | Scene depth |

> **주의:** GB2.rgb는 emissive 전용이다. `pbrDeferred.hlsl`·`pbrDeferredSkinned.hlsl`·`terrainDeferred.hlsl` 모두 `lightAccum = emissive`(지형/스킨드 모두)로 기록해야 한다. 과거 스킨드 셰이더만 `globalAmbient*albedo`를 굽던 버그가 있었고(이중 ambient: GB2 상수 ambient + lighting 패스 IBL), 셋 다 emissive-only로 통일했다.
>
> **GB2 HDR 전환(2026-06-22):** GB2는 과거 `R8G8B8A8_UNORM`이라 emissive intensity>1이 [0,1]로 클램프되어 Unity 같은 발광/블룸이 안 났다. `R11G11B10_FLOAT`로 바꿔 HDR을 보존한다. GB2의 alpha에 있던 **roughness는 GB3로 이전**(GB3를 `R8G8_UNORM` 2채널화: `.r=metallic`, `.g=roughness`). 셰이더 시맨틱도 Unity와 맞춰 `emission = emissionColor × emissionMap`(대입→곱) + `pow(2.2)` 선형화로 통일. GBuffer-writing 3종 + reading(`pbrDeferredLighting.hlsl`) + PSO RTV 5종(`shader.cpp`) + RT 생성(`sharedResources.cpp`)을 일괄 정합.
>
> **흡수 물결(absorption ripple):** `pbrDeferredSkinned.hlsl`의 PS는 `lightAccum`에 per-instance ripple emissive를 가산한다. GB2가 HDR(`R11G11B10_FLOAT`)이 되어 HDR 물결을 직접 블룸으로 표현할 수 있다(트리거 측 정규화·하향 보정은 연출 의도에 따라 선택).

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
- **포트레이트 장착 무기(non-skinned) 듀얼 디스패처**: 로비 포트레이트 RT(`gfx.hpp`의 슬롯별 `cameraDataLobbyPortrait_`/
  `lightDataLobbyPortrait_`/`frameDataLobbyPortrait_`)는 원래 캐릭터 본체용 `PBRSkinnedPipeline::Dispatcher` 1개만
  썼는데, 장착 무기는 항상 non-skinned 정적 메시라 같은 슬롯에 `PBRPipeline::Dispatcher`를 병렬로 추가했다(메인
  씬의 PBR/PBRSkinned 듀얼 디스패처 패턴과 동일, `gfx.cpp` 포트레이트 렌더 루프). 카메라/라이트/프레임 데이터는
  `PBRPipeline`과 `PBRSkinnedPipeline`의 대응 구조체가 필드 구성이 동일해 디스패치 시점에 인라인 변환만 하고
  별도 GFX 멤버는 늘리지 않았다. 제출 경로: `Object::renderPortrait()`가 본체(스킨드) 제출 후 `equipments_`를
  순회해 `Object::renderPortraitEquipment()` → `GFX::addLobbyPortraitDrawEventStatic()`(`drawEventsLobbyPortraitStatic_`,
  `resourcesLobbyPortraitStatic_`) 경로로 제출한다. shadowPass는 미수행(mainPass만).

**4) Tonemap resolve (`tonemapResolve.hlsl`, `TonemapPipeline`)** — 단일 톤매핑 지점
- fullscreen triangle: SceneColorHDR(+ bloom mip0 가산) → `color *= exposure` → **ACES Filmic(Narkowicz)** → gamma → **3D LUT color grading** → backbuffer.
- `PerDrawcallData(b0)`: idxSceneColor, idxBloom, exposure, bloomIntensity, debugMode, idxColorGradingLUT, **HeatParams heat + idxGB4**(heat distortion 굴절 워프용). (debugMode≠0 → 패스스루이며 워프도 스킵, idxColorGradingLUT.x<0 → grading 미적용)
- 노브: `GFX::tonemapExposure_`(기본 1.0).

**5) Color grading LUT (`SharedResources::ColorGrading`, `bindless.hlsli::sampleBindless3D`)** — gamma 보정 직후 적용되는 고정 단일 3D LUT
- 로드 타임 1회(`GFX::initSharedResources` → `addColorGradingLUT`), `resources/LUT/warm-natural_6.C0008.cube`(33³, DaVinci Resolve 표준 `.cube` 텍스트 포맷)를 파싱해
  `R8G8B8A8_UNORM` `Texture3D`로 업로드. 씬 전역·정적이라 런타임 전환 없음(LUT를 바꾸려면 파일 교체 + 경로 갱신 후 재빌드).
- **bindless Texture3D 풀**: 기존 Tex2D/Tex2DArray/TexCube 3종에 4번째로 추가(`bindless.hlsli`의 `gTex3Ds[] : register(t10, space4)`,
  `DefaultRootSig`의 `Texture3DPool` 파라미터, `GFX::srvTex3DPool_` — SRVHeap `[2100,2116)`).
- **half-texel 보정**: LUT 텍셀 i는 값 `i/(N-1)`을 나타내지만, 하드웨어 샘플링은 UV=v를 텍셀 연속좌표 `v*N-0.5`로 매핑한다.
  보정 없이 샘플링하면 그리드 인덱스가 어긋나 그레이딩 커브가 과장되고 identity LUT조차 완전한 passthrough가 되지 않는다.
  `sampleBindless3D`에서 `uvw = color * (N-1)/N + 0.5/N`로 보정하며, N은 `BindlessIndex.idxInArray`(Tex3D는 array slice가 없어 비는 슬롯)에 실어 전달한다.

**6) Bloom (`bloom.hlsl`, `BloomPipeline`, `SharedResources::Bloom`)** — 픽셀 기반 HDR 밉체인
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
  bloom SRV는 `srvTexPool_`(1800)에서 충당. color grading LUT SRV는 별도 `srvTex3DPool_`(16, SRVHeap `[2100,2116)`)에서 충당.
- **MT/동시성:** lighting write → SceneColor transitionToRead → bloom → resolve는 모두 RenderingSlave cmdlist를
  `cmdQ_` 제출 순서로 직렬화(동일 큐 → 별도 fence 불필요). bloom/SceneColor 리소스는 per-room이라 프레임 레이스 없음.
- **resize:** SceneColor·Bloom은 eraseX→addX로 재생성(IBL 맵은 해상도 무관·정적 유지).
- 지형 청색기 등 "차가운 룩"은 IBL/fog/톤매퍼가 아니라 **조명 리그(중립 태양 + 파란 하늘 ambient)** 의
  아트디렉션 이슈다. 보정은 `dirLight_.color`를 warm하게(예: (1.0,0.95,0.86)) — Phase 2 노브로는 해결 안 됨.

#### 몬스터 사망 에너지 오브 연출 (EnergyOrbPipeline + 흡수 물결)

몬스터 처치 보상 연출. 시체가 서브메시별 **자체 발광 에너지 오브**로 변해 로컬 플레이어에게
흡수되고, 흡수 타이밍에 스킬 charge HUD가 채워지며 플레이어 몸에 색 물결이 퍼진다. 게임 레벨
라이프사이클(시체 이관/풀링/charge 매칭)은 `gameArchitecture.md`의 동명 절을 참조; 여기서는 **렌더링**만 다룬다.

**파일:** `energyOrbPipeline.hpp/cpp` + `energyOrb.hlsl`(오브 렌더), `pbrDeferredSkinned.hlsl`(흡수 물결).

**오브 렌더(EnergyOrbPipeline):** MeshParticlePipeline 복제 + GS quad.
- VS: 죽은 서브메시 정점을 **사망 포즈 본 팔레트(boneData t2, 파이프라인 전용 StructuredBuffer)** 로
  스키닝 → `hash(SV_VertexID)`로 구한 단위 구체 내부 점을 목표로 `morphT`(0→1) 보간. 즉 정점이
  서브메시 표면에서 한 구체로 모여든다. 구체 중심은 서브메시 첫 정점의 LBS 스키닝 결과(서브메시마다
  독립된 조각).
- GS: point → 카메라향 quad(billboard `GSMain` 패턴, POINTLIST 토폴로지).
- PS: `lerp(서브메시 albedo, 랜덤 HDR 색, morphT)` × 원형 radial falloff → **SceneColorHDR에 가산**.
  즉 색도 시체색→발광색으로 모핑된다.
- PSO: additive blend, CullNone, reversed-Z(`DepthFunc=GREATER_EQUAL`, depth write off). **렌더 위치는
  deferred lighting 직후 ~ SceneColorHDR→SRV 전환/bloom 직전**(render 순서 6b) — 그래야 bloom이 걸린다.
- 과대 발광 억제: 가까워질수록(추적 후반) 오브 월드 크기를 축소하는 **응축 스케일**(EnergyOrbSystem
  `renderScale`)로 원근 팽창 + 가산 코어 bloom 블롭을 막는다. HDR 강도/포인트 크기도 보수적으로.

**흡수 물결(pbrDeferredSkinned.hlsl):** 오브가 흡수되면 충돌점에서 색 물결이 몸 표면으로 퍼진다.
- 데이터 위치 = **per-instance**(`PerInstanceData`의 `ripplePosAge[4]`/`rippleColorIntensity[4]`/
  `rippleCount`). per-drawcall(Material CB)이 아닌 이유: **같은 모델의 여러 플레이어가 한 드로우콜의
  인스턴스로 묶이므로** per-drawcall로는 "내 플레이어만"을 구분할 수 없다. 비-플레이어/원격 플레이어는
  `rippleCount=0` → 셰이더 비용 0.
- VS가 `instIdx`(nointerpolation)를 PS로 전달 → PS가 `gInstances[instIdx]`의 ripple을 읽어 **가우시안
  확장 링**(`band = exp(-d*d)`)을 GB2 emissive에 가산. `exp(-d*d)`로 제곱을 직접 계산하는 이유는
  `pow(음수, 2)`가 `exp(2·log(neg))=NaN`이 되어 bloom 검은 사각형을 유발하기 때문(아래 GGX NaN 절과 동일 함정).
- 앵커는 월드 고정점이 아니라 **본체 위치 기준 오프셋**으로 저장(Object `BodyRipple`)해 매 프레임 live
  pos에 재앵커 → 플레이어가 이동/달려도 링이 몸을 따라간다. 수명 `kBodyRippleLife`(1.0s) == HLSL `RIPPLE_LIFE`.
- 트리거 측에서 오브 HDR 색을 정규화(peak=1)+흰색 혼합(탈채도)+강도 하향(0.5)해 산만하지 않은
  부드러운 워시로 보정한다. (2026-06-22 GB2를 `R11G11B10_FLOAT` HDR로 전환한 뒤로는 클램프 제약이
  사라져 HDR 물결을 직접 표현할 수 있다 — 위 보정은 연출 선택사항이 됨.)

**커맨드 리스트 풀:** EnergyOrb 디스패처가 RenderingSlave cmdlist를 추가 소비하므로 `cmdListPool_`
RenderingSlave 용량을 64→**96**으로 키웠다(부족 시 SwordSlash/UI 디스패처가 alloc 실패해 해당 패스가
통째로 누락되는 버그가 있었음).

**드로우콜 용량 상한:** 오브 1개 = 서브메시 1개 = 드로우콜 1개이고, per-instance/per-drawcall 버퍼는
고정 크기다(`EnergyOrbPipeline::kMaxOrbDrawcalls`=512, `gfx.cpp`의 버퍼 sizing과 공유). 동시 다수
사망으로 오브가 이 수를 넘으면 `perDrawcallData.cbuffers[idx]` 가 vector 범위를 벗어나 **액세스 위반**이
났다. 해결: Dispatcher 생성자에서 초과분을 truncate(로그) + draw 루프에 방어 가드. 초과 오브는 그 프레임
드롭(graceful degrade).

#### 상호작용 강조 실루엣 (OutlinePipeline, inverted hull)

조준된 월드 아이템(드롭 보석)의 테두리를 빛나게 하는 패스.
파일: `outline.hlsl`, `outlinePipeline.{hpp,cpp}`, `shader.hpp`의 `OutlineShader`,
`shader.cpp`의 `createOutlineShader`.

**inverted hull.** 메시를 한 번 더 그리되 `CullMode = FRONT`로 앞면을 버려서, 확장된
껍질의 뒷면만 남아 원래 실루엣 둘레에 테두리가 생긴다. 별도 실루엣 추출 패스나 스텐실이
필요 없다(코드베이스에 스텐실 실루엣 경로는 없다).

**확장은 오브젝트 공간이 아니라 클립 공간에서 한다.** 오브젝트 공간으로 밀면 테두리
두께가 거리에 따라 변한다. VS에서 법선을 클립 공간에 투영해 그 화면 방향으로
`thicknessPx * 2 * invScreenSize * clip.w`만큼 민다 — `clip.w`를 곱해 원근 나눗셈을
상쇄하므로 두께가 거리와 무관한 정확한 픽셀 수가 된다.

**깊이 상태.** `DepthFunc = GREATER`(메인 카메라는 **reversed-Z**, near=1/far=0),
`DepthWriteMask = ZERO`. 깊이를 쓰지 않으므로 실루엣이 다른 것을 가리지 않는다.

**렌더 슬롯.** deferred lighting 이후, **bloom 이전**에 `SceneColorHDR`
(`R16G16B16A16_FLOAT`)로 **가산** 합성한다(`gfx.cpp`의 heat distortion 직후).
따라서 `DrawEvent::color`를 HDR 범위(예: 2.6, 2.1, 0.8)로 주면 **bloom이 알아서 테두리를
발광으로 만든다** — 별도 글로우 패스가 필요 없다.

**단일 스레드 / Hi-Z 제외.** 프레임당 드로우콜이 조준 대상 1개 수준이라 멀티스레드
디스패치와 인스턴싱 버퍼가 없고(`TwoSidesPipeline`을 축약한 형태), Hi-Z occlusion
컬링 대상도 아니다(별도 `renderObjectId` 불필요). 리소스는 16 드로우콜분만 잡는다.

루트 파라미터는 규약대로 `PerFrameData=b1`, `PerDrawcallData=b0`.
정점 입력은 POSITION(slot0) + NORMAL(slot1)뿐이며 VB view는
`vbViewsByPipeline["OutlinePipeline"]`에 지연 캐싱한다.

사용처와 게임플레이 계약은 `RoomServer/docs/itemDropSystem.md` §9.

#### 보스 Heat Distortion (위압 연출, HeatDistortionPipeline + tonemap warp)

중간보스(Grandbaum/Isys)·최종보스(FinalBoss) 주변 공기가 일렁이며 왜곡(굴절)되고, 왜곡 영역에 보스별
틴트가 입혀져 bloom으로 발광하는 화면 공간 효과. 파일: `heatField.hlsli`(공유 평가 헬퍼), `heatHaze.hlsl`
+ `HeatDistortionPipeline`(가산 글로우 패스), `tonemapResolve.hlsl`(굴절 워프 흡수), `shader.hpp`
`HeatDistortionShader`(`HeatSource`/`HeatParams`/`PerDrawcallData`).

- **두 GPU 기여, 스크래치 RT 0개.** 두 경로가 `heatField.hlsli::evalHeatField`로 동일 heat-field를 평가해
  글로우와 워프가 일치한다.
  - **(A) 틴트 글로우** — `heatHaze.hlsl` 풀스크린 가산 패스가 render 순서 **6c**(SceneColorHDR=RENDER_TARGET,
    bloom 이전)에서 `tint·radialFalloff·shimmer`를 SceneColorHDR에 가산 → bloom이 발광시킴. 출력은
    bloom NaN 가드와 동일하게 finite/clamp.
  - **(B) 굴절 워프** — 별도 패스/RT 없이 **tonemap resolve**(순서 9)의 `sampleBindless(idxSceneColor, uv)`
    UV를 보스 영역에서 오프셋. heat source 0개면 워프=0 → 출력 무변화(비용 게이팅).
- **깊이 인지(전경 차폐):** 두 패스 모두 GB4(linear view-Z, R32_FLOAT)를 샘플해 `pixelZ >= bossZ - margin`
  또는 배경(GB4==0, 지오메트리 미기록=하늘/공기)인 픽셀만 효과 적용. 보스보다 카메라에 가까운 전경 prop은
  왜곡/글로우에서 제외. (view-space Z는 양수 전방·LH; reversed-Z는 메인 depth 버퍼에만 적용되고 GB4는 선형)
- **하늘(skybox)에도 적용:** 종전엔 skybox가 tonemap **이후** backbuffer에 raw로 그려져 하늘 영역의 heat
  효과를 덮어썼다(버그). 수정: Deferred 경로에서 skybox를 **SceneColorHDR에 합성**(render 순서 6a, bloom/heat
  이전, 배경 픽셀만 depth-test)하고, **tonemap이 배경 픽셀을 패스스루**(노출/ACES/감마/LUT 미적용)로 내보내 하늘
  룩을 보존하면서 워프·글로우·bloom이 하늘에도 적용되게 했다. forward 오버레이의 deferred skybox 그리기는 제거
  (Forward/로비 경로만 backbuffer 직접). 부수효과: 하늘이 이제 bloom 입력에 포함돼 매우 밝은 하늘은 약하게 bloom될 수 있음(threshold≈1.0이라 미미). 디버그 뷰(`gBufferDebugMode_≠0`)에선 하늘 합성을 스킵(배경 검정).
- **노이즈:** 절차적 value-noise fbm(텍스처 asset 없음). 워프 flow는 raw 노이즈가 아니라 **노이즈 기울기(central
  difference)** 를 정규화한 gradient/curl 형 흐름장. 상승 기류 도메인(시간에 따라 화면 -Y로 스크롤).
- **game→gfx 데이터(단방향):** `GFX::addHeatSource(HeatSource)` + `setHeatGlobals(time, warpStrength, glowStrength)`
  를 매 프레임 render() 전에 호출. gfx는 최대 `kMaxSources`(=4)개를 `HeatParams`로 모아 haze CB와 tonemap CB
  양쪽에 업로드하고, 소비 후 `heatSources_`를 비운다. `HeatSource`={centerRadius(UV), zMarginIntensity(viewZ/
  margin/intensity/shimmerSpeed), tint(rgb + warpAmp)}.
- **Online 연결:** `Online::Game::submitBossHeatSources()`(renderInGame, `camera_.updateGFX` 직후)가
  `bossNpcIds_`∩`idMonsterMap_` 생존 보스마다 `worldToScreen` 류 clip 계산으로 centerUV, `proj._11/_22`
  해석적 투영으로 radiusUV(세로 타원 aspectY), 보스 pivot view-Z를 구해 `HeatSource`를 push. 보스별 틴트/강도/
  반경은 보스별 `BossHeatState`(createGrandbaum/createIsys/createBoss에서 distinct 틴트·강도·반경 등록 — 값은
  `onlineGame.cpp`에서 튜닝). 스폰 페이드 인(~0.8s)·사망 페이드 아웃(~1.2s, idMonsterMap_에서 사라진 뒤
  마지막 위치에서 감쇠)을 intensity envelope로 적용하고, 다 사라지면 `bossHeatProfiles_`에서 erase.
- **Standalone 디버그:** `StandAlone::Game`에서 **F9**로 `goblin_`에 디버그 heat source 토글(서버 없이 튜닝),
  **J/K**=warp 강도, **-/=**=glow 강도 조절. (테스트 키는 시뮬레이션 속도 키와 겹치지 않게 선택)
- **디버그 뷰:** `gBufferDebugMode_≠0`이면 haze 패스 스킵 + resolve 워프 스킵(기존 bloom/tonemap 게이팅과 동일).

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
3. `feedbackCullResultToAnim()`(이전 이름: `applyHiZCulling()`) 내에 `applyToEntity(obj)` 추가
4. Hi-Z OFF(`isHiZCullEnabled() == false`) 상태에서도 `setHiZCulled(false)` + `animBlender->setCulled(isFrustumCulled())` 복원 필요

**최초 1회 애니메이션 갱신 보장:**
서버에서 막 생성된 오브젝트는 Hi-Z readback이 아직 해당 renderObjectId를 한 번도
visible로 기록하지 못해 첫 프레임부터 invisible(culled) 판정을 받을 수 있다.
이 상태로 방치되면 AnimBlender가 한 번도 갱신되지 못한다(T-pose 등).
`AnimBlender::hasEverUpdated()`(최초 `onCalcLocal` 호출 시 true로 전환)가 false인 동안은
`feedbackCullResultToAnim()`에서 컬링 판정과 무관하게 `setCulled(false)`를 강제해
최소 1회는 애니메이션 갱신이 이루어지도록 한다.

**Baked 스키닝 stale clipId 가드 (생성 직후 stretch 방지):**
스킨드 deferred 경로(`pbrDeferredSkinned.hlsl`)는 `Mode::Baked`일 때 본 행렬을 업로드하지 않고
GPU가 `bakedClipId`로 bindless 텍스처를 직접 샘플한다(`loadBakedMatrix(clipIdx, ...)`).
이때 `clipId`는 **전역 bindless descriptor 인덱스**다(`AssetManager::setupBakedAnimationIds()`에서
`clip->id = bakedSamples.idxSrv.idxResource`).

문제 흐름:
1. `AnimSystem::updatePriorities()`는 time-slice 없이 전체 blender를 돌며 거리 기반으로 `mode_`를
   결정 → 먼 오브젝트는 즉시 `Mode::Baked`로 전환.
2. 그러나 `finalBakedClipId_`/`finalBakedClipFrame_`은 `onCalcLocal()`의 **Baked 분기에서만** 세팅.
3. `AnimSystem::update(0.01s)`는 time-slice 내 priority 순으로 일부 blender만 처리 → 첫 프레임엔
   다수가 미처리 → `finalBakedClipId_`가 기본값 **0**.
4. `clipId=0`은 애니메이션이 아닌 descriptor 0번 텍스처를 가리키고, 이를 4×4 본 행렬로 읽으면
   garbage 스키닝 → 캐릭터가 길게 늘어나는 **stretch**(특히 온라인 InGameScene 진입 직후).
   몬스터 종류가 많을수록 ① time-slice 누락 blender 증가 ② Baked 전환 객체 증가로 더 자주 발생.

**가드(`Object::render`):** `bakedReady = (mode==Baked && hasEverUpdated() && finalBakedClipId() > 0)`
가 false면 `bakedClipId/Frame = -1`을 넘겨 `boneXforms`(미갱신 시 identity = T-pose) 경로로 폴백한다.
유효한 baked clip이 준비되면 자동으로 baked 경로로 복귀. (비-deferred `PBRSkinnedPipeline`은 baked
필드가 없어 항상 boneXforms 경로 → 무관.)

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

#### 모델 scale 런타임 적용 (setModel + body_.scale 합성)

Unity에서 모델 루트에 `localScale`을 걸어 키운 모델(예: Hobgoblin)의 scale은 **추출 시점에 베이크하지 않고**, 메시·본·BV·래그돌을 모두 **unscaled로 추출**한 뒤 런타임에 오브젝트 단위 scale(`body_.scale()`)로 한 번만 적용한다.

- **베이크 폐기 이유:** 이전에는 정점/본 `toDress`/BV에 `Scale(rootScale)`을 베이크했는데, 스킨드 모델에서 애니메이션 본 변환(`finalXformData`, rigid)에는 scale 채널이 없어 애니메이션 재생 시 골격이 unscaled로 되돌아가 스키닝이 어긋났다. scale을 world 변환 한 곳에서 균일 적용하면 애니메이션과 독립적으로 정합한다.
- **추출 (unscaled, `ModelExtractor.cs`/`ModelExtractorForServer.cs`/`ExtractUtil.cs`):** Geometry 헤더(서버는 `ModelName` 직후)에 `ModelScale`(Vec3, = Unity root localScale) 필드를 기록한다. dress 변환은 `D = root.worldToLocal·node.localToWorld`(scale 제거)로 정점/노멀/탄젠트/bounds를 베이크하고, 본 `Dress`/`ToLocal`, BV `center`/`size`, 래그돌 `halfExtents`/`center`도 모두 unscaled. 노드 `LocalMatrix`/`DressMatrix`는 identity 유지(`meshXform` no-op).
- **런타임 주입 (`Object::setModel`):** `modelBaseScale_ = pModel->baseScale`을 흡수하고, `body_.scale() = modelBaseScale_ ⊙ instanceScale_`(component-wise)로 합성한다. `instanceScale_`은 per-instance 게임플레이 scale(`setScale`, 기본 1). 둘 중 하나가 바뀌면 `applyCompositeScale()`이 재합성 + `rebuildBodyBVH()`. setModel/setScale 호출 순서와 무관하게 합성식이 동일.
- **정합 (BV-mesh):** 렌더(`renderState_.world = scale(scale)·orient·pos`)와 BV(`rebuildBodyBVH`의 `transformShapeRigid` / 본-부착 `halfExtents*scale`)가 **동일한 `body_.scale()` 단일 경로**로 mesh와 BV를 같이 키워 정합한다. 본-부착 BV의 center는 `objWorld`(scale 포함)로 변환되어 mesh 스키닝(`vertex·anim·world`)과 일치하고, **회전은 scale을 뺀 rigid 행렬에서 추출**한다 — scale 섞인 행렬에 `quatRotMat`/`XMQuaternionRotationMatrix`를 먹이면 쿼터니언이 왜곡되므로, 클라 `rebuildBodyBVH`는 `objRigid`(scale 없는 object world)를 별도로 만들어 `boneToWorldRigid`로 회전을 뽑고(디버그 `update`의 `worldBVs`도 동일), 서버 `transformOBBByMatrix`는 회전 추출 직전 basis 행을 정규화한다.
- **래그돌 (`Ragdoll::build(..., modelScale)`):** `halfExtents`·관성·`capsuleOffset`에 합성 scale을 곱하고 `Ragdoll::modelScale_`에 저장한다. 본 위치는 `objectWorldMat`(= `renderState().world`, scale 포함)이 처리한다. **활성 시 메시 크기 유지:** `syncToFinalXforms`는 `boneWorldMat = scale(modelScale_)·makeRigidMat(...)`로 scale을 주입해야 한다 — rigid 행렬만 쓰면 `finalXform·world`에서 `objectWorldMat`의 scale이 상쇄되어 메시가 unscaled로 돌아간다(크기 원복 버그). 회전 추출(`extractOrient`)은 basis를 정규화해 scale-safe하게 하고, `syncFromPoseDFS`는 seed와 동일하게 `boneOrigin + orient.rotate(capsuleOffset)`로 통일한다. joint anchor는 `activate`의 `resetAnchors`가 seed된 scaled body 위치에서 재계산하므로 자동 scaled.
- **서버(RoomServer):** `ModelScale`을 읽어 `setModel`에서 동일하게 흡수(`Object`에 `modelBaseScale_`/`instanceScale_` 대칭 도입). `updateAnimBones`의 `entityWorld`에 `scale(body_.scale())`을 추가하고, 본-부착 BV의 `halfExtents`에 scale을 별도로 곱한다(`transformOBBByMatrix`는 center만 변환하고 회전은 정규화한 basis에서 추출). scale은 모델 고정값이라 네트워크 전송 불필요(클라/서버 동일 `.bin`).
- **제약:** **균일(uniform) scale만 지원**(x=y=z) — shear 및 비균일 회전추출 이슈 회피. **포맷에 `ModelScale` 필드가 추가되어 모든 `.bin`(클라+서버) 재추출 필요.**

#### 스킨드 메시 노드 변환 베이크 (skinned node dressXform → 정점)

위 모델 scale은 **모델 루트(root) localScale**을 다루지만, **스킨드 메시 노드 자체(SkinnedMeshRenderer 트랜스폼)의 변환**은 별개 문제다. FBX 임포트 시 일부 에셋은 SMR 노드에 회전(예: snake — Z/Y-up 변환의 X축 90°)이나 단위 스케일(예: birdy/Isys — cm→m의 0.01)이 남는다.

- **증상:** snake가 바닥에 누워야 하는데 90° 서 있고(스크린샷의 얇은 세로선), birdy 말단이 늘어남. **본 팔레트 수학·bind pose·애니메이션 정합은 정상**(frame-0 skin 행렬 특이값 전부 1.0)인데도 깨진다.
- **원인 (정점이 팔레트와 다른 공간):** 본 팔레트(`finalXformData`=`toLocal·animDress`)는 **root 기준 dress 공간**에서 만들어지고(`toLocal = bone.worldToLocal·root.localToWorld`), 스킨 정점이 그 공간에 있어야 한다. 그런데 스킨드 정점은 **raw mesh-local**이라 노드에 회전/스케일이 있으면 어긋난다. 해법은 정점을 dress 공간으로 미리 굽고 노드 행렬을 항등으로 두는 것(`position·anim·I·objWorld`).
- **베이크 변환 (핵심 — 정점별 LBS, 실제 bindpose 사용):** mesh-local→dress 변환은 **SMR 노드 변환이 아니라 `mesh.bindposes`에서** 와야 한다. Unity 는 스킨드 렌더링에서 SMR 노드 자체 변환을 **무시**하고 `bone.localToWorld·bindpose` 로만 정점을 배치하기 때문이다. 본 b 의 dress 스킨 행렬 `dressSkin[b] = root.worldToLocal·bones[b].localToWorld·bindposes[b]` 를 구하고, **정점 i 를 가중 합 `Σ w·dressSkin[boneIndex]` 로 굽는다**(정점별 LBS). 이는 본 팔레트가 rest 포즈에서 만드는 변형과 정확히 동일하다. 구현: `BuildSkinBakeMatrices`(정점별 `Matrix4x4[]`) → `ExtractMesh`가 position=`MultiplyPoint3x4`, normal/tangent=`MultiplyVector`(정규화, 런타임 셰이더가 normal 에 anim 을 그대로 곱하는 것과 일치). boneWeight 의 본 인덱스는 `boneIdxMap` 재매핑 '전'(smr.bones/bindposes 평행 배열) 기준.
- **왜 정점별이어야 하나 (단일 행렬의 한계):** 임의의 본 하나로 `M = bone.l2w·bindpose` 단일 행렬을 쓰면, **씬의 rest 포즈가 FBX bind 와 다를 때** 본마다 `dressSkin` 이 달라 메시 전체가 그 본의 포즈 오차만큼 **강체로 기울어진다**(snake 재추출 1차 시도에서 모양은 살았으나 기울던 증상). 씬==bind 인 birdy 는 모든 `dressSkin` 이 동일해 정점별/단일이 같은 결과.
- **과거 버그:** `bakeXform = root.worldToLocal·node.localToWorld`(SMR 노드 변환)로 구웠는데, 이는 `bindpose = bone.worldToLocal·SMR.localToWorld` 인 모델(birdy — 0.01 스케일이 bind 에 반영됨)에서만 우연히 맞았다. snake 처럼 SMR 노드에만 90° 회전이 남고 bindpose 는 root 기준인 모델에서는 **존재하지 않는 90° 를 정점에 굽어** 메시가 90° 서버렸다.
- **모델 root scale과 무관:** `bakeXform`은 `root.worldToLocal`로 root 자신의 transform(스케일 포함)을 상쇄하므로 노드의 **상대** 변환만 굽는다. 모델 root scale은 여전히 `ModelScale`→`baseScale`→body scale 경로로 분리 적용된다.
- **런타임/셰이더/서버 변경 0.** 서버 추출기(`ModelExtractorForServer.cs`)는 정점을 저장하지 않아 영향 없음(서버 `.bin` 재추출 불필요).
- **재추출 필요(클라):** 추출기 수정 후 **반드시 `snake.bin` 재추출**(기존 `.bin` 에는 잘못 구워진 변형이 남아 있음). `.bin` 만으로는 정점이 옳게 구워졌는지 노드 항등 여부로 구분 불가 — 반드시 수정된 추출기로 다시 뽑아야 한다.
- **다른 모델 재추출 사이드이펙트 없음 (검증):** 정점별 LBS 베이크는 모든 스킨드 메시에 적용되지만 다음과 같이 안전하다.
  - **정적(non-skinned) 메시:** `skinBakeMats=null` 로 베이크 분기 미진입 → 완전 불변(정점 raw, 노드 변환은 meshXform 유지).
  - **스킨드 + 노드 항등 + scene==bind:** 모든 `dressSkin[b]=I` 라 `Σ w·dressSkin=I` → 결과 동일(부동소수 오차 무시). 현재 정상 렌더되는 모델 전부 이 범주임을 확인 — `.bin` 메시 노드가 모두 항등: goblin/Hobgoblin/mushroom/slime/bomber/treant/Grandbaum(+player). 이들은 재추출해도 형상 불변(바이트만 미세 변동).
  - **스킨드 + 노드 비항등 또는 scene≠bind:** snake/birdy/Isys 등. 정점별로 본 팔레트 rest 와 정확히 정렬 → 개선(회귀 아님). 구버전 단일 행렬 베이크가 scene≠bind 에서 일으키던 **강체 tilt 도 해소**.
  - normal/tangent 는 런타임 셰이더와 동일한 `MultiplyVector`(정규화) 사용 → 균일 스케일에서 inverse-transpose 와 동일, 비균일에서도 런타임과 일치. 폴백(가중치 합 0 / bind·weight 정보 부족)은 유효 스킨드 메시에선 발동하지 않는다.

#### Baked 애니메이션 행렬 bindpose (AnimationExtractor → baked 텍스처)

위 "스킨드 메시 노드 변환 베이크"는 **`ModelExtractor` 의 정점 베이크**가 `mesh.bindposes` 를 써야 한다는 것이고, 같은 부류의 버그가 **`AnimationExtractor.cs` 의 baked 애니메이션 행렬**에도 있었다(Treant baked 모드 본 꼬임).

- **증상:** Treant 가 **baked 모드로 전환되는 순간 모든 클립에서 본이 꼬여** 렌더된다. keyframe 모드(근거리)·BV(본 부착)는 정상이고 다른 몬스터도 정상. **재추출해도 (리임포트 전까지는) 동일**.
- **두 경로가 같은 정점에 곱하는 행렬:** 스킨드 셰이더(`pbrDeferredSkinned.hlsl` VSMain)는 `position`(원본 메시 정점)에 `anim = Σ w·M[b]`(전체 스키닝 행렬, mesh-space→dress)를 곱한다.
  - **Keyframe:** `gBoneData` 팔레트 = `toLocal`(모델 `.bin` 의 본 inverse-bind, scene-rest) · 애니 dress 변환. 즉 bindpose 가 **모델 파일**에서 와 메시와 항상 정합.
  - **Baked:** `bakedSamplesOfBones` 텍스처에 전체 스키닝 행렬을 **추출 시점에 구워** 넣음(`AnimationExtractor.SampleMatrices` = `sampleTarget.W2L·bone.L2W(t)·bindpose[b]`).
- **원인:** `bindpose[b]` 를 `ProcessBoneHierarchy` 에서 `bone.worldToLocal·skeleton.localToWorld`(**씬 rest 역바인드 = dress→bone-local**)로 잡았다. 올바른 값은 **`mesh.bindposes[b]`(mesh→bone-local)** — `ModelExtractor` 의 `dressSkin` 과 동일 규약. **씬 rest == FBX bind 인 모델(goblin 등)은 둘이 같아 우연히 동작**하지만, **Treant 는 씬 rest ≠ FBX bind** 라 baked 행렬이 모델의 dress-bake 메시와 어긋나 꼬인다.
- **수정 (`AnimationExtractor.cs`):** `BuildBindposeMap()` 추가 — `Sample Target`(없으면 Target Skeleton) 아래 모든 `SkinnedMeshRenderer` 를 돌며 `bones[i] → sharedMesh.bindposes[i]`(평행 배열, first-wins) 매핑. `ProcessBoneHierarchy` 는 이 매핑을 우선 사용하고, 미스 시 기존 씬 rest 폴백(어떤 SMR 에도 안 묶인 헬퍼/루트 본은 정점 가중치가 없어 baked 행렬이 안 쓰이므로 무해).
- **곁들인 수정 (sampleCnt):** `SampleMatrices` 의 `sampleCnt` 를 `clip.length·clip.frameRate` → `clip.length·fps`(=`bakedSampleRate`)로. 샘플 간격이 `1/fps` 인데 개수를 native `frameRate` 로 세면 클라가 인덱싱하는 프레임 수(`length·bakedSampleRate`)와 어긋나 — `frameRate < fps` 면 **애니가 중간에 멈추고** `>` 면 텍스처가 낭비된다.
- **운영 제약 (중요):** 추출기는 Unity 에디터 C# 스크립트라 C++/런타임에서 검증 불가. 수정 후 **Unity 에서 Treant(및 씬 rest≠bind 인 모델) anim 을 재추출 + 반드시 리임포트** 해야 baked 샘플이 새 규약으로 다시 구워진다. (`Sample Target` 은 **메시(SMR)를 포함한 풀 모델**이어야 매핑이 채워진다 — 맨 스켈레톤 rig 면 전부 폴백으로 떨어져 fix 가 무효.) goblin 처럼 scene==bind 인 모델은 재추출해도 결과 동일(회귀 없음).

---

## 미니맵 (Minimap)

우상단 top-down North-up 미니맵(제거된 Hi-Z 디버그 프린트 자리). 코드 위치는 `docs/CODE_INDEX.md` "미니맵" 섹션 참조. 핵심 설계 결정만 여기 정리한다.

### 월드 고정 베이크 + 매 프레임 UV 스크롤 (스크롤의 핵심)

미니맵 지형 배경은 **플레이어 청크 중심의 N×N 청크(`kMinimapCoverageChunks`=7) 월드 고정 정사각 영역**을 직교(ortho) 카메라로 캐시 RT에 굽는다(`MinimapTerrainPipeline`, diffuse-only, PS alpha=1=로드 마스크). 베이크는 **청크 로드/언로드(dirty) 시에만** 수행한다. 매 프레임은 다시 굽지 않고, `MinimapHUD`가 플레이어 현재 위치 기준 **UV sub-rect**로 텍스처를 샘플해 스크롤한다:

- 베이크 카메라: `lookAt(C+(0,cov*4,0), C, up=(0,0,1))` + `ortho(-H,H,-H,H,...)`, `C`=청크 중심, `H=cov/2`. up=+Z라 **월드 +Z(북)→텍스처 위(V작음)**, +X(동)→텍스처 오른쪽(U큼).
- 매 프레임 UV: `scaleU=scaleV=2·vr/cov`, `biasU=(px-vr-cx+H)/cov`, `biasV=((cz+H)-(pz+vr))/cov`. `vr`=줌 반영 시야 반경. `UIPipeline::DrawEvent.uvScaleBias`(uv'=uv·xy+zw)로 전달. 플레이어가 중앙에 고정되고 지형이 반대로 흐른다. 같은 청크 안 이동도 UV만으로 스크롤(재굽기 없음).
- 줌(Shift+휠): `vr=clamp(baseViewRadius/zoom, 5, cov·0.45)`. 배경과 엔티티 아이콘이 동일 `vr`을 써 일관.
- 미로드 청크 영역은 베이크 시 비어(검정)→ `MinimapFogBlurPipeline`(2-pass box blur + `lerp(black,srcRGB,blurredAlpha)` 합성)이 가장자리를 fog-of-war로 페이드. 스크롤 시 fog도 함께 이동.

### 지형 splat + scatter prop 반영 (베이크 내용)

미니맵은 "어차피 베이크"이므로 단순화하지 않고 씬 자산을 그대로 반영한다:
- **지형**: `MinimapTerrainPipeline`이 splatMap 가중치로 diffuse 레이어를 블렌드(메인 terrain과 동일). 풀숲/길/흙이 매크로 색으로 구분된다.
- **prop**: `MinimapPropPipeline`이 scatter prop의 **BVH prop(나무/바위/메시 랜드마크)만** top-down albedo + alpha-cutout으로 지형 위(texA)에 겹쳐 굽는다(나무 캐노피가 잎 모양 blob으로 보임). 풀/꽃(비-BVH)은 다수라 미니맵을 도배하므로 제외. 패스 순서: 지형(2)→prop(2b)→fog 블러(3). prop도 alpha=1을 써 fog 커버리지에 기여. 인스턴스는 per-draw(캡 `kMaxDrawEvents`=4096, 베이크 영역 밖은 컬링) — 베이크가 드물어 허용.
- **fog 블러는 alpha만**: `MinimapFogBlurPipeline`은 커버리지 alpha만 2-pass 블러하고 **RGB는 중심 탭으로 선명 통과**, `finalRGB = sharpRGB × blurredAlpha`로 합성한다. RGB까지 블러하면 미니맵 전체가 흐려진다(초기 버그) — 반드시 alpha만 블러할 것.

### 단일 RT (per-room 아님) — 깜빡임 방지 + 해상도

캐시 RT는 GBuffer/SceneColor/Portrait처럼 per-room으로 두지 **않고 단일 인스턴스**다(IBL과 동일). 이유: 미니맵은 매 프레임 GPU가 쓰는 게 아니라 **재굽기 시에만 쓰고 매 프레임 읽는다**. 모든 명령은 단일 direct 큐(`cmdQ_`)에 제출 순서대로 **직렬 실행**되므로 같은 큐에서의 GPU-write→GPU-read는 프레임 간에도 해저드가 없다. per-room으로 두면 dirty 1회당 한 room만 갱신돼 나머지 room이 빈(검정) 채로 남아 `frameIdx_%N` 순환 시 **검정↔정상 깜빡임**이 났다(v2 초기 버그) — 단일 RT로 근본 해소. 재굽기도 1프레임으로 끝난다(N프레임 분산 불필요). per-draw 상수버퍼만 per-room cyclic(`frameIdx_%N`)으로 재사용.

### 커버리지 = 시야 기준(청크 크기와 무관) + 이동 재굽기

청크 변(邊)은 200m라 "N청크 커버리지"(7×200=1400m)를 512px에 구우면 0.37px/m로 시야(120m)가 ~44px만 샘플 → splat·prop이 뭉개져 균일하게 보였다(v2 초기 버그). 해법: 커버리지를 **청크와 무관하게 시야에 맞춰** `kMinimapCoverageWorld`(360m)로 작게 잡아 해상도(1024px → 2.84px/m)를 확보하고, 재굽기는 청크 로드/언로드 **또는 플레이어가 베이크 중심에서 `kMinimapRebakeMoveThreshold`(50m) 이상 이동** 시 수행한다(시야가 텍스처 가장자리에 닿기 전에 재중심). prop은 베이크 영역(±커버리지/2) 밖이면 컬링해 재굽기당 드로우를 제한.

미니맵 RT 초기 상태는 `PIXEL_SHADER_RESOURCE`(첫 재굽기 전 UI가 샘플해도 상태 불일치 디버그 에러 방지).

### 엔티티 아이콘 / 보스 식별

아이콘은 3D 렌더 없이 UI 레이어에서 world XZ→플레이어 상대 오프셋(`(e-p)/vr`)을 픽셀로 선형 변환한 단색 quad(`solidColorTex()`+`colorMul`). 보스/중간보스(주황)는 RTTI/`dynamic_cast`가 아니라, 스폰 시(서버 `ObjectType` 권위) `Online::Game::bossNpcIds_` 집합에 npcId를 넣어 판정한다(`dynamic_cast`는 스폰 경로/RTTI 가정에 취약해 폐기). 크기·위치는 `SkillDialHUD`와 같은 `uiScale=min(sw/1024,sh/768)` 해상도 상대화. 몬스터 아이콘은 다수 시뮬레이션을 고려해 작게(3.5px×uiScale).
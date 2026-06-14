# HDR + IBL 작업 진행/재개 문서 (WIP)

> 토큰 절약으로 일시 중지. 재개 시 이 문서부터 읽을 것.
> 설계 원본 플랜: `C:\Users\PC\.claude\plans\optimized-humming-puppy.md`
> 팀: `ibl-hdr` (task list: `~/.claude/tasks/ibl-hdr/`). `TaskList`로 상태 조회 가능.

## 목표 (사용자 확정)
DX12 클라이언트에 **HDR scene-color 파이프라인 + 글로벌 IBL** 도입.
1. HDR 풀 업그레이드(HDR 누적 버퍼 + 단일 tonemap resolve). 2. IBL 맵 런타임 프리컴퓨트. 3. 글로벌 IBL만(스카이박스 1개). 4. 환경 원본은 LDR DDS만 존재(포맷 무관 설계, 내부 맵 RGBA16F). 5. Forward·Deferred 두 경로 모두.

## 작업 방식
사용자 지시로 **에이전트 팀 운영**. Agent 툴이 team_name/name 미지원 → Lead(메인 세션)가 **백그라운드 워커**를 스폰해 위임하고 task list로 조정. 충돌 핫스팟(`gfx.cpp render()`, `shader.hpp`, `pbrLighting.hlsli`)은 Lead 직접 소유.

## 현재 repo 상태 (중요)
- **Phase 0 (Deferred HDR) 완료 + 빌드 통과.** 커밋 안 함.
- **Phase 1a (IBL 프리컴퓨트) 완료 + 빌드 통과.** IBL 맵(irradiance/prefiltered/brdfLUT)이 로드 시 생성됨. **단 아직 조명에 미사용**(Phase 1b가 연결). 즉 지금 실행해도 시각 변화 없음(로드 시 프리컴퓨트만 추가 실행).
- **다음 = Phase 1b (Deferred IBL 셰이딩 통합)** — 미착수. 아래 "Phase 1b 배선 계획"대로 편집하면 됨. 이게 실제 시각 payoff.
- 미검증: 런타임 "룩 동일"(Phase 0) + IBL 맵 시각(Phase 1a) — 사용자가 직접 실행 검증 예정.

### Phase 0 설계 정제 (실행 중 변경 — 중요)
포트레이트 회귀(로비 포트레이트가 forward `illuminateCSM`을 LDR RT에 렌더; `ui.hlsl`은 톤매핑 없이 표시)를 피하려고 **Phase 0를 Deferred 경로 전용 HDR로 한정**했다.
- Deferred 경로(기본)에서 forward `illuminateCSM`은 **포트레이트만** 사용 → 손대지 않으면 포트레이트 안전.
- 그래서 톤매핑 제거는 `pbrDeferredLighting.hlsl` 한 곳만. `illuminateCSM`/`terrain.hlsl`/`illuminate`/forward 경로/포트레이트 **전부 무수정**.
- **Forward 경로 HDR 누적은 후속**(Phase 0b/2): `applyTonemap` 플래그(forward `PBRShader`/`PBRSkinnedShader` PerFrameData에 uint 추가, 메인=0·포트레이트=1, `illuminateCSM` 말미 조건부 톤매핑)로 처리. forward는 현재 inline-tonemap 유지(룩 동일). Phase 1b에서 forward IBL은 inline-tonemap illuminateCSM에 computeIBL 추가로 동작(포트레이트도 환경광 받음).
- 알려진 minor: deferred GBuffer 디버그뷰(G키)는 HDR 경로에서 이미 감마인코딩된 값이 resolve에서 한 번 더 톤매핑/감마됨 → 더블감마(개발용·무해, 추후 정리).

### Phase 0 완료 편집 내역
- `pbrDeferredLighting.hlsl`: Reinhard+gamma 제거(fog lerp 유지, 선형 반환). [Task#2]
- `gfx.hpp`: `#include "TonemapPipeline.hpp"` + `TonemapPipeline::Resources resourcesTonemapPipeline_{};` 멤버.
- `gfx.cpp` [Task#3]:
  - init: `TonemapResolveShader` 등록 + `resourcesTonemapPipeline_.perDrawcallData` init(count=1).
  - initSharedResources: `SceneColor::addSceneColor`(addGBuffer 직후).
  - resize: `SceneColor::eraseSceneColor` + `addSceneColor`.
  - clear 단계(deferred 블록): SceneColor `transitionToWrite` + black clear.
  - 디스패처 블록: `skyboxRtv`(deferred=SceneColorHDR / forward=백버퍼 경로분기) + `sceneColorSrv` 계산, skybox RTV 교체, `tonemapPipelineDispatcher` 생성(SceneColor SRV→백버퍼).
  - deferred lighting RTV(2224): `deferredLitRtv`(SceneColorHDR)로 교체.
  - deferred 경로 skybox 직후·BV 직전: SceneColor `transitionToRead` 배리어 + `tonemapPipelineDispatcher` resolve draw.
- 워커 Task#1 산출물(SceneColor 네임스페이스, TonemapPipeline, tonemapResolve.hlsl, createTonemapResolveShader, vcxproj 등록)은 그대로 사용.

### (구) 원래 상태 메모
워커가 Task #1을 디스크에 작성 완료한 상태에서 Lead가 Task #2/#3를 위 내역대로 완료함.

## 작업 목록 상태 (team `ibl-hdr`)
- #1 [완료] Phase0 SceneColorHDR 리소스 + tonemap resolve 셰이더/PSO/Dispatcher (워커 산출)
- #2 [진행중-미착수] Phase0 lit 셰이더 인라인 톤매핑 제거 → 선형 (Lead)
- #3 [진행중-미착수] Phase0 render() 리타깃 + resolve 패스 삽입 (Lead) — #1 의존(해소됨)
- #4 [대기] Phase0 게이트: 빌드 + "룩 동일" 검증 — #2,#3 의존
- #5 [대기] Phase1a IBL 프리컴퓨트 컴퓨트셰이더+리소스+디스패처 (워커 위임 예정) — #4 의존
- #6 [대기] Phase1a render() 프리컴퓨트 트리거 배선 (Lead) — #5 의존
- #7 [대기] Phase1b PerFrameData 4종 IBL 필드 추가 — #4 의존
- #8 [대기] Phase1b computeIBL 셰이딩 통합 + GB2 emissive (워커 위임 예정) — #6,#7 의존
- #9 [대기] Phase1b render() 매프레임 IBL cbuffer 업로드 (Lead) — #8 의존
- #10 [대기] Phase1b 게이트 + Phase2 검증·디버그·문서

---

## 워커 Task #1 산출물 (= Task #3 통합 계약)

### 생성 파일
- `client/tonemapResolve.hlsl` — fullscreen VS(SV_VertexID) + PS. `sampleBindless(idxSceneColor, uv)` 후 **기존 곡선 정확 복제**: `color/(color+1)` → `pow(abs(color),1/2.2)`. cbuffer b0: `int4 idxSceneColor`.
- `client/TonemapPipeline.hpp`/`.cpp` — `TonemapPipeline::{Resources, Dispatcher}`.

### 수정 파일
- `client/sharedResources.hpp` — `struct SceneColorData{ Texture color; D3D12_CPU_DESCRIPTOR_HANDLE rtv; D3D12_RESOURCE_STATES curState; u32t width,height; }` + `namespace SceneColor` 선언(L172-208).
- `client/sharedResources.cpp` — `namespace SceneColor`(L842-933): 자체 `createSceneColorRT`(R16G16B16A16_FLOAT, RTV+bindless SRV, **BilinearClamp 샘플러**), `addSceneColor`, `transitionToWrite/Read`(curState 추적), `eraseSceneColor`(freeRTV/freeSRV 후 clear). **GBuffer 코드 무수정.**
- `client/shader.hpp` — `createTonemapResolveShader(ID3D12Device*, ID3D12RootSignature*)` 선언 + `namespace TonemapResolveShader{ struct PerDrawcallData{ BindlessIndex idxSceneColor; }; }`.
- `client/shader.cpp` — `createTonemapResolveShader`(L3741~): graphics PSO, VB없음, depth 없음(DepthEnable false), Cull none, RTV `R8G8B8A8_UNORM`, Blend 비활성.
- `client/client.vcxproj` + `.filters` — 신규 .cpp/.hpp(그래픽스\파이프라인), .hlsl(셰이더 파일, FxCompile ExcludedFromBuild=런타임 compileShader) 등록 완료.

### Lead가 gfx.cpp에서 호출할 API
```cpp
// 리소스 (per-room; roomCnt = backBuffers_.size())
SharedResources::SceneColor::addSceneColor(device_.Get(), W, H, backBuffers_.size(), rtvPool_, srvTexPool_);
SharedResources::SceneColor::transitionToWrite(roomIdx, cmdList);   // SRV→RT
SharedResources::SceneColor::transitionToRead (roomIdx, cmdList);   // RT→SRV(PIXEL_SHADER_RESOURCE)
SharedResources::SceneColor::eraseSceneColor(rtvPool_, srvTexPool_); // resize 시
// RTV 핸들:        SharedResources::SceneColor::sceneColorData[roomIdx].rtv
// SRV bindless:    SharedResources::SceneColor::sceneColorData[roomIdx].color.idxSrv  (BindlessIndex)

// 셰이더 등록(GFX::init, ~L309 PBRDeferredLightingShader 옆)
shaders_.try_emplace("TonemapResolveShader", createTonemapResolveShader(device_.Get(), defaultRootSig.get()));

// Resources 멤버(gfx.hpp) + init(GFX::init, Skybox Resources 패턴 ~L496-501)
//   TonemapPipeline::Resources resourcesTonemapPipeline_;   // gfx.hpp
//   resourcesTonemapPipeline_.perDrawcallData = createConstantBufferArray(
//       device_.Get(), sizeof(TonemapResolveShader::PerDrawcallData), 1u, backBuffers_.size(), "Tonemap_PerDrawcallData");
//   gfx.hpp 또는 gfx.cpp에 #include "TonemapPipeline.hpp"

// Dispatcher 생성자 인자 순서 (SkyboxPipeline 미러):
TonemapPipeline::Dispatcher(
  tmpDescriptorHeaps, &srvTexPool_, &srvTexArrayPool_, &srvTexCubePool_, &samPool_, &cmpSamPool_,
  rootSigs_.at("DefaultRootSignature"), shaders_.at("TonemapResolveShader"),
  cmdQ_, viewport, clRect, backBufferRtvs_[backbufIdx],
  &fenceToSignal, &resourcesTonemapPipeline_, &cmdListPool_,
  SharedResources::SceneColor::sceneColorData[roomIdx].color.idxSrv, roomIdx);
// 구동: updateGPUDataSingleThreaded() → drawSingleThreaded() (OMSetRenderTargets(backbuffer)+fullscreen draw)
```

---

## Task #2 — 인라인 톤매핑 제거 (Lead, 미착수)
선형 HDR 출력으로 전환. 제거 대상(워커 Task#1 완료 후이므로 이제 편집 안전):
- `pbrLighting.hlsli` `illuminateCSM()` L539-540: `color = color/(color+1); color = pow(abs(color),1/2.2);` 제거 → `return float4(color, albedo.w)` 선형.
- `pbrDeferredLighting.hlsl` L151-153: Reinhard+gamma 제거(주석+2줄). **fog lerp(L149)는 유지**, `return float4(color,1)` 선형.
- `terrain.hlsl` L201-203: 주석+Reinhard+gamma 제거 → `return float4(color,1)` 선형.
- `illuminate()`(SINGLE_SHADOW판) = **죽은 코드**(grep 0건), 손대지 말 것.

### ⚠ 포트레이트 회귀 위험 (미해결 — 재개 시 먼저 확인)
로비 포트레이트 패스(`gfx.cpp` L2497~, `PBRSkinnedPipeline::Dispatcher` L2536)가 **forward PBRSkinned→`illuminateCSM`**를 오프스크린 LDR RT에 렌더한다. `illuminateCSM` 톤매핑을 무조건 제거하면 포트레이트가 선형(어둡게) 깨질 수 있음.
- **확인 필요:** `SharedResources::Portrait` RT 포맷 + UI 합성 경로(`ui.hlsl`/uiPipeline의 `lobbyPortraitTexture` 샘플)가 별도 톤매핑/sRGB를 하는지.
- **해결안(유력):** forward `PBRShader::PerFrameData` / `PBRSkinnedShader::PerFrameData`에 `uint applyTonemap`(+pad) 추가, `illuminateCSM` 말미 `if(applyTonemap){...}`. 메인 forward 패스=0(→SceneColorHDR, resolve가 톤매핑), 포트레이트 패스=1(인라인 톤매핑 유지). Terrain은 포트레이트에 없으므로 무조건 제거(플래그 불필요). 이 플래그는 Phase0 한정 Lead 소유 변경(Phase1b IBL 필드 추가와 가법적으로 공존).
- (deferred 경로는 `illuminateFromGBuffer` 사용, 이미 pre-tonemap이라 무관.)

---

## Task #3 — gfx.cpp render() 통합 배선 (Lead, 미착수)
인덱스: `roomIdx = frameIdx_ % backBuffers_.size()`(per-room 리소스), `backbufIdx = swapChain_->GetCurrentBackBufferIndex()`(백버퍼). render()=`gfx.cpp` L1505~.

**파티션:** HDR그룹(→SceneColorHDR, resolve 이전) = deferred lighting + skybox + forward PBR/PBRSkinned/Terrain main. / Resolve = skybox 직후. / LDR 오버레이(→백버퍼, resolve 이후) = BV·billboard·파티클·UI.

1. **GFX::init**: TonemapResolveShader 등록(L309 옆) + `resourcesTonemapPipeline_` init(Skybox Resources 패턴 ~L496-501) + `#include "TonemapPipeline.hpp"`. gfx.hpp에 `TonemapPipeline::Resources resourcesTonemapPipeline_;` 멤버.
2. **initSharedResources** (L1173 `addGBuffer` 직후): `SceneColor::addSceneColor(...)` 호출(양 경로 공용이라 무조건).
3. **resize** (L1420; L1434 eraseGBuffer / L1497 addGBuffer 재호출): `SceneColor::eraseSceneColor` + `addSceneColor` 추가.
4. **render() clear 단계** (L1560-1600): SceneColorHDR `transitionToWrite(roomIdx)` + `ClearRenderTargetView(sceneColorData[roomIdx].rtv, black)` 추가. (roomIdx를 L1558 backbufIdx 근처에서 미리 계산.)
5. **lit 디스패처 RTV 리타깃** (생성자 캡처, L1635-1960): `pbrPipelineDispatcher`(L1679), `pbrSkinnedPipelineDispatcher`(L1692), `skyboxPipelineDispatcher`(L1712), `terrainPipelineDispatcher`(L1874)의 RTV 인자 `backBufferRtvs_[backbufIdx]` → `sceneColorData[roomIdx].rtv`. (skybox는 양 경로 공용이라 무조건 HDR OK.) **DSV는 기존 `depthBufferDsvs_[backbufIdx]` 유지.**
6. **deferred lighting RTV** (L2224 `OMSetRenderTargets(...&backBufferRtvs_[backbufIdx]...)`) → `sceneColorData[roomIdx].rtv`. depth copy(L2257)는 불변.
7. **deferred 경로 resolve 삽입** (skybox draw L2278-2279 직후, BV L2281 직전): SceneColor `transitionToRead(roomIdx)` 배리어 cmdList → `tonemapDispatcher.updateGPUDataSingleThreaded()/drawSingleThreaded()`. (deferred는 BV가 이미 skybox 뒤라 순서 OK.)
8. **forward 경로 resolve + BV 이동** (L2349-2419 single / L2420-2491 MT): main(pbr/skinned/terrain)·skybox는 SceneColorHDR. **BV(L2382/L2453)를 skybox+resolve 뒤로 이동.** skybox 직후 transitionToRead+resolve, 그 뒤 BV→billboard→파티클(백버퍼). 양 분기(MT/non-MT) 모두.
9. tonemapDispatcher는 L1635-1960 디스패처 블록에 함께 생성(roomIdx 필요).

**주의:** sceneColorData가 비어있을 가능성(초기화 전 render)은 GBuffer와 동일하게 initSharedResources에서 항상 생성되므로 정상 흐름에선 비지 않음. 크래시 방지가 걱정되면 `!sceneColorData.empty()` 가드 후 lit RTV를 backbuffer로 폴백.

---

## Task #4 — Phase 0 게이트
빌드: VS2022 `GameServer.sln` client (MSBuild 단독 시 `/p:SolutionDir` 필수, 메모리 `reference_build_command`). 실행 후 동일 씬/카메라 before↔after 스크린샷 = **시각적 동일**(곡선 보존). Forward·Deferred 둘 다, **로비 포트레이트** 포함 무회귀 확인.

## 확정된 코드 사실 (재확인 불필요)
- `createColorRT`(sharedResources.cpp:629) / `addGBuffer`(:731) / GBuffer `transitionToWrite/Read`(:765/:788) / `eraseGBuffer` 존재.
- `freeRTV`(gfxUtil.cpp:1142) / `freeSRV`(:1156) / `createConstantBufferArray(device, elemBytes, count, roomCnt, name)`(gfxUtil.cpp:596) 존재.
- `GFX::resize`(gfx.cpp:1420): L1434 eraseGBuffer, L1497 addGBuffer 재호출. ResizeBuffers L1451.
- 셰이더 등록 블록 gfx.cpp L290-318. Skybox Resources init L496-501(perFrameData.init + perDrawcallData=createConstantBufferArray). deferred lighting Resources: `deferredLightingPerFrameData_`/`deferredLightingLightData_`(L755-758).
- deferred lighting PerFrameData 빌드 L2148-2183(IBL 필드 채울 곳, Phase1b/#9). forward PBR/Skinned/Terrain PerFrameData 빌드 지점은 각 Dispatcher 내부(mainUpdate)일 것 — Phase1b 때 확인.
- `RootSig::get()` → `ID3D12RootSignature*`. create*Shader는 `ID3D12RootSignature*` 받음(`defaultRootSig.get()`).
- 디스크립터 풀(gfx.cpp L199-234): srvTexPool_(1800), srvTexArrayPool_(100), srvTexCubePool_(100), uavPool_(100), rtvPool_, dsvPool_.

## 재개 시 아직 안 읽은 것(필요시)
- `SharedResources::Portrait` RT 포맷 + UI 포트레이트 합성 경로(포트레이트 회귀 판정용) — **Task#2 착수 전 필수**.
- gfx.cpp Resources init 정확한 라인(L490-510 Skybox 패턴), gfx.hpp Resources 멤버 선언 위치 + 파이프라인 include 위치.
- forward PBR/PBRSkinned/Terrain PerFrameData 빌드/스테이징 위치(Phase1b #7/#9).

## Phase 1a 진행 중 (현재 위치)
- **IBL 컴퓨트 셰이더 3종 = Lead가 작성 완료**: `iblIrradiance.hlsl`(cosine convolution), `iblPrefilter.hlsl`(GGX importance, 256 samples), `iblBRDFLUT.hlsl`(split-sum, k=rough²/2). D3D 큐브 basis(+X,-X,+Y,-Y,+Z,-Z), GL Y-flip 없음, envIsLDR 역Reinhard 토글. 공통 계약: cbuffer `IBLParams`@b0(48B: int4 idxEnv/uint faceRes/mipLevel/mipCount/envIsLDR/float roughness/float3 pad), 출력 UAV@u0("DestTex": 큐브=RWTexture2DArray<float4> / LUT=RWTexture2D<float2>), env=bindless `sampleLevelBindlessCube`. numthreads(8,8,1), 큐브 dispatch z=6.
- **C++ 플러밍 = ibl-compute 워커 진행 중**(백그라운드): shader.hpp/cpp(IBLShader::IBLParams struct + create*Shader 3종 PSO), SharedResources::IBL(irradiance RGBA16F 32²/prefiltered RGBA16F 128² 5밉/brdfLUT RG16F 256², 큐브 밉 UAV=TEXTURE2DARRAY 6슬라이스/SRV=TEXTURECUBE), iblPrecomputePipeline.{hpp,cpp}(precomputeIBL — hiZPassCompute idiom, LoadFence), vcxproj 등록. **gfx.cpp/gfx.hpp는 안 건드림.** 워커가 빌드 검증까지 수행.

### Task #6 (Lead) 배선 계획 — 워커 완료 후
- `GFX::init`(~L309 셰이더 등록부): IBL 컴퓨트 PSO 3종 try_emplace + `iblParamsCBs_ = createConstantBufferArray(device, sizeof(IBLShader::IBLParams), 7, 1, "IBL_Params")`. gfx.hpp에 `ConstantBufferArray iblParamsCBs_;` 멤버.
- `initSharedResources`(addSceneColor 근처): `SharedResources::IBL::addIBL(device, uavPool_, srvTexCubePool_, srvTexPool_)`.
- **`loadRequestedAssets` 끝**(L1390 `waitOnFence("LoadFence")` 직후 ~ L1394 `requestsSkyboxLoad_.clear()` 이전)에 `precomputeIBL(...)` 1회 호출. 이 시점에 스카이박스 큐브 **데이터 상주**(LoadFence 완료) + `requestsSkyboxLoad_[0].pDest`(로드된 `Skybox`)로 `texSkybox.idxSrv`/`.res` 접근. envCurState=워커 보고값(loadTexture 후 상태), envIsLDR=true(현재 LDR), descriptorHeaps={srvCbvUavHeap_.heap, samHeap_.heap}.
- IBL은 정적(해상도 무관) → **resize 무수정**.
- **주의:** 워커 빌드 검증 중에는 gfx.cpp에 워커 함수 호출 추가 금지(미정의 참조로 워커 빌드 깨짐). 워커 완료 통지 후 배선.

## Phase 1b 배선 계획 (재개 시 바로 실행) — Deferred IBL 셰이딩

목표: deferred lighting에서 상수 ambient를 IBL로 교체. GB2.rgb=emissive 전용으로 변경. 시각 payoff 단계.

**IBL 리소스 API(워커 산출, 확인됨):** `SharedResources::IBL::iblData.{irradiance,prefiltered,brdfLUT}.idxSrv`(BindlessIndex), `.prefilteredMipCount`(=5). 이미 PIXEL_SHADER_RESOURCE 상태.

**편집 1 — `pbrLighting.hlsli` 끝(현재 L577 `#endif // DEFERRED_LIGHTING_PASS` 뒤)에 추가:**
IBL 함수는 cbuffer 전역(idxIrradiance 등)을 참조하므로 `#ifdef IBL_ENABLED` 가드(이 매크로 정의한 셰이더만 컴파일). N/V는 **월드 공간**. AO 규약은 기존과 동일하게 `(1-ao)`(ao=0→차폐 없음).
```hlsl
#ifdef IBL_ENABLED
float3 fresnelSchlickRoughness(float cosTheta, float3 F0, float roughness) {
    float3 r = max(float3(1.0f - roughness, 1.0f - roughness, 1.0f - roughness), F0);
    return F0 + (r - F0) * pow(saturate(1.0f - cosTheta), 5.0f);
}
// N, V: world-space. ao: 0=no occlusion (engine convention) -> multiply by (1-ao).
float3 computeIBL(float3 N, float3 V, float3 albedo, float roughness, float metallic, float ao) {
    float  NdotV = max(dot(N, V), 0.0f);
    float3 R     = reflect(-V, N);
    float3 F0    = lerp(float3(0.04f, 0.04f, 0.04f), albedo, metallic);
    float3 kS    = fresnelSchlickRoughness(NdotV, F0, roughness);
    float3 kD    = (1.0f - kS) * (1.0f - metallic);
    float3 irradiance = sampleBindlessCube(idxIrradiance, N).rgb;
    float3 diffuse    = irradiance * albedo;
    float  maxMip     = float(max(prefilteredMipCount, 1u) - 1u);
    float3 prefiltered = sampleLevelBindlessCube(idxPrefiltered, R, roughness * maxMip).rgb;
    float2 brdf       = sampleBindless(idxBRDFLUT, float2(NdotV, roughness)).rg;
    float3 specular   = prefiltered * (F0 * brdf.x + brdf.y);
    return (kD * diffuse + specular) * (1.0f - ao) * iblIntensity;
}
#endif
```

**편집 2 — `pbrDeferredLighting.hlsl`:**
- cbuffer 끝(L51 `float _pad2;` 다음, L52 `}` 앞)에 IBL 필드 추가:
```hlsl
    // IBL
    int4  idxIrradiance;
    int4  idxPrefiltered;
    int4  idxBRDFLUT;
    uint  prefilteredMipCount;
    float iblIntensity;
    float2 _iblPad;
```
- L62 `#include "pbrLighting.hlsli"` **앞**에 `#define IBL_ENABLED` 추가.
- PSMain(L138-143): GB2.rgb는 이제 emissive 전용. computeIBL 가산:
```hlsl
    float3 directLight = illuminateFromGBuffer(posV, posW, normalV, normalW, albedo, roughness, metallic, ao);
    float3 Vworld = normalize(camPos - posW);
    float3 ibl = computeIBL(normalW, Vworld, albedo, roughness, metallic, ao);
    float3 color = directLight + ibl + precompLight;   // precompLight = emissive only now
```
(fog/return L145-154는 그대로.)

**편집 3 — `shader.hpp` `PBRDeferredLightingShader::PerFrameData`(struct 시작 ~L942):**
struct 끝(camPos XMFLOAT3 + _pad2 float 뒤)에 HLSL과 동일 추가(16B 정렬):
```cpp
    BindlessIndex idxIrradiance;
    BindlessIndex idxPrefiltered;
    BindlessIndex idxBRDFLUT;
    u32t          prefilteredMipCount;
    float         iblIntensity;
    XMFLOAT2      _iblPad;
```
(재개 시 struct 끝 위치 정확히 읽고 추가. HLSL cbuffer와 바이트 동일 유지.)

**편집 4 — `pbrDeferred.hlsl` L147 (geometry pass, GB2 emissive 전용화):**
`float3 lightAccum = globalAmbient * albedo.rgb * (1.0f - ao) + emissive;` → `float3 lightAccum = emissive;`
(GB2.a=roughness 불변. globalAmbient는 이제 deferred에서 미사용이나 cbuffer 필드는 유지.)
주의: GBUF_DEBUG_LIGHTACCUM 디버그뷰는 이제 emissive만 표시(무해).

**편집 5 — `gfx.cpp` deferred lighting PerFrameData 빌드(anchor: `lpfd.idxSkybox = skyboxIdxSrv;`) 뒤에 추가:**
```cpp
lpfd.idxIrradiance       = SharedResources::IBL::iblData.irradiance.idxSrv;
lpfd.idxPrefiltered      = SharedResources::IBL::iblData.prefiltered.idxSrv;
lpfd.idxBRDFLUT          = SharedResources::IBL::iblData.brdfLUT.idxSrv;
lpfd.prefilteredMipCount = SharedResources::IBL::iblData.prefilteredMipCount;
lpfd.iblIntensity        = 1.0f;
```

**빌드/검증:** MSBuild client(/p:SolutionDir). 실행 시 금속이 환경 반사, 유전체가 irradiance 받음. iblIntensity로 강도 조절. envIsLDR=true라 LDR 근사(HDR HDRI 확보 전).

### Phase 1b-forward + Phase 2 (후속)
- Forward IBL: pbr.hlsl/pbrSkinned.hlsl(illuminateCSM)에 computeIBL 추가 + PBRShader/PBRSkinnedShader PerFrameData에 IBL 필드 + #define IBL_ENABLED + gfx.cpp forward PerFrameData 스테이징. illuminateCSM은 inline-tonemap이라 ambient를 computeIBL로 교체(포트레이트도 환경광 받음). Terrain도 동일.
- Forward HDR 누적(applyTonemap 플래그) — 별개 후속.
- Phase 2: 디버그뷰(irradiance/specular/BRDF), iblIntensity 튜닝, ACES/bloom 옵션, HDR HDRI 에셋, docs/iblArchitecture.md, CODE_INDEX 갱신.

## Phase 1 요약 (나중)
- 1a: `iblIrradiance/iblPrefilter/iblBRDFLUT.hlsl`(컴퓨트, RWTexture2DArray 큐브면, D3D basis·GL Y-flip 금지, envIsLDR 역톤매핑 토글), shader.cpp PSO(createHiZ*Shader 패턴), `SharedResources::IBL`(irradiance RGBA16F 32²/prefiltered RGBA16F 128² 5밉/brdfLUT RG16F 256², 큐브 밉 UAV=TEXTURE2DARRAY, HiZMap UAV 패턴 sharedResources.cpp:1023), `iblPrecomputePipeline`(hiZPassCompute idiom pbrDeferredSkinnedPipeline.cpp:283-421, LoadFence). 트리거: loadRequestedAssets skybox 로드(L1277) 직후.
- 1b: PerFrameData 4종에 idxIrradiance/idxPrefiltered/idxBRDFLUT(int4)+prefilteredMipCount(uint)+iblIntensity(float)+pad. `computeIBL`(split-sum: diffuse=irradiance(N)*albedo*(1-metallic), spec=prefiltered.SampleLevel(R,rough*maxMip)*(F0*brdf.x+brdf.y), ×ao×intensity) + fresnelSchlickRoughness. illuminate/illuminateCSM/illuminateFromGBuffer/terrain ambient 교체. **pbrDeferred.hlsl:147 GB2.rgb→emissive 전용**(ambient 선계산 제거), lighting이 GB2.rgb emissive 가산. GB2 의미 주석 sharedResources.hpp 갱신.

## 규약 리마인더
- 신규 .cpp/.hpp/.hlsl **한국어 주석 금지**(cp949 빌드오류). .md 문서는 무관.
- using alias 금지. PerDrawcallData=b0/PerFrameData=b1. mathUtil v*M, A*B*C. D3D 큐브 basis. NDC depth[0,1].
- 신규 .cpp/.hpp는 client.vcxproj 등록 필수.

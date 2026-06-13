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
- 워커가 **Task #1 (Phase 0 HDR 타깃+tonemap 파이프라인)** 파일들을 **이미 디스크에 작성 완료**(커밋 안 함, 빌드 검증 안 함).
- **Lead는 아직 어떤 파일도 편집하지 않음.** Task #2/#3 미착수(분석만 완료).
- 즉 working tree = 원본 + 워커의 Task #1 추가분.

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

## Phase 1 요약 (나중)
- 1a: `iblIrradiance/iblPrefilter/iblBRDFLUT.hlsl`(컴퓨트, RWTexture2DArray 큐브면, D3D basis·GL Y-flip 금지, envIsLDR 역톤매핑 토글), shader.cpp PSO(createHiZ*Shader 패턴), `SharedResources::IBL`(irradiance RGBA16F 32²/prefiltered RGBA16F 128² 5밉/brdfLUT RG16F 256², 큐브 밉 UAV=TEXTURE2DARRAY, HiZMap UAV 패턴 sharedResources.cpp:1023), `iblPrecomputePipeline`(hiZPassCompute idiom pbrDeferredSkinnedPipeline.cpp:283-421, LoadFence). 트리거: loadRequestedAssets skybox 로드(L1277) 직후.
- 1b: PerFrameData 4종에 idxIrradiance/idxPrefiltered/idxBRDFLUT(int4)+prefilteredMipCount(uint)+iblIntensity(float)+pad. `computeIBL`(split-sum: diffuse=irradiance(N)*albedo*(1-metallic), spec=prefiltered.SampleLevel(R,rough*maxMip)*(F0*brdf.x+brdf.y), ×ao×intensity) + fresnelSchlickRoughness. illuminate/illuminateCSM/illuminateFromGBuffer/terrain ambient 교체. **pbrDeferred.hlsl:147 GB2.rgb→emissive 전용**(ambient 선계산 제거), lighting이 GB2.rgb emissive 가산. GB2 의미 주석 sharedResources.hpp 갱신.

## 규약 리마인더
- 신규 .cpp/.hpp/.hlsl **한국어 주석 금지**(cp949 빌드오류). .md 문서는 무관.
- using alias 금지. PerDrawcallData=b0/PerFrameData=b1. mathUtil v*M, A*B*C. D3D 큐브 basis. NDC depth[0,1].
- 신규 .cpp/.hpp는 client.vcxproj 등록 필수.

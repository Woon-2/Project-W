 User approved Claude's plan
  ⎿  Plan saved to: ~\.claude\plans\optimized-humming-puppy.md · /plan to edit
     HDR 파이프라인 + 글로벌 IBL 도입

     Context (왜)

     현재 클라이언트의 간접광은 globalAmbient * albedo * (1-ao) 라는 상수 ambient 한 항뿐이라 재질이 환경을 반사하지 못하고 평면적으로 보인다. 사용자 목표는 "렌더링을 더 멋있게" — 이를 위해 Image Based Lighting(환경 기반 diffuse irradiance + specular 반사)을 도입한다.

     전제 사실:
     - 기존 BRDF(pbrLighting.hlsli)는 표준 Cook-Torrance(GGX + Smith-Schlick + Schlick Fresnel, F0=0.04→metallic lerp)라 split-sum IBL 수식이 거의 그대로 이식된다.
     - 큐브맵 bindless 인프라(TextureCubePool, sampleLevelBindlessCube, 큐브 SRV/UAV)와 컴퓨트 디스패치 인프라(Hi-Z)가 이미 존재해 재사용도가 높다.
     - 그러나 현재 ambient는 deferred geometry pass에서 GB2(R8G8B8A8_UNORM, [0,1] 클램프)에 미리 구워지고, 백버퍼/환경맵 모두 LDR 8비트다. IBL specular는 view 의존이라 lighting pass로 옮겨야 하고, 다이내믹 레인지를 살리려면 HDR 누적 버퍼가 필요하다.

     사용자 확정 결정:
     1. HDR 풀 업그레이드(HDR scene-color 누적 + 단일 tonemap resolve).
     2. IBL 맵은 런타임 프리컴퓨트(로드 시 컴퓨트로 환경 큐브맵에서 생성).
     3. 글로벌 IBL만(스카이박스 1개; 로컬 반사 프로브는 후속).
     4. 환경 원본은 현재 LDR 8비트 DDS 큐브만 존재 → 설계는 포맷 무관(HDR HDRI 추후 드롭인), 내부 IBL 맵은 half-float 저장.
     5. Forward/Deferred 두 경로 모두 IBL 적용(Forward 경로가 런타임에 실제 사용됨: onlineGame.cpp:3366,3905).

     산출: 모든 lit 출력이 선형 HDR로 누적되고 단일 지점에서 톤매핑되며, 금속은 환경을 반사하고 유전체는 환경 irradiance를 받는다. 이후 bloom/노출/ACES/로컬 프로브 확장의 토대가 된다.

     ---
     설계 결정 (요약)

     - SceneColorHDR = per-room. 프레임 내 write→read 이고 렌더러가 triple-buffered(roomIdx = frameIdx_ % backBuffers_.size())이므로 GBuffer와 동일하게 룸별로 둔다. createColorRT(sharedResources.cpp:629)를 R16G16B16A16_FLOAT로 호출, addGBuffer 패턴(per-room 루프 + transitionToWrite/Read +
     curState + rtvHandle 캐시)을 그대로 미러한 신규 SharedResources::SceneColor 네임스페이스.
     - IBL 맵 = 정적, 1회 생성, room 비종속(단일 인스턴스). read-only·룸 무관·프레임 중 write 없음.
     | 맵                                                                                                                                                                 | 타입        | 포맷    | 해상도    | 밉                                |
     |--------------------------------------------------------------------------------------------------------------------------------------------------------------------|-------------|---------|-----------|-----------------------------------|
     | Irradiance                                                                                                                                                         | TextureCube | RGBA16F | 32²       | 1                                 |
     | Prefiltered specular                                                                                                                                               | TextureCube | RGBA16F | 128² base | 5 (→8²), prefilteredMipCount 전달 |
     | BRDF LUT                                                                                                                                                           | Texture2D   | R16G16F | 256²      | 1, 기하항 k=roughness²/2          |
     | 생명주기: 스카이박스 큐브 상주 직후 precomputeIBL(skyboxCubeSrvIdx) 1회. 환경 교체 시 재호출 가능한 진입점. (포맷은 RGBA16F 확정 — R16F는 RGB 라디언스 저장 불가.) |             |         |           |                                   |

     - Forward/Deferred 코드 공유. pbrLighting.hlsli에 단일 computeIBL(N,V,albedo,roughness,metallic,ao) 추가. split-sum: diffuse = irradiance(N)*albedo*(1-metallic), specular = prefiltered.SampleLevel(R, roughness*maxMip)*(F0*brdf.x+brdf.y), ×ao ×iblIntensity. 세
     곳(illuminate/illuminateCSM/illuminateFromGBuffer+terrain)에서 상수 ambient를 이 함수로 교체. SRV 3개는 각 경로 PerFrameData로 전달.
     - LDR 역톤매핑 토글. 현재 환경맵이 LDR이라 컴퓨트 셰이더 내 클램프 inverse-Reinhard(x/(1-x))를 envIsLDR 플래그로 옵션 적용(근사). HDR HDRI 확보 시 envIsLDR=0으로 스킵 → 포맷 무관 드롭인.
     - Tonemap resolve = 전용 파이프라인. 신규 TonemapPipeline/TonemapResolveShader — fullscreen triangle(DrawInstanced(3,1,0,0), deferred lighting과 동일 패턴), SceneColorHDR SRV→백버퍼. 단일 톤매핑 지점(bloom/노출/ACES 확장 자리). Phase 0에서는 기존 곡선(c/(c+1) → pow(1/2.2))을 정확히
     복제.

     ---
     단계별 구현

     Phase 0 — HDR 배관 ("룩 동일" 게이트)

     모든 불투명 lit 출력을 SceneColorHDR(선형)로 보내고, 단일 resolve로 톤매핑, 투명/파티클/UI는 resolve 이후 LDR 백버퍼에 그대로.
     1. sharedResources.hpp/.cpp: 신규 SceneColor 네임스페이스(createColorRT를 RGBA16F로, per-room, transitionToWrite/Read, resize용 erase). initSharedResources(gfx.cpp addGBuffer 직후)에서 생성, rtvPool_+srvTexPool_ 사용.
     2. shader.hpp/.cpp + 신규 tonemapResolve.hlsl: TonemapResolveShader + PSO(GFX::init). PS는 현재 곡선 그대로 복제.
     3. gfx.cpp render(): deferred lighting OMSetRenderTargets(~2224)와 forward dispatcher RTV(PBR~1679 / PBRSkinned~1692 / Skybox~1712 / Terrain~1874)를 SceneColorHDR로 리타깃(deferred 시 skybox 포함). 깊이는 기존 DSV 공유. deferred lighting과 forward-always 사이(~2275)에 resolve 패스
     삽입(SceneColorHDR→PIXEL_SHADER_RESOURCE 전이→백버퍼 RTV→SRV 바인드→DrawInstanced(3,1,0,0)). 프레임당 SceneColorHDR clear + RT↔SRV 전이 추가.
     4. 인라인 톤매핑 제거 → 선형 출력: pbrLighting.hlsli(illuminate ~452-454, illuminateCSM ~539-540), pbrDeferredLighting.hlsl:151-153, terrain.hlsl:202-203.
     5. 게이트: 동일 씬/카메라 before↔after 시각적으로 동일(곡선 보존). Forward·Deferred 둘 다, 투명/UI 무영향 확인.

     Phase 1a — IBL 프리컴퓨트

     1. 신규 컴퓨트 셰이더(한국어 주석 금지): iblIrradiance.hlsl(cosine convolution, RWTexture2DArray 6면, D3D 큐브 basis·GL Y-flip 금지, envIsLDR 토글), iblPrefilter.hlsl(GGX importance sampling, roughness=mip/maxMip), iblBRDFLUT.hlsl(split-sum, k=roughness²/2). 모두 DefaultRootSignature
     공유.
     2. 컴퓨트 PSO: createHiZ*Shader(shader.cpp:~2948) 패턴.
     3. 큐브+밉 생성 경로 신규(밉별 UAV = D3D12_UAV_DIMENSION_TEXTURE2DARRAY, FirstArraySlice 0/ArraySize 6; SRV=TEXTURECUBE). HiZMap 밉별 UAV 패턴(sharedResources.cpp:1023) 응용. createTextureWithMips는 Tex2D 전용이라 큐브엔 미사용. SharedResources::IBL에 텍스처 3개+SRV 인덱스 3개+mipCount
     저장(uavPool_/srvTexCubePool_/srvTexPool_).
     4. 신규 iblPrecomputePipeline.cpp/.hpp: precomputeIBL(skyboxCubeSrvIdx) — hiZPassCompute(pbrDeferredSkinnedPipeline.cpp:283-421) idiom(SetComputeRootSignature→SetDescriptorHeaps→맵별 SetPipelineState/bind/Dispatch/uavBarrier→최종 UAV→PIXEL_SHADER_RESOURCE 전이→Close/Execute/fence). 로드
     타임 LoadFence 사용(프레임 슬레이브 펜스 아님).
     5. 트리거: 스카이박스 큐브 상주 이후 — loadRequestedAssets의 skybox 로드 루프(~1277) 직후. initSharedResources는 금지(스카이박스 미로드).
     6. 게이트: 각 맵 디버그 뷰 확인 + 알려진 스카이박스 방향으로 큐브 면 방향 검증.

     Phase 1b — IBL 셰이딩 통합

     1. shader.hpp: PBRShader::PerFrameData(188), PBRSkinnedShader::PerFrameData(258), PBRDeferredLightingShader::PerFrameData(908), terrain per-frame 구조체 끝에 idxIrradiance/idxPrefiltered/idxBRDFLUT(BindlessIndex=int4) + prefilteredMipCount(uint) + iblIntensity(float) + 패딩 추가. 각
     HLSL cbuffer를 명시적 16B 패킹으로 동기화.
     2. pbrLighting.hlsli: computeIBL + fresnelSchlickRoughness 추가, illuminate/illuminateCSM의 ambient 교체. illuminateFromGBuffer 호출부(pbrDeferredLighting.hlsl)가 IBL 가산.
     3. pbrDeferred.hlsl:147: GB2.rgb를 emissive 전용으로 변경(ambient 선계산 제거). pbrDeferredLighting.hlsl이 GB2.rgb emissive를 그대로 가산.
     4. terrain.hlsl: computeIBL 추가, 선형 출력.
     5. gfx.cpp: 매 프레임 SharedResources::IBL에서 IBL cbuffer 필드 채움(deferred PerFrameData 빌드 ~2183; forward PBR/Skinned/Terrain 빌드 지점).
     6. GB2 의미 주석(sharedResources.hpp:120) 갱신.
     7. 게이트: 금속이 환경 반사, 유전체가 irradiance 수신, roughness 스윕이 올바르게 흐려짐, iblIntensity=0 ≈ Phase-0 룩, 두 경로 모두 정상.

     Phase 2 — 검증/디버그/튜닝 (+선택 폴리시)

     - 디버그 모드 확장(deferred debugMode shader.hpp:929 + forward 등가: irradiance-only / specular-only / BRDF / GB2-emissive).
     - iblIntensity·prefilter 샘플수·LDR 역톤매핑 강도 튜닝.
     - 전 재질·지형·스킨드·두 경로 회귀. resize 경로(SceneColor 재생성, IBL 맵은 정적 유지) 확인.
     - 선택(사용자 추가 결정 시): Reinhard→ACES 톤매퍼 교체, bloom 추가 — resolve 패스가 유일 확장 지점.
     - docs/iblArchitecture.md 완성, CODE_INDEX.md 갱신, 메모리 기록.

     ---
     핵심 재사용 자산

     ┌───────────────────────┬──────────────────────────────────────────────────┬───────────────────────────────────────────────┐
     │         용도          │                   재사용 대상                    │                     경로                      │
     ├───────────────────────┼──────────────────────────────────────────────────┼───────────────────────────────────────────────┤
     │ HDR RT 생성           │ createColorRT / addGBuffer(per-room, transition) │ client/sharedResources.cpp:629, :731          │
     ├───────────────────────┼──────────────────────────────────────────────────┼───────────────────────────────────────────────┤
     │ 밉별 텍스처 UAV       │ HiZMap MipSlice 루프                             │ client/sharedResources.cpp:1023               │
     ├───────────────────────┼──────────────────────────────────────────────────┼───────────────────────────────────────────────┤
     │ SRV/UAV 등록          │ createSRV / createUAV                            │ client/gfxUtil.cpp:1102, :1127                │
     ├───────────────────────┼──────────────────────────────────────────────────┼───────────────────────────────────────────────┤
     │ 컴퓨트 PSO            │ createHiZ*Shader                                 │ client/shader.cpp:~2948                       │
     ├───────────────────────┼──────────────────────────────────────────────────┼───────────────────────────────────────────────┤
     │ 컴퓨트 디스패치 idiom │ hiZPassCompute                                   │ client/pbrDeferredSkinnedPipeline.cpp:283-421 │
     ├───────────────────────┼──────────────────────────────────────────────────┼───────────────────────────────────────────────┤
     │ 큐브 bindless 샘플    │ sampleLevelBindlessCube                          │ client/bindless.hlsli:61                      │
     ├───────────────────────┼──────────────────────────────────────────────────┼───────────────────────────────────────────────┤
     │ 로드타임 1회 GPU 작업 │ initSharedResources LoadFence 패턴               │ client/gfx.cpp:1145-1240                      │
     ├───────────────────────┼──────────────────────────────────────────────────┼───────────────────────────────────────────────┤
     │ fullscreen triangle   │ deferred lighting VS/PS                          │ client/pbrDeferredLighting.hlsl:66            │
     ├───────────────────────┼──────────────────────────────────────────────────┼───────────────────────────────────────────────┤
     │ 디스크립터 풀         │ uavPool_/srvTexCubePool_/srvTexPool_             │ client/gfx.cpp:199-234                        │
     └───────────────────────┴──────────────────────────────────────────────────┴───────────────────────────────────────────────┘

     ---
     위험 / 주의

     - 톤매핑 제거는 전역 회귀 핫스팟: illuminateCSM이 forward pbr+skinned 모두를 구동 → 모든 forward-lit 메시가 동시에 변함. resolve PS가 기존 곡선을 정확히 재현해야 함. Phase 0 "룩 동일" 게이트를 IBL 착수 전 엄수.
     - GB2 의미 변경(ambient→emissive): GB2.rgb를 lit-accum으로 읽는 모든 소비자(deferred 재구성, terrain-deferred, 디버그 뷰) 감사. GB2.a(roughness)는 불변.
     - deferred lighting cbuffer register: shader.hpp:907 주석은 b0이나 실제 HLSL은 b1(스테일 주석). 필드 append는 register 불변이라 논블로커지만 편집 전 실 binding 확인.
     - 큐브 UAV 배리어: 6면 write 후 단일 UAV→SRV 전이. 현재 prefilter는 env 큐브를 샘플(하위 밉 아님)하므로 밉 간 read 의존 없음.
     - MT render() + 신규 패스: resolve는 룸별 렌더 cmdList/펜스에서 실행(lighting write→resolve read 전이가 동기점). IBL 프리컴퓨트는 로드타임(LoadFence) 전용 — 프레임 슬레이브 펜스 금지.
     - PerFrameData 16B 패킹: int4+uint+float 레지스터 경계 주의. 명시적 패딩, shader.hpp↔HLSL 바이트 동일 유지.
     - 신규 파일 한국어 주석 금지(MSVC cp949 빌드오류; 기존 파일 편집은 무방). using alias 금지·각 namespace 직접 작성. mathUtil 규약(vM, AB*C). D3D 큐브 basis·NDC depth[0,1].

     ---
     에이전트 팀 구성 (실행)

     shared-file 충돌 핫스팟(gfx.cpp render(), shader.hpp, pbrLighting.hlsli)은 단일 소유자로 직렬화하고, 디스조인트 영역은 병렬화한다. 승인 후 TeamCreate(또는 워크트리별 Agent)로 인스턴스화하며, Lead가 체크포인트마다 통합 머지.

     ┌────────────────────────────────┬─────────────────────────────────────────────────────────────────────────────────────────────────────────────────────┬────────────────────────────────────────────────────────────────────────────────────────────────────────────────────┬─────────────────┐
     │              역할              │                                                     워크스트림                                                      │                                                   소유 파일/영역                                                   │      단계       │
     ├────────────────────────────────┼─────────────────────────────────────────────────────────────────────────────────────────────────────────────────────┼────────────────────────────────────────────────────────────────────────────────────────────────────────────────────┼─────────────────┤
     │ Lead (나) —                    │ render() 리타깃·resolve 삽입·트리거 배선·매프레임 IBL cbuffer 업로드·머지·게이트 판정                               │ gfx.cpp(render/initSharedResources/loadRequestedAssets 배선)                                                       │ 0,1a,1b         │
     │ 통합·오케스트레이션            │                                                                                                                     │                                                                                                                    │                 │
     ├────────────────────────────────┼─────────────────────────────────────────────────────────────────────────────────────────────────────────────────────┼────────────────────────────────────────────────────────────────────────────────────────────────────────────────────┼─────────────────┤
     │ T1 — HDR 타깃·Tonemap          │ SceneColorHDR per-room + resolve PSO/패스                                                                           │ sharedResources.*(SceneColor 영역만), tonemapResolve.hlsl, shader.*(TonemapResolveShader+TonemapPipeline만)        │ 0               │
     ├────────────────────────────────┼─────────────────────────────────────────────────────────────────────────────────────────────────────────────────────┼────────────────────────────────────────────────────────────────────────────────────────────────────────────────────┼─────────────────┤
     │ T2 — 셰이딩 수학·톤매핑 제거   │ 인라인 톤매핑 제거→선형, computeIBL 추가·통합, GB2 emissive                                                         │ pbrLighting.hlsli(단독 소유), terrain.hlsl, pbrDeferredLighting.hlsl, pbrDeferred.hlsl                             │ 0(제거),1b(IBL) │
     ├────────────────────────────────┼─────────────────────────────────────────────────────────────────────────────────────────────────────────────────────┼────────────────────────────────────────────────────────────────────────────────────────────────────────────────────┼─────────────────┤
     │ T3 — IBL 프리컴퓨트            │ 컴퓨트 셰이더 3종 + 큐브/LUT 할당 + 디스패처                                                                        │ iblIrradiance/iblPrefilter/iblBRDFLUT.hlsl, iblPrecomputePipeline.*, shader.cpp(IBL PSO), sharedResources.*(IBL    │ 1a              │
     │                                │                                                                                                                     │ 영역만)                                                                                                            │                 │
     ├────────────────────────────────┼─────────────────────────────────────────────────────────────────────────────────────────────────────────────────────┼────────────────────────────────────────────────────────────────────────────────────────────────────────────────────┼─────────────────┤
     │ T4 — 구조체·디버그·검증·문서   │ 4개 PerFrameData append(직렬화 지점·단독 소유), 디버그 뷰, CODE_INDEX.md+docs/iblArchitecture.md,                   │ shader.hpp(struct 영역), 문서, 디버그 뷰(조율)                                                                     │ 1b,2            │
     │                                │ 빌드·실행·스크린샷 회귀                                                                                             │                                                                                                                    │                 │
     └────────────────────────────────┴─────────────────────────────────────────────────────────────────────────────────────────────────────────────────────┴────────────────────────────────────────────────────────────────────────────────────────────────────────────────────┴─────────────────┘

     실행 순서 / 체크포인트
     1. CP0(Phase 0): T1(HDR타깃+resolve) ∥ T2(톤매핑 제거) → Lead가 render() 배선 → T4가 "룩 동일" 검증.
     2. CP1a: T3(맵+디스패처) → Lead 트리거 배선 → T4 맵 디버그 검증.
     3. CP1b: T4 struct append(직렬점) → T2 computeIBL+GB2 → Lead 매프레임 업로드 배선 → T4 IBL 정확도 검증.
     4. CP2: T4 회귀·문서; (선택) ACES/bloom.

     충돌 관리: sharedResources.*/shader.*는 다중 소유자 → 네임스페이스/영역 디스조인트 편집 + Lead가 체크포인트에서 직렬 머지(또는 에이전트별 worktree 후 Lead 통합).

     ---
     검증 방법

     - 빌드: VS2022 GameServer.sln로 client 빌드. 단독 .vcxproj 빌드 시 /p:SolutionDir 필수(누락 시 sepch.hpp 미발견; 메모리 reference_build_command 참조).
     - 실행: client 실행(standalone 또는 online). /run 또는 /verify 스킬로 구동·스크린샷.
     - Phase 0 게이트: 동일 씬·카메라 before↔after 스크린샷 A/B → 시각적 동일(곡선 보존). Forward·Deferred 토글 둘 다, 투명/파티클/UI 무영향.
     - Phase 1a 게이트: IBL 디버그 뷰로 irradiance/prefiltered(밉별)/BRDF LUT 시각 확인, 큐브 면 방향 정합.
     - Phase 1b 게이트: 금속 구가 환경 반사·유전체가 irradiance 수신·roughness 스윕이 흐려짐·iblIntensity=0≈Phase0·두 경로 정상. 'G' 디버그 사이클에 IBL 채널 추가로 항별 분리 확인.
     - 회귀: 전 재질/스킨드/지형/스카이박스, resize(창 크기 변경) 시 SceneColor 재생성·IBL 정적 유지.
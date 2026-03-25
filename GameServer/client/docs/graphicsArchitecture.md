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

#### TerrainPipeline 특성

- 파일: `terrain.hpp/cpp`, `terrain.hlsl`, `terrainPipeline.hpp/cpp`, `terrainShadowMap.hlsl`
- **shadow pass 있음** — 지형 기하가 공유 shadow map("ShadowMap" DSV)에 기록되어 PBR 객체 위에 지형 그림자를 드리움
- shadow pass: `shadowPass()` / `shadowPassMT()` (MT는 단일 스레드 위임, draw event 수가 적어 MT 효과 없음)
- shadow shader: `terrainShadowMap.hlsl` — position-only VS, PS 없음, NumRenderTargets=0
- 리소스 로드: `loadTerrainFromFiles()` — manifest 파싱 → height.raw 메시 빌드 → 텍스처 로드
- **manifest 태그 순서**: `HeightMap → SplatPath(s) → DiffusePath(s) → MetaData` (MetaData가 마지막)
- VB 3슬롯: Position(0) / Normal(1) / UV(2), IB 32-bit (513×513 정점 초과 가능)
- Splat map: RGBA 채널 = 레이어 0~3 블렌딩 가중치, 각 레이어마다 diffuse + normal map
- `terrain.hlsl`에서 `pbrLighting.hlsli` include 시 `#define TERRAIN_SHADER` 필수 — `illuminate()` 스킵 가드
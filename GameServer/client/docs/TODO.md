- [X] 모델 메시별로 셰이더 별도 적용
- [X] 공격을 위한 충돌처리 구현 (AABB 기반, CombatSystem 서브시스템)
- [X] 몬스터 단순 AI 구현 (쿨타임 기반 AABB 교차 공격)
- [X] 공격에 해당하는 바운딩 볼륨 렌더링 가능하도록 구현
- [X] OBB 충돌처리 지원, 캐릭터 오브젝트들에 대해 기존 AABB 전부 OBB로 교체 (AABB는 특정 단순 사물에만 사용할 예정)
- [X] 유니티에서 추출한 바이너리 리소스를 로드해 Bounding Volume Hierarchy 구축 및 그를 통한 충돌처리로 업그레이드
  - 유니티에서 어떻게 추출했는지는 `unityScripts/ExtractUtil.cs`, `unityScripts/ModelExtractor.cs`, `unityScripts/MultiBoundingVolume.cs` 참조
  - BVH 노드가 bone에 종속된 경우 `bone.toDress * finalXformData()[i] * world` 체인으로 월드 변환
- [X] Height map 기반 Terrain 구현/Terrain Splat까지 (Unity에서 맵 추출)
  - TerrainExtractor.cs로 추출된 height.raw + terrain_meta.bin + terrain_manifest.bin + DDS 텍스처 로드
  - N×N 그리드 메시 생성 (중앙차분 법선, 32-bit IB), RGBA splat map 기반 레이어 블렌딩
  - terrain.hlsl: Lambertian + globalAmbient + PCF shadow, terrainPipeline.hpp/cpp: Dispatcher
- [X] level 바이너리에서 Terrain WorldTRS 읽어 월드 변환 적용
  - TerrainObject(Object 상속)와 TerrainData 분리: Object/Model 패턴과 동일
  - importNode() "Terrain" 분기 → TerrainObject 생성 → importTerrain() → update(0ms, 1.f)
  - TerrainPipeline::DrawEvent에 world 필드 추가, mainPass()에서 ev.world로 WVP 계산
- [X] Terrain Shadow 구현 (지형이 PBR 객체 위에 그림자를 드리움, PBR 객체의 그림자가 지형 위에 드리움)
  - terrainShadowMap.hlsl + TerrainShadowMapShader PSO 추가 (position-only, depth-only, NumRenderTargets=0)
  - TerrainPipeline::Dispatcher에 shadowPass/shadowPassMT/shadowUpdate/shadowDraw 추가
  - 공유 shadow map("ShadowMap") DSV에 지형 기하를 기록 → PBR mainPass에서 샘플링
- [X] Terrain roughness metallic도 unity에서 추출 및 렌더링 시 반영하도록 수정
  - 현재는 셰이더에 하드코딩되어 있음.
- [X] Cascaded Shadow Mapping 구현
- [X] Rigid Body Physics 구현: 중력, 공기 저항, 마찰력 등 반영
- [X] Active Ragdoll 기반 구현
- [X] 시분할 애니메이션 제대로 적용
- [X] GPU Hi-Z Occlusion Culling 구현 (PBRDeferredSkinnedPipeline): GPU 5단계 compute (Clear→Cull→PrefixSum→Compact→Command), visibleFlags readback → Object::update/AnimBlender 스킵, GBuffer PID 계산 스킵
- [ ] Software Occlusion(Culling)을 통한 추가 최적화 (정적 메시, 지형 등 나머지 파이프라인 확장)
- [X] Deferred Shading을 위한 GBuffer 설계
- [X] Deferred Shading 구현
- [ ] 청크 구현 및 리소스 멀티스레드 동적 로딩 구현 (Seamless Openworld가 가능하도록)
- [ ] Image Based Lighting 구현



// UI(hp, inventory, login, loading), Effect, goal-based AI, clustered AI: 5월 초 게임 시작->집단 전투 컨텐츠 완성

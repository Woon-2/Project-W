## 기록

2024-09-07

- 꽤 오래 기록을 쉬어서, 너무 많은 것들을 했기 때문에 다 기록이 불가

- 그래픽스 프레임워크를 많이 개발해놓았으나, 아직 실제로 화면에 나타나는 게 없어서 불안감이 있음
  - `assimp`를 활용한 계층구조 모델 로딩
  - 특정 좌표계의 자식으로 다른 좌표계를 넣을 수 있는 좌표계 트리 개념 구현
    - 특정 좌표계의 점이나 벡터를 다른 좌표계로 표현할 수 있는 유틸리티 제공
  - 동시 입력, 순차 입력을 감지하여 키보드가 커맨드 입력을 받을 수 있도록 구현
  - GPU Virtual Memory Pool 구현, 하나의 거대한 `ID3D12Resource*`로부터    
  다수의 `D3D12_GPU_VIRTUAL_ADDRESS`를 생성하여 사용할 수 있도록 구현
    - 향후 메모리 블록과 할당된 `D3D12_GPU_VIRTUAL_ADDRESS`가 일대일로 매핑되는 것이 아닌,     
    할당할 수 있는 최대로 할당하고 레퍼런스 카운트를 유지하도록 변경 필요
  - Offscreen Render Target에 장면을 그린 뒤 Swapchain Backbuffer로 복사하도록 하고,    
  GPU로 올라갈 리소스들은 Resource Duplication을 통하여 향후 구현할 Frame Pipelining에 대비
  - Bindless Resource 기법 적용, 리소스 업로드 빈도 최적화
  - `DirectXTex`, `DirectX-Headers` 라이브러리의 필요한 부분을 빌드하여 그래픽스 프레임워크에 링크
  - raw mouse input을 다루어 마우스 커서 위치와 독립된 마우스 이동 감지 구현
  - graphics API와 독립적인 정보 전달 프로토콜을 가지는 `Renderer`와 `Scene` 모델 설계
- 수학 라이브러리 `MathUtil` 개발
  - `glm` 문법과 유사하고, 내부 구현은 `DirectXMath`
  - `Radian`과 `Degree`, `NVec<D>`과 `Vec<D>` 등 타입 캐스팅으로 간편하게 단위 변환이나 정규화를 표현할 수 있도록 유틸리티 제공
  - 향후 expression template 기법을 적용할지 검토 필요

- 이전에 간단하게 만든 채팅 서버 완성: `select` 이용하는 TCP 서버

## 노트

`doxygen`으로 문서화 한번 쫙 했었는데,     
변경이 된 것도 많고 추가된 게 너무 많아서 또 문서화에 많은 시간을 써야 할 것 같다.    

라이브러리가 거대해져가고 있기 때문에, 단순히 클래스 함수 설명이 아니라     
Getting Started라고 이름 지을 법한 가이드라인까지 구비해야 할 듯 싶다.

PBR은 원래 책이랑 웹 자료랑 찜해놓은 것들 있는데 그거 다 보고 만드려 했더니,     
구할 수 있는 Material들이 죄다 PBR Material이다.     
albedo 맵 하나로 Diffuse 돌리고 나머지는 다 상수로 쓰긴 좀 그러니까,     
Phong Shader를 만들긴 했는데 못 쓰고 바로 PBR 구현으로 넘어가야 한다.      
이쪽은 임시 코드로 가득차게 될 듯 싶다.

## 로드맵

- [X] MathUtil 라이브러리 구현
- [X] assimp 링크
- [X] 계층구조 모델 로드
- [X] 키보드 커맨드 입력 감지, Raw Mouse Input 처리
- [X] Render Protocol 설계
- [X] GpuMemoryPool 구현
- [X] Offscreen Render Target과 Duplicated Resources 만들기 (후에 Frame Pipelining을 위함)
- [X] PhongShader 클래스 만들기
- [X] DirectX Tex 라이브러리 import 하기
- [X] DirectX Headers 라이브러리 import 하기
- [X] Texture 클래스 만들기
- [ ] PBR 셰이더 만들기
- [ ] D3D12RenderContext에 cmdList 게터 만들기
- [ ] Frame Pipelining 구현하기
- [ ] RigidBody 컴포넌트 만들기 + PlayerController가 RigidBody의 속도 멤버를 조작하도록
- [ ] Particle
- [ ] Skybox
- [ ] 태양의 위치에 따른 산란들
- [ ] BVH
- [ ] SW Occlusion
- [ ] 리소스 로딩 멀티스레딩 + 코루틴
- [ ] ResourceBarrier 한꺼번에 Context에 기록
- [ ] Terrain
- [ ] Texture Mip Mapping
- [ ] Shadow mapping
- [ ] LOD 기반 시뮬레이션
- [ ] Tesselation
- [ ] 3D Sound
- [ ] Defered Shading
- [ ] SSAO, HBAO+
- [ ] STL 컨테이너들 pmr 적용
- [ ] GpuMemoryPool 블록 공유 구현
- [ ] Area light (Importance Sampling, Monte carlo Integration)
- [ ] Image Based Lighting
- [ ] depth peeling & Stochastic Transparency (OIT)
- [ ] GPU RigidBody Physics
- [ ] PBD (별도 샘플 프로그램 실험)
- [ ] Path Tracing (별도 샘플 프로그램 실험)
- [ ] vfx
- [ ] DLSS 구현
- [ ] 애니메이션
- [ ] 모션 블러
- [ ] NVidia GPU Gems 효과들
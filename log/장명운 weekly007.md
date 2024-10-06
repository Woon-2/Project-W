## 기록

2024-10-06

- Render Protocol Family 레이어 추가
  - `concept`으로 구현, 프로토콜이 서로 달라도 같은 프로토콜 패밀리라면 동일한 드로잉 함수 템플릿에 넣었을 때 잘 동작함이 보장됨.
- Renderer, Scene 디자인 변경
  - Renderer는 (프로토콜, 셰이더 포인터, 셰이더 옵션) 꼴로 프로토콜과 셰이더를 매핑해서 관리
  - Scene은 {프로토콜: (조명 포인터, 인스턴스 집합)} 꼴로 프로토콜과 (조명, 인스턴스 집합)을 매핑해서 관리
  - 프로토콜을 기준으로 렌더링 작업을 1차적으로 세분화한 뒤, (메시, 재질) 기준으로 렌더링 작업을 2차 세분화
    - 동일한 프로토콜, 동일한 메시, 동일한 재질을 사용한다면 인스턴싱을 통해 동일한 draw call에서 그려짐.
    - 동일한 프로토콜을 사용하면 반드시 동일한 셰이더와 셰이더 옵션을 사용함.
  - Phong Renderer, PBR Renderer와 같이 BRDF에 따라 다른 클래스로 Renderer를 디자인하는 것이 아닌,    
  다중 프로토콜과 다중 셰이더를 지원하는 일반적인 Renderer 클래스가 있고,     
  Illuminance Renderer, Shadow Renderer 등으로 렌더링 방법이 추상화된 클래스로 해당 클래스를 특수화하여 사용하도록 함.
- Input Layout, Mesh 사양 변경
  - 다중 슬롯을 사용할 수 있도록 해 VertexBuffer의 구성을 좀 더 유연하게 만듬.
  - Vertex Property들의 포함 여부를 담는 플래그를 슬롯별로 구성
    - 다중 Vertex Buffer와 연관된 모든 코드에서 해당 플래그를 활용해    
    통일된 문법으로 Vertex Buffer Layout을 알 수 있도록 함.
- 모델 로딩의 유연성 증대
  - 기존의, 특정 Input Layout을 만족하도록 모델을 로드하는 방식으로는    
    모델의 메시마다 Vertex Property 구성이 다른 경우를 처리하지 못함.
  - Input Layout 대신 플래그들을 활용해, 플래그마다 해당 플래그를 만족시킬 수 있는지 점검하고     
    만족할 수 있다면 그 플래그가 의미하는 바대로 로드하도록 함.
      - Scene을 구성할 때 {메시 포인터, 셰이더 포인터, 프로토콜}로 구성되는 Mesh View를 원소로 추가하도록 변경,     
      메시가 어떤 셰이더의 input layout을 만족하는지 점검하며 올바른 셰이더와 함께 Scene에 넣을 수 있음.
        - Renderer와 Scene이 다중 셰이더, 다중 프로토콜을 지원할 수 있게 되었기 때문
- block compression 적용 텍스처 생성
  - DirectxTex 라이브러리의 texconv를 활용해 block compression이 적용된 `.dds` 포맷의 텍스처로 변환
  - 입력 폴더의 모든 텍스처들에 대해 texconv 명령어를 수행해    
  결과로 만들어진 `.dds` 파일들을 출력 파일에 저장하는  배치 파일 작성

## 노트

추상화는 멀고도 험한 길,     
그러나 이전의 추상화에 비해 깔끔함도 성능도 늘었다.     
Debug 모드 + Visual Studio 로컬 디버거 조합으로 실행시키면 FPS가 1~3이 나와버리는 괴물 코드를     
동일한 조합으로 실행했을 때 144 FPS까지 잘 나오도록 바꾸었다.     
일반성을 위한 불필요한 자료구조들도 줄이면서 1.2GB까지도 잡아먹던 런타임 메모리가 400MB로 줄었다.     
정확히 Entity 메모리 풀 + 텍스처 사이즈로 잘 나왔다.    
클라이언트 모델 클래스는 메시 트리와 재질 트리를 별도로 가지는 대신 주어진 두 트리를 통합하는 알고리즘을 만들고,     
Fragmentizer에서 모델 트리 대신 (메시 뷰, 재질 포인터, 부모)의 노드들을 저장하도록 하면서     
이상한 트리들을 줄이고 트리 순회의 횟수도 줄였는데, 이게 효과적이었다.

한편, 프로그램의 거의 핵심이 되는 클래스들의 디자인이 바뀐 거라      
그 여파를 다 반영하는데 시간이 꽤 걸렸기 때문에,     
얼른 그림자 등을 구현해 적용해야 할 것이다.     

block compression을 적용해 텍스처도 생성하고,    
클라이언트 코드에서 해당 텍스처가 잘 로드되는 것까지 확인했지만,     
`.fbx` 파일에 박혀있는 재질 경로를 수정할 방법이 없어 해당 텍스처를 모델과 엮어 사용할 수가 없다...!     
간단한 후처리 로직을 assimp loader에 넣어줄 수도 있겠지만,     
여러모로 그냥 익스포터를 만드는 게 나을 것 같다.     
문제는 애니메이션까지 다 해보고 익스포터를 만드느냐, 익스포터를 먼저 만드느냐인데,     
익스포터의 효과가 어느정도인지 관찰하기 위해서는 일단 애니메이션까지 다 해보고 만들어봐야 할 것 같긴 하다.

## 로드맵

- [X] MathUtil 라이브러리 구현
- [X] 프레임락 및 렌더링 생략을 통한 프레임 안정화
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
- [X] Scene과 Renderer 완벽히 RenderProtocol 이용하여 정보 전달
- [X] Frame Pipelining 구현하기
- [X] RigidBody 컴포넌트 만들기 + PlayerController가 RigidBody의 속도 멤버를 조작하도록
- [X] 네트워크 라이브러리 링크 + 간단한 패킷 설계
- [X] Material 클래스 구현 및 Material 트리 구축
- [X] Model과 Coord 컴포넌트 구현
- [X] Asset System 구현, 게임 로직에서 에셋 로딩 로직을 독립시킴: 2024. 09. 20.
- [X] Select 기반 네트워크 버그 수정: 2024. 09. 25. 
- [X] D3D12RenderContext에 cmdList 게터 만들기
- [X] 블록 압축 픽셀 포맷 사용: 2024. 10. 02.
- [X] RenderProtocol을 RenderProtocolFamily와 RenderProtocol로 분리, 일부 모델의 메시들이 동일한 Input Layout을 사용하지 않는 점을 해결
- [X] 셰이더 클래스들 업데이트된 RenderProtocol들로 최신화
- [X] 다중 슬롯 Input Layout 지원
- [X] Scene과 Renderer가 다중 프로토콜 및 셰이더를 지원하도록 변경: 2024.10.06.
- [ ] Shadow mapping(Illuminance Renderer, Shadow Renderer 디자인): 2024.10.09.
- [ ] 계층 구조 모델 월드 변환 캐싱
- [ ] 텍스처 기반 지형 메시 추가
- [ ] Skybox
- [ ] UI Shader 구현
- [ ] 빌보드 나무 구현
- [ ] Texture Mip Mapping:
- [ ] 안개 구현: 2024. 10. 16.
- [ ] PBR 셰이더 만들기
- [ ] 문서화 및 폴더, vcxproj 필터 정리
- [ ] 태양의 위치에 따른 산란들
- [ ] BVH
- [ ] SW Occlusion: 2024. 10. 23.
- [ ] 단일 애니메이션 구현
- [ ] CPU Particle System
- [ ] 클라이언트에 동적 조명 추가, Debug Draw 가능하도록: 2024.10.30.
- [ ] 윈도우 띄우기 전에 리소스 로딩하기 + 로딩창 보여주기
- [ ] 레벨 디자인 방법 모색
- [ ] 월드 변환 최적화 (SRT Batching?): 2024.11.06.
- [ ] GPU Particle System (Compute Shader)
- [ ] LOD 디자인
- [ ] Tesselation
- [ ] Defered Shading
- [ ] 3D Sound
- [ ] Asset 리소스 패킹
- [ ] Assimp 파일 말고 메모리로부터 읽도록
- [ ] 리소스 로딩 멀티스레딩 + 코루틴
- [ ] ResourceBarrier 한꺼번에 Context에 기록
- [ ] 리소스 재정렬: 텍스처 srv 힙에서 안 쓰는 텍스처 제외 srv 힙 재구축, 동적 모델 관리 등
- [ ] Terrain
- [ ] Asset들 데이터베이스화
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
- [ ] 모션 블러
- [ ] NVidia GPU Gems 효과들
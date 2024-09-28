## 기록

2024-09-28

- 셰이더 디자인 변경
  - 텍스처 셰이더에서 Material의 정의를 `텍스처 인덱스 집합의 인덱스`에서 `텍스처 인덱스 집합`으로 변경,    
    PID(Per Instance Data)에서 PDD(Per Drawcall Data)로 저장구조 변경
    - 이에 따라 HW Instancing은 이제 Mesh 단위가 아닌 (Mesh, Material) 단위로 이루어짐
  - 이에 따른 CPU와 GPU의 셰이더 리소스 메모리 레이아웃 변경, 씬과 렌더러의 정보 교환 로직 변경(PID->PDD 반영),    
    루트 시그너처 변경
- ecs 프레임워크 리팩터링
  - Entity, Component, System의 구현에 있어서 상속 활용
  - Entity와 Component pooling
  - 템플릿 메타 프로그래밍과 유틸리티 함수들 추가로 표현력 증대 및 사용 코드 간결화
  - 클라이언트의 임시 렌더링 코드들 ecs를 활용하도록 변경
- Network 디자인
  - 객체 리플리케이션 구현(패킷 재조립을 고려하지 않아 불안정)

## 노트

리팩토링 후에도 저번 면담 때에 있던 네트워크 버그가 고쳐지지 않았는데,     
교수님과의 면담을 통해 패킷 재조립을 고려하지 않은 설계가 문제임을 알았다.     
패킷 재조립을 고려하도록 바꾸어야 한다.

3D게임프로그래밍2 수업에서 텍스처마다 signature 비트를 두어서,     
해당 비트가 활성화되어 있을 때에 셰이더에서 텍스처링을 수행하는 코드를 보았다.     
현재 우리 셰이더는 특정 조명모델을 위한 텍스처들이 종류별로 전부 갖추어져 있을 것을 상정하고 있는데,     
변화를 주는 것을 고려해보아야겠다.

드래곤 모델의 경우 변환 계층 구조는 매우 큰 트리인데, 메시는 3개밖에 존재하지 않는다.     
애니메이션을 상정해 본들이 들어가 있어서 계층 구조가 커진 것 같은데,     
현재 좌표계에서는 큰 계층구조에 대해서 부모-자식으로 내려오는 높이에 비례해서 월드변환 행렬 곱셈 연산량이 늘어나는데,     
변하지 않은 변환에 대해서는 이전의 결과를 재활용하는 것으로 최적화할 필요가 있다.

현재 assimp를 활용하고 있긴 하지만,     
바운딩 볼륨을 파일에 같이 저장한다든지 지원할 수 있는 render protocol을 기록한다든지     
우리 프로젝트에 맞게 정보들을 변환하여 저장하고 싶은 마음이 생겼다.    
assimp가 material을 로드할 때에 emmisive map, spuclar map 같은 것에 diffuse texture를 넣어버리는 부분이 특히 골치아프다.     
지원하지 않는다면 비워놓아야 그림이 그나마 괜찮게 그려지는데, 저건 대용으로 다른 텍스처를 넣어서 오히려 퀄리티를 떨어뜨리기 때문이다.     
이용희 교수님께서 Unity를 기반으로 모델을 export 하는 것을 알려주셨는데,    
C#을 모르기도 하고, 실행 한번으로 정해진 결과를 뚝딱 만드는 것보다     
GUI로 조금 더 다양한 설정들을 건드리면서 결과를 만들었으면 좋겠다.     
따라서 익스포터 프로젝트를 따로 만드는데, imgui를 붙여서 만들어봐야겠다.

또 매번 Visual Studio에서 새 인스턴스 시작으로 서버 1개, 클라이언트 10개 띄우긴 그러니,    
스크립트나 배치 파일을 만들어야 할 것 같다.     
편하게 여러 개 띄울 수 있으면 좋을 테니까!

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
- [ ] D3D12RenderContext에 cmdList 게터 만들기
- [ ] 계층 구조 모델 월드 변환 캐싱
- [ ] 블록 압축 픽셀 포맷 사용
- [ ] 텍스처 기반 지형 메시 추가
- [ ] 특정 렌더 프로토콜에서 몇몇 텍스처의 누락도 가능하도록 변경
- [ ] 모델 로드할 때 Input Layout 만족 못하는 메시 처리 및 디버그 기록 
- [ ] RenderProtocol을 RenderProtocolFamily와 RenderProtocol로 분리, 일부 모델의 메시들이 동일한 Input Layout을 사용하지 않는 점을 해결
- [ ] 셰이더 클래스들 업데이트된 RenderProtocol들로 최신화
- [ ] PBR 셰이더 만들기: 2024. 10. 02.
- [ ] 문서화 및 폴더, vcxproj 필터 정리
- [ ] Skybox
- [ ] 태양의 위치에 따른 산란들
- [ ] Shadow mapping
- [ ] UI Shader 구현
- [ ] 안개 구현: 2024. 10. 09.
- [ ] BVH
- [ ] SW Occlusion: 2024.10. 16.
- [ ] Texture Mip Mapping:
- [ ] 단일 애니메이션 구현: 2024. 10. 23.
- [ ] CPU Particle System
- [ ] 클라이언트에 동적 조명 추가, Debug Draw 가능하도록
- [ ] 윈도우 띄우기 전에 리소스 로딩하기 + 로딩창 보여주기
- [ ] 레벨 디자인 방법 모색: 2024. 10. 30.
- [ ] 월드 변환 최적화 (SRT Batching?)
- [ ] GPU Particle System (Compute Shader): 2024. 10. 30.
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
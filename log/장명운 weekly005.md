## 기록

2024-09-22

- `gfx::d3d12::Material` 클래스 사양 변경
  - 더 이상 텍스처 리소스를 소유하지 않고 외부의 텍스처 리소스를 참조하도록 (srv 힙에서의 인덱스만 저장)
  - `std::any`를 통해 필요한 속성들을 모두 담는 메모리를 사용자가 구조체나 클래스로 정의하여 전달하도록 하였으나,    
    assimp와 매칭이 잘 되도록 하고, 저장된 속성들로 더 다양한 렌더링 프로토콜을 지원할 수 있도록    
    속성들을 enum class를 통해 명시하고 컨테이너로 관리하도록 변경
- 일회성 업로드 버퍼를 코어 대신 사용처에서 관리
  - 코어에 업로드 버퍼를 쿼리하도록 해서 일회성 업로드 버퍼를 멤버로 들고있지 말자는 게 본의였는데,    
    결국 그 과정에서 쿼리에 필요한 인덱스를 또 한 번만 사용하는데도 불구하고 멤버로 저장해야 함.    
    따라서 쿼리의 오버헤드만 있는 기존 방법 대신 일회성 업로드 버퍼들도 멤버로 들고있도록 수정
- 재질 트리 구현
  - 동일한 모델에 서로 다른 재질들을 적용할 수 있도록    
  - 트리의 모양이 모델의 메시 트리와 같아지도록 구현

## 노트

3D게임프로그래밍1 수업 때 동일한 메시를 사용하는 객체들을 집합적으로 관리하여 인스턴싱을 했었는데,    
현재 클라이언트 코드는 메시 단위가 아니라 메시들의 트리인 모델 단위로 처리한다.    
한편, 인스턴싱은 메시 단위로만 가능하다.    
정점 버퍼와 인덱스 버퍼가 메시에 종속되어있기 때문이다.    
클라이언트에는 최대한 모델만 드러내면서 중간에 메시 인스턴스들을 집합적으로 관리하는 클래스를 설계할 필요가 있다.    
메시 인스턴스들을 모두 감싸는 바운딩 볼륨이 커지게 되면 그만큼 뷰 프러스텀 컬링으로 해당 바운딩 볼륨이 제외될 확률이 낮아지므로,    
동일한 메시를 사용한다고 해서 모두 한 집합에 넣는 것이 아니라, 근처에 있는 메시들끼리 묶을 필요가 있는데,    
그것을 클라이언트에 드러내지 않으면서 수행할 만한 로직이 필요하다.    
근처에 있다라는 것을 어떻게 프로그램적으로 정의할 것인가가 관건인 것 같다.    

재질 트리와 모델을 통해 어떠한 객체의 렌더링에 필요한 모든 리소스 구조가 갖춰졌으므로,    
사람 모양 모델을 띄워보도록 한다.    

상훈이가 IOCP를 공부하고 적용하는데 2주 안에는 힘들다고 하였으므로,    
현재 작성해 놓은 Select 기반 코드들로 한 번 더 검사를 맡아야 한다.    
이쪽에 버그가 굉장히 많으므로, 남은 시간 동안은 이 버그 수정이 주가 될 것 같다.

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
- [ ] D3D12RenderContext에 cmdList 게터 만들기
- [ ] Select 기반 네트워크 버그 수정: 2024. 09. 25. 
- [ ] 모델 로드할 때 Input Layout 만족 못하는 메시 처리 및 디버그 기록 
- [ ] RenderProtocol을 RenderProtocolFamily와 RenderProtocol로 분리, 일부 모델의 메시들이 동일한 Input Layout을 사용하지 않는 점을 해결
- [ ] 셰이더 클래스들 업데이트된 RenderProtocol들로 최신화
- [ ] PBR 셰이더 만들기: 2024. 10. 02.
- [ ] 문서화 및 폴더, vcxproj 필터 정리
- [ ] Skybox
- [ ] 태양의 위치에 따른 산란들
- [ ] Shadow mapping
- [ ] 안개 구현: 2024. 10. 09.
- [ ] BVH
- [ ] SW Occlusion
- [ ] Texture Mip Mapping: 2024. 10. 16.
- [ ] 애니메이션 구현: 2024. 10. 23.
- [ ] CPU Particle System
- [ ] GPU Particle System (Compute Shader): 2024. 10. 30.
- [ ] Asset 리소스 패킹
- [ ] Assimp 파일 말고 메모리로부터 읽도록
- [ ] 리소스 로딩 멀티스레딩 + 코루틴
- [ ] ResourceBarrier 한꺼번에 Context에 기록
- [ ] 리소스 재정렬: 텍스처 srv 힙에서 안 쓰는 텍스처 제외 srv 힙 재구축, 동적 모델 관리 등
- [ ] Terrain
- [ ] Asset들 데이터베이스화
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
- [ ] 모션 블러
- [ ] NVidia GPU Gems 효과들
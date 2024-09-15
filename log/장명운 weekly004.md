## 기록

2024-09-15

- 클라와 서버 연결
  - 현재는 서버 통신을 select 기반으로 구현, 후에 상훈이와 함께 iocp 기반으로 교체할 예정
  - 네트워크 id를 할당하는 Hello Packet, 월드 상태를 갱신하는 Update Packet, 연결 해제를 나타내는 Leave Packet 세 종류의 패킷을 디자인
- 텍스처를 입힌 메시 출력
- Frame Pipelining 구현 완료
  - 한 개의 명령 리스트가 실행 중인 동안 다른 명령 리스트를 기록해야 하므로 다중 명령 리스트가 필요해짐,
    향후 멀티스레딩이 필요할 것까지 대비해 명령 리스트 풀을 구현
  - fence의 개수를 클라이언트 코드에서 초기화 시 설정 가능하도록 하고 특정 fence를 지정하여 signal 및 wait 할 수 있는 기능 구현
- 텍스처 로딩 관련 버그들 수정
  - move되어 invalid 상태인 `std::string`을 계속 참조하고 있던 버그, move를 제거하여 해결
  - 셰이더 리소스가 기존엔 Renderer 계열 클래스에 있었으나 Shader 계열 클래스로 옮김
    - Shader 계열 클래스들의 메모리 레이아웃이 달라짐에 따라 가상 소멸자 필요,
    Core가 `Shader`가 아닌 `std::unique_ptr<Shader>`들을 저장해 다형성 구현.

## 노트

네트워크 기능이 붙여지면서 많은 버그들이 발생했다.    
10개의 클라이언트가 연결이야 가능하지만, 화면에 다른 클라이언트의 객체를 나타내지 못하고 이상한 깜빡임 현상이 있다.     
현재까지는 이유를 알기 힘들지만 더욱 디버그 해야 한다.

메시에 재질 정보를 결합해야 하는지 말아야 하는지 선택할 필요가 있다.    
첫 번째 고려 사항은 한 가지 메시가 다양한 재질을 사용할 가능성이고,    
두 번째 고려 사항은 여러 메시가 한 가지 재질을 공유할 가능성이다.     
세 번째 고려 사항은 메시의 인스턴스마다 다른 재질을 사용할 가능성이다.    
마지막 고려 사항은 그 어떤 리소스도 중복 로드하지 않아야 한다는 것이다.    
현재는 메시와 재질 정보를 분리하는 방향으로 가고 있다.      
또한 메시에 재질 포인터를 두고 설정하는 것보다,
렌더링할 때 메시와 재질을 클라이언트가 선택해서 렌더링하게 하는 것이     
훨씬 코드가 표현력이 있을 것이다.

한편 재질 정보를 메시 및 모델과 분리한다면,     
모델 파일에 저장되어 있는 재질 정보들이 무용지물이다.     
클라이언트가 모델의 메시와 특정 재질을 직접 선택해 연결시켜야 한다.
모델 클래스는 계층구조 전체를 포괄하는 만큼,     
모델의 특정 메시에 특정 재질을 클라이언트가 연결시키는 작업은     
언뜻 추상화를 저해하는 것처럼도 보인다.     
고민이 더 필요하다.

하나의 씬을 여러 개의 렌더러로 그린다. 라는 개념으로 렌더링 로직을 디자인할지    
공간 분할 및 그림자 렌더링 별도 분리 등에 따라 만들어진 여러 개의 씬들을 서로 다른 렌더러들과 일대일 매칭시킨다.     
라는 개념으로 렌더링 로직을 디자인할지 아직 가닥을 잡지 못했다.    
이것도 함께 고민해보도록 한다.

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
- [ ] D3D12RenderContext에 cmdList 게터 만들기
- [ ] Assimp 파일 말고 메모리로부터 읽도록
- [ ] Material 트리 구축
- [ ] 모델 로드할 때 Input Layout 만족 못하는 메시 처리 및 디버그 기록 
- [ ] Asset System 구현, 특정 에셋을 쿼리 가능하도록: 2024. 09. 20.
- [ ] Model과 Coord 컴포넌트 구현
- [ ] RenderProtocol을 RenderProtocolFamily와 RenderProtocol로 분리, 일부 모델의 메시들이 동일한 Input Layout을 사용하지 않는 점을 해결
- [ ] 셰이더 클래스들 업데이트된 RenderProtocol들로 최신화
- [ ] PBR 셰이더 만들기
- [ ] 문서화 및 폴더, vcxproj 필터 정리
- [ ] Skybox
- [ ] 태양의 위치에 따른 산란들
- [ ] 안개 구현
- [ ] BVH
- [ ] SW Occlusion
- [ ] 애니메이션
- [ ] CPU Particle System
- [ ] GPU Particle System (Compute Shader)
- [ ] 리소스 로딩 멀티스레딩 + 코루틴
- [ ] ResourceBarrier 한꺼번에 Context에 기록
- [ ] 리소스 재정렬: 텍스처 srv 힙에서 안 쓰는 텍스처 제외 srv 힙 재구축, 동적 모델 관리 등
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
- [ ] 모션 블러
- [ ] NVidia GPU Gems 효과들
# 로비 씬 & 백그라운드 리소스 로딩

## 개요
Online 클라이언트(`online/onlineGame`)를 **로비 씬 / 인게임 씬**으로 분리하고,
실행 직후에는 **최소 리소스로 로비**에 진입한 뒤 **ThreadPool로 인게임 리소스를
백그라운드 로드**한다.

## 씬 구조
- `enum class Scene { Lobby, InGame }` — `scene_`
- `update()` / `render()`는 `switch(scene_)`로 분기:
  - `Scene::Lobby`  → `LobbyScene(dt)` / `renderLobby()`
  - `Scene::InGame` → `InGameScene(dt)` / `renderInGame()`  (기존 update/render 본문 이전)
- 로비는 별도 함수 없이 `enum class LobbyState { MainMenu, WaitingRoom }`(`lobbyState_`)을
  `switch`/조건으로 구분한다. 프로토타입 `client/ui/html`의 `mainView`/`lobbyView`에 대응.

## 진입 흐름
1. `Online::Game` 생성자: `gfx_.initSharedResources()`만 수행(공용 GPU 리소스).
2. `ClientApp::setup(Online)` → `enterLobby()`:
   - UIManager 기본 리소스 초기화(`setScreenSize`/`requestDebugResources`, 1회)
   - `buildLobbyUI()` + `refreshLobbyUI()`
   - `startInGameAssetLoad()` — ThreadPool에 인게임 로드 잡 1개 등록
3. 호스트가 "게임 시작"(로드 완료 시 활성) → `enterInGame()`:
   - 로비 UI 숨김 → 렌더 경로 Deferred 복원 → `setupStage()` → `scene_=InGame`
   - 플레이어/오브젝트 생성은 **서버 S_Enter 패킷**(`InGameScene`의 `SleepEx`에서 처리)이 담당.
     enterInGame에서 별도 플레이어를 만들면 S_Enter가 `setupPlayer`를 재호출해 이전 플레이어의
     물리 바디가 물리월드에 dangling으로 남아 크래시하므로, 클라에서 플레이어를 직접 만들지 않는다.

## 리소스 분류
- **로비(즉시, 메인 스레드)**: GFX 코어(폰트 포함) + `initSharedResources`(그림자맵/GBuffer/HiZ/
  정적 메시/white tex) + 로비 UI 위젯.
- **인게임(로비 중 백그라운드)**: 모델(cube/player/goblin), 스카이박스, 지형, 파티클 텍스처(~30),
  이펙트 메시(meshbin), 파티클 머티리얼, 애니메이션 클립(= `AssetManager::loadGFXAssets` 전체)
  **+ 파티클 이펙트 JSON 파싱**(`prefetchParticleConfigs`: `effects/*_ParticleSystems.json`을 모두 파싱해
  `particleConfigCache_`에 적재. key = "<파일명>|<relativePath>").
- **인게임 진입 시(메인 스레드)**: `setupStage()`(지형/스카이박스/인게임 UI), `setupPlayer()`
  (`setParticle()`로 파티클 시스템/이펙트 빌드 + 스킬 컴파일).
  - `setParticle()`의 `loadUnityParticleConfig`는 디스크 재파싱 대신 `particleConfigCache_`에서 config를
    꺼내 GPU 에셋 포인터만 결합한다(캐시 미스 시 직접 파싱으로 폴백). JSON 디스크 I/O가 백그라운드로 이동.

## GFX 로드 경로 분리
`GFX::loadAssets()`를 분리:
- `initSharedResources(configs)` — 공용 GPU 리소스 생성(실행 시 1회, 메인 스레드).
- `loadRequestedAssets()` — `addRequestXXLoad`로 큐잉된 요청만 처리(백그라운드 호출 가능).
- `loadAssets()`는 위 둘을 호출하는 편의 래퍼(standalone 등 기존 경로 유지).

## 동시성(백그라운드 로드 ↔ 메인 로비 렌더)
워커가 GPU 리소스를 로드하는 동안 메인 스레드는 로비를 렌더한다. 보호 포인트:
- **CommandListPool**: 로드는 `ResourceLoading`, 렌더는 `RenderingMaster/Slave`로 서로 다른
  usage 리스트 → 동시 접근해도 안전(락 불필요).
- **DescriptorPool**: 로드의 `createSRV`와 메인의 UI TextImage 생성이 같은 `srvTexPool_`을
  alloc → `DescriptorPool::alloc/free`에 인스턴스별 뮤텍스(`shared_ptr<std::mutex>`) 추가.
  `cpuHandle/gpuHandle/bind`(읽기 전용, 드로우 핫패스)는 잠그지 않는다.
- **로깅**: `gSharedLog`/`dumpLog()`는 thread-unsafe(메인 `render()`가 매 프레임 dumpLog 호출).
  백그라운드 로드 동안 `muteLog()`로 dumpLog를 무력화하여 워커가 단독으로 버퍼를 사용하게 한다.
  로드 완료 시 워커가 `unmuteLog()` 후 완료 플래그(`inGameAssetsLoaded_`)를 store.
- ID3D12Device 리소스 생성, `cmdQ_->ExecuteCommandLists`는 free-threaded. 로드 전용 `LoadFence`는
  프레임 펜스와 분리.

## 로비 네트워킹(현 범위)
룸 프로토콜 미구현(`protocol.hpp`에 C_Enter/S_Enter만 존재)으로, 방 생성/참가/플레이어 슬롯은
`script.js` 프로토타입을 이식한 **mock** 상태로 동작. 실제 LobbyServer/RoomServer 연동은 후속.

## 방 코드 입력 (TextInput 위젯)
프로토타입의 `joinRoomForm`(6자리 코드 입력 → 참가)을 위해 `TextInput` 위젯을 신설했다.
- `client/ui/widgets/TextInput.hpp/.cpp` — `UIElement` 상속, `interactive=true`. 배경 쿼드는 자체 렌더,
  텍스트는 내부 child `Label`로 표시. 포커스 시 `|` 캐럿. 옵션: `uppercase`/`alnumOnly`/`maxLength`/
  `placeholder`, `onChange`/`onSubmit` 콜백.
- 문자 입력 라우팅: `UIElement`에 `onChar(wchar_t)`/`onFocus()`/`onBlur()` 추가. `UIManager::onWndMsg`가
  `WM_CHAR`를 포커스 요소의 `onChar`로 전달하고, 클릭 시 포커스 변경에 맞춰 `onBlur`/`onFocus` 호출
  (빈 공간 클릭 시 포커스 해제). `WM_CHAR`는 메시지 루프의 `TranslateMessage`로 생성됨.
- 메인메뉴: `방 만들기` + 방 코드 입력(`roomCodeInput_`) + `참가`(`lobbyJoinRoom`). 참가는 6자리 검증만
  하고, 서버 미연동 상태라 실제 방 조회가 불가하므로 항상 "방을 찾을 수 없습니다" 로그/메시지를 출력한다
  (대기실 전환 없음). `방 만들기`는 self-host mock으로 대기실 전환.

## 후속(범위 밖)
- 실제 룸 프로토콜 설계·연동 (현재 방 생성/참가/시작은 mock)
- TextInput 캐럿 블링킹/텍스트 선택/클립보드 등 고급 편집

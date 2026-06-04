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
3. 호스트가 "게임 시작"(**로딩 중에도 항상 활성**) → `lobbyStartGame()`:
   - 인게임 에셋 로드 완료면 즉시 `enterInGame()`.
   - 아직 로딩 중이면 `pendingStart_=true`로 두고 **로딩 화면**을 띄운 뒤, `LobbyScene` 폴링이
     `inGameAssetsLoaded_`를 보면 `enterInGame()` 자동 진입. (`pendingStart_`는 `enterLobby`/
     `lobbyLeaveRoom`/`enterInGame`에서 초기화.)
   - `enterInGame()`: 로비 UI 숨김 → 렌더 경로 Deferred 복원 → `setupStage()` → `scene_=InGame`
   - 플레이어/오브젝트 생성은 **서버 S_Enter 패킷**(`InGameScene`의 `SleepEx`에서 처리)이 담당.
     enterInGame에서 별도 플레이어를 만들면 S_Enter가 `setupPlayer`를 재호출해 이전 플레이어의
     물리 바디가 물리월드에 dangling으로 남아 크래시하므로, 클라에서 플레이어를 직접 만들지 않는다.

## 리소스 분류
- **로비(즉시, 메인 스레드)**: GFX 코어(폰트 포함) + `initSharedResources`(그림자맵/GBuffer/HiZ/
  정적 메시/white tex) + 로비 UI 위젯.
- **인게임(로비 중 백그라운드, 2단계)**: `startInGameAssetLoad`가 ThreadPool 잡 1개로 처리.
  - **Phase 1** (`AssetManager::loadLobbyVisualAssets`): 큐브/플레이어 모델, 스카이박스, 플레이어 애니 전체
    → `lobbyVisualAssetsLoaded_` set. 대기실 3D를 띄울 최소 에셋.
  - **직렬화 핸드셰이크**: 메인 스레드가 WaitingRoom 진입 시 `setupStageVisual()`(지형 동기 로드)을 돌려
    `stageVisualReady_`를 세울 때까지 워커가 대기(같은 `LoadFence` 충돌 방지). 방 미생성 시 워커는 파킹.
  - **지형 스트리밍 지연**: `chunkManager_.update()`(매 프레임)도 `recordTerrainResourceLoad`로 같은
    `LoadFence`/`associatedResources_`를 쓴다. Phase 2 `loadRequestedAssets`(워커)와 동시 실행되면 워커의
    업로드 버퍼가 `waitOnFence`의 `clear()`로 삭제돼 D3D12 `OBJECT_DELETED_WHILE_STILL_IN_USE` 크래시.
    → 대기실 스트리밍은 `inGameMeshAssetsLoaded_`(Phase 2 GPU 로드 완료) 이후에만 수행. 그 전에는
    `setupStageVisual`의 동기 baseline 지형으로 충분(로비 카메라는 거의 정지).
  - **Phase 2** (`AssetManager::loadRemainingInGameAssets`): 고블린 모델, 파티클 텍스처(~30), 이펙트
    메시(meshbin), 파티클 머티리얼, 고블린 애니 → `inGameMeshAssetsLoaded_` set
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

## 로비 네트워킹(LobbyServer 연동)
방 생성/참가/퇴장/플레이어 슬롯/게임시작 신호를 **실제 LobbyServer**(127.0.0.1:8000)와 연동한다.
mock 상태(`script.js` 프로토타입 이식)는 제거됨.

- **접속**: 클라이언트는 startup에서 `ServerSession`으로 `lobbyServerPort`(8000)에 접속한다(`ServerSession.hpp`
  생성자). 접속 실패 시 기존대로 StandAlone 폴백.
- **펌핑**: 로비 수신은 APC 완료 루틴(`ServerSession::completionCallback`) 기반이라 alertable 대기가 필요하다.
  `LobbyScene()` 진입부에서 `SleepEx(1, true)`(수신 APC) + `INet::ClientApp::send()`(송신 flush)를 매 프레임 호출한다
  (InGameScene와 동일 단일 스레드 모델 → 패킷 핸들러가 메인 스레드에서 실행되어 UI/상태 변경에 락 불필요).
- **요청(C→S)**: `lobbyCreateRoom`→`C_CreateRoom`, `lobbyJoinRoom`(6자리 정규화 후)→`C_JoinRoom`,
  `lobbyLeaveRoom`→`C_LeaveRoom`, `lobbyStartGame`(호스트 가드)→`C_GameStart`.
  요청 함수는 상태를 직접 바꾸지 않고 패킷만 보낸다.
- **응답(S→C)**: `PacketManager`가 `Online::Game`의 핸들러를 호출.
  - `S_CreateRoom`→`onLobbyCreated(code, myId)`: 본인이 호스트, 슬롯1=본인, 대기실 전환.
  - `S_JoinRoom`→`onLobbyJoined(success, hostId, myId, code, playerIds)`: 실패 시 "방을 찾을 수 없습니다",
    성공 시 슬롯을 서버 목록으로 채우고 `isHost_=(myId==hostId)`.
  - `S_LobbyRoomPlayerJoined`→`onLobbyPlayerJoined(sessionId)`: 슬롯 추가(중복 방지).
  - `S_LobbyRoomPlayerLeft`→`onLobbyPlayerLeft(sessionId)`: 슬롯 제거. 떠난 자가 호스트면 남은 목록의 front를
    새 호스트로(서버 규칙과 일치).
  - `S_GameStart`→`onGameStart(ip, port, code)`: **현재 범위에선 로그만**. RoomServer 접속/인게임 전환은 후속.
- **자기 식별**: `protocol.hpp`의 `S_CreateRoom`/`S_JoinRoom`에 `myId`(수신자 본인 sessionId)를 추가해
  본인 슬롯 표시("나")와 호스트 판별/이양을 정확히 처리한다. `LobbyPlayer.id`(string)는 `sessionId`(uint16)로 변경.

## 방 코드 입력 (TextInput 위젯)
프로토타입의 `joinRoomForm`(6자리 코드 입력 → 참가)을 위해 `TextInput` 위젯을 신설했다.
- `client/ui/widgets/TextInput.hpp/.cpp` — `UIElement` 상속, `interactive=true`. 배경 쿼드는 자체 렌더,
  텍스트는 내부 child `Label`로 표시. 포커스 시 `|` 캐럿. 옵션: `uppercase`/`alnumOnly`/`maxLength`/
  `placeholder`, `onChange`/`onSubmit` 콜백.
- 문자 입력 라우팅: `UIElement`에 `onChar(wchar_t)`/`onFocus()`/`onBlur()` 추가. `UIManager::onWndMsg`가
  `WM_CHAR`를 포커스 요소의 `onChar`로 전달하고, 클릭 시 포커스 변경에 맞춰 `onBlur`/`onFocus` 호출
  (빈 공간 클릭 시 포커스 해제). `WM_CHAR`는 메시지 루프의 `TranslateMessage`로 생성됨.
- 메인메뉴: `방 만들기` + 방 코드 입력(`roomCodeInput_`) + `참가`(`lobbyJoinRoom`). 참가는 6자리 정규화/검증 후
  `C_JoinRoom`을 전송하고, 서버의 `S_JoinRoom` 응답으로 성공 시 대기실 전환·실패 시 "방을 찾을 수 없습니다"를
  표시한다(`onLobbyJoined`). `방 만들기`는 `C_CreateRoom` 전송 후 `S_CreateRoom`(`onLobbyCreated`)으로 대기실 전환.

## 메인화면 배경 이미지 & 로고 (이미지 에셋)
메인화면(`LobbyState::MainMenu`)을 단색 UI에서 **배경 키 아트 + 게임 로고(OutLander)** 구성으로 전환.
- 에셋: `resources/UI/ui_lobby_bg.dds`(배경, 1672×941≈16:9), `resources/UI/ui_lobby_logo.dds`(로고, 2172×724≈3:1, 알파 포함).
  파일명 컨벤션은 기존 UI 에셋(`player_hp_*`)을 따른 소문자 snake_case + `ui_` 접두사.
- **즉시 로드**: `loadLobbyTextures()`가 `addRequestTextureLoad` 2건을 큐잉하고 **메인 스레드에서 즉시
  `loadRequestedAssets()`**를 호출한다. `enterLobby()`에서 `buildLobbyUI()` **이전**에 불러 위젯에 텍스처를
  연결한다. 인게임 백그라운드 로드(`startInGameAssetLoad`)가 시작되기 전이라 요청 큐가 겹치지 않는다.
  텍스처는 전용 `lobbyTexHashMap_`/`lobbyBgTex_`/`lobbyLogoTex_`에 보관(인게임 `AssetManager` 해시맵과 분리).
- **배경(cover 스케일)**: `UI::Image`를 화면 중앙에 두고, 원본 종횡비를 유지한 채 화면을 덮도록 크기를
  계산(화면이 더 세로로 길면 높이 기준, 가로로 길면 너비 기준). 넘치는 영역은 뷰포트에서 클리핑.
  텍스처 로드 실패 대비로 단색 sky(`zOrder -11`)를 뒤에 깔고 이미지를 `zOrder -10`에 올린다.
- **로고**: `UI::Image`를 메인 패널 위(`Anchors::Center`/`Pivots::BottomCenter`, `offsetY = -(패널높이/2+16)`)에
  종횡비 유지하여 배치(`zOrder 5`). 두 위젯 모두 `lobbyBgImage_`/`lobbyLogoImage_`로 핸들 보관.
- 로더는 `DDSTextureLoader12`(압축/비압축·알파 지원). 대기실(3D 맵 배경)은 별도 후속 작업.

## 9-slice 프레임 (패널/버튼/입력창 텍스처)
단색 UI 블록을 **9-slice 텍스처 프레임**으로 교체. 에셋은 normal 상태 1장씩만 준비하고
hover/pressed는 엔진 틴트로 처리.
- 에셋: `ui_panel_frame.dds`(1254², 패널), `ui_btn_primary.dds`/`ui_btn_secondary.dds`(720², 버튼),
  `ui_input.dds`(2048×768, 입력창). 모두 `loadLobbyTextures()`에서 즉시 로드(BilinearClamp).
- **9-slice 렌더링**: `UIShader::PerInstanceData`/`ui.hlsl`에 `uvScaleBias`(uv'=uv*xy+zw) 추가,
  `UIPipeline::DrawEvent`에 동일 필드 추가. `UI::emitNineSlice()`(`UIElement.cpp`)가 요소 사각형을
  9개 셀로 나눠 셀마다 부분 UV로 `DrawEvent`를 emit한다. 코너는 화면 px 고정, 가장자리/중앙만 늘어남.
- **위젯 지원**: `Button`에 `tex*`(기존) + `sliceUvBorderX/Y`/`sliceCornerX/Y` + 상태별 `texTint*`,
  `TextInput`에 `backgroundTex` + 동일 slice 필드. 텍스처 경로에 `colorMul` 틴트 적용(hover 밝게/press 어둡게).
- **적용 대상**: 메인/대기실 패널 배경, primary/secondary 버튼, 방 코드 입력창. 작은 배지·구분선·더미
  버튼은 단색 유지. 텍스처 미로드 시 자동으로 기존 단색 폴백.
- **튜닝**: slice 경계/코너 px는 `buildLobbyUI`의 `stylePanel`/`stylePrimary`/`styleSecondary` 람다와
  입력창 블록의 상수로 조정(에셋 모양에 맞춰 눈으로 맞추는 값).

## 대기실 3D 맵 배경 + 스쿼드 스테이지 레이아웃 (작업 B-1)
대기실(`LobbyState::WaitingRoom`)을 단색/이미지 배경에서 **3D 맵(terrain+skybox) 배경 + 반투명
스쿼드 스테이지 UI**로 전환. (HTML 프로토타입 `ui/html`의 squad-stage 구조 이식.)
- **스테이지 비주얼 분리**: `setupStage()`에서 3D 비주얼부(`chunkManager_.init` + skybox + `dirLight_`)를
  `setupStageVisual()`로 추출하고 `stageVisualReady_` 플래그로 1회만 init(idempotent). 대기실 3D 배경과
  인게임이 공유 → `enterInGame()`의 `setupStage()`가 지형을 재init하지 않는다.
- **정적 카메라**: 전용 `lobbyCamera_`(게임 `camera_`와 분리) + `Camera::setView(eye, at)` 신설(타겟
  추종형과 무관하게 view 직접 설정). 포커스(`stageFocus_`)는 **`level.bin`의 `PlayerStart` 노드 위치**
  평균 XZ를 지형 높이(`heightAtWorld`)에 앉힌 지점(스폰 좌표가 없으면 `worldCenter()` 폴백). `PlayerStart`는
  `importNode`에서 `stageSpawnPositions_`로 캡처(레벨 파싱은 `setupStageVisual`로 이동 — 대기실도 스폰 정보
  획득). orbit은 쓰지 않음(슬롯 정렬 유지). 카메라 오프셋/FOV는 `LobbyScene` 상수로 튜닝.
- **타이밍/로딩 화면**: 대기실 진입 후 `lobbyVisualAssetsLoaded_`(Phase 1)가 true가 되면 `LobbyScene`에서
  `setupStageVisual()` → Deferred 전환 + 카메라/`chunkManager_.update` 갱신. 그 전(및 `setupStageVisual`의
  동기 지형 로드까지, 즉 `!stageVisualReady_`)에는 **로딩 화면**(검정+로고+ProgressBar+"loading..."+점 링
  스피너, `buildLoadingScreen`/`updateLoadingScreen`)을 띄운다. `pendingStart_`일 때도 동일. 진행도는
  `loadProgress01()`(Phase 1 → Phase 2 메시 → 파티클 순).
- **워밍업 게이트(첫 프레임 팝인 가림)**: `stageVisualReady_`가 서는 순간 곧장 오버레이를 내리면, 지형 submit /
  캐릭터 애니 settle / 포트레이트 오프스크린 RT / GPU 업로드가 1~수 프레임에 걸쳐 들어와 "그려지는 과정"이
  노출된다. 이를 막기 위해 로딩 종료 조건을 렌더 시작과 분리했다. `renderLobby()`는 `stageVisualReady_`되면
  바로 `renderWaitingRoom()`을 호출하되, **로딩 오버레이(불투명 검정, 화면 전체)가 위를 덮은 채로** 3D를
  워밍업한다. `renderWaitingRoom()`이 실제 렌더한 프레임을 `waitingRoomWarmupFrames_`로 세고, 이 값이
  `kWarmupFrames`(=4)에 도달해야(`waitingRoomWarm`) 오버레이를 내린다 → 플레이어가 보는 첫 프레임은 이미
  지형·캐릭터·포트레이트가 채워진 완성 화면. 카운터는 메인 스레드 전용(render 증가 / LobbyScene update 읽기,
  단일 루프라 atomic 불필요)이며 `lobbyLeaveRoom`/`enterInGame`에서 0으로 리셋(`stageVisualReady_`는
  idempotent로 유지되므로, 재입장 시 게이트는 카운터 리셋으로만 다시 작동). `pendingStart_` 경로는 워밍업과
  무관하게 오버레이를 유지한다.
- **렌더 분기**: `renderLobby()`는 `WaitingRoom && stageVisualReady_`이면 `renderWaitingRoom()`(=
  `renderInGame()`의 3D 부분 최소 복제: skybox + terrain submit + `lobbyCamera_.updateGFX` +
  `dirLight_.render` + PBR/Terrain FrameData → UI 오버레이 → `gfx_.render()`), 아니면 UI-only.
  (플레이어/이펙트/HiZ/CSM 그림자 제외.)
- **전환 보정**: `lobbyLeaveRoom()`(대기실→메인) Forward 복원 + 키아트 bg 재표시. `setRenderPath`는 단순
  enum 대입이라 매 프레임/전환 호출 안전.
- **반투명 UI 레이아웃**(`buildLobbyUI` 대기실 섹션 재작성): 넓은 패널 → 상단 툴바(방코드+복사 /
  게임시작·대기메시지 / 방나가기) + 가로 4칸 슬롯(슬롯번호·모델베이·이름표·호스트뱃지) + 디버그 툴.
  3D 위 가독성을 위해 어두운 반투명 scrim + 밝은 텍스트, 슬롯 컬럼은 더 투명(캐릭터/맵 비침). 반투명은
  신규 에셋 없이 `colorMul.a`(단색)·`texTint.a`(9-slice 버튼)로 처리.
- **슬롯 멤버**: `slotPanels_`/`slotBays_`/`slotNumberLabels_`/`slotNameplateBgs_`/`slotNameLabels_`/
  `slotHostBadgeLabels_`. `slotBays_[i]`는 렌더 없는 `UIElement`로 **B-2에서 3D 캐릭터를 투영할 화면
  사각형**(`resolvedRect_`)을 제공한다.

### 작업 B-2 (구현됨) — 슬롯별 3D 캐릭터 + IDLE
- **카메라**(B-1 보정): `stageFocus_` 자동 계산 대신 standalone에서 잡은 **쇼케이스 위치(baseEye/baseAt)
  하드코딩 + 느린 좌우 sway 패닝**(`lobbyCameraTime_`). 스폰 평균점은 지면 높이가 0으로 잡혀 부정확해
  실제 보기 좋은 뷰를 직접 고정함.
- **전시 캐릭터**: `lobbyChars_`(최대 4, `Player`, **물리 없음**). `setupLobbyCharacters()`가
  모델/애니메이션/고유 `renderObjectId`(0..3 고정)를 준비하고, `updateLobbyCharacterTransforms()`가 배치한다.
- **카메라-고정 배치(sway 대응)**: 캐릭터 XZ를 **`lobbyCamera_.eye()` 기준 카메라 공간**(eye + forward·standDist
  + right·lateral)으로 잡는다. 배경 sway 시 eye가 가장 크게 움직이므로, eye 기준으로 두면 캐릭터가 카메라와
  함께 이동해 **화면 슬롯 위치/정면이 고정**되고 월드 고정인 지형/스카이박스만 패럴랙스로 흐른다. (at 기준으로
  잡으면 eye만큼 안 따라가 캐릭터가 좌우로 흔들려 얼굴이 프레임 안팎으로 드나든다.) Y는 시선 높이(`at.y`)로
  안정화(스트리밍 타이밍 무관). 각 캐릭터는 매 프레임 `eye`를 향하도록 yaw(`NQuat` (roll,pitch,yaw)).
  `standDist`/`spacing`/`footOffset` 튜닝.
- **루프**: `LobbyScene`에서 최초 진입 시 생성(`lobbyChars_.empty()`), 이후 **매 프레임**
  `updateLobbyCharacterTransforms()`(화면 슬롯 고정) → `ch->update(dt,1.f)`(idle 선택/월드행렬) →
  `animSystem_.update(0.01s)`(본 행렬). `renderWaitingRoom`에서 **채워진 슬롯 수만큼만** `ch->render(gfx_)`.
- **정리(누수 방지)**: `lobbyLeaveRoom`/`enterInGame`에서 `clearLobbyCharacters()`로 `animSystem_`
  트랙 해제 + shared_ptr 제거(재입장 시 재생성). `setupStageVisual`에서 `setMaxRenderObjectId`로 스킨드
  렌더 가시성 배열 확보.
- 후속(선택): 캐릭터 턴테이블 회전, CSM 그림자, 이름표를 캐릭터에 정확 정렬(`worldToScreen`).

### 작업 B-3 (구현됨) — 슬롯 캐릭터를 배경 카메라에서 분리(오프스크린 RT → UI 합성)
B-2는 캐릭터를 배경 카메라(`lobbyCamera_`) 기준 월드 좌표에 놓아 sway 시 화면에 *대략* 고정되게
하는 편법이라 슬롯 사각형에 정확히 정렬되지 않았다. B-3은 캐릭터 렌더링을 배경 카메라에서
**완전히 분리**해 슬롯에 픽셀 단위로 정확히 들어가게 한다.
- **오프스크린 포트레이트 RT**: `SharedResources::Portrait`(`sharedResources.hpp/.cpp`). 가로 아틀라스
  1장(`kPortraitCellW × kMaxPortraitSlots` 폭 × `kPortraitCellH`)을 **roomIdx별(triple-buffer)** 로
  생성. color(R8G8B8A8, RTV+bindless SRV) + depth(D32, DSV only, 미샘플). `transitionToWrite/Read`,
  `clearPortraitRT`(투명 0,0,0,0). `GFX::initSharedResources`에서 생성. RTV heap 16→24, DSV heap 24→32 증설.
- **포워드 스킨드로 렌더**: 배경은 기존대로 deferred로 backbuffer에, 캐릭터는 **포워드
  `PBRSkinnedPipeline`**으로 포트레이트 RT 셀에 그린다(작은 RT에 GBuffer는 과함). 슬롯마다 전용
  카메라/viewport로 `mainPass()`만 호출(`shadowPass` 미수행).
- **그림자 off**: Dispatcher의 `mainDirectionalLight.cascadeCount=0`. 단 `pbrLighting.hlsli`의
  `calcCSMShadow`/디버그 블록이 `cascadeCount - 1u` 언더플로를 일으키므로, 두 곳에 `cascadeCount==0`
  early-return/가드를 추가했다. 포트레이트는 항상 일반 `PBRSkinnedShader` PSO 고정(디버그 토글 무시).
- **직접광**: lightData가 비면 ambient만 받아 밋밋하므로 정면-상단 고정 키 라이트 1개를 공급
  (`addLobbyPortraitLightData`). view-space 변환은 슬롯 카메라로 `mainUpdate`가 처리.
- **슬롯별 전용 Resources**: `ShaderInputBuffer::stage()`가 같은 room 버퍼에 즉시 memcpy하므로 4슬롯이
  Resources를 공유하면 프레임 내 덮어쓰기 발생 → `resourcesLobbyPortrait_[slot]` 분리. `Object::render`가
  submesh마다 DrawEvent를 내고 boneData가 누적되므로 용량은 submesh 단위(`kMaxPortraitDrawEventsPerSlot`,
  `kMaxPortraitBonesPerSlot`)로 산정. mainPass만 쓰므로 shadowPass 버퍼는 미init.
- **GFX 채널**: `drawEventsLobbyPortrait_[slot]`/`cameraDataLobbyPortrait_[slot]`/`lightDataLobbyPortrait_`/
  `mainDirectionalLightLobbyPortrait_`/`frameDataLobbyPortrait_`/`lobbyPortraitActive_` + API
  (`addLobbyPortraitDrawEvent`/`setLobbyPortraitCamera`/`addLobbyPortraitLightData`/`addLobbyPortraitFrameData`/
  `setLobbyPortraitActive`/`lobbyPortraitTextureForThisFrame`/`lobbyPortraitCellUvScaleBias`).
- **render() 삽입 위치**: deferred lighting 이후 + UI 디스패치 이전. 활성(`lobbyPortraitActive_`) 동안
  채워진 슬롯이 0개여도 clear+transitionToRead 수행(전 슬롯 비는 전환 프레임 잔상 방지). 인게임에선
  비활성 → 패스 전체 스킵(비용 0). 패스 후 `lightDataLobbyPortrait_`/draw event clear(per-frame 누적 방지).
- **UI 합성**: `slotBays_[i]`를 렌더 없는 `UIElement`에서 **`UI::Image`**로 전환. `UI::Image`에
  `uvScaleBias`/`colorMul` 필드를 추가(`onRender`가 `UIPipeline::DrawEvent`로 전달)해, 아틀라스의 셀
  sub-rect(`lobbyPortraitCellUvScaleBias`)를 샘플한다. 투명 배경 알파 블렌딩 → 뒤 3D맵 비침.
  매 프레임 `renderWaitingRoom()`에서 `lobbyPortraitTextureForThisFrame()`(이번 프레임 room의 color SRV)로
  텍스처를 갱신 → UI가 참조하는 SRV와 포트레이트 패스가 쓰는 room이 정합. 채워진 슬롯만 `visible=true`.
- **캐릭터 배치**: 슬롯마다 독립 셀에 렌더되므로 모든 캐릭터를 원점·정면(+Z, 카메라가 +Z쪽)에 두고
  동일 프레이밍 카메라(`lobbyPortraitCams_`, perspective aspect=cellW/cellH)를 쓴다. 프레이밍 튜닝값
  (`lobbyPortraitCamDist_`/`CamHeight_`/`LookHeight_`/`FovYDeg_`)은 대기실 숫자키 1~6로 런타임 조정.
- **제출 경로**: 기존 `Player::render`는 deferred로 라우팅되므로 재사용 불가. `Object::renderPortrait(gfx, slot)`
  신설(스킨드 메시만, 컬링 무시) → 포워드 `PBRSkinnedPipeline::DrawEvent`를 포트레이트 채널로 제출.
- **정리**: `clearLobbyCharacters()`(대기실 이탈/인게임 진입의 공통 chokepoint)에서 `setLobbyPortraitActive(false)`
  + 슬롯 이미지 숨김/텍스처 해제. 포트레이트 RT는 공용 리소스로 상주(재입장 시 재사용).

## 후속(범위 밖)
- `S_GameStart` 수신 후 RoomServer 핸드오프: LobbyServer 연결 해제 → RoomServer(ip/port) 재접속 →
  `enterInGame()`. ServerSession 재타게팅(소켓 재생성/recvBuf 리셋) 필요. RoomServer는 현재 lobbyCode가 아닌
  접속 순서(`totalSessions % 4`)로 방을 묶으므로, lobbyCode 기반 그룹화도 함께 설계 필요.
- 호스트 이양 시 비-퇴장 클라이언트가 새 호스트를 명시적으로 통보받는 패킷(현재는 left 패킷의 leaver id로 추론).
- TextInput 캐럿 블링킹/텍스트 선택/클립보드 등 고급 편집

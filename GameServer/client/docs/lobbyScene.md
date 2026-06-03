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
- **타이밍/폴백**: 대기실 진입 후 `inGameAssetsLoaded_`가 true가 될 때까지는 키아트 bg(`lobbyBgImage_`)를
  폴백으로 보여주고(Forward), 완료되면 `LobbyScene`에서 `setupStageVisual()` → Deferred 전환 + bg 숨김 +
  카메라/`chunkManager_.update(center)` 갱신.
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

## 후속(범위 밖)
- 실제 룸 프로토콜 설계·연동 (현재 방 생성/참가/시작은 mock)
- TextInput 캐럿 블링킹/텍스트 선택/클립보드 등 고급 편집

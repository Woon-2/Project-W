# 로비 / 설정창 UI 분리 (onlineGame UI 레이어 추출)

## 배경

`online/onlineGame.cpp`가 4,783줄까지 커졌고, 그중 로비/설정/로딩 UI 코드가 약 1,500줄(전체의 1/3 가까이)을 차지했다. 파일이 너무 커서 편집·탐색 시 토큰 낭비가 컸다. UI 위젯 빌드 코드(특히 `buildLobbyUI` ~530줄)를 별도 컴포넌트로 분리한다.

추가 요구사항: **설정창을 나중에 인게임 씬에서 ESC로도 재사용**한다. 기존 설정 패널은 `mainMenuRoot_`(→ `lobbyRoot_`) 자식이어서 인게임 씬(`lobbyRoot_` 숨김)에선 같이 사라진다. 따라서 설정창은 씬 비종속 독립 컴포넌트로 뽑는다.

## 결정

UI를 **전용 컴포넌트 클래스**로 추출하되 2D UI 레이어만 분리한다(3D 배경/네트워킹/씬 상태머신/에셋로드는 `Online::Game`에 잔류).

| 컴포넌트 | 파일 | 책임 |
|----------|------|------|
| `UI::Build` 헬퍼 | `ui/uiBuild.hpp` | `addSolid/addLabel/addButton/applyRect` 공용 inline 빌더 |
| `Online::LobbyUI` | `online/lobbyUI.hpp/cpp` | 메인메뉴 + 스쿼드 스테이지(대기실) + 로딩 오버레이 + 로비 텍스처 |
| `UI::SettingsPanel` | `ui/settingsPanel.hpp/cpp` | 씬 비종속 재사용 설정창 |
| `GameSettings` | `ui/settingsPanel.hpp` | 영속 설정 값(Game 소유) |

위젯의 소유권은 그대로 `UIManager` 트리(`addChild` → `unique_ptr`)에 있고, 컴포넌트는 비소유 raw 포인터만 보관한다 → **수명/소유권 변화 없음**.

## 경계와 데이터 흐름

- **LobbyUI**는 위젯/텍스처/로딩오버레이를 소유. Game은 `build(uiManager, Callbacks)`로 1회 구성하고, 상태 변화 시 `refresh(ViewState)` 스냅샷을 넘긴다. 버튼 액션은 `Callbacks`(create/join/leave/start/copy/openSettings/quit)로 Game에 역호출. 로딩 진행률은 Game의 `loadProgress01()` 결과 float만 `updateLoading()`에 전달 → **로드 atomic은 경계를 넘지 않음**.
- **세션 상태**(`roomCode_`/`lobbyPlayers_`/`isHost_`/`hostId_`/`myId_`)와 **씬 상태**(`scene_`/`lobbyState_`)는 네트워킹이 소유하므로 Game에 잔류. `refreshLobbyUI()`는 이들로 `LobbyUI::ViewState`를 만들어 위임하는 얇은 래퍼가 됐다.
- **SettingsPanel**은 `uiManager_.root()` 직속(zOrder 50)에 빌드돼 로비·인게임 어느 씬 위에도 뜬다. `open()/close()/toggle()`로 토글. 값은 `GameSettings&`에 write-through하므로 로비/인게임/게임플레이가 같은 값 하나를 공유한다.

### 동시성

로비 UI 코드는 전부 메인 스레드(LobbyScene/render/lobby recv APC)에서만 실행된다. 분리 후에도 스레딩 모델·직렬화는 변하지 않는다. 로드 진행 atomic은 Game 쪽에 남아 `loadProgress01()`에서만 읽힌다.

## 인게임 ESC 재사용 (구현 완료)

설정창이 씬 비종속이라(uiManager_.root() 직속, lobbyRoot_ 가시성과 무관) 인게임에서도 그대로 떠 있고, `InGameScene`이 매 프레임 `uiManager_.layout/update/render`를 호출하므로 입력·렌더가 동작한다.

- **열기/닫기**: `processInput()`(인게임에서만 호출)에서 `VK_ESCAPE` 엣지를 감지해 `settingsPanel_.toggle()`. 닫기는 ESC 재입력 또는 패널의 "닫기" 버튼 둘 다 가능.
- **입력 차단**: `settingsPanel_.isOpen()`이면 `processInputGame()`과 Enter/Space 커서 토글을 건너뛰고(early return) 누적 마우스 델타를 비운다 → 카메라/이동/공격/스킬/커서 토글이 전부 막히고, 닫은 직후 카메라 튐도 없다.
- **커서**: 설정창 열림/닫힘 **전이**를 추적(`settingsOpenPrev_`). 열리면 `releaseCursor()+showCursor()`로 커서를 풀어 UI 클릭 가능, 닫히면 `cursorCaptureEnabled_`/`cursorShowEnabled_` 플래그 기준으로 게임플레이 커서 모드를 복원. 전이 기반이라 ESC·"닫기" 버튼 어느 경로로 닫혀도 일관되게 복원된다.
- **포커스 복귀 가드**: `WM_SETFOCUS`에서 설정창이 열려 있으면 게임플레이 커서 캡처/숨김을 복원하지 않는다(Alt-Tab 복귀 시 UI 클릭 유지).

남은 후속: `settingsAllyDamageVisible_`/`monsterDamageOpacity_`/`resolutionIndex`/`fullscreen` 값의 실제 게임플레이·렌더 반영(현재는 `GameSettings`에 저장만 되고 소비처 없음).

## 메모

- 새 파일은 UTF-8 with BOM으로 저장(한글 문자열 리터럴 보존). 새 파일에는 한글 **주석**을 넣지 않는다(프로젝트 규칙). 한글 **UI 캡션 리터럴**은 의도된 것이라 보존.
- `onlineGame.cpp`는 약 4,016줄로 축소(약 767줄 감소). `onlineGame.hpp`의 로비/설정/로딩 멤버 선언(텍스처 7 + 위젯 다수 + 설정 상태 6 + 로딩 5)이 제거됨.

# UI 시스템 설계 문서

## 1. 아키텍처 개요

기존 `UIPipeline` (저수준 렌더링) 위에 게임 레벨 UI 추상화 계층을 구축한다.
`UIPipeline`, `GFX`, `Font` 시스템은 **변경하지 않는다**.

```
Game Logic (StandAlone::Game, Online::Game)
    │
    ▼
UIManager  ── 엘리먼트 트리 소유, 입력 라우팅, 레이아웃 계산
    │
    ▼
UIElement tree  ── Panel, Image, Label, Button, ProgressBar, Slider
    │  각 엘리먼트가 0~N개의 DrawEvent 생성
    ▼
GFX::addDrawEvent(UIPipeline::DrawEvent)  →  UIPipeline::Dispatcher  →  GPU
```

### 디렉터리 구조

```
client/
  ui/
    UITypes.hpp          -- 공유 열거형, 구조체 (Anchor, Rect 등)
    UIElement.hpp/.cpp   -- 베이스 클래스
    UIManager.hpp/.cpp   -- 중앙 관리자
    widgets/
      Panel.hpp/.cpp
      Image.hpp/.cpp
      Label.hpp/.cpp
      Button.hpp/.cpp
      ProgressBar.hpp/.cpp
      Slider.hpp/.cpp
```

---

## 2. 핵심 타입 (UITypes.hpp)

### Anchor
부모 사각형 위의 기준점. `(0,0)` = 좌상단, `(1,1)` = 우하단.

```cpp
struct Anchor {
    float x = 0.f;  // 0..1
    float y = 0.f;  // 0..1
};
```

프리셋:
| 이름 | x | y |
|------|---|---|
| TopLeft | 0.0 | 0.0 |
| TopCenter | 0.5 | 0.0 |
| TopRight | 1.0 | 0.0 |
| CenterLeft | 0.0 | 0.5 |
| Center | 0.5 | 0.5 |
| CenterRight | 1.0 | 0.5 |
| BottomLeft | 0.0 | 1.0 |
| BottomCenter | 0.5 | 1.0 |
| BottomRight | 1.0 | 1.0 |

### Pivot
엘리먼트 자기 자신의 기준점. Anchor 위치에 Pivot 지점이 놓인다.

```cpp
struct Pivot {
    float x = 0.f;  // 0..1
    float y = 0.f;  // 0..1
};
```

### DimValue
픽셀 또는 부모 대비 퍼센트로 표현되는 값.

```cpp
struct DimValue {
    float value = 0.f;
    bool isPercent = false;  // true: value는 0~100 사이 퍼센트

    float resolve(float parentDim) const {
        return isPercent ? (value * 0.01f * parentDim) : value;
    }
};
```

### Rect
레이아웃 계산 후의 절대 픽셀 사각형. **(0,0) = 화면 상단 좌측**, Y 증가 = 아래쪽.

```cpp
struct Rect {
    float x = 0.f;      // 좌측 모서리 (픽셀)
    float y = 0.f;      // 상단 모서리 (픽셀)
    float width = 0.f;
    float height = 0.f;

    bool contains(float px, float py) const {
        return px >= x && px <= x + width && py >= y && py <= y + height;
    }
};
```

### Color

```cpp
struct Color {
    float r = 1.f, g = 1.f, b = 1.f, a = 1.f;
};
```

---

## 3. 베이스 클래스 (UIElement)

### 인터페이스

```cpp
class UIElement {
public:
    virtual ~UIElement() = default;

    // --- 속성 ---
    std::string name;
    Anchor anchor       = Anchors::TopLeft;
    Pivot  pivot        = Pivots::TopLeft;
    DimValue offsetX;          // 앵커 위치로부터의 오프셋
    DimValue offsetY;
    DimValue width;            // 엘리먼트 크기
    DimValue height;
    Color colorTint     = { 1.f, 1.f, 1.f, 1.f };
    int   zOrder        = 0;   // 높을수록 나중에 그림 (위에 표시)
    bool  visible       = true;
    bool  interactive   = false; // 마우스 이벤트 수신 여부

    // --- 계층 ---
    UIElement* parent() const;
    const std::vector<std::unique_ptr<UIElement>>& children() const;
    UIElement* addChild(std::unique_ptr<UIElement> child);
    void removeChild(UIElement* child);
    UIElement* findChild(std::string_view name) const;

    // --- 레이아웃 ---
    void layout(const Rect& parentRect);
    const Rect& resolvedRect() const;

    // --- 업데이트/렌더 ---
    virtual void onUpdate(const UpdateContext& ctx) {}
    virtual void onRender(GFX& gfx) {}
    void updateTree(const UpdateContext& ctx);
    void renderTree(GFX& gfx);

    // --- 입력 콜백 ---
    virtual void onMouseEnter() {}
    virtual void onMouseLeave() {}
    virtual void onMouseDown(MouseButton btn, float localX, float localY) {}
    virtual void onMouseUp(MouseButton btn, float localX, float localY) {}
    virtual void onMouseMove(float localX, float localY) {}
    virtual void onKeyDown(int vkCode) {}
    virtual void onKeyUp(int vkCode) {}

protected:
    mu::Mat4x4 buildWorldMatrix() const;
    Rect resolvedRect_{};

private:
    UIElement* parent_ = nullptr;
    std::vector<std::unique_ptr<UIElement>> children_;
};
```

### UpdateContext

Label 등 일부 위젯이 GFX 접근을 필요로 하므로, 업데이트 시 컨텍스트를 전달한다.

```cpp
struct UpdateContext {
    float deltaTimeSec;
    GFX* gfx;
    FontHandle* defaultFont;
};
```

### 소유권 모델

- 부모가 `unique_ptr`로 자식을 소유한다.
- `parent_`는 raw 포인터 (역참조용).
- 결정론적 수명, 순환 참조 없음.

### renderTree 정렬

`renderTree`는 자식을 `zOrder` 오름차순으로 정렬한 후 순서대로 `onRender` 호출.
`UIPipeline`은 제출 순서대로 그리므로, 제출 순서 = 시각적 계층.

---

## 4. 레이아웃 시스템

### 해석 알고리즘

`UIElement::layout(const Rect& parentRect)`:

```
1. 부모 기준점 계산:
     refX = parentRect.x + anchor.x * parentRect.width
     refY = parentRect.y + anchor.y * parentRect.height

2. 크기 해석:
     w = width.resolve(parentRect.width)
     h = height.resolve(parentRect.height)

3. 오프셋 해석:
     ox = offsetX.resolve(parentRect.width)
     oy = offsetY.resolve(parentRect.height)

4. 피벗 적용 (엘리먼트의 어느 지점이 기준점+오프셋에 놓이는지):
     finalX = refX + ox - pivot.x * w
     finalY = refY + oy - pivot.y * h

5. 저장:
     resolvedRect_ = { finalX, finalY, w, h }

6. 자식들에 대해 resolvedRect_를 부모 Rect로 재귀.
```

### 루트 사각형

`UIManager`가 화면 전체를 나타내는 루트 Rect를 제공:
```
Rect screenRect = { 0, 0, screenWidth, screenHeight };
```

### Y축 변환 (레이아웃 → 셰이더)

레이아웃은 **top-down** 좌표계 (Y=0 = 상단)를 사용한다.
그러나 `ui.hlsl`은 픽셀 Y를 다음과 같이 NDC로 변환한다:
```hlsl
ndc.y = (pos.y / screenHeight) * 2.0 - 1.0
```
이 공식에서 Y=0 → NDC -1 (하단), Y=screenHeight → NDC +1 (상단).
따라서 **셰이더에서 Y가 클수록 화면 위쪽**이다.

`buildWorldMatrix()`에서 Y를 뒤집는다:
```cpp
mu::Mat4x4 UIElement::buildWorldMatrix() const {
    mu::Vec3 scale{ resolvedRect_.width * 0.5f,
                    resolvedRect_.height * 0.5f,
                    1.f };
    mu::Vec3 translate{
        resolvedRect_.x + resolvedRect_.width * 0.5f,
        screenHeight - (resolvedRect_.y + resolvedRect_.height * 0.5f),
        0.f
    };
    return mu::Mat4x4(mu::scale(scale)) * mu::translate(translate);
}
```

> 참고: `BasicPlayerHpUI`에서 HP바 Y위치가 `768.0f - 40.0f = 728.0f`인 것은 "화면 하단에서 40px 위"를 의미한다. 이 패턴과 일치한다.

### 사용 예시

**HP바 (화면 하단 중앙):**
```cpp
auto hpBar = std::make_unique<ProgressBar>();
hpBar->anchor  = Anchors::BottomCenter;
hpBar->pivot   = Pivots::BottomCenter;
hpBar->width   = { 1024.f, false };  // 1024px
hpBar->height  = { 64.f, false };
hpBar->offsetY = { -40.f, false };   // 하단에서 40px 위
```

**크로스헤어 (화면 정중앙):**
```cpp
auto crosshair = std::make_unique<Image>();
crosshair->anchor  = Anchors::Center;
crosshair->pivot   = Pivots::Center;
crosshair->width   = { 32.f, false };
crosshair->height  = { 32.f, false };
```

**패널 (우측 상단, 화면 너비의 30%):**
```cpp
auto panel = std::make_unique<Panel>();
panel->anchor = Anchors::TopRight;
panel->pivot  = { 1.f, 0.f };          // 우상단 피벗
panel->width  = { 30.f, true };         // 부모 너비의 30%
panel->height = { 100.f, true };        // 부모 높이의 100%
```

---

## 5. 위젯 클래스

### Panel
컨테이너 역할. 선택적으로 배경 텍스처를 렌더링.

```cpp
class Panel : public UIElement {
public:
    const Texture* backgroundTex = nullptr;
    void onRender(GFX& gfx) override;
    // backgroundTex가 설정된 경우에만 DrawEvent 제출
};
```

### Image
단일 텍스처를 표시하는 가장 단순한 렌더러블 엘리먼트.

```cpp
class Image : public UIElement {
public:
    const Texture* texture = nullptr;
    void onRender(GFX& gfx) override;
    // DrawEvent 1개 제출: world = buildWorldMatrix(), pTex = texture
};
```

### Label
기존 `TextImage` + `Font` 시스템을 사용하여 텍스트 렌더링.

```cpp
class Label : public UIElement {
public:
    void setText(const std::wstring& text);
    void setFont(FontHandle* font);
    void onUpdate(const UpdateContext& ctx) override;
    void onRender(GFX& gfx) override;

private:
    std::wstring text_;
    std::wstring prevText_;     // dirty-check용
    FontHandle* fontHandle_ = nullptr;
    TextImage*  textImage_  = nullptr;  // GFX::addRequestTextImageLoad로 할당
    bool dirty_ = true;
};
```

**텍스트 렌더링 흐름** (`BasicPlayerHpUI::update` 패턴 재사용):
1. `dirty_`일 때 `gfx.WriteTextToBitmap(textImage_, ...)` → CPU 비트맵 래스터화
2. `gfx.UpdateTextureWithTextImage(textImage_, ...)` → GPU 업로드
3. `onRender`에서 `DrawEvent{ pTex = &textImage_->texture, pCopySrc = &textImage_->textureUpload }` 제출

**주의**: `TextImage` 객체는 GPU 리소스를 포함하므로 프레임 간 유지되어야 한다. 초기화 시 한 번 할당하고, 매 프레임 재할당하지 않는다.

### Button
호버/프레스 시각 상태를 가진 인터랙티브 엘리먼트.

```cpp
class Button : public UIElement {
public:
    Button() { interactive = true; }

    const Texture* texNormal  = nullptr;
    const Texture* texHovered = nullptr;
    const Texture* texPressed = nullptr;

    std::function<void()> onClick;  // 마우스 업 시 호출

    void onRender(GFX& gfx) override;
    void onMouseEnter() override;
    void onMouseLeave() override;
    void onMouseDown(MouseButton btn, float lx, float ly) override;
    void onMouseUp(MouseButton btn, float lx, float ly) override;

private:
    enum class State { Normal, Hovered, Pressed };
    State state_ = State::Normal;
};
```

렌더링 시 현재 `state_`에 따라 해당 텍스처를 선택하여 DrawEvent 제출.

### ProgressBar
수평 fill 바. `BasicPlayerHpUI`를 대체한다.

```cpp
class ProgressBar : public UIElement {
public:
    const Texture* backgroundTex = nullptr;  // 전체 너비 배경
    const Texture* fillTex       = nullptr;  // fill 부분

    void setProgress(float t);  // t: 0~1
    float progress() const;
    void onRender(GFX& gfx) override;

private:
    float progress_ = 1.f;
};
```

**Fill 바 월드 행렬 계산** (`BasicPlayerHpUI::update` 라인 20-22 패턴):
```
fillScaleX    = rect.width * 0.5f * progress_
fillTranslateX = rect.x + rect.width * 0.5f * progress_
// 좌측 기준, 우측에서 줄어듦
```

### Slider
드래그로 값을 조절하는 인터랙티브 위젯.

```cpp
class Slider : public UIElement {
public:
    Slider() { interactive = true; }

    const Texture* trackTex  = nullptr;
    const Texture* handleTex = nullptr;

    float value = 0.5f;           // 0..1
    float handleWidthPx = 16.f;

    std::function<void(float)> onValueChanged;

    void onRender(GFX& gfx) override;
    void onMouseDown(MouseButton btn, float lx, float ly) override;
    void onMouseMove(float lx, float ly) override;
    void onMouseUp(MouseButton btn, float lx, float ly) override;

private:
    bool dragging_ = false;
};
```

렌더링: 트랙(전체 사각형) + 핸들(value 비율 위치에 작은 사각형) 총 2개 DrawEvent.

---

## 6. 입력 처리

### UIManager::onWndMsg

`IGame::receiveWndMsg`에서 호출. UI가 메시지를 소비하면 `true` 반환.

```cpp
bool UIManager::onWndMsg(UINT msg, WPARAM wParam, LPARAM lParam);
```

처리 메시지:
| 메시지 | 용도 |
|--------|------|
| `WM_MOUSEMOVE` | 커서 위치 → 호버 감지 |
| `WM_LBUTTONDOWN` / `WM_LBUTTONUP` | 좌클릭 |
| `WM_RBUTTONDOWN` / `WM_RBUTTONUP` | 우클릭 |
| `WM_KEYDOWN` / `WM_KEYUP` | 포커스된 엘리먼트에 키보드 입력 |

### 히트 테스트

1. `lParam`에서 커서 위치 추출 (`LOWORD`/`HIWORD`)
2. 엘리먼트 트리에서 `visible && interactive`인 것들 수집
3. `zOrder` 내림차순 정렬 (앞→뒤)
4. `resolvedRect_.contains(cursorX, cursorY)`인 첫 번째 엘리먼트에 이벤트 디스패치
5. 인터랙티브 엘리먼트가 히트되면 `true` 반환 → 게임 로직은 이 입력을 무시

### 상태 추적

```cpp
UIElement* hoveredElement_ = nullptr;   // 현재 호버 중인 엘리먼트
UIElement* pressedElement_ = nullptr;   // mouseDown을 받은 엘리먼트 (드래그/릴리즈 추적)
UIElement* focusedElement_ = nullptr;   // 키보드 포커스를 가진 엘리먼트
```

- 커서가 새 엘리먼트로 이동: 이전 → `onMouseLeave()`, 새 → `onMouseEnter()`
- `onMouseDown` 발생 시 focusedElement_ 설정
- `onKeyDown`/`onKeyUp`은 focusedElement_에만 전달

### 게임 통합

`StandAlone::Game::receiveWndMsg`에서:
```cpp
LRESULT Game::receiveWndMsg(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    // UI 시스템이 먼저 처리 시도
    if (uiManager_.onWndMsg(msg, wParam, lParam)) {
        return 0;  // UI가 소비함
    }
    // 기존 입력 처리 (카메라, raw input 등)
    ...
}
```

### 커서 캡처 전환

게임플레이 모드에서는 커서가 캡처되어 숨겨져 있다.
인터랙티브 UI(메뉴 등)를 표시할 때는 커서를 해제해야 한다.

```cpp
// UIManager가 제공
bool UIManager::needsCursor() const;
// 인터랙티브 엘리먼트가 visible인 경우 true 반환
```

게임의 `processInput`에서 이 값을 확인하여 커서 캡처를 토글한다.

---

## 7. UIManager

### 인터페이스

```cpp
class UIManager {
public:
    void setScreenSize(float w, float h);

    UIElement* root();  // 루트 엘리먼트 접근. 모든 UI는 root의 자식.

    // 매 프레임 호출
    void layout();                                       // 트리 전체 레이아웃 재계산
    void update(float deltaTimeSec, GFX& gfx, FontHandle* defaultFont);  // 업데이트
    void render(GFX& gfx);                               // DrawEvent 제출

    // 입력 라우팅
    bool onWndMsg(UINT msg, WPARAM wParam, LPARAM lParam);
    bool needsCursor() const;

private:
    UIElement root_;           // 보이지 않는 루트, 화면 전체를 나타냄
    float screenWidth_  = 1024.f;
    float screenHeight_ = 768.f;

    UIElement* hoveredElement_ = nullptr;
    UIElement* pressedElement_ = nullptr;
    UIElement* focusedElement_ = nullptr;
    float cursorX_ = 0.f;
    float cursorY_ = 0.f;
};
```

### 렌더링 흐름

1. `root_.renderTree(gfx)` → 트리를 zOrder 순서로 순회, 각 엘리먼트 `onRender(gfx)` 호출
2. 각 엘리먼트의 `onRender`가 `gfx.addDrawEvent(UIPipeline::DrawEvent{...})` 호출
3. `UIPipeline`의 기존 렌더링 패스에서 처리됨

---

## 8. 기존 코드 통합

### 변경 없는 시스템

| 시스템 | 이유 |
|--------|------|
| **UIPipeline** | DrawEvent를 생성하기만 하면 됨 |
| **GFX** | 기존 `addDrawEvent`, `WriteTextToBitmap`, `UpdateTextureWithTextImage` 사용 |
| **Font / TextImage** | Label이 `BasicPlayerHpUI`와 동일한 패턴 재사용 |
| **AssetManager** | UI 텍스처도 기존 `addRequestTextureLoad`로 로드 |

### 변경이 필요한 코드

| 대상 | 변경 내용 |
|------|----------|
| `StandAlone::Game` | `UIManager` 멤버 추가, update/render/receiveWndMsg에 연결 |
| `Online::Game` | 동일 |
| `BasicPlayerHpUI` | Phase 2 완료 후 `ProgressBar` + `Label`로 대체, 제거 |

---

## 9. 구현 단계

### Phase 1: Core Framework

**생성 파일:**
- `client/ui/UITypes.hpp`
- `client/ui/UIElement.hpp` + `.cpp`
- `client/ui/UIManager.hpp` + `.cpp`
- `client/ui/widgets/Panel.hpp` + `.cpp`
- `client/ui/widgets/Image.hpp` + `.cpp`

**작업:**
1. `UIElement` 베이스 클래스 구현: 계층 관리, `layout()`, `buildWorldMatrix()`, `updateTree()`, `renderTree()`
2. `UIManager` 구현: 화면 Rect, 레이아웃 패스, 렌더 패스
3. `Panel`, `Image` 위젯 구현
4. 간단한 Image 엘리먼트를 화면 중앙에 배치하여 렌더링 검증

**검증:** 화면 중앙에 앵커된 Image가 기존 크로스헤어와 동일한 위치에 렌더링되는지 확인.

### Phase 2: Text / Data 위젯

**생성 파일:**
- `client/ui/widgets/Label.hpp` + `.cpp`
- `client/ui/widgets/ProgressBar.hpp` + `.cpp`

**작업:**
1. `Label` 구현 (TextImage 통합, dirty-check)
2. `ProgressBar` 구현 (배경 + fill 이중 쿼드 렌더링)
3. `UpdateContext` 전달 구조 구축
4. `BasicPlayerHpUI`를 `ProgressBar` + `Label`로 마이그레이션
5. 기존 `BasicPlayerHpUI` 제거

**검증:** HP바와 텍스트 표시가 기존 구현과 시각적으로 동일한지 확인.

### Phase 3: Interactive 위젯 + 입력

**생성 파일:**
- `client/ui/widgets/Button.hpp` + `.cpp`
- `client/ui/widgets/Slider.hpp` + `.cpp`

**작업:**
1. `UIManager::onWndMsg` 히트테스팅 구현
2. 호버 추적 (`onMouseEnter`/`onMouseLeave`)
3. `Button` 구현 (시각 상태 변경 + `onClick` 콜백)
4. `Slider` 구현 (드래그 인터랙션)
5. `StandAlone::Game::receiveWndMsg` / `Online::Game::receiveWndMsg`에 통합
6. 입력 소비 확인 (UI 클릭이 카메라 회전을 트리거하지 않는지)

**검증:** 버튼 클릭 시 콜백 호출, 슬라이더 드래그 시 값 변경 확인.

### Phase 4: 해상도 대응 및 정리

**작업:**
1. `WM_SIZE` 메시지 처리 → `UIManager::setScreenSize()` 호출
2. 리사이즈 시 트리 전체 re-layout
3. 퍼센트 기반 사이징이 다양한 해상도에서 올바르게 동작하는지 테스트
4. 새 파일에 한국어 주석 없는지 확인 (CLAUDE.md 인코딩 규칙)

---

## 10. 잠재적 문제와 대응

### Y축 방향 불일치
셰이더는 Y=0을 NDC 하단으로 매핑. 레이아웃은 Y=0을 화면 상단으로 사용.
→ `buildWorldMatrix()`에서 `translateY = screenHeight - topDownY`로 변환.
한 함수에 격리되어 있으므로 관리 용이.

### TextImage 수명 관리
`TextImage`는 GPU 리소스를 포함. Label은 안정적인 `TextImage` 포인터를 유지해야 한다.
→ 초기화 시 `GFX::addRequestTextImageLoad`로 한 번 할당, 매 프레임 재할당 금지.

### 드로우 순서와 투명도
알파가 있는 UI 엘리먼트(텍스트, 반투명 패널)는 올바른 드로우 순서 필요.
→ `renderTree`가 zOrder를 준수하고, `UIPipeline`은 알파 블렌딩을 지원.
제출 순서가 곧 시각적 순서.

### 커서 캡처 모드 전환
게임플레이 중 커서가 캡처됨. 메뉴/인터랙티브 UI 표시 시 해제 필요.
→ `UIManager::needsCursor()`로 상태 판단, 게임의 `processInput`에서 커서 캡처 토글.

### 스레드 안전성
`DrawEvent` 생성(`UIManager::render`)은 게임 스레드에서 싱글스레드로 수행.
`GFX::render()` 호출 전에 완료. 기존 패턴과 동일하므로 스레드 안전 문제 없음.

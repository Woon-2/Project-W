# UI 시스템 문서

## 1. 아키텍처 개요

기존 `UIPipeline` (저수준 렌더링) 위에 게임 레벨 UI 추상화 계층을 구축한다.

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
    UITypes.hpp          -- 공유 열거형, 구조체 (Anchor, Rect, TextHAlign 등)
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
    bool isPercent = false;

    float resolve(float parentDim) const;

    static DimValue px(float v);   // 픽셀
    static DimValue pct(float v);  // 퍼센트 (0~100)
};
```

### Rect
레이아웃 계산 후의 절대 픽셀 사각형. **(0,0) = 화면 상단 좌측**, Y 증가 = 아래쪽.

```cpp
struct Rect {
    float x, y, width, height;
    bool contains(float px, float py) const;
};
```

### TextHAlign / TextVAlign
Label 텍스트 정렬 방향.

```cpp
enum class TextHAlign { Leading, Center, Trailing };  // 좌, 중, 우
enum class TextVAlign { Top, Center, Bottom };         // 상, 중, 하
```

---

## 3. 베이스 클래스 (UIElement)

### 컨텍스트 구조체

```cpp
struct UpdateContext {
    float deltaTimeSec = 0.f;
    GFX* gfx = nullptr;
    FontHandle* defaultFont = nullptr;  // gfx.defaultFont() 전달
    float screenWidth  = 1024.f;
    float screenHeight = 768.f;
};

struct RenderContext {
    GFX* gfx = nullptr;
    float screenHeight = 768.f;
};
```

### 인터페이스

```cpp
class UIElement {
public:
    // --- 속성 ---
    std::string name;
    Anchor   anchor     = Anchors::TopLeft;
    Pivot    pivot      = Pivots::TopLeft;
    DimValue offsetX;
    DimValue offsetY;
    DimValue width;
    DimValue height;
    Color    colorTint  = { 1.f, 1.f, 1.f, 1.f };
    int      zOrder     = 0;
    bool     visible    = true;
    bool     interactive = false;

    // --- 계층 ---
    UIElement* addChild(std::unique_ptr<UIElement> child);
    void removeChild(UIElement* child);
    UIElement* findChild(std::string_view name) const;

    // --- 레이아웃 ---
    void layout(const Rect& parentRect);
    const Rect& resolvedRect() const;

    // --- 업데이트/렌더 ---
    virtual void onUpdate(const UpdateContext& ctx) {}
    virtual void onRender(const RenderContext& rc) {}
    void updateTree(const UpdateContext& ctx);
    void renderTree(const RenderContext& rc);

    // --- 입력 콜백 ---
    virtual void onMouseEnter() {}
    virtual void onMouseLeave() {}
    virtual void onMouseDown(MouseButton btn, float localX, float localY) {}
    virtual void onMouseUp(MouseButton btn, float localX, float localY) {}
    virtual void onMouseMove(float localX, float localY) {}
    virtual void onKeyDown(int vkCode) {}
    virtual void onKeyUp(int vkCode) {}

protected:
    mu::Mat4x4 buildWorldMatrix(float screenHeight) const;
    Rect resolvedRect_{};
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

### Anchor와 Pivot 개념

| | 의미 | 비유 |
|---|---|---|
| **Anchor** | 부모의 어느 점에 붙을지 | "어느 벽에 못을 박을지" |
| **Pivot** | 내 박스의 어느 점을 못에 걸지 | "액자의 어느 부분에 걸이 구멍이 있는지" |

`anchor == pivot`으로 맞추면 엘리먼트가 해당 모서리/중앙에 자연스럽게 붙는다.

**예시 — hpLabel (화면 하단 중앙, 1024×768 기준):**
```cpp
anchor = BottomCenter  // {0.5, 1.0} → 기준점 = (512, 768)
pivot  = BottomCenter  // {0.5, 1.0} → 박스 하단 중앙을 기준점에 맞춤
width  = 512px,  height = 256px
offsetY = -10px  // 화면 아래에서 10px 위

// 결과: resolvedRect = {256, 502, 512, 256}
//       박스 하단이 y=758 (화면 바닥 10px 위)
```

**비교 — anchor=Center, pivot=Center (화면 정중앙):**
```cpp
// refX = 512, refY = 384
// finalX = 512 - 0.5*512 = 256
// finalY = 384 - 0.5*256 = 256
// resolvedRect = {256, 256, 512, 256}  → 정중앙
```

**비교 — anchor=TopRight, pivot=TopRight (우상단 고정):**
```cpp
// refX = 1024, refY = 0
// finalX = 1024 - 1.0*512 = 512
// finalY = 0 - 0.0*256 = 0
// resolvedRect = {512, 0, 512, 256}  → 화면 우상단
```

### Y축 변환 (레이아웃 → 셰이더)

레이아웃은 **top-down** 좌표계 (Y=0 = 상단)를 사용한다.
`ui.hlsl`은 픽셀 Y를 NDC로 변환할 때 Y=0 → NDC 하단이 된다.
따라서 `buildWorldMatrix()`에서 Y를 뒤집는다:

```cpp
mu::Vec3 pos{
    resolvedRect_.x + resolvedRect_.width  * 0.5f,
    screenHeight - (resolvedRect_.y + resolvedRect_.height * 0.5f),  // Y 반전
    0.f
};
```

### 레이아웃 사용 예시

**HP바 (화면 하단 중앙):**
```cpp
hpBar->anchor  = Anchors::BottomCenter;
hpBar->pivot   = Pivots::BottomCenter;
hpBar->width   = DimValue::px(1024.f);
hpBar->height  = DimValue::px(64.f);
hpBar->offsetY = DimValue::px(-40.f);   // 하단에서 40px 위
```

**크로스헤어 (화면 정중앙):**
```cpp
crosshair->anchor = Anchors::Center;
crosshair->pivot  = Pivots::Center;
crosshair->width  = DimValue::px(32.f);
crosshair->height = DimValue::px(32.f);
```

**패널 (우측, 화면 너비의 30%):**
```cpp
panel->anchor = Anchors::TopRight;
panel->pivot  = Pivots::TopRight;
panel->width  = DimValue::pct(30.f);
panel->height = DimValue::pct(100.f);
```

---

## 5. 위젯 클래스

### Panel
컨테이너 역할. 선택적으로 배경 텍스처를 렌더링.

```cpp
class Panel : public UIElement {
public:
    const Texture* backgroundTex = nullptr;
    void onRender(const RenderContext& rc) override;
};
```

### Image
단일 텍스처를 표시하는 가장 단순한 렌더러블 엘리먼트.

```cpp
class Image : public UIElement {
public:
    const Texture* texture = nullptr;
    void onRender(const RenderContext& rc) override;
};
```

### Label
`TextImage` + `Font` 시스템을 사용하여 텍스트 렌더링.

```cpp
class Label : public UIElement {
public:
    void setText(const std::wstring& text);
    void setFont(FontHandle* font);

    // 텍스트 정렬 (변경 시 자동으로 dirty → 다음 프레임 재렌더링)
    void setTextHAlign(TextHAlign a);
    void setTextVAlign(TextVAlign a);

    void onUpdate(const UpdateContext& ctx) override;
    void onRender(const RenderContext& rc) override;
};
```

**텍스트 렌더링 흐름:**
1. `onUpdate` 진입 시 `resolvedRect_` 크기가 바뀌었으면 `gfx.createTextImageImmediate(w, h)` 로 `ownedTextImage_` 재생성
2. `dirty_`일 때 `gfx.WriteTextToBitmap(...)` → D2D 래스터화 → `ownedTextImage_.pData`에 기록
3. 정렬 오프셋 계산 후 `pData` 내 픽셀을 이동 (CPU 쪽에서 shift)
4. `gfx.UpdateTextureWithTextImage(...)` → GPU 업로드 버퍼에 복사
5. `onRender`에서 `DrawEvent{ pCopySrc = &textureUpload }` → GPU 업로드 실행

**텍스트 정렬 구현 원리:**
DirectWrite는 항상 LEADING(좌상단)으로 렌더링한다. 정렬은 `pData` 내 픽셀을 원하는 위치로
shift해서 구현한다. 셰이더나 쿼드 크기는 변경되지 않으며, 텍스처 데이터 안에 정렬 위치가 반영된다.

**주의:**
- `TextImage`는 Label이 내부 소유(`ownedTextImage_`). 외부 주입 불필요.
- `update()` 호출 시 `ctx.defaultFont`가 null이면 텍스트 렌더링이 스킵된다.
  `gfx_.defaultFont()`(내장 Tahoma 폰트)를 전달해야 한다.

### Button
호버/프레스 시각 상태를 가진 인터랙티브 엘리먼트.

```cpp
class Button : public UIElement {
public:
    Button() { interactive = true; }

    const Texture* texNormal  = nullptr;
    const Texture* texHovered = nullptr;
    const Texture* texPressed = nullptr;

    std::function<void()> onClick;

    void onRender(const RenderContext& rc) override;
    void onMouseEnter() override;
    void onMouseLeave() override;
    void onMouseDown(MouseButton btn, float lx, float ly) override;
    void onMouseUp(MouseButton btn, float lx, float ly) override;
};
```

### ProgressBar
수평 fill 바.

```cpp
class ProgressBar : public UIElement {
public:
    const Texture* backgroundTex = nullptr;
    const Texture* fillTex       = nullptr;

    void setProgress(float t);  // 0~1
    float progress() const;
    void onRender(const RenderContext& rc) override;
};
```

### Slider
드래그로 값을 조절하는 인터랙티브 위젯.

```cpp
class Slider : public UIElement {
public:
    Slider() { interactive = true; }

    const Texture* trackTex  = nullptr;
    const Texture* handleTex = nullptr;
    float value = 0.5f;
    float handleWidthPx = 16.f;

    std::function<void(float)> onValueChanged;

    void onRender(const RenderContext& rc) override;
    void onMouseDown(MouseButton btn, float lx, float ly) override;
    void onMouseMove(float lx, float ly) override;
    void onMouseUp(MouseButton btn, float lx, float ly) override;
};
```

---

## 6. 입력 처리

### UIManager::onWndMsg

`receiveWndMsg`에서 호출. UI가 메시지를 소비하면 `true` 반환.

처리 메시지:
| 메시지 | 용도 |
|--------|------|
| `WM_MOUSEMOVE` | 커서 위치 → 호버 감지 |
| `WM_LBUTTONDOWN` / `WM_LBUTTONUP` | 좌클릭 |
| `WM_RBUTTONDOWN` / `WM_RBUTTONUP` | 우클릭 |
| `WM_KEYDOWN` / `WM_KEYUP` | 포커스된 엘리먼트에 전달 |

### 히트 테스트

1. `lParam`에서 커서 위치 추출
2. `visible && interactive`인 엘리먼트 수집
3. `zOrder` 내림차순 정렬
4. `resolvedRect_.contains(x, y)`인 첫 번째 엘리먼트에 이벤트 디스패치
5. 히트 시 `true` 반환 → 게임 입력 처리 차단

---

## 7. UIManager

### 인터페이스

```cpp
class UIManager {
public:
    void setScreenSize(float w, float h);
    UIElement* root();

    // 매 프레임 호출 순서
    void layout();
    void update(float deltaTimeSec, GFX& gfx, FontHandle* defaultFont);
    void render(GFX& gfx);

    // 입력
    bool onWndMsg(UINT msg, WPARAM wParam, LPARAM lParam);
    bool needsCursor() const;

    // 디버그 오버레이 (→ 섹션 9 참고)
    void requestDebugResources(GFX& gfx);
    void toggleDebugMode();
    bool debugMode() const;
};
```

### 게임 통합 (StandAlone::Game)

**초기화 (setupStage):**
```cpp
uiManager_.setScreenSize(screenW, screenH);
uiManager_.requestDebugResources(gfx_);  // loadAssets 이후에 호출

auto* label = static_cast<UI::Label*>(
    uiManager_.root()->addChild(std::make_unique<UI::Label>())
);
label->setTextHAlign(UI::TextHAlign::Center);
label->setTextVAlign(UI::TextVAlign::Center);
label->setText(L"...");
```

**매 프레임 (update):**
```cpp
uiManager_.layout();
uiManager_.update(dt, gfx_, gfx_.defaultFont());  // defaultFont 필수
```

**렌더:**
```cpp
uiManager_.render(gfx_);
```

**입력:**
```cpp
LRESULT Game::receiveWndMsg(...) {
    if (uiManager_.onWndMsg(msg, wParam, lParam)) return 0;
    // 기존 입력 처리
}
```

---

## 8. UIPipeline::DrawEvent — colorMul

`DrawEvent`에 `colorMul` 필드가 추가되었다. 기본값은 `{1,1,1,1}` (색상 변화 없음).
셰이더에서 샘플 결과에 곱해진다:

```cpp
struct DrawEvent {
    mu::Mat4x4   world;
    const Texture* pTex;
    const Texture* pCopySrc;
    XMFLOAT4     colorMul = { 1.f, 1.f, 1.f, 1.f };
};
```

```hlsl
// ui.hlsl PSMain
return sampleBindless(material.idxAlbedo, input.uv) * material.cAlbedo;
```

디버그 오버레이가 이 필드를 이용해 색상이 있는 테두리 사각형을 그린다.

---

## 9. 디버그 오버레이

**U 키**로 토글. 활성화 시 모든 visible 엘리먼트의 `resolvedRect_` 위치/크기를 컬러 테두리로 시각화한다.

### 색상 — 계층 깊이(depth)별

| Depth | 색상 |
|-------|------|
| 0 | 하늘색 (cyan) |
| 1 | 노란색 (yellow) |
| 2 | 분홍색 (magenta) |
| 3 | 초록색 (green) |
| 4 | 주황색 (orange) |

### 초기화

`loadAssets()` 이후, `setupStage()`에서 한 번 호출:
```cpp
uiManager_.requestDebugResources(gfx_);
```

내부적으로 1×1 흰색 `TextImage`를 생성하고, 첫 렌더 시 GPU에 업로드한다.
이 텍스처에 `colorMul`을 곱해 원하는 색의 사각형을 그린다.

---

## 10. GFX 추가 API

UI 시스템 구현 과정에서 GFX에 다음 API가 추가되었다.

```cpp
// 내장 Tahoma 폰트 핸들 반환. UIManager::update의 defaultFont로 전달한다.
FontHandle* GFX::defaultFont();

// loadAssets() 이후에 즉시 TextImage를 생성한다.
// 기존 addRequestTextImageLoad(비동기)와 달리 동기적으로 생성된다.
void GFX::createTextImageImmediate(UINT width, UINT height, TextImage* pDest);
```

---

## 11. 잠재적 문제와 대응

### Y축 방향 불일치
셰이더는 Y=0을 NDC 하단으로 매핑. 레이아웃은 Y=0을 화면 상단으로 사용.
→ `buildWorldMatrix(screenHeight)`에서 `translateY = screenHeight - topDownY`로 변환. 한 함수에 격리.

### TextImage 수명 관리
`TextImage`는 GPU 리소스를 포함. Label이 참조하는 TextImage는 Label보다 오래 살아야 한다.
→ AssetManager 또는 게임 객체가 소유, Label은 포인터만 참조.

### Label defaultFont 누락
`update()` 호출 시 `ctx.defaultFont = nullptr`이면 `onUpdate`가 조기 반환되어 텍스트가 렌더링되지 않는다.
→ 반드시 `gfx_.defaultFont()`를 전달한다.

### DirectWrite 정렬 제한
`Font::WriteTextToBitmap`은 항상 LEADING 정렬로 `(0,0)`부터 복사한다.
DirectWrite의 `SetTextAlignment(CENTER)`를 사용하면 텍스트가 D2D 비트맵 중앙에 그려지지만,
복사 영역이 `(0,0)`부터 `metrics.width × metrics.height`이므로 텍스트가 잘린다.
→ DirectWrite 정렬을 변경하지 않고, `onUpdate`에서 `pData` 내 픽셀을 CPU에서 shift하여 정렬 구현.

### 드로우 순서와 투명도
알파가 있는 UI 엘리먼트는 올바른 드로우 순서 필요.
→ `renderTree`가 zOrder를 준수. `UIPipeline`은 알파 블렌딩 지원. 제출 순서 = 시각적 순서.

### 스레드 안전성
`UIManager::render`는 게임 스레드에서 싱글스레드로 수행.
`GFX::render()` 호출 전에 완료되므로 기존 패턴과 동일. 스레드 안전 문제 없음.

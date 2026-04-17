#include "pch.hpp"
#include "UIManager.hpp"
#include "../gfx.hpp"

namespace UI {

void UIManager::setScreenSize(float w, float h) {
    screenWidth_ = w;
    screenHeight_ = h;
}

void UIManager::layout() {
    // Root element must span the full screen so children inherit correct parent dimensions.
    root_.width  = DimValue::pct(100.f);
    root_.height = DimValue::pct(100.f);

    Rect screenRect{ 0.f, 0.f, screenWidth_, screenHeight_ };
    root_.layout(screenRect);
}

void UIManager::update(float deltaTimeSec, GFX& gfx, FontHandle* defaultFont) {
    UpdateContext ctx;
    ctx.deltaTimeSec = deltaTimeSec;
    ctx.gfx = &gfx;
    ctx.defaultFont = defaultFont;
    ctx.screenWidth = screenWidth_;
    ctx.screenHeight = screenHeight_;

    root_.updateTree(ctx);
}

void UIManager::render(GFX& gfx) {
    gfx.addFrameData(UIPipeline::FrameData{
        .screenWidth = screenWidth_,
        .screenHeight = screenHeight_
    });
    RenderContext rc;
    rc.gfx = &gfx;
    rc.screenHeight = screenHeight_;
    root_.renderTree(rc);

    if (debugMode_) {
        renderDebugOverlay(gfx);
    }
}

// --- Input ---

bool UIManager::onWndMsg(UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_MOUSEMOVE: {
        cursorX_ = static_cast<float>(LOWORD(lParam));
        cursorY_ = static_cast<float>(HIWORD(lParam));

        UIElement* hit = hitTest(cursorX_, cursorY_);

        // Hover tracking
        if (hit != hoveredElement_) {
            if (hoveredElement_) hoveredElement_->onMouseLeave();
            hoveredElement_ = hit;
            if (hoveredElement_) hoveredElement_->onMouseEnter();
        }

        if (pressedElement_) {
            float lx = cursorX_ - pressedElement_->resolvedRect().x;
            float ly = cursorY_ - pressedElement_->resolvedRect().y;
            pressedElement_->onMouseMove(lx, ly);
            return true;
        }

        return hit != nullptr;
    }

    case WM_LBUTTONDOWN: {
        UIElement* hit = hitTest(cursorX_, cursorY_);
        if (hit) {
            pressedElement_ = hit;
            focusedElement_ = hit;
            float lx = cursorX_ - hit->resolvedRect().x;
            float ly = cursorY_ - hit->resolvedRect().y;
            hit->onMouseDown(MouseButton::Left, lx, ly);
            return true;
        }
        return false;
    }

    case WM_LBUTTONUP: {
        if (pressedElement_) {
            float lx = cursorX_ - pressedElement_->resolvedRect().x;
            float ly = cursorY_ - pressedElement_->resolvedRect().y;
            pressedElement_->onMouseUp(MouseButton::Left, lx, ly);
            pressedElement_ = nullptr;
            return true;
        }
        return false;
    }

    case WM_RBUTTONDOWN: {
        UIElement* hit = hitTest(cursorX_, cursorY_);
        if (hit) {
            float lx = cursorX_ - hit->resolvedRect().x;
            float ly = cursorY_ - hit->resolvedRect().y;
            hit->onMouseDown(MouseButton::Right, lx, ly);
            return true;
        }
        return false;
    }

    case WM_RBUTTONUP: {
        UIElement* hit = hitTest(cursorX_, cursorY_);
        if (hit) {
            float lx = cursorX_ - hit->resolvedRect().x;
            float ly = cursorY_ - hit->resolvedRect().y;
            hit->onMouseUp(MouseButton::Right, lx, ly);
            return true;
        }
        return false;
    }

    case WM_KEYDOWN: {
        if (focusedElement_) {
            focusedElement_->onKeyDown(static_cast<int>(wParam));
            return true;
        }
        return false;
    }

    case WM_KEYUP: {
        if (focusedElement_) {
            focusedElement_->onKeyUp(static_cast<int>(wParam));
            return true;
        }
        return false;
    }

    default:
        return false;
    }
}

bool UIManager::needsCursor() const {
    struct Finder {
        static bool any(const UIElement& e) {
            if (!e.visible) return false;
            if (e.interactive) return true;
            for (const auto& c : e.children())
                if (any(*c)) return true;
            return false;
        }
    };
    return Finder::any(root_);
}

UIElement* UIManager::hitTest(float x, float y) {
    std::vector<UIElement*> interactives;
    collectInteractive(&root_, interactives);

    // Sort front-to-back (higher zOrder first).
    std::ranges::sort(interactives, std::greater{}, &UIElement::zOrder);

    for (auto* elem : interactives) {
        if (elem->resolvedRect().contains(x, y)) {
            return elem;
        }
    }
    return nullptr;
}

void UIManager::collectInteractive(UIElement* elem, std::vector<UIElement*>& out) {
    if (!elem->visible) return;

    if (elem->interactive) {
        out.push_back(elem);
    }
    for (auto& child : elem->children()) {
        collectInteractive(child.get(), out);
    }
}

// --- Debug overlay ---

void UIManager::requestDebugResources(GFX& gfx) {
    gfx.createTextImageImmediate(1, 1, &debugWhiteTex_);
}

void UIManager::renderDebugOverlay(GFX& gfx) {
    // Lazy-initialize: fill white pixel and stage for GPU upload on first use.
    if (!debugWhiteTexReady_) {
        if (debugWhiteTex_.pData.empty()) return;  // not yet created
        std::ranges::fill(debugWhiteTex_.pData, static_cast<BYTE>(0xFF));
        gfx.UpdateTextureWithTextImage(&debugWhiteTex_, 1, 1);
        debugWhiteTexReady_ = true;
        debugWhiteTexNeedsCopy_ = true;
    }

    // Per-depth border colors (RGBA).
    static constexpr XMFLOAT4 kColors[] = {
        { 0.f, 1.f, 1.f, 1.f },   // depth 0 — cyan
        { 1.f, 1.f, 0.f, 1.f },   // depth 1 — yellow
        { 1.f, 0.f, 1.f, 1.f },   // depth 2 — magenta
        { 0.f, 1.f, 0.f, 1.f },   // depth 3 — green
        { 1.f, 0.5f, 0.f, 1.f },  // depth 4 — orange
    };
    constexpr int kColorCount = static_cast<int>(std::size(kColors));

    // Collect all visible elements (DFS, excluding root itself).
    struct Entry { UIElement* elem; int depth; };
    std::vector<Entry> entries;
    std::function<void(UIElement*, int)> collect = [&](UIElement* e, int d) {
        if (!e->visible) return;
        entries.push_back({ e, d });
        for (auto& c : e->children()) collect(c.get(), d + 1);
    };
    for (auto& c : root_.children()) collect(c.get(), 0);

    constexpr float kBorder = 2.f;

    // Build world matrix for an arbitrary rect (mirrors UIElement::buildWorldMatrix).
    auto makeWorld = [&](float x, float y, float w, float h) -> mu::Mat4x4 {
        mu::Vec3 scl{ w * 0.5f, h * 0.5f, 1.f };
        mu::Vec3 pos{ x + w * 0.5f, screenHeight_ - (y + h * 0.5f), 0.f };
        return mu::Mat4x4(mu::scale(scl)) * mu::translate(pos);
    };

    bool firstDraw = debugWhiteTexNeedsCopy_;

    for (auto& [elem, depth] : entries) {
        const Rect& r = elem->resolvedRect();
        if (r.width < 1.f || r.height < 1.f) continue;

        const XMFLOAT4& col = kColors[depth % kColorCount];

        auto addBorderQuad = [&](float bx, float by, float bw, float bh) {
            if (bw < 0.01f || bh < 0.01f) return;
            gfx.addDrawEvent(UIPipeline::DrawEvent{
                .world    = makeWorld(bx, by, bw, bh),
                .pTex     = &debugWhiteTex_.texture,
                .pCopySrc = firstDraw ? &debugWhiteTex_.textureUpload : nullptr,
                .colorMul = col,
            });
            firstDraw = false;
        };

        float t = kBorder;
        addBorderQuad(r.x,               r.y,                r.width,        t);             // top
        addBorderQuad(r.x,               r.y + r.height - t, r.width,        t);             // bottom
        addBorderQuad(r.x,               r.y + t,            t,              r.height - 2*t); // left
        addBorderQuad(r.x + r.width - t, r.y + t,            t,              r.height - 2*t); // right
    }

    debugWhiteTexNeedsCopy_ = false;
}

}   // namespace UI

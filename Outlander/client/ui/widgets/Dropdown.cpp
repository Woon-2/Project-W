#include "pch.hpp"
#include "Dropdown.hpp"
#include "Label.hpp"
#include "Button.hpp"
#include "../../gfx.hpp"
#include "../../uiPipeline.hpp"

namespace UI {

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

mu::Mat4x4 Dropdown::makeSubQuadWorld(float rx, float ry, float rw, float rh,
                                      float screenH) const {
    mu::Vec3 scl{ std::round(rw * 0.5f), std::round(rh * 0.5f), 1.f };
    mu::Vec3 pos{
        std::round(rx + rw * 0.5f),
        std::round(screenH - (ry + rh * 0.5f)),
        0.f
    };
    return mu::Mat4x4(mu::scale(scl)) * mu::translate(pos);
}

// ---------------------------------------------------------------------------
// State
// ---------------------------------------------------------------------------

void Dropdown::toggle() {
    open_ = !open_;
    syncVisibility();
}

void Dropdown::select(int idx) {
    selectedIdx_ = idx;
    open_        = false;
    syncVisibility();
    syncLabel();
    if (onSelectionChanged) onSelectionChanged(selectedIdx_);
}

void Dropdown::syncVisibility() {
    float totalH = open_
        ? headerHeight + static_cast<float>(items_.size()) * itemHeight
        : headerHeight;
    height = DimValue::px(totalH);

    if (listRoot_) listRoot_->visible = open_;
}

void Dropdown::syncLabel() {
    if (labelDisplay_ && !items_.empty()) {
        std::wstring ws(items_[selectedIdx_].begin(), items_[selectedIdx_].end());
        labelDisplay_->setText(ws);
    }
}

// ---------------------------------------------------------------------------
// Setup
// ---------------------------------------------------------------------------

void Dropdown::setup(std::vector<std::string> items) {
    items_       = std::move(items);
    interactive  = false;  // children handle all input
    zOrder       = 100;

    // --- Header label (current selection) ---
    {
        auto lbl = std::make_unique<Label>();
        lbl->anchor  = Anchors::TopLeft;
        lbl->pivot   = Pivots::TopLeft;
        lbl->offsetX = DimValue::px(8.f);
        lbl->offsetY = DimValue::px(0.f);
        lbl->width   = DimValue::pct(100.f);  // toggle button overlaps right edge; text is left-aligned so it stays readable
        lbl->height  = DimValue::px(headerHeight);
        lbl->setFontSize(14.f);
        lbl->setTextVAlign(TextVAlign::Center);
        lbl->setTextHAlign(TextHAlign::Leading);
        lbl->setTextColor(0.f, 0.f, 0.f, 1.f);  // black text on white bg
        labelDisplay_ = static_cast<Label*>(addChild(std::move(lbl)));
    }

    // --- Toggle button (blue square on the right) ---
    {
        auto btn = std::make_unique<Button>();
        btn->anchor         = Anchors::TopRight;
        btn->pivot          = Pivots::TopRight;
        btn->offsetX        = DimValue::px(0.f);
        btn->offsetY        = DimValue::px(0.f);
        btn->width          = DimValue::px(toggleWidth);
        btn->height         = DimValue::px(headerHeight);
        btn->bgColor        = { 0.20f, 0.50f, 1.00f, 1.00f };
        btn->bgColorHovered = { 0.30f, 0.62f, 1.00f, 1.00f };
        btn->bgColorPressed = { 0.10f, 0.38f, 0.88f, 1.00f };
        btn->onClick = [this] { toggle(); };
        btnToggle_ = static_cast<Button*>(addChild(std::move(btn)));
    }

    // --- List container (hidden when closed) ---
    {
        auto root   = std::make_unique<UIElement>();
        root->anchor  = Anchors::TopLeft;
        root->pivot   = Pivots::TopLeft;
        root->offsetX = DimValue::px(0.f);
        root->offsetY = DimValue::px(headerHeight);
        root->width   = DimValue::pct(100.f);
        root->height  = DimValue::px(static_cast<float>(items_.size()) * itemHeight);
        root->visible = false;
        listRoot_ = addChild(std::move(root));

        for (int i = 0; i < static_cast<int>(items_.size()); ++i) {
            auto btn    = std::make_unique<Button>();
            btn->anchor  = Anchors::TopLeft;
            btn->pivot   = Pivots::TopLeft;
            btn->offsetX = DimValue::px(0.f);
            btn->offsetY = DimValue::px(static_cast<float>(i) * itemHeight);
            btn->width   = DimValue::pct(100.f);
            btn->height  = DimValue::px(itemHeight);
            btn->bgColor        = { 0.92f, 0.92f, 0.92f, 1.00f };
            btn->bgColorHovered = { 0.70f, 0.82f, 1.00f, 1.00f };
            btn->bgColorPressed = { 0.55f, 0.72f, 1.00f, 1.00f };

            // Item label (child of button)
            {
                auto lbl     = std::make_unique<Label>();
                lbl->anchor  = Anchors::TopLeft;
                lbl->pivot   = Pivots::TopLeft;
                lbl->offsetX = DimValue::px(8.f);
                lbl->offsetY = DimValue::px(0.f);
                lbl->width   = DimValue::pct(100.f);
                lbl->height  = DimValue::pct(100.f);
                lbl->setFontSize(13.f);
                lbl->setTextVAlign(TextVAlign::Center);
                lbl->setTextHAlign(TextHAlign::Leading);
                lbl->setTextColor(0.f, 0.f, 0.f, 1.f);
                std::wstring ws(items_[i].begin(), items_[i].end());
                lbl->setText(ws);
                btn->addChild(std::move(lbl));
            }

            int captured = i;
            btn->onClick = [this, captured] { select(captured); };
            itemBtns_.push_back(static_cast<Button*>(listRoot_->addChild(std::move(btn))));
        }
    }

    syncLabel();
    syncVisibility();
}

// ---------------------------------------------------------------------------
// Render
// ---------------------------------------------------------------------------

void Dropdown::onRender(const RenderContext& rc) {
    const auto& r       = resolvedRect_;
    const Texture* wTex = rc.gfx->solidColorTex();
    const float headerH = headerHeight * rc.uiScale;

    // White header background (behind label + toggle button)
    rc.gfx->addDrawEvent(UIPipeline::DrawEvent{
        .world    = makeSubQuadWorld(r.x, r.y, r.width, headerH, rc.screenHeight),
        .pTex     = wTex,
        .pCopySrc = nullptr,
        .colorMul = headerBgColor
    });

    // List background strip (behind item buttons)
    if (open_) {
        float listH = static_cast<float>(items_.size()) * itemHeight * rc.uiScale;
        rc.gfx->addDrawEvent(UIPipeline::DrawEvent{
            .world    = makeSubQuadWorld(r.x, r.y + headerH, r.width, listH,
                                        rc.screenHeight),
            .pTex     = wTex,
            .pCopySrc = nullptr,
            .colorMul = listBgColor
        });
    }
    // Child buttons and labels are rendered by renderTree() after onRender().
}

}   // namespace UI

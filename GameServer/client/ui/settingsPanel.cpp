#include "pch.hpp"
#include "settingsPanel.hpp"

#include "uiBuild.hpp"
#include "UIManager.hpp"
#include "widgets/Button.hpp"
#include "widgets/Label.hpp"
#include "../gfx.hpp"

#include <functional>

namespace UI {

using Build::applyRect;
using Build::addSolid;
using Build::addLabel;
using Build::addButton;

void SettingsPanel::build(UIManager& uiManager, const Texture* panelTex,
                          const Texture* buttonTex, GameSettings& settings) {
    settings_ = &settings;

    const float screenW = uiManager.screenWidth();
    const float screenH = uiManager.screenHeight();

    // Full-screen overlay root. Hidden until open(); high zOrder so it sits above
    // the lobby / in-game content (siblings under uiManager.root()).
    root_ = uiManager.root()->addChild(std::make_unique<UIElement>());
    root_->name    = "settingsRoot";
    root_->anchor  = Anchors::TopLeft;
    root_->pivot   = Pivots::TopLeft;
    root_->offsetX = DimValue::px(0.f);
    root_->offsetY = DimValue::px(0.f);
    root_->width   = DimValue::px(screenW);
    root_->height  = DimValue::px(screenH);
    root_->zOrder  = 50;
    root_->visible = false;

    // Scrim + 9-slice panel backdrop. Interactive so it absorbs clicks meant for
    // the controls behind it.
    auto* scrim = static_cast<Button*>(root_->addChild(std::make_unique<Button>()));
    scrim->name    = "settingsPanelBackground";
    scrim->anchor  = Anchors::TopLeft;
    scrim->pivot   = Pivots::TopLeft;
    scrim->offsetX = DimValue::px(0.f);
    scrim->offsetY = DimValue::px(0.f);
    scrim->width   = DimValue::pct(100.f);
    scrim->height  = DimValue::pct(100.f);
    scrim->interactive = true;
    scrim->bgColor = { 0.08f, 0.13f, 0.17f, 0.34f };
    scrim->zOrder  = 18;
    if (panelTex && panelTex->res) {
        scrim->texNormal       = panelTex;
        scrim->sliceUvBorderX  = 0.30f;
        scrim->sliceUvBorderY  = 0.30f;
        scrim->sliceCornerX    = 40.f;
        scrim->sliceCornerY    = 40.f;
        scrim->texTint         = { 1.f, 1.f, 1.f, 0.90f };
        scrim->texTintHovered  = scrim->texTint;
        scrim->texTintPressed  = scrim->texTint;
    }

    const XMFLOAT4 settingsInk      = { 0.08f, 0.12f, 0.16f, 1.f };
    const XMFLOAT4 settingsRow      = { 0.02f, 0.04f, 0.05f, 0.18f };
    const XMFLOAT4 settingsLine     = { 0.06f, 0.12f, 0.16f, 0.28f };
    const XMFLOAT4 settingsAccent   = { 0.05f, 0.48f, 0.66f, 0.92f };
    const XMFLOAT4 settingsInactive = { 0.10f, 0.13f, 0.15f, 0.18f };
    const XMFLOAT4 settingsButtonText = { 1.f, 1.f, 1.f, 1.f };
    const XMFLOAT4 surfaceSoft      = { 0.953f, 0.976f, 0.992f, 1.f };
    const XMFLOAT4 primarySoft      = { 0.847f, 0.937f, 0.988f, 1.f };

    const float titleX       = screenW * 0.08f;
    const float groupX       = screenW * 0.16f;
    const float rowX         = screenW * 0.23f;
    const float contentRight = screenW * 0.88f;
    const float rowW         = contentRight - rowX;
    const float settingsY    = screenH * 0.14f;
    const float rowH         = std::max(44.f, screenH * 0.052f);
    const float rowGap       = 7.f;
    const float groupGap     = screenH * 0.06f;
    const float nameW        = rowW * 0.43f;
    const float controlX     = rowX + nameW;
    const float controlW     = rowW - nameW;

    currentResolutionText_ =
        std::to_wstring(static_cast<int>(screenW)) + L" × " + std::to_wstring(static_cast<int>(screenH));

    addLabel(scrim, L"설정", titleX, settingsY - 58.f, 42.f, rowW, 58.f,
        settingsInk, TextHAlign::Leading, 2);

    auto makeDivider = [&](float y) {
        addSolid(scrim, "settingsDivider", groupX, y, contentRight - groupX, 2.f, settingsLine, 1);
    };

    auto makeGroupTitle = [&](const std::wstring& title, float y) {
        addLabel(scrim, title, groupX, y, 26.f, contentRight - groupX, 36.f,
            settingsInk, TextHAlign::Leading, 2);
        makeDivider(y + 42.f);
    };

    auto makeSettingRow = [&](const std::wstring& name, float y) {
        addSolid(scrim, "settingsRowBg", rowX, y, rowW, rowH, settingsRow, 1);
        addLabel(scrim, name, rowX + 24.f, y, 19.f, nameW - 32.f, rowH,
            settingsInk, TextHAlign::Leading, 3);
    };

    auto makeToggleButtons = [&](float x, float y, float w, float h,
                                 const std::wstring& left, const std::wstring& right,
                                 std::function<void()> onLeft, std::function<void()> onRight,
                                 Button** leftOut, Button** rightOut) {
        const float halfW = (w - 8.f) * 0.5f;
        auto* leftBtn = addButton(scrim, left, x, y + 7.f, halfW, h - 14.f,
            settingsInactive, settingsAccent, settingsAccent, 17.f, std::move(onLeft), settingsButtonText);
        auto* rightBtn = addButton(scrim, right, x + halfW + 8.f, y + 7.f, halfW, h - 14.f,
            settingsInactive, settingsAccent, settingsAccent, 17.f, std::move(onRight), settingsButtonText);
        leftBtn->zOrder = 22;
        rightBtn->zOrder = 22;
        *leftOut = leftBtn;
        *rightOut = rightBtn;
    };

    auto makeArrowButton = [&](float x, float y, const std::wstring& text, std::function<void()> onClick) {
        auto* btn = addButton(scrim, text, x, y + 6.f, 46.f, rowH - 12.f,
            settingsInactive, settingsAccent, settingsAccent, 22.f, std::move(onClick), settingsButtonText);
        btn->zOrder = 22;
        return btn;
    };

    float groupY = settingsY;
    makeGroupTitle(L"화면", groupY);
    float rowY = groupY + 56.f;
    makeSettingRow(L"화면 모드", rowY);
    makeToggleButtons(controlX + 24.f, rowY, controlW - 48.f, rowH,
        L"창모드", L"전체화면",
        [this]() { settings_->fullscreen = false; refreshPreview(); },
        [this]() { settings_->fullscreen = true;  refreshPreview(); },
        &windowedButton_, &fullscreenButton_);
    rowY += rowH + rowGap;
    makeSettingRow(L"해상도", rowY);
    resolutionPrevButton_ = makeArrowButton(controlX + 16.f, rowY, L"<", [this]() {
        settings_->resolutionIndex = std::max(0, settings_->resolutionIndex - 1);
        refreshPreview();
    });
    resolutionNextButton_ = makeArrowButton(controlX + controlW - 62.f, rowY, L">", [this]() {
        settings_->resolutionIndex = std::min(2, settings_->resolutionIndex + 1);
        refreshPreview();
    });
    resolutionValueLabel_ = addLabel(scrim, L"", controlX + 72.f, rowY, 20.f,
        controlW - 144.f, rowH, settingsInk, TextHAlign::Center, 3);
    addSolid(scrim, "settingsResolutionLine",
        controlX + 78.f, rowY + rowH - 9.f, controlW - 156.f, 2.f, settingsLine, 2);

    groupY = rowY + rowH + groupGap;
    makeGroupTitle(L"전투 표시", groupY);
    rowY = groupY + 56.f;
    makeSettingRow(L"동료 플레이어 데미지 표시", rowY);
    makeToggleButtons(controlX + 24.f, rowY, controlW - 48.f, rowH,
        L"OFF", L"ON",
        [this]() { settings_->allyDamageVisible = false; refreshPreview(); },
        [this]() { settings_->allyDamageVisible = true;  refreshPreview(); },
        &allyDamageOffButton_, &allyDamageOnButton_);
    rowY += rowH + rowGap;
    makeSettingRow(L"몬스터 데미지 투명도", rowY);
    monsterOpacityPrevButton_ = makeArrowButton(controlX + 16.f, rowY, L"<", [this]() {
        settings_->monsterDamageOpacity = std::max(0, settings_->monsterDamageOpacity - 5);
        refreshPreview();
    });
    monsterOpacityNextButton_ = makeArrowButton(controlX + controlW - 62.f, rowY, L">", [this]() {
        settings_->monsterDamageOpacity = std::min(100, settings_->monsterDamageOpacity + 5);
        refreshPreview();
    });
    addSolid(scrim, "settingsMonsterOpacityTrack",
        controlX + 78.f, rowY + rowH - 11.f, controlW - 156.f, 3.f, settingsLine, 2);
    monsterOpacityFillMaxWidth_ = controlW - 156.f;
    monsterOpacityFill_ = addSolid(scrim, "settingsMonsterOpacityFill",
        controlX + 78.f, rowY + rowH - 12.f, monsterOpacityFillMaxWidth_, 5.f, settingsAccent, 3);
    monsterOpacityValueLabel_ = addLabel(scrim, L"", controlX + 72.f, rowY, 20.f,
        controlW - 144.f, rowH * 0.60f, settingsInk, TextHAlign::Center, 3);

    // Close button (bottom-right). Sibling of the scrim so it draws on top.
    const XMFLOAT4 ink = { 0.090f, 0.125f, 0.200f, 1.f };
    closeButton_ = addButton(root_, L"닫기", 0.f, 0.f, 154.f, 52.f,
        surfaceSoft, { 0.847f, 0.937f, 0.988f, 1.f }, primarySoft, 20.f,
        [this]() { close(); }, ink);
    applyRect(closeButton_, Anchors::BottomRight, Pivots::BottomRight, -48.f, -38.f, 154.f, 52.f);
    closeButton_->zOrder = 19;
    if (buttonTex && buttonTex->res) {
        closeButton_->texNormal      = buttonTex;
        closeButton_->sliceUvBorderX = 0.40f;
        closeButton_->sliceUvBorderY = 0.40f;
        closeButton_->sliceCornerX   = 22.f;
        closeButton_->sliceCornerY   = 22.f;
    }

    refreshPreview();
}

void SettingsPanel::open() {
    if (root_) root_->visible = true;
    open_ = true;
}

void SettingsPanel::close() {
    if (root_) root_->visible = false;
    open_ = false;
}

void SettingsPanel::toggle() {
    if (open_) close();
    else       open();
}

void SettingsPanel::refreshPreview() {
    if (!settings_) return;

    const XMFLOAT4 selected      = { 0.05f, 0.48f, 0.66f, 0.92f };
    const XMFLOAT4 selectedHover = { 0.07f, 0.58f, 0.78f, 0.96f };
    const XMFLOAT4 inactive      = { 0.10f, 0.13f, 0.15f, 0.18f };
    const XMFLOAT4 inactiveHover = { 0.10f, 0.13f, 0.15f, 0.28f };
    const XMFLOAT4 disabled      = { 0.10f, 0.12f, 0.13f, 0.10f };

    auto paintButton = [&](Button* btn, bool isSelected, bool isEnabled = true) {
        if (!btn) return;
        btn->interactive = isEnabled;
        if (!isEnabled) {
            btn->bgColor        = disabled;
            btn->bgColorHovered = disabled;
            btn->bgColorPressed = disabled;
            return;
        }
        btn->bgColor        = isSelected ? selected : inactive;
        btn->bgColorHovered = isSelected ? selectedHover : inactiveHover;
        btn->bgColorPressed = selectedHover;
    };

    paintButton(windowedButton_,   !settings_->fullscreen);
    paintButton(fullscreenButton_,  settings_->fullscreen);
    paintButton(allyDamageOffButton_, !settings_->allyDamageVisible);
    paintButton(allyDamageOnButton_,   settings_->allyDamageVisible);

    const bool resolutionEnabled = !settings_->fullscreen;
    paintButton(resolutionPrevButton_, false, resolutionEnabled && settings_->resolutionIndex > 0);
    paintButton(resolutionNextButton_, false, resolutionEnabled && settings_->resolutionIndex < 2);

    if (resolutionValueLabel_) {
        std::wstring resolutionText = currentResolutionText_;
        if (settings_->resolutionIndex == 1) {
            resolutionText = L"1280 × 720";
        } else if (settings_->resolutionIndex == 2) {
            resolutionText = L"1920 × 1080";
        }
        resolutionValueLabel_->setText(resolutionText);
        if (resolutionEnabled) {
            resolutionValueLabel_->setTextColor(0.08f, 0.12f, 0.16f, 1.f);
        } else {
            resolutionValueLabel_->setTextColor(0.38f, 0.43f, 0.48f, 1.f);
        }
    }

    paintButton(monsterOpacityPrevButton_, false, settings_->monsterDamageOpacity > 0);
    paintButton(monsterOpacityNextButton_, false, settings_->monsterDamageOpacity < 100);

    if (monsterOpacityValueLabel_) {
        monsterOpacityValueLabel_->setText(std::to_wstring(settings_->monsterDamageOpacity) + L"%");
    }
    if (monsterOpacityFill_) {
        const float t = static_cast<float>(settings_->monsterDamageOpacity) / 100.f;
        monsterOpacityFill_->width = DimValue::px(std::max(0.f, monsterOpacityFillMaxWidth_ * t));
    }
}

}   // namespace UI

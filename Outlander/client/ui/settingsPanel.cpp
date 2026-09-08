#include "pch.hpp"
#include "settingsPanel.hpp"

#include "uiBuild.hpp"
#include "UIManager.hpp"
#include "widgets/Button.hpp"
#include "widgets/Label.hpp"
#include "widgets/ScrollView.hpp"
#include "../gfx.hpp"

#include <functional>

namespace UI {

using Build::applyRect;
using Build::addSolid;
using Build::addLabel;
using Build::addButton;

namespace {

// Keep the settings window above every in-game HUD/overlay. In particular,
// DialogueSystem uses 10000, while tactical dialogue and status icons use lower
// values. A distinct value also avoids unstable ordering between equal-z siblings.
constexpr int kSettingsOverlayZOrder = 20000;

} // namespace

void SettingsPanel::build(UIManager& uiManager, const Texture* panelTex,
                          const Texture* buttonTex, GameSettings& settings,
                          const std::vector<Resolution>& resolutions,
                          std::function<void()> onQuitGame) {
    settings_ = &settings;
    resolutions_ = resolutions;
    onQuitGame_ = std::move(onQuitGame);

    // Idempotent: on a rebuild (e.g. resolution change) drop the previous subtree.
    // open_ resets to false; the caller re-opens if the panel should stay visible.
    if (root_) {
        uiManager.root()->removeChild(root_);
        root_ = nullptr;
        open_ = false;
    }

    const float screenW = uiManager.layoutWidth();
    const float screenH = uiManager.layoutHeight();

    // Full-screen overlay root. Hidden until open(); topmost zOrder keeps every
    // lobby / in-game overlay behind the settings window.
    root_ = uiManager.root()->addChild(std::make_unique<UIElement>());
    root_->name    = "settingsRoot";
    root_->anchor  = Anchors::TopLeft;
    root_->pivot   = Pivots::TopLeft;
    root_->offsetX = DimValue::px(0.f);
    root_->offsetY = DimValue::px(0.f);
    root_->width   = DimValue::px(screenW);
    root_->height  = DimValue::px(screenH);
    root_->zOrder  = kSettingsOverlayZOrder;
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
    const float settingsY    = screenH * 0.10f;
    const float rowH         = std::max(44.f, screenH * 0.052f);
    const float rowGap       = 7.f;
    const float groupGap     = screenH * 0.04f;
    const float nameW        = rowW * 0.43f;
    const float controlX     = rowX + nameW;
    const float controlW     = rowW - nameW;

    addLabel(scrim, L"설정", titleX, settingsY - 58.f, 42.f, rowW, 58.f,
        settingsInk, TextHAlign::Leading, 2);

    // Scrollable content region between the fixed title and the fixed close button.
    // Windowed (smaller) resolutions can't fit every row, so the rows live inside a
    // ScrollView that clips + wheel-scrolls; fullscreen simply never needs to scroll.
    const float viewTop    = settingsY;
    const float viewBottom = screenH - 100.f;
    const float viewH      = std::max(120.f, viewBottom - viewTop);
    const float contentPad = 6.f;

    auto* scrollView = static_cast<ScrollView*>(scrim->addChild(std::make_unique<ScrollView>()));
    scrollView->name    = "settingsScroll";
    scrollView->anchor  = Anchors::TopLeft;
    scrollView->pivot   = Pivots::TopLeft;
    scrollView->offsetX = DimValue::px(0.f);
    scrollView->offsetY = DimValue::px(viewTop);
    scrollView->width   = DimValue::px(screenW);
    scrollView->height  = DimValue::px(viewH);
    scrollView->zOrder  = 20;
    UIElement* host = scrollView->content();

    auto makeDivider = [&](float y) {
        addSolid(host, "settingsDivider", groupX, y, contentRight - groupX, 2.f, settingsLine, 1);
    };

    auto makeGroupTitle = [&](const std::wstring& title, float y) {
        addLabel(host, title, groupX, y, 26.f, contentRight - groupX, 36.f,
            settingsInk, TextHAlign::Leading, 2);
        makeDivider(y + 42.f);
    };

    auto makeSettingRow = [&](const std::wstring& name, float y) {
        addSolid(host, "settingsRowBg", rowX, y, rowW, rowH, settingsRow, 1);
        addLabel(host, name, rowX + 24.f, y, 19.f, nameW - 32.f, rowH,
            settingsInk, TextHAlign::Leading, 3);
    };

    auto makeToggleButtons = [&](float x, float y, float w, float h,
                                 const std::wstring& left, const std::wstring& right,
                                 std::function<void()> onLeft, std::function<void()> onRight,
                                 Button** leftOut, Button** rightOut) {
        const float halfW = (w - 8.f) * 0.5f;
        auto* leftBtn = addButton(host, left, x, y + 7.f, halfW, h - 14.f,
            settingsInactive, settingsAccent, settingsAccent, 17.f, std::move(onLeft), settingsButtonText);
        auto* rightBtn = addButton(host, right, x + halfW + 8.f, y + 7.f, halfW, h - 14.f,
            settingsInactive, settingsAccent, settingsAccent, 17.f, std::move(onRight), settingsButtonText);
        leftBtn->zOrder = 22;
        rightBtn->zOrder = 22;
        *leftOut = leftBtn;
        *rightOut = rightBtn;
    };

    auto makeArrowButton = [&](float x, float y, const std::wstring& text, std::function<void()> onClick) {
        auto* btn = addButton(host, text, x, y + 6.f, 46.f, rowH - 12.f,
            settingsInactive, settingsAccent, settingsAccent, 22.f, std::move(onClick), settingsButtonText);
        btn->zOrder = 22;
        return btn;
    };

    // A < > stepper row with a track+fill bar and a percent label (0..100). Used by
    // the volume controls; mirrors the monster-damage-opacity row pattern.
    auto makeStepperRow = [&](const std::wstring& name, float y, int step,
                              std::function<int()> get, std::function<void(int)> set,
                              Button** prevOut, Button** nextOut, Button** fillOut, Label** labelOut) {
        makeSettingRow(name, y);
        *prevOut = makeArrowButton(controlX + 16.f, y, L"<", [this, get, set, step]() {
            set(std::max(0, get() - step));
            refreshPreview();
        });
        *nextOut = makeArrowButton(controlX + controlW - 62.f, y, L">", [this, get, set, step]() {
            set(std::min(100, get() + step));
            refreshPreview();
        });
        addSolid(host, "settingsStepperTrack",
            controlX + 78.f, y + rowH - 11.f, controlW - 156.f, 3.f, settingsLine, 2);
        volumeFillMaxWidth_ = controlW - 156.f;
        *fillOut = addSolid(host, "settingsStepperFill",
            controlX + 78.f, y + rowH - 12.f, volumeFillMaxWidth_, 5.f, settingsAccent, 3);
        *labelOut = addLabel(host, L"", controlX + 72.f, y, 20.f,
            controlW - 144.f, rowH * 0.60f, settingsInk, TextHAlign::Center, 3);
    };

    float groupY = contentPad;
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
        const int lastIdx = resolutions_.empty() ? 0 : static_cast<int>(resolutions_.size()) - 1;
        settings_->resolutionIndex = std::min(lastIdx, settings_->resolutionIndex + 1);
        refreshPreview();
    });
    resolutionValueLabel_ = addLabel(host, L"", controlX + 72.f, rowY, 20.f,
        controlW - 144.f, rowH, settingsInk, TextHAlign::Center, 3);
    addSolid(host, "settingsResolutionLine",
        controlX + 78.f, rowY + rowH - 9.f, controlW - 156.f, 2.f, settingsLine, 2);
    rowY += rowH + rowGap;
    makeSettingRow(L"수직 동기화", rowY);
    makeToggleButtons(controlX + 24.f, rowY, controlW - 48.f, rowH,
        L"OFF", L"ON",
        [this]() { settings_->vsync = false; refreshPreview(); },
        [this]() { settings_->vsync = true;  refreshPreview(); },
        &vsyncOffButton_, &vsyncOnButton_);

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
    addSolid(host, "settingsMonsterOpacityTrack",
        controlX + 78.f, rowY + rowH - 11.f, controlW - 156.f, 3.f, settingsLine, 2);
    monsterOpacityFillMaxWidth_ = controlW - 156.f;
    monsterOpacityFill_ = addSolid(host, "settingsMonsterOpacityFill",
        controlX + 78.f, rowY + rowH - 12.f, monsterOpacityFillMaxWidth_, 5.f, settingsAccent, 3);
    monsterOpacityValueLabel_ = addLabel(host, L"", controlX + 72.f, rowY, 20.f,
        controlW - 144.f, rowH * 0.60f, settingsInk, TextHAlign::Center, 3);

    groupY = rowY + rowH + groupGap;
    makeGroupTitle(L"사운드", groupY);
    rowY = groupY + 56.f;
    makeStepperRow(L"마스터 볼륨", rowY, 5,
        [this]() { return settings_->masterVolume; }, [this](int v) { settings_->masterVolume = v; },
        &masterVolPrev_, &masterVolNext_, &masterVolFill_, &masterVolLabel_);
    rowY += rowH + rowGap;
    makeStepperRow(L"배경음 (BGM)", rowY, 5,
        [this]() { return settings_->bgmVolume; }, [this](int v) { settings_->bgmVolume = v; },
        &bgmVolPrev_, &bgmVolNext_, &bgmVolFill_, &bgmVolLabel_);
    rowY += rowH + rowGap;
    makeStepperRow(L"효과음 (SFX)", rowY, 5,
        [this]() { return settings_->sfxVolume; }, [this](int v) { settings_->sfxVolume = v; },
        &sfxVolPrev_, &sfxVolNext_, &sfxVolFill_, &sfxVolLabel_);

    groupY = rowY + rowH + groupGap;
    makeGroupTitle(L"게임", groupY);
    rowY = groupY + 56.f;
    makeSettingRow(L"게임 종료", rowY);
    quitGameButton_ = addButton(host, L"종료하기",
        controlX + controlW - 190.f, rowY + 7.f, 166.f, rowH - 14.f,
        settingsInactive, settingsAccent, settingsAccent, 17.f,
        [this]() {
            if (onQuitGame_) onQuitGame_();
        },
        settingsButtonText);
    quitGameButton_->zOrder = 22;

    // Finalize the scroll metrics now that all rows are placed.
    scrollView->viewportHeight = viewH;
    scrollView->contentHeight  = rowY + rowH + contentPad;

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
    paintButton(vsyncOffButton_, !settings_->vsync);
    paintButton(vsyncOnButton_,   settings_->vsync);
    paintButton(allyDamageOffButton_, !settings_->allyDamageVisible);
    paintButton(allyDamageOnButton_,   settings_->allyDamageVisible);

    // 해상도는 런타임 목록(resolutions_) 위를 < > 로 이동. 목록은 모니터에 맞게 필터됨.
    const int lastIdx = resolutions_.empty() ? 0 : static_cast<int>(resolutions_.size()) - 1;
    if (settings_->resolutionIndex < 0)        settings_->resolutionIndex = 0;
    if (settings_->resolutionIndex > lastIdx)  settings_->resolutionIndex = lastIdx;

    const bool resolutionEnabled = !settings_->fullscreen;
    paintButton(resolutionPrevButton_, false, resolutionEnabled && settings_->resolutionIndex > 0);
    paintButton(resolutionNextButton_, false, resolutionEnabled && settings_->resolutionIndex < lastIdx);

    if (resolutionValueLabel_) {
        std::wstring resolutionText = L"--";
        if (!resolutions_.empty()) {
            const Resolution& r = resolutions_[settings_->resolutionIndex];
            resolutionText = std::to_wstring(r.w) + L" × " + std::to_wstring(r.h);
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
    paintButton(quitGameButton_, false, static_cast<bool>(onQuitGame_));

    if (monsterOpacityValueLabel_) {
        monsterOpacityValueLabel_->setText(std::to_wstring(settings_->monsterDamageOpacity) + L"%");
    }
    if (monsterOpacityFill_) {
        const float t = static_cast<float>(settings_->monsterDamageOpacity) / 100.f;
        monsterOpacityFill_->width = DimValue::px(std::max(0.f, monsterOpacityFillMaxWidth_ * t));
    }

    auto paintStepper = [&](Button* prev, Button* next, Button* fill, Label* label, int value) {
        paintButton(prev, false, value > 0);
        paintButton(next, false, value < 100);
        if (label) label->setText(std::to_wstring(value) + L"%");
        if (fill) {
            fill->width = DimValue::px(std::max(0.f, volumeFillMaxWidth_ * (static_cast<float>(value) / 100.f)));
        }
    };
    paintStepper(masterVolPrev_, masterVolNext_, masterVolFill_, masterVolLabel_, settings_->masterVolume);
    paintStepper(bgmVolPrev_,    bgmVolNext_,    bgmVolFill_,    bgmVolLabel_,    settings_->bgmVolume);
    paintStepper(sfxVolPrev_,    sfxVolNext_,    sfxVolFill_,    sfxVolLabel_,    settings_->sfxVolume);
}

}   // namespace UI

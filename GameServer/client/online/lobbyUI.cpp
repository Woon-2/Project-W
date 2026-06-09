#include "pch.hpp"
#include "lobbyUI.hpp"

#include "../ui/uiBuild.hpp"
#include "../gfx.hpp"

#include <algorithm>
#include <cmath>
#include <functional>

namespace Online {

void LobbyUI::loadTextures(GFX& gfx) {
    gfx.addRequestTextureLoad(RequestTextureLoad{
        .name            = "LobbyBg",
        .texturePath     = "../resources/UI/ui_lobby_bg.dds",
        .pDest           = &lobbyBgTex_,
        .pTexHashMap     = &lobbyTexHashMap_,
        .needsUploadInfo = false,
        .sampler         = Samplers::BilinearClamp
    });
    gfx.addRequestTextureLoad(RequestTextureLoad{
        .name            = "LobbyLogo",
        .texturePath     = "../resources/UI/ui_lobby_logo.dds",
        .pDest           = &lobbyLogoTex_,
        .pTexHashMap     = &lobbyTexHashMap_,
        .needsUploadInfo = false,
        .sampler         = Samplers::BilinearClamp
    });

    // 9-slice UI frames (panel/button/input) use clamp sampling to avoid edge bleed.
    gfx.addRequestTextureLoad(RequestTextureLoad{
        .name            = "LobbyPanel",
        .texturePath     = "../resources/UI/ui_panel_frame.dds",
        .pDest           = &lobbyPanelTex_,
        .pTexHashMap     = &lobbyTexHashMap_,
        .needsUploadInfo = false,
        .sampler         = Samplers::BilinearClamp
    });
    gfx.addRequestTextureLoad(RequestTextureLoad{
        .name            = "LobbyBtnPrimary",
        .texturePath     = "../resources/UI/ui_btn_primary.dds",
        .pDest           = &lobbyBtnPrimaryTex_,
        .pTexHashMap     = &lobbyTexHashMap_,
        .needsUploadInfo = false,
        .sampler         = Samplers::BilinearClamp
    });
    gfx.addRequestTextureLoad(RequestTextureLoad{
        .name            = "LobbyBtnSecondary",
        .texturePath     = "../resources/UI/ui_btn_secondary.dds",
        .pDest           = &lobbyBtnSecondaryTex_,
        .pTexHashMap     = &lobbyTexHashMap_,
        .needsUploadInfo = false,
        .sampler         = Samplers::BilinearClamp
    });
    gfx.addRequestTextureLoad(RequestTextureLoad{
        .name            = "LobbyInput",
        .texturePath     = "../resources/UI/ui_input.dds",
        .pDest           = &lobbyInputTex_,
        .pTexHashMap     = &lobbyTexHashMap_,
        .needsUploadInfo = false,
        .sampler         = Samplers::BilinearClamp
    });

    // Process immediately while the in-game background load queue is still empty.
    gfx.loadRequestedAssets();
}

void LobbyUI::build(UI::UIManager& uiManager, const Callbacks& callbacks) {
    callbacks_ = callbacks;

    auto* root = uiManager.root();

    // Idempotent: on a rebuild (e.g. resolution change) drop the previous widget
    // subtree first. All member pointers below are reassigned during this build.
    if (lobbyRoot_) {
        root->removeChild(lobbyRoot_);
        lobbyRoot_ = nullptr;
    }

    const float screenW = uiManager.screenWidth();
    const float screenH = uiManager.screenHeight();
    const float mainPanelW = std::min(560.f, std::max(360.f, screenW - 40.f));
    const float mainPanelH = 500.f;
    // The waiting room is a wide squad-stage layout over the 3D map.
    const float roomPanelW = std::min(1120.f, std::max(640.f, screenW - 40.f));
    const float roomPanelH = std::min(700.f,  std::max(460.f, screenH - 40.f));

    const XMFLOAT4 skyBlue     = { 0.529f, 0.808f, 0.980f, 1.f };
    const XMFLOAT4 ink         = { 0.090f, 0.125f, 0.200f, 1.f };
    const XMFLOAT4 muted       = { 0.384f, 0.439f, 0.518f, 1.f };
    const XMFLOAT4 surface     = { 1.000f, 1.000f, 1.000f, 1.f };
    const XMFLOAT4 surfaceSoft = { 0.953f, 0.976f, 0.992f, 1.f };
    const XMFLOAT4 primary     = { 0.082f, 0.365f, 0.545f, 1.f };
    const XMFLOAT4 primaryDark = { 0.055f, 0.286f, 0.435f, 1.f };
    const XMFLOAT4 primarySoft = { 0.847f, 0.937f, 0.988f, 1.f };

    lobbyRoot_ = root->addChild(std::make_unique<UI::UIElement>());
    lobbyRoot_->name    = "lobbyRoot";
    lobbyRoot_->anchor  = UI::Anchors::TopLeft;
    lobbyRoot_->pivot   = UI::Pivots::TopLeft;
    lobbyRoot_->offsetX = UI::DimValue::px(0.f);
    lobbyRoot_->offsetY = UI::DimValue::px(0.f);
    lobbyRoot_->width   = UI::DimValue::px(screenW);
    lobbyRoot_->height  = UI::DimValue::px(screenH);

    // Thin wrappers over the shared builders so the layout code stays declarative.
    auto applyRect = [](UI::UIElement* e, UI::Anchor anchor, UI::Pivot pivot,
                        float x, float y, float w, float h) {
        UI::Build::applyRect(e, anchor, pivot, x, y, w, h);
    };
    auto makeSolid = [](UI::UIElement* parent, const std::string& name,
                        float x, float y, float w, float h, XMFLOAT4 color,
                        int zOrder = 0) -> UI::Button* {
        return UI::Build::addSolid(parent, name, x, y, w, h, color, zOrder);
    };
    auto makeGroup = [&](UI::UIElement* parent, const std::string& name,
                         float w, float h) -> UI::UIElement* {
        auto* group = parent->addChild(std::make_unique<UI::UIElement>());
        group->name = name;
        applyRect(group, UI::Anchors::Center, UI::Pivots::Center, 0.f, 0.f, w, h);
        return group;
    };
    auto makeLabel = [](UI::UIElement* parent, const std::wstring& text,
                        float x, float y, float fontSize, float w, float h,
                        XMFLOAT4 color, UI::TextHAlign hAlign = UI::TextHAlign::Leading,
                        int zOrder = 1) -> UI::Label* {
        return UI::Build::addLabel(parent, text, x, y, fontSize, w, h, color, hAlign, zOrder);
    };
    auto makeButton = [](UI::UIElement* parent, const std::wstring& text,
                         float x, float y, float w, float h,
                         XMFLOAT4 normal, XMFLOAT4 hovered, XMFLOAT4 pressed,
                         float fontSize, std::function<void()> onClick,
                         XMFLOAT4 textColor) -> UI::Button* {
        return UI::Build::addButton(parent, text, x, y, w, h, normal, hovered, pressed,
                                    fontSize, std::move(onClick), textColor);
    };

    // 9-slice texture style helpers. On load failure the solid color is kept.
    auto stylePanel = [&](UI::Button* b) -> UI::Button* {
        if (b && lobbyPanelTex_.res) {
            b->texNormal = &lobbyPanelTex_;
            b->sliceUvBorderX = 0.30f; b->sliceUvBorderY = 0.30f;
            b->sliceCornerX = 40.f;    b->sliceCornerY = 40.f;
        }
        return b;
    };
    auto stylePrimary = [&](UI::Button* b) -> UI::Button* {
        if (b && lobbyBtnPrimaryTex_.res) {
            b->texNormal = &lobbyBtnPrimaryTex_;
            b->sliceUvBorderX = 0.40f; b->sliceUvBorderY = 0.40f;
            b->sliceCornerX = 22.f;    b->sliceCornerY = 22.f;
        }
        return b;
    };
    auto styleSecondary = [&](UI::Button* b) -> UI::Button* {
        if (b && lobbyBtnSecondaryTex_.res) {
            b->texNormal = &lobbyBtnSecondaryTex_;
            b->sliceUvBorderX = 0.40f; b->sliceUvBorderY = 0.40f;
            b->sliceCornerX = 22.f;    b->sliceCornerY = 22.f;
        }
        return b;
    };

    // Background: solid sky at the very back (-11), key art cover-scaled on top (-10).
    lobbySkyBg_ = makeSolid(lobbyRoot_, "lobbySkyBackground", 0.f, 0.f, screenW, screenH, skyBlue, -11);
    if (lobbyBgTex_.res) {
        const float bgAspect = 1672.f / 941.f;   // ui_lobby_bg.dds source ratio
        float bgW, bgH;
        if (screenW / screenH < bgAspect) {
            bgH = screenH;
            bgW = screenH * bgAspect;
        } else {
            bgW = screenW;
            bgH = screenW / bgAspect;
        }
        auto* bg = static_cast<UI::Image*>(lobbyRoot_->addChild(std::make_unique<UI::Image>()));
        bg->name    = "lobbyBgImage";
        applyRect(bg, UI::Anchors::Center, UI::Pivots::Center, 0.f, 0.f, bgW, bgH);
        bg->texture = &lobbyBgTex_;
        bg->zOrder  = -10;
        lobbyBgImage_ = bg;
    }

    // ---- Main menu (mainView) ----
    mainMenuRoot_ = lobbyRoot_->addChild(std::make_unique<UI::UIElement>());
    mainMenuRoot_->name   = "mainMenuRoot";
    mainMenuRoot_->anchor = UI::Anchors::TopLeft;
    mainMenuRoot_->pivot  = UI::Pivots::TopLeft;
    mainMenuRoot_->width  = UI::DimValue::px(screenW);
    mainMenuRoot_->height = UI::DimValue::px(screenH);

    // Game logo (OutLander) above the main panel (transparent-PNG-based texture).
    if (lobbyLogoTex_.res) {
        const float logoAspect = 2172.f / 724.f;          // ui_lobby_logo.dds source ratio (~3:1)
        const float logoH = 110.f;
        const float logoW = logoH * logoAspect;
        auto* logo = static_cast<UI::Image*>(mainMenuRoot_->addChild(std::make_unique<UI::Image>()));
        logo->name    = "lobbyLogoImage";
        applyRect(logo, UI::Anchors::Center, UI::Pivots::BottomCenter,
            0.f, -(mainPanelH * 0.5f + 16.f), logoW, logoH);
        logo->texture = &lobbyLogoTex_;
        logo->zOrder  = 5;
        lobbyLogoImage_ = logo;
    }

    auto* mainPanel = makeGroup(mainMenuRoot_, "mainPanel", mainPanelW, mainPanelH);
    stylePanel(makeSolid(mainPanel, "mainPanelBg", 0.f, 0.f, mainPanelW, mainPanelH, surface));
    makeLabel(mainPanel, L"PROJECT-W MULTIPLAYER", 34.f, 30.f, 13.f, mainPanelW - 68.f, 24.f, primary);
    makeLabel(mainPanel, L"비공개 로비", 34.f, 56.f, 42.f, mainPanelW - 68.f, 58.f, ink);

    auto* createBtn = makeButton(mainPanel, L"", 34.f, 132.f, mainPanelW - 68.f, 72.f,
        primary, primaryDark, primaryDark, 22.f, [this]() { if (callbacks_.onCreateRoom) callbacks_.onCreateRoom(); },
        { 1.f, 1.f, 1.f, 1.f });
    stylePrimary(createBtn);
    makeLabel(createBtn, L"방 만들기", 18.f, 0.f, 24.f, 220.f, 72.f, { 1.f, 1.f, 1.f, 1.f });
    makeLabel(createBtn, L"Create Room", mainPanelW - 220.f, 0.f, 13.f, 130.f, 72.f,
        { 1.f, 1.f, 1.f, 0.82f }, UI::TextHAlign::Trailing);

    // Room-code input + join (joinRoomForm in the prototype).
    {
        auto* joinBox = makeSolid(mainPanel, "joinBox", 34.f, 220.f, mainPanelW - 68.f, 116.f, surface);
        if (lobbyPanelTex_.res) {
            joinBox->texNormal     = &lobbyPanelTex_;
            joinBox->sliceUvBorderX = 0.30f; joinBox->sliceUvBorderY = 0.30f;
            joinBox->sliceCornerX   = 22.f;  joinBox->sliceCornerY   = 22.f;
        } else {
            joinBox->bgColor = surfaceSoft;
        }
    }
    makeLabel(mainPanel, L"방 코드로 참가", 52.f, 234.f, 18.f, 240.f, 28.f, ink);
    {
        roomCodeInput_ = static_cast<UI::TextInput*>(
            mainPanel->addChild(std::make_unique<UI::TextInput>()));
        roomCodeInput_->name    = "roomCodeInput";
        applyRect(roomCodeInput_, UI::Anchors::TopLeft, UI::Pivots::TopLeft,
            52.f, 274.f, mainPanelW - 232.f, 48.f);
        roomCodeInput_->bgColor        = { 1.f, 1.f, 1.f, 1.f };
        roomCodeInput_->bgColorFocused = { 0.953f, 0.976f, 0.992f, 1.f };
        roomCodeInput_->textColor        = ink;
        roomCodeInput_->placeholderColor = muted;
        roomCodeInput_->uppercase = true;
        roomCodeInput_->alnumOnly = true;
        roomCodeInput_->setMaxLength(6);
        roomCodeInput_->setPlaceholder(L"JOIN01");
        roomCodeInput_->onSubmit = [this](const std::wstring& code) {
            if (callbacks_.onJoinRoom) callbacks_.onJoinRoom(std::string(code.begin(), code.end()));
        };
        if (lobbyInputTex_.res) {
            roomCodeInput_->backgroundTex  = &lobbyInputTex_;
            roomCodeInput_->sliceUvBorderX = 0.15f;
            roomCodeInput_->sliceUvBorderY = 0.42f;
            roomCodeInput_->sliceCornerX   = 18.f;
            roomCodeInput_->sliceCornerY   = 18.f;
            roomCodeInput_->texTintFocused = { 1.10f, 1.10f, 1.10f, 1.f };
        }

        auto* joinBtn = makeButton(mainPanel, L"참가", mainPanelW - 146.f, 274.f, 94.f, 48.f,
            primary, primaryDark, primaryDark, 20.f, [this]() {
                const std::wstring& w = roomCodeInput_->text();
                if (callbacks_.onJoinRoom) callbacks_.onJoinRoom(std::string(w.begin(), w.end()));
            }, { 1.f, 1.f, 1.f, 1.f });
        joinBtn->name    = "joinButton";
        stylePrimary(joinBtn);
    }

    mainMenuMsgLabel_ = makeLabel(mainPanel, L"", 34.f, 348.f, 18.f, mainPanelW - 68.f, 30.f,
        { 0.70f, 0.24f, 0.24f, 1.f }, UI::TextHAlign::Center);

    const float quietW = (mainPanelW - 80.f) * 0.5f;
    styleSecondary(makeButton(mainPanel, L"설정", 34.f, 392.f, quietW, 48.f,
        surfaceSoft, { 0.847f, 0.937f, 0.988f, 1.f }, primarySoft, 20.f,
        [this]() { if (callbacks_.onOpenSettings) callbacks_.onOpenSettings(); }, ink));
    styleSecondary(makeButton(mainPanel, L"종료", 46.f + quietW, 392.f, quietW, 48.f,
        surfaceSoft, { 0.847f, 0.937f, 0.988f, 1.f }, primarySoft, 20.f,
        [this]() { if (callbacks_.onQuit) callbacks_.onQuit(); }, ink));

    // ---- Waiting room (lobbyView): a translucent squad stage over the 3D map ----
    const XMFLOAT4 textLight   = { 0.95f, 0.97f, 1.00f, 1.f };
    const XMFLOAT4 textMuted   = { 0.78f, 0.84f, 0.92f, 1.f };
    const XMFLOAT4 buttonInk   = { 0.07f, 0.11f, 0.18f, 1.f };
    const XMFLOAT4 scrimBar    = { 0.06f, 0.09f, 0.14f, 0.52f };  // toolbar background
    const XMFLOAT4 wrBtnAlpha  = { 1.f, 1.f, 1.f, 0.92f };        // translucent tint for textured buttons

    auto fadeBtn = [&](UI::Button* b) -> UI::Button* {
        if (b) { b->texTint = wrBtnAlpha; b->texTintHovered = { 1.08f, 1.08f, 1.08f, 0.96f };
                 b->texTintPressed = { 0.82f, 0.82f, 0.82f, 0.96f }; }
        return b;
    };

    waitingRoomRoot_ = lobbyRoot_->addChild(std::make_unique<UI::UIElement>());
    waitingRoomRoot_->name   = "waitingRoomRoot";
    waitingRoomRoot_->anchor = UI::Anchors::TopLeft;
    waitingRoomRoot_->pivot  = UI::Pivots::TopLeft;
    waitingRoomRoot_->width  = UI::DimValue::px(screenW);
    waitingRoomRoot_->height = UI::DimValue::px(screenH);

    auto* roomPanel = makeGroup(waitingRoomRoot_, "waitingRoomPanel", roomPanelW, roomPanelH);

    const float pad   = 28.f;
    const float gap   = 12.f;
    const float innerW = roomPanelW - 2.f * pad;

    // Header
    makeLabel(roomPanel, L"PRIVATE ROOM", pad, 18.f, 13.f, 280.f, 22.f, textLight);
    makeLabel(roomPanel, L"대기실", pad, 40.f, 40.f, 320.f, 54.f, textLight);
    makeSolid(roomPanel, "playerCountBadgeBg", roomPanelW - pad - 92.f, 30.f, 92.f, 44.f, scrimBar);
    playerCountLabel_ = makeLabel(roomPanel, L"1 / 4", roomPanelW - pad - 92.f, 30.f, 22.f, 92.f, 44.f,
        textLight, UI::TextHAlign::Center);

    // Toolbar: [room code + copy] [host action] [leave room]
    const float toolbarY = 102.f;
    const float toolbarH = 72.f;
    const float leaveW   = 150.f;
    const float hostW    = 220.f;
    const float codeW    = innerW - hostW - leaveW - 2.f * gap;
    const float hostX    = pad + codeW + gap;
    const float leaveX   = hostX + hostW + gap;

    // Room-code area
    makeSolid(roomPanel, "roomCodeArea", pad, toolbarY, codeW, toolbarH, scrimBar);
    makeLabel(roomPanel, L"방 코드", pad + 18.f, toolbarY + 10.f, 13.f, 160.f, 20.f, textMuted);
    roomCodeLabel_ = makeLabel(roomPanel, L"------", pad + 18.f, toolbarY + 28.f, 34.f,
        codeW - 150.f, 40.f, textLight);
    fadeBtn(stylePrimary(makeButton(roomPanel, L"코드 복사",
        pad + codeW - 124.f, toolbarY + 12.f, 112.f, toolbarH - 24.f,
        primary, primaryDark, primaryDark, 16.f,
        [this]() { if (callbacks_.onCopyCode) callbacks_.onCopyCode(); }, textLight)));

    // Host action: start button / wait message (overlaid, toggled in refresh()).
    startGameButton_ = makeButton(roomPanel, L"게임 시작", hostX, toolbarY, hostW, toolbarH,
        primary, primaryDark, primaryDark, 22.f, [this]() { if (callbacks_.onStartGame) callbacks_.onStartGame(); }, textLight);
    fadeBtn(stylePrimary(startGameButton_));
    startGameLabel_  = static_cast<UI::Label*>(startGameButton_->children().front().get());

    waitMessageBg_ = makeSolid(roomPanel, "waitMessageBg", hostX, toolbarY, hostW, toolbarH, scrimBar);
    hostStatusLabel_ = makeLabel(roomPanel, L"호스트 대기 중...", hostX, toolbarY, 17.f,
        hostW, toolbarH, textMuted, UI::TextHAlign::Center);

    // Leave room
    fadeBtn(styleSecondary(makeButton(roomPanel, L"방 나가기", leaveX, toolbarY, leaveW, toolbarH,
        surfaceSoft, primarySoft, primarySoft, 18.f, [this]() { if (callbacks_.onLeaveRoom) callbacks_.onLeaveRoom(); }, buttonInk)));

    // Squad stage: 4 horizontal slots
    const float slotsY    = toolbarY + toolbarH + 16.f;
    const float debugH    = 40.f;
    const float slotsH    = roomPanelH - slotsY - debugH - 14.f;
    const float slotW     = (innerW - 3.f * gap) / 4.f;
    const float nameH     = 56.f;
    const XMFLOAT4 slotPanelCol    = { 0.09f, 0.13f, 0.20f, 0.16f };  // character backdrop (near opaque)
    const XMFLOAT4 slotNameplateCol= { 0.05f, 0.08f, 0.13f, 0.78f };  // nameplate background

    for (int i = 0; i < kMaxLobbyPlayers; ++i) {
        const float x = pad + static_cast<float>(i) * (slotW + gap);

        slotNumberLabels_[i] = makeLabel(roomPanel, L"", x + 12.f, slotsY + 10.f, 13.f, 60.f, 20.f,
            textMuted, UI::TextHAlign::Leading, 2);

        // Model bay: composites the offscreen portrait RT (character) sampled per cell.
        // texture/uvScaleBias/visible are refreshed each frame in Game::renderWaitingRoom().
        slotPanels_[i] = makeSolid(roomPanel, "slotPanelBg",
            x + 8.f, slotsY + 8.f, slotW - 16.f, slotsH - nameH - 22.f, slotPanelCol, 0);

        auto* bay = roomPanel->addChild(std::make_unique<UI::Image>());
        bay->name = "slotBay";
        applyRect(bay, UI::Anchors::TopLeft, UI::Pivots::TopLeft,
            x + 8.f, slotsY + 8.f, slotW - 16.f, slotsH - nameH - 22.f);
        bay->zOrder  = 1;
        bay->visible = false;
        slotBays_[i] = static_cast<UI::Image*>(bay);

        const float nameY = slotsY + slotsH - nameH - 4.f;
        slotNameplateBgs_[i] = makeSolid(roomPanel, "nameplateBg",
            x + 10.f, nameY, slotW - 20.f, 52.f, slotNameplateCol, 1);
        slotNameLabels_[i] = makeLabel(roomPanel, L"대기 중",
            x + 10.f, nameY, 18.f, slotW - 20.f, 28.f,
            textLight, UI::TextHAlign::Center, 3);
        slotHostBadgeLabels_[i] = makeLabel(roomPanel, L"",
            x + 10.f, slotsY + slotsH - 30.f, 12.f, slotW - 20.f, 22.f,
            textMuted, UI::TextHAlign::Center, 3);

        // Gold rectangular border around the host name (4 edges, zOrder 2). Hidden by
        // default; shown only for the host in refresh().
        const XMFLOAT4 borderCol = { 0.95f, 0.80f, 0.35f, 1.f };
        const float bpad = 4.f, bt = 2.f;
        const float bx = x + 10.f - bpad, by = nameY - bpad;
        const float bw = (slotW - 20.f) + 2.f * bpad, bh = 28.f + 2.f * bpad;
        slotNameBorders_[i][0] = makeSolid(roomPanel, "nameBorderT", bx, by, bw, bt, borderCol, 2);
        slotNameBorders_[i][1] = makeSolid(roomPanel, "nameBorderB", bx, by + bh - bt, bw, bt, borderCol, 2);
        slotNameBorders_[i][2] = makeSolid(roomPanel, "nameBorderL", bx, by, bt, bh, borderCol, 2);
        slotNameBorders_[i][3] = makeSolid(roomPanel, "nameBorderR", bx + bw - bt, by, bt, bh, borderCol, 2);
        for (auto* edge : slotNameBorders_[i]) {
            if (edge) edge->visible = false;
        }
    }

    // Full-screen loading overlay (built last so it sits on top by zOrder).
    buildLoadingScreen(uiManager);
}

// Black full-screen loading overlay: centered logo, "loading..." text, a progress
// bar, and a 12-dot ring spinner. Hidden by default; toggled from updateLoading().
void LobbyUI::buildLoadingScreen(UI::UIManager& uiManager) {
    if (!lobbyRoot_) return;

    const float screenW = uiManager.screenWidth();
    const float screenH = uiManager.screenHeight();
    constexpr int kZ = 100;  // above all lobby UI

    loadingRoot_ = lobbyRoot_->addChild(std::make_unique<UI::UIElement>());
    loadingRoot_->name    = "loadingRoot";
    loadingRoot_->anchor  = UI::Anchors::TopLeft;
    loadingRoot_->pivot   = UI::Pivots::TopLeft;
    loadingRoot_->offsetX = UI::DimValue::px(0.f);
    loadingRoot_->offsetY = UI::DimValue::px(0.f);
    loadingRoot_->width   = UI::DimValue::px(screenW);
    loadingRoot_->height  = UI::DimValue::px(screenH);
    loadingRoot_->zOrder  = kZ;
    loadingRoot_->visible = false;

    // Opaque black background covering the whole screen.
    auto* bg = static_cast<UI::Button*>(loadingRoot_->addChild(std::make_unique<UI::Button>()));
    bg->name        = "loadingBg";
    bg->anchor      = UI::Anchors::TopLeft;
    bg->pivot       = UI::Pivots::TopLeft;
    bg->offsetX     = UI::DimValue::px(0.f);
    bg->offsetY     = UI::DimValue::px(0.f);
    bg->width       = UI::DimValue::px(screenW);
    bg->height      = UI::DimValue::px(screenH);
    bg->interactive = true;  // absorb clicks so hidden lobby controls cannot fire through the overlay
    bg->bgColor     = { 0.f, 0.f, 0.f, 1.f };
    bg->zOrder      = kZ;

    // Centered logo (above middle). Skipped if the logo texture failed to load.
    if (lobbyLogoTex_.res) {
        auto* logo = static_cast<UI::Image*>(loadingRoot_->addChild(std::make_unique<UI::Image>()));
        logo->name    = "loadingLogo";
        logo->anchor  = UI::Anchors::Center;
        logo->pivot   = UI::Pivots::Center;
        logo->offsetX = UI::DimValue::px(0.f);
        logo->offsetY = UI::DimValue::px(-120.f);
        logo->width   = UI::DimValue::px(420.f);
        logo->height  = UI::DimValue::px(160.f);
        logo->texture = &lobbyLogoTex_;
        logo->zOrder  = kZ + 1;
    }

    // "loading..." label, just above the progress bar.
    loadingTextLabel_ = static_cast<UI::Label*>(loadingRoot_->addChild(std::make_unique<UI::Label>()));
    loadingTextLabel_->name    = "loadingText";
    loadingTextLabel_->anchor  = UI::Anchors::Center;
    loadingTextLabel_->pivot   = UI::Pivots::Center;
    loadingTextLabel_->offsetX = UI::DimValue::px(0.f);
    loadingTextLabel_->offsetY = UI::DimValue::px(64.f);
    loadingTextLabel_->width   = UI::DimValue::px(420.f);
    loadingTextLabel_->height  = UI::DimValue::px(28.f);
    loadingTextLabel_->setTextHAlign(UI::TextHAlign::Center);
    loadingTextLabel_->setTextVAlign(UI::TextVAlign::Center);
    loadingTextLabel_->setFontSize(18.f);
    loadingTextLabel_->setTextColor(0.92f, 0.95f, 1.f, 1.f);
    loadingTextLabel_->setText(L"loading...");
    loadingTextLabel_->zOrder = kZ + 1;

    // Progress bar (real load progress).
    loadingBar_ = static_cast<UI::ProgressBar*>(loadingRoot_->addChild(std::make_unique<UI::ProgressBar>()));
    loadingBar_->name      = "loadingBar";
    loadingBar_->anchor    = UI::Anchors::Center;
    loadingBar_->pivot     = UI::Pivots::Center;
    loadingBar_->offsetX   = UI::DimValue::px(0.f);
    loadingBar_->offsetY   = UI::DimValue::px(100.f);
    loadingBar_->width     = UI::DimValue::px(420.f);
    loadingBar_->height    = UI::DimValue::px(14.f);
    loadingBar_->bgColor   = { 0.16f, 0.18f, 0.22f, 1.f };
    loadingBar_->fillColor = { 0.18f, 0.62f, 0.95f, 1.f };
    loadingBar_->zOrder    = kZ + 1;
    loadingBar_->setProgress(0.f);

    // Dot-ring spinner below the bar. The rotating "arc" is faked by phase-shifting
    // each dot's alpha (no rotation field / arc texture needed).
    constexpr float kRadius = 26.f;
    const float spinnerCY = 176.f;
    for (int i = 0; i < static_cast<int>(spinnerDots_.size()); ++i) {
        const float ang = (static_cast<float>(i) / static_cast<float>(spinnerDots_.size())) * 6.2831853f;
        auto* dot = static_cast<UI::Button*>(loadingRoot_->addChild(std::make_unique<UI::Button>()));
        dot->name        = "spinnerDot";
        dot->anchor      = UI::Anchors::Center;
        dot->pivot       = UI::Pivots::Center;
        dot->offsetX     = UI::DimValue::px(std::sin(ang) * kRadius);
        dot->offsetY     = UI::DimValue::px(spinnerCY - std::cos(ang) * kRadius);
        dot->width       = UI::DimValue::px(7.f);
        dot->height      = UI::DimValue::px(7.f);
        dot->interactive = false;
        dot->bgColor     = { 0.85f, 0.90f, 1.f, 0.25f };
        dot->zOrder      = kZ + 1;
        spinnerDots_[i] = dot;
    }
}

void LobbyUI::updateLoading(float deltaTimeSec, bool visible, float progress01) {
    if (!loadingRoot_) return;
    loadingRoot_->visible = visible;
    if (!visible) return;

    if (loadingBar_) loadingBar_->setProgress(progress01);

    // Rotate the bright "head" around the dot ring; trailing dots fade out.
    loadingSpinTime_ += deltaTimeSec;
    const int n = static_cast<int>(spinnerDots_.size());
    const float headF = std::fmod(loadingSpinTime_ * 1.6f, 1.f) * static_cast<float>(n);
    for (int i = 0; i < n; ++i) {
        if (!spinnerDots_[i]) continue;
        float dist = static_cast<float>(i) - headF;
        while (dist < 0.f) dist += static_cast<float>(n);
        const float t = dist / static_cast<float>(n);
        const float alpha = 0.15f + 0.85f * (1.f - t);
        auto c = spinnerDots_[i]->bgColor;
        c.w = alpha;
        spinnerDots_[i]->bgColor = c;
    }
}

void LobbyUI::hideAllSlotBays() {
    for (auto* bay : slotBays_) {
        if (bay) { bay->visible = false; bay->texture = nullptr; }
    }
}

void LobbyUI::setFlatBackgroundVisible(bool v) {
    if (lobbySkyBg_)   lobbySkyBg_->visible   = v;
    if (lobbyBgImage_) lobbyBgImage_->visible = v;
}

void LobbyUI::refresh(const ViewState& s) {
    if (!lobbyRoot_) {
        return;
    }

    const XMFLOAT4 primary     = { 0.082f, 0.365f, 0.545f, 1.f };
    const XMFLOAT4 primaryDark = { 0.055f, 0.286f, 0.435f, 1.f };
    const XMFLOAT4 textLight   = { 0.95f, 0.97f, 1.00f, 1.f };
    const XMFLOAT4 textMuted   = { 0.78f, 0.84f, 0.92f, 1.f };

    lobbyRoot_->visible = s.inLobbyScene;

    const bool inMain = s.inMainMenu;
    const bool waitingRoom3D = !inMain && s.waitingRoom3DReady;
    if (lobbySkyBg_)   lobbySkyBg_->visible   = !waitingRoom3D;
    if (lobbyBgImage_) lobbyBgImage_->visible = !waitingRoom3D;

    if (mainMenuRoot_)    mainMenuRoot_->visible    = inMain;
    if (waitingRoomRoot_) waitingRoomRoot_->visible = !inMain;

    if (inMain) {
        return;
    }

    // Room code
    if (roomCodeLabel_) {
        const std::wstring code(s.roomCode.begin(), s.roomCode.end());
        roomCodeLabel_->setText(code.empty() ? std::wstring(L"------") : code);
    }

    // Player count
    if (playerCountLabel_) {
        playerCountLabel_->setText(
            std::to_wstring(s.players.size()) + L" / " + std::to_wstring(s.maxPlayers));
    }

    // Player slots (squad stage)
    for (int i = 0; i < kMaxLobbyPlayers; ++i) {
        if (slotNumberLabels_[i]) {
            wchar_t num[4] = {};
            swprintf_s(num, L"%02d", i + 1);
            slotNumberLabels_[i]->setText(num);
        }

        const bool filled = (i < static_cast<int>(s.players.size()));
        if (filled) {
            const auto& player = s.players[i];

            if (slotNameLabels_[i]) {
                slotNameLabels_[i]->setText(player.name);
                slotNameLabels_[i]->setTextColor(textLight.x, textLight.y, textLight.z, textLight.w);
            }
            if (slotHostBadgeLabels_[i]) {
                slotHostBadgeLabels_[i]->setText(player.isHost ? L"\U0001F451 호스트" : L"");
            }
            for (auto* edge : slotNameBorders_[i]) {
                if (edge) edge->visible = player.isHost;
            }
        } else {
            if (slotNameLabels_[i]) {
                slotNameLabels_[i]->setText(L"대기 중");
                slotNameLabels_[i]->setTextColor(textMuted.x, textMuted.y, textMuted.z, textMuted.w);
            }
            if (slotHostBadgeLabels_[i]) slotHostBadgeLabels_[i]->setText(L"");
            for (auto* edge : slotNameBorders_[i]) {
                if (edge) edge->visible = false;
            }
        }
    }

    // Start button (host) / wait message (non-host) overlaid in the same spot.
    if (startGameButton_) startGameButton_->visible = s.isHost;
    if (waitMessageBg_)   waitMessageBg_->visible   = !s.isHost;
    if (hostStatusLabel_) hostStatusLabel_->visible = !s.isHost;
    if (s.isHost && startGameLabel_) {
        startGameLabel_->setText(L"게임 시작");
    }
    if (s.isHost && startGameButton_) {
        // Solid fallback path (when texNormal is unset).
        startGameButton_->bgColor        = primary;
        startGameButton_->bgColorHovered = primaryDark;
        startGameButton_->bgColorPressed = primaryDark;

        // Textured path (stylePrimary set texNormal) ignores bgColor*, uses texTint*.
        startGameButton_->texTint        = XMFLOAT4{ 1.f,   1.f,   1.f,   0.92f };
        startGameButton_->texTintHovered = XMFLOAT4{ 1.08f, 1.08f, 1.08f, 0.96f };
        startGameButton_->texTintPressed = XMFLOAT4{ 0.82f, 0.82f, 0.82f, 0.96f };
    }
}

}   // namespace Online

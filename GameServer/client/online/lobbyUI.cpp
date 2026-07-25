#include "pch.hpp"
#include "lobbyUI.hpp"

#include "../ui/uiBuild.hpp"
#include "../gfx.hpp"

#include <algorithm>
#include <cmath>
#include <functional>

namespace Online {

namespace {

constexpr const wchar_t* kLobbyFontFamily = L"넥슨Lv2고딕";

// Self-contained UTF-8 -> UTF-16 decoder. The Windows NLS path (MultiByteToWideChar)
// is unavailable here because pch.hpp defines NONLS, so winnls.h (CP_UTF8 and the
// NLS prototypes) is excluded. Invalid byte sequences are skipped defensively.
std::wstring utf8ToWide(const std::string& bytes) {
    std::wstring out;
    out.reserve(bytes.size());
    const std::size_t n = bytes.size();
    std::size_t i = 0;
    while (i < n) {
        const unsigned char c = static_cast<unsigned char>(bytes[i]);
        char32_t cp = 0;
        int extra = 0;
        if (c < 0x80)              { cp = c;        extra = 0; }
        else if ((c >> 5) == 0x06) { cp = c & 0x1F; extra = 1; }
        else if ((c >> 4) == 0x0E) { cp = c & 0x0F; extra = 2; }
        else if ((c >> 3) == 0x1E) { cp = c & 0x07; extra = 3; }
        else                       { ++i; continue; }   // stray continuation/invalid lead

        if (i + static_cast<std::size_t>(extra) >= n) break;
        bool ok = true;
        for (int k = 1; k <= extra; ++k) {
            const unsigned char cc = static_cast<unsigned char>(bytes[i + k]);
            if ((cc >> 6) != 0x02) { ok = false; break; }
            cp = (cp << 6) | (cc & 0x3F);
        }
        if (!ok) { ++i; continue; }
        i += static_cast<std::size_t>(extra) + 1;

        if (cp <= 0xFFFF) {
            out.push_back(static_cast<wchar_t>(cp));
        } else {
            cp -= 0x10000;
            out.push_back(static_cast<wchar_t>(0xD800 + (cp >> 10)));
            out.push_back(static_cast<wchar_t>(0xDC00 + (cp & 0x3FF)));
        }
    }
    return out;
}

// Loads a UTF-8 (optionally BOM-prefixed) text file as a wide string for label
// rendering. Returns empty on failure. CRLF is normalized to LF so DirectWrite
// sees clean hard line breaks; the file is expected to be pre-wrapped to the
// story column width (auto word-wrap happens at the global font bitmap, not the
// label rect).
std::wstring loadStoryTextUtf8(const std::filesystem::path& path) {
    std::ifstream ifs(path, std::ios::binary);
    if (!ifs) return std::wstring();

    ifs.seekg(0, std::ios::end);
    const std::streamoff size = ifs.tellg();
    ifs.seekg(0, std::ios::beg);
    if (size <= 0) return std::wstring();

    std::string bytes(static_cast<std::size_t>(size), '\0');
    ifs.read(bytes.data(), static_cast<std::streamsize>(bytes.size()));

    if (bytes.size() >= 3 &&
        static_cast<unsigned char>(bytes[0]) == 0xEF &&
        static_cast<unsigned char>(bytes[1]) == 0xBB &&
        static_cast<unsigned char>(bytes[2]) == 0xBF) {
        bytes.erase(0, 3);
    }
    bytes.erase(std::remove(bytes.begin(), bytes.end(), '\r'), bytes.end());
    if (bytes.empty()) return std::wstring();

    return utf8ToWide(bytes);
}

// Greedy word/character wrap so one source line fits maxWidthPx. DirectWrite's own
// wrapping happens at the global 1024px font bitmap (not the narrow label rect), so
// long lines clip instead of wrapping; we pre-break them here using the real
// measured width. Prefers breaking at the last space; falls back to per-character
// breaks for space-less runs (e.g. Korean). maxWidthPx is in physical pixels and the
// font must be sized in the same (uiScale-applied) space as measureText expects.
std::vector<std::wstring> wrapToWidth(GFX* gfx, FontHandle* font,
                                      const std::wstring& line, float maxWidthPx) {
    std::vector<std::wstring> out;
    if (!gfx || !font || maxWidthPx <= 1.f) { out.push_back(line); return out; }

    auto widthOf = [&](const std::wstring& s) -> int {
        if (s.empty()) return 0;
        int w = 0, h = 0;
        gfx->measureText(font, s.c_str(), static_cast<DWORD>(s.size()), 1.0e6f, 1.0e6f, &w, &h);
        return w;
    };

    std::wstring cur;
    for (const wchar_t ch : line) {
        std::wstring trial = cur;
        trial.push_back(ch);
        if (!cur.empty() && static_cast<float>(widthOf(trial)) > maxWidthPx) {
            const std::size_t sp = cur.find_last_of(L' ');
            if (sp != std::wstring::npos && sp > 0) {
                out.push_back(cur.substr(0, sp));   // break after the last whole word
                cur = cur.substr(sp + 1);
            } else {
                out.push_back(cur);                 // space-less run: hard character break
                cur.clear();
            }
        }
        cur.push_back(ch);
    }
    out.push_back(cur);   // remainder (empty string preserved for blank source lines)
    return out;
}

constexpr int weaponIndex(PlayerWeaponType weaponType) {
    const int idx = static_cast<int>(weaponType);
    return (idx >= 0 && idx < 4) ? idx : 0;
}

const wchar_t* weaponDisplayName(PlayerWeaponType weaponType) {
    switch (weaponType) {
    case PlayerWeaponType::Katana:       return L"검";
    case PlayerWeaponType::SpearHook:    return L"창";
    case PlayerWeaponType::CrystalWand:  return L"완드";
    case PlayerWeaponType::HeavyArrow:   return L"활";
    default:                             return L"검";
    }
}

}   // namespace

void LobbyUI::loadTextures(GFX& gfx) {
    gfx_ = &gfx;   // cached for build-time text measurement (story word-wrap)
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

    gfx.addRequestTextureLoad(RequestTextureLoad{
        .name            = "LobbyLeftButton",
        .texturePath     = "../resources/UI/ui_left_button.dds",
        .pDest           = &lobbyLeftButtonTex_,
        .pTexHashMap     = &lobbyTexHashMap_,
        .needsUploadInfo = false,
        .sampler         = Samplers::BilinearClamp
    });

    const char* weaponIconPaths[4] = {
        "../resources/UI/katana.dds",
        "../resources/UI/spear-hook.dds",
        "../resources/UI/crystal-wand.dds",
        "../resources/UI/heavy-arrow.dds",
    };
    const char* weaponIconNames[4] = {
        "LobbyWeaponKatana",
        "LobbyWeaponSpearHook",
        "LobbyWeaponCrystalWand",
        "LobbyWeaponHeavyArrow",
    };
    for (int i = 0; i < 4; ++i) {
        gfx.addRequestTextureLoad(RequestTextureLoad{
            .name            = weaponIconNames[i],
            .texturePath     = weaponIconPaths[i],
            .pDest           = &lobbyWeaponIconTex_[i],
            .pTexHashMap     = &lobbyTexHashMap_,
            .needsUploadInfo = false,
            .sampler         = Samplers::BilinearClamp
        });
    }

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

    const float screenW = uiManager.layoutWidth();
    const float screenH = uiManager.layoutHeight();
    const float mainPanelW = std::min(720.f, std::max(520.f, screenW - 40.f));
    const float mainPanelH = 494.f;

    // The waiting room is a wide squad-stage layout over the 3D map. It fills the
    // whole screen (full-bleed) instead of the centered 4:3 safe area, so wide
    // resolutions have no empty side bands. We size it from the framework's
    // layout-space screen extent (screenWidth/uiScale) rather than the fixed
    // 1024x768 base, so it tracks the real resolution on every rebuild. The panel
    // is centered (Anchors::Center), and since the safe area is itself centered,
    // a full-width panel lands flush against both screen edges.
    const float fullLayoutW = uiManager.screenWidth()  / uiManager.uiScale();
    const float fullLayoutH = uiManager.screenHeight() / uiManager.uiScale();
    const float roomMargin  = 24.f;   // small inset from the physical screen edges
    const float roomPanelW  = std::max(640.f, fullLayoutW - 2.f * roomMargin);
    const float roomPanelH  = std::max(460.f, fullLayoutH - 2.f * roomMargin);

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
        auto* label = UI::Build::addLabel(parent, text, x, y, fontSize, w, h, color, hAlign, zOrder);
        label->setFontFamily(kLobbyFontFamily);
        return label;
    };
    auto makeButton = [](UI::UIElement* parent, const std::wstring& text,
                         float x, float y, float w, float h,
                         XMFLOAT4 normal, XMFLOAT4 hovered, XMFLOAT4 pressed,
                         float fontSize, std::function<void()> onClick,
                         XMFLOAT4 textColor) -> UI::Button* {
        auto* button = UI::Build::addButton(parent, text, x, y, w, h, normal, hovered, pressed,
                                             fontSize, std::move(onClick), textColor);
        if (!button->children().empty()) {
            auto* label = static_cast<UI::Label*>(button->children().front().get());
            label->setFontFamily(kLobbyFontFamily);
        }
        return button;
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
    auto styleInput = [&](UI::TextInput* input) -> UI::TextInput* {
        if (!input) return input;
        input->bgColor          = { 1.f, 1.f, 1.f, 1.f };
        input->bgColorFocused   = surfaceSoft;
        input->textColor        = ink;
        input->placeholderColor = muted;
        input->setFontFamily(kLobbyFontFamily);
        input->setTextPaddingX(15.f);
        if (lobbyInputTex_.res) {
            input->backgroundTex  = &lobbyInputTex_;
            input->sliceUvBorderX = 0.15f;
            input->sliceUvBorderY = 0.42f;
            input->sliceCornerX   = 18.f;
            input->sliceCornerY   = 18.f;
            input->texTintFocused = { 1.10f, 1.10f, 1.10f, 1.f };
        }
        return input;
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
        const float logoH = 84.f;
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

    const float contentX = 40.f;
    const float contentW = mainPanelW - 2.f * contentX;
    const float accountInputX = 150.f;
    const float accountInputW = contentW - accountInputX;
    const float authRowGap = 18.f;
    const float authInputH = 62.f;
    const float authButtonH = 88.f;

    // Authentication and room selection occupy the same panel region. refresh()
    // exposes exactly one root so hidden controls cannot receive pointer input.
    authRoot_ = mainPanel->addChild(std::make_unique<UI::UIElement>());
    authRoot_->name = "authRoot";
    applyRect(authRoot_, UI::Anchors::TopLeft, UI::Pivots::TopLeft,
        contentX, 24.f, contentW, 270.f);

    // Account inputs stay local until the authentication server is implemented.
    makeLabel(authRoot_, L"아이디", 0.f, 8.f, 24.f, 132.f, 54.f, ink);
    loginIdInput_ = static_cast<UI::TextInput*>(
        authRoot_->addChild(std::make_unique<UI::TextInput>()));
    loginIdInput_->name = "loginIdInput";
    applyRect(loginIdInput_, UI::Anchors::TopLeft, UI::Pivots::TopLeft,
        accountInputX, 4.f, accountInputW, authInputH);
    loginIdInput_->setFontSize(22.f);
    loginIdInput_->setPlaceholder(L"아이디 입력");
    loginIdInput_->zOrder = 2;
    styleInput(loginIdInput_);

    makeLabel(authRoot_, L"비밀번호", 0.f, 8.f + authInputH + authRowGap, 24.f, 132.f, 54.f, ink);
    loginPasswordInput_ = static_cast<UI::TextInput*>(
        authRoot_->addChild(std::make_unique<UI::TextInput>()));
    loginPasswordInput_->name = "loginPasswordInput";
    applyRect(loginPasswordInput_, UI::Anchors::TopLeft, UI::Pivots::TopLeft,
        accountInputX, 4.f + authInputH + authRowGap, accountInputW, authInputH);
    loginPasswordInput_->setFontSize(22.f);
    loginPasswordInput_->setPlaceholder(L"비밀번호 입력");
    loginPasswordInput_->setPasswordMode(true);
    loginPasswordInput_->zOrder = 2;
    loginPasswordInput_->onSubmit = [this](const std::wstring&) { submitLogin(); };
    styleInput(loginPasswordInput_);

    const float authButtonGap = authRowGap;
    const float authButtonInset = 8.f;
    const float authButtonW = (contentW - authButtonGap - 2.f * authButtonInset) * 0.5f;
    const float authButtonY = 4.f + 2.f * authInputH + 2.f * authRowGap;
    auto* loginButton = makeButton(authRoot_, L"", authButtonInset, authButtonY,
        authButtonW, authButtonH,
        primary, primaryDark, primaryDark, 24.f,
        [this]() { submitLogin(); }, { 1.f, 1.f, 1.f, 1.f });
    loginButton->name = "loginButton";
    stylePrimary(loginButton);
    makeLabel(loginButton, L"로그인", 0.f, 23.f, 26.f, authButtonW, 42.f,
        { 1.f, 1.f, 1.f, 1.f }, UI::TextHAlign::Center);

    auto* signupButton = makeButton(authRoot_, L"",
        authButtonInset + authButtonW + authButtonGap, authButtonY, authButtonW, authButtonH,
        surfaceSoft, primarySoft, primarySoft, 24.f,
        [this]() { openSignup(); }, ink);
    signupButton->name = "openSignupButton";
    styleSecondary(signupButton);
    makeLabel(signupButton, L"회원가입", 0.f, 23.f, 26.f, authButtonW, 42.f,
        ink, UI::TextHAlign::Center);

    const float roomRowH = 128.f;
    const float roomRowY = 134.f;
    const float roomActionGap = 18.f;
    const float createRoomW = 190.f;
    const float joinBoxW = contentW - createRoomW - roomActionGap;

    roomSelectionRoot_ = mainPanel->addChild(std::make_unique<UI::UIElement>());
    roomSelectionRoot_->name = "roomSelectionRoot";
    applyRect(roomSelectionRoot_, UI::Anchors::TopLeft, UI::Pivots::TopLeft,
        contentX, 24.f, contentW, roomRowY + roomRowH);

    // Temporary local profile presentation. The framed square and placeholder
    // name reserve the final server-backed portrait/nickname area.
    constexpr float profileBoxSize = 116.f;
    profileImageBox_ = stylePanel(makeSolid(roomSelectionRoot_, "profileImageBox",
        0.f, 0.f, profileBoxSize, profileBoxSize, surfaceSoft, 1));
    makeLabel(roomSelectionRoot_, L"닉네임", 140.f, 16.f, 18.f,
        contentW - 140.f, 26.f, muted);
    profileNicknameLabel_ = makeLabel(roomSelectionRoot_, L"PLAYER",
        140.f, 44.f, 36.f, contentW - 140.f, 54.f, ink);

    // Room-code input + join (joinRoomForm in the prototype), paired with the
    // create-room action on the right as in the reference layout.
    {
        auto* joinBox = makeSolid(roomSelectionRoot_, "joinBox", 0.f, roomRowY,
            joinBoxW, roomRowH, surface);
        if (lobbyPanelTex_.res) {
            joinBox->texNormal     = &lobbyPanelTex_;
            joinBox->sliceUvBorderX = 0.30f; joinBox->sliceUvBorderY = 0.30f;
            joinBox->sliceCornerX   = 22.f;  joinBox->sliceCornerY   = 22.f;
        } else {
            joinBox->bgColor = surfaceSoft;
        }
    }
    makeLabel(roomSelectionRoot_, L"방 코드로 참가", 20.f, roomRowY + 12.f,
        20.f, joinBoxW - 40.f, 30.f, ink);
    {
        roomCodeInput_ = static_cast<UI::TextInput*>(
            roomSelectionRoot_->addChild(std::make_unique<UI::TextInput>()));
        roomCodeInput_->name    = "roomCodeInput";
        applyRect(roomCodeInput_, UI::Anchors::TopLeft, UI::Pivots::TopLeft,
            20.f, roomRowY + 58.f, joinBoxW - 152.f, 60.f);
        roomCodeInput_->uppercase = true;
        roomCodeInput_->alnumOnly = true;
        roomCodeInput_->setMaxLength(6);
        roomCodeInput_->setFontSize(20.f);
        roomCodeInput_->setPlaceholder(L"방 코드 입력 (예: JOIN01)");
        roomCodeInput_->zOrder = 2;
        roomCodeInput_->onSubmit = [this](const std::wstring& code) {
            if (callbacks_.onJoinRoom) callbacks_.onJoinRoom(std::string(code.begin(), code.end()));
        };
        styleInput(roomCodeInput_);

        auto* joinBtn = makeButton(roomSelectionRoot_, L"",
            joinBoxW - 124.f, roomRowY + 58.f, 104.f, 60.f,
            primary, primaryDark, primaryDark, 22.f, [this]() {
                const std::wstring& w = roomCodeInput_->text();
                if (callbacks_.onJoinRoom) callbacks_.onJoinRoom(std::string(w.begin(), w.end()));
            }, { 1.f, 1.f, 1.f, 1.f });
        joinBtn->name    = "joinButton";
        stylePrimary(joinBtn);
        makeLabel(joinBtn, L"참가", 0.f, 15.f, 22.f, 104.f, 30.f,
            { 1.f, 1.f, 1.f, 1.f }, UI::TextHAlign::Center);
    }

    auto* createBtn = makeButton(roomSelectionRoot_, L"",
        joinBoxW + roomActionGap, roomRowY, createRoomW, roomRowH,
        primary, primaryDark, primaryDark, 24.f,
        [this]() { if (callbacks_.onCreateRoom) callbacks_.onCreateRoom(); },
        { 1.f, 1.f, 1.f, 1.f });
    createBtn->name = "createRoomButton";
    stylePrimary(createBtn);
    makeLabel(createBtn, L"방 만들기", 0.f, 26.f, 26.f, createRoomW, 44.f,
        { 1.f, 1.f, 1.f, 1.f }, UI::TextHAlign::Center);
    makeLabel(createBtn, L"CREATE ROOM", 0.f, 78.f, 14.f, createRoomW, 24.f,
        { 1.f, 1.f, 1.f, 0.82f }, UI::TextHAlign::Center);

    const float quietW = (contentW - authButtonGap) * 0.5f;
    const float quietY = authButtonY + authButtonH + authRowGap;
    const float quietH = 64.f;
    auto* settingsButton = makeButton(mainPanel, L"", contentX, quietY, quietW, quietH,
        surfaceSoft, { 0.847f, 0.937f, 0.988f, 1.f }, primarySoft, 24.f,
        [this]() { if (callbacks_.onOpenSettings) callbacks_.onOpenSettings(); }, ink);
    settingsButton->name = "settingsButton";
    styleSecondary(settingsButton);
    makeLabel(settingsButton, L"설정", 0.f, 13.f, 24.f, quietW, 38.f,
        ink, UI::TextHAlign::Center);

    auto* quitButton = makeButton(mainPanel, L"",
        contentX + quietW + authButtonGap, quietY, quietW, quietH,
        surfaceSoft, { 0.847f, 0.937f, 0.988f, 1.f }, primarySoft, 24.f,
        [this]() { if (callbacks_.onQuit) callbacks_.onQuit(); }, ink);
    quitButton->name = "quitButton";
    styleSecondary(quitButton);
    makeLabel(quitButton, L"종료", 0.f, 13.f, 24.f, quietW, 38.f,
        ink, UI::TextHAlign::Center);

    mainMenuMsgLabel_ = makeLabel(mainPanel, L"", contentX, quietY + quietH + authRowGap,
        19.f, contentW, 30.f,
        { 0.70f, 0.24f, 0.24f, 1.f }, UI::TextHAlign::Center);

    // ---- Registration modal ----
    // Every interactive child uses a zOrder above the scrim because UIManager's
    // hit test compares interactive widgets globally rather than accumulating
    // their parents' zOrder.
    signupRoot_ = lobbyRoot_->addChild(std::make_unique<UI::UIElement>());
    signupRoot_->name = "signupRoot";
    applyRect(signupRoot_, UI::Anchors::TopLeft, UI::Pivots::TopLeft,
        0.f, 0.f, screenW, screenH);
    signupRoot_->zOrder  = 40;
    signupRoot_->visible = false;

    auto* signupScrim = makeSolid(signupRoot_, "signupScrim", 0.f, 0.f, screenW, screenH,
        { 0.04f, 0.07f, 0.10f, 0.52f }, 90);
    signupScrim->interactive = true;

    const float signupPanelW = std::min(460.f, std::max(340.f, screenW - 80.f));
    const float signupPanelH = 432.f;
    auto* signupPanel = makeGroup(signupRoot_, "signupPanel", signupPanelW, signupPanelH);
    signupPanel->zOrder = 91;
    stylePanel(makeSolid(signupPanel, "signupPanelBg", 0.f, 0.f,
        signupPanelW, signupPanelH, surface));

    makeLabel(signupPanel, L"회원가입", 34.f, 20.f, 32.f, signupPanelW - 68.f, 44.f, ink);
    makeLabel(signupPanel, L"아이디", 34.f, 68.f, 18.f, signupPanelW - 68.f, 24.f, ink);
    signupIdInput_ = static_cast<UI::TextInput*>(
        signupPanel->addChild(std::make_unique<UI::TextInput>()));
    signupIdInput_->name = "signupIdInput";
    applyRect(signupIdInput_, UI::Anchors::TopLeft, UI::Pivots::TopLeft,
        34.f, 96.f, signupPanelW - 68.f, 52.f);
    signupIdInput_->setFontSize(19.f);
    signupIdInput_->setPlaceholder(L"아이디 입력");
    signupIdInput_->zOrder = 94;
    styleInput(signupIdInput_);

    makeLabel(signupPanel, L"비밀번호", 34.f, 154.f, 18.f, signupPanelW - 68.f, 24.f, ink);
    signupPasswordInput_ = static_cast<UI::TextInput*>(
        signupPanel->addChild(std::make_unique<UI::TextInput>()));
    signupPasswordInput_->name = "signupPasswordInput";
    applyRect(signupPasswordInput_, UI::Anchors::TopLeft, UI::Pivots::TopLeft,
        34.f, 182.f, signupPanelW - 68.f, 52.f);
    signupPasswordInput_->setFontSize(19.f);
    signupPasswordInput_->setPlaceholder(L"비밀번호 입력");
    signupPasswordInput_->setPasswordMode(true);
    signupPasswordInput_->zOrder = 94;
    signupPasswordInput_->onSubmit = [this](const std::wstring&) { submitRegistration(); };
    styleInput(signupPasswordInput_);

    makeLabel(signupPanel, L"닉네임", 34.f, 240.f, 18.f, signupPanelW - 68.f, 24.f, ink);
    signupNicknameInput_ = static_cast<UI::TextInput*>(
        signupPanel->addChild(std::make_unique<UI::TextInput>()));
    signupNicknameInput_->name = "signupNicknameInput";
    applyRect(signupNicknameInput_, UI::Anchors::TopLeft, UI::Pivots::TopLeft,
        34.f, 268.f, signupPanelW - 68.f, 52.f);
    signupNicknameInput_->setFontSize(19.f);
    signupNicknameInput_->setPlaceholder(L"닉네임 입력");
    signupNicknameInput_->zOrder = 94;
    signupNicknameInput_->onSubmit = [this](const std::wstring&) { submitRegistration(); };
    styleInput(signupNicknameInput_);

    signupMsgLabel_ = makeLabel(signupPanel, L"", 34.f, 326.f, 15.f,
        signupPanelW - 68.f, 22.f, { 0.70f, 0.24f, 0.24f, 1.f },
        UI::TextHAlign::Center, 94);

    const float signupButtonW = (signupPanelW - 80.f) * 0.5f;
    auto* submitSignupButton = makeButton(signupPanel, L"가입", 34.f, 354.f,
        signupButtonW, 48.f, primary, primaryDark, primaryDark, 20.f,
        [this]() { submitRegistration(); }, { 1.f, 1.f, 1.f, 1.f });
    submitSignupButton->name = "submitSignupButton";
    submitSignupButton->zOrder = 94;
    stylePrimary(submitSignupButton);

    auto* closeSignupButton = makeButton(signupPanel, L"닫기",
        46.f + signupButtonW, 354.f, signupButtonW, 48.f,
        surfaceSoft, primarySoft, primarySoft, 20.f,
        [this]() { closeSignup(); }, ink);
    closeSignupButton->name = "closeSignupButton";
    closeSignupButton->zOrder = 94;
    styleSecondary(closeSignupButton);

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

    // Squad stage (left ~60% of the panel width) + story panel (right ~40%),
    // split across innerW with one gap between the two blocks.
    const float slotsY     = toolbarY + toolbarH + 16.f;
    const float debugH     = 40.f;
    const float slotsH     = roomPanelH - slotsY - debugH - 14.f;
    const float storyGap   = gap;                       // separation between squad block and story
    const float squadAreaW = innerW * 0.6f;             // 4 player slots packed into this width
    const float storyW     = innerW * 0.4f - storyGap;  // story panel
    const float storyX     = pad + squadAreaW + storyGap;
    const float slotW      = (squadAreaW - 3.f * gap) / 4.f;
    const float nameH      = 92.f;
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

        // The portrait RT cell has a fixed aspect (kPortraitCellW:kPortraitCellH).
        // The slot cell's aspect varies with the screen aspect (full-bleed lobby
        // layout), so letterbox the bay to the portrait aspect and center it inside
        // the cell — otherwise the character stretches when the resolution's aspect
        // ratio changes. slotPanelBg keeps filling the full cell behind the margins.
        const float cellX = x + 8.f;
        const float cellY = slotsY + 8.f;
        const float cellW = slotW - 16.f;
        const float cellH = slotsH - nameH - 22.f;
        const float portraitAspect =
            static_cast<float>(GFX::kPortraitCellW) / static_cast<float>(GFX::kPortraitCellH);
        float bayW = cellW;
        float bayH = cellH;
        if (cellW / std::max(1.f, cellH) > portraitAspect) {
            bayH = cellH;
            bayW = bayH * portraitAspect;   // pillarbox (left/right margins)
        } else {
            bayW = cellW;
            bayH = bayW / portraitAspect;   // letterbox (top/bottom margins)
        }
        const float bayX = cellX + (cellW - bayW) * 0.5f;
        const float bayY = cellY + (cellH - bayH) * 0.5f;

        auto* bay = roomPanel->addChild(std::make_unique<UI::Image>());
        bay->name = "slotBay";
        applyRect(bay, UI::Anchors::TopLeft, UI::Pivots::TopLeft,
            bayX, bayY, bayW, bayH);
        bay->zOrder  = 1;
        bay->visible = false;
        slotBays_[i] = static_cast<UI::Image*>(bay);

        const float nameY = slotsY + slotsH - nameH - 4.f;
        slotNameplateBgs_[i] = makeSolid(roomPanel, "nameplateBg",
            x + 10.f, nameY, slotW - 20.f, 88.f, slotNameplateCol, 1);
        slotNameLabels_[i] = makeLabel(roomPanel, L"대기 중",
            x + 10.f, nameY, 18.f, slotW - 20.f, 28.f,
            textLight, UI::TextHAlign::Center, 3);
        slotHostBadgeLabels_[i] = makeLabel(roomPanel, L"",
            x + 10.f, nameY + 66.f, 12.f, slotW - 20.f, 20.f,
            textMuted, UI::TextHAlign::Center, 3);

        auto* weaponRoot = roomPanel->addChild(std::make_unique<UI::UIElement>());
        weaponRoot->name = "weaponSelector";
        weaponRoot->zOrder = 3;
        applyRect(weaponRoot, UI::Anchors::TopLeft, UI::Pivots::TopLeft,
            x + 14.f, nameY + 30.f, slotW - 28.f, 34.f);
        slotWeaponRoots_[i] = weaponRoot;

        const float weaponRowW = slotW - 28.f;
        const float arrowW = 22.f;
        const float iconSize = 28.f;
        const float labelX = arrowW + iconSize + 8.f;
        const float labelW = std::max(42.f, weaponRowW - labelX - arrowW - 4.f);

        auto* prevBtn = static_cast<UI::Button*>(weaponRoot->addChild(std::make_unique<UI::Button>()));
        prevBtn->name = "weaponPrev";
        applyRect(prevBtn, UI::Anchors::TopLeft, UI::Pivots::TopLeft, 0.f, 4.f, arrowW, 26.f);
        prevBtn->texNormal = lobbyLeftButtonTex_.res ? &lobbyLeftButtonTex_ : nullptr;
        prevBtn->texTint        = { 0.95f, 0.97f, 1.f, 0.88f };
        prevBtn->texTintHovered = { 1.15f, 1.15f, 1.f, 1.f };
        prevBtn->texTintPressed = { 0.75f, 0.78f, 0.82f, 1.f };
        prevBtn->onClick = [this]() { if (callbacks_.onSelectWeapon) callbacks_.onSelectWeapon(-1); };
        slotWeaponPrevButtons_[i] = prevBtn;

        auto* icon = weaponRoot->addChild(std::make_unique<UI::Image>());
        icon->name = "weaponIcon";
        applyRect(icon, UI::Anchors::TopLeft, UI::Pivots::TopLeft, arrowW + 4.f, 3.f, iconSize, iconSize);
        icon->zOrder = 1;
        slotWeaponIcons_[i] = static_cast<UI::Image*>(icon);

        slotWeaponLabels_[i] = makeLabel(weaponRoot, L"", labelX, 3.f, 13.f, labelW, 28.f,
            textLight, UI::TextHAlign::Center, 2);

        auto* nextBtn = static_cast<UI::Button*>(weaponRoot->addChild(std::make_unique<UI::Button>()));
        nextBtn->name = "weaponNext";
        applyRect(nextBtn, UI::Anchors::TopLeft, UI::Pivots::TopLeft,
            weaponRowW - arrowW, 4.f, arrowW, 26.f);
        nextBtn->texNormal = lobbyLeftButtonTex_.res ? &lobbyLeftButtonTex_ : nullptr;
        nextBtn->uvScaleBias = { -1.f, 1.f, 1.f, 0.f };
        nextBtn->texTint        = { 0.95f, 0.97f, 1.f, 0.88f };
        nextBtn->texTintHovered = { 1.15f, 1.15f, 1.f, 1.f };
        nextBtn->texTintPressed = { 0.75f, 0.78f, 0.82f, 1.f };
        nextBtn->onClick = [this]() { if (callbacks_.onSelectWeapon) callbacks_.onSelectWeapon(1); };
        slotWeaponNextButtons_[i] = nextBtn;

        // Gold rectangular border around the host name (4 edges, zOrder 2). Hidden by
        // default; shown only for the host in refresh().
        const XMFLOAT4 borderCol = { 0.95f, 0.80f, 0.35f, 1.f };
        const float bpad = 4.f, bt = 2.f;
        const float bx = x + 10.f - bpad, by = nameY - bpad;
        const float bw = (slotW - 20.f) + 2.f * bpad, bh = 88.f + 2.f * bpad;
        slotNameBorders_[i][0] = makeSolid(roomPanel, "nameBorderT", bx, by, bw, bt, borderCol, 2);
        slotNameBorders_[i][1] = makeSolid(roomPanel, "nameBorderB", bx, by + bh - bt, bw, bt, borderCol, 2);
        slotNameBorders_[i][2] = makeSolid(roomPanel, "nameBorderL", bx, by, bt, bh, borderCol, 2);
        slotNameBorders_[i][3] = makeSolid(roomPanel, "nameBorderR", bx + bw - bt, by, bt, bh, borderCol, 2);
        for (auto* edge : slotNameBorders_[i]) {
            if (edge) edge->visible = false;
        }
    }

    // ---- Story panel (right column): static text read from a file ----
    // Scroll is out of scope for now; the body text lives under its own content
    // node so a scroll view can later wrap/clip it. The file is pre-wrapped to the
    // column width (auto word-wrap happens at the 1024px font bitmap, not the rect).
    {
        // The panel art (ui_panel_frame) is a cream/yellow frame, so the text is
        // dark for contrast. The solid fallback (used only if the texture fails to
        // load) is also light so the dark text stays readable either way.
        const XMFLOAT4 storyBgCol    = { 0.93f, 0.90f, 0.82f, 0.92f };
        const XMFLOAT4 storyTitleCol = { 0.12f, 0.10f, 0.08f, 1.f };
        const XMFLOAT4 storyBodyCol  = { 0.16f, 0.14f, 0.11f, 1.f };
        const float    storyPadX     = 16.f;
        const float    storyTitleH   = 28.f;

        storyPanelBg_ = makeSolid(roomPanel, "storyPanelBg", storyX, slotsY, storyW, slotsH, storyBgCol, 0);
        if (lobbyPanelTex_.res) {
            storyPanelBg_->texNormal      = &lobbyPanelTex_;
            storyPanelBg_->sliceUvBorderX = 0.30f; storyPanelBg_->sliceUvBorderY = 0.30f;
            storyPanelBg_->sliceCornerX   = 22.f;  storyPanelBg_->sliceCornerY   = 22.f;
            storyPanelBg_->texTint        = { 1.f, 1.f, 1.f, 0.90f };
        }

        storyTitleLabel_ = makeLabel(roomPanel, L"스토리", storyX + storyPadX, slotsY + 12.f, 18.f,
            storyW - 2.f * storyPadX, storyTitleH, storyTitleCol, UI::TextHAlign::Leading, 2);

        const float bodyX = storyX + storyPadX;
        const float bodyY = slotsY + 12.f + storyTitleH + 8.f;
        const float bodyW = storyW - 2.f * storyPadX;
        const float bodyH = slotsH - (bodyY - slotsY) - storyPadX;

        // Content viewport node (future scroll clip region). zOrder must beat the
        // panel background: renderTree sorts siblings by zOrder with an UNSTABLE sort,
        // so a content node left at the default 0 can be drawn behind storyPanelBg_
        // (also 0) and the cream panel then covers the text. Put it above the panel.
        storyContentRoot_ = roomPanel->addChild(std::make_unique<UI::UIElement>());
        storyContentRoot_->name   = "storyContentRoot";
        storyContentRoot_->zOrder = 2;
        applyRect(storyContentRoot_, UI::Anchors::TopLeft, UI::Pivots::TopLeft, bodyX, bodyY, bodyW, bodyH);

        std::wstring story = loadStoryTextUtf8(L"../resources/story/intro.txt");
        if (story.empty()) {
            story = L"이야기를 불러오지 못했습니다.";
        }

        // Stack one single-line Label per display line. A single Label cannot exceed
        // the 256px font bitmap (CreateBitmapFromText then fails and nothing draws),
        // so the body is never one tall multi-line label. Each source line is first
        // word-wrapped to the column width, then each resulting display line gets a
        // label. measureText/createFont work in physical pixels, so the font size and
        // wrap width are scaled by uiScale.
        const float bodyFontSize = 15.f;
        const float lineH        = 22.f;   // layout-space line pitch (> font size for leading)
        const int   maxLines     = std::max(1, static_cast<int>(bodyH / lineH));
        const float uiScale      = uiManager.uiScale();
        FontHandle  bodyFont     = gfx_ ? gfx_->createFont(bodyFontSize * uiScale) : FontHandle{};
        const float wrapWidthPx  = bodyW * uiScale;

        storyLineLabels_.clear();
        std::size_t start = 0;
        int  row  = 0;
        bool done = false;
        while (!done && row < maxLines) {
            const std::size_t nl = story.find(L'\n', start);
            const std::wstring srcLine = (nl == std::wstring::npos)
                ? story.substr(start)
                : story.substr(start, nl - start);
            if (nl == std::wstring::npos) done = true; else start = nl + 1;

            const std::vector<std::wstring> displayLines =
                gfx_ ? wrapToWidth(gfx_, &bodyFont, srcLine, wrapWidthPx)
                     : std::vector<std::wstring>{ srcLine };

            for (const auto& disp : displayLines) {
                if (row >= maxLines) break;
                auto* lineLabel = makeLabel(storyContentRoot_, disp, 0.f, static_cast<float>(row) * lineH,
                    bodyFontSize, bodyW, lineH, storyBodyCol, UI::TextHAlign::Leading, 2);
                lineLabel->setTextVAlign(UI::TextVAlign::Center);
                storyLineLabels_.push_back(lineLabel);
                ++row;
            }
        }
    }

    // Full-screen loading overlay (built last so it sits on top by zOrder).
    buildLoadingScreen(uiManager);
}

// Black full-screen loading overlay: centered logo, "loading..." text, a progress
// bar, and a 12-dot ring spinner. Hidden by default; toggled from updateLoading().
void LobbyUI::buildLoadingScreen(UI::UIManager& uiManager) {
    if (!lobbyRoot_) return;

    const float screenX = uiManager.screenToLayoutX(0.f);
    const float screenY = uiManager.screenToLayoutY(0.f);
    const float screenW = uiManager.screenWidth() / uiManager.uiScale();
    const float screenH = uiManager.screenHeight() / uiManager.uiScale();
    constexpr int kZ = 100;  // above all lobby UI

    loadingRoot_ = lobbyRoot_->addChild(std::make_unique<UI::UIElement>());
    loadingRoot_->name    = "loadingRoot";
    loadingRoot_->anchor  = UI::Anchors::TopLeft;
    loadingRoot_->pivot   = UI::Pivots::TopLeft;
    loadingRoot_->offsetX = UI::DimValue::px(screenX);
    loadingRoot_->offsetY = UI::DimValue::px(screenY);
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
    loadingTextLabel_->setFontFamily(kLobbyFontFamily);
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

void LobbyUI::openSignup() {
    setSignupMessage(L"");
    if (signupRoot_) signupRoot_->visible = true;
}

void LobbyUI::closeSignup() {
    if (signupRoot_) signupRoot_->visible = false;
}

void LobbyUI::submitLogin() {
    if (callbacks_.onLogin && loginIdInput_ && loginPasswordInput_) {
        callbacks_.onLogin(loginIdInput_->text(), loginPasswordInput_->text());
    }
}

void LobbyUI::submitRegistration() {
    if (callbacks_.onRegister && signupIdInput_ &&
        signupPasswordInput_ && signupNicknameInput_) {
        callbacks_.onRegister(signupIdInput_->text(),
            signupPasswordInput_->text(), signupNicknameInput_->text());
    }
}

void LobbyUI::setMainMenuMessage(const std::wstring& msg, bool isError) {
    if (!mainMenuMsgLabel_) return;

    mainMenuMsgLabel_->setText(msg);
    if (isError) {
        mainMenuMsgLabel_->setTextColor(0.70f, 0.24f, 0.24f, 1.f);
    } else {
        mainMenuMsgLabel_->setTextColor(0.10f, 0.42f, 0.27f, 1.f);
    }
}

void LobbyUI::setSignupMessage(const std::wstring& msg) {
    if (signupMsgLabel_) signupMsgLabel_->setText(msg);
}

void LobbyUI::completeRegistration(const std::wstring& id) {
    if (loginIdInput_) loginIdInput_->setText(id);
    clearLoginPassword();

    if (signupIdInput_) signupIdInput_->clear();
    if (signupPasswordInput_) signupPasswordInput_->clear();
    if (signupNicknameInput_) signupNicknameInput_->clear();
    setSignupMessage(L"");
    closeSignup();
    setMainMenuMessage(L"회원가입이 완료되었습니다. 로그인해 주세요.", false);
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

    if (authRoot_) authRoot_->visible = inMain && !s.isAuthenticated;
    if (roomSelectionRoot_) roomSelectionRoot_->visible = inMain && s.isAuthenticated;
    if (profileNicknameLabel_) {
        profileNicknameLabel_->setText(
            s.nickname.empty() ? std::wstring(L"PLAYER") : s.nickname);
    }

    if (!s.inLobbyScene || !inMain || s.isAuthenticated) {
        closeSignup();
    }

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
            if (slotWeaponRoots_[i]) slotWeaponRoots_[i]->visible = true;
            const int widx = weaponIndex(player.weaponType);
            if (slotWeaponIcons_[i]) {
                slotWeaponIcons_[i]->texture = lobbyWeaponIconTex_[widx].res ? &lobbyWeaponIconTex_[widx] : nullptr;
            }
            if (slotWeaponLabels_[i]) {
                slotWeaponLabels_[i]->setText(weaponDisplayName(player.weaponType));
                slotWeaponLabels_[i]->setTextColor(textLight.x, textLight.y, textLight.z, textLight.w);
            }
            if (slotWeaponPrevButtons_[i]) slotWeaponPrevButtons_[i]->visible = player.isSelf;
            if (slotWeaponNextButtons_[i]) slotWeaponNextButtons_[i]->visible = player.isSelf;
            for (auto* edge : slotNameBorders_[i]) {
                if (edge) edge->visible = player.isHost;
            }
        } else {
            if (slotNameLabels_[i]) {
                slotNameLabels_[i]->setText(L"대기 중");
                slotNameLabels_[i]->setTextColor(textMuted.x, textMuted.y, textMuted.z, textMuted.w);
            }
            if (slotHostBadgeLabels_[i]) slotHostBadgeLabels_[i]->setText(L"");
            if (slotWeaponRoots_[i]) slotWeaponRoots_[i]->visible = false;
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

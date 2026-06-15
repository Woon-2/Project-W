#ifndef __LOBBY_UI_HPP
#define __LOBBY_UI_HPP

// No Korean comments in new files (encoding issue per CLAUDE.md). Korean string
// literals (UI captions) are intentional and preserved.

#include <array>
#include <functional>
#include <string>
#include <vector>

#include "../gfx.hpp"
#include "../ui/UIManager.hpp"
#include "../ui/widgets/ProgressBar.hpp"
#include "../ui/widgets/Button.hpp"
#include "../ui/widgets/Label.hpp"
#include "../ui/widgets/Image.hpp"
#include "../ui/widgets/TextInput.hpp"

namespace Online {

// Lobby 2D UI layer: main menu, the squad-stage waiting room overlay and the
// loading overlay, plus the lobby UI textures. Owns the widget pointers (the
// widgets themselves live in the UIManager tree) and the lobby texture handles.
//
// The lobby *scene* (3D background, networking, scene state machine, asset-load
// orchestration) stays in Online::Game; Game drives this class through build(),
// refresh() and updateLoading(). Button actions are routed back to Game through
// the Callbacks set at build() time. The settings window is a separate,
// scene-agnostic UI::SettingsPanel owned by Game (not part of this class).
class LobbyUI {
public:
    static constexpr int kMaxLobbyPlayers = 4;

    // Actions invoked from lobby buttons; wired by Game at build() time.
    struct Callbacks {
        std::function<void()>                    onCreateRoom;
        std::function<void(const std::string&)>  onJoinRoom;
        std::function<void()>                    onLeaveRoom;
        std::function<void()>                    onStartGame;
        std::function<void()>                    onCopyCode;
        std::function<void()>                    onOpenSettings;
        std::function<void()>                    onQuit;
    };

    // Snapshot of lobby/session state passed to refresh() each time it changes.
    struct PlayerSlot {
        std::wstring name;
        bool         isHost = false;
    };
    struct ViewState {
        bool        inLobbyScene       = true;   // scene_ == Scene::Lobby
        bool        inMainMenu         = true;   // lobbyState_ == LobbyState::MainMenu
        bool        waitingRoom3DReady = false;  // stageVisualReady_
        bool        isHost             = false;
        std::string roomCode;
        std::vector<PlayerSlot> players;
        int         maxPlayers         = kMaxLobbyPlayers;
    };

    // Loads the lobby UI textures on the main thread (synchronous flush). Must run
    // before build() so the widgets can reference the textures.
    void loadTextures(GFX& gfx);

    // Builds the lobby widget tree under uiManager.root() and the loading overlay.
    void build(UI::UIManager& uiManager, const Callbacks& callbacks);

    // Re-applies widget visibility/text from a lobby state snapshot.
    void refresh(const ViewState& state);

    // Toggles the loading overlay; while visible, pushes progress to the bar and
    // animates the spinner. progress01 is the caller-computed load fraction [0..1].
    void updateLoading(float deltaTimeSec, bool visible, float progress01);

    // --- Per-frame access needed by Game's lobby scene rendering ---
    UI::Image* slotBay(int i) const { return slotBays_[i]; }

    // Hides every portrait slot image (called when lobby display characters are cleared).
    void hideAllSlotBays();

    // --- Targeted toggles used by scene transitions ---
    void setRootVisible(bool v)            { if (lobbyRoot_) lobbyRoot_->visible = v; }
    void setLoadingVisible(bool v)         { if (loadingRoot_) loadingRoot_->visible = v; }
    void setFlatBackgroundVisible(bool v);
    void setMainMenuMessage(const std::wstring& msg) { if (mainMenuMsgLabel_) mainMenuMsgLabel_->setText(msg); }
    void clearRoomCodeInput()              { if (roomCodeInput_) roomCodeInput_->clear(); }

    // Textures shared with the settings panel (9-slice frames).
    const Texture* panelTexture()            const { return &lobbyPanelTex_; }
    const Texture* secondaryButtonTexture()  const { return &lobbyBtnSecondaryTex_; }

private:
    void buildLoadingScreen(UI::UIManager& uiManager);

    Callbacks callbacks_{};

    // Cached GFX for build-time text measurement (set in loadTextures, used to
    // word-wrap the story column). Stable across rebuilds (same GFX instance).
    GFX* gfx_ = nullptr;

    // --- Lobby UI textures (loaded on the main thread, owned here) ---
    std::unordered_map<std::string, Texture> lobbyTexHashMap_{};
    Texture lobbyBgTex_{};
    Texture lobbyLogoTex_{};
    Texture lobbyPanelTex_{};
    Texture lobbyBtnPrimaryTex_{};
    Texture lobbyBtnSecondaryTex_{};
    Texture lobbyInputTex_{};

    // --- Widget pointers (owned by the UIManager tree) ---
    UI::UIElement* lobbyRoot_       = nullptr;
    UI::Button*    lobbySkyBg_      = nullptr;
    UI::Image*     lobbyBgImage_    = nullptr;
    UI::Image*     lobbyLogoImage_  = nullptr;
    UI::UIElement* mainMenuRoot_    = nullptr;
    UI::UIElement* waitingRoomRoot_ = nullptr;
    UI::TextInput* roomCodeInput_   = nullptr;
    UI::Label*     mainMenuMsgLabel_= nullptr;
    UI::Label*     roomCodeLabel_   = nullptr;
    UI::Label*     playerCountLabel_= nullptr;

    std::array<UI::Image*,  kMaxLobbyPlayers> slotBays_{};
    std::array<UI::Button*, kMaxLobbyPlayers> slotPanels_{};
    std::array<UI::Button*, kMaxLobbyPlayers> slotNameplateBgs_{};
    std::array<UI::Label*,  kMaxLobbyPlayers> slotNumberLabels_{};
    std::array<UI::Label*,  kMaxLobbyPlayers> slotNameLabels_{};
    std::array<UI::Label*,  kMaxLobbyPlayers> slotHostBadgeLabels_{};
    std::array<std::array<UI::Button*, 4>, kMaxLobbyPlayers> slotNameBorders_{};

    UI::Button*    startGameButton_ = nullptr;
    UI::Label*     startGameLabel_  = nullptr;
    UI::Button*    waitMessageBg_   = nullptr;
    UI::Label*     hostStatusLabel_ = nullptr;

    // --- Story panel (waiting room, right column). Static text read from a file.
    // Scroll is not implemented yet; the body text sits under its own content node
    // so a future scroll view can wrap/clip it without touching the layout above. ---
    UI::Button*    storyPanelBg_     = nullptr;
    UI::Label*     storyTitleLabel_  = nullptr;
    UI::UIElement* storyContentRoot_ = nullptr;
    // One Label per text line: a single Label cannot exceed the 256px font bitmap
    // height, so the body is stacked as single-line labels inside storyContentRoot_.
    std::vector<UI::Label*> storyLineLabels_{};

    // --- Loading overlay ---
    UI::UIElement*   loadingRoot_      = nullptr;
    UI::ProgressBar* loadingBar_       = nullptr;
    UI::Label*       loadingTextLabel_ = nullptr;
    std::array<UI::Button*, 12> spinnerDots_{};
    float            loadingSpinTime_  = 0.f;
};

}   // namespace Online

#endif  // __LOBBY_UI_HPP

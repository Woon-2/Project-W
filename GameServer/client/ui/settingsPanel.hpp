#ifndef __SETTINGS_PANEL_HPP
#define __SETTINGS_PANEL_HPP

// No Korean comments in new files (encoding issue per CLAUDE.md).

#include <string>
#include <vector>
#include <functional>

struct Texture;

// A selectable windowed resolution (client area, pixels). The list of choices is
// built at runtime and filtered to the current monitor (see Game), so options that
// don't fit the display are never offered.
struct Resolution {
    int w = 0;
    int h = 0;
};

namespace UI {
class UIManager;
class UIElement;
class Button;
class Label;
}

// Persistent, gameplay-facing settings edited through SettingsPanel. Owned by the
// game so the lobby, the in-game HUD and gameplay systems all read the same values.
struct GameSettings {
    bool fullscreen           = false;  // default windowed -> resolution control is enabled from the start
    bool allyDamageVisible    = true;
    int  resolutionIndex      = 0;    // index into the runtime windowed-resolution list (0 = smallest/default)
    int  monsterDamageOpacity = 100;  // percent, [0..100]
};

namespace UI {

// Scene-agnostic settings window. Mounted on uiManager.root() at a high zOrder and
// toggled with open()/close(), so the same panel overlays either the lobby main
// menu or (later) the in-game scene via ESC. All edits write through to a
// caller-owned GameSettings instance.
class SettingsPanel {
public:
    // Build the widget tree under uiManager.root(). panelTex/buttonTex are optional
    // 9-slice frame textures for the scrim background and the close button (null ->
    // solid-color fallback). `resolutions` is the monitor-filtered windowed-resolution
    // list the < > stepper iterates over (resolutionIndex indexes into it). The
    // referenced GameSettings must outlive this panel.
    void build(UIManager& uiManager, const Texture* panelTex, const Texture* buttonTex,
               GameSettings& settings, const std::vector<Resolution>& resolutions,
               std::function<void()> onQuitGame = {});

    void open();
    void close();
    void toggle();
    bool isOpen() const { return open_; }

    // Re-apply button highlight states and value labels from the current settings.
    void refreshPreview();

private:
    GameSettings* settings_ = nullptr;

    // All widgets below are owned by the UIManager tree (non-owning pointers).
    UIElement* root_ = nullptr;
    Button*    closeButton_ = nullptr;
    Button*    windowedButton_ = nullptr;
    Button*    fullscreenButton_ = nullptr;
    Button*    resolutionPrevButton_ = nullptr;
    Button*    resolutionNextButton_ = nullptr;
    Label*     resolutionValueLabel_ = nullptr;
    Button*    allyDamageOffButton_ = nullptr;
    Button*    allyDamageOnButton_ = nullptr;
    Button*    monsterOpacityPrevButton_ = nullptr;
    Button*    monsterOpacityNextButton_ = nullptr;
    Button*    monsterOpacityFill_ = nullptr;
    Label*     monsterOpacityValueLabel_ = nullptr;
    Button*    quitGameButton_ = nullptr;

    float        monsterOpacityFillMaxWidth_ = 0.f;
    std::vector<Resolution> resolutions_;   // monitor-filtered windowed-resolution choices
    std::function<void()> onQuitGame_;
    bool         open_ = false;
};

}   // namespace UI

#endif  // __SETTINGS_PANEL_HPP

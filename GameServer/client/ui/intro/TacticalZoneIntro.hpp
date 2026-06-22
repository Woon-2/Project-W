#ifndef __UI_TACTICAL_ZONE_INTRO_HPP
#define __UI_TACTICAL_ZONE_INTRO_HPP

// No Korean comments in new files (encoding issue per CLAUDE.md).

#include "../UIManager.hpp"
#include "../widgets/Panel.hpp"
#include "../widgets/Label.hpp"
#include "../widgets/Image.hpp"

class AssetManager;

namespace UI {

// Arena entry title card: a timed, non-interactive overlay shown when the local
// player enters a tactical combat zone. The standard sequence is a banner +
// emblem + category/title reveal; the boss arena ("WallBoss") prepends a
// dedicated glitchy "WARNING" phase before that reveal.
//
// Owns its own UI subtree (mounted under UIManager::root) and animates entirely
// by driving widget alpha/offset/size each update — mirroring DialogueSystem's
// "self-contained overlay module" shape so onlineGame only owns + delegates.
class TacticalZoneIntro {
public:
    // Builds the (hidden) UI subtree under uiManager.root(). assetManager
    // supplies the banner / per-arena emblem / warning textures (looked up by
    // pointer, so textures still loading at init fill in later).
    void init(UIManager& uiManager, AssetManager& assetManager);

    // Starts the intro for an arena wall prefix:
    //   "WallHobgoblin" / "WallGrandbaum" / "WallIsys" / "WallBoss".
    // Unknown prefixes are ignored and return false (no state change).
    bool trigger(std::string_view arenaPrefix);

    // Advances the active intro animation; no-op when inactive.
    void update(float deltaTimeSec);

    bool active() const { return active_; }

private:
    // Lazily builds the widget tree once; guarded against re-entry.
    void buildUI();

    UIManager*    uiManager_    = nullptr;
    AssetManager* assetManager_ = nullptr;

    Panel* root_      = nullptr;
    Panel* scrim_     = nullptr;
    Image* banner_    = nullptr;
    Image* emblem_    = nullptr;
    Panel* lineLeft_  = nullptr;
    Panel* lineRight_ = nullptr;
    Label* category_  = nullptr;
    Label* title_     = nullptr;

    // Boss-only "WARNING" phase widgets (chromatic-aberration glitch).
    Panel* warningScrim_      = nullptr;
    Image* warningNoise_      = nullptr;
    Image* warningGlitchRed_  = nullptr;
    Image* warningGlitchCyan_ = nullptr;
    Label* warningText_       = nullptr;
    Label* warningTextRed_    = nullptr;
    Label* warningTextCyan_   = nullptr;

    float elapsed_    = 0.f;
    bool  active_     = false;
    bool  hasWarning_ = false;
};

}   // namespace UI

#endif  // __UI_TACTICAL_ZONE_INTRO_HPP

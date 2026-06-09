#ifndef __editor_editorController_HPP
#define __editor_editorController_HPP

// EditorController -- drives the in-game skill / monster-pattern authoring tool.
//
// Hosted by StandAlone::Game (which boots straight into editor mode). The game
// owns world setup (assets, scene, physics, gfx); the controller layers the
// editor on top: character & skill dropdowns, hitbox picking, keyboard nudge
// editing of a draft SkillAsset, slow-motion / pause, and a free-fly camera.
//
// Edits are never written back to .lua: the draft is played live, and a hotkey
// dumps an original/current/delta guide to the console (see SkillDraft).

#include "characterSkillMap.hpp"
#include "skillDraft.hpp"

#include "../camera.hpp"
#include "../debugBVView.hpp"
#include "../object.hpp"
#include "../skill/skillSystem.hpp"

#include "../ui/UIManager.hpp"
#include "../ui/widgets/Dropdown.hpp"
#include "../ui/widgets/Label.hpp"
#include "../ui/widgets/Panel.hpp"

#include <array>
#include <functional>
#include <memory>
#include <vector>

namespace Editor {

class Controller {
public:
    struct InitRefs {
        SkillSystem*            skillSystem = nullptr;
        SkillDispatchContext*   skillCtx    = nullptr;
        Camera*                 camera      = nullptr;
        UI::UIManager*          uiManager   = nullptr;
        DebugBVView*            debugBV     = nullptr;
        std::shared_ptr<Player> player;
        std::shared_ptr<Goblin> goblin;
        HWND                    hwnd        = nullptr;
        // Terrain height query (x,z)->y; used to drop the test dummy onto the ground.
        std::function<float(float, float)> terrainHeightAt;
    };

    void init(const InitRefs& refs);

    // Called from Game::processInput (raw keyboard/mouse state, window focused).
    void handleInput(const BYTE* cur, const BYTE* prev,
                     long mouseDx, long mouseDy, Milliseconds realDt);

    // Called where the game used to call camera_.update(); drives follow or free-fly.
    void updateCamera(Milliseconds realDt);

    // Per-frame refresh of the editor panel + selected-hitbox debug highlight.
    void refresh();

    // Simulation time multiplier (0 = paused). The host scales physics/skill/anim by this.
    float timeScale()  const { return timeScale_; }
    bool  cameraFree() const { return cameraFree_; }

private:
    // --- selection / playback ---
    void selectCharacter(int idx);
    void rebuildSkillDropdown(const std::vector<std::string>& items);
    void selectSkill(int idx);
    void play();
    void resetCharacters();
    void positionDummyInFront();   // place the target object in front of the caster, on the ground
    void clearHitboxSelection();   // ESC: leave hitbox editing

    // --- editing ---
    void moveCursor(int dir);
    void nudge(int dir, bool coarse);
    void pickHitbox();
    void revalidateSelection();

    // --- camera ---
    void enableFreeCamera(bool enable);

    // --- ui ---
    void buildUI();
    void refreshPanel();

    std::shared_ptr<Object> casterObj() const;
    std::shared_ptr<Object> targetObj() const;

    // --- refs ---
    SkillSystem*          skillSystem_ = nullptr;
    SkillDispatchContext* skillCtx_    = nullptr;
    Camera*               camera_      = nullptr;
    UI::UIManager*        uiManager_   = nullptr;
    DebugBVView*          debugBV_     = nullptr;
    std::shared_ptr<Player> player_;
    std::shared_ptr<Goblin> goblin_;
    HWND                  hwnd_        = nullptr;
    std::function<float(float, float)> terrainHeightAt_;

    // --- selection state ---
    CharacterKind            casterKind_ = CharacterKind::Player;
    i32t                     casterId_   = 0;
    i32t                     targetId_   = 1;
    std::vector<std::string> skillNames_;       // names backing the skill dropdown
    SkillDraft               draft_;
    int                      selectedDefIdx_   = -1;  // persists across replays
    int                      selectedHitboxIdx_ = -1; // live pool index (highlight + live sync)
    int                      fieldCursor_      = 0;

    // --- playback / time ---
    float timeScale_     = 1.f;
    float prevTimeScale_ = 1.f;  // restored when un-pausing

    // --- camera ---
    bool       cameraFree_  = false;
    mu::Radian camPitch_    = 0.f;
    mu::Radian camYaw_      = 0.f;
    mu::Vec3   freeEye_     {};
    float      freeYaw_     = 0.f;
    float      freePitch_   = 0.f;

    // --- spawn transforms (for reset) ---
    mu::Vec3  playerSpawnPos_{};  mu::NQuat playerSpawnOrient_{};
    mu::Vec3  goblinSpawnPos_{};  mu::NQuat goblinSpawnOrient_{};

    // --- UI (owned by uiManager_) ---
    UI::Dropdown* characterDropdown_ = nullptr;
    UI::Dropdown* skillDropdown_     = nullptr;
    UI::Panel*    panel_             = nullptr;
    UI::Label*    statusLabel_       = nullptr;
    UI::Label*    helpLabel_         = nullptr;  // controls cheat-sheet (top-right)
    static constexpr int kMaxPanelLines = 28;
    std::array<UI::Label*, kMaxPanelLines> panelLines_{};

    // scratch
    std::vector<SkillSystem::ActiveHitboxRef> activeScratch_;
};

}   // namespace Editor

#endif  // __editor_editorController_HPP

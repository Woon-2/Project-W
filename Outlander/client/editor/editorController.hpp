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
        // Monster caster hot-swap (setMonsterCaster): the model/animation source and
        // the AnimSystem to (un)track the swapped blender. Required only once monster
        // entries in kCharacterSkillMap are enabled; safe to leave set otherwise.
        const AssetManager*     assetManager = nullptr;
        AnimSystem*             animSystem   = nullptr;
        HWND                    hwnd        = nullptr;
        // Terrain height query (x,z)->y; used to drop the test dummy onto the ground.
        std::function<float(float, float)> terrainHeightAt;
        // Drains in-flight GPU work. Must be called before destroying UI widgets at
        // runtime: Labels own GPU text textures, and freeing one still referenced by an
        // in-flight command list is a use-after-free → device removed (TDR) → crash
        // (Release only; Debug's timing hides it). See rebuildSkillDropdown().
        std::function<void()> flushGpu;
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

    // Currently controlled object (player, or the hot-swapped monster caster rig).
    // Exposed for host-side debug tools (e.g. ragdoll toggle).
    std::shared_ptr<Object> controlledObject() const { return casterObj(); }

private:
    // --- selection / playback ---
    void selectCharacter(int idx);
    // Weapon dropdown (Player caster only): equips the weapon model, switches the
    // player's animation clip set, and re-filters the skill list to that weapon.
    void selectWeapon(int idx);
    void applyWeaponToPlayer();
    // Rebuilds skillNames_ + the skill dropdown for the current caster. For the
    // Player the list is filtered by the selected weapon (skills of other weapons
    // are dropped; weapon-less skills stay at the end).
    void rebuildSkillList();
    void rebuildSkillDropdown(const std::vector<std::string>& items);
    void selectSkill(int idx);
    void play();
    void resetCharacters();
    // Hot-swap the single monster object (goblin_) to the model + AnimBlender of the
    // selected monster kind, so its attack skills play on the right rig. Per-kind
    // cases are disabled behind [Name] guards (enable with the monster's scaffolding).
    void setMonsterCaster(CharacterKind kind);
    void positionDummyInFront();   // place the target object in front of the caster, on the ground
    void killFrontParticle();      // debug: destroy the first live particle of any active VFX (tests killParticle)
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
    const AssetManager*   assetManager_ = nullptr;   // monster caster model/anim source
    AnimSystem*           animSystem_   = nullptr;    // (un)track blenders on hot-swap
    HWND                  hwnd_        = nullptr;
    std::function<float(float, float)> terrainHeightAt_;
    std::function<void()> flushGpu_;   // drains GPU before destroying UI widgets (see InitRefs::flushGpu)

    // --- selection state ---
    CharacterKind            casterKind_ = CharacterKind::Player;
    int                      characterIdx_ = 0;   // index into kCharacterSkillMap
    PlayerWeaponType         weapon_     = PlayerWeaponType::Katana;
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
    // Top row, left to right: character | weapon | skill, each with a caption above.
    UI::Dropdown* characterDropdown_ = nullptr;
    UI::Dropdown* weaponDropdown_    = nullptr;  // hidden for monster casters
    UI::Dropdown* skillDropdown_     = nullptr;
    // Only the weapon caption needs a handle (it is shown/hidden with its dropdown);
    // the other two captions are static and stay owned by the UI tree alone.
    UI::Label*    captionWeapon_     = nullptr;
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

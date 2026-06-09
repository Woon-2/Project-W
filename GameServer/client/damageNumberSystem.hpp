#ifndef __damageNumberSystem_HPP
#define __damageNumberSystem_HPP

#include "gfxUtil.hpp"   // mu math types, Texture
#include <vector>
#include <cstdint>

class GFX;
class Camera;

// World-anchored, screen-uniform floating damage numbers (Lost Ark style):
// numbers spawn at the hit point, float up in screen space, pop on appear and
// fade out. Rendered via DigitAtlas over the existing UIPipeline (always on top,
// never occluded by world geometry). Single-threaded: spawned and updated on the
// game thread inside the event-dispatch loop, so no locking is required.

enum class DamageKind {
    EnemyHit,      // damage we deal to monsters (blue)
    PlayerHit,     // damage a player takes (red)
    StrongholdHit, // damage we deal to strongholds (gold)
};

// All presentation values live here as a single tunable bundle. These are the
// v1 spec; final values are recorded in docs/combatFeedbackUI.md after playtest.
struct DamageNumberTuning {
    float lifetime         = 0.75f;   // total seconds on screen
    float fadeOutDuration  = 0.20f;   // trailing fade (seconds)
    float floatUpPx        = 44.0f;   // total upward travel in screen pixels

    float scaleEnd         = 0.90f;   // scale while fading out

    // Scale punch: sharp entrance spike from a hardcoded keyframe table (see .cpp,
    // kPunchKeys). impactScale is the punch STRENGTH applied only to the punch's
    // deviation from 1.0 (1.0 = keyframes as authored); separate from base/perspective size.
    float impactScale      = 1.00f;

    float glyphHeightSmall = 32.0f;   // normal hit glyph height (px)
    float glyphHeightBig   = 44.0f;   // big hit glyph height (px)
    int   bigHitThreshold  = 40;      // dmg >= this -> big glyph (simple constant, no stats system)

    // Perspective: glyph scales with view depth (clip.w). 1.0 at perspRefDepth.
    float perspRefDepth    = 4.0f;
    float perspScaleMin    = 0.30f;
    float perspScaleMax    = 1.70f;
    // Final glyph-height clamp so perspScale * punch cannot blow up.
    float glyphHeightMin   = 10.0f;
    float glyphHeightMax   = 72.0f;

    float mergeWindow      = 0.25f;   // same-target hits within this window accumulate
    float jitterXMax       = 45.0f;   // horizontal spread to avoid overlap (px, scaled by perspScale)
    float startYJitterMax  = 8.0f;    // vertical spread at spawn (px)

    float worldHeadOffsetY = 1.2f;    // world-units above the target anchor (torso, below head)
};

class DamageNumberSystem {
public:
    void init(const Texture* digitAtlas, const DamageNumberTuning& tuning = {});

    // worldPos is the hit anchor (already includes any head offset). amount > 0.
    void spawn(mu::Vec3 worldPos, int amount, DamageKind kind, std::uint16_t targetId);

    void update(float dtSec);
    void render(GFX& gfx, const Camera& cam, float screenW, float screenH, float uiScale = 1.f);
    void clear();

    const DamageNumberTuning& tuning() const { return tuning_; }

private:
    static constexpr int kMaxActive = 256;

    struct DamageNumber {
        mu::Vec3   worldAnchor{};
        int        value        = 0;
        DamageKind kind         = DamageKind::EnemyHit;
        float      age          = 0.f;   // total time alive (drives float-up + fade)
        float      popAge       = 0.f;   // time since last (re)pop; also the merge clock
        float      jitterX      = 0.f;
        float      startYJitter = 0.f;
        std::uint16_t targetId  = 0;
    };

    int oldestIndex() const;

    std::vector<DamageNumber> pool_{};
    int                       activeCount_ = 0;
    const Texture*            atlas_       = nullptr;
    DamageNumberTuning        tuning_{};
};

#endif  // __damageNumberSystem_HPP

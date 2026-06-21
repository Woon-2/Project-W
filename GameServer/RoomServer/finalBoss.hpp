#ifndef final_boss_hpp
#define final_boss_hpp

#include "Npc.hpp"
#include "BehaviorTree.hpp"
#include <memory>

// Final boss: a standalone field NPC (1:1 combat, NOT a tactical/platoon unit).
// Unlike normal monsters (which use the Npc FSM) the final boss is driven by a
// dedicated BehaviorTree. Its attack leaves reuse the very same skill path as the
// other monsters (Room::skillStartInternal -> authoritative hitbox damage +
// S_SkillStart broadcast); the difference is BT situational selection instead of
// the FSM's uniform-random pickAttack. It spawns only inside the boss arena zone,
// so it has no detection range: every living player is always a candidate target.
class FinalBoss : public Npc {
public:
    FinalBoss() = default;
    FinalBoss(Object&& base) : Npc(std::move(base)) {}

    void applyBossConfig();

    // Drives the BehaviorTree instead of the Npc FSM (Npc::update is virtual).
    NpcUpdateResult update(Seconds dt, Room& room) override;

    // Builds btRoot_. Call from Room::setupFinalBoss AFTER addAttack() so the
    // attack leaves can resolve their skill ids/clip keys by index.
    void buildBehaviorTree();

    void setDamageScale(float s) { damageScale_ = s; }

    // -- BehaviorTree leaf helpers (called from the leaf action nodes) -----------
    // Resolve the current target session (alive-checked); nullptr if none.
    GameSession* resolveTarget(Room& room) const;
    // XZ distance to the current target; very large if no target.
    float distanceToTarget(Room& room) const;
    // Score every living player (no detection range) and (re)select the best.
    void evaluateTarget(Seconds dt, Room& room);
    // Steer toward dest on the XZ plane (preserves vertical velocity) + face it.
    void MU_CALLCONV moveToward(mu::Vec3 dest, float speedMult);
    // Rotate (yaw only) to look at a world position.
    void MU_CALLCONV faceToward(mu::Vec3 worldPos);
    // Cast the registered attack at index (faces target, switches clip, fires the
    // authoritative skill via Room::skillStartInternal with damageScale_).
    void castAttack(Room& room, int attackIdx);

    float bossAttackRange() const { return attackRange(); }  // Npc protected accessor
    float gapRange()        const { return gapRange_; }

private:
    // Distance-based score: nearer + lower-HP players rank higher. Threat weighting
    // for multi-player can be folded into this later.
    float scoreTarget(GameSession* s, Room& room) const;

    std::unique_ptr<bt::BtNode> btRoot_;

    int32   targetId_{ -1 };               // session/player id, -1 = none
    Seconds targetEvalTimer_{ 0s };
    float   gapRange_{ 9.f };               // upper bound for the gap-closer (Smite) band
    float   damageScale_{ 1.0f };           // caster damage multiplier for skill hits
    bool    raged_{ false };                // reserved for a future rage/phase branch

    static constexpr Seconds TARGET_EVAL_INTERVAL{ 0.5f };
};

#endif // final_boss_hpp

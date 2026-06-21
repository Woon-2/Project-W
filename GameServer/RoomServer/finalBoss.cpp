#include "rspch.hpp"
#include "finalBoss.hpp"
#include "Room.hpp"
#include "GameSession.hpp"
#include <limits>
#include <random>
#include <cmath>

// --- BtContext ----------------------------------------------------------------
// The BehaviorTree blackboard for the final boss. The framework forward-declares
// this and only passes it through by reference; the boss owns the actual shape.
// No separate blackboard class -- FinalBoss members hold the AI state.
namespace bt {
struct BtContext {
    FinalBoss& boss;
    Room&      room;
};
} // namespace bt

// --- applyBossConfig ----------------------------------------------------------

void FinalBoss::applyBossConfig() {
    NpcConfig cfg;
    cfg.maxHp          = 2000.f;
    cfg.moveSpeed      = 3.5f;
    cfg.attackRange    = 3.0f;
    cfg.detectionRange = 30.f;   // unused by the BT (boss arena has no detection gate)
    cfg.attackDamage   = 40.f;   // legacy fallback only; skill hitboxes are authoritative
    cfg.respawnDelay   = 60s;
    applyConfig(cfg);
    setHp(static_cast<int32>(cfg.maxHp));
}

// --- BehaviorTree leaf action nodes -------------------------------------------
// File-local: only referenced from buildBehaviorTree(). All damage flows through
// the authoritative skill system (Room::skillStartInternal), never direct setHp.

namespace {

// Holds the boss in place (Running) while an authoritative skill instance is live,
// so the Combat selector cannot preempt a committed attack with a new cast. The
// skill's own lua timeline drives windup/hit/recover; this guard just waits it out.
class BossSkillBusyGuard : public bt::BtNode {
public:
    bt::BtStatus tick(Seconds /*dt*/, bt::BtContext& ctx) override {
        if (ctx.room.npcSkillActive(static_cast<int32>(ctx.boss.getId()))) {
            ctx.boss.setLinearVel(mu::Vec3(0.f, ctx.boss.body().linearVel().y(), 0.f));
            return bt::BtStatus::Running;
        }
        return bt::BtStatus::Failure;
    }
};

// Fires one registered attack (by index) via the skill path, then returns Success
// so the wrapping BtCooldown starts counting from the cast moment. The busy-guard
// above holds the tree Running for the rest of the cast.
class BossSkillAttackAction : public bt::BtNode {
public:
    explicit BossSkillAttackAction(int idx) : idx_(idx) {}
    bt::BtStatus tick(Seconds /*dt*/, bt::BtContext& ctx) override {
        if (!ctx.boss.resolveTarget(ctx.room))
            return bt::BtStatus::Failure;
        ctx.boss.castAttack(ctx.room, idx_);
        return bt::BtStatus::Success;
    }
private:
    int idx_;
};

// Fallback: close the gap to the target. Running as long as a target exists.
class BossChaseAction : public bt::BtNode {
public:
    bt::BtStatus tick(Seconds /*dt*/, bt::BtContext& ctx) override {
        GameSession* t = ctx.boss.resolveTarget(ctx.room);
        if (!t)
            return bt::BtStatus::Failure;

        const float dist     = ctx.boss.distanceToTarget(ctx.room);
        const bool  approach = dist > ctx.boss.bossAttackRange() * 0.9f;
        if (approach) {
            ctx.boss.moveToward(t->player()->estimatedPos(ctx.room.getElapsedMs()), 1.f);
        } else {
            ctx.boss.faceToward(t->player()->pos());
            ctx.boss.setLinearVel(mu::Vec3(0.f, ctx.boss.body().linearVel().y(), 0.f));
        }
        ctx.boss.animController().switchClip(approach ? "Run" : "Idle");
        return bt::BtStatus::Running;
    }
};

// Fallback: no living player. Rare in the boss arena. Always Running.
class BossIdleAction : public bt::BtNode {
public:
    bt::BtStatus tick(Seconds /*dt*/, bt::BtContext& ctx) override {
        ctx.boss.setLinearVel(mu::Vec3(0.f, ctx.boss.body().linearVel().y(), 0.f));
        ctx.boss.animController().switchClip("Idle");
        return bt::BtStatus::Running;
    }
};

} // namespace

// --- buildBehaviorTree --------------------------------------------------------
// Root (Selector)
// +- [reserved] phase/rage priority branch (added later)
// +- EngageSeq: [Cond: has target] -> Combat (Selector)
//     +- BossSkillBusyGuard                       (hold during a live cast)
//     +- Cooldown: [Cond: gap band]  -> Smite     (3, gap closer)
//     +- Cooldown: [Cond: in melee]  -> Combo     (1, heavy)
//     +- Cooldown: [Cond: in melee]  -> BackAttack(2, variety)
//     +- Cooldown: [Cond: in melee]  -> Swings    (0, light filler)
//     +- BossChaseAction                          (fallback)
// Root last child: BossIdleAction                 (no target)
//
// Attack indices match setupFinalBoss's addAttack order (Swings/Combo/BackAttack/
// Smite). Distance bands and cooldowns are gameplay tunables.

void FinalBoss::buildBehaviorTree() {
    using namespace bt;

    auto inMelee = [](BtContext& ctx) {
        return ctx.boss.distanceToTarget(ctx.room) <= ctx.boss.bossAttackRange();
    };
    auto addMeleeAttack = [&inMelee](BtSelector& combat, int attackIdx, float cooldownS) {
        auto seq = std::make_unique<BtSequence>();
        seq->addChild(std::make_unique<BtCondition>(inMelee));
        seq->addChild(std::make_unique<BossSkillAttackAction>(attackIdx));
        combat.addChild(std::make_unique<BtCooldown>(Seconds{ cooldownS }, std::move(seq)));
    };

    auto combat = std::make_unique<BtSelector>();
    combat->addChild(std::make_unique<BossSkillBusyGuard>());

    // Smite (3): gap-closer when the target is just past melee but within gapRange.
    {
        auto seq = std::make_unique<BtSequence>();
        seq->addChild(std::make_unique<BtCondition>([](BtContext& ctx) {
            const float d = ctx.boss.distanceToTarget(ctx.room);
            return d > ctx.boss.bossAttackRange() && d <= ctx.boss.gapRange();
        }));
        seq->addChild(std::make_unique<BossSkillAttackAction>(3));
        combat->addChild(std::make_unique<BtCooldown>(Seconds{ 6.f }, std::move(seq)));
    }

    addMeleeAttack(*combat, 1, 5.0f);   // Combo  -- heavy
    addMeleeAttack(*combat, 2, 7.0f);   // BackAttack -- variety
    addMeleeAttack(*combat, 0, 2.5f);   // Swings -- light filler

    combat->addChild(std::make_unique<BossChaseAction>());

    auto engage = std::make_unique<BtSequence>();
    engage->addChild(std::make_unique<BtCondition>([](BtContext& ctx) {
        return ctx.boss.resolveTarget(ctx.room) != nullptr;
    }));
    engage->addChild(std::move(combat));

    auto root = std::make_unique<BtSelector>();
    // [reserved] phase/rage priority branch slots in here (Root top) later.
    root->addChild(std::move(engage));
    root->addChild(std::make_unique<BossIdleAction>());
    btRoot_ = std::move(root);
}

// --- update -------------------------------------------------------------------

NpcUpdateResult FinalBoss::update(Seconds dt, Room& room) {
    updateAnimBones(dt);   // keep hitbox bone transforms in sync with the anim clip

    if (hp() <= 0) {       // one-shot boss: stop on death (client handles corpse/ragdoll)
        animController().switchClip("Die");
        setLinearVel(mu::Vec3(0.f, body().linearVel().y(), 0.f));
        return {};
    }

    evaluateTarget(dt, room);
    if (btRoot_) {
        bt::BtContext ctx{ *this, room };
        btRoot_->tick(dt, ctx);
    }
    // Hits arrive via the skill path (S_SkillHit), so result.hit stays empty -- the
    // inline boss tick in Room::updateMonsterAI then skips the legacy S_NpcAttack/
    // S_Hit broadcast (same as every other skill-based NPC).
    return {};
}

// --- BehaviorTree leaf helpers ------------------------------------------------

GameSession* FinalBoss::resolveTarget(Room& room) const {
    if (targetId_ < 0) return nullptr;
    return room.findLivingSessionByPlayerId(targetId_);
}

float FinalBoss::distanceToTarget(Room& room) const {
    GameSession* t = resolveTarget(room);
    if (!t) return std::numeric_limits<float>::max();
    mu::Vec3 d = t->player()->pos() - pos();
    return mu::Vec3(d.x(), 0.f, d.z()).len();
}

void FinalBoss::evaluateTarget(Seconds dt, Room& room) {
    targetEvalTimer_ -= dt;
    const bool valid = (resolveTarget(room) != nullptr);
    if (valid && targetEvalTimer_ > 0s)
        return;
    targetEvalTimer_ = TARGET_EVAL_INTERVAL;

    GameSession* best     = nullptr;
    float        bestScore = -std::numeric_limits<float>::max();
    for (GameSession* s : room.getLivingPlayers()) {
        const float sc = scoreTarget(s, room);
        if (sc > bestScore) { bestScore = sc; best = s; }
    }
    targetId_ = best ? static_cast<int32>(best->id()) : -1;
}

float FinalBoss::scoreTarget(GameSession* s, Room& /*room*/) const {
    // Nearest-first. Threat / low-HP weighting can be folded in here later.
    mu::Vec3 d = s->player()->pos() - pos();
    const float dist = mu::Vec3(d.x(), 0.f, d.z()).len();
    return 100.f / (dist + 1.f);
}

void MU_CALLCONV FinalBoss::moveToward(mu::Vec3 dest, float speedMult) {
    mu::Vec3 to = dest - pos();
    mu::Vec3 toXZ(to.x(), 0.f, to.z());
    if (toXZ.len2() < 1e-6f) return;
    mu::NVec3 nd(toXZ);
    const float spd = moveSpeed() * speedMult;
    setLinearVel(mu::Vec3(nd.x() * spd, body().linearVel().y(), nd.z() * spd));
    setOrient(mu::NQuat(mu::Radian(), mu::Radian(), mu::Radian(std::atan2(nd.x(), nd.z()))));
}

void MU_CALLCONV FinalBoss::faceToward(mu::Vec3 worldPos) {
    mu::Vec3 to = worldPos - pos();
    mu::Vec3 toXZ(to.x(), 0.f, to.z());
    if (toXZ.len2() < 1e-6f) return;
    mu::NVec3 nd(toXZ);
    setOrient(mu::NQuat(mu::Radian(), mu::Radian(), mu::Radian(std::atan2(nd.x(), nd.z()))));
}

void FinalBoss::castAttack(Room& room, int attackIdx) {
    const std::vector<NpcAttack>& atk = attacks();
    if (attackIdx < 0 || attackIdx >= static_cast<int>(atk.size())) return;

    // Hold position; the skill timeline drives the swing.
    setLinearVel(mu::Vec3(0.f, body().linearVel().y(), 0.f));
    if (GameSession* t = resolveTarget(room))
        faceToward(t->player()->estimatedPos(room.getElapsedMs()));
    if (!atk[attackIdx].clipKey.empty())
        animController().switchClip(atk[attackIdx].clipKey);

    room.skillStartInternal(static_cast<int32>(getId()), atk[attackIdx].skillId,
                            std::random_device{}(), damageScale_);
}

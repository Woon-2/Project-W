#include "rspch.hpp"
#include "finalBoss.hpp"
#include "Room.hpp"
#include "GameSession.hpp"
#include <algorithm>
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

// Jittered timers for the circling/charge cadence. Same file-local thread_local engine
// pattern the rest of the server AI uses (Npc.cpp).
static thread_local std::mt19937 s_bossRng{ std::random_device{}() };

static Seconds randomRange(Seconds lo, Seconds hi) {
    std::uniform_real_distribution<float> dist(lo.count(), hi.count());
    return Seconds{ dist(s_bossRng) };
}

// --- applyBossConfig ----------------------------------------------------------

void FinalBoss::applyBossConfig() {
    NpcConfig cfg;
    cfg.maxHp          = 2000.f;
    // Walk speed, used while circling the target. The charge leg runs at
    // moveSpeed * RUN_SPEED_MULT (= 8.75) -- see FinalBoss::engage.
    cfg.moveSpeed      = 3.5f;
    // The client blends Boss_Walk_* against Boss_Run, so its playback rate runs off a
    // weight-blended reference speed (AnimBlenderBoss::update) rather than a single clip's.
    // The server has no blend weight, so it approximates with max(bandEnd, speed)/refSpeed.
    // These two values are picked so the approximation is *exact* at both gaits -- 3.5 (walk,
    // pure Boss_Walk_* -> 0.729x) and 8.75 (run, pure Boss_Run -> 0.911x) -- which is where
    // the boss actually spends its time.
    cfg.animRefSpeed   = 9.6f;
    cfg.animBandEnd    = 7.0f;
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

// Holds the boss in place (Running) while an authoritative skill instance is live, so the
// Combat selector cannot preempt a committed attack with a new cast. The skill's own lua
// timeline drives windup/hit/recover; this guard just waits it out.
//
// The boss stands still for the whole swing on purpose. A walking swing was tried and
// reverted: the attack clips have a large, weighty windup that reads wrong while the feet
// are still travelling. The answer to a target that backs off is to close the distance
// *before* committing -- see attackCommitRange() and the charge in FinalBoss::engage.
class BossSkillBusyGuard : public bt::BtNode {
public:
    bt::BtStatus tick(Seconds /*dt*/, bt::BtContext& ctx) override {
        if (ctx.room.npcSkillActive(static_cast<int32>(ctx.boss.getId()))) {
            ctx.boss.halt();
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

// Fallback: approach the target. Circles it at walk speed, then commits to a straight run
// (FinalBoss::engage owns the whole state machine). Running as long as a target exists.
class BossEngageAction : public bt::BtNode {
public:
    bt::BtStatus tick(Seconds dt, bt::BtContext& ctx) override {
        GameSession* t = ctx.boss.resolveTarget(ctx.room);
        if (!t)
            return bt::BtStatus::Failure;
        ctx.boss.engage(dt, ctx.room, *t);
        return bt::BtStatus::Running;
    }
};

// Fallback: no living player. Rare in the boss arena. Always Running.
class BossIdleAction : public bt::BtNode {
public:
    bt::BtStatus tick(Seconds /*dt*/, bt::BtContext& ctx) override {
        ctx.boss.halt();
        ctx.boss.animController().switchClip("Idle");
        return bt::BtStatus::Running;
    }
};

} // namespace

// --- buildBehaviorTree --------------------------------------------------------
// Root (Selector)
// +- [reserved] phase/rage priority branch (added later)
// +- EngageSeq: [Cond: has target] -> Combat (Selector)
//     +- BossSkillBusyGuard                          (hold during a live cast)
//     +- Cooldown: [Cond: committed && just charged] -> Smite     (3, run-in payoff)
//     +- Cooldown: [Cond: committed]                 -> Combo     (1, heavy)
//     +- Cooldown: [Cond: committed]                 -> BackAttack(2, variety)
//     +- Cooldown: [Cond: committed]                 -> Swings    (0, light filler)
//     +- BossEngageAction                            (fallback: circle, then charge)
// Root last child: BossIdleAction                    (no target)
//
// Attack indices match setupFinalBoss's addAttack order (Swings/Combo/BackAttack/
// Smite). Distance bands and cooldowns are gameplay tunables.
//
// Every attack now gates on attackCommitRange() (well inside attackRange) rather than the
// range edge. All four hitboxes are the same weapon_r OBB, so their real reach is identical
// -- Smite is no longer a "gap closer" (that would need a moving cast, which was reverted),
// it is the heavy the boss answers a completed charge with.

void FinalBoss::buildBehaviorTree() {
    using namespace bt;

    auto committed = [](BtContext& ctx) {
        return ctx.boss.distanceToTarget(ctx.room) <= ctx.boss.attackCommitRange();
    };
    auto addMeleeAttack = [&committed](BtSelector& combat, int attackIdx, float cooldownS) {
        auto seq = std::make_unique<BtSequence>();
        seq->addChild(std::make_unique<BtCondition>(committed));
        seq->addChild(std::make_unique<BossSkillAttackAction>(attackIdx));
        combat.addChild(std::make_unique<BtCooldown>(Seconds{ cooldownS }, std::move(seq)));
    };

    auto combat = std::make_unique<BtSelector>();
    combat->addChild(std::make_unique<BossSkillBusyGuard>());

    // Smite (3): the payoff for a charge that just landed on the target.
    {
        auto seq = std::make_unique<BtSequence>();
        seq->addChild(std::make_unique<BtCondition>([](BtContext& ctx) {
            return ctx.boss.justCharged()
                && ctx.boss.distanceToTarget(ctx.room) <= ctx.boss.attackCommitRange();
        }));
        seq->addChild(std::make_unique<BossSkillAttackAction>(3));
        combat->addChild(std::make_unique<BtCooldown>(Seconds{ 6.f }, std::move(seq)));
    }

    addMeleeAttack(*combat, 1, 5.0f);   // Combo  -- heavy
    addMeleeAttack(*combat, 2, 7.0f);   // BackAttack -- variety
    addMeleeAttack(*combat, 0, 2.5f);   // Swings -- light filler

    combat->addChild(std::make_unique<BossEngageAction>());

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
        halt();
        return {};
    }

    // Approach-state bookkeeping lives here, not in engage(), because the Combat selector
    // evaluates the attack leaves BEFORE the engage leaf: the tick a charge arrives is
    // usually the tick an attack preempts engage entirely. Leaving the transition inside
    // engage would strand charging_ at true forever and never open the post-charge window.
    if (postChargeTimer_ > 0s) postChargeTimer_ -= dt;

    evaluateTarget(dt, room);

    if (charging_ && distanceToTarget(room) <= attackCommitRange()) {
        charging_        = false;
        chargeTimer_     = randomRange(CHARGE_INTERVAL_MIN, CHARGE_INTERVAL_MAX);
        postChargeTimer_ = POST_CHARGE_WINDOW;   // full window when the leaves read it below
    }
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

    // 위협 목록을 평가당 1회만 수집해 후보 루프에 공유한다(보스는 chargeable이라
    // 피격 시 noteDamager로 damager가 채워진다 -- Room::noteAndMaybeReward).
    std::vector<int32> threats;
    collectRecentDamagers(room.getElapsedMs(), TARGET_THREAT_WINDOW, threats);

    GameSession* best     = nullptr;
    float        bestScore = -std::numeric_limits<float>::max();
    for (GameSession* s : room.getLivingPlayers()) {
        float sc = scoreTarget(s, threats);
        // 히스테리시스: 현재 타깃에 가산점을 줘서 비슷한 점수에서 매 평가마다 튀는 것을 막는다.
        if (static_cast<int32>(s->id()) == targetId_)
            sc += TARGET_STICKY_BONUS;
        if (sc > bestScore) { bestScore = sc; best = s; }
    }
    targetId_ = best ? static_cast<int32>(best->id()) : -1;
}

float FinalBoss::scoreTarget(GameSession* s, const std::vector<int32>& recentDamagers) const {
    const Player* p = s->player();

    // 1) 근접: 가까울수록 1에 근접 (0,1]
    mu::Vec3 d = p->pos() - pos();
    const float dist = mu::Vec3(d.x(), 0.f, d.z()).len();
    const float prox = 1.f / (dist + 1.f);

    // 2) 저HP: 약할수록 1에 근접 [0,1] (마무리 유도)
    const float hpFrac = std::clamp(
        static_cast<float>(p->hp()) / static_cast<float>(kPlayerMaxHp), 0.f, 1.f);
    const float lowHp = 1.f - hpFrac;

    // 3) 위협: 최근 보스를 때린 플레이어면 1
    bool isThreat = false;
    for (int32 id : recentDamagers)
        if (id == static_cast<int32>(s->id())) { isThreat = true; break; }
    const float threat = isThreat ? 1.f : 0.f;

    return TARGET_W_PROXIMITY * prox
         + TARGET_W_LOWHP     * lowHp
         + TARGET_W_THREAT    * threat;
}

void MU_CALLCONV FinalBoss::moveToward(mu::Vec3 dest, float speedMult, Seconds dt, bool reorient) {
    mu::Vec3 to = dest - pos();
    mu::Vec3 toXZ(to.x(), 0.f, to.z());
    if (toXZ.len2() < 1e-6f) return;
    mu::NVec3 nd(toXZ);

    // Ramp the speed instead of snapping to it. Clients read the gait off the broadcast
    // velocity, so a hard 3.5 -> 8.75 step jumps clean across the walk<->run blend band
    // and the crossfade never actually plays -- that is the "hitch" at every gait change.
    const float target = moveSpeed() * speedMult;
    const float rate   = (target > curSpeed_) ? MOVE_ACCEL : MOVE_DECEL;
    const float step   = rate * dt.count();
    curSpeed_ += std::clamp(target - curSpeed_, -step, step);

    setLinearVel(mu::Vec3(nd.x() * curSpeed_, body().linearVel().y(), nd.z() * curSpeed_));
    if (reorient)
        setOrient(mu::NQuat(mu::Radian(), mu::Radian(), mu::Radian(std::atan2(nd.x(), nd.z()))));
}

void FinalBoss::halt() {
    setLinearVel(mu::Vec3(0.f, body().linearVel().y(), 0.f));
    curSpeed_ = 0.f;
}

void MU_CALLCONV FinalBoss::faceToward(mu::Vec3 worldPos) {
    mu::Vec3 to = worldPos - pos();
    mu::Vec3 toXZ(to.x(), 0.f, to.z());
    if (toXZ.len2() < 1e-6f) return;
    mu::NVec3 nd(toXZ);
    setOrient(mu::NQuat(mu::Radian(), mu::Radian(), mu::Radian(std::atan2(nd.x(), nd.z()))));
}

// One tick of the approach. Three states; the charge->press transition is owned by
// update() (see the comment there), everything else is decided here.
//
//   charging -- a straight run at the target. Entered when the circling timer runs out (the
//               sudden commitment) or when the target has simply got too far away.
//   pressing -- the window right after a charge lands. Stays inside attackCommitRange so the
//               melee cooldowns get to fire more than once, instead of immediately walking
//               back out to the circling radius (which would cap the boss at one swing per
//               charge and read as timid).
//   circling -- locked on to the target (faceToward every tick) while side-stepping around it
//               at walk speed, holding STRAFE_RADIUS. Direction flips on a random timer. This
//               is what makes the boss read as "sizing you up" rather than a homing missile,
//               and it is why the client needs the 4-directional walk blend.
//
// The boss never attacks while moving (see BossSkillBusyGuard); closing to
// attackCommitRange first is what stops the target dodging by taking one step back.
void FinalBoss::engage(Seconds dt, Room& room, GameSession& target) {
    const float    dist   = distanceToTarget(room);
    const mu::Vec3 tgtPos = target.player()->pos();

    if (charging_) {
        // Lead the target: at 8.75 m/s the gap closes fast enough that aiming at where the
        // player *is* would consistently undershoot a running one.
        moveToward(target.player()->estimatedPos(room.getElapsedMs()), RUN_SPEED_MULT, dt);
        animController().switchClip("Run");
        return;
    }

    // --- pressing -------------------------------------------------------------------
    if (postChargeTimer_ > 0s) {
        faceToward(tgtPos);
        if (dist > attackCommitRange() * PRESS_HOLD_FRACTION) {
            moveToward(tgtPos, 1.f, dt);
            animController().switchClip("Walk");
        } else {
            halt();
            animController().switchClip("Idle");
        }
        return;
    }

    // --- circling -------------------------------------------------------------------
    chargeTimer_ -= dt;
    if (dist > CHARGE_FORCE_DISTANCE || chargeTimer_ <= 0s) {
        charging_ = true;
        return;                         // the run starts on the next tick, from curSpeed_
    }

    strafeFlipTimer_ -= dt;
    if (strafeFlipTimer_ <= 0s) {
        strafeDirTarget_ = -strafeDirTarget_;
        strafeFlipTimer_ = randomRange(STRAFE_FLIP_MIN, STRAFE_FLIP_MAX);
    }
    // Slew through zero rather than flipping the sign: an instant reversal would swap the
    // client's Boss_Walk_Left/Right in a single frame. Passing through zero makes the boss
    // plant and turn, and the near-zero tangent naturally stalls it for a beat.
    {
        const float turnStep = STRAFE_TURN_RATE * dt.count();
        strafeDir_ += std::clamp(strafeDirTarget_ - strafeDir_, -turnStep, turnStep);
    }

    const mu::Vec3 away = mu::Vec3(pos().x() - tgtPos.x(), 0.f, pos().z() - tgtPos.z());
    const float    r    = away.len();
    if (r < 1e-3f) {                    // degenerate: standing on top of the target
        halt();
        animController().switchClip("Walk");
        return;
    }

    const mu::Vec3 radial(away.x() / r, 0.f, away.z() / r);                       // target -> boss
    const mu::Vec3 tangent(-radial.z() * strafeDir_, 0.f, radial.x() * strafeDir_);
    // Positive error = too close, push outward; negative = too far, spiral inward.
    const float    radialErr = std::clamp((STRAFE_RADIUS - r) / STRAFE_RADIUS, -1.f, 1.f);
    const mu::Vec3 dir       = tangent + radial * (radialErr * STRAFE_RADIAL_GAIN);
    // Degenerate mid-reversal (tangent ~ 0 while sitting on the preferred radius): plant.
    if (dir.len2() < 1e-2f) {
        halt();
    } else {
        // reorient = false: the feet go sideways, the body keeps staring at the target.
        // The client's blend space turns that velocity into the matching Boss_Walk_Left/Right.
        moveToward(pos() + dir * 2.f, 1.f, dt, /*reorient=*/false);
    }
    faceToward(tgtPos);
    animController().switchClip("Walk");
}

void FinalBoss::castAttack(Room& room, int attackIdx) {
    const std::vector<NpcAttack>& atk = attacks();
    if (attackIdx < 0 || attackIdx >= static_cast<int>(atk.size())) return;

    // Stand still for the swing; BossSkillBusyGuard keeps holding it for the whole cast.
    halt();
    if (GameSession* t = resolveTarget(room))
        faceToward(t->player()->estimatedPos(room.getElapsedMs()));
    if (!atk[attackIdx].clipKey.empty())
        animController().switchClip(atk[attackIdx].clipKey);

    room.skillStartInternal(static_cast<int32>(getId()), atk[attackIdx].skillId,
                            std::random_device{}(), damageScale_);
}

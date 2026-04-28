#include "rspch.hpp"
#include "npc.hpp"
#include "Room.hpp"
#include "GameSession.hpp"
#include "NpcGroup.hpp"
#include <cmath>
#include <algorithm>

// ─── 생성자 ───────────────────────────────────────────────────────────────────

Npc::Npc(Object&& base, const NpcConfig& cfg)
    : Object(std::move(base))
{
    applyConfig(cfg);
    nearbyCache_.reserve(16);
}

void Npc::applyConfig(const NpcConfig& cfg) {
    detectionRange_     = cfg.detectionRange;
    attackRange_        = cfg.attackRange;
    moveSpeed_          = cfg.moveSpeed;
    attackDamage_       = cfg.attackDamage;
    attackWindupTime_   = cfg.attackWindupTime;
    attackRecoverTime_  = cfg.attackRecoverTime;
    separationRadius_   = cfg.separationRadius;
    separationWeight_   = cfg.separationWeight;
    canReAggroOnReturn_ = cfg.canReAggroOnReturn;
    overlapThreshold_   = cfg.overlapThreshold;
    returnSpeedMult_    = cfg.returnSpeedMult;
    activityZoneRadius_ = cfg.activityZoneRadius;
}

void MU_CALLCONV Npc::setSpawnPos(mu::Vec3 p) {
    spawnPos_           = p;
    activityZoneCenter_ = p;
}

void MU_CALLCONV Npc::setActivityZone(mu::Vec3 center, float radius) {
    activityZoneCenter_ = center;
    activityZoneRadius_ = radius;
}

// ─── transitionTo ─────────────────────────────────────────────────────────────

void Npc::transitionTo(NpcState next) {
    if (state_ == next) return;
    if (next == NpcState::AttackWindup)  windupTimer_     = 0s;
    if (next == NpcState::AttackRecover) recoverTimer_    = 0s;
    if (next == NpcState::Reposition)    repositionTimer_ = 0s;
    state_ = next;
}

// ─── 메인 업데이트 분기 ──────────────────────────────────────────────────────

NpcUpdateResult Npc::update(Seconds dt, Room& room) {
    if (hp() <= 0) {
        return updateDead();
    }

    // Chase 중이고 그룹 메모리 위치가 활동 구역 밖이면 귀환
    if (groupId_ >= 0 && state_ == NpcState::Chase && targetId_ != -1) {
        NpcGroup* group = room.getNpcGroup(groupId_);
        if (group) {
            const SharedTargetMemory* mem = group->getBestMemory(room.getElapsedMs());
            if (mem && !group->isInsideActivityArea(mem->lastKnownPosition)) {
                targetId_ = -1;
                transitionTo(NpcState::Return);
                return {};
            }
        }
    }

    switch (state_) {
        case NpcState::Idle:           return updateIdle          (dt, room);
        case NpcState::Chase:          return updateChase         (dt, room);
        case NpcState::AttackWindup:   return updateAttackWindup  (dt, room);
        case NpcState::AttackRecover:  return updateAttackRecover (dt, room);
        case NpcState::Return:         return updateReturn        (dt, room);
        case NpcState::Reposition:     return updateReposition    (dt, room);
        case NpcState::Dead:           return {};
        case NpcState::Investigate:    return updateInvestigate   (dt, room);
    }
    return {};
}

// ─── Idle ─────────────────────────────────────────────────────────────────────

NpcUpdateResult Npc::updateIdle(Seconds dt, Room& room) {
    GameSession* best = selectBestTarget(room);
    if (best) {
        targetId_ = best->id();
        if (groupId_ >= 0) {
            NpcGroup* group = room.getNpcGroup(groupId_);
            if (group)
                group->reportSight(getId(), targetId_,
                                   best->player()->pos(), room.getElapsedMs());
        }
        transitionTo(NpcState::Chase);
        return {};
    }

    if (groupId_ >= 0) {
        NpcGroup* group = room.getNpcGroup(groupId_);
        if (group) {
            if (group->getBestMemoryInsideActivityArea(room.getElapsedMs())) {
                transitionTo(NpcState::Investigate);
                return {};
            }
            if (group->hasValidMemory(room.getElapsedMs()))
                return {};
        }
    }
    return {};
}

// ─── Chase ────────────────────────────────────────────────────────────────────

NpcUpdateResult Npc::updateChase(Seconds dt, Room& room) {
    targetEvalTimer_ -= dt;
    if (targetEvalTimer_ <= 0s) {
        targetEvalTimer_ = TARGET_EVAL_INTERVAL;
        GameSession* newBest = selectBestTarget(room);
        if (newBest && newBest->id() != targetId_)
            targetId_ = newBest->id();
    }

    GameSession* targetSession = room.findLivingSessionByPlayerId(targetId_);
    if (!targetSession) {
        targetId_ = -1;
        if (!isOutsideActivityZone() && groupId_ >= 0) {
            NpcGroup* group = room.getNpcGroup(groupId_);
            if (group && group->getBestMemoryInsideActivityArea(room.getElapsedMs())) {
                transitionTo(NpcState::Investigate);
                return {};
            }
        }
        transitionTo(NpcState::Return);
        return {};
    }
    if (isOutsideActivityZone()) {
        targetId_ = -1;
        transitionTo(NpcState::Return);
        return {};
    }

    if (groupId_ >= 0) {
        NpcGroup* group = room.getNpcGroup(groupId_);
        if (group)
            group->reportSight(getId(), targetId_,
                               targetSession->player()->pos(), room.getElapsedMs());
    }

    mu::Vec3 toTarget = targetSession->player()->pos() - pos();
    if (toTarget.len2() <= attackRange_ * attackRange_) {
        transitionTo(NpcState::AttackWindup);
        return {};
    }

    nearbyCache_.clear();
    room.findNearbyNpcPositions(pos(), separationRadius_, getId(), nearbyCache_);
    mu::Vec3 sepForce = calcSeparationForce(nearbyCache_);
    mu::NVec3 nd(toTarget + sepForce * separationWeight_);

    setLinearVel(mu::Vec3(nd.x() * moveSpeed_, body().linearVel().y(), nd.z() * moveSpeed_));
    setOrient(mu::NQuat(mu::Radian(), mu::Radian(), mu::Radian(std::atan2(nd.x(), nd.z()))));
    return {};
}

// ─── AttackWindup ─────────────────────────────────────────────────────────────

NpcUpdateResult Npc::updateAttackWindup(Seconds dt, Room& room) {
    GameSession* targetSession = room.findLivingSessionByPlayerId(targetId_);
    if (!targetSession) {
        targetId_ = -1;
        transitionTo(NpcState::Return);
        return {};
    }
    if (isOutsideActivityZone()) {
        targetId_ = -1;
        transitionTo(NpcState::Return);
        return {};
    }

    nearbyCache_.clear();
    room.findNearbyNpcPositions(pos(), separationRadius_, getId(), nearbyCache_);
    mu::Vec3 sep = calcSeparationForce(nearbyCache_);
    if (sep.len() > 0.1f) {
        mu::NVec3 newFacing(forward() + sep * 0.3f);
        setOrient(mu::NQuat(mu::Radian(), mu::Radian(),
                            mu::Radian(std::atan2(newFacing.x(), newFacing.z()))));
    }

    windupTimer_ += dt;
    if (windupTimer_ >= attackWindupTime_) {
        mu::Vec3 toTarget = targetSession->player()->pos() - pos();
        if (toTarget.len() <= attackRange_) {
            int32 newHp = std::max(targetSession->player()->hp() - static_cast<int32>(attackDamage_), 0);
            targetSession->player()->setHp(newHp);

            NpcUpdateResult result;
            result.hit = { static_cast<uint16>(targetSession->id()), newHp };

            if (newHp <= 0) {
                targetId_ = -1;
                transitionTo(NpcState::Return);
            } else {
                transitionTo(NpcState::AttackRecover);
            }
            return result;
        }
        transitionTo(NpcState::AttackRecover);
    }
    return {};
}

// ─── AttackRecover ────────────────────────────────────────────────────────────

NpcUpdateResult Npc::updateAttackRecover(Seconds dt, Room& room) {
    GameSession* targetSession = room.findLivingSessionByPlayerId(targetId_);
    if (!targetSession) {
        targetId_ = -1;
        transitionTo(NpcState::Return);
        return {};
    }
    if (isOutsideActivityZone()) {
        targetId_ = -1;
        transitionTo(NpcState::Return);
        return {};
    }

    nearbyCache_.clear();
    room.findNearbyNpcPositions(pos(), separationRadius_, getId(), nearbyCache_);
    mu::Vec3 sep = calcSeparationForce(nearbyCache_);
    if (sep.len() > 0.1f) {
        float driftSpd = sep.len() * separationWeight_ * 0.3f * moveSpeed_;
        mu::NVec3 nd(sep);
        setLinearVel(mu::Vec3(nd.x() * driftSpd, body().linearVel().y(), nd.z() * driftSpd));
    }

    recoverTimer_ += dt;
    if (recoverTimer_ >= attackRecoverTime_) {
        if (isOvercrowded(nearbyCache_)) {
            mu::Vec3 toTarget = targetSession->player()->pos() - pos();
            mu::NVec3 tt(toTarget);
            repositionDir_ = (getId() % 2 == 0)
                ? mu::Vec3{  tt.z(), 0.f, -tt.x() }
                : mu::Vec3{ -tt.z(), 0.f,  tt.x() };
            transitionTo(NpcState::Reposition);
            return {};
        }
        mu::Vec3 toTarget = targetSession->player()->pos() - pos();
        if (toTarget.len2() <= attackRange_ * attackRange_)
            transitionTo(NpcState::AttackWindup);
        else
            transitionTo(NpcState::Chase);
    }
    return {};
}

// ─── Return ───────────────────────────────────────────────────────────────────

NpcUpdateResult Npc::updateReturn(Seconds dt, Room& room) {
    if (canReAggroOnReturn_ && !isOutsideActivityZone()) {
        GameSession* candidate = selectBestTarget(room);
        if (candidate) {
            if (groupId_ >= 0) {
                NpcGroup* group = room.getNpcGroup(groupId_);
                if (group)
                    group->reportSight(getId(), static_cast<uint32>(candidate->id()),
                                       candidate->player()->pos(), room.getElapsedMs());
            }
            targetId_ = candidate->id();
            transitionTo(NpcState::Chase);
            return {};
        }
    }

    mu::Vec3 toSpawn = spawnPos_ - pos();
    if (toSpawn.len2() < 0.3f * 0.3f) {
        setPos(spawnPos_);
        body().snapToCurrent();
        setLinearVel(mu::Vec3{});
        transitionTo(NpcState::Idle);
        return {};
    }

    nearbyCache_.clear();
    room.findNearbyNpcPositions(pos(), separationRadius_, getId(), nearbyCache_);
    mu::Vec3 sep = calcSeparationForce(nearbyCache_);
    mu::NVec3 nd(toSpawn + sep * (separationWeight_ * 0.25f));

    float spd = moveSpeed_ * returnSpeedMult_;
    setLinearVel(mu::Vec3(nd.x() * spd, body().linearVel().y(), nd.z() * spd));
    setOrient(mu::NQuat(mu::Radian(), mu::Radian(), mu::Radian(std::atan2(nd.x(), nd.z()))));
    return {};
}

// ─── Reposition ───────────────────────────────────────────────────────────────

NpcUpdateResult Npc::updateReposition(Seconds dt, Room& room) {
    GameSession* targetSession = room.findLivingSessionByPlayerId(targetId_);
    if (!targetSession) {
        targetId_ = -1;
        transitionTo(NpcState::Return);
        return {};
    }
    if (isOutsideActivityZone()) {
        targetId_ = -1;
        transitionTo(NpcState::Return);
        return {};
    }

    repositionTimer_ += dt;
    if (repositionTimer_ >= REPOSITION_TIMEOUT) {
        transitionTo(NpcState::Chase);
        return {};
    }

    nearbyCache_.clear();
    room.findNearbyNpcPositions(pos(), separationRadius_, getId(), nearbyCache_);

    if (!isOvercrowded(nearbyCache_)) {
        mu::Vec3 toTarget = targetSession->player()->pos() - pos();
        if (toTarget.len2() <= attackRange_ * attackRange_)
            transitionTo(NpcState::AttackWindup);
        else
            transitionTo(NpcState::Chase);
        return {};
    }

    mu::Vec3 toTarget = targetSession->player()->pos() - pos();
    mu::Vec3 sep      = calcSeparationForce(nearbyCache_);
    mu::NVec3 nd(toTarget + repositionDir_ * 0.8f + sep * separationWeight_);

    setLinearVel(mu::Vec3(nd.x() * moveSpeed_, body().linearVel().y(), nd.z() * moveSpeed_));
    setOrient(mu::NQuat(mu::Radian(), mu::Radian(), mu::Radian(std::atan2(nd.x(), nd.z()))));
    return {};
}

// ─── Investigate ─────────────────────────────────────────────────────────────

NpcUpdateResult Npc::updateInvestigate(Seconds dt, Room& room) {
    GameSession* best = selectBestTarget(room);
    if (best) {
        targetId_ = best->id();
        if (groupId_ >= 0) {
            NpcGroup* group = room.getNpcGroup(groupId_);
            if (group)
                group->reportSight(getId(), targetId_,
                                   best->player()->pos(), room.getElapsedMs());
        }
        transitionTo(NpcState::Chase);
        return {};
    }

    if (isOutsideActivityZone()) {
        transitionTo(NpcState::Return);
        return {};
    }

    NpcGroup* group = room.getNpcGroup(groupId_);
    const SharedTargetMemory* mem = group
        ? group->getBestMemoryInsideActivityArea(room.getElapsedMs())
        : nullptr;
    if (!mem) {
        transitionTo(NpcState::Return);
        return {};
    }

    mu::Vec3 diff = mem->lastKnownPosition - pos();
    float dist = diff.len();
    if (dist > 0.5f) {
        mu::NVec3 nd(diff);
        setLinearVel(mu::Vec3(nd.x() * moveSpeed_, body().linearVel().y(), nd.z() * moveSpeed_));
        setOrient(mu::NQuat(mu::Radian(), mu::Radian(), mu::Radian(std::atan2(nd.x(), nd.z()))));
    } else {
        transitionTo(NpcState::Return);
    }
    return {};
}

// ─── Dead ─────────────────────────────────────────────────────────────────────

NpcUpdateResult Npc::updateDead() {
    if (state_ != NpcState::Dead)
        transitionTo(NpcState::Dead);
    return {};
}

// ─── evaluateTargetScore ──────────────────────────────────────────────────────

float Npc::evaluateTargetScore(GameSession* s, Room& room) const {
    float dist  = (s->player()->pos() - pos()).len();
    float score = std::max(0.f, (1.f - dist / (activityZoneRadius_ * 2.f))) * 50.f;

    if (s->id() == targetId_) score += 20.f;
    if (dist <= attackRange_)                       score += 15.f;

    int aggro = room.countNpcsTargeting(s->id());
    if (static_cast<uint32>(s->id()) == targetId_ && aggro > 0) --aggro;
    score -= static_cast<float>(aggro) * 8.f;

    return score;
}

// ─── selectBestTarget ────────────────────────────────────────────────────────

GameSession* Npc::selectBestTarget(Room& room) const {
    NpcGroup* group    = (groupId_ >= 0) ? room.getNpcGroup(groupId_) : nullptr;
    GameSession* best  = nullptr;
    float  bestScore   = -999.f;

    for (GameSession* s : room.getLivingPlayers()) {
        mu::Vec3 toPlayer = s->player()->pos() - pos();
        if (toPlayer.len2() > detectionRange_ * detectionRange_) continue;
        if (group && !group->isInsideActivityArea(s->player()->pos())) continue;
        float sc = evaluateTargetScore(s, room);
        if (sc > bestScore) { bestScore = sc; best = s; }
    }
    return best;
}

// ─── calcSeparationForce ──────────────────────────────────────────────────────

mu::Vec3 MU_CALLCONV Npc::calcSeparationForce(const std::vector<mu::Vec3>& nearby) const {
    mu::Vec3 force{ 0.f, 0.f, 0.f };
    for (const mu::Vec3& op : nearby) {
        mu::Vec3 away = pos() - op;
        float d = away.len();
        if (d < 1e-4f) {
            float a = static_cast<float>(getId()) * 1.2f;
            force += mu::Vec3{ std::cosf(a), 0.f, std::sinf(a) };
            continue;
        }
        float strength = 1.f - (d / separationRadius_);
        force += (away / d) * strength;
    }
    return force;
}

// ─── isOutsideActivityZone ───────────────────────────────────────────────────

bool Npc::isOutsideActivityZone() const {
    return (pos() - activityZoneCenter_).len2() > activityZoneRadius_ * activityZoneRadius_;
}

// ─── isOvercrowded ────────────────────────────────────────────────────────────

bool Npc::isOvercrowded(const std::vector<mu::Vec3>& nearby) const {
    float checkRadius = separationRadius_ * 0.7f;
    int   count       = 0;
    for (const mu::Vec3& p : nearby)
        if ((pos() - p).len2() < checkRadius * checkRadius)
            ++count;
    return count >= overlapThreshold_;
}

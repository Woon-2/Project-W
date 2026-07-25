#include "rspch.hpp"
#include "npc.hpp"
#include "Model.hpp"
#include "Room.hpp"
#include "GameSession.hpp"
#include "NpcGroup.hpp"
#include <cmath>
#include <algorithm>

static thread_local std::mt19937 s_rng{ std::random_device{}() };

static Seconds randomRange(Seconds lo, Seconds hi) {
    std::uniform_real_distribution<float> dist(lo.count(), hi.count());
    return Seconds{ dist(s_rng) };
}

// ─── 생성자 ───────────────────────────────────────────────────────────────────

Npc::Npc(Object&& base, const NpcConfig& cfg)
    : Object(std::move(base))
{
    applyConfig(cfg);
    nearbyCache_.reserve(16);
    setCanReceiveDamage(true);
}

void Npc::applyConfig(const NpcConfig& cfg) {
    maxHp_              = cfg.maxHp;
    respawnDelay_       = cfg.respawnDelay;
    detectionRange_     = cfg.detectionRange;
    attackRange_        = cfg.attackRange;
    moveSpeed_          = cfg.moveSpeed;
    setAnimRefSpeed(cfg.animRefSpeed);
    setAnimBandEnd(cfg.animBandEnd);
    attackDamage_       = cfg.attackDamage;
    attackWindupTime_   = cfg.attackWindupTime;
    attackRecoverTime_  = cfg.attackRecoverTime;
    separationRadius_   = cfg.separationRadius;
    separationWeight_   = cfg.separationWeight;
    canReAggroOnReturn_ = cfg.canReAggroOnReturn;
    overlapThreshold_   = cfg.overlapThreshold;
    returnSpeedMult_    = cfg.returnSpeedMult;
    maxDirectReactDelay_ = cfg.maxDirectReactDelay;
    maxGroupReactDelay_  = cfg.maxGroupReactDelay;
    patrolRadius_        = cfg.patrolRadius;
    patrolSpeedMult_     = cfg.patrolSpeedMult;
    minIdleTime_         = cfg.minIdleTime;
    maxIdleTime_         = cfg.maxIdleTime;
    minPatrolTime_       = cfg.minPatrolTime;
    maxPatrolTime_       = cfg.maxPatrolTime;

    // 초기 상태는 transitionTo를 거치지 않는 Idle이므로 첫 휴식 길이를 직접 초기화
    idleTimer_ = randomRange(minIdleTime_, maxIdleTime_);

    body().setMotorMaxAcceleration(cfg.motorMaxAcceleration);
    body().setMotorMaxDeceleration(cfg.motorMaxDeceleration);
    body().setMotorGain(cfg.motorGain);
}

void Npc::addAttack(uint32 skillId, std::string clipKey) {
    if (skillId == 0) return;   // skill not found in registry -> skip (keeps fallback intact)
    attacks_.push_back({ skillId, std::move(clipKey) });
}

const NpcAttack& Npc::pickAttack() const {
    // Uniform random selection over the registered attacks (multi-attack monsters).
    std::uniform_int_distribution<size_t> dist(0, attacks_.size() - 1);
    return attacks_[dist(s_rng)];
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

    if (state_ == NpcState::Idle || state_ == NpcState::Patrol) {
        directReactTimer_ = -1s;
        groupReactTimer_  = -1s;
    }
    
    if (next == NpcState::AttackWindup)  { windupTimer_ = 0s; attackCast_ = false; }
    if (next == NpcState::AttackRecover) recoverTimer_    = 0s;
    if (next == NpcState::Reposition)    repositionTimer_ = 0s;

    if (next == NpcState::Idle){
        idleTimer_ = randomRange(minIdleTime_, maxIdleTime_);
    }

    if (next == NpcState::Patrol) {
        patrolDest_     = pickPatrolDest();
        patrolDuration_ = randomRange(minPatrolTime_, maxPatrolTime_);
        patrolTimer_    = 0s;
    }

    if (next == NpcState::AttackWindup || next == NpcState::Idle || next == NpcState::Dead){
        setDesiredVel(mu::Vec3{});
    }
    
    state_ = next;

    switch (next) {
        case NpcState::Idle:
        case NpcState::Return:
        case NpcState::Investigate:
            animController().switchClip("Idle");    break;
        case NpcState::Chase:
        case NpcState::Reposition:
            animController().switchClip("Walk");    break;
        case NpcState::AttackWindup:
            animController().switchClip("Attack");  break;
        case NpcState::AttackRecover:
            // Skill-based NPCs keep the cast attack clip playing through recover so the
            // server hitbox bones stay in sync with the skill timeline; only legacy
            // (no skill-attack) NPCs fall back to the generic "Attack" clip here.
            if (!hasSkillAttacks()) animController().switchClip("Attack");
            break;
        case NpcState::Dead:
            animController().switchClip("Die");     break;
    }
}

// ─── 메인 업데이트 분기 ──────────────────────────────────────────────────────

NpcUpdateResult Npc::update(Seconds dt, Room& room) {
    updateAnimBones(dt);

    if (hp() <= 0) {
        return updateDead(dt);
    }

    switch (state_) {
        case NpcState::Idle:           return updateIdle          (dt, room);
        case NpcState::Patrol:         return updatePatrol        (dt, room);
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

// ─── checkAlert ───────────────────────────────────────────────────────────────
// 직접 감지/그룹 메모리를 판정. 경계 상태(Chase/Investigate로 전환했거나 반응 타이머
// 대기 중)면 true를 반환해 호출측이 배회하지 않고 즉시 멈추도록 한다.

bool Npc::checkAlert(Seconds dt, Room& room) {
    // --- 직접 감지 ---
    GameSession* best = selectBestVisibleTarget(room);

    if (best) {
        if (directReactTimer_ < 0s) {
            std::uniform_real_distribution<float> dist(0.f, maxDirectReactDelay_.count());
            directReactTimer_ = Seconds{dist(s_rng)};
        }

        directReactTimer_ -= dt;
        if (directReactTimer_ <= 0s) {
            best = selectBestVisibleTarget(room);

            if (best) {
                targetId_ = best->id();
                if (groupId_ >= 0) {
                    NpcGroup* group = room.getNpcGroup(groupId_);
                    if (group)
                        group->reportSight(getId(), targetId_, best->player()->pos(), room.getElapsedMs());
                }
                transitionTo(NpcState::Chase);
            }
            directReactTimer_ = -1s;
        }
        return true;
    }
    directReactTimer_ = -1s;

    // --- 그룹 메모리 ---
    if (groupId_ >= 0) {
        NpcGroup* group = room.getNpcGroup(groupId_);

        if (group) {
            if (group->getBestMemoryInsideActivityArea(room.getElapsedMs())) {
                if (groupReactTimer_ < 0s) {
                    std::uniform_real_distribution<float> dist(0.f, maxGroupReactDelay_.count());
                    groupReactTimer_ = Seconds{dist(s_rng)};
                }

                groupReactTimer_ -= dt;
                if (groupReactTimer_ <= 0s) {
                    transitionTo(NpcState::Investigate);
                }
                return true;
            }
            groupReactTimer_ = -1s;

            if (group->hasValidMemory(room.getElapsedMs()))
                return true;  // 활동 구역 밖 메모리만 있음 — 자연 만료 대기
        }
    }
    return false;
}

// ─── Idle ─────────────────────────────────────────────────────────────────────

NpcUpdateResult Npc::updateIdle(Seconds dt, Room& room) {
    if (checkAlert(dt, room)) return {};

    // 잠시 쉬었다가 순찰 재개
    idleTimer_ -= dt;
    if (idleTimer_ <= 0s)
        transitionTo(NpcState::Patrol);
    return {};
}

// ─── Patrol ───────────────────────────────────────────────────────────────────

NpcUpdateResult Npc::updatePatrol(Seconds dt, Room& room) {
    if (checkAlert(dt, room)) return {};

    patrolTimer_ += dt;

    mu::Vec3 toDest   = patrolDest_ - pos();
    mu::Vec3 toDestXZ( toDest.x(), 0.f, toDest.z() );

    // 웨이포인트 도착 또는 순찰 시간 만료 → 잠시 Idle로 휴식
    if ( toDestXZ.len2() < 0.5f * 0.5f || patrolTimer_ >= patrolDuration_ ) {
        setLinearVel( mu::Vec3( 0.f, body().linearVel().y(), 0.f ) );
        transitionTo( NpcState::Idle );
        return {};
    }

    // 천천히 이동 (Chase 이동 패턴 동일, 속도만 patrolSpeedMult_)
    nearbyCache_.clear();
    room.findNearbyNpcPositions( pos(), separationRadius_, getId(), nearbyCache_ );
    mu::NVec3 dirN( toDestXZ );
    mu::Vec3  dir = { dirN.x(), 0.f, dirN.z() };
    mu::Vec3  sep = calcSeparationForce( nearbyCache_, separationRadius_ );
    mu::Vec3  sepPerp = sep - dir * mu::dot( sep, dir );
    mu::NVec3 nd( dir + sepPerp * separationWeight_ );

    float spd = moveSpeed_ * patrolSpeedMult_;
    setLinearVel( mu::Vec3( nd.x() * spd, body().linearVel().y(), nd.z() * spd ) );
    setOrient( mu::NQuat( mu::Radian(), mu::Radian(), mu::Radian( std::atan2( nd.x(), nd.z() ) ) ) );
    return {};
}

// ─── pickPatrolDest ───────────────────────────────────────────────────────────

mu::Vec3 Npc::pickPatrolDest() const {
    // 스폰 근처 랜덤 각도 + [patrolRadius*0.3, patrolRadius] 반경.
    // patrolRadius(~5) ≪ activityZoneRadius(28), 스폰=활동구역 중심이라 항상 구역 내.
    std::uniform_real_distribution<float> ang( 0.f, 6.2831853f );
    std::uniform_real_distribution<float> rad( patrolRadius_ * 0.3f, patrolRadius_ );
    float a = ang( s_rng );
    float r = rad( s_rng );
    return mu::Vec3( spawnPos_.x() + std::cos(a) * r, spawnPos_.y(), spawnPos_.z() + std::sin(a) * r );
}

// ─── Chase ────────────────────────────────────────────────────────────────────

NpcUpdateResult Npc::updateChase(Seconds dt, Room& room) {
    targetEvalTimer_ -= dt;
    if ( targetEvalTimer_ <= 0s ) {
        targetEvalTimer_ = TARGET_EVAL_INTERVAL;

        GameSession* newBest = selectBestVisibleTarget( room );

        if ( newBest && newBest->id() != targetId_ ) {
            targetId_ = newBest->id();
        }
    }

    GameSession* targetSession = room.findLivingSessionByPlayerId( targetId_ );
    if ( !targetSession ) {
        targetId_ = -1;

        if ( !isOutsideActivityZone() && groupId_ >= 0 ) {
            NpcGroup* group = room.getNpcGroup(groupId_);

		    // 그룹 메모리 위치가 활동 구역 안에 있으면 Investigate로 전환
            if ( group && group->getBestMemoryInsideActivityArea( room.getElapsedMs() ) ) {
                transitionTo( NpcState::Investigate );
                return {};
            }
        }

		// 타깃을 잃었으므로 Return으로 전환
        transitionTo( NpcState::Return );
        return {};
    }

	// 활동 구역 밖이면 Return으로 전환
    if ( isOutsideActivityZone() ) {
        targetId_ = -1;
        transitionTo( NpcState::Return );
        return {};
    }

    // 추격 중 그룹에 시야(위치) 보고 — 플레이어가 활동 구역 밖이면 귀환
    if ( groupId_ >= 0 ) {
        NpcGroup* group = room.getNpcGroup( groupId_ );

        if ( group ) {
            if ( group->isInsideActivityArea( targetSession->player()->pos() ) ) {
                group->reportSight( getId(), targetId_, targetSession->player()->pos(), room.getElapsedMs() );
            }
            else {
                targetId_ = -1;
                transitionTo( NpcState::Return );
                return {};
            }
        }
    }

	// 타깃과의 거리가 공격 범위 안이면 AttackWindup으로 전환
    mu::Vec3 toTarget = targetSession->player()->estimatedPos(room.getElapsedMs()) - pos();
    if ( toTarget.len2() <= attackRange_ * attackRange_ ) {
        transitionTo( NpcState::AttackWindup );
        return {};
    }

    nearbyCache_.clear();
    room.findNearbyNpcPositions( pos(), separationRadius_, getId(), nearbyCache_ );
    mu::NVec3 chaseDirN( toTarget );
    mu::Vec3  chaseDir = { chaseDirN.x(), 0.f, chaseDirN.z() };
    mu::Vec3  sep      = calcSeparationForce( nearbyCache_, separationRadius_ );
    mu::Vec3  sepPerp  = sep - chaseDir * mu::dot( sep, chaseDir );
    mu::NVec3 nd( chaseDir + sepPerp * separationWeight_ );

    setDesiredVel( mu::Vec3( nd.x() * moveSpeed_, 0.f, nd.z() * moveSpeed_ ) );
    setOrient( mu::NQuat( mu::Radian(), mu::Radian(), mu::Radian( std::atan2( nd.x(), nd.z() ) ) ) );
    return {};
}

// ─── AttackWindup ─────────────────────────────────────────────────────────────

NpcUpdateResult Npc::updateAttackWindup( Seconds dt, Room& room ) {
    GameSession* targetSession = room.findLivingSessionByPlayerId( targetId_ );

	// 타깃을 잃었으므로 Return으로 전환
    if ( !targetSession ) {
        targetId_ = -1;
        transitionTo( NpcState::Return );
        return {};
    }
	// 활동 구역 밖이면 Return으로 전환
    if ( isOutsideActivityZone() ) {
        targetId_ = -1;
        transitionTo( NpcState::Return );
        return {};
    }

    mu::Vec3 toTarget = targetSession->player()->estimatedPos(room.getElapsedMs()) - pos();

    // Skill-based attack (preferred): cast once at the start of the windup. The
    // skill's own timeline (PlayAnimation + SpawnHitbox) supplies the telegraph and
    // hit; its hitbox applies damage authoritatively (no direct setHp). Multi-attack
    // monsters pick a random attack here. AttackRecover keeps the NPC in place until
    // the skill finishes (see updateAttackRecover) so the bones track the hitboxes.
    if ( hasSkillAttacks() ) {
        // Re-chase only before committing the cast; once cast, the swing follows through.
        if ( !attackCast_ && toTarget.len2() > attackRange_ * attackRange_ ) {
            mu::NVec3 nd( toTarget );
            setLinearVel( mu::Vec3( nd.x() * moveSpeed_, body().linearVel().y(), nd.z() * moveSpeed_ ) );
            setOrient( mu::NQuat( mu::Radian(), mu::Radian(), mu::Radian( std::atan2( nd.x(), nd.z() ) ) ) );
            transitionTo( NpcState::Chase );
            return {};
        }
        if ( !attackCast_ ) {
            // Face the target, select an attack, drive the server anim, cast.
            mu::NVec3 nd( toTarget );
            setOrient( mu::NQuat( mu::Radian(), mu::Radian(), mu::Radian( std::atan2( nd.x(), nd.z() ) ) ) );
            const NpcAttack& atk = pickAttack();
            if ( !atk.clipKey.empty() ) animController().switchClip( atk.clipKey );
            const uint32 seed = std::random_device{}();
            room.skillStartInternal( static_cast<int32>( getId() ), atk.skillId, seed );
            attackCast_ = true;
        }
        windupTimer_ += dt;
        if ( windupTimer_ >= attackWindupTime_ )
            transitionTo( NpcState::AttackRecover );
        return {};
    }

    // ===== Legacy fallback: direct-damage melee (NPCs with no registered skill) =====
    if ( toTarget.len2() > attackRange_ * attackRange_ ) {
        mu::NVec3 nd( toTarget );
        setLinearVel( mu::Vec3( nd.x() * moveSpeed_, body().linearVel().y(), nd.z() * moveSpeed_ ) );
        setOrient( mu::NQuat( mu::Radian(), mu::Radian(), mu::Radian( std::atan2( nd.x(), nd.z() ) ) ) );
        transitionTo( NpcState::Chase );
        return {};
    }

    windupTimer_ += dt;
    if ( windupTimer_ >= attackWindupTime_ ) {
        mu::Vec3 toTarget2 = targetSession->player()->estimatedPos(room.getElapsedMs()) - pos();

        if ( toTarget2.len() <= attackRange_ ) {
            int32 newHp = std::max( targetSession->player()->hp() - static_cast<int32>(attackDamage_), 0 );
            targetSession->player()->setHp( newHp );

            NpcUpdateResult result;
            result.hit = { static_cast<uint16>(targetSession->id()), newHp };

            if ( newHp <= 0 ) {
                targetId_ = -1;
                transitionTo( NpcState::Return );
            }
            else {
                transitionTo( NpcState::AttackRecover );
            }
            return result;
        }
        else {
            transitionTo( NpcState::AttackRecover );
        }
    }
    return {};
}

// ─── AttackRecover ────────────────────────────────────────────────────────────

NpcUpdateResult Npc::updateAttackRecover( Seconds dt, Room& room ) {
    GameSession* targetSession = room.findLivingSessionByPlayerId( targetId_ );

	// 타깃을 잃었으므로 Return으로 전환
    if ( !targetSession ) {
        targetId_ = -1;
        transitionTo( NpcState::Return );
        return {};
    }
	// 활동 구역 밖이면 Return으로 전환
    if ( isOutsideActivityZone() ) {
        targetId_ = -1;
        transitionTo( NpcState::Return );
        return {};
    }

    constexpr float BODY_RADIUS = 0.8f;
    nearbyCache_.clear();
    room.findNearbyNpcPositions( pos(), BODY_RADIUS * 2.f, getId(), nearbyCache_ );
    mu::Vec3 push = calcSeparationForce( nearbyCache_, BODY_RADIUS * 2.f );

    if ( push.len() > 0.1f ) {
        mu::NVec3 nd( push );
        float driftSpd = moveSpeed_ * 0.15f;
        setDesiredVel( mu::Vec3( nd.x() * driftSpd, 0.f, nd.z() * driftSpd ) );
    }

    recoverTimer_ += dt;
    // Skill-based NPCs hold AttackRecover (and the cast attack clip) until the skill
    // instance finishes, so the server hitbox bones stay valid for late hitboxes.
    if ( recoverTimer_ >= attackRecoverTime_ &&
         !( hasSkillAttacks() && room.npcSkillActive( static_cast<int32>( getId() ) ) ) ) {
        mu::Vec3 toTargetXZ = targetSession->player()->pos() - pos();
        toTargetXZ = mu::Vec3( toTargetXZ.x(), 0.f, toTargetXZ.z() );
        if ( toTargetXZ.len() > 0.001f ) {
            mu::NVec3 nd( toTargetXZ );
            setOrient( mu::NQuat( mu::Radian(), mu::Radian(), mu::Radian( std::atan2( nd.x(), nd.z() ) ) ) );
        }

        nearbyCache_.clear();
        room.findNearbyNpcPositions( pos(), separationRadius_, getId(), nearbyCache_ );
        mu::Vec3 toTarget = targetSession->player()->estimatedPos(room.getElapsedMs()) - pos();

		// 회복 타임이 끝났는데도 여전히 과밀이면 Reposition 시도
        if ( isOvercrowded( nearbyCache_ ) ) {
            mu::NVec3 tt( toTarget );
            repositionDir_ = (getId() % 2 == 0) ? mu::Vec3{ tt.z(), 0.f, -tt.x() } : mu::Vec3{ -tt.z(), 0.f,  tt.x() };
            transitionTo( NpcState::Reposition );
            return {};
        }

		// 회복 타임이 끝났는데 타깃과의 거리가 여전히 공격 범위 안이면 AttackWindup로 전환, 아니면 Chase로 전환
        if ( toTarget.len2() <= attackRange_ * attackRange_ ) {
            transitionTo( NpcState::AttackWindup );
        }
        else {
            transitionTo( NpcState::Chase );
        }
    }
    return {};
}

// ─── Return ───────────────────────────────────────────────────────────────────

NpcUpdateResult Npc::updateReturn( Seconds dt, Room& room ) {
    if ( canReAggroOnReturn_ && !isOutsideActivityZone() ) {
        GameSession* candidate = selectBestVisibleTarget( room );

        if ( candidate ) {
			// 재어그로 시 메모리 갱신 후 Chase로 전환
            if ( groupId_ >= 0 ) {
                NpcGroup* group = room.getNpcGroup( groupId_ );
                if ( group ) {
                    group->reportSight( getId(), candidate->id(), candidate->player()->pos(), room.getElapsedMs() );
                }
            }

            targetId_ = candidate->id();
            transitionTo( NpcState::Chase );
            return {};
        }
    }

    // 스폰 근처에 도달했고 그룹 메모리가 있으면 Investigate로 전환
    // 귀환 도중 진행 방향이 바뀌는 것을 막기 위해 스폰 근처에서만 확인
    mu::Vec3 toSpawn = spawnPos_ - pos();
    // Dynamic 물리로 인해 pos().y가 spawnPos_.y와 다를 수 있으므로 XZ 2D 거리로 판정
    float xzToSpawn2 = toSpawn.x() * toSpawn.x() + toSpawn.z() * toSpawn.z();
    if ( !isOutsideActivityZone() && xzToSpawn2 < 1.0f * 1.0f && groupId_ >= 0 ) {
        NpcGroup* group = room.getNpcGroup( groupId_ );

        if ( group && group->getBestMemoryInsideActivityArea( room.getElapsedMs() ) ) {
            transitionTo( NpcState::Investigate );
            return {};
        }
    }

	// 스폰 위치에 거의 도달했으면 XZ를 스폰 위치로 보정하고 Idle로 전환. Y는 물리 엔진이 관리.
    if ( xzToSpawn2 < 0.3f * 0.3f ) {
        setPos( mu::Vec3( spawnPos_.x(), pos().y(), spawnPos_.z() ) );
        body().snapToCurrent();
        setLinearVel( mu::Vec3{} );
        transitionTo( NpcState::Idle );
        return {};
    }

    nearbyCache_.clear();
    room.findNearbyNpcPositions( pos(), separationRadius_, getId(), nearbyCache_ );
    mu::Vec3 toSpawnXZ( toSpawn.x(), 0.f, toSpawn.z() );
    mu::NVec3 homeDirN( toSpawnXZ );
    mu::Vec3  homeDir = { homeDirN.x(), 0.f, homeDirN.z() };
    mu::Vec3  sep     = calcSeparationForce( nearbyCache_, separationRadius_ );
    mu::Vec3  sepPerp = sep - homeDir * mu::dot( sep, homeDirN );
    mu::NVec3 nd( homeDir + sepPerp * separationWeight_ );

    float spd = moveSpeed_ * returnSpeedMult_;
    setDesiredVel( mu::Vec3( nd.x() * spd, 0.f, nd.z() * spd ) );
    setOrient( mu::NQuat( mu::Radian(), mu::Radian(), mu::Radian( std::atan2( nd.x(), nd.z() ) ) ) );
    return {};
}

// ─── Reposition ───────────────────────────────────────────────────────────────

NpcUpdateResult Npc::updateReposition( Seconds dt, Room& room ) {
    GameSession* targetSession = room.findLivingSessionByPlayerId( targetId_ );

	// 타깃을 잃었으므로 Return으로 전환
    if ( !targetSession ) {
        targetId_ = -1;
        transitionTo( NpcState::Return );
        return {};
    }
	// 활동 구역 밖이면 Return으로 전환
    if ( isOutsideActivityZone() ) {
        targetId_ = -1;
        transitionTo( NpcState::Return );
        return {};
    }

	// 과밀 회피 이동이 너무 오래 지속되면 강제로 Chase로 전환
    repositionTimer_ += dt;
    if ( repositionTimer_ >= REPOSITION_TIMEOUT ) {
        transitionTo( NpcState::Chase );
        return {};
    }

    nearbyCache_.clear();
    room.findNearbyNpcPositions( pos(), separationRadius_, getId(), nearbyCache_ );

	// 과밀이 해소되었으면 타깃과의 거리에 따라 AttackWindup 또는 Chase로 전환
    if ( !isOvercrowded( nearbyCache_ ) ) {
        mu::Vec3 toTarget = targetSession->player()->estimatedPos(room.getElapsedMs()) - pos();

        if ( toTarget.len2() <= attackRange_ * attackRange_ ) {
            transitionTo( NpcState::AttackWindup );
        }
        else {
            transitionTo( NpcState::Chase );
        }
        return {};
    }

    mu::Vec3 toTarget = targetSession->player()->pos() - pos();
    mu::Vec3 sep = calcSeparationForce( nearbyCache_, separationRadius_ );
    mu::NVec3 nd( toTarget + repositionDir_ * 0.8f + sep * separationWeight_ );

    setDesiredVel( mu::Vec3( nd.x() * moveSpeed_, 0.f, nd.z() * moveSpeed_ ) );
    setOrient( mu::NQuat( mu::Radian(), mu::Radian(), mu::Radian( std::atan2( nd.x(), nd.z() ) ) ) );
    return {};
}

// ─── Investigate ─────────────────────────────────────────────────────────────

NpcUpdateResult Npc::updateInvestigate( Seconds dt, Room& room ) {
	// 직접 감지 성공 시 Chase로 전환
    {
        GameSession* best = selectBestVisibleTarget( room );

        if ( best ) {
            targetId_ = best->id();

            if ( groupId_ >= 0 ) {
                NpcGroup* group = room.getNpcGroup( groupId_ );
                if ( group ) {
                    group->reportSight( getId(), targetId_, best->player()->pos(), room.getElapsedMs() );
                }
            }

            transitionTo( NpcState::Chase );
            return {};
        }
    }

	// 활동 구역 밖이면 Return으로 전환
    if ( isOutsideActivityZone() ) {
        transitionTo( NpcState::Return );
        return {};
    }

    NpcGroup* group = room.getNpcGroup( groupId_ );
    // 메모리 위치가 활동 구역 밖에면 Return을 전환
    const SharedTargetMemory* mem = group ? group->getBestMemoryInsideActivityArea( room.getElapsedMs() ) : nullptr;
    if ( !mem ) {
        transitionTo( NpcState::Return );
        return {};
    }

    mu::Vec3 diff = mem->lastKnownPosition - pos();
    if ( diff.len() > 0.5f ) {
        mu::NVec3 nd( diff );
        setDesiredVel( mu::Vec3( nd.x() * moveSpeed_, 0.f, nd.z() * moveSpeed_ ) );
        setOrient( mu::NQuat( mu::Radian(), mu::Radian(), mu::Radian( std::atan2( nd.x(), nd.z() ) ) ) );
    }
	// Investigate 했지만 플레이어가 없다면 Return으로 전환
    else {
        transitionTo( NpcState::Return );
    }
    return {};
} 

// ─── Dead ─────────────────────────────────────────────────────────────────────

NpcUpdateResult Npc::updateDead(Seconds /*dt*/) {
    if (state_ != NpcState::Dead)
        transitionTo(NpcState::Dead);

    // Revival is driven by the owning Stronghold (fixed-pool population model),
    // not by a per-NPC timer. The corpse simply stays Dead until reviveAt().
    return {};
}

void MU_CALLCONV Npc::reviveAt(mu::Vec3 pos) {
    setHp(static_cast<int32>(maxHp_));
    spawnPos_         = pos;          // new leash home
    setPos(pos);
    body().snapToCurrent();           // teleport: clear prev state, no interpolation drag
    setLinearVel({ 0.f, 0.f, 0.f });
    setDesiredVel(mu::Vec3{});
    targetId_         = -1;
    windupTimer_      = 0s;
    recoverTimer_     = 0s;
    directReactTimer_ = -1s;
    groupReactTimer_  = -1s;
    idleTimer_        = randomRange(minIdleTime_, maxIdleTime_);
    state_            = NpcState::Idle;
    // state_를 직접 대입하므로 transitionTo의 switchClip가 안 불린다. 사망 시 "Die"로 바뀐
    // 클립을 명시적으로 Idle로 되돌리지 않으면, 부활한 NPC(hp>0)가 엎드린 사망 포즈로 굳어
    // 본-부착 피격 BVH가 지면을 파고든 채 남는다(다음 진짜 Idle 전이까지). 초기화(setupGoblin)와
    // 동일하게 여기서 직접 Idle 클립으로 리셋한다.
    animController().switchClip("Idle");
}

void Npc::onHitImpulse() {
    // Velocity motor handles knockback convergence — no timer needed.
}

// ─── evaluateTargetScore ──────────────────────────────────────────────────────

float Npc::evaluateTargetScore( GameSession* s, Room& room ) const {
    float dist = (s->player()->pos() - pos()).len();
    float score = std::max( 0.f, (1.f - dist / (activityZoneRadius_ * 2.f)) ) * 50.f;

    if ( s->id() == targetId_ ) score += 20.f;
    if ( dist <= attackRange_ ) score += 15.f;

    int aggro = room.countNpcsTargeting( s->id() );
    if ( s->id() == targetId_ && aggro > 0 ) --aggro;
    score -= static_cast<float>(aggro) * 8.f;

    return score;
}

// ─── selectBestVisibleTarget ─────────────────────────────────────────────────

GameSession* Npc::selectBestVisibleTarget( Room& room ) const {
    NpcGroup* group = (groupId_ >= 0) ? room.getNpcGroup( groupId_ ) : nullptr;
    GameSession* best = nullptr;
    float bestScore = -999.f;

    for ( GameSession* s : room.getLivingPlayers() ) {
        mu::Vec3 toPlayer = s->player()->pos() - pos();
        if ( toPlayer.len2() > detectionRange_ * detectionRange_ ) continue;

        if ( group && !group->isInsideActivityArea( s->player()->pos() ) ) continue;

        float sc = evaluateTargetScore( s, room );
        if ( sc > bestScore ) { 
            bestScore = sc;
            best = s;
        }
    }
    return best;
}


// ─── isOutsideActivityZone ───────────────────────────────────────────────────

bool Npc::isOutsideActivityZone() const {
    return (pos() - activityZoneCenter_).len2() > activityZoneRadius_ * activityZoneRadius_;
}

// ─── isOvercrowded ────────────────────────────────────────────────────────────

bool Npc::isOvercrowded( const std::vector<mu::Vec3>& nearby ) const {
    float checkRadius = separationRadius_ * 0.7f;
    int   count = 0;
    for ( const mu::Vec3& p : nearby )
        if ( (pos() - p).len2() < checkRadius * checkRadius )
            ++count;
    return count >= overlapThreshold_;
}

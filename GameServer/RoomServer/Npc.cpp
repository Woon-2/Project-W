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

NpcUpdateResult Npc::updateIdle( Seconds dt, Room& room ) {
    GameSession* best = selectBestVisibleTarget( room );

    if ( best ) {
        targetId_ = best->id();

        // 타깃을 발견했으므로 그룹에 보고
        if ( groupId_ >= 0 ) {
            NpcGroup* group = room.getNpcGroup( groupId_ );
            if ( group ) {
                group->reportSight( getId(), targetId_, best->player()->pos(), room.getElapsedMs() );
            }
        }
		// 타깃을 발견했으므로 Chase로 전환
        transitionTo( NpcState::Chase );
        return {};
    }

	// 직접 감지 실패 시 공유 메모리 위치 확인 후 Investigate로 전환
    if ( groupId_ >= 0 ) {
        NpcGroup* group = room.getNpcGroup( groupId_ );

        if ( group ) {
            if ( group->getBestMemoryInsideActivityArea( room.getElapsedMs() ) ) {
                transitionTo( NpcState::Investigate );
                return {};
            }
            if ( group->hasValidMemory( room.getElapsedMs() ) ) {
                return {};  // npc 활동 구역 밖 메모리만 있음 - 자연 만료 대기
            }
        }
    }
    return {};
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
    mu::Vec3 toTarget = targetSession->player()->pos() - pos();
    if ( toTarget.len2() <= attackRange_ * attackRange_ ) {
        transitionTo( NpcState::AttackWindup );
        return {};
    }

    nearbyCache_.clear();
    room.findNearbyNpcPositions( pos(), separationRadius_, getId(), nearbyCache_ );
    mu::Vec3 sepForce = calcSeparationForce( nearbyCache_ );
    mu::NVec3 nd( toTarget + sepForce * separationWeight_ );

    setLinearVel( mu::Vec3( nd.x() * moveSpeed_, body().linearVel().y(), nd.z() * moveSpeed_ ) );
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

    nearbyCache_.clear();
    room.findNearbyNpcPositions( pos(), separationRadius_, getId(), nearbyCache_ );
    mu::Vec3 sep = calcSeparationForce( nearbyCache_ );

    if ( sep.len() > 0.1f ) {
        mu::NVec3 newFacing( forward() + sep * 0.3f );
        setOrient( mu::NQuat( mu::Radian(), mu::Radian(), mu::Radian( std::atan2( newFacing.x(), newFacing.z() ) ) ) );
    }

    windupTimer_ += dt;
    if ( windupTimer_ >= attackWindupTime_ ) {
        mu::Vec3 toTarget = targetSession->player()->pos() - pos();

        if ( toTarget.len() <= attackRange_ ) {
            int32 newHp = std::max( targetSession->player()->hp() - static_cast<int32>(attackDamage_), 0 );
            targetSession->player()->setHp( newHp );

            NpcUpdateResult result;
            result.hit = { static_cast<uint16>(targetSession->id()), newHp };

			// 타깃 체력이 0이하가 되었으므로 타깃 잃음 처리 후 Return으로 전환
            if ( newHp <= 0 ) {
                targetId_ = -1;
                transitionTo( NpcState::Return );
            }
			// 타깃이 아직 살아있으면 AttackRecover로 전환
            else {
                transitionTo( NpcState::AttackRecover );
            }
            return result;
        }
        else {
			// 공격 빗나갔으므로 AttackRecover로 전환
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

    nearbyCache_.clear();
    room.findNearbyNpcPositions( pos(), separationRadius_, getId(), nearbyCache_ );
    mu::Vec3 sep = calcSeparationForce( nearbyCache_ );

    if ( sep.len() > 0.1f ) {
        float driftSpd = sep.len() * separationWeight_ * 0.3f * moveSpeed_;
        mu::NVec3 nd( sep );
        setLinearVel( mu::Vec3( nd.x() * driftSpd, body().linearVel().y(), nd.z() * driftSpd ) );
    }

    recoverTimer_ += dt;
    if ( recoverTimer_ >= attackRecoverTime_ ) {
        mu::Vec3 toTarget = targetSession->player()->pos() - pos();

		// 회복 타임이 끝났는데도 여전히 과밀이면 Repostion 시도
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
    if ( !isOutsideActivityZone() && toSpawn.len2() < 1.0f * 1.0f && groupId_ >= 0 ) {
        NpcGroup* group = room.getNpcGroup( groupId_ );

        if ( group && group->getBestMemoryInsideActivityArea( room.getElapsedMs() ) ) {
            transitionTo( NpcState::Investigate );
            return {};
        }
    }

	// 스폰 위치로 이동. 스폰 위치에 거의 도달했으면 정확히 스폰 위치로 보정 후 Idle로 전환
    if ( toSpawn.len2() < 0.6f * 0.6f ) {
        setPos( spawnPos_ );
        body().snapToCurrent();
        setLinearVel( mu::Vec3{} );
        transitionTo( NpcState::Idle );
        return {};
    }

    nearbyCache_.clear();
    room.findNearbyNpcPositions( pos(), separationRadius_, getId(), nearbyCache_ );
    mu::Vec3 sep = calcSeparationForce( nearbyCache_ );
    mu::NVec3 nd( toSpawn + sep * (separationWeight_ * 0.25f) );

    float spd = moveSpeed_ * returnSpeedMult_;
    setLinearVel( mu::Vec3( nd.x() * spd, body().linearVel().y(), nd.z() * spd ) );
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
        mu::Vec3 toTarget = targetSession->player()->pos() - pos();

        if ( toTarget.len2() <= attackRange_ * attackRange_ ) {
            transitionTo( NpcState::AttackWindup );
        }
        else {
            transitionTo( NpcState::Chase );
        }
        return {};
    }

    mu::Vec3 toTarget = targetSession->player()->pos() - pos();
    mu::Vec3 sep = calcSeparationForce( nearbyCache_ );
    mu::NVec3 nd( toTarget + repositionDir_ * 0.8f + sep * separationWeight_ );

    setLinearVel( mu::Vec3( nd.x() * moveSpeed_, body().linearVel().y(), nd.z() * moveSpeed_ ) );
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
        setLinearVel( mu::Vec3( nd.x() * moveSpeed_, body().linearVel().y(), nd.z() * moveSpeed_ ) );
        setOrient( mu::NQuat( mu::Radian(), mu::Radian(), mu::Radian( std::atan2( nd.x(), nd.z() ) ) ) );
    }
	// Investigate 했지만 플레이어가 없다면 Return으로 전환
    else {
        transitionTo( NpcState::Return );
    }
    return {};
} 

// ─── Dead ─────────────────────────────────────────────────────────────────────

NpcUpdateResult Npc::updateDead() {
    if ( state_ != NpcState::Dead ) {
        transitionTo( NpcState::Dead );
    }
    return {};
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

// ─── calcSeparationForce ──────────────────────────────────────────────────────

mu::Vec3 MU_CALLCONV Npc::calcSeparationForce( const std::vector<mu::Vec3>& nearby ) const {
    mu::Vec3 force{ 0.f, 0.f, 0.f };
    for ( const mu::Vec3& op : nearby ) {
        mu::Vec3 away = pos() - op;
        float d = away.len();
        if ( d < 1e-4f ) {
            float a = static_cast<float>( getId() ) * 1.2f;
            force += mu::Vec3{ std::cosf( a ), 0.f, std::sinf( a ) };
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

bool Npc::isOvercrowded( const std::vector<mu::Vec3>& nearby ) const {
    float checkRadius = separationRadius_ * 0.7f;
    int   count = 0;
    for ( const mu::Vec3& p : nearby )
        if ( (pos() - p).len2() < checkRadius * checkRadius )
            ++count;
    return count >= overlapThreshold_;
}

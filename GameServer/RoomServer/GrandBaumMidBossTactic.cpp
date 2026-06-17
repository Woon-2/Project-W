#include "rspch.hpp"
#include "GrandBaumMidBossTactic.hpp"
#include "PlatoonLeader.hpp"
#include "Room.hpp"
#include "GameSession.hpp"
#include "TacticalSquad.hpp"
#include "TacticalNpc.hpp"
#include "object.hpp"
#include "PacketManager.hpp"
#include <algorithm>
#include <cmath>
#include <random>

/*--------------------------------
	  Local Utility Function
--------------------------------*/

static mu::Vec3 norm3( mu::Vec3 v ) {
    float l = v.len();
    return l > 0.0001f ? v * (1.f / l) : mu::Vec3( 0.f, 0.f, 0.f );
}

/*-------------------------------
      GrandBaumMidBossTactic
-------------------------------*/

void GrandBaumMidBossTactic::update( Seconds dt, Room& room, PlatoonLeader& leader ) {
    // 원본 뱀 부대(squad 3) 로스터를 첫 틱에 캡처(부활/위협판정 기준).
    TacticalSquad* originalSnakeSquadForRoster = leader.getSquads().size() >= 4
        ? leader.getSquads()[ 3 ]
        : nullptr;
    captureOriginalSnakeRoster( room, originalSnakeSquadForRoster );
    leader.removeDeadMembersFromSquads();

    const auto& squads = leader.getSquads();
    if ( squads.empty() ) {
        return;
    }

    // 보스는 항상 최근접 플레이어를 바라본다(전투 중이 아니어도 지휘 연출).
    uint32 facingTargetId = selectNearestPlayerId( room, leader.pos() );
    GameSession* facingTarget = facingTargetId != 0
        ? room.findLivingSessionByPlayerId( static_cast<int32>( facingTargetId ) )
        : nullptr;
    if ( facingTarget ) {
        mu::Vec3 dir = facingTarget->player()->pos() - leader.pos();
        if ( dir.len() > 0.1f ) {
            leader.setFacing( norm3( dir ) );
        }
    }

    // HP 비율이 66%/33%를 "넘어 떨어지는" 순간을 각 1회만 트리거(단조 증가 stage).
    float hpRatio = (leader.maxHp() > 0)
        ? static_cast<float>( leader.hp() ) / static_cast<float>( leader.maxHp() )
        : 1.f;

    int32 crossedStage = shieldWallTriggerStage_;
    if ( previousHpRatio_ > FIRST_SHIELD_WALL_HP_RATIO && hpRatio <= FIRST_SHIELD_WALL_HP_RATIO ) {
        crossedStage = std::max( crossedStage, 1 );
    }
    if ( previousHpRatio_ > SECOND_SHIELD_WALL_HP_RATIO && hpRatio <= SECOND_SHIELD_WALL_HP_RATIO ) {
        crossedStage = std::max( crossedStage, 2 );
    }
    previousHpRatio_ = hpRatio;

    if ( crossedStage > shieldWallTriggerStage_ ) {
        shieldWallTriggerStage_ = crossedStage;
        pendingShieldWallTrigger_ = true;
    }

    if ( phase_ == Phase::Cooldown ) {
        tacticCooldown_ -= dt;
        updateBossMelee( dt, room, leader );

        if ( shouldPreserveOriginalSnakes() ) {
            TacticalSquad* snakeSquad = squads.size() >= 4 ? squads[ 3 ] : nullptr;
            updateSnakeEvasion( dt, room, leader, snakeSquad );
        }

        engageRefreshTimer_ -= dt;
        if ( engageRefreshTimer_ <= 0s ) {
            engageRefreshTimer_ = ENGAGE_REFRESH_INTERVAL;
            issueEngage( room, leader );
        }

        if ( tacticCooldown_ > 0s ) {
            return;
        }

        enterPhase( Phase::Engage, leader );
        return;
    }

    if ( phase_ == Phase::Engage ) {
        updateBossMelee( dt, room, leader );

        if ( shouldPreserveOriginalSnakes() ) {
            TacticalSquad* snakeSquad = squads.size() >= 4 ? squads[ 3 ] : nullptr;
            updateSnakeEvasion( dt, room, leader, snakeSquad );
        }

        if ( !engageOrderIssued_ ) {
            issueEngage( room, leader );
            engageRefreshTimer_ = ENGAGE_REFRESH_INTERVAL;
            engageOrderIssued_ = true;
            return;
        }

        engageRefreshTimer_ -= dt;
        if ( engageRefreshTimer_ <= 0s ) {
            engageRefreshTimer_ = ENGAGE_REFRESH_INTERVAL;
            issueEngage( room, leader );
        }

        // ShieldWall 발동 게이트: 살아있는 원본 뱀 ≥ 1 AND 살아있는 슬라임 ≥ 10.
        // 둘 중 하나라도 불충족이면 발동을 스킵하고 바로 Cooldown(뱀이 없으면 매복 불가).
        if ( pendingShieldWallTrigger_ ) {
            pendingShieldWallTrigger_ = false;
            TacticalSquad* originalSnakeSquad = squads.size() >= 4 ? squads[ 3 ] : nullptr;
            int32 liveOriginalSnakes = countLiveMembers( room, originalSnakeSquad );
            if ( liveOriginalSnakes <= 0 ) {
                cleanupSnakeWave( room );
                tacticCooldown_ = TACTIC_COOLDOWN_DURATION;
                enterPhase( Phase::Cooldown, leader );
                return;
            }

            int32 liveSlimes = countLiveSlimeMembers( room, leader );
            if ( !canFormShieldWall( liveSlimes ) ) {
                cleanupSnakeWave( room );
                tacticCooldown_ = TACTIC_COOLDOWN_DURATION;
                enterPhase( Phase::Cooldown, leader );
                return;
            }

            originalSnakeCountAtShieldWall_ = liveOriginalSnakes;
            shieldWallRingRadius_ = calcShieldWallRadius( liveSlimes );
            cleanupSnakeWave( room );
            enterPhase( Phase::ShieldWall, leader );
            applyShieldWallProtection( room, leader, true );
            issueShieldWall( room, leader );
            return;
        }
    }

    if ( phase_ == Phase::ShieldWall ) {
        applyShieldWallProtection( room, leader, true );

        orderRefreshTimer_ -= dt;
        if ( orderRefreshTimer_ <= 0s ) {
            orderRefreshTimer_ = ORDER_REFRESH_INTERVAL;
            issueShieldWall( room, leader );

            // 벽이 형성될 때까지(슬라임 슬롯 미도착 동안) 링 안에 들어온 플레이어를 재넉백+재락한다.
            // 고정 락(2s)은 형성 시간(가변)을 못 따라가 미형성 벽으로 재진입을 허용하므로, 형성 상태에
            // 게이트한다. 형성 완료 시 슬라임 barrier가 차단을 인계 → 재넉백 중단. 안전 타임아웃으로 영구 락 방지.
            if ( shieldWallRingIssued_ && !shieldWallFormed_ ) {
                shieldWallFormTimer_ += ORDER_REFRESH_INTERVAL;
                if ( isShieldWallFormed( room, leader ) || shieldWallFormTimer_ >= SHIELD_WALL_FORM_KNOCK_MAX ) {
                    shieldWallFormed_ = true;
                }
                else {
                    room.knockPlayersOutOfShieldWall( shieldWallRingCenter_, shieldWallRingRadius_ );
                }
            }
        }

        TacticalSquad* originalSnakeSquad = squads.size() >= 4 ? squads[ 3 ] : nullptr;
        updateSnakeAmbush( dt, room, leader, originalSnakeSquad );
    }
}

void GrandBaumMidBossTactic::onLeaderDead( Room& room, PlatoonLeader& leader ) {
    applyShieldWallProtection( room, leader, false );
    cleanupSnakeWave( room );
    MidBossTacticBase::onLeaderDead( room, leader );
}

void GrandBaumMidBossTactic::enterPhase( Phase next, PlatoonLeader& leader ) {
    phase_ = next;

    if ( next == Phase::Engage ) {
        engageOrderIssued_ = false;
        engageRefreshTimer_ = 0s;
        shieldWallRingIssued_ = false;
        shieldWallFormed_ = false;
        shieldWallFormTimer_ = 0s;
        shieldWallRingRadius_ = MAX_SHIELD_RING_RADIUS;
        snakeRetreatTimer_ = 0s;
        snakeWaveSpawned_ = false;
        originalSnakeCountAtShieldWall_ = 0;
        snakeAmbushStage_ = SnakeAmbushStage::Evasion;
        snakeWanderCenterSet_ = false;
        snakePersonalTargets_.clear();
        snakePersonalTimers_.clear();
        snakePersonalEvading_.clear();
        return;
    }

    if ( next == Phase::ShieldWall ) {
        orderRefreshTimer_ = ORDER_REFRESH_INTERVAL;
        snakeRetreatTimer_ = 0s;
        snakeWaveSpawned_ = false;
        shieldWallRingIssued_ = false;
        shieldWallFormed_ = false;
        shieldWallFormTimer_ = 0s;
        resetBossMelee( leader );
        snakePersonalTargets_.clear();
        snakePersonalTimers_.clear();
        snakePersonalEvading_.clear();
        snakeAmbushStage_ = SnakeAmbushStage::RetreatingOriginal;
        return;
    }

    if ( next == Phase::Cooldown ) {
        engageRefreshTimer_ = ENGAGE_REFRESH_INTERVAL;
        shieldWallRingIssued_ = false;
        shieldWallRingRadius_ = MAX_SHIELD_RING_RADIUS;
        snakeRetreatTimer_ = 0s;
        snakeWaveSpawned_ = false;
    }
}

void GrandBaumMidBossTactic::resetBossMelee( PlatoonLeader& leader ) {
    bossMeleeState_ = BossMeleeState::AcquireTarget;
    bossMeleeTimer_ = 0s;
    bossMeleeTargetLockTimer_ = 0s;
    bossMeleeSamePriorityRetargetTimer_ = 0s;
    bossMeleeTargetId_ = 0;
    bossMeleeTargetPriority_ = BossTargetPriority::None;
    leader.setTacticalTarget( 0 );
    leader.setDesiredVel( mu::Vec3{} );
    leader.transitionTacticalState( TacticalNpcState::Idle );
}

void GrandBaumMidBossTactic::updateBossMelee( Seconds dt, Room& room, PlatoonLeader& leader ) {
    if ( bossMeleeTargetLockTimer_ > 0s ) {
        bossMeleeTargetLockTimer_ = std::max( Seconds{ 0.f }, bossMeleeTargetLockTimer_ - dt );
    }
    if ( bossMeleeSamePriorityRetargetTimer_ > 0s ) {
        bossMeleeSamePriorityRetargetTimer_ = std::max( Seconds{ 0.f }, bossMeleeSamePriorityRetargetTimer_ - dt );
    }

    auto acquireTarget = [&]() -> bool {
        BossTargetChoice choice = selectBossMeleeTarget( room, leader );
        bossMeleeTargetId_ = choice.targetId;
        bossMeleeTargetPriority_ = choice.priority;
        leader.setTacticalTarget( bossMeleeTargetId_ );
        if ( bossMeleeTargetId_ == 0 ) {
            bossMeleeTargetLockTimer_ = 0s;
            bossMeleeSamePriorityRetargetTimer_ = 0s;
            leader.setDesiredVel( mu::Vec3{} );
            leader.transitionTacticalState( TacticalNpcState::Idle );
            return false;
        }
        bossMeleeTargetLockTimer_ = BOSS_TARGET_LOCK_DURATION;
        bossMeleeSamePriorityRetargetTimer_ = BOSS_SAME_PRIORITY_RETARGET_INTERVAL;
        bossMeleeState_ = BossMeleeState::ChaseTarget;
        leader.transitionTacticalState( TacticalNpcState::Chase );
        return true;
    };

    if ( bossMeleeState_ == BossMeleeState::AcquireTarget ) {
        acquireTarget();
        return;
    }

    GameSession* target = bossMeleeTargetId_ != 0
        ? room.findLivingSessionByPlayerId( static_cast<int32>( bossMeleeTargetId_ ) )
        : nullptr;
    if ( !target ) {
        bool wasChasing = bossMeleeState_ == BossMeleeState::ChaseTarget;
        bossMeleeState_ = BossMeleeState::AcquireTarget;
        bossMeleeTargetId_ = 0;
        bossMeleeTargetPriority_ = BossTargetPriority::None;
        bossMeleeTargetLockTimer_ = 0s;
        bossMeleeSamePriorityRetargetTimer_ = 0s;
        if ( !acquireTarget() || !wasChasing ) {
            return;
        }

        target = bossMeleeTargetId_ != 0
            ? room.findLivingSessionByPlayerId( static_cast<int32>( bossMeleeTargetId_ ) )
            : nullptr;
        if ( !target ) {
            return;
        }
    }

    TacticalNpcConfig cfg = leader.getConfig();
    float attackRange = leader.getAttackRange();
    float attackRangeSq = attackRange * attackRange;

    if ( bossMeleeState_ == BossMeleeState::ChaseTarget ) {
        if ( !isCurrentBossMeleeTargetValid( room, leader ) ) {
            bossMeleeState_ = BossMeleeState::AcquireTarget;
            bossMeleeTargetId_ = 0;
            bossMeleeTargetPriority_ = BossTargetPriority::None;
            bossMeleeTargetLockTimer_ = 0s;
            bossMeleeSamePriorityRetargetTimer_ = 0s;
            if ( !acquireTarget() ) {
                return;
            }

            target = bossMeleeTargetId_ != 0
                ? room.findLivingSessionByPlayerId( static_cast<int32>( bossMeleeTargetId_ ) )
                : nullptr;
            if ( !target ) {
                return;
            }
        }

        // 락 만료 시 우선순위 재평가: 더 높은 우선순위면 즉시, 같은 우선순위면 재타겟 쿨다운 후 전환.
        if ( bossMeleeTargetLockTimer_ <= 0s ) {
            BossTargetChoice priorityTarget = selectBossMeleeTarget( room, leader );
            bool shouldSwitchTarget = false;
            if ( priorityTarget.targetId != 0 ) {
                int32 newPriority = static_cast<int32>( priorityTarget.priority );
                int32 currentPriority = static_cast<int32>( bossMeleeTargetPriority_ );
                if ( newPriority > currentPriority ) {
                    shouldSwitchTarget = true;
                }
                else if ( newPriority == currentPriority &&
                          priorityTarget.targetId != bossMeleeTargetId_ &&
                          bossMeleeSamePriorityRetargetTimer_ <= 0s ) {
                    shouldSwitchTarget = true;
                }
            }

            if ( shouldSwitchTarget ) {
                bool targetChanged = priorityTarget.targetId != bossMeleeTargetId_;
                bossMeleeTargetId_ = priorityTarget.targetId;
                bossMeleeTargetPriority_ = priorityTarget.priority;
                bossMeleeTargetLockTimer_ = BOSS_TARGET_LOCK_DURATION;
                bossMeleeSamePriorityRetargetTimer_ = BOSS_SAME_PRIORITY_RETARGET_INTERVAL;
                leader.setTacticalTarget( bossMeleeTargetId_ );
                if ( targetChanged ) {
                    target = room.findLivingSessionByPlayerId( static_cast<int32>( bossMeleeTargetId_ ) );
                    if ( !target ) {
                        bossMeleeState_ = BossMeleeState::AcquireTarget;
                        bossMeleeTargetId_ = 0;
                        bossMeleeTargetPriority_ = BossTargetPriority::None;
                        bossMeleeTargetLockTimer_ = 0s;
                        bossMeleeSamePriorityRetargetTimer_ = 0s;
                        return;
                    }
                }
            }
        }

        mu::Vec3 toTarget = target->player()->pos() - leader.pos();
        if ( toTarget.len2() <= attackRangeSq ) {
            bossMeleeState_ = BossMeleeState::AttackWindup;
            bossMeleeTimer_ = 0s;
            leader.setDesiredVel( mu::Vec3{} );
            leader.transitionTacticalState( TacticalNpcState::AttackWindup );
            return;
        }

        moveBossToward( leader, target->player()->pos(), BOSS_CHASE_SPEED_MULT );
        return;
    }

    if ( bossMeleeState_ == BossMeleeState::AttackWindup ) {
        mu::Vec3 toTarget = target->player()->pos() - leader.pos();
        if ( toTarget.len2() > 0.01f ) {
            leader.setFacing( norm3( toTarget ) );
        }
        leader.setDesiredVel( mu::Vec3{} );

        bossMeleeTimer_ += dt;
        if ( bossMeleeTimer_ < cfg.attackWindupTime ) {
            return;
        }

        if ( ( target->player()->pos() - leader.pos() ).len2() <= attackRangeSq ) {
            leader.applyHitToSession( target, leader.getAttackDamage() );
        }

        bossMeleeState_ = BossMeleeState::AttackRecover;
        bossMeleeTimer_ = 0s;
        leader.transitionTacticalState( TacticalNpcState::AttackRecover );
        return;
    }

    if ( bossMeleeState_ == BossMeleeState::AttackRecover ) {
        mu::Vec3 toTarget = target->player()->pos() - leader.pos();
        if ( toTarget.len2() > 0.01f ) {
            leader.setFacing( norm3( toTarget ) );
        }
        leader.setDesiredVel( mu::Vec3{} );

        bossMeleeTimer_ += dt;
        if ( bossMeleeTimer_ < cfg.attackRecoverTime ) {
            return;
        }

        bossMeleeState_ = BossMeleeState::AcquireTarget;
        bossMeleeTimer_ = 0s;
    }
}

// 보스 표적 우선순위: (원본 뱀 보존 중에 한해) SnakeThreat > SlimeThreat, 그 외엔 항상 Nearest.
GrandBaumMidBossTactic::BossTargetChoice
GrandBaumMidBossTactic::selectBossMeleeTarget( Room& room, const PlatoonLeader& leader ) const {
    if ( shouldPreserveOriginalSnakes() ) {
        BossTargetChoice snakeThreat = selectOriginalSnakeThreatTarget( room, leader );
        if ( snakeThreat.targetId != 0 ) {
            return snakeThreat;
        }
        BossTargetChoice slimeThreat = selectSlimeThreatTarget( room, leader );
        if ( slimeThreat.targetId != 0 ) {
            return slimeThreat;
        }
    }
    return selectNearestPlayerTarget( room, leader.pos() );
}

// 살아있는 원본 뱀 중 하나라도 SNAKE_STOP_EVADE_RANGE 내에 둔 플레이어(자원을 위협하는 자) 중 최근접.
GrandBaumMidBossTactic::BossTargetChoice
GrandBaumMidBossTactic::selectOriginalSnakeThreatTarget( Room& room, const PlatoonLeader& leader ) const {
    uint32 bestPlayerId = 0;
    float bestDistSq = -1.f;
    float rangeSq = SNAKE_STOP_EVADE_RANGE * SNAKE_STOP_EVADE_RANGE;

    for ( uint32 snakeId : getOriginalSnakeCandidateIds( leader ) ) {
        TacticalNpc* snake = room.findTacticalNpcById( snakeId );
        if ( !snake || snake->hp() <= 0 ) {
            continue;
        }

        for ( GameSession* s : room.getLivingPlayers() ) {
            if ( !s ) {
                continue;
            }
            float distSq = ( snake->pos() - s->player()->pos() ).len2();
            if ( distSq > rangeSq ) {
                continue;
            }
            if ( bestDistSq < 0.f || distSq < bestDistSq ) {
                bestDistSq = distSq;
                bestPlayerId = static_cast<uint32>( s->id() );
            }
        }
    }

    return { bestPlayerId, bestPlayerId != 0 ? BossTargetPriority::SnakeThreat : BossTargetPriority::None };
}

GrandBaumMidBossTactic::BossTargetChoice
GrandBaumMidBossTactic::selectSlimeThreatTarget( Room& room, const PlatoonLeader& leader ) const {
    int32 liveCount = 0;
    mu::Vec3 center = calcLiveSlimeCentroid( room, leader, liveCount );
    if ( liveCount <= 0 ) {
        return {};
    }
    return selectNearestPlayerNear( room, center, BOSS_SLIME_THREAT_RANGE, BossTargetPriority::SlimeThreat );
}

std::vector<uint32> GrandBaumMidBossTactic::getOriginalSnakeCandidateIds( const PlatoonLeader& leader ) const {
    if ( !originalSnakeRoster_.empty() ) {
        return originalSnakeRoster_;
    }
    if ( leader.getSquads().size() >= 4 && leader.getSquads()[ 3 ] ) {
        return leader.getSquads()[ 3 ]->getMembers();
    }
    return {};
}

GrandBaumMidBossTactic::BossTargetChoice MU_CALLCONV
GrandBaumMidBossTactic::selectNearestPlayerTarget( Room& room, mu::Vec3 center ) const {
    uint32 bestId = 0;
    float bestDistSq = -1.f;

    for ( GameSession* s : room.getLivingPlayers() ) {
        if ( !s ) {
            continue;
        }
        float distSq = ( center - s->player()->pos() ).len2();
        if ( bestDistSq < 0.f || distSq < bestDistSq ) {
            bestDistSq = distSq;
            bestId = static_cast<uint32>( s->id() );
        }
    }

    return { bestId, bestId != 0 ? BossTargetPriority::Nearest : BossTargetPriority::None };
}

GrandBaumMidBossTactic::BossTargetChoice MU_CALLCONV
GrandBaumMidBossTactic::selectNearestPlayerNear( Room& room, mu::Vec3 center, float radius, BossTargetPriority priority ) const {
    uint32 bestId = 0;
    float bestDistSq = -1.f;
    float radiusSq = radius * radius;

    for ( GameSession* s : room.getLivingPlayers() ) {
        if ( !s ) {
            continue;
        }
        float distSq = ( center - s->player()->pos() ).len2();
        if ( distSq > radiusSq ) {
            continue;
        }
        if ( bestDistSq < 0.f || distSq < bestDistSq ) {
            bestDistSq = distSq;
            bestId = static_cast<uint32>( s->id() );
        }
    }

    return { bestId, bestId != 0 ? priority : BossTargetPriority::None };
}

bool GrandBaumMidBossTactic::isCurrentBossMeleeTargetValid( Room& room, const PlatoonLeader& leader ) const {
    if ( bossMeleeTargetId_ == 0 ) {
        return false;
    }

    GameSession* target = room.findLivingSessionByPlayerId( static_cast<int32>( bossMeleeTargetId_ ) );
    if ( !target ) {
        return false;
    }

    if ( bossMeleeTargetPriority_ == BossTargetPriority::Nearest ) {
        return true;
    }
    if ( !shouldPreserveOriginalSnakes() ) {
        return false;
    }

    mu::Vec3 targetPos = target->player()->pos();

    if ( bossMeleeTargetPriority_ == BossTargetPriority::SnakeThreat ) {
        float rangeSq = SNAKE_STOP_EVADE_RANGE * SNAKE_STOP_EVADE_RANGE;
        for ( uint32 snakeId : getOriginalSnakeCandidateIds( leader ) ) {
            TacticalNpc* snake = room.findTacticalNpcById( snakeId );
            if ( !snake || snake->hp() <= 0 ) {
                continue;
            }
            if ( ( snake->pos() - targetPos ).len2() <= rangeSq ) {
                return true;
            }
        }
        return false;
    }

    if ( bossMeleeTargetPriority_ == BossTargetPriority::SlimeThreat ) {
        int32 liveCount = 0;
        mu::Vec3 center = calcLiveSlimeCentroid( room, leader, liveCount );
        return liveCount > 0 &&
            ( center - targetPos ).len2() <= BOSS_SLIME_THREAT_RANGE * BOSS_SLIME_THREAT_RANGE;
    }

    return false;
}

bool GrandBaumMidBossTactic::isResourceThreatPriority( BossTargetPriority priority ) const {
    return priority == BossTargetPriority::SnakeThreat || priority == BossTargetPriority::SlimeThreat;
}

mu::Vec3 MU_CALLCONV GrandBaumMidBossTactic::calcLiveOriginalSnakeCentroid(
    Room& room, const PlatoonLeader& leader, int32& outLiveCount ) const {
    mu::Vec3 sum{};
    outLiveCount = 0;

    if ( !originalSnakeRoster_.empty() ) {
        for ( uint32 memberId : originalSnakeRoster_ ) {
            TacticalNpc* npc = room.findTacticalNpcById( memberId );
            if ( npc && npc->hp() > 0 ) {
                sum = sum + npc->pos();
                ++outLiveCount;
            }
        }
    }
    else if ( leader.getSquads().size() >= 4 && leader.getSquads()[ 3 ] ) {
        for ( uint32 memberId : leader.getSquads()[ 3 ]->getMembers() ) {
            TacticalNpc* npc = room.findTacticalNpcById( memberId );
            if ( npc && npc->hp() > 0 ) {
                sum = sum + npc->pos();
                ++outLiveCount;
            }
        }
    }

    if ( outLiveCount > 0 ) {
        return sum * ( 1.f / static_cast<float>( outLiveCount ) );
    }
    return leader.pos();
}

mu::Vec3 MU_CALLCONV GrandBaumMidBossTactic::calcLiveSlimeCentroid(
    Room& room, const PlatoonLeader& leader, int32& outLiveCount ) const {
    mu::Vec3 sum{};
    outLiveCount = 0;

    const auto& squads = leader.getSquads();
    const size_t slimeIndices[] = { 0, 1, 2 };
    for ( size_t idx : slimeIndices ) {
        if ( idx >= squads.size() || !squads[ idx ] ) {
            continue;
        }
        for ( uint32 memberId : squads[ idx ]->getMembers() ) {
            TacticalNpc* npc = room.findTacticalNpcById( memberId );
            if ( npc && npc->hp() > 0 ) {
                sum = sum + npc->pos();
                ++outLiveCount;
            }
        }
    }

    if ( outLiveCount > 0 ) {
        return sum * ( 1.f / static_cast<float>( outLiveCount ) );
    }
    return leader.pos();
}

// 보스 이동은 물리 motor(setDesiredVel)에 위임 — 시뮬의 setPosition 직접 적분 대체(dt 불필요).
void MU_CALLCONV GrandBaumMidBossTactic::moveBossToward(
    PlatoonLeader& leader, mu::Vec3 targetPos, float speedMult ) const {
    mu::Vec3 toTarget = targetPos - leader.pos();
    float dist = toTarget.len();
    if ( dist <= 0.01f ) {
        return;
    }

    mu::Vec3 dir = toTarget * ( 1.f / dist );
    leader.setFacing( dir );

    float spd = leader.getLeaderMoveSpeed() * speedMult;
    leader.setDesiredVel( mu::Vec3( dir.x() * spd, 0.f, dir.z() * spd ) );
}

// 슬라임 부대(0,1,2)만 안정화 교전(균형배정 + 생존중 고정). 원본 뱀(3)은 updateSnakeEvasion이,
// 증원 웨이브는 DistributedEngage가 별도 처리한다. issueStableEngage는 MidBossTacticBase 공용.
void GrandBaumMidBossTactic::issueEngage( Room& room, PlatoonLeader& leader ) {
    const auto& squads = leader.getSquads();
    std::vector<TacticalSquad*> slimeSquads;
    for ( size_t i = 0; i < squads.size() && i < 3; ++i ) {
        if ( squads[ i ] && !squads[ i ]->isEmpty() ) {
            slimeSquads.push_back( squads[ i ] );
        }
    }
    if ( slimeSquads.empty() ) {
        return;
    }

    // Engage 진입 첫 발행(및 Cooldown 복귀)은 전면 재배정, 이후엔 sticky 유지.
    issueStableEngage( room, slimeSquads, !engageOrderIssued_ );
}

// 원본 뱀 개개를 personal HoldSlot으로 산개/회피시킨다. 위협 가중 중심에서 도주(Evasion)하거나
// wander 중심 주변을 배회(Wander). 플레이어 감지/이탈 거리로 회피 상태를 히스테리시스 전환.
void GrandBaumMidBossTactic::updateSnakeEvasion( Seconds dt, Room& room, PlatoonLeader& /*leader*/, TacticalSquad* snakeSquad ) {
    if ( !snakeSquad || snakeSquad->isEmpty() ) {
        return;
    }

    if ( !snakeWanderCenterSet_ ) {
        snakeWanderCenter_ = snakeSquad->calcCentroid();
        snakeWanderCenterSet_ = true;
    }

    std::vector<uint32> liveSnakeIds;
    liveSnakeIds.reserve( snakeSquad->getMembers().size() );
    for ( uint32 memberId : snakeSquad->getMembers() ) {
        TacticalNpc* npc = room.findTacticalNpcById( memberId );
        if ( npc && npc->hp() > 0 ) {
            liveSnakeIds.push_back( memberId );
        }
    }

    auto isLiveSnake = [&]( uint32 id ) {
        return std::find( liveSnakeIds.begin(), liveSnakeIds.end(), id ) != liveSnakeIds.end();
    };

    for ( auto it = snakePersonalTargets_.begin(); it != snakePersonalTargets_.end(); ) {
        if ( !isLiveSnake( it->first ) ) it = snakePersonalTargets_.erase( it );
        else ++it;
    }
    for ( auto it = snakePersonalTimers_.begin(); it != snakePersonalTimers_.end(); ) {
        if ( !isLiveSnake( it->first ) ) it = snakePersonalTimers_.erase( it );
        else ++it;
    }
    for ( auto it = snakePersonalEvading_.begin(); it != snakePersonalEvading_.end(); ) {
        if ( !isLiveSnake( it->first ) ) it = snakePersonalEvading_.erase( it );
        else ++it;
    }

    for ( uint32 memberId : liveSnakeIds ) {
        TacticalNpc* snake = room.findTacticalNpcById( memberId );
        if ( !snake ) {
            continue;
        }

        mu::Vec3 snakePos = snake->pos();
        uint32 targetId = selectNearestPlayerId( room, snakePos );
        GameSession* nearestPlayer = selectNearestPlayer( room, snakePos );
        mu::Vec3 nearestPos = nearestPlayer ? nearestPlayer->player()->pos() : snakeWanderCenter_;

        bool wasEvading = snakePersonalEvading_[ memberId ];
        bool shouldEvade = false;
        if ( nearestPlayer ) {
            float nearestDist = ( snakePos - nearestPos ).len();
            shouldEvade = wasEvading
                ? ( nearestDist < SNAKE_STOP_EVADE_RANGE )
                : ( nearestDist < SNAKE_DETECT_RANGE );
        }

        mu::Vec3 threatCenter{};
        float threatWeightSum = 0.f;
        for ( GameSession* s : room.getLivingPlayers() ) {
            if ( !s ) {
                continue;
            }
            mu::Vec3 playerPos = s->player()->pos();
            float dist = ( snakePos - playerPos ).len();
            float weight = std::max( 0.f, SNAKE_THREAT_WEIGHT_RANGE - dist );
            if ( weight <= 0.f ) {
                continue;
            }
            threatCenter = threatCenter + playerPos * weight;
            threatWeightSum += weight;
        }
        if ( threatWeightSum > 0.f ) {
            threatCenter = threatCenter * ( 1.f / threatWeightSum );
        }
        else {
            threatCenter = nearestPos;
        }

        bool isTooFarFromCenter = ( snakePos - snakeWanderCenter_ ).len() > SNAKE_PERSONAL_MAX_LEASH_RADIUS;

        Seconds& timer = snakePersonalTimers_[ memberId ];
        timer -= dt;
        if ( shouldEvade != wasEvading ) {
            timer = 0s;
            snakePersonalEvading_[ memberId ] = shouldEvade;
        }

        if ( timer <= 0s || isTooFarFromCenter ||
             snakePersonalTargets_.find( memberId ) == snakePersonalTargets_.end() ) {
            float random01 = static_cast<float>( std::rand() ) / static_cast<float>( RAND_MAX );

            if ( shouldEvade ) {
                mu::Vec3 fleeDir = snakePos - threatCenter;
                if ( fleeDir.len2() < 0.01f ) fleeDir = mu::Vec3( 1.f, 0.f, 0.f );
                else                          fleeDir = norm3( fleeDir );

                mu::Vec3 right( -fleeDir.z(), 0.f, fleeDir.x() );
                float scatter = ( random01 * 2.f - 1.f ) * SNAKE_PERSONAL_SCATTER_RADIUS;
                snakePersonalTargets_[ memberId ] = snakePos + fleeDir * SNAKE_EVASION_RADIUS + right * scatter;
                timer = SNAKE_EVASION_REFRESH * ( 0.75f + random01 * 0.5f );
            }
            else {
                snakePersonalTargets_[ memberId ] = pickSnakePersonalWanderTarget( snakeWanderCenter_ );
                timer = SNAKE_WANDER_INTERVAL * ( 0.75f + random01 * 0.5f );
            }

            if ( isTooFarFromCenter && !shouldEvade ) {
                mu::Vec3 fromCenter = snakePos - snakeWanderCenter_;
                if ( fromCenter.len2() > 0.01f ) {
                    snakePersonalTargets_[ memberId ] = snakeWanderCenter_ + norm3( fromCenter ) * SNAKE_DISPERSE_WANDER_RADIUS;
                }
            }
        }

        mu::Vec3 targetFromCenter = snakePersonalTargets_[ memberId ] - snakeWanderCenter_;
        if ( targetFromCenter.len() > SNAKE_PERSONAL_MAX_LEASH_RADIUS ) {
            snakePersonalTargets_[ memberId ] = snakeWanderCenter_ + norm3( targetFromCenter ) * SNAKE_PERSONAL_MAX_LEASH_RADIUS;
        }

        if ( targetId == 0 ) {
            TacticalCommand idle;
            idle.type = TacticalCommandType::Idle;
            snake->receiveCommand( idle );
            continue;
        }

        TacticalCommand cmd;
        cmd.type = TacticalCommandType::HoldSlot;
        cmd.targetId = targetId;
        cmd.slotOffset = snakePersonalTargets_[ memberId ];
        cmd.speedMult = shouldEvade ? SNAKE_EVASION_SPEED_MULT : SNAKE_WANDER_SPEED_MULT;
        snake->receiveCommand( cmd );
    }
}

mu::Vec3 MU_CALLCONV GrandBaumMidBossTactic::pickSnakePersonalWanderTarget( mu::Vec3 center ) const {
    float angle = static_cast<float>( std::rand() ) / static_cast<float>( RAND_MAX ) * 2.f * 3.14159265f;
    float dist  = SNAKE_DISPERSE_WANDER_RADIUS * ( 0.3f + 0.7f * static_cast<float>( std::rand() ) / static_cast<float>( RAND_MAX ) );
    return center + mu::Vec3( std::cos( angle ) * dist, 0.f, std::sin( angle ) * dist );
}

// 슬라임 부대를 보스 중심 원형 링(RingGuard)으로 배치. 각 부대는 생존 비율만큼 원호 섹터를 나눠 가짐.
void GrandBaumMidBossTactic::issueShieldWall( Room& room, PlatoonLeader& leader ) {
    const auto& squads = leader.getSquads();
    uint32 targetId = selectNearestPlayerId( room, leader.pos() );

    mu::Vec3 leaderPos = leader.pos();
    mu::Vec3 playerCentroid = calcPlayerCentroid( room, leaderPos );
    mu::Vec3 forward = playerCentroid - leaderPos;
    if ( forward.len2() > 0.01f ) {
        forward = norm3( forward );
    }
    else {
        forward = mu::Vec3( 1.f, 0.f, 0.f );
    }

    if ( !shieldWallRingIssued_ ) {
        shieldWallRingCenter_ = leaderPos;
        shieldWallRingStartAngle_ = std::atan2f( forward.z(), forward.x() ) - 3.14159265f;
        shieldWallRingIssued_ = true;
        room.knockPlayersOutOfShieldWall( shieldWallRingCenter_, shieldWallRingRadius_ );

        std::vector<TacticalSquad*> slimeSquads;
        int32 totalSlimeMembers = 0;
        const size_t slimeIndices[] = { 0, 1, 2 };
        for ( size_t idx : slimeIndices ) {
            if ( idx >= squads.size() ) {
                continue;
            }
            TacticalSquad* squad = squads[ idx ];
            if ( !squad || squad->isEmpty() ) {
                continue;
            }
            int32 liveMembers = countLiveMembers( room, squad );
            if ( liveMembers <= 0 ) {
                continue;
            }
            slimeSquads.push_back( squad );
            totalSlimeMembers += liveMembers;
        }

        constexpr float TWO_PI = 2.f * 3.14159265f;
        float angleAccum = shieldWallRingStartAngle_;
        if ( totalSlimeMembers > 0 ) {
            for ( TacticalSquad* squad : slimeSquads ) {
                int32 liveMembers = countLiveMembers( room, squad );
                float fraction = static_cast<float>( liveMembers ) / static_cast<float>( totalSlimeMembers );
                float sectorSpan = TWO_PI * fraction;

                auto ord = SquadOrder{};
                ord.type = SquadOrderType::RingGuard;
                ord.targetId = targetId;
                ord.tacticCenter = shieldWallRingCenter_;
                ord.sectorAngle = angleAccum + sectorSpan * 0.5f;
                ord.sectorSpan = sectorSpan;
                ord.approachRadius = shieldWallRingRadius_;
                ord.slotSpacingScale = SHIELD_RING_MIN_ARC_SPACING;
                ord.slotColumnScale = SHIELD_RING_LANE_SPACING;
                ord.speedMult = SHIELD_WALL_APPROACH_SPEED_MULT;   // 형성 접근만 가속(전투 속도 불변)
                squad->receiveOrder( ord );

                angleAccum += sectorSpan;
            }
        }
    }

    if ( snakeAmbushStage_ == SnakeAmbushStage::RetreatingOriginal && squads.size() >= 4 ) {
        issueOriginalSnakeRetreat( room, leader, squads[ 3 ] );
    }
}

// 보스 + 슬라임(0,1,2)에 받는 피해 배율을 적용/해제. 활성 시 슬라임을 하드 블로커로 등록(링 통과 차단).
void GrandBaumMidBossTactic::applyShieldWallProtection( Room& room, PlatoonLeader& leader, bool enabled ) {
    float multiplier = enabled ? SHIELDWALL_DAMAGE_MULT : 1.f;
    leader.setDamageTakenMultiplier( multiplier );

    if ( !enabled ) {
        room.clearShieldWallBlockers();
    }

    const auto& squads = leader.getSquads();
    std::vector<uint32> blockerIds;
    const size_t slimeIndices[] = { 0, 1, 2 };
    for ( size_t idx : slimeIndices ) {
        if ( idx >= squads.size() || !squads[ idx ] ) {
            continue;
        }
        for ( uint32 memberId : squads[ idx ]->getMembers() ) {
            TacticalNpc* npc = room.findTacticalNpcById( memberId );
            if ( npc ) {
                npc->setDamageTakenMultiplier( multiplier );
                if ( enabled && npc->hp() > 0 ) {
                    blockerIds.push_back( memberId );
                }
            }
        }
    }

    if ( enabled ) {
        room.setShieldWallBlockers( blockerIds );
    }
}

void GrandBaumMidBossTactic::captureOriginalSnakeRoster( Room& room, TacticalSquad* originalSnakeSquad ) {
    if ( !originalSnakeRoster_.empty() || !originalSnakeSquad ) {
        return;
    }

    for ( uint32 memberId : originalSnakeSquad->getMembers() ) {
        TacticalNpc* npc = room.findTacticalNpcById( memberId );
        if ( !npc ) {
            continue;
        }
        originalSnakeRoster_.push_back( memberId );
        originalSnakeSpawnPositions_[ memberId ] = npc->pos();
    }
}

bool GrandBaumMidBossTactic::shouldPreserveOriginalSnakes() const {
    return shieldWallTriggerStage_ < 2;
}

int32 GrandBaumMidBossTactic::countLiveMembers( Room& room, TacticalSquad* squad ) const {
    if ( !squad ) {
        return 0;
    }
    int32 count = 0;
    for ( uint32 memberId : squad->getMembers() ) {
        TacticalNpc* npc = room.findTacticalNpcById( memberId );
        if ( npc && npc->hp() > 0 ) {
            ++count;
        }
    }
    return count;
}

int32 GrandBaumMidBossTactic::countLiveSlimeMembers( Room& room, const PlatoonLeader& leader ) const {
    int32 count = 0;
    const auto& squads = leader.getSquads();
    const size_t slimeIndices[] = { 0, 1, 2 };
    for ( size_t idx : slimeIndices ) {
        if ( idx >= squads.size() ) {
            continue;
        }
        count += countLiveMembers( room, squads[ idx ] );
    }
    return count;
}

bool GrandBaumMidBossTactic::canFormShieldWall( int32 liveSlimeCount ) const {
    return liveSlimeCount >= MIN_SHIELD_WALL_SLIME_COUNT;
}

// 슬라임 부대(0,1,2) 중 살아있는 멤버가 있는 부대가 모두 슬롯 도착이면 형성 완료. 한 부대라도 미도착이면 false.
bool GrandBaumMidBossTactic::isShieldWallFormed( Room& room, PlatoonLeader& leader ) const {
    const auto& squads = leader.getSquads();
    const size_t slimeIndices[] = { 0, 1, 2 };
    bool anyLiveSquad = false;
    for ( size_t idx : slimeIndices ) {
        if ( idx >= squads.size() ) {
            continue;
        }
        TacticalSquad* squad = squads[ idx ];
        if ( !squad || squad->isEmpty() || countLiveMembers( room, squad ) <= 0 ) {
            continue;
        }
        anyLiveSquad = true;
        if ( !squad->areMembersAtSlots() ) {
            return false;
        }
    }
    return anyLiveSquad;
}

float GrandBaumMidBossTactic::calcShieldWallRadius( int32 liveSlimeCount ) const {
    constexpr float TWO_PI = 2.f * 3.14159265f;
    float radius = static_cast<float>( liveSlimeCount ) * SLIME_RING_SLOT_SPACING / TWO_PI;
    return std::clamp( radius, MIN_SHIELD_RING_RADIUS, MAX_SHIELD_RING_RADIUS );
}

int32 GrandBaumMidBossTactic::calcSnakeWaveSpawnCount( int32 liveOriginalSnakeCount ) const {
    if ( liveOriginalSnakeCount <= 0 ) {
        return 0;
    }
    int32 spawnCount = std::min( liveOriginalSnakeCount * SNAKE_WAVE_MULTIPLIER, SNAKE_WAVE_MAX_COUNT );
    return ( spawnCount / 4 ) * 4;
}

TacticalNpcConfig GrandBaumMidBossTactic::findSnakeConfig( Room& room, TacticalSquad* originalSnakeSquad ) const {
    if ( originalSnakeSquad ) {
        for ( uint32 memberId : originalSnakeSquad->getMembers() ) {
            TacticalNpc* npc = room.findTacticalNpcById( memberId );
            if ( npc ) {
                return npc->getConfig();
            }
        }
    }

    TacticalNpcConfig cfg;
    cfg.maxHp             = 45.f;
    cfg.moveSpeed         = 18.f;
    cfg.attackRange       = 2.6f;
    cfg.attackDamage      = 12.f;
    cfg.attackWindupTime  = 0.35s;
    cfg.attackRecoverTime = 0.8s;
    cfg.separationRadius  = 3.f;
    cfg.separationWeight  = 0.9f;
    return cfg;
}

bool GrandBaumMidBossTactic::isSnakeWaveAnnihilated( Room& room ) const {
    if ( !snakeWaveSpawned_ ) {
        return false;
    }
    for ( uint32 npcId : snakeWaveNpcIds_ ) {
        TacticalNpc* npc = room.findTacticalNpcById( npcId );
        if ( npc && npc->hp() > 0 ) {
            return false;
        }
    }
    return true;
}

/*------------------------------------------------------------
   M2: 뱀 매복(후퇴→증원 웨이브→전멸→종료→부활)
   전투 중 NPC 동적 소환/디스폰 인프라와 함께 구현 예정.
------------------------------------------------------------*/

// RetreatingOriginal: 원본 뱀이 외곽 슬롯 도착(또는 타임아웃) → 증원 웨이브 소환 → WaveActive.
// WaveActive: 증원 웨이브가 전멸하면 ShieldWall 종료(보스 다시 취약).
void GrandBaumMidBossTactic::updateSnakeAmbush( Seconds dt, Room& room, PlatoonLeader& leader, TacticalSquad* originalSnakeSquad ) {
    if ( snakeAmbushStage_ == SnakeAmbushStage::RetreatingOriginal ) {
        snakeRetreatTimer_ += dt;

        if ( !snakeWaveSpawned_ &&
             ( ( !originalSnakeSquad || originalSnakeSquad->areMembersAtSlots() ) ||
               snakeRetreatTimer_ >= SNAKE_RETREAT_MAX_TIME ) ) {
            spawnSnakeWave( room, leader, originalSnakeSquad );   // 웨이브 cfg 추출이 끝난 뒤
            despawnOriginalSnakeSquad( room );                    // 원본 뱀을 전장에서 퇴장(숨김)
            snakeAmbushStage_ = SnakeAmbushStage::WaveActive;
        }
        return;
    }

    if ( snakeAmbushStage_ == SnakeAmbushStage::WaveActive && isSnakeWaveAnnihilated( room ) ) {
        finishShieldWall( room, leader );
    }
}

void GrandBaumMidBossTactic::issueOriginalSnakeRetreat( Room& room, PlatoonLeader& leader, TacticalSquad* originalSnakeSquad ) {
    if ( !originalSnakeSquad || originalSnakeSquad->isEmpty() ) {
        return;
    }
    uint32 targetId = selectNearestPlayerId( room, leader.pos() );
    if ( targetId == 0 ) {
        return;
    }

    mu::Vec3 snakeCentroid = originalSnakeSquad->calcCentroid();
    mu::Vec3 retreatDir = snakeCentroid - shieldWallRingCenter_;
    if ( retreatDir.len2() <= 0.01f ) {
        mu::Vec3 playerCentroid = calcPlayerCentroid( room, leader.pos() );
        retreatDir = shieldWallRingCenter_ - playerCentroid;
    }
    if ( retreatDir.len2() > 0.01f ) {
        retreatDir = norm3( retreatDir );
    }
    else {
        retreatDir = mu::Vec3( 1.f, 0.f, 0.f );
    }

    mu::Vec3 retreatCenter = shieldWallRingCenter_ + retreatDir * SNAKE_OUTER_RADIUS;

    SquadOrder ord;
    ord.type = SquadOrderType::FormationHold;
    ord.targetId = targetId;
    ord.tacticCenter = retreatCenter;
    ord.formationTargetPos = shieldWallRingCenter_;
    ord.slotSpacingScale = 0.75f;
    ord.slotColumnScale = 2.0f;
    ord.speedMult = SNAKE_RETREAT_SPEED_MULT;
    originalSnakeSquad->receiveOrder( ord );
}

// 증원 뱀 웨이브(원본수×10, 최대 60)를 링 외곽 원주에 동적 소환하고 클라에 통지.
void GrandBaumMidBossTactic::spawnSnakeWave( Room& room, PlatoonLeader& leader, TacticalSquad* originalSnakeSquad ) {
    int32 spawnCount = calcSnakeWaveSpawnCount( originalSnakeCountAtShieldWall_ );
    if ( spawnCount <= 0 ) {
        return;
    }

    cleanupSnakeWave( room );
    snakeWaveSpawned_ = true;
    snakeWaveSquadId_ = SNAKE_WAVE_SQUAD_ID;
    snakeWaveNpcIds_.clear();

    TacticalNpcConfig cfg = findSnakeConfig( room, originalSnakeSquad );
    auto waveSquad = std::make_unique<TacticalSquad>( snakeWaveSquadId_, cfg.attackRange, cfg.separationRadius );

    constexpr float TWO_PI = 2.f * 3.14159265f;
    for ( int32 i = 0; i < spawnCount; ++i ) {
        float angle = shieldWallRingStartAngle_ + TWO_PI * static_cast<float>( i ) / static_cast<float>( spawnCount );
        mu::Vec3 pos = shieldWallRingCenter_ + mu::Vec3( std::cos( angle ) * SNAKE_OUTER_RADIUS, 0.f, std::sin( angle ) * SNAKE_OUTER_RADIUS );

        TacticalNpc* snake = room.spawnTacticalWaveNpc( pos, cfg, snakeWaveSquadId_ );
        if ( !snake ) {
            continue;
        }
        waveSquad->addMember( snake );
        snakeWaveNpcIds_.push_back( snake->getId() );
    }

    TacticalSquad* waveSquadPtr = room.addDynamicTacticalSquad( std::move( waveSquad ) );
    issueSnakeWaveEngage( room, waveSquadPtr );
    room.broadcastTacticalNpcSpawn( snakeWaveNpcIds_ );
}

void GrandBaumMidBossTactic::issueSnakeWaveEngage( Room& room, TacticalSquad* waveSquad ) {
    if ( !waveSquad || waveSquad->isEmpty() ) {
        return;
    }

    std::vector<uint32> playerIds;
    for ( GameSession* s : room.getLivingPlayers() ) {
        if ( s ) {
            playerIds.push_back( static_cast<uint32>( s->id() ) );
        }
    }
    std::sort( playerIds.begin(), playerIds.end() );

    if ( playerIds.empty() ) {
        SquadOrder idle;
        idle.type = SquadOrderType::Idle;
        waveSquad->receiveOrder( idle );
        return;
    }

    SquadOrder ord;
    ord.type = SquadOrderType::DistributedEngage;
    ord.targetIds = playerIds;
    waveSquad->receiveOrder( ord );
}

void GrandBaumMidBossTactic::finishShieldWall( Room& room, PlatoonLeader& leader ) {
    applyShieldWallProtection( room, leader, false );
    cleanupSnakeWave( room );
    reviveOriginalSnakeSquad( room, leader );
    snakeAmbushStage_ = SnakeAmbushStage::ReturningOriginal;
    issueEngage( room, leader );
    tacticCooldown_ = TACTIC_COOLDOWN_DURATION;
    enterPhase( Phase::Cooldown, leader );
}

void GrandBaumMidBossTactic::cleanupSnakeWave( Room& room ) {
    // 보스 사망/스킵 등으로 아직 살아있는 웨이브가 있으면 클라에 사망 통지 후 제거(디스폰 패킷 대체).
    for ( uint32 npcId : snakeWaveNpcIds_ ) {
        TacticalNpc* npc = room.findTacticalNpcById( npcId );
        if ( npc && npc->hp() > 0 ) {
            npc->setHp( 0 );
            room.broadcast( PacketManager::makeSHitPacket( static_cast<uint16>( npcId ), 0, 0 ) );
        }
    }

    if ( snakeWaveSquadId_ >= 0 ) {
        room.removeTacticalSquadById( snakeWaveSquadId_ );
    }
    for ( uint32 npcId : snakeWaveNpcIds_ ) {
        room.removeTacticalNpcById( npcId );
    }

    snakeWaveNpcIds_.clear();
    snakeWaveSquadId_ = -1;
    snakeWaveSpawned_ = false;
}

// 후퇴 완료한 원본 뱀을 전장에서 퇴장(숨김)시킨다. 서버 상태는 사망(hp0+물리제거)으로 두되 클라엔
// 시체 없이 즉시 숨김(S_NpcHide). 살아있던 뱀만 숨기고(이미 죽은 뱀은 시체로 유지), 복귀는
// finishShieldWall→reviveOriginalSnakeSquad가 hp<=0 분기로 일괄 부활시킨다.
void GrandBaumMidBossTactic::despawnOriginalSnakeSquad( Room& room ) {
    std::vector<uint32> hiddenIds;
    for ( uint32 memberId : originalSnakeRoster_ ) {
        TacticalNpc* npc = room.findTacticalNpcById( memberId );
        if ( !npc || npc->hp() <= 0 ) {
            continue;
        }
        room.despawnTacticalNpcHidden( memberId );
        hiddenIds.push_back( memberId );
    }
    if ( !hiddenIds.empty() ) {
        room.broadcast( PacketManager::makeSNpcHidePacket( hiddenIds ) );
    }
}

// 후퇴했던(또는 죽었던) 원본 뱀을 외곽에서 복귀시켜 squad에 재등록하고 evasion을 재개한다.
void GrandBaumMidBossTactic::reviveOriginalSnakeSquad( Room& room, PlatoonLeader& leader ) {
    if ( originalSnakeRoster_.empty() || leader.getSquads().size() < 4 ) {
        return;
    }
    TacticalSquad* originalSnakeSquad = leader.getSquads()[ 3 ];
    if ( !originalSnakeSquad ) {
        return;
    }

    mu::Vec3 reviveCenter = shieldWallRingIssued_ ? shieldWallRingCenter_ : leader.pos();
    constexpr float TWO_PI = 2.f * 3.14159265f;

    for ( size_t i = 0; i < originalSnakeRoster_.size(); ++i ) {
        uint32 memberId = originalSnakeRoster_[ i ];
        TacticalNpc* snake = room.findTacticalNpcById( memberId );
        if ( !snake ) {
            continue;
        }

        float angle = shieldWallRingStartAngle_ +
            TWO_PI * static_cast<float>( i ) / static_cast<float>( std::max<size_t>( 1, originalSnakeRoster_.size() ) );
        mu::Vec3 revivePos = reviveCenter + mu::Vec3( std::cos( angle ) * SNAKE_OUTER_RADIUS, 0.f, std::sin( angle ) * SNAKE_OUTER_RADIUS );
        if ( !shieldWallRingIssued_ ) {
            auto spawnIt = originalSnakeSpawnPositions_.find( memberId );
            if ( spawnIt != originalSnakeSpawnPositions_.end() ) {
                revivePos = spawnIt->second;
            }
        }

        if ( snake->hp() <= 0 ) {
            room.reviveTacticalNpc( memberId, revivePos );   // 물리 재등록 + S_NpcRespawn
        }

        snake->setSquadId( originalSnakeSquad->getSquadId() );
        const std::vector<uint32>& members = originalSnakeSquad->getMembers();
        if ( std::find( members.begin(), members.end(), memberId ) == members.end() ) {
            originalSnakeSquad->addMember( snake );
        }
    }

    snakePersonalTargets_.clear();
    snakePersonalTimers_.clear();
    snakePersonalEvading_.clear();
    snakeWanderCenterSet_ = false;
}

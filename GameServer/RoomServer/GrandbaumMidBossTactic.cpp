#include "rspch.hpp"
#include "GrandbaumMidBossTactic.hpp"
#include "PlatoonLeader.hpp"
#include "Room.hpp"
#include "GameSession.hpp"
#include "TacticalSquad.hpp"
#include "TacticalNpc.hpp"
#include "object.hpp"
#include <algorithm>
#include <cmath>
#include <vector>

/*--------------------------------
      Local Utility Function
--------------------------------*/

static mu::Vec3 norm3( mu::Vec3 v ) {
    float l = v.len();
    return l > 0.0001f ? v * ( 1.f / l ) : mu::Vec3( 0.f, 0.f, 0.f );
}

/*-------------------------------
      GrandbaumMidBossTactic
-------------------------------*/

void GrandbaumMidBossTactic::update( Seconds dt, Room& room, PlatoonLeader& leader ) {
    leader.removeDeadMembersFromSquads();

    // 첫 틱: 평상시 피해 프로파일(보스 취약 / 슬라임 단단) 1회 보장.
    if ( !damageProfileInitialized_ ) {
        applyNormalDamageProfile( room, leader );
        damageProfileInitialized_ = true;
    }

    const auto& squads = leader.getSquads();
    if ( squads.empty() ) {
        return;
    }

    // 보스는 항상 최근접 플레이어를 바라본다(지휘 연출).
    uint32 facingTargetId = selectNearestPlayerId( room, leader.pos() );
    if ( facingTargetId != 0 ) {
        GameSession* facingTarget = room.findLivingSessionByPlayerId( static_cast<int32>( facingTargetId ) );
        if ( facingTarget ) {
            mu::Vec3 dir = facingTarget->player()->pos() - leader.pos();
            if ( dir.len() > 0.1f ) {
                leader.setFacing( norm3( dir ) );
            }
        }
    }

    // 현재 HP 구간을 단계로 환산한다. 한 틱에 66%와 33%를 함께 통과하면 단계 2를 한 번만 소비한다.
    float hpRatio = ( leader.maxHp() > 0 )
        ? static_cast<float>( leader.hp() ) / static_cast<float>( leader.maxHp() )
        : 1.f;

    int32 currentHpStage = 0;
    if ( hpRatio <= SECOND_SHIELD_WALL_HP_RATIO ) {
        currentHpStage = 2;
    }
    else if ( hpRatio <= FIRST_SHIELD_WALL_HP_RATIO ) {
        currentHpStage = 1;
    }
    shieldWallReachedStage_ = std::max( shieldWallReachedStage_, currentHpStage );

    if ( phase_ == Phase::Cooldown ) {
        // 33% 단계가 새로 예약되면 보스가 취약한 Cooldown을 기다리지 않고 즉시 발동한다.
        if ( tryConsumePendingShieldWall( room, leader ) ) {
            return;
        }

        tacticCooldown_ -= dt;
        updateBossMelee( dt, room, leader );

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
        if ( tryConsumePendingShieldWall( room, leader ) ) {
            return;
        }

        updateBossMelee( dt, room, leader );

        if ( !engageOrderIssued_ ) {
            issueEngage( room, leader );
            engageRefreshTimer_ = ENGAGE_REFRESH_INTERVAL;
            engageOrderIssued_ = true;
        }
        else {
            engageRefreshTimer_ -= dt;
            if ( engageRefreshTimer_ <= 0s ) {
                engageRefreshTimer_ = ENGAGE_REFRESH_INTERVAL;
                issueEngage( room, leader );
            }
        }

        return;
    }

    if ( phase_ == Phase::ShieldWall ) {
        applyShieldWallProtection( room, leader );   // 매 틱 DR/블로커 갱신(죽은 슬라임 반영)

        // 벽이 형성될 때까지 링 안에 들어온 플레이어를 재넉백한다. 형성 완료/타임아웃 시 중단
        // (슬라임 barrier가 차단 인계). 고정 락(2s)이 가변 형성 시간을 못 따라가는 문제 방지.
        if ( shieldWallRingIssued_ && !shieldWallFormed_ ) {
            shieldWallFormTimer_ += dt;
            if ( isShieldWallFormed( room, leader ) || shieldWallFormTimer_ >= SHIELD_WALL_FORM_KNOCK_MAX ) {
                shieldWallFormed_ = true;
            }
            else {
                orderRefreshTimer_ -= dt;
                if ( orderRefreshTimer_ <= 0s ) {
                    orderRefreshTimer_ = ORDER_REFRESH_INTERVAL;
                    room.knockPlayersOutOfShieldWall( shieldWallRingCenter_, shieldWallRingRadius_ );
                }
            }
        }

        // 유일한 파훼: 형성 시점 대비 SHIELD_BREAK_KILL_FRACTION만큼 벽 슬라임 처치.
        int32 liveSlimes = countLiveSlimeMembers( room, leader );
        int32 breakThreshold = static_cast<int32>(
            static_cast<float>( shieldWallSlimeCountAtForm_ ) * ( 1.f - SHIELD_BREAK_KILL_FRACTION ) );
        if ( liveSlimes <= breakThreshold ) {
            // 활성 중 33% 단계가 예약됐다면 현재 벽 파훼 직후 Cooldown 없이 새 벽을 형성한다.
            if ( tryConsumePendingShieldWall( room, leader ) ) {
                return;
            }
            finishShieldWall( room, leader );
        }
        return;
    }
}

void GrandbaumMidBossTactic::onLeaderDead( Room& room, PlatoonLeader& leader ) {
    // The shared death path releases every surviving squad member's protection.
    room.clearShieldWallBlockers();
    MidBossTacticBase::onLeaderDead( room, leader );   // pushConfusedToSquads
}

void GrandbaumMidBossTactic::enterPhase( Phase next, PlatoonLeader& leader ) {
    phase_ = next;

    if ( next == Phase::Engage ) {
        engageOrderIssued_ = false;
        engageRefreshTimer_ = 0s;
        shieldWallRingIssued_ = false;
        shieldWallFormed_ = false;
        shieldWallFormTimer_ = 0s;
        shieldWallRingRadius_ = MAX_SHIELD_RING_RADIUS;
        return;
    }

    if ( next == Phase::ShieldWall ) {
        orderRefreshTimer_ = ORDER_REFRESH_INTERVAL;
        shieldWallRingIssued_ = false;
        shieldWallFormed_ = false;
        shieldWallFormTimer_ = 0s;
        resetBossMelee( leader );   // 발동 중 보스는 벽 안에서 보호받으며 대기
        return;
    }

    if ( next == Phase::Cooldown ) {
        engageRefreshTimer_ = ENGAGE_REFRESH_INTERVAL;
        shieldWallRingIssued_ = false;
        shieldWallRingRadius_ = MAX_SHIELD_RING_RADIUS;
    }
}

/*------------------------------------------------------------
   피해경감(DR) 프로파일
------------------------------------------------------------*/

void GrandbaumMidBossTactic::setAllSlimeDamageMultiplier( Room& room, PlatoonLeader& leader, float m ) const {
    for ( TacticalSquad* sq : leader.getSquads() ) {
        if ( !sq ) {
            continue;
        }
        for ( uint32 memberId : sq->getMembers() ) {
            if ( TacticalNpc* npc = room.findTacticalNpcById( memberId ) ) {
                npc->setDamageTakenMultiplier( m );
            }
        }
    }
}

// 평상시: 보스 취약(1.0) / 슬라임 무적(0.0) + 블로커 해제.
void GrandbaumMidBossTactic::applyNormalDamageProfile( Room& room, PlatoonLeader& leader ) {
    leader.setDamageTakenMultiplier( 1.f );
    setAllSlimeDamageMultiplier( room, leader, SLIME_NORMAL_DAMAGE_MULT );
    room.clearShieldWallBlockers();
}

// ShieldWall 발동 중: 보스 보호(0.1) / 슬라임 반감(0.5) + 살아있는 슬라임을 하드 블로커로.
void GrandbaumMidBossTactic::applyShieldWallProtection( Room& room, PlatoonLeader& leader ) {
    leader.setDamageTakenMultiplier( SHIELDWALL_BOSS_DAMAGE_MULT );
    setAllSlimeDamageMultiplier( room, leader, SHIELDWALL_SLIME_DAMAGE_MULT );

    std::vector<uint32> blockerIds;
    for ( TacticalSquad* sq : leader.getSquads() ) {
        if ( !sq ) {
            continue;
        }
        for ( uint32 memberId : sq->getMembers() ) {
            TacticalNpc* npc = room.findTacticalNpcById( memberId );
            if ( npc && npc->hp() > 0 ) {
                blockerIds.push_back( memberId );
            }
        }
    }
    room.setShieldWallBlockers( blockerIds );
}

/*------------------------------------------------------------
   보스 melee (최근접 플레이어 추적·근접 공격)
------------------------------------------------------------*/

void GrandbaumMidBossTactic::resetBossMelee( PlatoonLeader& leader ) {
    bossMeleeState_ = BossMeleeState::AcquireTarget;
    bossMeleeTimer_ = 0s;
    bossMeleeTargetId_ = 0;
    leader.setTacticalTarget( 0 );
    leader.setDesiredVel( mu::Vec3{} );
    leader.transitionTacticalState( TacticalNpcState::Idle );
}

void GrandbaumMidBossTactic::updateBossMelee( Seconds dt, Room& room, PlatoonLeader& leader ) {
    if ( bossMeleeState_ == BossMeleeState::AcquireTarget ) {
        bossMeleeTargetId_ = selectNearestPlayerId( room, leader.pos() );
        leader.setTacticalTarget( bossMeleeTargetId_ );
        if ( bossMeleeTargetId_ == 0 ) {
            leader.setDesiredVel( mu::Vec3{} );
            leader.transitionTacticalState( TacticalNpcState::Idle );
            return;
        }
        bossMeleeState_ = BossMeleeState::ChaseTarget;
        leader.transitionTacticalState( TacticalNpcState::Chase );
        return;
    }

    GameSession* target = room.findLivingSessionByPlayerId( static_cast<int32>( bossMeleeTargetId_ ) );
    if ( !target ) {
        resetBossMelee( leader );
        return;
    }

    TacticalNpcConfig cfg = leader.getConfig();
    float attackRange = leader.getAttackRange();
    float attackRangeSq = attackRange * attackRange;
    mu::Vec3 targetPos = target->player()->pos();

    if ( bossMeleeState_ == BossMeleeState::ChaseTarget ) {
        if ( ( targetPos - leader.pos() ).len2() <= attackRangeSq ) {
            bossMeleeState_ = BossMeleeState::AttackWindup;
            bossMeleeTimer_ = 0s;
            leader.setDesiredVel( mu::Vec3{} );
            leader.transitionTacticalState( TacticalNpcState::AttackWindup );
            return;
        }
        moveBossToward( leader, targetPos, BOSS_CHASE_SPEED_MULT );
        return;
    }

    if ( bossMeleeState_ == BossMeleeState::AttackWindup ) {
        mu::Vec3 toTarget = targetPos - leader.pos();
        if ( toTarget.len2() > 0.01f ) {
            leader.setFacing( norm3( toTarget ) );
        }
        leader.setDesiredVel( mu::Vec3{} );

        bossMeleeTimer_ += dt;
        if ( bossMeleeTimer_ < cfg.attackWindupTime ) {
            return;
        }

        if ( ( targetPos - leader.pos() ).len2() <= attackRangeSq ) {
            // 스킬 기반 공격(다양 + damageScale). 등록 스킬이 없으면 레거시 직접 데미지.
            if ( !leader.castSkillAttack( room ) ) {
                leader.applyHitToSession( target, leader.getAttackDamage() );
            }
        }

        bossMeleeState_ = BossMeleeState::AttackRecover;
        bossMeleeTimer_ = 0s;
        leader.transitionTacticalState( TacticalNpcState::AttackRecover );
        return;
    }

    if ( bossMeleeState_ == BossMeleeState::AttackRecover ) {
        mu::Vec3 toTarget = targetPos - leader.pos();
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

// 보스 이동은 물리 motor(setDesiredVel)에 위임.
void MU_CALLCONV GrandbaumMidBossTactic::moveBossToward(
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

/*------------------------------------------------------------
   부대 명령
------------------------------------------------------------*/

// 전 슬라임 부대를 안정화 교전(균형배정 + 생존중 고정)시킨다.
void GrandbaumMidBossTactic::issueEngage( Room& room, PlatoonLeader& leader ) {
    std::vector<TacticalSquad*> slimeSquads;
    for ( TacticalSquad* sq : leader.getSquads() ) {
        if ( sq && !sq->isEmpty() ) {
            slimeSquads.push_back( sq );
        }
    }
    if ( slimeSquads.empty() ) {
        return;
    }

    // Engage 진입 첫 발행(및 Cooldown 복귀)은 전면 재배정, 이후엔 sticky 유지.
    issueStableEngage( room, slimeSquads, !engageOrderIssued_ );
}

// 슬라임 부대 전체를 보스 중심 원형 링(RingGuard)으로 배치. 슬롯은 전역에서 한 번 생성한 뒤
// 각 부대에 겹치지 않게 나눠 부대 경계의 과밀/중복을 막는다.
void GrandbaumMidBossTactic::issueShieldWall( Room& room, PlatoonLeader& leader ) {
    if ( shieldWallRingIssued_ ) {
        return;
    }

    const auto& squads = leader.getSquads();
    uint32 targetId = selectNearestPlayerId( room, leader.pos() );

    mu::Vec3 leaderPos = leader.pos();
    mu::Vec3 playerCentroid = calcPlayerCentroid( room, leaderPos );
    mu::Vec3 forward = playerCentroid - leaderPos;
    forward = ( forward.len2() > 0.01f ) ? norm3( forward ) : mu::Vec3( 1.f, 0.f, 0.f );

    shieldWallRingCenter_ = leaderPos;
    shieldWallRingStartAngle_ = std::atan2f( forward.z(), forward.x() ) - 3.14159265f;
    shieldWallRingIssued_ = true;
    room.knockPlayersOutOfShieldWall( shieldWallRingCenter_, shieldWallRingRadius_ );

    struct LiveSlimeRef {
        TacticalSquad* squad{};
        TacticalNpc* npc{};
        size_t squadIndex{};
    };

    std::vector<TacticalSquad*> slimeSquads;
    std::vector<LiveSlimeRef> liveSlimes;
    for ( TacticalSquad* squad : squads ) {
        if ( !squad || squad->isEmpty() ) {
            continue;
        }

        size_t squadIndex = slimeSquads.size();
        bool hasLiveMember = false;
        for ( TacticalNpc* npc : squad->getMemberCache() ) {
            if ( npc && npc->hp() > 0 ) {
                liveSlimes.push_back( { squad, npc, squadIndex } );
                hasLiveMember = true;
            }
        }

        if ( !hasLiveMember ) {
            continue;
        }
        slimeSquads.push_back( squad );
    }

    int32 totalSlimeMembers = static_cast<int32>(liveSlimes.size());
    if ( totalSlimeMembers <= 0 ) {
        return;
    }

    constexpr float TWO_PI = 2.f * 3.14159265f;
    std::vector<mu::Vec3> globalSlots = TacticalSquad::calcRingSlots(
        shieldWallRingCenter_, shieldWallRingStartAngle_ + 3.14159265f, TWO_PI,
        shieldWallRingRadius_, totalSlimeMembers,
        SHIELD_RING_MIN_ARC_SPACING, SHIELD_RING_LANE_SPACING,
        static_cast<int32>(slimeSquads.size()) );

    std::vector<bool> slotUsed( globalSlots.size(), false );
    std::vector<std::vector<mu::Vec3>> slotsBySquad( slimeSquads.size() );

    // 현재 위치에서 가까운 전역 슬롯을 일대일로 배정한다. 부대별 재배정은 이 부분집합 안에서만
    // 일어나므로 서로 다른 부대가 같은 슬롯을 노릴 수 없다.
    for ( const LiveSlimeRef& slime : liveSlimes ) {
        int32 bestSlot = -1;
        float bestDistSq = -1.f;
        for ( int32 i = 0; i < static_cast<int32>(globalSlots.size()); ++i ) {
            if ( slotUsed[ i ] ) {
                continue;
            }

            float dSq = (slime.npc->pos() - globalSlots[ i ]).len2();
            if ( bestDistSq < 0.f || dSq < bestDistSq ) {
                bestDistSq = dSq;
                bestSlot = i;
            }
        }

        if ( bestSlot >= 0 ) {
            slotUsed[ bestSlot ] = true;
            slotsBySquad[ slime.squadIndex ].push_back( globalSlots[ bestSlot ] );
        }
    }

    float angleAccum = shieldWallRingStartAngle_;
    for ( size_t i = 0; i < slimeSquads.size(); ++i ) {
        TacticalSquad* squad = slimeSquads[ i ];
        int32 liveMembers = static_cast<int32>(slotsBySquad[ i ].size());
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
        ord.explicitSlots = std::move( slotsBySquad[ i ] );
        squad->receiveOrder( ord );

        angleAccum += sectorSpan;
    }
}

void GrandbaumMidBossTactic::finishShieldWall( Room& room, PlatoonLeader& leader ) {
    applyNormalDamageProfile( room, leader );   // 보스 취약/슬라임 단단 복귀 + clearShieldWallBlockers
    issueEngage( room, leader );
    tacticCooldown_ = TACTIC_COOLDOWN_DURATION;
    enterPhase( Phase::Cooldown, leader );
}

bool GrandbaumMidBossTactic::tryConsumePendingShieldWall( Room& room, PlatoonLeader& leader ) {
    if ( shieldWallReachedStage_ <= shieldWallConsumedStage_ ) {
        return false;
    }

    // 도달한 최고 단계를 한 번에 소비한다. 100%에서 33% 이하로 바로 내려가도 발동은 한 번뿐이다.
    shieldWallConsumedStage_ = shieldWallReachedStage_;

    int32 liveSlimes = countLiveSlimeMembers( room, leader );
    if ( !canFormShieldWall( liveSlimes ) ) {
        applyNormalDamageProfile( room, leader );
        issueEngage( room, leader );
        tacticCooldown_ = TACTIC_COOLDOWN_DURATION;
        enterPhase( Phase::Cooldown, leader );
        return true;
    }

    shieldWallSlimeCountAtForm_ = liveSlimes;
    shieldWallRingRadius_ = calcShieldWallRadius( liveSlimes, countLiveSlimeSquads( room, leader ) );
    enterPhase( Phase::ShieldWall, leader );
    applyShieldWallProtection( room, leader );   // 보스 보호(0.1) + 슬라임 반감(0.5) + 하드 블로커
    issueShieldWall( room, leader );             // 넉백 + 슬라임 링(RingGuard) 발령(1회)
    return true;
}

/*------------------------------------------------------------
   집계 / 판정
------------------------------------------------------------*/

int32 GrandbaumMidBossTactic::countLiveMembers( Room& room, TacticalSquad* squad ) const {
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

int32 GrandbaumMidBossTactic::countLiveSlimeMembers( Room& room, PlatoonLeader& leader ) const {
    int32 count = 0;
    for ( TacticalSquad* sq : leader.getSquads() ) {
        count += countLiveMembers( room, sq );
    }
    return count;
}

int32 GrandbaumMidBossTactic::countLiveSlimeSquads( Room& room, PlatoonLeader& leader ) const {
    int32 count = 0;
    for ( TacticalSquad* sq : leader.getSquads() ) {
        if ( countLiveMembers( room, sq ) > 0 ) {
            ++count;
        }
    }
    return count;
}

bool GrandbaumMidBossTactic::canFormShieldWall( int32 liveSlimeCount ) const {
    return liveSlimeCount >= MIN_SHIELD_WALL_SLIME_COUNT;
}

// 살아있는 멤버가 있는 부대가 모두 슬롯에 실제 안착하면 형성 완료. 한 부대라도 이동 중이면 false.
bool GrandbaumMidBossTactic::isShieldWallFormed( Room& room, PlatoonLeader& leader ) const {
    bool anyLiveSquad = false;
    for ( TacticalSquad* squad : leader.getSquads() ) {
        if ( !squad || squad->isEmpty() || countLiveMembers( room, squad ) <= 0 ) {
            continue;
        }
        anyLiveSquad = true;
        if ( !squad->areMembersSettledAtSlots() ) {
            return false;
        }
    }
    return anyLiveSquad;
}

float GrandbaumMidBossTactic::calcShieldWallRadius( int32 liveSlimeCount, int32 liveSquadCount ) const {
    constexpr float TWO_PI = 2.f * 3.14159265f;
    liveSquadCount = std::max( liveSquadCount, 1 );

    // calcRingSlots와 같은 수용량 규칙으로, outward lane 없이 전원을 수용하고 각 링이
    // 클라이언트 barrier 연결 거리 안에서 닫히는지 검사한다.
    auto fitsCompactRing = [&]( float outerRadius, int32* totalCapacity ) {
        bool capacityOnly = totalCapacity != nullptr;
        float laneSpacing = std::max( SHIELD_RING_LANE_SPACING, SHIELD_RING_MIN_ARC_SPACING );
        float innerFloor = std::max( laneSpacing, outerRadius * 0.5f );
        int32 inwardLaneCount = std::max(
            static_cast<int32>(std::floorf((outerRadius - innerFloor) / laneSpacing)) + 1, 1 );

        int32 remaining = liveSlimeCount;
        int32 capacitySum = 0;
        for ( int32 lane = 0; lane < inwardLaneCount; ++lane ) {
            float radius = outerRadius - laneSpacing * static_cast<float>( lane );
            float chordRatio = std::clamp( SHIELD_RING_MIN_ARC_SPACING / (2.f * radius), 0.f, 1.f );
            float minSlotAngle = 2.f * std::asinf( chordRatio );
            int32 laneCapacity = std::max( static_cast<int32>(std::floorf(TWO_PI / minSlotAngle)), 1 );
            if ( liveSquadCount > 1 && laneCapacity >= liveSquadCount ) {
                laneCapacity = (laneCapacity / liveSquadCount) * liveSquadCount;
            }
            capacitySum += laneCapacity;

            if ( capacityOnly || remaining <= 0 ) {
                continue;
            }

            int32 laneMembers = std::min( laneCapacity, remaining );
            if ( laneMembers <= 1 ) {
                return false;
            }

            float maxChord = 2.f * radius * std::sinf( 3.14159265f / static_cast<float>( laneMembers ) );
            if ( maxChord > SHIELD_RING_MAX_ARC_SPACING ) {
                return false;
            }
            remaining -= laneMembers;
        }

        if ( totalCapacity ) {
            *totalCapacity = capacitySum;
            return true;
        }
        return remaining <= 0;
    };

    // 최대 반경의 inward lane을 가득 채우는 80마리는 기존 8m 배치를 그대로 유지한다.
    int32 maxRadiusCapacity = 0;
    fitsCompactRing( MAX_SHIELD_RING_RADIUS, &maxRadiusCapacity );
    if ( liveSlimeCount >= maxRadiusCapacity ) {
        return MAX_SHIELD_RING_RADIUS;
    }

    int32 stepCount = static_cast<int32>(std::ceilf(
        (MAX_SHIELD_RING_RADIUS - MIN_SHIELD_RING_RADIUS) / SHIELD_RING_RADIUS_SEARCH_STEP ));
    for ( int32 step = 0; step <= stepCount; ++step ) {
        float radius = std::min(
            MIN_SHIELD_RING_RADIUS + SHIELD_RING_RADIUS_SEARCH_STEP * static_cast<float>( step ),
            MAX_SHIELD_RING_RADIUS );
        if ( fitsCompactRing( radius, nullptr ) ) {
            return radius;
        }
    }

    return MAX_SHIELD_RING_RADIUS;
}

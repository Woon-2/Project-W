#include "rspch.hpp"
#include "IsysMidBossTactic.hpp"
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

// 수평(XZ) 거리. 보스는 지형 높이에 붙으므로 후퇴 도착 판정은 Y를 무시(슬롯/후퇴 목표는 평평한 Y).
static float lenXZ( mu::Vec3 v ) {
    return mu::Vec3( v.x(), 0.f, v.z() ).len();
}

/*-------------------------------
      IsysMidBossTactic
-------------------------------*/

void IsysMidBossTactic::update( Seconds dt, Room& room, PlatoonLeader& leader ) {
    captureInitialSquadSizes( leader );
    leader.removeDeadMembersFromSquads();
    updateBossDamageReaction( dt, leader );

    // Isys has no dedicated BossSolo phase. If every squad is gone, cancel any
    // in-flight pincer state and reuse Engage as the boss-only combat state.
    if ( !hasLiveSquadMembers( leader ) ) {
        if ( phase_ != Phase::Engage ) {
            enterPhase( Phase::Engage, room, leader );
        }
        else {
            setEncounterDamageProfile( room, leader, 1.f, 1.f );
        }
    }

    const auto& squads = leader.getSquads();
    if ( squads.empty() ) {
        return;
    }

    // 어느 한 스쿼드든 초기 인원 80% 미만 → 전술 잠금 해제 후 즉시 협공 사이클 진입(Cooldown→Engage→후퇴).
    if ( !tacticsUnlocked_ && checkUnlockCondition( leader ) ) {
        tacticsUnlocked_ = true;
        cooldownTimer_ = 0s;
        enterPhase( Phase::Cooldown, room, leader );
    }

    if ( phase_ == Phase::Engage ) {
        if ( !engageOrderIssued_ ) {
            issueEngage( room, leader, /*reset=*/true );
            engageRefreshTimer_ = ENGAGE_REFRESH_INTERVAL;
            engageOrderIssued_ = true;
        }
        else {
            engageRefreshTimer_ -= dt;
            if ( engageRefreshTimer_ <= 0s ) {
                engageRefreshTimer_ = ENGAGE_REFRESH_INTERVAL;
                issueEngage( room, leader, /*reset=*/false );
            }
        }
        if ( tacticsUnlocked_ ) {
            cooldownTimer_ -= dt;
            if ( cooldownTimer_ <= 0s && hasLiveBomberSquad( leader ) ) {
                enterPhase( Phase::RetreatForPincer, room, leader );
                return;
            }
        }
    }
    else if ( phase_ == Phase::Cooldown ) {
        engageRefreshTimer_ -= dt;
        if ( engageRefreshTimer_ <= 0s ) {
            engageRefreshTimer_ = ENGAGE_REFRESH_INTERVAL;
            issueEngage( room, leader, /*reset=*/false );
        }
        cooldownTimer_ -= dt;
        if ( cooldownTimer_ <= 0s ) {
            enterPhase( Phase::Engage, room, leader );
        }
    }
    else if ( phase_ == Phase::RetreatForPincer ) {
        phaseTimer_ += dt;
        if ( !pincerIssued_ ) {
            issueRetreatForPincer( room, leader );
            pincerIssued_ = true;
        }
        bool leaderAtRetreat = lenXZ( retreatTargetPos_ - leader.pos() ) <= ISIS_RETREAT_ARRIVE_DIST;
        if ( ( leaderAtRetreat && allLiveSquadsAtSlots( leader ) ) || phaseTimer_ >= RETREAT_TIMEOUT ) {
            enterPhase( Phase::RegroupBombers, room, leader );
        }
    }
    else if ( phase_ == Phase::RegroupBombers ) {
        phaseTimer_ += dt;
        if ( !pincerIssued_ ) {
            issueRegroupBombers( room, leader );
            pincerIssued_ = true;
        }
        if ( activeStrikeSquadsAtSlots() || phaseTimer_ >= REGROUP_TIMEOUT ) {
            enterPhase( Phase::FirstBomberWedge, room, leader );
        }
    }
    else if ( phase_ == Phase::FirstBomberWedge ) {
        phaseTimer_ += dt;
        if ( !pincerIssued_ ) {
            issueWedgeStrike( room, leader, /*useBuddy=*/false, /*penalty=*/false, /*remember=*/true );
            pincerIssued_ = true;
            if ( hasLiveBuddySquad( leader ) ) {
                issueRegroupBuddies( room, leader );   // 1차 돌격과 동시에 Buddy 재집결 착수
            }
        }
        if ( !wedgeForceStartIssued_ && phaseTimer_ >= WEDGE_PREP_FORCE_TIMEOUT ) {
            forceActiveWedgeCharges();
            wedgeForceStartIssued_ = true;
        }
        updateActiveStrikeEngage( room, leader, /*forceAll=*/false );
        bool timeout = phaseTimer_ >= PINCER_TIMEOUT;
        if ( timeout ) {
            updateActiveStrikeEngage( room, leader, /*forceAll=*/true );
        }
        if ( activeStrikeTasksEngaged() || timeout ) {
            if ( hasLiveBuddySquad( leader ) ) {
                enterPhase( Phase::RegroupBuddies, room, leader );
            }
            else {
                enterCooldown( room, leader );
                issueEngage( room, leader, /*reset=*/true );
            }
        }
    }
    else if ( phase_ == Phase::RegroupBuddies ) {
        phaseTimer_ += dt;
        if ( !secondStrikePrepIssued_ ) {
            issueRegroupBuddies( room, leader );
        }
        if ( !hasLiveBuddySquad( leader ) ) {
            enterCooldown( room, leader );
            issueEngage( room, leader, /*reset=*/true );
            return;
        }
        if ( !ensureBossBuddyWedgeJoin( room, leader ) ) {
            enterCooldown( room, leader );
            issueEngage( room, leader, /*reset=*/true );
            return;
        }
        updateBossBuddyWedgeJoin( dt, leader );
        if ( isSecondStrikePrepReady( leader ) || phaseTimer_ >= BUDDY_REGROUP_TIMEOUT ) {
            enterPhase( Phase::SecondBuddyWedge, room, leader );
        }
    }
    else if ( phase_ == Phase::SecondBuddyWedge ) {
        phaseTimer_ += dt;
        if ( !pincerIssued_ ) {
            issueWedgeStrike( room, leader, /*useBuddy=*/true, /*penalty=*/true, /*remember=*/false );
            pincerIssued_ = true;
        }
        if ( !wedgeForceStartIssued_ && phaseTimer_ >= WEDGE_PREP_FORCE_TIMEOUT ) {
            forceActiveWedgeCharges();
            wedgeForceStartIssued_ = true;
        }
        syncBossBuddyWedgeChargeStart( leader );
        updateBossBuddyWedgeJoin( dt, leader );
        updateActiveStrikeEngage( room, leader, /*forceAll=*/false );
        bool timeout = phaseTimer_ >= PINCER_TIMEOUT;
        if ( timeout ) {
            updateActiveStrikeEngage( room, leader, /*forceAll=*/true );
        }
        bool strikeDone = activeStrikeTasksEngaged() &&
            ( !bossBuddyWedgeJoinActive_ || bossBuddyWedgeChargeComplete_ );
        if ( strikeDone || timeout ) {
            enterCooldown( room, leader );
            issueEngage( room, leader, /*reset=*/true );
        }
    }

    // ── 보스 본체 이동/상태 ──
    // 집결/쐐기 phase: 합류 로직이 보스를 이동(updateBossBuddyWedgeJoin)하거나 슬롯 대기.
    if ( phase_ == Phase::RegroupBombers || phase_ == Phase::FirstBomberWedge ||
         phase_ == Phase::RegroupBuddies || phase_ == Phase::SecondBuddyWedge ) {
        if ( bossBuddyWedgeJoinActive_ && bossBuddyWedgeDir_.len2() > 0.01f ) {
            leader.setFacing( bossBuddyWedgeDir_ );
        }
        if ( phase_ == Phase::SecondBuddyWedge && bossBuddyWedgeJoinActive_ &&
             bossBuddyWedgeChargeStarted_ && !bossBuddyWedgeChargeComplete_ ) {
            leader.transitionTacticalState( TacticalNpcState::ChargeThrough );
        }
        else {
            leader.transitionTacticalState( TacticalNpcState::HoldSlot );
        }
        return;
    }

    // 후퇴 phase: 보스가 후방 집결점으로 고속 후퇴(모터).
    if ( phase_ == Phase::RetreatForPincer ) {
        leader.transitionTacticalState( TacticalNpcState::Chase );
        if ( lenXZ( retreatTargetPos_ - leader.pos() ) > ISIS_RETREAT_ARRIVE_DIST ) {
            moveBossToward( leader, retreatTargetPos_, RETREAT_LEADER_SPEED_MULT );
        }
        else {
            leader.setDesiredVel( mu::Vec3{} );
        }
        return;
    }

    // Engage/Cooldown: 보스 근접 FSM이 이동/공격 전담.
    updateBossPersonalCombat( dt, room, leader );
}

void IsysMidBossTactic::enterCooldown( Room& room, PlatoonLeader& leader ) {
    cooldownTimer_ = rollCooldown();
    enterPhase( Phase::Cooldown, room, leader );
}

void IsysMidBossTactic::onLeaderDead( Room& room, PlatoonLeader& leader ) {
    restoreBossWedgeMotorBoost( leader );
    MidBossTacticBase::onLeaderDead( room, leader );
}

void IsysMidBossTactic::enterPhase( Phase next, Room& room, PlatoonLeader& leader ) {
    phase_ = next;
    phaseTimer_ = 0s;
    engageOrderIssued_ = false;
    pincerIssued_ = false;
    wedgeForceStartIssued_ = false;
    activeStrikeSquads_.clear();
    activeStrikeTasks_.clear();

    // FirstBomberWedge 진입 시 RegroupBuddies가 미리 깔아둔 2차 준비를 보존(1차와 동시 진행).
    bool preserveSecondPrep =
        next == Phase::FirstBomberWedge ||
        next == Phase::RegroupBuddies ||
        next == Phase::SecondBuddyWedge;
    if ( !preserveSecondPrep ) {
        secondStrikePrepIssued_ = false;
        secondStrikePrepSquads_.clear();
        secondStrikeClusters_.clear();
        resetBossBuddyWedgeJoin();
    }
    if ( next == Phase::RetreatForPincer || next == Phase::Engage ) {
        firstStrikeTargetIds_.clear();
    }

    resetBossPersonalCombat( leader );

    if ( next != Phase::SecondBuddyWedge ) {
        restoreBossWedgeMotorBoost( leader );
    }

    if ( next == Phase::Engage || next == Phase::Cooldown ) {
        engageRefreshTimer_ = 0s;
        engageTargetBySquad_.clear();
    }
    if ( next == Phase::RetreatForPincer ) {
        leader.transitionTacticalState( TacticalNpcState::Chase );
    }
    else if ( next == Phase::RegroupBombers || next == Phase::FirstBomberWedge ||
              next == Phase::RegroupBuddies || next == Phase::SecondBuddyWedge ) {
        leader.transitionTacticalState( TacticalNpcState::HoldSlot );
    }

    const bool tacticInvulnerable =
        next == Phase::RetreatForPincer ||
        next == Phase::RegroupBombers ||
        next == Phase::FirstBomberWedge ||
        next == Phase::RegroupBuddies ||
        next == Phase::SecondBuddyWedge;
    const float multiplier = tacticInvulnerable ? 0.f : 1.f;
    setEncounterDamageProfile( room, leader, multiplier, multiplier );
}

// 첫 틱(부대 배선 완료 후)에 각 스쿼드 초기 인원을 캡처 — unlock(80% 손실) 판정 기준.
void IsysMidBossTactic::captureInitialSquadSizes( const PlatoonLeader& leader ) {
    if ( initialSizesSet_ ) {
        return;
    }
    const auto& squads = leader.getSquads();
    if ( squads.empty() ) {
        return;   // 아직 부대 미배선 — 다음 틱에 캡처
    }
    initialSquadSizes_.clear();
    initialSquadSizes_.reserve( squads.size() );
    for ( TacticalSquad* sq : squads ) {
        initialSquadSizes_.push_back( sq ? static_cast<int32>( sq->getMembers().size() ) : 0 );
    }
    initialSizesSet_ = true;
}

bool IsysMidBossTactic::checkUnlockCondition( const PlatoonLeader& leader ) const {
    const auto& squads = leader.getSquads();
    for ( size_t i = 0; i < squads.size(); ++i ) {
        int32 initial = ( i < initialSquadSizes_.size() ) ? initialSquadSizes_[ i ] : 0;
        int32 current = squads[ i ] ? static_cast<int32>( squads[ i ]->getMembers().size() ) : 0;
        if ( initial > 0 &&
             static_cast<float>( current ) / static_cast<float>( initial ) < UNLOCK_SQUAD_RATIO ) {
            return true;
        }
    }
    return false;
}

// 4스쿼드 전부 균형 교전(squad별 sticky 배정). issueStableEngage는 MidBossTacticBase 공용.
void IsysMidBossTactic::issueEngage( Room& room, PlatoonLeader& leader, bool resetAssignments ) {
    std::vector<TacticalSquad*> liveSquads = collectLiveSquads( leader );
    if ( liveSquads.empty() ) {
        return;
    }
    issueStableEngage( room, liveSquads, resetAssignments );
}

void IsysMidBossTactic::updateBossDamageReaction( Seconds dt, const PlatoonLeader& leader ) {
    if ( !bossHpTracked_ ) {
        bossHpTracked_ = true;
        previousBossHp_ = static_cast<float>( leader.hp() );
        return;
    }

    float currentHp = static_cast<float>( leader.hp() );
    float damageDelta = previousBossHp_ - currentHp;
    if ( damageDelta > 0.f ) {
        bossDamageSinceBackstep_ += damageDelta;
    }
    previousBossHp_ = currentHp;

    if ( bossBackstepCooldownTimer_ > 0s ) {
        bossBackstepCooldownTimer_ = std::max( Seconds{ 0.f }, bossBackstepCooldownTimer_ - dt );
    }
}

// 보스 근접 FSM. Goblin updateBossPersonalCombat 미러(score 기반 타깃 + switch margin)에
// 피해 반응 Backstep/Retreat 추가. 협공 phase 동안엔 phase 가드로 정지(보스는 2차 쐐기에 직접 합류).
bool IsysMidBossTactic::updateBossPersonalCombat( Seconds dt, Room& room, PlatoonLeader& leader ) {
    if ( phase_ != Phase::Engage && phase_ != Phase::Cooldown ) {
        return false;
    }

    // 누적 피해 임계 초과 & 쿨다운 만료 → Backstep으로 이탈.
    if ( bossPersonalState_ != BossPersonalState::Backstep &&
         bossPersonalState_ != BossPersonalState::Retreat &&
         bossDamageSinceBackstep_ >= BOSS_DAMAGE_REACTION_THRESHOLD &&
         bossBackstepCooldownTimer_ <= 0s ) {
        beginBossBackstep( room, leader );
        return true;
    }

    auto resolveCurrentTarget = [&]() -> GameSession* {
        GameSession* s = resolveBossPersonalTarget( room, bossPersonalTargetId_ );
        if ( !s ) {
            bossPersonalState_ = BossPersonalState::EvaluateTarget;
            bossPersonalTargetId_ = 0;
        }
        return s;
    };

    if ( bossPersonalState_ == BossPersonalState::EvaluateTarget ) {
        bossPersonalTargetId_ = selectBossPersonalTarget( room, leader );
        leader.setTacticalTarget( bossPersonalTargetId_ );
        bossPersonalTimer_ = 0s;
        bossTargetEvalTimer_ = BOSS_TARGET_EVAL_INTERVAL;
        if ( bossPersonalTargetId_ == 0 ) {
            leader.setDesiredVel( mu::Vec3{} );
            leader.transitionTacticalState( TacticalNpcState::Idle );
            return true;
        }
        bossPersonalState_ = BossPersonalState::ChaseTarget;
        leader.transitionTacticalState( TacticalNpcState::Chase );
        return true;
    }

    GameSession* target = resolveCurrentTarget();
    if ( !target ) {
        return true;
    }

    mu::Vec3 toTarget = target->player()->pos() - leader.pos();
    float dist = toTarget.len();
    mu::Vec3 dir = (dist > 0.01f) ? toTarget * (1.f / dist) : mu::Vec3( 1.f, 0.f, 0.f );

    if ( bossPersonalState_ == BossPersonalState::ChaseTarget ) {
        leader.setTacticalTarget( bossPersonalTargetId_ );
        leader.transitionTacticalState( TacticalNpcState::Chase );

        bossTargetEvalTimer_ -= dt;
        if ( bossTargetEvalTimer_ <= 0s ) {
            bossTargetEvalTimer_ = BOSS_TARGET_EVAL_INTERVAL;
            BossTargetScore candidate = selectBossPersonalTargetScore( room, leader );
            float currentScore = 0.f;
            if ( candidate.targetId != 0 && candidate.targetId != bossPersonalTargetId_
                && calcBossPersonalTargetScore( room, leader, bossPersonalTargetId_, currentScore )
                && candidate.score > currentScore + BOSS_TARGET_SWITCH_MARGIN ) {
                bossPersonalTargetId_ = candidate.targetId;
                leader.setTacticalTarget( bossPersonalTargetId_ );
                target = resolveBossPersonalTarget( room, bossPersonalTargetId_ );
                if ( !target ) {
                    bossPersonalState_ = BossPersonalState::EvaluateTarget;
                    bossPersonalTargetId_ = 0;
                    return true;
                }
                toTarget = target->player()->pos() - leader.pos();
                dist = toTarget.len();
                dir = (dist > 0.01f) ? toTarget * (1.f / dist) : mu::Vec3( 1.f, 0.f, 0.f );
            }
        }

        if ( dist <= leader.getAttackRange() ) {
            bossPersonalTimer_ = 0s;
            bossPersonalState_ = BossPersonalState::AttackWindup;
            leader.setDesiredVel( mu::Vec3{} );
            leader.transitionTacticalState( TacticalNpcState::AttackWindup );
            return true;
        }

        moveBossToward( leader, target->player()->pos(), BOSS_CHASE_SPEED_MULT );
        return true;
    }

    if ( bossPersonalState_ == BossPersonalState::AttackWindup ) {
        leader.setDesiredVel( mu::Vec3{} );
        leader.setFacing( dir );
        bossPersonalTimer_ += dt;
        TacticalNpcConfig cfg = leader.getConfig();
        if ( bossPersonalTimer_ < cfg.attackWindupTime ) {
            return true;
        }
        if ( dist <= leader.getAttackRange() ) {
            // Skill-based attack (varied + damageScale); legacy direct-damage only if no roster.
            if ( !leader.castSkillAttack( room ) )
                leader.applyHitToSession( target, leader.getAttackDamage() );
        }
        bossPersonalTimer_ = 0s;
        bossPersonalState_ = BossPersonalState::AttackRecover;
        leader.transitionTacticalState( TacticalNpcState::AttackRecover );
        return true;
    }

    if ( bossPersonalState_ == BossPersonalState::AttackRecover ) {
        leader.setDesiredVel( mu::Vec3{} );
        leader.setFacing( dir );
        bossPersonalTimer_ += dt;
        TacticalNpcConfig cfg = leader.getConfig();
        if ( bossPersonalTimer_ < cfg.attackRecoverTime ) {
            return true;
        }
        bossPersonalTimer_ = 0s;
        bossPersonalState_ = BossPersonalState::EvaluateTarget;
        return true;
    }

    // Backstep: 타깃 반대 방향 고정점(bossBackstepTargetPos_)까지 고속 이탈 → 도착 시 Retreat.
    if ( bossPersonalState_ == BossPersonalState::Backstep ) {
        leader.transitionTacticalState( TacticalNpcState::Chase );
        float backstepDist = lenXZ( bossBackstepTargetPos_ - leader.pos() );
        if ( backstepDist <= BOSS_BACKSTEP_ARRIVE_DIST ) {
            bossPersonalState_ = BossPersonalState::Retreat;
            leader.setDesiredVel( mu::Vec3{} );
            return true;
        }
        moveBossToward( leader, bossBackstepTargetPos_, BOSS_BACKSTEP_SPEED_MULT );
        return true;
    }

    // Retreat: 타깃에서 충분히 멀어질 때까지 후퇴 → 재평가.
    if ( bossPersonalState_ == BossPersonalState::Retreat ) {
        leader.transitionTacticalState( TacticalNpcState::Chase );
        if ( dist >= BOSS_RETREAT_DIST ) {
            bossPersonalTimer_ = 0s;
            bossPersonalState_ = BossPersonalState::EvaluateTarget;
            leader.setDesiredVel( mu::Vec3{} );
            return true;
        }
        mu::Vec3 awayDir = (dist > 0.01f) ? dir * -1.f : mu::Vec3( -1.f, 0.f, 0.f );
        mu::Vec3 awayPos = leader.pos() + awayDir * BOSS_RETREAT_DIST;
        moveBossToward( leader, awayPos, BOSS_RETREAT_SPEED_MULT );
        return true;
    }

    return true;
}

void IsysMidBossTactic::resetBossPersonalCombat( PlatoonLeader& leader ) {
    bossPersonalState_ = BossPersonalState::EvaluateTarget;
    bossPersonalTimer_ = 0s;
    bossTargetEvalTimer_ = 0s;
    bossPersonalTargetId_ = 0;
    leader.setTacticalTarget( 0 );
    leader.setDesiredVel( mu::Vec3{} );
    leader.transitionTacticalState( TacticalNpcState::Idle );
}

void IsysMidBossTactic::beginBossBackstep( Room& room, PlatoonLeader& leader ) {
    if ( bossPersonalTargetId_ == 0 ) {
        bossPersonalTargetId_ = selectBossPersonalTarget( room, leader );
    }

    GameSession* target = resolveBossPersonalTarget( room, bossPersonalTargetId_ );
    mu::Vec3 awayDir( -1.f, 0.f, 0.f );
    if ( target ) {
        mu::Vec3 fromTarget = leader.pos() - target->player()->pos();
        if ( fromTarget.len2() > 0.01f ) {
            awayDir = norm3( fromTarget );
        }
    }

    bossBackstepTargetPos_ = leader.pos() + awayDir * BOSS_BACKSTEP_DIST;
    bossDamageSinceBackstep_ = 0.f;
    bossBackstepCooldownTimer_ = BOSS_BACKSTEP_COOLDOWN;
    bossPersonalTimer_ = 0s;
    bossPersonalState_ = BossPersonalState::Backstep;
    leader.setTacticalTarget( bossPersonalTargetId_ );
    leader.transitionTacticalState( TacticalNpcState::Chase );
}

uint32 IsysMidBossTactic::selectBossPersonalTarget( Room& room, const PlatoonLeader& leader ) const {
    return selectBossPersonalTargetScore( room, leader ).targetId;
}

// 군집 점수(인원 ×1000 − 보스 거리) 최대 플레이어 — 가장 크고 가까운 군집의 대표를 노린다.
IsysMidBossTactic::BossTargetScore
IsysMidBossTactic::selectBossPersonalTargetScore( Room& room, const PlatoonLeader& leader ) const {
    std::vector<PlayerCluster> clusters = MidBossTacticBase::buildPlayerClusters( room, CLUSTER_RADIUS );
    mu::Vec3 leaderPos = leader.pos();
    BossTargetScore best{};
    bool hasBest = false;

    for ( const PlayerCluster& cluster : clusters ) {
        float clusterBaseScore = static_cast<float>( cluster.playerIds.size() ) * 1000.f;
        for ( uint32 playerId : cluster.playerIds ) {
            const GameSession* s = room.findLivingSessionByPlayerId( static_cast<int32>( playerId ) );
            if ( !s ) {
                continue;
            }
            float distance = ( leaderPos - s->player()->pos() ).len();
            float score = clusterBaseScore - distance;
            bool better = !hasBest || score > best.score + 0.001f
                || ( std::fabs( score - best.score ) <= 0.001f && playerId < best.targetId );
            if ( !better ) {
                continue;
            }
            best.targetId = playerId;
            best.score = score;
            hasBest = true;
        }
    }
    return best;
}

bool IsysMidBossTactic::calcBossPersonalTargetScore(
    Room& room, const PlatoonLeader& leader, uint32 targetId, float& outScore ) const {
    if ( targetId == 0 ) {
        return false;
    }

    const GameSession* s = room.findLivingSessionByPlayerId( static_cast<int32>( targetId ) );
    if ( !s ) {
        return false;
    }

    std::vector<PlayerCluster> clusters = MidBossTacticBase::buildPlayerClusters( room, CLUSTER_RADIUS );
    mu::Vec3 leaderPos = leader.pos();
    for ( const PlayerCluster& cluster : clusters ) {
        if ( std::find( cluster.playerIds.begin(), cluster.playerIds.end(), targetId ) == cluster.playerIds.end() ) {
            continue;
        }
        float distance = ( leaderPos - s->player()->pos() ).len();
        outScore = static_cast<float>( cluster.playerIds.size() ) * 1000.f - distance;
        return true;
    }
    return false;
}

GameSession* IsysMidBossTactic::resolveBossPersonalTarget( Room& room, uint32 targetId ) const {
    if ( targetId == 0 ) {
        return nullptr;
    }
    return room.findLivingSessionByPlayerId( static_cast<int32>( targetId ) );
}

// 보스 이동은 물리 motor(setDesiredVel)에 위임 — 시뮬의 setPosition 직접 적분 대체(dt 불필요).
void MU_CALLCONV IsysMidBossTactic::moveBossToward(
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

/*-------------------------------
   IsysMidBossTactic — 협공 사이클
-------------------------------*/

static std::mt19937& isisRng() {
    static thread_local std::mt19937 rng{ std::random_device{}() };
    return rng;
}

Seconds IsysMidBossTactic::rollCooldown() {
    std::uniform_real_distribution<float> dist( MIN_COOLDOWN, MAX_COOLDOWN );
    return Seconds{ dist( isisRng() ) };
}

bool IsysMidBossTactic::hasLiveBomberSquad( const PlatoonLeader& leader ) const {
    const auto& squads = leader.getSquads();
    for ( size_t i = 2; i < 4 && i < squads.size(); ++i ) {
        if ( squads[ i ] && !squads[ i ]->isEmpty() ) {
            return true;
        }
    }
    return false;
}

bool IsysMidBossTactic::hasLiveBuddySquad( const PlatoonLeader& leader ) const {
    const auto& squads = leader.getSquads();
    for ( size_t i = 0; i < 2 && i < squads.size(); ++i ) {
        if ( squads[ i ] && !squads[ i ]->isEmpty() ) {
            return true;
        }
    }
    return false;
}

bool IsysMidBossTactic::allLiveSquadsAtSlots( const PlatoonLeader& leader ) const {
    bool any = false;
    for ( TacticalSquad* sq : leader.getSquads() ) {
        if ( !sq || sq->isEmpty() ) {
            continue;
        }
        any = true;
        if ( !sq->areMembersAtSlots() ) {
            return false;
        }
    }
    return any;
}

bool IsysMidBossTactic::activeStrikeSquadsAtSlots() const {
    if ( activeStrikeSquads_.empty() ) {
        return false;
    }
    for ( TacticalSquad* sq : activeStrikeSquads_ ) {
        if ( sq && !sq->isEmpty() && !sq->areMembersAtSlots() ) {
            return false;
        }
    }
    return true;
}

bool IsysMidBossTactic::activeStrikeTasksEngaged() const {
    if ( activeStrikeTasks_.empty() ) {
        return false;
    }
    for ( const StrikeTask& task : activeStrikeTasks_ ) {
        if ( !task.engageIssued ) {
            return false;
        }
    }
    return true;
}

// 표적 군집 = buildPlayerClusters 위에 점수(인원 ×1000 − 보스거리) 부여. 2차 쐐기(applyRepeatPenalty)는
// 1차 타깃과 겹치는 군집에 페널티를 줘 다른 군집으로 분산시킨다. 상위 2개 군집 반환.
std::vector<IsysMidBossTactic::StrikeCluster>
IsysMidBossTactic::selectStrikeClusters( Room& room, const PlatoonLeader& leader, bool applyRepeatPenalty ) const {
    std::vector<PlayerCluster> baseClusters = MidBossTacticBase::buildPlayerClusters( room, CLUSTER_RADIUS );
    std::vector<StrikeCluster> result;
    result.reserve( baseClusters.size() );

    mu::Vec3 leaderPos = leader.pos();
    for ( PlayerCluster& cluster : baseClusters ) {
        if ( cluster.playerIds.empty() ) {
            continue;
        }
        if ( cluster.representativeId == 0 ) {
            cluster.representativeId = cluster.playerIds.front();
        }

        float distance = ( cluster.centroid - leaderPos ).len();
        bool overlapsFirstStrike = false;
        if ( applyRepeatPenalty ) {
            for ( uint32 playerId : cluster.playerIds ) {
                if ( std::find( firstStrikeTargetIds_.begin(), firstStrikeTargetIds_.end(), playerId )
                     != firstStrikeTargetIds_.end() ) {
                    overlapsFirstStrike = true;
                    break;
                }
            }
        }

        float score = static_cast<float>( cluster.playerIds.size() ) * 1000.f - distance;
        if ( overlapsFirstStrike ) {
            score -= SECOND_STRIKE_REPEAT_PENALTY;
        }
        cluster.score = score;
        result.push_back( { cluster, score } );
    }

    std::sort( result.begin(), result.end(),
        []( const StrikeCluster& a, const StrikeCluster& b ) {
            if ( a.cluster.playerIds.size() != b.cluster.playerIds.size() ) {
                return a.cluster.playerIds.size() > b.cluster.playerIds.size();
            }
            if ( std::fabs( a.score - b.score ) > 0.001f ) {
                return a.score > b.score;
            }
            return a.cluster.representativeId < b.cluster.representativeId;
        } );

    if ( result.size() > 2 ) {
        result.resize( 2 );
    }
    return result;
}

// 가장 크고 가까운 군집의 대표 플레이어(또는 최근접). 후퇴/대형의 facing 기준.
GameSession* IsysMidBossTactic::selectPrimaryTarget( Room& room, const PlatoonLeader& leader ) const {
    std::vector<StrikeCluster> clusters = selectStrikeClusters( room, leader, false );
    if ( !clusters.empty() ) {
        GameSession* s = room.findLivingSessionByPlayerId(
            static_cast<int32>( clusters.front().cluster.representativeId ) );
        if ( s ) {
            return s;
        }
    }
    return selectNearestPlayer( room, leader.pos() );
}

void IsysMidBossTactic::issueRetreatForPincer( Room& room, PlatoonLeader& leader ) {
    GameSession* primary = selectPrimaryTarget( room, leader );
    if ( !primary ) {
        issueEngage( room, leader, true );
        return;
    }

    mu::Vec3 playerCentroid = calcPlayerCentroid( room, leader.pos() );
    mu::Vec3 awayDir = leader.pos() - playerCentroid;
    float awayLen = awayDir.len();
    awayDir = ( awayLen > 0.01f ) ? awayDir * ( 1.f / awayLen ) : mu::Vec3( -1.f, 0.f, 0.f );

    float currentBossDist = ( leader.pos() - playerCentroid ).len();
    float retreatDist = std::max( currentBossDist + ISIS_RETREAT_EXTRA_DIST, ISIS_RETREAT_MIN_DIST );
    retreatTargetPos_ = playerCentroid + awayDir * retreatDist;
    retreatTargetPos_ = findSafeBossFormationPosition( room, leader, retreatTargetPos_ );

    mu::Vec3 forward = playerCentroid - retreatTargetPos_;
    float forwardLen = forward.len();
    forward = ( forwardLen > 0.01f ) ? forward * ( 1.f / forwardLen ) : mu::Vec3( 1.f, 0.f, 0.f );
    mu::Vec3 right( -forward.z(), 0.f, forward.x() );
    right = ( right.len2() > 0.01f ) ? norm3( right ) : mu::Vec3( 0.f, 0.f, 1.f );

    uint32 primaryId = static_cast<uint32>( primary->id() );
    const auto& squads = leader.getSquads();
    auto issueRetreatHold = [&]( size_t squadIndex, mu::Vec3 center,
                                 float spacingScale, float columnScale, int32 columnCount ) {
        if ( squadIndex >= squads.size() ) {
            return;
        }
        TacticalSquad* squad = squads[ squadIndex ];
        if ( !squad || squad->isEmpty() ) {
            return;
        }
        SquadOrder ord;
        ord.type = SquadOrderType::FormationHold;
        ord.targetId = primaryId;
        ord.tacticCenter = center;
        ord.formationTargetPos = playerCentroid;
        ord.slotSpacingScale = spacingScale;
        ord.slotColumnScale = columnScale;
        ord.slotColumnCount = columnCount;
        ord.speedMult = RETREAT_SPEED_MULT;
        ord.avoidStaticObstacles = true;
        squad->receiveOrder( ord );
    };

    // Buddy(0,1)는 후방, Bomber(2,3)는 전방 슬롯에 집결.
    issueRetreatHold( 0,
        retreatTargetPos_ - forward * RETREAT_BUDDY_BACK_OFFSET - right * RETREAT_BUDDY_SIDE_OFFSET,
        BUDDY_COLUMN_SPACING_SCALE, BUDDY_COLUMN_SCALE, BUDDY_COLUMN_COUNT );
    issueRetreatHold( 1,
        retreatTargetPos_ - forward * RETREAT_BUDDY_BACK_OFFSET + right * RETREAT_BUDDY_SIDE_OFFSET,
        BUDDY_COLUMN_SPACING_SCALE, BUDDY_COLUMN_SCALE, BUDDY_COLUMN_COUNT );
    issueRetreatHold( 2,
        retreatTargetPos_ + forward * RETREAT_BOMBER_FRONT_OFFSET - right * RETREAT_BOMBER_SIDE_OFFSET,
        BOMBER_REGROUP_SPACING_SCALE, BOMBER_REGROUP_COLUMN_SCALE, BOMBER_REGROUP_COLUMN_COUNT );
    issueRetreatHold( 3,
        retreatTargetPos_ + forward * RETREAT_BOMBER_FRONT_OFFSET + right * RETREAT_BOMBER_SIDE_OFFSET,
        BOMBER_REGROUP_SPACING_SCALE, BOMBER_REGROUP_COLUMN_SCALE, BOMBER_REGROUP_COLUMN_COUNT );
}

void IsysMidBossTactic::issueRegroupBombers( Room& room, PlatoonLeader& leader ) {
    std::vector<StrikeCluster> clusters = selectStrikeClusters( room, leader );
    if ( clusters.empty() ) {
        issueEngage( room, leader, true );
        return;
    }

    const auto& squads = leader.getSquads();
    TacticalSquad* bomberSquads[ 2 ] = {
        squads.size() > 2 ? squads[ 2 ] : nullptr,
        squads.size() > 3 ? squads[ 3 ] : nullptr
    };

    activeStrikeSquads_.clear();
    int32 assignedBomber = 0;
    for ( int i = 0; i < 2; ++i ) {
        TacticalSquad* bomber = bomberSquads[ i ];
        if ( !bomber || bomber->isEmpty() ) {
            continue;
        }
        int32 clusterIdx = ( clusters.size() == 1 ) ? 0 : std::min( assignedBomber, 1 );
        issueBomberRegroup( room, bomber, clusters[ clusterIdx ], assignedBomber == 0 ? -1.f : 1.f );
        activeStrikeSquads_.push_back( bomber );
        ++assignedBomber;
    }
}

void IsysMidBossTactic::issueRegroupBuddies( Room& room, PlatoonLeader& leader ) {
    if ( secondStrikePrepIssued_ ) {
        return;
    }
    std::vector<StrikeCluster> clusters = selectStrikeClusters( room, leader, true );
    if ( clusters.empty() ) {
        issueEngage( room, leader, true );
        return;
    }

    secondStrikePrepIssued_ = true;
    secondStrikePrepSquads_.clear();
    secondStrikeClusters_ = clusters;

    const auto& squads = leader.getSquads();
    TacticalSquad* buddySquads[ 2 ] = {
        squads.size() > 0 ? squads[ 0 ] : nullptr,
        squads.size() > 1 ? squads[ 1 ] : nullptr
    };

    for ( int i = 0; i < 2; ++i ) {
        if ( !buddySquads[ i ] || buddySquads[ i ]->isEmpty() ) {
            continue;
        }
        int32 clusterIdx = ( clusters.size() == 1 ) ? 0 : i;
        issueBuddyColumn( room, buddySquads[ i ], clusters[ clusterIdx ], i == 0 ? -1.f : 1.f );
        secondStrikePrepSquads_.push_back( buddySquads[ i ] );
    }
}

// Isys 쐐기는 Goblin과 동일한 공용 WedgeCharge 실행 경로 재사용: TacticalSquad가 쐐기 준비/돌진,
// TacticalNpc가 ChargeThrough로 공용 impact/damage 규칙 적용. 보스 합류 스쿼드는 apex 예약 + 피해 ×1.5.
void IsysMidBossTactic::issueWedgeStrike( Room& room, PlatoonLeader& leader,
                                          bool useBuddySquads, bool applyRepeatPenalty, bool rememberTargets ) {
    std::vector<StrikeCluster> clusters =
        ( useBuddySquads && !secondStrikeClusters_.empty() )
            ? secondStrikeClusters_
            : selectStrikeClusters( room, leader, applyRepeatPenalty );
    if ( clusters.empty() ) {
        if ( useBuddySquads ) {
            resetBossBuddyWedgeJoin();
        }
        issueEngage( room, leader, true );
        return;
    }

    const auto& squads = leader.getSquads();
    TacticalSquad* strikeSquads[ 2 ] = {
        useBuddySquads ? ( squads.size() > 0 ? squads[ 0 ] : nullptr )
                       : ( squads.size() > 2 ? squads[ 2 ] : nullptr ),
        useBuddySquads ? ( squads.size() > 1 ? squads[ 1 ] : nullptr )
                       : ( squads.size() > 3 ? squads[ 3 ] : nullptr )
    };

    activeStrikeSquads_.clear();
    activeStrikeTasks_.clear();
    if ( rememberTargets ) {
        firstStrikeTargetIds_.clear();
    }
    if ( useBuddySquads ) {
        bossBuddyWedgeChargeStarted_ = false;
        bossBuddyWedgeChargeComplete_ = false;
    }

    int32 assignedSquad = 0;
    bool bossJoinedStrikeIssued = false;
    for ( int i = 0; i < 2; ++i ) {
        TacticalSquad* squad = strikeSquads[ i ];
        if ( !squad || squad->isEmpty() ) {
            continue;
        }
        int32 clusterIdx = ( clusters.size() > 1 ) ? std::min( assignedSquad, 1 ) : 0;
        const StrikeCluster& strikeCluster = clusters[ clusterIdx ];

        SquadOrder ord;
        ord.type = SquadOrderType::WedgeCharge;
        ord.targetId = strikeCluster.cluster.representativeId;
        ord.targetIds = strikeCluster.cluster.playerIds;
        ord.tacticCenter = strikeCluster.cluster.centroid;
        ord.chargeSpeedMult = ISIS_WEDGE_SPEED_MULT;
        ord.chargeAcceleration = ISIS_WEDGE_MOTOR_ACCELERATION;
        ord.avoidStaticObstacles = true;
        if ( useBuddySquads ) {
            ord.wedgeSpacingMult = ISIS_BUDDY_WEDGE_SPACING_MULT;
            if ( isBossJoinedBuddySquad( squad ) ) {
                ord.wedgeDamageMult = ISIS_BOSS_JOINED_WEDGE_DAMAGE_MULT;   // 보스 합류 결정타 ×1.5
                ord.reserveWedgeApex = true;                                // 보스용 apex 슬롯 비움
                setupBossBuddyWedgeJoin( room, leader, squad, strikeCluster, squad->calcCentroid() );
                bossBuddyWedgeChargeComplete_ = false;
                bossJoinedStrikeIssued = true;
            }
        }
        squad->receiveOrder( ord );
        activeStrikeSquads_.push_back( squad );
        activeStrikeTasks_.push_back( StrikeTask{ squad, strikeCluster.cluster.playerIds, false } );

        if ( rememberTargets ) {
            for ( uint32 playerId : strikeCluster.cluster.playerIds ) {
                if ( std::find( firstStrikeTargetIds_.begin(), firstStrikeTargetIds_.end(), playerId )
                     == firstStrikeTargetIds_.end() ) {
                    firstStrikeTargetIds_.push_back( playerId );
                }
            }
        }
        ++assignedSquad;
    }

    if ( useBuddySquads && bossBuddyWedgeJoinActive_ && !bossJoinedStrikeIssued ) {
        resetBossBuddyWedgeJoin();
    }
}

void IsysMidBossTactic::issueBomberRegroup( Room& room, TacticalSquad* squad,
                                            const StrikeCluster& strikeCluster, float sideSign ) {
    if ( !squad || squad->isEmpty() ) {
        return;
    }

    mu::Vec3 fallbackDir = strikeCluster.cluster.centroid - squad->calcCentroid();
    if ( fallbackDir.len2() <= 0.01f ) {
        fallbackDir = mu::Vec3( 1.f, 0.f, 0.f );
    }

    mu::Vec3 playerFacing = calcAveragePlayerFacing( room, fallbackDir );
    mu::Vec3 forward = ( playerFacing.len2() > 0.01f ) ? norm3( playerFacing ) : norm3( fallbackDir );
    mu::Vec3 right( -forward.z(), 0.f, forward.x() );
    right = ( right.len2() <= 0.01f ) ? mu::Vec3( 0.f, 0.f, 1.f ) : norm3( right );

    mu::Vec3 center = strikeCluster.cluster.centroid
        - forward * BOMBER_REGROUP_BACK_OFFSET
        + right * ( BOMBER_REGROUP_SIDE_OFFSET * sideSign );

    SquadOrder ord;
    ord.type = SquadOrderType::FormationHold;
    ord.targetId = strikeCluster.cluster.representativeId;
    ord.tacticCenter = center;
    ord.formationTargetPos = strikeCluster.cluster.centroid;
    ord.slotSpacingScale = BOMBER_REGROUP_SPACING_SCALE;
    ord.slotColumnScale = BOMBER_REGROUP_COLUMN_SCALE;
    ord.slotColumnCount = BOMBER_REGROUP_COLUMN_COUNT;
    ord.speedMult = BOMBER_REGROUP_SPEED_MULT;
    ord.avoidStaticObstacles = true;
    squad->receiveOrder( ord );
}

void MU_CALLCONV IsysMidBossTactic::issueBuddyColumn( Room& /*room*/, TacticalSquad* squad,
                                                      const StrikeCluster& strikeCluster, float sideSign ) {
    if ( !squad || squad->isEmpty() ) {
        return;
    }

    mu::Vec3 attackDir = strikeCluster.cluster.centroid - retreatTargetPos_;
    attackDir = ( attackDir.len2() > 0.01f ) ? norm3( attackDir ) : mu::Vec3( 1.f, 0.f, 0.f );
    mu::Vec3 right( -attackDir.z(), 0.f, attackDir.x() );
    right = ( right.len2() <= 0.01f ) ? mu::Vec3( 0.f, 0.f, 1.f ) : norm3( right );

    mu::Vec3 center = retreatTargetPos_
        + attackDir * RETREAT_BOMBER_FRONT_OFFSET
        + right * ( RETREAT_BUDDY_SIDE_OFFSET * sideSign );

    SquadOrder ord;
    ord.type = SquadOrderType::FormationHold;
    ord.targetId = strikeCluster.cluster.representativeId;
    ord.tacticCenter = center;
    ord.formationTargetPos = strikeCluster.cluster.centroid;
    ord.slotSpacingScale = BUDDY_COLUMN_SPACING_SCALE;
    ord.slotColumnScale = BUDDY_COLUMN_SCALE;
    ord.slotColumnCount = BUDDY_COLUMN_COUNT;
    ord.speedMult = BUDDY_SPEED_MULT;
    ord.avoidStaticObstacles = true;
    squad->receiveOrder( ord );
}

uint32 IsysMidBossTactic::selectStrikeEngageTarget( Room& room, const PlatoonLeader& leader, const StrikeTask& task ) const {
    mu::Vec3 from = task.squad ? task.squad->calcCentroid() : leader.pos();
    uint32 bestTargetId = 0;
    float bestDistSq = -1.f;
    for ( uint32 targetId : task.targetIds ) {
        GameSession* s = room.findLivingSessionByPlayerId( static_cast<int32>( targetId ) );
        if ( !s ) {
            continue;
        }
        float distSq = ( from - s->player()->pos() ).len2();
        if ( bestDistSq < 0.f || distSq < bestDistSq ) {
            bestDistSq = distSq;
            bestTargetId = targetId;
        }
    }
    if ( bestTargetId != 0 ) {
        return bestTargetId;
    }
    GameSession* fallback = selectPrimaryTarget( room, leader );
    return fallback ? static_cast<uint32>( fallback->id() ) : 0;
}

// 쐐기 관통이 끝난(또는 강제) 스쿼드를 일반 Engage로 전환. charge 종료 + 군집 내 최근접 타깃 교전.
void IsysMidBossTactic::updateActiveStrikeEngage( Room& room, PlatoonLeader& leader, bool forceAll ) {
    for ( StrikeTask& task : activeStrikeTasks_ ) {
        if ( task.engageIssued ) {
            continue;
        }
        if ( !task.squad || task.squad->isEmpty() ) {
            task.engageIssued = true;
            continue;
        }
        if ( !forceAll && !task.squad->areChargeMembersComplete() ) {
            continue;
        }

        task.squad->endActiveWedgeCharge( room );
        uint32 targetId = selectStrikeEngageTarget( room, leader, task );
        if ( targetId != 0 ) {
            SquadOrder ord;
            ord.type = SquadOrderType::Engage;
            ord.targetId = targetId;
            task.squad->receiveOrder( ord );
        }
        task.engageIssued = true;
    }
}

void IsysMidBossTactic::forceActiveWedgeCharges() {
    for ( TacticalSquad* squad : activeStrikeSquads_ ) {
        if ( squad && !squad->isEmpty() ) {
            squad->forceStartWedgeCharge();
        }
    }
}

/*-------------------------------
   IsysMidBossTactic — 보스 2차 쐐기 합류
-------------------------------*/

void IsysMidBossTactic::selectBossJoinedBuddySquad( const PlatoonLeader& leader ) {
    resetBossBuddyWedgeJoin();

    const auto& squads = leader.getSquads();
    std::vector<int32> candidates;
    for ( int i = 0; i < 2; ++i ) {
        if ( static_cast<size_t>( i ) >= squads.size() ) {
            continue;
        }
        TacticalSquad* squad = squads[ static_cast<size_t>( i ) ];
        if ( squad && !squad->isEmpty() ) {
            candidates.push_back( i );
        }
    }
    if ( candidates.empty() ) {
        return;
    }

    std::uniform_int_distribution<int> dist( 0, static_cast<int>( candidates.size() ) - 1 );
    bossJoinedBuddySquadIndex_ = candidates[ dist( isisRng() ) ];
}

void IsysMidBossTactic::resetBossBuddyWedgeJoin() {
    bossJoinedBuddySquadIndex_ = -1;
    bossBuddyWedgeJoinActive_ = false;
    bossBuddyWedgeChargeStarted_ = false;
    bossBuddyWedgeChargeComplete_ = false;
    bossBuddyWedgePreparePos_ = mu::Vec3{};
    bossBuddyWedgeExitPos_ = mu::Vec3{};
    bossBuddyWedgeDir_ = mu::Vec3( 1.f, 0.f, 0.f );
}

bool IsysMidBossTactic::isBossJoinedBuddySquad( const TacticalSquad* squad ) const {
    return squad && bossJoinedBuddySquadIndex_ >= 0 &&
        squad->getSquadId() == bossJoinedBuddySquadIndex_;
}

bool IsysMidBossTactic::ensureBossBuddyWedgeJoin( Room& room, PlatoonLeader& leader ) {
    const auto& squads = leader.getSquads();
    auto selectedSquadAlive = [&]() {
        for ( int i = 0; i < 2; ++i ) {
            if ( static_cast<size_t>( i ) >= squads.size() ) {
                continue;
            }
            TacticalSquad* squad = squads[ static_cast<size_t>( i ) ];
            if ( squad && !squad->isEmpty() && isBossJoinedBuddySquad( squad ) ) {
                return true;
            }
        }
        return false;
    };

    if ( bossBuddyWedgeJoinActive_ ) {
        if ( selectedSquadAlive() ) {
            return true;
        }
        resetBossBuddyWedgeJoin();
    }

    if ( secondStrikeClusters_.empty() ) {
        return false;
    }

    if ( bossJoinedBuddySquadIndex_ < 0 ) {
        selectBossJoinedBuddySquad( leader );
    }
    if ( bossJoinedBuddySquadIndex_ < 0 ) {
        return false;
    }
    if ( !selectedSquadAlive() ) {
        selectBossJoinedBuddySquad( leader );
        if ( bossJoinedBuddySquadIndex_ < 0 ) {
            return false;
        }
    }

    for ( int i = 0; i < 2; ++i ) {
        if ( static_cast<size_t>( i ) >= squads.size() ) {
            continue;
        }
        TacticalSquad* squad = squads[ static_cast<size_t>( i ) ];
        if ( !squad || squad->isEmpty() || !isBossJoinedBuddySquad( squad ) ) {
            continue;
        }
        if ( !squad->areMembersAtSlots() ) {
            return true;   // 아직 집결 중 — prepare pos는 집결 완료 후 산출
        }
        int32 clusterIdx = ( secondStrikeClusters_.size() > 1 ) ? std::min( i, 1 ) : 0;
        setupBossBuddyWedgeJoin( room, leader, squad,
            secondStrikeClusters_[ static_cast<size_t>( clusterIdx ) ], squad->calcCentroid() );
        return bossBuddyWedgeJoinActive_;
    }

    resetBossBuddyWedgeJoin();
    return false;
}

bool IsysMidBossTactic::isBossBuddyWedgeJoinReady( const PlatoonLeader& leader ) const {
    if ( !bossBuddyWedgeJoinActive_ ) {
        return bossJoinedBuddySquadIndex_ < 0;   // 합류 대상 없음 → 보스 대기 불필요
    }
    if ( bossBuddyWedgeChargeStarted_ ) {
        return true;
    }
    return ( leader.pos() - bossBuddyWedgePreparePos_ ).len() <= ISIS_BOSS_WEDGE_JOIN_READY_DIST;
}

bool IsysMidBossTactic::areSecondStrikePrepSquadsAtSlots() const {
    if ( secondStrikePrepSquads_.empty() ) {
        return false;
    }
    for ( TacticalSquad* squad : secondStrikePrepSquads_ ) {
        if ( squad && !squad->isEmpty() && !squad->areMembersAtSlots() ) {
            return false;
        }
    }
    return true;
}

bool IsysMidBossTactic::isSecondStrikePrepReady( const PlatoonLeader& leader ) const {
    return secondStrikePrepIssued_ && areSecondStrikePrepSquadsAtSlots() && isBossBuddyWedgeJoinReady( leader );
}

void MU_CALLCONV IsysMidBossTactic::setupBossBuddyWedgeJoin( Room& room, PlatoonLeader& leader,
                                                             TacticalSquad* squad,
                                                             const StrikeCluster& strikeCluster,
                                                             mu::Vec3 squadCenter ) {
    if ( !isBossJoinedBuddySquad( squad ) ) {
        return;
    }
    mu::Vec3 forward = strikeCluster.cluster.centroid - squadCenter;
    forward = ( forward.len2() > 0.01f ) ? norm3( forward ) : mu::Vec3( 1.f, 0.f, 0.f );

    bossBuddyWedgeJoinActive_ = true;
    bossBuddyWedgeDir_ = forward;
    bossBuddyWedgePreparePos_ = findSafeBossFormationPosition(
        room, leader, squadCenter + forward * TacticalSquad::WEDGE_PREP_APEX_DISTANCE );
    bossBuddyWedgeExitPos_ = findSafeBossFormationPosition(
        room, leader, strikeCluster.cluster.centroid + forward * TacticalSquad::WEDGE_EXIT_DISTANCE );
}

mu::Vec3 MU_CALLCONV IsysMidBossTactic::findSafeBossFormationPosition(
    Room& room, const PlatoonLeader& leader, mu::Vec3 desired ) const {
    constexpr float SEARCH_STEP = 1.5f;
    constexpr int32 SEARCH_RINGS = 4;
    constexpr int32 SAMPLES_PER_RING = 12;
    constexpr float TWO_PI = 2.f * 3.14159265f;

    auto groundSnap = [&]( mu::Vec3 p ) {
        return mu::Vec3( p.x(), room.groundHeightAtWorld( p.x(), p.z() ), p.z() );
    };

    desired = groundSnap( desired );
    if ( room.isTacticalFormationPositionOpen( desired, leader ) ) {
        return desired;
    }

    float angleOffset = static_cast<float>(leader.getId() % SAMPLES_PER_RING) * (TWO_PI / SAMPLES_PER_RING);
    for ( int32 ring = 1; ring <= SEARCH_RINGS; ++ring ) {
        float radius = SEARCH_STEP * static_cast<float>( ring );
        for ( int32 sample = 0; sample < SAMPLES_PER_RING; ++sample ) {
            float angle = angleOffset + TWO_PI * static_cast<float>( sample ) / static_cast<float>( SAMPLES_PER_RING );
            mu::Vec3 candidate = groundSnap( desired + mu::Vec3(
                std::cosf( angle ) * radius, 0.f, std::sinf( angle ) * radius ) );
            if ( room.isTacticalFormationPositionOpen( candidate, leader ) ) {
                return candidate;
            }
        }
    }

    return groundSnap( leader.pos() );
}

void IsysMidBossTactic::syncBossBuddyWedgeChargeStart( const PlatoonLeader& leader ) {
    if ( !bossBuddyWedgeJoinActive_ || bossBuddyWedgeChargeStarted_ || bossJoinedBuddySquadIndex_ < 0 ) {
        return;
    }
    const auto& squads = leader.getSquads();
    for ( int i = 0; i < 2; ++i ) {
        if ( static_cast<size_t>( i ) >= squads.size() ) {
            continue;
        }
        TacticalSquad* squad = squads[ static_cast<size_t>( i ) ];
        if ( !squad || !isBossJoinedBuddySquad( squad ) ) {
            continue;
        }
        if ( squad->isWedgeChargeActive() ) {
            bossBuddyWedgeChargeStarted_ = true;
        }
        return;
    }
}

// 보스를 prepare apex(합류) → exit(돌진)로 모터 이동. 도착 시 정지 + 돌진 완료 플래그.
void IsysMidBossTactic::updateBossBuddyWedgeJoin( Seconds /*dt*/, PlatoonLeader& leader ) {
    if ( !bossBuddyWedgeJoinActive_ ) {
        return;
    }
    mu::Vec3 targetPos = bossBuddyWedgeChargeStarted_ ? bossBuddyWedgeExitPos_ : bossBuddyWedgePreparePos_;
    float arriveDist = bossBuddyWedgeChargeStarted_ ? ISIS_BOSS_WEDGE_CHARGE_ARRIVE_DIST : ISIS_BOSS_WEDGE_PREP_ARRIVE_DIST;

    float dist = ( targetPos - leader.pos() ).len();
    if ( dist <= arriveDist ) {
        if ( bossBuddyWedgeChargeStarted_ ) {
            bossBuddyWedgeChargeComplete_ = true;
            restoreBossWedgeMotorBoost( leader );
        }
        if ( bossBuddyWedgeDir_.len2() > 0.01f ) {
            leader.setFacing( bossBuddyWedgeDir_ );
        }
        leader.setDesiredVel( mu::Vec3{} );
        return;
    }

    float speedMult = bossBuddyWedgeChargeStarted_ ? ISIS_BOSS_WEDGE_CHARGE_SPEED_MULT : ISIS_BOSS_WEDGE_JOIN_SPEED_MULT;
    if ( bossBuddyWedgeChargeStarted_ ) {
        applyBossWedgeMotorBoost( leader );
    }
    moveBossToward( leader, targetPos, speedMult );
}

void IsysMidBossTactic::applyBossWedgeMotorBoost( PlatoonLeader& leader ) {
    if ( bossWedgeMotorBoostActive_ ) {
        return;
    }
    savedBossWedgeMotorAcceleration_ = leader.body().motor().maxAcceleration;
    leader.body().setMotorMaxAcceleration( ISIS_WEDGE_MOTOR_ACCELERATION );
    bossWedgeMotorBoostActive_ = true;
}

void IsysMidBossTactic::restoreBossWedgeMotorBoost( PlatoonLeader& leader ) {
    if ( !bossWedgeMotorBoostActive_ ) {
        return;
    }
    leader.body().setMotorMaxAcceleration( savedBossWedgeMotorAcceleration_ );
    bossWedgeMotorBoostActive_ = false;
}

#include "rspch.hpp"
#include "MidBossTactics.hpp"
#include "PlatoonLeader.hpp"
#include "Room.hpp"
#include "GameSession.hpp"
#include "TacticalSquad.hpp"
#include "TacticalNpc.hpp"
#include "object.hpp"
#include "PacketManager.hpp"
#include <algorithm>
#include <cmath>

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

/*---------------------------
      MidBossTacticBase
---------------------------*/

void MidBossTacticBase::onLeaderDead( Room& room, PlatoonLeader& leader ) {
    leader.pushConfusedToSquads( room );
}

std::vector<TacticalSquad*> MidBossTacticBase::collectLiveSquads( PlatoonLeader& leader ) const {
    std::vector<TacticalSquad*> live;

    for ( TacticalSquad* sq : leader.getSquads() ) {
        if ( sq && !sq->isEmpty() ) {
            live.push_back( sq );
        }
    }

    return live;
}

std::vector<MidBossTacticBase::PlayerCluster> MidBossTacticBase::buildPlayerClusters( const Room& room, float clusterRadius ) const {
    const auto& players = room.getLivingPlayers();
    int32 count = static_cast<int32>(players.size());

    std::vector<PlayerCluster> result;

    if ( count <= 0 ) {
        return result;
    }

    float clusterRadiusSq = clusterRadius * clusterRadius;
    std::vector<bool> visited( count, false );

    for ( int32 i = 0; i < count; ++i ) {
        if ( visited[ i ] ) {
            continue;
        }

        PlayerCluster cluster;
        std::vector<int32> stack;
        stack.push_back( i );
        visited[ i ] = true;

        while ( !stack.empty() ) {
            int32 cur = stack.back();
            stack.pop_back();

            GameSession* s = players[ cur ];
            mu::Vec3 curPos = s->player()->pos();
            cluster.centroid = cluster.centroid + curPos;
            uint32 pid = static_cast<uint32>(s->id());
            cluster.playerIds.push_back( pid );

            if ( cluster.representativeId == 0 ) {
                cluster.representativeId = pid;
            }

            for ( int32 j = 0; j < count; ++j ) {
                if ( visited[ j ] ) {
                    continue;
                }

                mu::Vec3 otherPos = players[ j ]->player()->pos();
                if ( (curPos - otherPos).len2() <= clusterRadiusSq ) {
                    visited[ j ] = true;
                    stack.push_back( j );
                }
            }
        }

        if ( !cluster.playerIds.empty() ) {
            cluster.centroid = cluster.centroid * (1.f / static_cast<float>(cluster.playerIds.size()));
            result.push_back( cluster );
        }
    }

    return result;
}

mu::Vec3 MidBossTacticBase::calcPlayerCentroid( const Room& room, mu::Vec3 fallback ) const {
    const auto& players = room.getLivingPlayers();

    if ( players.empty() ) {
        return fallback;
    }

    mu::Vec3 sum{};
    for ( GameSession* s : players ) {
        sum = sum + s->player()->pos();
    }

    return sum * (1.f / static_cast<float>(players.size()));
}

mu::Vec3 MidBossTacticBase::calcAveragePlayerFacing( const Room& room, mu::Vec3 fallbackDir ) const {
    mu::Vec3 sum{};
    for ( GameSession* s : room.getLivingPlayers() ) {
        sum = sum + s->player()->forward();
    }

    if ( sum.len2() > 0.01f ) {
        return norm3( sum );
    }

    if ( fallbackDir.len2() > 0.01f ) {
        return norm3( fallbackDir );
    }

    return mu::Vec3{};
}

GameSession* MidBossTacticBase::selectNearestPlayer( Room& room, mu::Vec3 from ) const {
    GameSession* best = nullptr;
    float bestDistSq = -1.f;

    for ( GameSession* s : room.getLivingPlayers() ) {
        float dSq = ( from - s->player()->pos() ).len2();

        if ( bestDistSq < 0.f || dSq < bestDistSq ) {
            bestDistSq = dSq;
            best = s;
        }
    }

    return best;
}

uint32 MidBossTacticBase::selectNearestPlayerId( Room& room, mu::Vec3 from ) const {
    GameSession* s = selectNearestPlayer( room, from );
    return s ? static_cast<uint32>(s->id()) : 0;
}

void MidBossTacticBase::issueEngageAll( PlatoonLeader& leader, uint32 targetId ) const {
    if ( targetId == 0 ) {
        return;
    }

    for ( TacticalSquad* sq : leader.getSquads() ) {
        if ( !sq || sq->isEmpty() ) {
            continue;
        }

        auto ord = SquadOrder{
			.type = SquadOrderType::Engage,
			.targetId = targetId
        };
        sq->receiveOrder( ord );
    }
}

void MidBossTacticBase::issueIdleAll( PlatoonLeader& leader ) const {
    for ( TacticalSquad* sq : leader.getSquads() ) {
        if ( !sq || sq->isEmpty() ) {
            continue;
        }

        auto ord = SquadOrder{
            .type = SquadOrderType::Idle
        };
        sq->receiveOrder( ord );
    }
}

/*-----------------------------
      GoblinMidBossTactic
-----------------------------*/

void GoblinMidBossTactic::update(Seconds dt, Room& room, PlatoonLeader& leader) {
    const auto& squads = leader.getSquads();

    phaseElapsed_ += dt;   // 현재 단계 누적 시간(타임아웃 폴백용)

    if ( !initialSizesSet_ ) {
        initialSizesSet_ = true;
        for ( TacticalSquad* sq : squads ) {
            initialSquadSizes_.push_back( static_cast<int32>(sq->getMembers().size()) );
        }
    }

    leader.removeDeadMembersFromSquads();

    bool hasLiveSquad = false;
    for ( TacticalSquad* sq : squads ) {
        if ( sq && !sq->isEmpty() ) {
            hasLiveSquad = true;
            break;
        }
    }

    if ( !hasLiveSquad && leaderPhase_ != LeaderPhase::BossSolo ) {
        tacticCooldown_ = 0s;
        enterPhase( LeaderPhase::BossSolo );
    }

    if ( leaderPhase_ == LeaderPhase::Cooldown ) {
        tacticCooldown_ -= dt;
        if ( tacticCooldown_ <= 0s ) {
            enterPhase( tacticsUnlocked_ ? LeaderPhase::TacticalRetreat : LeaderPhase::BoxAdvance );
        }
    }
    else if ( leaderPhase_ == LeaderPhase::Encircle ) {
        auto liveSquads = collectLiveSquads( leader );
        auto clusters = buildPlayerClusters( room, leader );

        if ( clusters.size() != 1 || !canStartEncircle( liveSquads, clusters.front() ) ) {
            enterTacticFailCooldown( room, leader );
        }
        else {
            int32 liveMembers = countLiveMembers( liveSquads );

            if ( phaseOrderIssued_ && liveMembers != encircleIssuedLiveMembers_ ) {
                phaseOrderIssued_ = false;
                tacticTimer_ = 0s;
            }
        }

        if ( phaseOrderIssued_ && formationReady( leader ) ) {
            // 포위 완성 → squad별 균형 재배정으로 일반 교전 전환(전면 재배정).
            issueStableEngage( room, collectLiveSquads( leader ), /*resetAssignments=*/true );

            tacticCooldown_ = TACTIC_COOLDOWN_DURATION;
            enterPhase( LeaderPhase::Cooldown );
        }
    }
    else if ( leaderPhase_ == LeaderPhase::DivideAndConquer ) {
        updateDivideAndConquer( dt, room, leader );
    }
    else {
        // no-op
    }

    GameSession* primary = selectPrimaryTarget(room, leader);

    if ( leaderPhase_ != LeaderPhase::BossSolo && !tacticsUnlocked_
        && primary && checkTacticsConditions( leader )
    ) {
        tacticsUnlocked_ = true;
        enterPhase( LeaderPhase::TacticalRetreat );
    }

    if ( leaderPhase_ == LeaderPhase::BoxAdvance && primary ) {
        if ( formationReady( leader ) ) {
            phaseHoldTimer_ += dt;   // 박스 대형 완성 → 잠시 유지(과시) 후 다음 단계
        }
        else {
            phaseHoldTimer_ = 0s;    // 아직 미집결 → 체류 타이머 리셋
        }

        if ( formationReady( leader ) && phaseHoldTimer_ >= FORMATION_HOLD_DURATION ) {
            if ( !tacticsUnlocked_ ) {
                enterPhase( LeaderPhase::Engage );
                // 박스 대형 완성 → squad별 균형 재배정으로 일반 교전 전환(전면 재배정).
                issueStableEngage( room, collectLiveSquads( leader ), /*resetAssignments=*/true );
            }
            else {
                auto clusters = buildPlayerClusters( room, leader );
                auto liveSquads = collectLiveSquads( leader );

                if ( clusters.size() == 1 && canStartEncircle( liveSquads, clusters.front() ) ) {
                    enterPhase( LeaderPhase::Encircle );
                }
                else if ( clusters.size() == 1 ) {
                    enterTacticFailCooldown( room, leader );
                }
                else {
                    // 박스에서 클러스터 ≥2 → 경계 단계 없이 곧장 쐐기. 쐐기 명령 발행 시
                    // 클러스터를 다시 검사해 ≤1이면 포위/실패로 폴백하므로 재판단은 보존된다.
                    enterPhase( LeaderPhase::DivideAndConquer );
                }
            }
        }
    }

    bool leaderAtRetreat = lenXZ( leader.pos() - retreatTargetPos_ ) <= 2.0f;
    if ( leaderPhase_ == LeaderPhase::TacticalRetreat && phaseOrderIssued_
        && formationReady( leader ) && leaderAtRetreat
    ) {
        phaseHoldTimer_ += dt;   // 후퇴 집결 완료 → 잠시 유지 후 박스 대형 전환
        if ( phaseHoldTimer_ >= FORMATION_HOLD_DURATION ) {
            enterPhase( LeaderPhase::BoxAdvance );
        }
    }
    else if ( leaderPhase_ == LeaderPhase::TacticalRetreat ) {
        phaseHoldTimer_ = 0s;    // 아직 집결 전 → 체류 타이머 리셋
    }

    tacticTimer_ -= dt;
    if ( tacticTimer_ <= 0s ) {
        tacticTimer_ = TACTIC_INTERVAL;
        evaluateTactics( room, leader );
    }

    if ( leaderPhase_ == LeaderPhase::TacticalRetreat ) {
        mu::Vec3 toRetreat = retreatTargetPos_ - leader.pos();
        float d = lenXZ( toRetreat );   // 수평 거리(보스는 지형에 붙으므로 Y 무시)

        constexpr float RETREAT_ARRIVE = 1.0f;
        constexpr float RETREAT_SLOW   = 5.0f;
        if ( d > RETREAT_ARRIVE ) {
            mu::Vec3 dir = mu::Vec3( toRetreat.x() / d, 0.f, toRetreat.z() / d );   // XZ 방향
            float spd = leader.getLeaderMoveSpeed() * TACTICAL_SPEED_MULT;
            if ( d < RETREAT_SLOW ) spd *= std::max( 0.2f, d / RETREAT_SLOW );   // 도착 감속(오버슈트 방지)
            leader.setDesiredVel( mu::Vec3( dir.x() * spd, 0.f, dir.z() * spd ) );
            leader.setFacing( dir );
        }
        else {
            leader.setDesiredVel( mu::Vec3{} );   // 도착 → 정지(leaderAtRetreat 안정화)
            // 집결 대기 중 등 돌리지 않게 플레이어 군집 중심을 바라봄 (XZ만 사용)
            mu::Vec3 toPlayers = calcPlayerCentroid( room, leader.pos() ) - leader.pos();
            leader.setFacing( mu::Vec3( toPlayers.x(), 0.f, toPlayers.z() ) );
        }
        return;
    }

    // 전술 발동 후 기동(박스 전진/포위/쐐기) 중에는 보스가 전투에 개입하지 않고
    // 후방에서 지휘만 한다. Cooldown 진입(전술 종료) 후 개인 전투 재개.
    const bool tacticManeuverActive = tacticsUnlocked_ &&
        ( leaderPhase_ == LeaderPhase::BoxAdvance ||
          leaderPhase_ == LeaderPhase::Encircle ||
          leaderPhase_ == LeaderPhase::DivideAndConquer );
    if ( tacticManeuverActive ) {
        leader.setDesiredVel( mu::Vec3{} );
        // 지휘 연출: 플레이어 군집 중심을 바라봄 (XZ만 사용)
        mu::Vec3 toPlayers = calcPlayerCentroid( room, leader.pos() ) - leader.pos();
        leader.setFacing( mu::Vec3( toPlayers.x(), 0.f, toPlayers.z() ) );
        if ( leader.getState() != TacticalNpcState::Idle ) {
            leader.setTacticalTarget( 0 );
            leader.transitionTacticalState( TacticalNpcState::Idle );
        }
        // 전투 재개 시 묵은 windup/recover 상태로 이어지지 않게 초기화
        bossPersonalState_ = BossPersonalState::EvaluateTarget;
        bossPersonalTargetId_ = 0;
        return;
    }

    updateBossPersonalCombat( dt, room, leader );
}

void GoblinMidBossTactic::enterPhase( LeaderPhase next ) {
    leaderPhase_ = next;
    phaseOrderIssued_ = false;
    phaseHoldTimer_ = 0s;
    phaseElapsed_ = 0s;   // 단계 누적 시간 리셋(타임아웃 폴백용)

    if ( next != LeaderPhase::Encircle ) {
        encircleIssuedLiveMembers_ = 0;
    }
    if ( next != LeaderPhase::DivideAndConquer ) {
        divideTasks_.clear();
    }
    if ( next != LeaderPhase::Cooldown ) {
        tacticTimer_ = 0s;
    }
    // 전술 대형/솔로 phase 진입 시 squad 교전 배정 캐시를 비워, 전술 종료 후 새 일반 교전이
    // 깨끗한 균형 재배정으로 시작되게 한다(GameServer에는 Vigilance 단계 없음).
    if ( next == LeaderPhase::BoxAdvance ||
         next == LeaderPhase::TacticalRetreat ||
         next == LeaderPhase::Encircle ||
         next == LeaderPhase::DivideAndConquer ||
         next == LeaderPhase::BossSolo ) {
        engageTargetBySquad_.clear();
    }
}

void GoblinMidBossTactic::enterTacticFailCooldown( Room& room, PlatoonLeader& leader ) {
    leader.removeDeadMembersFromSquads();

    clearDivideBarriers( room );   // 쐐기 전술이 차단벽을 켜뒀다면 해제(타 전술 경로에선 no-op)

    tacticCooldown_ = TACTIC_FAIL_COOLDOWN_DURATION;
    enterPhase( LeaderPhase::Cooldown );

    std::vector<TacticalSquad*> liveSquads;
    for ( TacticalSquad* sq : leader.getSquads() ) {
        if ( !sq->isEmpty() ) {
            liveSquads.push_back( sq );
        }
    }

    GameSession* primary = selectPrimaryTarget( room, leader );
    if ( !primary || liveSquads.empty() ) {
        return;
    }

    // 전술 실패/종료 시 전면 재배정(reset=true): squad별 플레이어 타깃을 균형 재분배.
    issueStableEngage( room, liveSquads, /*resetAssignments=*/true );
}

void GoblinMidBossTactic::evaluateTactics( Room& room, PlatoonLeader& leader ) {
    leader.removeDeadMembersFromSquads();

    std::vector<TacticalSquad*> liveSquads;
    for ( TacticalSquad* sq : leader.getSquads() ) {
        if ( !sq->isEmpty() ) {
            liveSquads.push_back( sq );
        }
    }

    if ( leaderPhase_ == LeaderPhase::BossSolo ) {
        return;
    }

    GameSession* primary = selectPrimaryTarget( room, leader );

    if ( !primary || liveSquads.empty() ) {
        for ( TacticalSquad* sq : liveSquads ) {
            auto ord = SquadOrder{
                .type = SquadOrderType::Idle
			};
            sq->receiveOrder( ord );
        }

        if ( leader.getState() != TacticalNpcState::Idle ) {
            leader.setTacticalTarget( 0 );
            leader.transitionTacticalState( TacticalNpcState::Idle );
        }
        return;
    }

    uint32 primaryId = static_cast<uint32>(primary->id());
    if ( leader.getTargetId() != primaryId ) {
        leader.setTacticalTarget( primaryId );

        if ( leader.getState() == TacticalNpcState::Idle ) {
            leader.transitionTacticalState( TacticalNpcState::Chase );
        }
    }

    int32 numSquads = static_cast<int32>(liveSquads.size());
    primaryTargetId_ = primaryId;

    if ( leaderPhase_ == LeaderPhase::TacticalRetreat ) {
        if ( phaseOrderIssued_ ) return;

        mu::Vec3 playerCent = calcPlayerCentroid( room, leader.pos() );
        mu::Vec3 awayDir = norm3( leader.pos() - playerCent );
        if ( awayDir.len2() < 0.001f ) {
            awayDir = mu::Vec3( -1.f, 0.f, 0.f );
        }

        retreatTargetPos_ = playerCent + awayDir * REGROUP_DIST;

        for ( int32 i = 0; i < numSquads; ++i ) {
            auto ord = SquadOrder{
                .type = SquadOrderType::RetreatFormUp,
                .targetId = primaryTargetId_,
                .leaderPos = leader.pos(),
                .formationTargetPos = playerCent,
                .tacticCenter = retreatTargetPos_
            };
            liveSquads[ i ]->receiveOrder( ord );
        }

        phaseOrderIssued_ = true;
        return;
    }

    if ( leaderPhase_ == LeaderPhase::BoxAdvance ) {
        if ( phaseOrderIssued_ ) {
            return;
        }

        boxAdvanceTargetPos_ = calcPlayerCentroid( room, leader.pos() );
        mu::Vec3 toTgt = boxAdvanceTargetPos_ - leader.pos();
        float len = toTgt.len();
        mu::Vec3 fwd = (len > 0.01f) ? toTgt * (1.f / len) : mu::Vec3( 1.f, 0.f, 0.f );
        mu::Vec3 right( -fwd.z(), 0.f, fwd.x() );
        mu::Vec3 boxCenter = leader.pos() + fwd * BOX_FRONT_OFFSET;

        std::vector<std::pair<float, TacticalSquad*>> sqByLat;
        sqByLat.reserve( numSquads );

        for ( TacticalSquad* sq : liveSquads ) {
            mu::Vec3 sum{};
            int32 cnt{};

            for ( TacticalNpc* npc : sq->getMemberCache() ) {
                if ( npc && npc->hp() > 0 ) { 
                    sum = sum + npc->pos();
                    ++cnt;
                }
            }

            mu::Vec3 cen = (cnt > 0) ? sum * (1.f / static_cast<float>(cnt)) : leader.pos();
            sqByLat.push_back( { mu::dot( cen, right ), sq } );
        }

        std::sort( sqByLat.begin(), sqByLat.end(),
            []( const std::pair<float, TacticalSquad*>& a, const std::pair<float, TacticalSquad*>& b ) {
                    return a.first < b.first;
            }
        );

        auto offsets = calcSquadBoxOffsets( numSquads );
        for ( int32 i = 0; i < numSquads; ++i ) {
            auto ord = SquadOrder{
                .type = SquadOrderType::BoxAdvance,
                .targetId = primaryTargetId_,
                .leaderPos = leader.pos(),
                .sectorPos = offsets[ i ],
                .formationTargetPos = boxAdvanceTargetPos_,
                .tacticCenter = boxCenter
            };
            sqByLat[ i ].second->receiveOrder( ord );
        }

        phaseOrderIssued_ = true;
        return;
    }

    if ( leaderPhase_ == LeaderPhase::DivideAndConquer ) {
        if ( phaseOrderIssued_ ) {
            return;
        }

        auto clusters = buildPlayerClusters( room, leader );
        if ( clusters.size() <= 1 ) {
            if ( !clusters.empty() && canStartEncircle( liveSquads, clusters.front() ) ) {
                enterPhase( LeaderPhase::Encircle );
            }
            else {
                enterTacticFailCooldown( room, leader );
            }
            return;
        }

        if ( !issueDivideAndConquer( room, leader, liveSquads, clusters ) ) {
            enterTacticFailCooldown( room, leader );
            return;
        }
        phaseOrderIssued_ = true;
        return;
    }

    if ( leaderPhase_ == LeaderPhase::Encircle ) {
        if ( phaseOrderIssued_ ) {
            return;
        }

        constexpr float TWO_PI = 2.f * 3.14159265f;
        auto clusters = buildPlayerClusters( room, leader );

        if ( clusters.size() != 1 || !canStartEncircle( liveSquads, clusters.front() ) ) {
            enterTacticFailCooldown( room, leader );
            return;
        }

        mu::Vec3 encircleCenter = calcPlayerCentroid( room, leader.pos() );
        int32 totalMembers = countLiveMembers( liveSquads );

        if ( totalMembers < 1 ) {
            totalMembers = 1;
        }

        float encircleRadius = calcEncircleRadius( totalMembers );
        float angleAccum = 0.f;

        for ( int32 i = 0; i < numSquads; ++i ) {
            int32 memberCount = static_cast<int32>( liveSquads[ i ]->getMembers().size() );
            float fraction = static_cast<float>( memberCount ) / static_cast<float>( totalMembers );
            float sectorSpan = TWO_PI * fraction;
            float sectorAngle = angleAccum + sectorSpan * 0.5f;

            auto ord = SquadOrder{
                .type = SquadOrderType::Encircle,
                .targetId = primaryId,
                .sectorAngle = sectorAngle,
                .sectorSpan = sectorSpan,
                .approachRadius = encircleRadius,
                .tacticCenter = encircleCenter
            };
            liveSquads[ i ]->receiveOrder( ord );
            angleAccum += sectorSpan;
        }

        encircleIssuedLiveMembers_ = totalMembers;
        phaseOrderIssued_ = true;
        return;
    }

    // 정상 교전 틱: 생존 중 타깃 고정(reset=false), 사망 시에만 재배정 + 중복 engage 억제.
    issueStableEngage( room, liveSquads, /*resetAssignments=*/false );
}

bool MidBossTacticBase::isLivingPlayerTarget( const Room& room, uint32 playerId ) const {
    return playerId != 0 && room.findLivingSessionByPlayerId( static_cast<int32>(playerId) ) != nullptr;
}

void MidBossTacticBase::issueStableEngage( Room& room,
    const std::vector<TacticalSquad*>& liveSquads, bool resetAssignments ) {
    if ( resetAssignments )
        engageTargetBySquad_.clear();

    // 죽은 squad나 죽은 타깃에 묶인 배정 정리.
    for ( auto it = engageTargetBySquad_.begin(); it != engageTargetBySquad_.end(); ) {
        bool squadAlive = false;
        for ( TacticalSquad* squad : liveSquads ) {
            if ( squad && squad->getSquadId() == it->first ) { squadAlive = true; break; }
        }
        if ( !squadAlive || !isLivingPlayerTarget( room, it->second ) )
            it = engageTargetBySquad_.erase( it );
        else
            ++it;
    }

    // 비-reset이면 squad의 현재 Engage 타깃을 맵에 흡수해 고정(sticky) 유지.
    if ( !resetAssignments ) {
        for ( TacticalSquad* squad : liveSquads ) {
            if ( !squad || squad->isEmpty() ) continue;
            int32 squadId = squad->getSquadId();
            if ( engageTargetBySquad_.find( squadId ) != engageTargetBySquad_.end() )
                continue;
            uint32 currentTargetId = squad->getEngageTargetId();
            if ( isLivingPlayerTarget( room, currentTargetId ) )
                engageTargetBySquad_[ squadId ] = currentTargetId;
        }
    }

    const auto& players = room.getLivingPlayers();
    if ( players.empty() )
        return;

    // 플레이어별 현재 배정 수 집계(균형 배정의 1순위 기준).
    std::unordered_map<uint32, int32> assignmentCounts;
    for ( GameSession* p : players )
        assignmentCounts[ static_cast<uint32>(p->id()) ] = 0;
    for ( const auto& [squadId, targetId] : engageTargetBySquad_ ) {
        auto cit = assignmentCounts.find( targetId );
        if ( cit != assignmentCounts.end() )
            ++cit->second;
    }

    for ( TacticalSquad* squad : liveSquads ) {
        if ( !squad || squad->isEmpty() ) continue;

        int32 squadId = squad->getSquadId();
        auto assignedIt = engageTargetBySquad_.find( squadId );
        if ( assignedIt == engageTargetBySquad_.end() ) {
            // 미배정 squad: 배정 수 최소 → 거리 → id 순으로 플레이어 선택.
            GameSession* best = nullptr;
            int32 bestCount = 0;
            float bestDistSq = 0.f;
            mu::Vec3 squadCenter = squad->calcCentroid();

            for ( GameSession* p : players ) {
                uint32 pid = static_cast<uint32>(p->id());
                int32 count = assignmentCounts[ pid ];
                float distSq = (squadCenter - p->player()->pos()).len2();
                if ( !best ||
                     count < bestCount ||
                     (count == bestCount &&
                      (distSq < bestDistSq ||
                       (distSq == bestDistSq && pid < static_cast<uint32>(best->id())))) ) {
                    best = p;
                    bestCount = count;
                    bestDistSq = distSq;
                }
            }

            if ( !best ) continue;
            uint32 bestId = static_cast<uint32>(best->id());
            assignedIt = engageTargetBySquad_.emplace( squadId, bestId ).first;
            ++assignmentCounts[ bestId ];
        }

        // 이미 같은 타깃을 교전 중이면 명령을 재발행하지 않는다(동일 engage 중복 방지).
        if ( squad->getEngageTargetId() == assignedIt->second )
            continue;

        SquadOrder order;
        order.type = SquadOrderType::Engage;
        order.targetId = assignedIt->second;
        squad->receiveOrder( order );
    }
}

int32 GoblinMidBossTactic::countLiveMembers( const std::vector<TacticalSquad*>& liveSquads ) const {
    int32 total{};

    for ( const TacticalSquad* sq : liveSquads ) {
        if ( sq ) {
            total += static_cast<int32>(sq->getMembers().size());
        }
    }
    return total;
}

int32 GoblinMidBossTactic::minMembersForEncircle( int32 playerCount ) const {
    if ( playerCount <= 1 ) {
        return 6;
    }
    if ( playerCount == 2 ) {
        return 8;
    }
    if ( playerCount == 3 ) {
        return 10;
    }
    return 12;
}

bool GoblinMidBossTactic::canStartEncircle( const std::vector<TacticalSquad*>& liveSquads, const PlayerCluster& cluster ) const {
    return countLiveMembers( liveSquads ) >= minMembersForEncircle( static_cast<int32>(cluster.playerIds.size()) );
}

float GoblinMidBossTactic::calcEncircleRadius( int32 liveMembers ) const {
    constexpr float TWO_PI = 2.f * 3.14159265f;
    float radiusByCount = static_cast<float>(std::max( liveMembers, 1 )) * ENCIRCLE_SLOT_SPACING / TWO_PI;
    return std::clamp( radiusByCount, ENCIRCLE_MIN_RADIUS, ENCIRCLE_RADIUS );
}

bool GoblinMidBossTactic::checkTacticsConditions( const PlatoonLeader& leader ) const {
    float maxHp = leader.getConfig().maxHp;
    if ( maxHp > 0.f && static_cast<float>(leader.hp()) / maxHp <= TACTIC_HP_THRESHOLD ) {
        return true;
    }

    const auto& squads = leader.getSquads();
    for ( int32 i = 0; i < squads.size(); ++i ) {
        int32 initial = ( i < initialSquadSizes_.size() ) ? initialSquadSizes_[ i ] : 0;
        int32 current = static_cast<int32>( squads[ i ]->getMembers().size() );

        if ( initial > 0 && static_cast<float>( current ) / static_cast<float>( initial ) <= TACTIC_SQUAD_RATIO ) {
            return true;
        }
    }
    return false;
}

int32 GoblinMidBossTactic::clusterPlayers( const Room& room, const PlatoonLeader& leader ) const {
    return static_cast<int32>( buildPlayerClusters( room, leader ).size() );
}

std::vector<GoblinMidBossTactic::PlayerCluster> GoblinMidBossTactic::buildPlayerClusters( const Room& room, const PlatoonLeader& leader ) const {
    std::vector<PlayerCluster> result = MidBossTacticBase::buildPlayerClusters( room, CLUSTER_RADIUS );

    for ( PlayerCluster& cluster : result ) {
        cluster.representativeId = 0;
        cluster.score = -1.f;

        for ( uint32 id : cluster.playerIds ) {
            const GameSession* s = room.findLivingSessionByPlayerId( static_cast<int32>(id) );
            if ( !s ) {
                continue;
            }

            float score = evaluatePlayerScore( s, leader );
            if ( cluster.representativeId == 0 || score > cluster.score ) {
                cluster.representativeId = id;
                cluster.score = score;
            }
        }
    }
    return result;
}

GameSession* GoblinMidBossTactic::selectPrimaryTarget( Room& room, const PlatoonLeader& leader ) const {
    GameSession* best = nullptr;
    float bestScore = -1.f;

    for ( GameSession* s : room.getLivingPlayers() ) {
        float sc = evaluatePlayerScore( s, leader );
        if ( sc > bestScore ) {
            bestScore = sc;
            best = s;
        }
    }
    return best;
}

uint32 GoblinMidBossTactic::selectReplacementTarget( Room& room, const PlatoonLeader& leader, const std::vector<uint32>& playerIds ) const {
    GameSession* best = nullptr;
    float bestScore = -1.f;

    for ( uint32 id : playerIds ) {
        GameSession* s = room.findLivingSessionByPlayerId( static_cast<int32>(id) );
        if ( !s ) {
            continue;
        }

        float score = evaluatePlayerScore( s, leader );
        if ( score > bestScore ) { 
            bestScore = score;
            best = s;
        }
    }
    return best ? static_cast<uint32>( best->id() ) : 0;
}

bool GoblinMidBossTactic::issueDivideAndConquer( Room& room, PlatoonLeader& leader,
                                                 const std::vector<TacticalSquad*>& liveSquads, const std::vector<PlayerCluster>& clusters ) {
    divideTasks_.clear();
    divideTargetPlayerIds_.clear();
    divideStage_ = DivideStage::Preparing;
    divideEngageTimer_ = 0s;
    divideStageTimer_ = 0s;

    if ( liveSquads.size() < 3 || clusters.size() <= 1 ) {
        return false;
    }

    std::vector<PlayerCluster> sorted = clusters;
    std::sort( sorted.begin(), sorted.end(),
        []( const PlayerCluster& a, const PlayerCluster& b ) {
            return a.score > b.score;
        }
    );

    const PlayerCluster& chargeCluster = sorted[ 0 ];

    // chargeCluster에 가장 가까운 부대를 돌진 부대로 선정
    int32 chargeSquadIdx = 0;
    float bestDist = -1.f;
    for ( int32 i = 0; i < static_cast<int32>( liveSquads.size() ); ++i ) {
        float d = ( liveSquads[ i ]->calcCentroid() - chargeCluster.centroid ).len();
        if ( bestDist < 0.f || d < bestDist ) {
            bestDist = d;
            chargeSquadIdx = i;
        }
    }

    TacticalSquad* chargeSquad = liveSquads[ chargeSquadIdx ];
    if ( !chargeSquad || chargeSquad->isEmpty() ) {
        return false;
    }

    // 회랑 좌표계: chargeSquad → chargeCluster 방향이 전진축, 그 수직이 좌우축
    divideCorridorForward_ = chargeCluster.centroid - chargeSquad->calcCentroid();
    if ( divideCorridorForward_.len2() <= 0.01f ) {
        divideCorridorForward_ = chargeCluster.centroid - leader.pos();
    }
    if ( divideCorridorForward_.len2() <= 0.01f ) {
        divideCorridorForward_ = mu::Vec3( 1.f, 0.f, 0.f );
    }
    else {
        divideCorridorForward_ = norm3( divideCorridorForward_ );
    }
    divideCorridorRight_ = mu::Vec3( -divideCorridorForward_.z(), 0.f, divideCorridorForward_.x() );
    divideCorridorCenter_ = chargeCluster.centroid;
    divideCorridorHalfWidth_ = chargeSquad->estimateWedgeHalfWidth() + CAPTURE_CORRIDOR_CLEARANCE;
    divideTargetPlayerIds_ = chargeCluster.playerIds;

    // 나머지 부대를 chargeCluster 거리순으로 정렬해 가까운 2개를 차단선으로
    // (차단선 길이 divideCorridorHalfLength_ 확정 후 돌진 정점을 회랑 후방 입구 밖으로 잡아야 하므로
    //  돌진 명령은 차단선 루프 뒤에서 발행)
    std::vector<TacticalSquad*> screenCandidates;
    screenCandidates.reserve( liveSquads.size() - 1 );
    for ( int32 i = 0; i < static_cast<int32>( liveSquads.size() ); ++i ) {
        if ( i != chargeSquadIdx ) {
            screenCandidates.push_back( liveSquads[ i ] );
        }
    }
    std::sort( screenCandidates.begin(), screenCandidates.end(),
        [&chargeCluster]( const TacticalSquad* a, const TacticalSquad* b ) {
            return ( a->calcCentroid() - chargeCluster.centroid ).len2() <
                   ( b->calcCentroid() - chargeCluster.centroid ).len2();
        }
    );

    divideCorridorHalfLength_ = -1.f;
    std::vector<uint32> barrierIds;   // 차단선(벽) NPC id — 성공 시 클라에 barrier on 통보
    for ( int32 screenIdx = 0; screenIdx < 2; ++screenIdx ) {
        TacticalSquad* screenSquad = screenCandidates[ screenIdx ];
        if ( !screenSquad || screenSquad->isEmpty() ) {
            return false;
        }

        int32 memberCount = static_cast<int32>( screenSquad->getMembers().size() );
        if ( memberCount < 2 ) {
            return false;
        }

        float separationRadius = 3.f;
        if ( !screenSquad->getMemberCache().empty() && screenSquad->getMemberCache().front() ) {
            separationRadius = screenSquad->getMemberCache().front()->getSeparationRadius();
        }

        // 차단선을 "틈 없는 벽"으로: 인접 NPC 중심 간격을 플레이어가 못 끼는 수준으로 좁힌다.
        // (플레이어 XZ 반경 ~0.4m → 지름 ~0.8m. 간격 CAPTURE_WALL_SPACING(1.2m)면 클라
        //  ContactConstraint가 측면 통과를 막는다. 군집 이탈은 더 이상 전술을 취소하지 않으므로
        //  회랑 길이를 늘리려 간격을 벌리지 않는다 — 간격을 벌리던 것이 종전 이탈의 원인이었다.)
        float spacingScale = CAPTURE_WALL_SPACING / std::max( separationRadius, 0.01f );
        float spacing = std::max( separationRadius * spacingScale, 1.2f );
        float lineHalfLength = static_cast<float>( std::max( memberCount - 1, 0 ) ) * spacing * 0.5f;
        if ( divideCorridorHalfLength_ < 0.f || lineHalfLength < divideCorridorHalfLength_ ) {
            divideCorridorHalfLength_ = lineHalfLength;
        }

        float sideSign = (screenIdx == 0) ? 1.f : -1.f;
        SquadOrder screen{};
        screen.type = SquadOrderType::FormationGuard;
        // FormationGuard 핸들러는 살아있는 플레이어 id를 요구하므로 군집 대표 플레이어를 지정
        screen.targetId = chargeCluster.representativeId;
        screen.slotSpacingScale = spacingScale;
        screen.slotColumnCount = memberCount; // 강제 일렬 배치
        screen.tacticCenter = divideCorridorCenter_ + divideCorridorRight_ * (divideCorridorHalfWidth_ * sideSign);
        screen.formationTargetPos = divideCorridorCenter_;
        screenSquad->receiveOrder( screen );
        divideTasks_.push_back( { screenSquad, DivideTaskType::Screen, chargeCluster.representativeId, chargeCluster.playerIds } );

        for ( TacticalNpc* npc : screenSquad->getMemberCache() ) {
            if ( npc && npc->hp() > 0 ) {
                barrierIds.push_back( static_cast<uint32>( npc->getId() ) );
            }
        }
    }

    // 돌진 부대: 쐐기 준비만 하고 차단선 완성 전까지 release 대기.
    // 준비 정점을 회랑 후방 입구 밖(차단선 끝보다 더 뒤)으로 잡아 차단선 사이를 관통하도록 함.
    SquadOrder charge{};
    charge.type = SquadOrderType::WedgeCharge;
    charge.targetId = chargeCluster.representativeId;
    charge.targetIds = chargeCluster.playerIds;
    charge.tacticCenter = chargeCluster.centroid;
    charge.waitForChargeRelease = true;
    charge.hasWedgeApex = true;
    charge.wedgeApexPos = divideCorridorCenter_
        - divideCorridorForward_ * (divideCorridorHalfLength_ + CAPTURE_CHARGE_STANDOFF);
    chargeSquad->receiveOrder( charge );
    divideTasks_.push_back( { chargeSquad, DivideTaskType::Charge, chargeCluster.representativeId, chargeCluster.playerIds } );

    // 남는 부대는 대기
    for ( size_t i = 2; i < screenCandidates.size(); ++i ) {
        SquadOrder idle{};
        idle.type = SquadOrderType::Idle;
        screenCandidates[ i ]->receiveOrder( idle );
    }

    const bool success = ( divideTasks_.size() == 3 );
    if ( success && !barrierIds.empty() ) {
        // 차단선 NPC id를 보관만 한다. barrier 활성화는 형성이 끝나는 시점(돌진 발동)에 해야
        // 형성 중 이동하는 NPC가 대상 군집을 밀어 돌진 경로 밖으로 내보내는 걸 막는다.
        divideBarrierNpcIds_ = barrierIds;
        divideBarrierOn_ = false;
    }
    return success;
}

void GoblinMidBossTactic::clearDivideBarriers( Room& room ) {
    if ( divideBarrierNpcIds_.empty() ) {
        return;
    }
    if ( divideBarrierOn_ ) {   // 실제로 켜진 경우에만 off 송신
        room.broadcast( PacketManager::makeSNpcBarrierPacket( false, divideBarrierNpcIds_ ) );
        divideBarrierOn_ = false;
    }
    divideBarrierNpcIds_.clear();
}

void GoblinMidBossTactic::updateDivideAndConquer( Seconds dt, Room& room, PlatoonLeader& leader ) {
    if ( !phaseOrderIssued_ ) {
        return;
    }
    if ( divideTasks_.size() != 3 ) {
        enterTacticFailCooldown( room, leader );
        return;
    }

    divideStageTimer_ += dt;   // 현재 스테이지 누적 시간(타임아웃 폴백용)

    TacticalSquad* chargeSquad = nullptr;
    bool screenSquadsReady = true;
    for ( const auto& task : divideTasks_ ) {
        if ( !task.squad || task.squad->isEmpty() ) {
            // 배정된 부대가 전멸하면 진행 중 돌진을 정리하고 실패 처리
            if ( task.type == DivideTaskType::Charge && task.squad ) {
                task.squad->endActiveWedgeCharge( room );
            }
            else if ( chargeSquad ) {
                chargeSquad->endActiveWedgeCharge( room );
            }
            enterTacticFailCooldown( room, leader );
            return;
        }

        if ( task.type == DivideTaskType::Charge ) {
            chargeSquad = task.squad;
        }
        else if ( task.type == DivideTaskType::Screen && !task.squad->areMembersAtSlots() ) {
            screenSquadsReady = false;
        }
    }
    if ( !chargeSquad ) {
        enterTacticFailCooldown( room, leader );
        return;
    }

    if ( divideStage_ == DivideStage::Preparing ) {
        // 군집이 회랑을 벗어나도 전술을 취소하지 않는다. 측면 차단벽이 물리적으로 막고,
        // 탈출(벽 NPC 처치로 구멍 내기 / 열린 앞·뒤로 우회)은 플레이어의 정당한 카운터플레이다.
        // 빠져나갔다면 빈 회랑을 그대로 관통하며 돌진을 마친다(놓침 허용).

        // 쐐기 준비 완료 + 좌우 차단선 완성(또는 타임아웃) → 돌진 동시 발동
        if ( (chargeSquad->isWedgePrepared() && screenSquadsReady)
            || divideStageTimer_ >= DIVIDE_PREP_TIMEOUT ) {
            chargeSquad->releaseWedgeCharge();
            // 차단선 형성 완료(또는 타임아웃) → 이제 barrier 활성화. 형성 중엔 미활성이라 군집을 안 밀고,
            // 돌진이 시작되며 측면 벽이 단단해져 트랩이 닫힌다.
            if ( !divideBarrierOn_ && !divideBarrierNpcIds_.empty() ) {
                room.broadcast( PacketManager::makeSNpcBarrierPacket( true, divideBarrierNpcIds_ ) );
                divideBarrierOn_ = true;
            }
            divideStage_ = DivideStage::Charging;
            divideStageTimer_ = 0s;
        }
        return;
    }

    if ( divideStage_ == DivideStage::Charging ) {
        // 돌진 완료(또는 타임아웃)까지 대기 — 끼인 멤버로 인한 종료 지연 방지
        if ( !chargeSquad->areChargeMembersComplete() && divideStageTimer_ < DIVIDE_CHARGE_TIMEOUT ) {
            return;
        }

        chargeSquad->endActiveWedgeCharge( room );
        clearDivideBarriers( room );   // 돌진 관통 완료 → 벽 역할 종료, 차단 해제
        issueDivideEngage( room, leader );
        divideStage_ = DivideStage::Engaging;
        divideStageTimer_ = 0s;
        divideEngageTimer_ = DIVIDE_ENGAGE_DURATION;
        return;
    }

    divideEngageTimer_ -= dt;
    if ( divideEngageTimer_ <= 0s ) {
        clearDivideBarriers( room );   // 안전망(보통 돌진 완료 시 이미 해제됨)
        tacticCooldown_ = TACTIC_COOLDOWN_DURATION;
        enterPhase( LeaderPhase::Cooldown );
    }
}

void GoblinMidBossTactic::issueDivideEngage( Room& room, PlatoonLeader& leader ) {
    // 쐐기/각개격파 종료 → squad별 균형 재배정으로 일반 교전 전환(전면 재배정).
    issueStableEngage( room, collectLiveSquads( leader ), /*resetAssignments=*/true );
}

bool GoblinMidBossTactic::updateBossPersonalCombat( Seconds dt, Room& room, PlatoonLeader& leader ) {
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
            leader.setDesiredVel( mu::Vec3{} );   // 타깃 없음 → 정지(잔여 motor 속도 제거)
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
                && candidate.score > currentScore + BOSS_TARGET_SWITCH_MARGIN 
            ) {
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
            leader.setDesiredVel( mu::Vec3{} );   // 사거리 진입 → 정지(공격 중 미끄러짐 방지)
            leader.transitionTacticalState( TacticalNpcState::AttackWindup );
            return true;
        }

        moveBossToward( leader, target->player()->pos(), BOSS_CHASE_SPEED_MULT );
        return true;
    }

    if ( bossPersonalState_ == BossPersonalState::AttackWindup ) {
        leader.setDesiredVel( mu::Vec3{} );   // 공격 윈드업 중 정지 유지
        leader.setFacing( dir );
        bossPersonalTimer_ += dt;

        TacticalNpcConfig cfg = leader.getConfig();
        if ( bossPersonalTimer_ < cfg.attackWindupTime ) {
            return true;
        }

        if ( dist <= leader.getAttackRange() ) {
            leader.applyHitToSession( target, leader.getAttackDamage() );
        }

        bossPersonalTimer_ = 0s;
        bossPersonalState_ = BossPersonalState::AttackRecover;
        leader.transitionTacticalState( TacticalNpcState::AttackRecover );
        return true;
    }

    if ( bossPersonalState_ == BossPersonalState::AttackRecover ) {
        leader.setDesiredVel( mu::Vec3{} );   // 공격 후딜 중 정지 유지
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

    return true;
}

uint32 GoblinMidBossTactic::selectBossPersonalTarget( Room& room, const PlatoonLeader& leader ) const {
    return selectBossPersonalTargetScore( room, leader ).targetId;
}

GoblinMidBossTactic::BossTargetScore GoblinMidBossTactic::selectBossPersonalTargetScore( Room& room, const PlatoonLeader& leader ) const {
    std::vector<PlayerCluster> clusters = MidBossTacticBase::buildPlayerClusters( room, CLUSTER_RADIUS );
    mu::Vec3 leaderPos = leader.pos();
    BossTargetScore best{};
    bool hasBest = false;

    for ( const PlayerCluster& cluster : clusters ) {
        float clusterBaseScore = static_cast<float>( cluster.playerIds.size() ) * 1000.f;

        for ( uint32 playerId : cluster.playerIds ) {
            const GameSession* s = room.findLivingSessionByPlayerId( static_cast<int32>(playerId) );
            if ( !s ) {
                continue;
            }

            float distance = ( leaderPos - s->player()->pos() ).len();
            float score = clusterBaseScore - distance;

            bool better = !hasBest || score > best.score + 0.001f || (std::fabs( score - best.score ) <= 0.001f && playerId < best.targetId);
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

bool GoblinMidBossTactic::calcBossPersonalTargetScore( Room& room, const PlatoonLeader& leader, uint32 targetId, float& outScore ) const {
    if ( targetId == 0 ) {
        return false;
    }

    const GameSession* s = room.findLivingSessionByPlayerId( static_cast<int32>(targetId) );
    if ( !s ) {
        return false;
    }

    std::vector<PlayerCluster> clusters = MidBossTacticBase::buildPlayerClusters( room, CLUSTER_RADIUS );
    mu::Vec3 leaderPos = leader.pos();

    for ( const PlayerCluster& cluster : clusters ) {
        if ( std::find( cluster.playerIds.begin(), cluster.playerIds.end(), targetId ) == cluster.playerIds.end() ) {
            continue;
        }

        float distance = (leaderPos - s->player()->pos()).len();
        outScore = static_cast<float>(cluster.playerIds.size()) * 1000.f - distance;
        return true;
    }
    return false;
}

GameSession* GoblinMidBossTactic::resolveBossPersonalTarget( Room& room, uint32 targetId ) const {
    if ( targetId == 0 ) {
        return nullptr;
    }

    return room.findLivingSessionByPlayerId( static_cast<int32>(targetId) );
}

void GoblinMidBossTactic::moveBossToward( PlatoonLeader& leader, mu::Vec3 targetPos, float speedMult ) const {
    mu::Vec3 toTarget = targetPos - leader.pos();
    float dist = toTarget.len();

    if ( dist <= 0.01f ) {
        return;
    }

    mu::Vec3 dir = toTarget * (1.f / dist);
    leader.setFacing( dir );

    float spd = leader.getLeaderMoveSpeed() * speedMult;
    leader.setDesiredVel( mu::Vec3( dir.x() * spd, 0.f, dir.z() * spd ) );
}

float GoblinMidBossTactic::evaluatePlayerScore( const GameSession* s, const PlatoonLeader& leader ) const {
    float dist = (leader.pos() - s->player()->pos()).len();
    float distScore = 1.f / (1.f + dist);
    float hpRatio = static_cast<float>(s->player()->hp()) / 1'000'000.f;
    float hpScore = 1.f - hpRatio;
    return distScore * 0.5f + hpScore * 0.5f;
}

bool GoblinMidBossTactic::allMembersArrived( const PlatoonLeader& leader ) const {
    bool anyAlive = false;

    for ( TacticalSquad* sq : leader.getSquads() ) {
        if ( sq->isEmpty() ) {
            continue;
        }

        anyAlive = true;

        if ( !sq->areMembersAtSlots() ) {
            return false;
        }
    }
    return anyAlive;
}

bool GoblinMidBossTactic::formationReady( const PlatoonLeader& leader ) const {
    // 도착 완료 또는 단계 타임아웃 — 끼인 소수 멤버로 인한 무한 stall 방지
    return allMembersArrived( leader ) || phaseElapsed_ >= FORMATION_TIMEOUT;
}

std::vector<mu::Vec3> GoblinMidBossTactic::calcSquadBoxOffsets( int32 numSquads ) const {
    int32 rows = static_cast<int32>( std::max( 1.f, std::floorf(std::sqrtf(static_cast<float>(numSquads))) ) );
    int32 cols = (numSquads + rows - 1) / rows;

    std::vector<mu::Vec3> offsets;
    offsets.reserve( numSquads );

    for ( int32 i = 0; i < numSquads; ++i ) {
        int32 col = i % cols;
        int32 row = i / cols;
        float colOff = (static_cast<float>( col ) - static_cast<float>( cols - 1 ) * 0.5f) * BOX_SQUAD_SPACING;
        float halfCols = static_cast<float>( cols - 1 ) * 0.5f;
        float latFrac = (cols > 1)
            ? std::abs( static_cast<float>(col) - halfCols ) / halfCols
            : 0.f;
        float rowOff = (static_cast<float>(row) - static_cast<float>(rows - 1) * 0.5f) * BOX_SQUAD_SPACING;
        float arcZ = rowOff - BOX_ARC_DEPTH * latFrac;
        offsets.push_back( mu::Vec3( colOff, 0.f, arcZ ) );
    }
    return offsets;
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
            spawnSnakeWave( room, leader, originalSnakeSquad );
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
            room.broadcast( PacketManager::makeSHitPacket( static_cast<uint16>( npcId ), 0 ) );
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

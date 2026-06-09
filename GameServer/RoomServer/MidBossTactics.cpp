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

void MidBossTacticBase::assignSquadsToPlayers( const Room& room, const PlatoonLeader& leader,
                                               const std::vector<TacticalSquad*>& liveSquads, std::vector<uint32_t>& outTargetIds ) const
{
    const auto& players = room.getLivingPlayers();
    int32 numSquads = static_cast<int32>(liveSquads.size());
    int32 numPlayers = static_cast<int32>(players.size());

    if ( numPlayers <= 1 ) {
        return;
    }

    int32 maxPerPlayer = (numSquads + numPlayers - 1) / numPlayers;

    struct DistEntry { 
        float dist; 
        int32 squadIdx; 
        int32 playerIdx;
    };

    std::vector<DistEntry> entries;
    entries.reserve( numSquads * numPlayers );

    for ( int32 si = 0; si < numSquads; ++si ) {
        mu::Vec3 centroid{};
        int32 cnt{};

        for ( TacticalNpc* npc : liveSquads[ si ]->getMemberCache() ) {
            if ( npc && npc->hp() > 0 ) {
                centroid = centroid + npc->pos();
                ++cnt;
            }
        }

        if ( cnt > 0 ) {
            centroid = centroid * (1.f / static_cast<float>(cnt));
        }
        else {
            centroid = leader.pos();
        }

        for ( int32 pi = 0; pi < numPlayers; ++pi ) {
            float dSq = ( centroid - players[ pi ]->player()->pos() ).len2();
            entries.push_back( { dSq, si, pi } );
        }
    }

    std::sort( entries.begin(), entries.end(),
        []( const DistEntry& a, const DistEntry& b ) { 
            return a.dist < b.dist;
        }
    );

    std::vector<bool>  squadDone( numSquads, false );
    std::vector<int32> playerCount( numPlayers, 0 );

    for ( const auto& e : entries ) {
        if ( squadDone[ e.squadIdx ] ) {
            continue;
        }
        if ( playerCount[ e.playerIdx ] >= maxPerPlayer ) {
            continue;
        }

        outTargetIds[ e.squadIdx ] = static_cast<uint32_t>(players[ e.playerIdx ]->id());
        squadDone[ e.squadIdx ] = true;
        ++playerCount[ e.playerIdx ];
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
            GameSession* encircleTarget = selectPrimaryTarget( room, leader );

            if ( encircleTarget ) {
                for ( auto* sq : squads ) {
                    if ( sq->isEmpty() ) {
                        continue;
                    }

                    auto ord = SquadOrder{
                        .type = SquadOrderType::Engage,
                        .targetId = static_cast<uint32>(encircleTarget->id())
					};
                    sq->receiveOrder( ord );
                }
            }

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

                for ( auto* sq : squads ) {
                    if ( sq->isEmpty() ) {
                        continue;
                    }

                    auto ord = SquadOrder{
                        .type = SquadOrderType::Engage,
                        .targetId = primaryTargetId_
                    };
                    sq->receiveOrder( ord );
                }
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
        }
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

    std::vector<uint32> targets( liveSquads.size(), static_cast<uint32>(primary->id()) );
    assignSquadsToPlayers( room, leader, liveSquads, targets );

    for ( int32 i = 0; i < liveSquads.size(); ++i ) {
        auto ord = SquadOrder{
            .type = SquadOrderType::Engage,
            .targetId = targets[ i ]
		};
        liveSquads[ i ]->receiveOrder( ord );
    }
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

    std::vector<uint32_t> targets( numSquads, primaryId );
    assignSquadsToPlayers( room, leader, liveSquads, targets );

    for ( int32 i = 0; i < numSquads; ++i ) {
        auto ord = SquadOrder{
            .type = SquadOrderType::Engage,
			.targetId = targets[ i ]
		};
        liveSquads[ i ]->receiveOrder( ord );
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
    uint32 targetId = selectReplacementTarget( room, leader, divideTargetPlayerIds_ );
    if ( targetId == 0 ) {
        GameSession* primary = selectPrimaryTarget( room, leader );
        targetId = primary ? static_cast<uint32>( primary->id() ) : 0;
    }

    for ( TacticalSquad* squad : leader.getSquads() ) {
        if ( !squad || squad->isEmpty() ) {
            continue;
        }
        SquadOrder order{};
        order.type = (targetId != 0) ? SquadOrderType::Engage : SquadOrderType::Idle;
        order.targetId = targetId;
        squad->receiveOrder( order );
    }
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

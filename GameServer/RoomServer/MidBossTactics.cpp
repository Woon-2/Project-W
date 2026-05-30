#include "rspch.hpp"
#include "MidBossTactics.hpp"
#include "PlatoonLeader.hpp"
#include "Room.hpp"
#include "GameSession.hpp"
#include "TacticalSquad.hpp"
#include "TacticalNpc.hpp"
#include "object.hpp"
#include <algorithm>
#include <cmath>

/*--------------------------------
	  Local Utility Function
--------------------------------*/

static mu::Vec3 norm3( mu::Vec3 v ) {
    float l = v.len();
    return l > 0.0001f ? v * (1.f / l) : mu::Vec3( 0.f, 0.f, 0.f );
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
        enterPhase( LeaderPhase::BossSolo, leader );
    }

    if ( leaderPhase_ == LeaderPhase::Cooldown ) {
        tacticCooldown_ -= dt;
        if ( tacticCooldown_ <= 0s ) {
            enterPhase( tacticsUnlocked_ ? LeaderPhase::TacticalRetreat : LeaderPhase::BoxAdvance, leader );
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

        if ( phaseOrderIssued_ && allMembersArrived( room, leader ) ) {
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
            enterPhase( LeaderPhase::Cooldown, leader );
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
        enterPhase( LeaderPhase::TacticalRetreat, leader );
    }

    if ( leaderPhase_ == LeaderPhase::BoxAdvance && primary && allMembersArrived( room, leader ) ) {
        if ( !tacticsUnlocked_ ) {
            enterPhase( LeaderPhase::Engage, leader );

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
                enterPhase( LeaderPhase::Encircle, leader );
            }
            else if ( clusters.size() == 1 ) {
                enterTacticFailCooldown( room, leader );
            }
            else {
                enterPhase( LeaderPhase::Vigilance, leader );
            }
        }
    }

    if ( leaderPhase_ == LeaderPhase::Vigilance && phaseOrderIssued_ && allMembersArrived( room, leader ) ) {
        auto clusters = buildPlayerClusters( room, leader );
        auto liveSquads = collectLiveSquads( leader );

        if ( clusters.size() <= 1 && !clusters.empty() && canStartEncircle( liveSquads, clusters.front() ) ) {
            enterPhase( LeaderPhase::Encircle, leader );
        }
        else if ( clusters.size() <= 1 ) {
            enterTacticFailCooldown( room, leader );
        }
        else {
            enterPhase( LeaderPhase::DivideAndConquer, leader );
        }
    }

    bool leaderAtRetreat = ( leader.pos() - retreatTargetPos_ ).len() <= 1.5f;
    if ( leaderPhase_ == LeaderPhase::TacticalRetreat && phaseOrderIssued_
        && allMembersArrived( room, leader ) && leaderAtRetreat
    ) {
        enterPhase( LeaderPhase::BoxAdvance, leader );
    }

    tacticTimer_ -= dt;
    if ( tacticTimer_ <= 0s ) {
        tacticTimer_ = TACTIC_INTERVAL;
        evaluateTactics( room, leader );
    }

    if ( leaderPhase_ == LeaderPhase::TacticalRetreat ) {
        mu::Vec3 toRetreat = retreatTargetPos_ - leader.pos();
        float d = toRetreat.len();

        if ( d > 1.f ) {
            mu::Vec3 dir = toRetreat * (1.f / d);
            float spd = leader.getLeaderMoveSpeed() * TACTICAL_SPEED_MULT;
            float yVel = leader.body().linearVel().y();
            leader.setLinearVel( mu::Vec3( dir.x() * spd, yVel, dir.z() * spd ) );
            leader.setFacing( dir );
        }
        return;
    }

    updateBossPersonalCombat( dt, room, leader );
}

void GoblinMidBossTactic::enterPhase( LeaderPhase next, PlatoonLeader& leader ) {
    leaderPhase_ = next;
    phaseOrderIssued_ = false;

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

    tacticCooldown_ = TACTIC_FAIL_COOLDOWN_DURATION;
    enterPhase( LeaderPhase::Cooldown, leader );

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
    };

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

    if ( leaderPhase_ == LeaderPhase::Vigilance ) {
        if ( phaseOrderIssued_ ) {
            return;
        }

        mu::Vec3 playerCent = calcPlayerCentroid( room, leader.pos() );
        mu::Vec3 toPlayers = playerCent - leader.pos();
        float len = toPlayers.len();
        mu::Vec3 forward = (len > 0.01f) ? toPlayers * (1.f / len) : mu::Vec3( 1.f, 0.f, 0.f );
        float baseAngle = std::atan2f( forward.z(), forward.x() );
        constexpr float TWO_PI = 2.f * 3.14159265f;

        for ( int32 i = 0; i < numSquads; ++i ) {
            auto ord = SquadOrder{
                .type = SquadOrderType::GuardBoss,
                .targetId = primaryTargetId_,
                .sectorAngle = baseAngle + TWO_PI * static_cast<float>( i ) / static_cast<float>( numSquads ),
                .approachRadius = VIGILANCE_GUARD_RADIUS,
                .formationTargetPos = playerCent,
                .tacticCenter = leader.pos()
            };
            liveSquads[ i ]->receiveOrder( ord );
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
                enterPhase( LeaderPhase::Encircle, leader );
            }
            else {
                enterTacticFailCooldown( room, leader );
            }
            return;
        }

        issueDivideAndConquer( leader, liveSquads, clusters );
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

void GoblinMidBossTactic::issueDivideAndConquer( PlatoonLeader& leader,
                                                 const std::vector<TacticalSquad*>& liveSquads, const std::vector<PlayerCluster>& clusters ) {
    divideTasks_.clear();
    if ( liveSquads.empty() || clusters.size() <= 1 ) {
        return;
    }

    std::vector<PlayerCluster> sorted = clusters;
    std::sort( sorted.begin(), sorted.end(),
        []( const PlayerCluster& a, const PlayerCluster& b ) {
            return a.score > b.score;
        }
    );

    const PlayerCluster& chargeCluster = sorted[ 0 ];
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
    auto charge = SquadOrder{
        .type = SquadOrderType::WedgeCharge,
        .targetId = chargeCluster.representativeId,
        .tacticCenter = chargeCluster.centroid,
        .targetIds = chargeCluster.playerIds
    };
    chargeSquad->receiveOrder( charge );
    divideTasks_.push_back( { chargeSquad, DivideTaskType::Charge, chargeCluster.representativeId, chargeCluster.playerIds } );

    mu::Vec3 supportCentroid{};
    int32 supportCount{};
    uint32 supportTargetId{};
    std::vector<uint32> supportPlayerIds;

    for ( int32 i = 1; i < sorted.size(); ++i ) {
        const PlayerCluster& cl = sorted[ i ];
        supportCentroid = supportCentroid + cl.centroid;
        ++supportCount;

        if ( supportTargetId == 0 ) {
            supportTargetId = cl.representativeId;
        }

        supportPlayerIds.insert( supportPlayerIds.end(), cl.playerIds.begin(), cl.playerIds.end() );
    }

    if ( supportCount == 0 ) {
        return;
    }

    supportCentroid = supportCentroid * (1.f / static_cast<float>( supportCount ));

    mu::Vec3 blockDir = norm3( supportCentroid - chargeCluster.centroid );
    if ( blockDir.len2() < 0.001f ) {
        blockDir = mu::Vec3( 1.f, 0.f, 0.f );
    }

    mu::Vec3 blockCenter = chargeCluster.centroid + (supportCentroid - chargeCluster.centroid) * SCREEN_BLOCK_CENTER_BIAS;
    mu::Vec3 blockRight( -blockDir.z(), 0.f, blockDir.x() );
    float baseAngle = std::atan2f( blockRight.z(), blockRight.x() );

    int32 screenIdx{};
    for ( int32 i = 0; i < static_cast<int32>( liveSquads.size() ); ++i ) {
        if ( i == chargeSquadIdx ) {
            continue;
        }

        float sideSign = (screenIdx % 2 == 0) ? 1.f : -1.f;

        auto screen = SquadOrder{
            .type = SquadOrderType::GuardBoss,
            .targetId = ( supportTargetId != 0 ) ? supportTargetId : chargeCluster.representativeId,
            .slotSpacingScale = SCREEN_SLOT_SPACING_SCALE,
            .slotColumnScale = SCREEN_SLOT_COLUMN_SCALE,
            .slotColumnCount = SCREEN_SLOT_COLUMN_COUNT,
            .sectorAngle = baseAngle + ( sideSign < 0.f ? 3.14159265f : 0.f ),
            .approachRadius = SCREEN_BLOCK_SPACING * ( 1.f + 0.5f * static_cast<float>( screenIdx / 2 ) ),
            .formationTargetPos = supportCentroid,
            .tacticCenter = blockCenter
        };
        liveSquads[ i ]->receiveOrder( screen );
        divideTasks_.push_back( { liveSquads[ i ], DivideTaskType::Screen, screen.targetId, supportPlayerIds } );
        ++screenIdx;
    }
}

void GoblinMidBossTactic::updateDivideAndConquer( Seconds dt, Room& room, PlatoonLeader& leader ) {
    if ( !phaseOrderIssued_ ) {
        return;
    }
    if ( divideTasks_.empty() ) {
        enterTacticFailCooldown( room, leader );
        return;
    }

    for ( auto& task : divideTasks_ ) {
        if ( !task.squad || task.squad->isEmpty() ) {
            task.taskCompleted = true;
        }
        else if ( !task.taskCompleted ) {
            switch ( task.type ) {
            case DivideTaskType::Charge:
                task.taskCompleted = task.squad->areChargeMembersComplete();
                break;

            case DivideTaskType::Screen:
                task.taskCompleted = task.squad->areMembersAtSlots();
                break;

            default:
                break;
            }
        }
        else {
			// no-op
        }
    }

    bool allScreensCompleted = true;
    for ( const auto& task : divideTasks_ ) {
        if ( task.type == DivideTaskType::Screen && !task.taskCompleted ) {
            allScreensCompleted = false;
            break;
        }
    }

    bool allProtected = true;
    for ( auto& task : divideTasks_ ) {
        if ( task.taskCompleted && !task.engageIssued ) {
            if ( task.type == DivideTaskType::Screen && !allScreensCompleted ) {
                allProtected = false;
                continue;
            }

            uint32 targetId = selectReplacementTarget( room, leader, task.clusterPlayerIds );
            if ( targetId != 0 && task.squad && !task.squad->isEmpty() ) {
                auto ord = SquadOrder{
                    .type = SquadOrderType::Engage,
                    .targetId = targetId
				};
                task.squad->receiveOrder( ord );
                task.targetId = targetId;
            }

            task.engageIssued = true;
            task.engageProtectTimer = 0s;
        }

        if ( task.engageIssued ) {
            if ( task.targetId != 0 ) {
                GameSession* target = room.findLivingSessionByPlayerId( static_cast<int32>(task.targetId) );

                if ( !target ) {
                    uint32 replacement = selectReplacementTarget( room, leader, task.clusterPlayerIds );
                    task.targetId = replacement;

                    if ( replacement != 0 && task.squad && !task.squad->isEmpty() ) {
                        auto ord = SquadOrder{
                            .type = SquadOrderType::Engage,
                            .targetId = replacement
                        };
                        task.squad->receiveOrder( ord );
                    }
                }
            }
            task.engageProtectTimer += dt;
        }

        if ( !task.engageIssued || task.engageProtectTimer < DIVIDE_ENGAGE_PROTECT_DURATION ) {
            allProtected = false;
        }
    }

    if ( allProtected ) {
        tacticCooldown_ = TACTIC_COOLDOWN_DURATION;
        enterPhase( LeaderPhase::Cooldown, leader );
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
            leader.transitionTacticalState( TacticalNpcState::AttackWindup );
            return true;
        }

        moveBossToward( leader, target->player()->pos(), BOSS_CHASE_SPEED_MULT );
        return true;
    }

    if ( bossPersonalState_ == BossPersonalState::AttackWindup ) {
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
    float yVel = leader.body().linearVel().y();
    leader.setLinearVel( mu::Vec3( dir.x() * spd, yVel, dir.z() * spd ) );
}

float GoblinMidBossTactic::evaluatePlayerScore( const GameSession* s, const PlatoonLeader& leader ) const {
    float dist = (leader.pos() - s->player()->pos()).len();
    float distScore = 1.f / (1.f + dist);
    float hpRatio = static_cast<float>(s->player()->hp()) / 1'000'000.f;
    float hpScore = 1.f - hpRatio;
    return distScore * 0.5f + hpScore * 0.5f;
}

bool GoblinMidBossTactic::allMembersArrived( const Room& /*room*/, const PlatoonLeader& leader ) const {
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

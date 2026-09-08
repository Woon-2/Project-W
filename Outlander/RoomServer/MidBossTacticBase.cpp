#include "rspch.hpp"
#include "MidBossTacticBase.hpp"
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

void MidBossTacticBase::onLeaderDead( Room& room, PlatoonLeader& leader ) {
    // Once the boss falls, every surviving trooper must be cleanly finishable
    // regardless of the protection profile that was active at the time.
    setEncounterDamageProfile( room, leader, 1.f, 1.f );
    leader.removeDeadMembersFromSquads();
    if ( hasLiveSquadMembers( leader ) ) {
        room.notifyTacticalDialogue( TacticalDialogueId::TroopersFlee );
    }
    leader.pushConfusedToSquads( room );
}

bool MidBossTacticBase::hasLiveSquadMembers( const PlatoonLeader& leader ) const {
    for ( const TacticalSquad* squad : leader.getSquads() ) {
        if ( squad && !squad->isEmpty() ) {
            return true;
        }
    }
    return false;
}

void MidBossTacticBase::setEncounterDamageProfile(
    Room& room, PlatoonLeader& leader, float leaderMultiplier, float memberMultiplier ) const {
    leader.setDamageTakenMultiplier( leaderMultiplier );
    for ( TacticalSquad* squad : leader.getSquads() ) {
        if ( !squad ) {
            continue;
        }
        for ( uint32 memberId : squad->getMembers() ) {
            if ( TacticalNpc* npc = room.findTacticalNpcById( memberId ); npc && npc->hp() > 0 ) {
                npc->setDamageTakenMultiplier( memberMultiplier );
            }
        }
    }
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

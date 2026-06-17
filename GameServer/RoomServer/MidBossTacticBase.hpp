#ifndef midboss_tactic_base_hpp
#define midboss_tactic_base_hpp

#include "IMidBossTactic.hpp"
#include "mathUtil.hpp"
#include <cstdint>
#include <vector>
#include <unordered_map>

class TacticalSquad;
class TacticalNpc;
class GameSession;
struct TacticalNpcConfig;

/*---------------------------
      MidBossTacticBase
---------------------------*/

class MidBossTacticBase : public IMidBossTactic {
public:
    struct PlayerCluster {
        mu::Vec3              centroid{};
        uint32                representativeId{ 0 };
        std::vector<uint32>   playerIds{};
        float                 score{ 0.f };
    };

	MidBossTacticBase() = default;
	virtual ~MidBossTacticBase() = default;

    virtual void onLeaderDead( Room& room, PlatoonLeader& leader ) override;

protected:
    std::vector<TacticalSquad*>  collectLiveSquads( PlatoonLeader& leader ) const;
    std::vector<PlayerCluster>   buildPlayerClusters( const Room& room, float clusterRadius ) const;
    mu::Vec3 MU_CALLCONV calcPlayerCentroid( const Room& room, mu::Vec3 fallback ) const;
    mu::Vec3 MU_CALLCONV calcAveragePlayerFacing( const Room& room, mu::Vec3 fallbackDir ) const;
    GameSession* MU_CALLCONV selectNearestPlayer( Room& room, mu::Vec3 from ) const;
    uint32       MU_CALLCONV selectNearestPlayerId( Room& room, mu::Vec3 from ) const;
    void issueEngageAll( PlatoonLeader& leader, uint32 targetId ) const;
    void issueIdleAll( PlatoonLeader& leader ) const;
    // squad별 Engage 타깃을 균형(배정 수)→거리→id 순으로 배정하되, 생존 중에는 고정(sticky)하고
    // 타깃이 바뀔 때만 명령을 발행해 중복 engage를 막는다. resetAssignments=true면 전면 재배정.
    // (Goblin/GrandBaum 등 전술 공용 — squad+player 균형배정이라 전술 무관.)
    void issueStableEngage( Room& room, const std::vector<TacticalSquad*>& liveSquads, bool resetAssignments );
    bool isLivingPlayerTarget( const Room& room, uint32 playerId ) const;

    // squadId → 고정된 Engage 타깃 플레이어 id. 생존 중 유지, 사망/전술종료 시 재배정.
    std::unordered_map<int32, uint32> engageTargetBySquad_{};
};

#endif // midboss_tactic_base_hpp

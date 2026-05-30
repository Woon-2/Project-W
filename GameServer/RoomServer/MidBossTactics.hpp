#ifndef midboss_tactic_hpp
#define midboss_tactic_hpp

#include "IMidBossTactic.hpp"
#include "mathUtil.hpp"
#include <cstdint>
#include <vector>

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
    void assignSquadsToPlayers( const Room& room, const PlatoonLeader& leader,
                                const std::vector<TacticalSquad*>& liveSquads, std::vector<uint32>& outTargetIds ) const;
    void issueEngageAll( PlatoonLeader& leader, uint32 targetId ) const;
    void issueIdleAll( PlatoonLeader& leader ) const;
};

/*-----------------------------
      GoblinMidBossTactic
-----------------------------*/

class GoblinMidBossTactic : public MidBossTacticBase {
public:
	GoblinMidBossTactic() = default;
	virtual ~GoblinMidBossTactic() = default;

    virtual void update(Seconds dt, Room& room, PlatoonLeader& leader) override;

private:
    enum class LeaderPhase : byte {
        BoxAdvance,
        Engage,
        TacticalRetreat,
        Encircle,
        Vigilance,
        DivideAndConquer,
        Cooldown,
        BossSolo
    };

    enum class BossPersonalState : byte {
        EvaluateTarget,
        ChaseTarget,
        AttackWindup,
        AttackRecover
    };

    enum class DivideTaskType : byte { None, Charge, Screen };

    struct DivideSquadTask {
        TacticalSquad*        squad{ nullptr };
        DivideTaskType        type{ DivideTaskType::None };
        uint32                targetId{ 0 };
        std::vector<uint32>   clusterPlayerIds{};
        bool                  taskCompleted{ false };
        bool                  engageIssued{ false };
        Seconds               engageProtectTimer{};
    };

    struct BossTargetScore {
        uint32   targetId{ 0 };
        float    score{ 0.f };
    };

    void        enterPhase( LeaderPhase next, PlatoonLeader& leader );
    void        enterTacticFailCooldown( Room& room, PlatoonLeader& leader );
    void        evaluateTactics( Room& room, PlatoonLeader& leader );
    bool        updateBossPersonalCombat( Seconds dt, Room& room, PlatoonLeader& leader );
    uint32      selectBossPersonalTarget( Room& room, const PlatoonLeader& leader ) const;
    BossTargetScore selectBossPersonalTargetScore( Room& room, const PlatoonLeader& leader ) const;
    bool        calcBossPersonalTargetScore( Room& room, const PlatoonLeader& leader, uint32 targetId, float& outScore ) const;
    GameSession* resolveBossPersonalTarget( Room& room, uint32 targetId ) const;
    void MU_CALLCONV moveBossToward( PlatoonLeader& leader, mu::Vec3 targetPos, float speedMult ) const;
    GameSession* selectPrimaryTarget( Room& room, const PlatoonLeader& leader ) const;
    float        evaluatePlayerScore( const GameSession* s, const PlatoonLeader& leader ) const;
    int32        clusterPlayers( const Room& room, const PlatoonLeader& leader ) const;
    std::vector<PlayerCluster> buildPlayerClusters( const Room& room, const PlatoonLeader& leader ) const;
    void         issueDivideAndConquer( PlatoonLeader& leader,
                                        const std::vector<TacticalSquad*>& liveSquads, const std::vector<PlayerCluster>& clusters );
    void         updateDivideAndConquer( Seconds dt, Room& room, PlatoonLeader& leader );
    uint32       selectReplacementTarget( Room& room, const PlatoonLeader& leader, const std::vector<uint32>& playerIds ) const;
    int32        countLiveMembers( const std::vector<TacticalSquad*>& liveSquads ) const;
    int32        minMembersForEncircle( int32 playerCount ) const;
    bool         canStartEncircle( const std::vector<TacticalSquad*>& liveSquads, const PlayerCluster& cluster ) const;
    float        calcEncircleRadius( int32 liveMembers ) const;
    bool         allMembersArrived( const Room& room, const PlatoonLeader& leader ) const;
    std::vector<mu::Vec3> calcSquadBoxOffsets( int32 numSquads ) const;
    bool         checkTacticsConditions( const PlatoonLeader& leader ) const;

    LeaderPhase      leaderPhase_{ LeaderPhase::BoxAdvance };
    bool             phaseOrderIssued_{ false };
    bool             tacticsUnlocked_{ false };
    bool             initialSizesSet_{ false };
    std::vector<int32> initialSquadSizes_{};
    Seconds          tacticTimer_{};
    Seconds          tacticCooldown_{};
    mu::Vec3         boxAdvanceTargetPos_{};
    mu::Vec3         retreatTargetPos_{};
    uint32           primaryTargetId_{ 0 };
    int32            encircleIssuedLiveMembers_{ 0 };
    std::vector<DivideSquadTask> divideTasks_{};
    BossPersonalState bossPersonalState_{ BossPersonalState::EvaluateTarget };
    Seconds           bossPersonalTimer_{};
    Seconds           bossTargetEvalTimer_{};
    uint32            bossPersonalTargetId_{ 0 };

    static constexpr Seconds TACTIC_INTERVAL{ 1.f };
    static constexpr float CLUSTER_RADIUS                = 20.f;
    static constexpr float ENCIRCLE_RADIUS               = 50.0f;
    static constexpr float ENCIRCLE_MIN_RADIUS           = 18.0f;
    static constexpr float ENCIRCLE_SLOT_SPACING         = 7.5f;
    static constexpr float TACTIC_HP_THRESHOLD           = 0.70f;
    static constexpr float TACTIC_SQUAD_RATIO            = 0.80f;
    static constexpr Seconds TACTIC_COOLDOWN_DURATION{ 8.0f };
    static constexpr Seconds TACTIC_FAIL_COOLDOWN_DURATION{ 5.0f };
    static constexpr Seconds DIVIDE_ENGAGE_PROTECT_DURATION{ 3.0f };
    static constexpr float SCREEN_BLOCK_SPACING          = 8.0f;
    static constexpr float SCREEN_SLOT_SPACING_SCALE     = 0.65f;
    static constexpr float SCREEN_SLOT_COLUMN_SCALE      = 3.0f;
    static constexpr int32 SCREEN_SLOT_COLUMN_COUNT      = 7;
    static constexpr float SCREEN_BLOCK_CENTER_BIAS      = 0.5f;
    static constexpr float BOX_FRONT_OFFSET              = 15.f;
    static constexpr float BOX_SQUAD_SPACING             = 35.f;
    static constexpr float BOX_ARC_DEPTH                 = 10.f;
    static constexpr float REGROUP_DIST                  = 70.f;
    static constexpr float VIGILANCE_GUARD_RADIUS        = 20.f;
    static constexpr float TACTICAL_SPEED_MULT           = 3.f;
    static constexpr Seconds BOSS_TARGET_EVAL_INTERVAL{ 0.5f };
    static constexpr float BOSS_TARGET_SWITCH_MARGIN     = 120.f;
    static constexpr float BOSS_CHASE_SPEED_MULT         = 1.0f;
};

#endif // midboss_tactic_hpp
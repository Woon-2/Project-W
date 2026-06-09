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

    // 회랑(corridor) 포획 전술 단계: 준비 → 돌진 → 교전
    enum class DivideStage : byte { Preparing, Charging, Engaging };

    struct DivideSquadTask {
        TacticalSquad*        squad{ nullptr };
        DivideTaskType        type{ DivideTaskType::None };
        uint32                targetId{ 0 };
        std::vector<uint32>   clusterPlayerIds{};
    };

    struct BossTargetScore {
        uint32   targetId{ 0 };
        float    score{ 0.f };
    };

    void        enterPhase( LeaderPhase next );
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
    bool         issueDivideAndConquer( Room& room, PlatoonLeader& leader,
                                        const std::vector<TacticalSquad*>& liveSquads, const std::vector<PlayerCluster>& clusters );
    void         updateDivideAndConquer( Seconds dt, Room& room, PlatoonLeader& leader );
    void         issueDivideEngage( Room& room, PlatoonLeader& leader );
    void         clearDivideBarriers( Room& room );   // 차단벽 barrier 해제(클라에 off 통보). 비어있으면 no-op
    uint32       selectReplacementTarget( Room& room, const PlatoonLeader& leader, const std::vector<uint32>& playerIds ) const;
    int32        countLiveMembers( const std::vector<TacticalSquad*>& liveSquads ) const;
    int32        minMembersForEncircle( int32 playerCount ) const;
    bool         canStartEncircle( const std::vector<TacticalSquad*>& liveSquads, const PlayerCluster& cluster ) const;
    float        calcEncircleRadius( int32 liveMembers ) const;
    bool         allMembersArrived( const PlatoonLeader& leader ) const;
    bool         formationReady( const PlatoonLeader& leader ) const;
    std::vector<mu::Vec3> calcSquadBoxOffsets( int32 numSquads ) const;
    bool         checkTacticsConditions( const PlatoonLeader& leader ) const;

    LeaderPhase      leaderPhase_{ LeaderPhase::BoxAdvance };
    bool             phaseOrderIssued_{ false };
    bool             tacticsUnlocked_{ false };
    bool             initialSizesSet_{ false };
    std::vector<int32> initialSquadSizes_{};
    Seconds          tacticTimer_{};
    Seconds          tacticCooldown_{};
    Seconds          phaseHoldTimer_{};   // 대형 완성 후 체류(과시) 시간 누적
    Seconds          phaseElapsed_{};     // 현재 단계 누적 시간(타임아웃 폴백용)
    Seconds          divideStageTimer_{}; // DivideAndConquer 스테이지 누적 시간(타임아웃 폴백용)
    mu::Vec3         boxAdvanceTargetPos_{};
    mu::Vec3         retreatTargetPos_{};
    uint32           primaryTargetId_{ 0 };
    int32            encircleIssuedLiveMembers_{ 0 };
    std::vector<DivideSquadTask> divideTasks_{};
    DivideStage      divideStage_{ DivideStage::Preparing };
    mu::Vec3         divideCorridorCenter_{};
    mu::Vec3         divideCorridorForward_{ 1.f, 0.f, 0.f };
    mu::Vec3         divideCorridorRight_{ 0.f, 0.f, 1.f };
    float            divideCorridorHalfWidth_{ 0.f };
    float            divideCorridorHalfLength_{ 0.f };
    Seconds          divideEngageTimer_{};
    std::vector<uint32> divideTargetPlayerIds_{};
    std::vector<uint32> divideBarrierNpcIds_{};   // 차단선 NPC id (형성 완료 시 barrier 토글 / off 송신용)
    bool                divideBarrierOn_{ false };  // barrier on을 실제로 broadcast했는지(off 중복·불필요 송신 방지)
    BossPersonalState bossPersonalState_{ BossPersonalState::EvaluateTarget };
    Seconds           bossPersonalTimer_{};
    Seconds           bossTargetEvalTimer_{};
    uint32            bossPersonalTargetId_{ 0 };

    static constexpr Seconds TACTIC_INTERVAL{ 1.f };
    static constexpr Seconds FORMATION_HOLD_DURATION{ 1.f };   // 후퇴/박스 대형 완성 후 다음 단계 전환 전 체류

    static constexpr float CLUSTER_RADIUS                = 8.f;
    static constexpr float ENCIRCLE_RADIUS               = 20.0f;
    static constexpr float ENCIRCLE_MIN_RADIUS           = 7.0f;
    static constexpr float ENCIRCLE_SLOT_SPACING         = 3.0f;
    static constexpr float TACTIC_HP_THRESHOLD           = 0.70f;
    static constexpr float TACTIC_SQUAD_RATIO            = 0.80f;
    static constexpr Seconds TACTIC_COOLDOWN_DURATION{ 8.0f };
    static constexpr Seconds TACTIC_FAIL_COOLDOWN_DURATION{ 5.0f };
    static constexpr Seconds DIVIDE_ENGAGE_DURATION{ 3.0f };
    static constexpr Seconds FORMATION_TIMEOUT{ 7.0f };       // 박스/포위/경계/후퇴 집결 타임아웃 폴백
    static constexpr Seconds DIVIDE_PREP_TIMEOUT{ 6.0f };     // 쐐기 준비+차단선 형성 타임아웃 폴백
    static constexpr Seconds DIVIDE_CHARGE_TIMEOUT{ 5.0f };   // 쐐기 돌진 완료 타임아웃 폴백
    static constexpr float CAPTURE_CORRIDOR_CLEARANCE    = 2.5f;   // 인게임 스케일 (시뮬 6.0 × ~0.4)
    static constexpr float CAPTURE_WALL_SPACING          = 1.2f;   // 차단선 NPC 중심 간격(m). 플레이어 XZ 반경(~0.4m)이 못 끼는 틈 없는 벽
    static constexpr float CAPTURE_CHARGE_STANDOFF       = 4.0f;   // 차단선 후방 끝에서 쐐기 정점을 더 뒤로 띄우는 여유
    static constexpr float BOX_FRONT_OFFSET              = 6.f;
    static constexpr float BOX_SQUAD_SPACING             = 15.f;
    static constexpr float BOX_ARC_DEPTH                 = 4.f;
    static constexpr float REGROUP_DIST                  = 25.f;
    static constexpr float VIGILANCE_GUARD_RADIUS        = 8.f;
    static constexpr float TACTICAL_SPEED_MULT           = 3.f;
    static constexpr Seconds BOSS_TARGET_EVAL_INTERVAL{ 0.5f };
    static constexpr float BOSS_TARGET_SWITCH_MARGIN     = 120.f;
    static constexpr float BOSS_CHASE_SPEED_MULT         = 1.0f;
};

#endif // midboss_tactic_hpp
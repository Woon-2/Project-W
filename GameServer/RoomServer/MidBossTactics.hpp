#ifndef midboss_tactic_hpp
#define midboss_tactic_hpp

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
    static constexpr float TACTICAL_SPEED_MULT           = 3.f;
    static constexpr Seconds BOSS_TARGET_EVAL_INTERVAL{ 0.5f };
    static constexpr float BOSS_TARGET_SWITCH_MARGIN     = 120.f;
    static constexpr float BOSS_CHASE_SPEED_MULT         = 1.0f;
};

/*-------------------------------
      GrandBaumMidBossTactic
-------------------------------*/

// 중간보스 GrandBaum 전술. 전술은 단 하나(ShieldWall): 보스 HP 66%/33%에서 각 1회 발동.
// 발동 시 슬라임이 보스를 원형으로 감싸(뚫을 수 없는 링) 보스/슬라임이 받는 피해 90% 경감.
// 파훼는 오직 뱀 — 발동 전 원본 뱀 부대를 전멸시키면 발동 스킵, 발동 후 증원 뱀 웨이브를
// 전멸시키면 종료. 평소 보스는 표적 우선순위(Snake>Slime>Nearest) melee만 한다.
// NPCAI sim/MidBossTactics.* 포팅.
class GrandBaumMidBossTactic : public MidBossTacticBase {
public:
    GrandBaumMidBossTactic() = default;
    virtual ~GrandBaumMidBossTactic() = default;

    virtual void update( Seconds dt, Room& room, PlatoonLeader& leader ) override;
    virtual void onLeaderDead( Room& room, PlatoonLeader& leader ) override;

private:
    enum class Phase : byte {
        Engage,
        ShieldWall,
        Cooldown
    };

    enum class SnakeAmbushStage : byte {
        Evasion,
        RetreatingOriginal,
        WaveActive,
        ReturningOriginal
    };

    enum class BossMeleeState : byte {
        AcquireTarget,
        ChaseTarget,
        AttackWindup,
        AttackRecover
    };

    enum class BossTargetPriority : byte {
        None        = 0,
        Nearest     = 1,
        SlimeThreat = 2,
        SnakeThreat = 3
    };

    struct BossTargetChoice {
        uint32             targetId{ 0 };
        BossTargetPriority priority{ BossTargetPriority::None };
    };

    void         enterPhase( Phase next, PlatoonLeader& leader );
    void         updateBossMelee( Seconds dt, Room& room, PlatoonLeader& leader );
    void         resetBossMelee( PlatoonLeader& leader );
    BossTargetChoice selectBossMeleeTarget( Room& room, const PlatoonLeader& leader ) const;
    BossTargetChoice selectOriginalSnakeThreatTarget( Room& room, const PlatoonLeader& leader ) const;
    BossTargetChoice selectSlimeThreatTarget( Room& room, const PlatoonLeader& leader ) const;
    std::vector<uint32> getOriginalSnakeCandidateIds( const PlatoonLeader& leader ) const;
    BossTargetChoice MU_CALLCONV selectNearestPlayerTarget( Room& room, mu::Vec3 center ) const;
    BossTargetChoice MU_CALLCONV selectNearestPlayerNear( Room& room, mu::Vec3 center, float radius, BossTargetPriority priority ) const;
    bool         isCurrentBossMeleeTargetValid( Room& room, const PlatoonLeader& leader ) const;
    bool         isResourceThreatPriority( BossTargetPriority priority ) const;
    mu::Vec3 MU_CALLCONV calcLiveOriginalSnakeCentroid( Room& room, const PlatoonLeader& leader, int32& outLiveCount ) const;
    mu::Vec3 MU_CALLCONV calcLiveSlimeCentroid( Room& room, const PlatoonLeader& leader, int32& outLiveCount ) const;
    void MU_CALLCONV moveBossToward( PlatoonLeader& leader, mu::Vec3 targetPos, float speedMult ) const;
    void         issueEngage( Room& room, PlatoonLeader& leader );
    void         issueShieldWall( Room& room, PlatoonLeader& leader );
    void         updateSnakeAmbush( Seconds dt, Room& room, PlatoonLeader& leader, TacticalSquad* originalSnakeSquad );
    void         updateSnakeEvasion( Seconds dt, Room& room, PlatoonLeader& leader, TacticalSquad* snakeSquad );
    mu::Vec3 MU_CALLCONV pickSnakePersonalWanderTarget( mu::Vec3 center ) const;
    void         issueOriginalSnakeRetreat( Room& room, PlatoonLeader& leader, TacticalSquad* originalSnakeSquad );
    void         spawnSnakeWave( Room& room, PlatoonLeader& leader, TacticalSquad* originalSnakeSquad );
    void         issueSnakeWaveEngage( Room& room, TacticalSquad* waveSquad );
    void         finishShieldWall( Room& room, PlatoonLeader& leader );
    void         cleanupSnakeWave( Room& room );
    void         captureOriginalSnakeRoster( Room& room, TacticalSquad* originalSnakeSquad );
    void         reviveOriginalSnakeSquad( Room& room, PlatoonLeader& leader );
    bool         shouldPreserveOriginalSnakes() const;
    int32        countLiveMembers( Room& room, TacticalSquad* squad ) const;
    int32        countLiveSlimeMembers( Room& room, const PlatoonLeader& leader ) const;
    bool         canFormShieldWall( int32 liveSlimeCount ) const;
    float        calcShieldWallRadius( int32 liveSlimeCount ) const;
    int32        calcSnakeWaveSpawnCount( int32 liveOriginalSnakeCount ) const;
    bool         isSnakeWaveAnnihilated( Room& room ) const;
    TacticalNpcConfig findSnakeConfig( Room& room, TacticalSquad* originalSnakeSquad ) const;
    void         applyShieldWallProtection( Room& room, PlatoonLeader& leader, bool enabled );

    Phase              phase_{ Phase::Engage };
    Seconds            engageRefreshTimer_{};
    Seconds            orderRefreshTimer_{};
    Seconds            snakeRetreatTimer_{};
    Seconds            tacticCooldown_{};
    float              previousHpRatio_{ 1.f };
    int32              shieldWallTriggerStage_{ 0 };
    bool               engageOrderIssued_{ false };
    bool               snakeWaveSpawned_{ false };
    bool               pendingShieldWallTrigger_{ false };
    bool               shieldWallRingIssued_{ false };
    mu::Vec3           shieldWallRingCenter_{};
    float              shieldWallRingRadius_{ 12.f };
    float              shieldWallRingStartAngle_{ 0.f };
    int32              originalSnakeCountAtShieldWall_{ 0 };
    int32              snakeWaveSquadId_{ -1 };
    BossMeleeState     bossMeleeState_{ BossMeleeState::AcquireTarget };
    Seconds            bossMeleeTimer_{};
    Seconds            bossMeleeTargetLockTimer_{};
    Seconds            bossMeleeSamePriorityRetargetTimer_{};
    uint32             bossMeleeTargetId_{ 0 };
    BossTargetPriority bossMeleeTargetPriority_{ BossTargetPriority::None };
    std::vector<uint32>                  originalSnakeRoster_{};
    std::unordered_map<uint32, mu::Vec3> originalSnakeSpawnPositions_{};
    bool                                 snakeWanderCenterSet_{ false };
    mu::Vec3                             snakeWanderCenter_{};
    std::unordered_map<uint32, mu::Vec3> snakePersonalTargets_{};
    std::unordered_map<uint32, Seconds>  snakePersonalTimers_{};
    std::unordered_map<uint32, bool>     snakePersonalEvading_{};
    SnakeAmbushStage                     snakeAmbushStage_{ SnakeAmbushStage::Evasion };
    std::vector<uint32>                  snakeWaveNpcIds_{};

    // ── 상수 ──
    // 월드 거리/반경/슬롯간격은 인게임 스케일 ×~0.4 적용(주석에 시뮬 원본 병기, 실검증 후 미세조정).
    // 시간(Seconds)·비율·카운트·HP 임계는 시뮬 원본 유지.
    static constexpr Seconds ENGAGE_REFRESH_INTERVAL{ 1.0f };
    static constexpr Seconds ORDER_REFRESH_INTERVAL{ 0.5f };
    static constexpr Seconds TACTIC_COOLDOWN_DURATION{ 8.0f };
    static constexpr float FIRST_SHIELD_WALL_HP_RATIO    = 0.66f;
    static constexpr float SECOND_SHIELD_WALL_HP_RATIO   = 0.33f;
    static constexpr float MIN_SHIELD_RING_RADIUS        = 3.f;     // 시뮬 7
    static constexpr float MAX_SHIELD_RING_RADIUS        = 5.f;     // 시뮬 12
    static constexpr float SLIME_RING_SLOT_SPACING       = 1.8f;    // 시뮬 4.5 (반경 계산용)
    static constexpr float SHIELD_RING_MIN_ARC_SPACING   = 1.2f;    // 시뮬 3.0 (슬롯 최소 호 간격)
    static constexpr float SHIELD_RING_LANE_SPACING      = 0.9f;    // 시뮬 2.2 (다중 열 간격)
    static constexpr int32 MIN_SHIELD_WALL_SLIME_COUNT   = 10;
    static constexpr float SHIELDWALL_DAMAGE_MULT        = 0.1f;
    static constexpr float BOSS_CHASE_SPEED_MULT         = 1.0f;   // setDesiredVel motor 방식(시뮬 8.0 직접적분 대체)
    static constexpr Seconds BOSS_TARGET_LOCK_DURATION{ 1.4f };
    static constexpr Seconds BOSS_SAME_PRIORITY_RETARGET_INTERVAL{ 2.5f };
    static constexpr float BOSS_SLIME_THREAT_RANGE       = 5.f;     // 시뮬 12
    static constexpr float SNAKE_OUTER_RADIUS            = 26.f;    // 시뮬 64
    static constexpr float SNAKE_EVASION_RADIUS          = 10.f;    // 시뮬 24
    static constexpr float SNAKE_EVASION_SPEED_MULT      = 0.45f;
    static constexpr float SNAKE_RETREAT_SPEED_MULT      = 1.0f;
    static constexpr Seconds SNAKE_RETREAT_MAX_TIME{ 1.5f };
    static constexpr float SNAKE_DETECT_RANGE            = 7.f;     // 시뮬 18
    static constexpr float SNAKE_STOP_EVADE_RANGE        = 10.f;    // 시뮬 24 (detect < stop_evade 히스테리시스 유지)
    static constexpr float SNAKE_DISPERSE_WANDER_RADIUS  = 10.f;    // 시뮬 24
    static constexpr float SNAKE_PERSONAL_SCATTER_RADIUS = 2.f;     // 시뮬 5
    static constexpr float SNAKE_PERSONAL_MAX_LEASH_RADIUS = 17.f;  // 시뮬 42
    static constexpr float SNAKE_THREAT_WEIGHT_RANGE     = SNAKE_STOP_EVADE_RANGE;
    static constexpr Seconds SNAKE_WANDER_INTERVAL{ 3.5f };
    static constexpr float SNAKE_WANDER_SPEED_MULT       = 0.2f;
    static constexpr Seconds SNAKE_EVASION_REFRESH{ 0.5f };
    static constexpr int32 SNAKE_WAVE_MAX_COUNT          = 60;
    static constexpr int32 SNAKE_WAVE_MULTIPLIER         = 10;
    static constexpr int32 SNAKE_WAVE_SQUAD_ID           = 9003;
};

/*-------------------------------
      IsisMidBossTactic
-------------------------------*/

// 중간보스 Isis 전술. GrandBaum(수동적 생존형)과 대비되는 능동적 섬멸형 — 평소 4스쿼드 분산 교전 +
// 보스 자체 근접전을 하다가, 어느 한 스쿼드든 초기 인원 80% 미만으로 깎이면 전군을 후방으로 빼 집결시킨 뒤
// Bomber로 1차 쐐기 → Buddy+보스로 2차 쐐기(피해 ×1.5)를 "다른 군집"에 꽂는 2연속 쐐기 협공을 반복한다.
// 부대 계약: squad[0]/[1]=Buddy, squad[2]/[3]=Bomber, 보스 본체=PlatoonLeader.
// 기존 인프라 재사용: WedgeCharge(+wedgeDamageMult/reserveWedgeApex)·FormationHold·issueStableEngage·
// buildPlayerClusters. NPCAI sim/MidBossTactics.* 포팅.
class IsisMidBossTactic : public MidBossTacticBase {
public:
    IsisMidBossTactic() = default;
    virtual ~IsisMidBossTactic() = default;

    virtual void update( Seconds dt, Room& room, PlatoonLeader& leader ) override;
    virtual void onLeaderDead( Room& room, PlatoonLeader& leader ) override;

private:
    enum class Phase : byte {
        Engage,
        RetreatForPincer,
        RegroupBombers,
        FirstBomberWedge,
        RegroupBuddies,
        SecondBuddyWedge,
        Cooldown
    };

    // 보스 자체 근접 FSM(Engage/Cooldown 중). Acquire/Chase/Windup/Recover는 Goblin 미러,
    // Backstep/Retreat는 피해 반응 이탈(누적 피해 임계 초과 시).
    enum class BossPersonalState : byte {
        EvaluateTarget,
        ChaseTarget,
        AttackWindup,
        AttackRecover,
        Backstep,
        Retreat
    };

    struct BossTargetScore {
        uint32 targetId{ 0 };
        float  score{ 0.f };
    };

    // 협공 표적 군집(buildPlayerClusters + 점수). 2차 쐐기는 1차 타깃과 겹치면 페널티.
    struct StrikeCluster {
        PlayerCluster cluster{};
        float         score{ 0.f };
    };

    // 진행 중인 쐐기 1건(쐐기 → 일반 교전 전환 추적).
    struct StrikeTask {
        TacticalSquad*       squad{ nullptr };
        std::vector<uint32>  targetIds{};
        bool                 engageIssued{ false };
    };

    void    enterPhase( Phase next, PlatoonLeader& leader );
    void    enterCooldown( PlatoonLeader& leader );
    bool    checkUnlockCondition( const PlatoonLeader& leader ) const;
    void    captureInitialSquadSizes( const PlatoonLeader& leader );
    void    issueEngage( Room& room, PlatoonLeader& leader, bool resetAssignments );

    // ── 협공 사이클(2연속 쐐기) ──
    void    issueRetreatForPincer( Room& room, PlatoonLeader& leader );
    void    issueRegroupBombers( Room& room, PlatoonLeader& leader );
    void    issueRegroupBuddies( Room& room, PlatoonLeader& leader );
    void    issueWedgeStrike( Room& room, PlatoonLeader& leader,
                              bool useBuddySquads, bool applyRepeatPenalty, bool rememberTargets );
    void    issueBomberRegroup( Room& room, TacticalSquad* squad, const StrikeCluster& strikeCluster, float sideSign );
    void MU_CALLCONV issueBuddyColumn( Room& room, TacticalSquad* squad, const StrikeCluster& strikeCluster, float sideSign );
    std::vector<StrikeCluster> selectStrikeClusters( Room& room, const PlatoonLeader& leader, bool applyRepeatPenalty = false ) const;
    GameSession* selectPrimaryTarget( Room& room, const PlatoonLeader& leader ) const;
    uint32  selectStrikeEngageTarget( Room& room, const PlatoonLeader& leader, const StrikeTask& task ) const;
    void    updateActiveStrikeEngage( Room& room, PlatoonLeader& leader, bool forceAll );
    Seconds rollCooldown();
    bool    hasLiveBomberSquad( const PlatoonLeader& leader ) const;
    bool    hasLiveBuddySquad( const PlatoonLeader& leader ) const;
    bool    allLiveSquadsAtSlots( const PlatoonLeader& leader ) const;
    bool    activeStrikeSquadsAtSlots() const;
    bool    activeStrikeTasksEngaged() const;

    // ── 보스 본체의 2차 쐐기 합류 ──
    void    selectBossJoinedBuddySquad( const PlatoonLeader& leader );
    void    resetBossBuddyWedgeJoin();
    bool    isBossJoinedBuddySquad( const TacticalSquad* squad ) const;
    bool    ensureBossBuddyWedgeJoin( Room& room, const PlatoonLeader& leader );
    bool    isBossBuddyWedgeJoinReady( const PlatoonLeader& leader ) const;
    bool    areSecondStrikePrepSquadsAtSlots() const;
    bool    isSecondStrikePrepReady( const PlatoonLeader& leader ) const;
    void MU_CALLCONV setupBossBuddyWedgeJoin( TacticalSquad* squad, const StrikeCluster& strikeCluster, mu::Vec3 squadCenter );
    void    syncBossBuddyWedgeChargeStart( const PlatoonLeader& leader );
    void    updateBossBuddyWedgeJoin( Seconds dt, PlatoonLeader& leader );

    // 보스 근접 전투 — Goblin updateBossPersonalCombat 미러 + 피해 반응 Backstep/Retreat 추가.
    bool         updateBossPersonalCombat( Seconds dt, Room& room, PlatoonLeader& leader );
    void         updateBossDamageReaction( Seconds dt, const PlatoonLeader& leader );
    void         resetBossPersonalCombat( PlatoonLeader& leader );
    void         beginBossBackstep( Room& room, PlatoonLeader& leader );
    uint32       selectBossPersonalTarget( Room& room, const PlatoonLeader& leader ) const;
    BossTargetScore selectBossPersonalTargetScore( Room& room, const PlatoonLeader& leader ) const;
    bool         calcBossPersonalTargetScore( Room& room, const PlatoonLeader& leader, uint32 targetId, float& outScore ) const;
    GameSession* resolveBossPersonalTarget( Room& room, uint32 targetId ) const;
    void MU_CALLCONV moveBossToward( PlatoonLeader& leader, mu::Vec3 targetPos, float speedMult ) const;

    Phase              phase_{ Phase::Engage };
    bool               initialSizesSet_{ false };
    bool               tacticsUnlocked_{ false };
    bool               engageOrderIssued_{ false };
    bool               pincerIssued_{ false };
    Seconds            engageRefreshTimer_{};
    Seconds            phaseTimer_{};
    Seconds            cooldownTimer_{};
    std::vector<int32> initialSquadSizes_{};

    // 협공 상태
    mu::Vec3                     retreatTargetPos_{};
    std::vector<TacticalSquad*>  activeStrikeSquads_{};
    std::vector<StrikeTask>      activeStrikeTasks_{};
    std::vector<uint32>          firstStrikeTargetIds_{};      // 1차(Bomber) 타깃 — 2차 페널티 기준
    bool                         secondStrikePrepIssued_{ false };
    std::vector<TacticalSquad*>  secondStrikePrepSquads_{};
    std::vector<StrikeCluster>   secondStrikeClusters_{};

    // 보스 2차 쐐기 합류
    int32     bossJoinedBuddySquadIndex_{ -1 };
    bool      bossBuddyWedgeJoinActive_{ false };
    bool      bossBuddyWedgeChargeStarted_{ false };
    bool      bossBuddyWedgeChargeComplete_{ false };
    mu::Vec3  bossBuddyWedgePreparePos_{};
    mu::Vec3  bossBuddyWedgeExitPos_{};
    mu::Vec3  bossBuddyWedgeDir_{ 1.f, 0.f, 0.f };

    // 보스 근접 전투 상태
    BossPersonalState  bossPersonalState_{ BossPersonalState::EvaluateTarget };
    Seconds            bossPersonalTimer_{};
    Seconds            bossTargetEvalTimer_{};
    uint32             bossPersonalTargetId_{ 0 };
    mu::Vec3           bossBackstepTargetPos_{};
    bool               bossHpTracked_{ false };
    float              previousBossHp_{ 0.f };
    float              bossDamageSinceBackstep_{ 0.f };
    Seconds            bossBackstepCooldownTimer_{};

    // ── 상수 ──
    // 월드 거리/반경은 인게임 스케일 ×~0.4(주석에 시뮬 원본 병기, 실검증 후 미세조정). 시간/비율/카운트/
    // 점수 임계는 시뮬 원본 유지. 보스 고속 이동은 setDesiredVel 모터라 시뮬의 직접적분 배율을 그대로
    // 못 쓰므로 인게임값으로 캡(주석에 시뮬 원본 병기, M3 튜닝).
    static constexpr float   UNLOCK_SQUAD_RATIO           = 0.80f;
    static constexpr float   CLUSTER_RADIUS               = 8.f;     // 시뮬 20
    static constexpr Seconds ENGAGE_REFRESH_INTERVAL{ 1.0f };
    static constexpr Seconds BOSS_TARGET_EVAL_INTERVAL{ 0.5f };
    static constexpr float   BOSS_TARGET_SWITCH_MARGIN    = 120.f;
    static constexpr float   BOSS_CHASE_SPEED_MULT        = 1.0f;    // 모터(시뮬 5.35 직접적분 대체)
    static constexpr float   BOSS_DAMAGE_REACTION_THRESHOLD = 60.f;
    static constexpr Seconds BOSS_BACKSTEP_COOLDOWN{ 3.0f };
    static constexpr float   BOSS_BACKSTEP_ARRIVE_DIST    = 0.5f;    // 시뮬 1.0
    static constexpr float   BOSS_BACKSTEP_DIST           = 7.f;     // 시뮬 18
    static constexpr float   BOSS_BACKSTEP_SPEED_MULT     = 4.0f;    // 모터 캡(시뮬 20)
    static constexpr float   BOSS_RETREAT_DIST            = 11.f;    // 시뮬 28
    static constexpr float   BOSS_RETREAT_SPEED_MULT      = 2.0f;    // 모터 캡(시뮬 5.35)

    // ── 협공 사이클 상수 ──
    // 시간/비율/카운트/점수/spacing은 시뮬 원본 유지. 월드 거리/오프셋은 인게임 스케일 ×~0.4(주석 병기).
    // 보스 고속 이동(후퇴/합류/돌진)은 모터라 시뮬 배율을 못 써 인게임값으로 캡(주석 병기, M3 튜닝).
    static constexpr float   MIN_COOLDOWN                 = 7.0f;    // 초(rollCooldown)
    static constexpr float   MAX_COOLDOWN                 = 13.0f;   // 초
    static constexpr Seconds RETREAT_TIMEOUT{ 5.0f };
    static constexpr Seconds REGROUP_TIMEOUT{ 3.5f };
    static constexpr Seconds PINCER_TIMEOUT{ 7.0f };
    static constexpr float   ISIS_RETREAT_EXTRA_DIST      = 14.f;    // 시뮬 35
    static constexpr float   ISIS_RETREAT_MIN_DIST        = 36.f;    // 시뮬 90
    static constexpr float   ISIS_RETREAT_ARRIVE_DIST     = 1.5f;    // 보스 후퇴 도착 판정
    static constexpr float   RETREAT_SPEED_MULT           = 1.15f;   // squad 멤버(비율 유지)
    static constexpr float   RETREAT_LEADER_SPEED_MULT    = 6.0f;    // 모터 캡(시뮬 15.5)
    static constexpr float   RETREAT_BOMBER_FRONT_OFFSET  = 7.f;     // 시뮬 18
    static constexpr float   RETREAT_BOMBER_SIDE_OFFSET   = 8.f;     // 시뮬 20
    static constexpr float   RETREAT_BUDDY_BACK_OFFSET    = 5.f;     // 시뮬 12
    static constexpr float   RETREAT_BUDDY_SIDE_OFFSET    = 11.f;    // 시뮬 28
    static constexpr float   BOMBER_REGROUP_SPEED_MULT    = 0.75f;
    static constexpr float   BOMBER_REGROUP_BACK_OFFSET   = 11.f;    // 시뮬 28
    static constexpr float   BOMBER_REGROUP_SIDE_OFFSET   = 9.f;     // 시뮬 22
    static constexpr float   BOMBER_REGROUP_SPACING_SCALE = 0.9f;
    static constexpr float   BOMBER_REGROUP_COLUMN_SCALE  = 1.8f;
    static constexpr int32   BOMBER_REGROUP_COLUMN_COUNT  = 0;
    static constexpr float   BUDDY_COLUMN_SPACING_SCALE   = 0.85f;
    static constexpr float   BUDDY_COLUMN_SCALE           = 1.0f;
    static constexpr int32   BUDDY_COLUMN_COUNT           = 2;
    static constexpr float   BUDDY_SPEED_MULT             = 0.75f;
    static constexpr float   ISIS_WEDGE_SPEED_MULT        = 1.50f;   // squad 쐐기 돌진(chargeSpeedMult)
    static constexpr float   ISIS_BUDDY_WEDGE_SPACING_MULT = 1.90f;
    static constexpr float   ISIS_BOSS_JOINED_WEDGE_DAMAGE_MULT = 1.50f;   // 2차 결정타 ×1.5(wedgeDamageMult)
    static constexpr float   ISIS_BOSS_WEDGE_JOIN_SPEED_MULT  = 6.0f;  // 모터 캡(시뮬 15.5)
    static constexpr float   ISIS_BOSS_WEDGE_CHARGE_SPEED_MULT = 8.0f; // 모터 캡(시뮬 28)
    static constexpr float   ISIS_BOSS_WEDGE_JOIN_READY_DIST  = 1.5f;
    static constexpr float   ISIS_BOSS_WEDGE_PREP_ARRIVE_DIST = 1.0f;
    static constexpr float   ISIS_BOSS_WEDGE_CHARGE_ARRIVE_DIST = 0.75f;
    static constexpr float   SECOND_STRIKE_REPEAT_PENALTY = 350.0f;
};

#endif // midboss_tactic_hpp
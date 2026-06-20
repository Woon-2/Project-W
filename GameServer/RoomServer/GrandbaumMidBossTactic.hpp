#ifndef grandbaum_midboss_tactic_hpp
#define grandbaum_midboss_tactic_hpp

#include "MidBossTacticBase.hpp"

/*-------------------------------
      GrandbaumMidBossTactic
-------------------------------*/

// 중간보스 Grandbaum 전술. 전술은 단 하나(ShieldWall): 보스 HP 66%/33%에서 각 1회 발동.
// 발동 시 슬라임이 보스를 원형으로 감싸(뚫을 수 없는 링) 보스/슬라임이 받는 피해 90% 경감.
// 파훼는 오직 뱀 — 발동 전 원본 뱀 부대를 전멸시키면 발동 스킵, 발동 후 증원 뱀 웨이브를
// 전멸시키면 종료. 평소 보스는 표적 우선순위(Snake>Slime>Nearest) melee만 한다.
// NPCAI sim/MidBossTactics.* 포팅.
class GrandbaumMidBossTactic : public MidBossTacticBase {
public:
    GrandbaumMidBossTactic() = default;
    virtual ~GrandbaumMidBossTactic() = default;

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
    // 후퇴 완료한 원본 뱀을 전장에서 퇴장(숨김)시킨다. 웨이브 동안 노출 방지, 종료 시 reviveOriginalSnakeSquad로 복귀.
    void         despawnOriginalSnakeSquad( Room& room );
    void         captureOriginalSnakeRoster( Room& room, TacticalSquad* originalSnakeSquad );
    void         reviveOriginalSnakeSquad( Room& room, PlatoonLeader& leader );
    bool         shouldPreserveOriginalSnakes() const;
    int32        countLiveMembers( Room& room, TacticalSquad* squad ) const;
    int32        countLiveSlimeMembers( Room& room, const PlatoonLeader& leader ) const;
    bool         canFormShieldWall( int32 liveSlimeCount ) const;
    // 슬라임 부대(0,1,2) 중 살아있는 부대가 모두 슬롯 도착(areMembersAtSlots)이면 true = 방패벽 형성 완료.
    bool         isShieldWallFormed( Room& room, PlatoonLeader& leader ) const;
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
    bool               shieldWallFormed_{ false };       // 벽 형성 완료 → 형성 중 재넉백 중단(슬라임 barrier가 인계)
    Seconds            shieldWallFormTimer_{};            // 형성 경과(안전 타임아웃용)
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
    // 방패벽 형성 가속(RingGuard 오더 speedMult). 전투 속도(slime moveSpeed 2.5)는 불변, 형성 접근만 ×2 (≈15 m/s).
    static constexpr float SHIELD_WALL_APPROACH_SPEED_MULT = 2.0f;
    // 형성 중 재넉백 안전 타임아웃: 형성이 비정상 지연돼도 이 시간 후엔 재넉백 중단(플레이어 영구 락 방지).
    static constexpr Seconds SHIELD_WALL_FORM_KNOCK_MAX{ 6.0f };
    static constexpr float BOSS_CHASE_SPEED_MULT         = 1.8f;   // setDesiredVel motor 방식(시뮬 8.0 직접적분 대체). 인게임 튜닝
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

#endif // grandbaum_midboss_tactic_hpp

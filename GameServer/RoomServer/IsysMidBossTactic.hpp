#ifndef isis_midboss_tactic_hpp
#define isis_midboss_tactic_hpp

#include "MidBossTacticBase.hpp"

/*-------------------------------
      IsysMidBossTactic
-------------------------------*/

// 중간보스 Isys 전술. Grandbaum(수동적 생존형)과 대비되는 능동적 섬멸형 — 평소 4스쿼드 분산 교전 +
// 보스 자체 근접전을 하다가, 어느 한 스쿼드든 초기 인원 80% 미만으로 깎이면 전군을 후방으로 빼 집결시킨 뒤
// Bomber로 1차 쐐기 → Buddy+보스로 2차 쐐기(피해 ×1.5)를 "다른 군집"에 꽂는 2연속 쐐기 협공을 반복한다.
// 부대 계약: squad[0]/[1]=Buddy, squad[2]/[3]=Bomber, 보스 본체=PlatoonLeader.
// 기존 인프라 재사용: WedgeCharge(+wedgeDamageMult/reserveWedgeApex)·FormationHold·issueStableEngage·
// buildPlayerClusters. NPCAI sim/MidBossTactics.* 포팅.
class IsysMidBossTactic : public MidBossTacticBase {
public:
    IsysMidBossTactic() = default;
    virtual ~IsysMidBossTactic() = default;

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

    void    enterPhase( Phase next, Room& room, PlatoonLeader& leader );
    void    enterCooldown( Room& room, PlatoonLeader& leader );
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
    bool    forceActiveWedgeCharges( Room& room );
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
    bool    ensureBossBuddyWedgeJoin( Room& room, PlatoonLeader& leader );
    bool    isBossBuddyWedgeJoinReady( const PlatoonLeader& leader ) const;
    bool    areSecondStrikePrepSquadsAtSlots() const;
    bool    isSecondStrikePrepReady( const PlatoonLeader& leader ) const;
    void MU_CALLCONV setupBossBuddyWedgeJoin( Room& room, PlatoonLeader& leader,
                                              TacticalSquad* squad, const StrikeCluster& strikeCluster,
                                              mu::Vec3 squadCenter );
    mu::Vec3 MU_CALLCONV findSafeBossFormationPosition(
        Room& room, const PlatoonLeader& leader, mu::Vec3 desired ) const;
    void    syncBossBuddyWedgeChargeStart( const PlatoonLeader& leader );
    void    updateBossBuddyWedgeJoin( Seconds dt, PlatoonLeader& leader );
    void    applyBossWedgeMotorBoost( PlatoonLeader& leader );
    void    restoreBossWedgeMotorBoost( PlatoonLeader& leader );

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
    bool               wedgeForceStartIssued_{ false };
    Seconds            engageRefreshTimer_{};
    Seconds            phaseTimer_{};
    Seconds            cooldownTimer_{};
    std::vector<int32> initialSquadSizes_{};

    // 협공 상태
    mu::Vec3                     retreatTargetPos_{};
    // 집결 좌표계의 공용 전진축(후퇴 지점 → 플레이어 군집). Bomber/Buddy 집결 위치를 전부 이
    // 축 하나로 계산해야 두 대형의 전후 간격이 보장된다 — 부대별 타깃 군집 방향을 쓰면 군집이
    // 벌어질수록 축이 어긋나 서로 겹친다. 바라보는 방향(formationTargetPos)은 부대별로 유지.
    mu::Vec3                     retreatForwardDir_{ 1.f, 0.f, 0.f };
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
    bool      bossWedgeMotorBoostActive_{ false };
    float     savedBossWedgeMotorAcceleration_{ 20.f };

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
    // 비율/카운트/점수/spacing은 시뮬 원본 유지. 쿨다운은 게임 템포에 맞춘 별도 튜닝값.
    // 월드 거리/오프셋은 인게임 스케일 ×~0.4(주석 병기).
    // 보스 고속 이동(후퇴/합류/돌진)은 모터라 시뮬 배율을 못 써 인게임값으로 캡(주석 병기, M3 튜닝).
    static constexpr float   MIN_COOLDOWN                 = 15.0f;   // 초(rollCooldown)
    static constexpr float   MAX_COOLDOWN                 = 21.0f;   // 초
    static constexpr Seconds RETREAT_TIMEOUT{ 5.0f };
    static constexpr Seconds REGROUP_TIMEOUT{ 3.5f };
    static constexpr Seconds BUDDY_REGROUP_TIMEOUT{ 4.0f };
    // 쐐기 대형이 완성(85% 안착)되기 전 강제 돌진까지의 유예. 2.5s는 라인→V자 재배치가
    // 끝나기 전에 발동해 쐐기 완성형이 화면에 안 보였다. 자연 완료가 먼저 차면 즉시 출발한다.
    static constexpr Seconds WEDGE_PREP_FORCE_TIMEOUT{ 4.0f };
    // 쐐기 준비(최대 WEDGE_PREP_FORCE_TIMEOUT) + 후퇴 지점에서 군집까지의 돌진·관통 시간을
    // 모두 담아야 한다. 이 타이머가 먼저 차면 돌진 중이던 부대가 Engage로 강제 전환돼 관통이 끊긴다.
    static constexpr Seconds PINCER_TIMEOUT{ 9.0f };
    static constexpr float   ISIS_RETREAT_EXTRA_DIST      = 14.f;    // 시뮬 35
    static constexpr float   ISIS_RETREAT_MIN_DIST        = 36.f;    // 시뮬 90
    static constexpr float   ISIS_RETREAT_ARRIVE_DIST     = 1.5f;    // 보스 후퇴 도착 판정
    static constexpr float   RETREAT_SPEED_MULT           = 1.15f;   // squad 멤버(비율 유지)
    static constexpr float   RETREAT_LEADER_SPEED_MULT    = 6.0f;    // 모터 캡(시뮬 15.5)
    static constexpr float   RETREAT_BOMBER_FRONT_OFFSET  = 7.f;     // 시뮬 18
    static constexpr float   RETREAT_BOMBER_SIDE_OFFSET   = 8.f;     // 시뮬 20
    // Buddy 집결(후퇴·2차 공용)이 집결지 뒤로 물러나는 거리. Bomber 쐐기 준비가 집결지 앞
    // −2.2m까지 뻗으므로, Buddy 열 깊이(±6.4m)를 감안해 앞 끝이 그보다 뒤에 오도록 잡는다.
    static constexpr float   RETREAT_BUDDY_BACK_OFFSET    = 14.f;    // 시뮬 12(전방 겹침으로 상향)
    static constexpr float   RETREAT_BUDDY_SIDE_OFFSET    = 11.f;    // 시뮬 28
    static constexpr float   BOMBER_REGROUP_SPEED_MULT    = 0.75f;
    // (제거) BOMBER_REGROUP_BACK/SIDE_OFFSET — 군집 기준 앵커를 쓰던 시뮬 잔재. 돌격 라인은
    // 이제 후퇴 집결지 기준(RETREAT_BOMBER_*)이다. issueBomberRegroup 참조.
    static constexpr float   BOMBER_REGROUP_SPACING_SCALE = 0.9f;
    // 1.8은 40기 블록을 29.7m 폭으로 퍼뜨려, 부대 중심 간격(RETREAT_BOMBER_SIDE_OFFSET×2 = 16m)
    // 안에서 두 Bomber 부대가 서로 파고들었다(findSafeFormationSlot은 같은 부대 슬롯끼리만 본다).
    // 1.0이면 16.2m 폭 × 13.5m 깊이 — 부대끼리 안 겹치고, 이어질 쐐기(18m × 13.2m)와 모양이
    // 비슷해 V자 재배치 이동도 짧아진다.
    static constexpr float   BOMBER_REGROUP_COLUMN_SCALE  = 1.0f;
    static constexpr int32   BOMBER_REGROUP_COLUMN_COUNT  = 0;
    static constexpr float   BUDDY_COLUMN_SPACING_SCALE   = 0.85f;
    static constexpr float   BUDDY_COLUMN_SCALE           = 1.0f;
    static constexpr int32   BUDDY_COLUMN_COUNT           = 2;
    static constexpr float   BUDDY_SPEED_MULT             = 0.75f;
    static constexpr float   ISIS_WEDGE_SPEED_MULT        = 2.0f;    // Buddy 24m/s, Bomber 30m/s
    static constexpr float   ISIS_WEDGE_MOTOR_ACCELERATION = 60.f;
    static constexpr float   ISIS_BUDDY_WEDGE_SPACING_MULT = 1.90f;
    static constexpr float   ISIS_BOSS_JOINED_WEDGE_DAMAGE_MULT = 1.50f;   // 2차 결정타 ×1.5(wedgeDamageMult)
    static constexpr float   ISIS_BOSS_WEDGE_JOIN_SPEED_MULT  = 6.0f;  // 모터 캡(시뮬 15.5)
    static constexpr float   ISIS_BOSS_WEDGE_CHARGE_SPEED_MULT = 10.0f;
    static constexpr float   ISIS_BOSS_WEDGE_JOIN_READY_DIST  = 1.5f;
    static constexpr float   ISIS_BOSS_WEDGE_PREP_ARRIVE_DIST = 1.0f;
    static constexpr float   ISIS_BOSS_WEDGE_CHARGE_ARRIVE_DIST = 0.75f;
    static constexpr float   SECOND_STRIKE_REPEAT_PENALTY = 350.0f;
};

#endif // isis_midboss_tactic_hpp

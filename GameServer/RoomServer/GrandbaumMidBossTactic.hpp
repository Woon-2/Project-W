#ifndef grandbaum_midboss_tactic_hpp
#define grandbaum_midboss_tactic_hpp

#include "MidBossTacticBase.hpp"

/*-------------------------------
      GrandbaumMidBossTactic
-------------------------------*/

// 중간보스 Grandbaum 전술 — "피해경감(DR) 토글" 방식.
//  - 평상시: 슬라임 받는 피해 90% 경감(단단)/보스 취약 → 플레이어가 보스를 공격하도록 유도(보스가 먼저 죽음).
//    평상시 슬라임이 거의 안 죽으므로 2페이즈용 슬라임이 보존된다.
//  - 보스 HP 66%/33%에서 ShieldWall 발동: 플레이어 넉백 + 슬라임이 보스를 원형 벽(RingGuard, 하드 블로커)으로
//    보호 + 보스 받는 피해 90% 경감. 단 이때만 슬라임 경감을 풀어(취약) 벽 슬라임을 처치할 수 있게 한다.
//  - 파훼: 벽 슬라임을 일정 수(SHIELD_BREAK_KILL_FRACTION) 처치해야만 평상시로 복귀.
//  - 보스 사망: 잔여(혼란 상태) 슬라임 경감 영구 해제 → 깔끔히 정리되어 아레나 벽 해제.
// 런타임 NPC/스쿼드 생성·삭제는 일절 없다(이전 뱀 웨이브/숨김/부활 구조 제거 — 크래시 원인).
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

    enum class BossMeleeState : byte {
        AcquireTarget,
        ChaseTarget,
        AttackWindup,
        AttackRecover
    };

    void  enterPhase( Phase next, PlatoonLeader& leader );

    // ── 피해경감(DR) 프로파일 ──
    void  applyNormalDamageProfile( Room& room, PlatoonLeader& leader );          // 보스 취약 / 슬라임 단단(0.1)
    void  applyShieldWallProtection( Room& room, PlatoonLeader& leader );         // 보스 보호(0.1) / 슬라임 취약 + 하드 블로커
    void  setAllSlimeDamageMultiplier( Room& room, PlatoonLeader& leader, float m ) const;

    // ── 보스 melee ──
    void  updateBossMelee( Seconds dt, Room& room, PlatoonLeader& leader );
    void  resetBossMelee( PlatoonLeader& leader );
    void MU_CALLCONV moveBossToward( PlatoonLeader& leader, mu::Vec3 targetPos, float speedMult ) const;

    // ── 부대 명령 ──
    void  issueEngage( Room& room, PlatoonLeader& leader );
    void  issueShieldWall( Room& room, PlatoonLeader& leader );
    bool  tryConsumePendingShieldWall( Room& room, PlatoonLeader& leader );
    void  finishShieldWall( Room& room, PlatoonLeader& leader );

    // ── 집계/판정 ──
    int32 countLiveMembers( Room& room, TacticalSquad* squad ) const;
    int32 countLiveSlimeMembers( Room& room, PlatoonLeader& leader ) const;
    int32 countLiveSlimeSquads( Room& room, PlatoonLeader& leader ) const;
    bool  canFormShieldWall( int32 liveSlimeCount ) const;
    bool  isShieldWallFormed( Room& room, PlatoonLeader& leader ) const;
    float calcShieldWallRadius( int32 liveSlimeCount, int32 liveSquadCount ) const;

    Phase          phase_{ Phase::Engage };
    bool           damageProfileInitialized_{ false };
    Seconds        engageRefreshTimer_{};
    Seconds        orderRefreshTimer_{};
    Seconds        tacticCooldown_{};
    int32          shieldWallReachedStage_{ 0 };    // 지금까지 도달한 최고 HP 단계(회복해도 감소하지 않음)
    int32          shieldWallConsumedStage_{ 0 };   // 이미 발동 또는 스킵으로 소비한 HP 단계
    bool           engageOrderIssued_{ false };
    bool           shieldWallRingIssued_{ false };
    bool           shieldWallFormed_{ false };       // 벽 형성 완료 → 형성 중 재넉백 중단(슬라임 barrier가 인계)
    Seconds        shieldWallFormTimer_{};           // 형성 경과(안전 타임아웃용)
    mu::Vec3       shieldWallRingCenter_{};
    float          shieldWallRingRadius_{ 12.f };
    float          shieldWallRingStartAngle_{ 0.f };
    int32          shieldWallSlimeCountAtForm_{ 0 }; // 형성 시점 생존 슬라임 수(파훼 임계 기준)
    BossMeleeState bossMeleeState_{ BossMeleeState::AcquireTarget };
    Seconds        bossMeleeTimer_{};
    uint32         bossMeleeTargetId_{ 0 };

    // ── 상수 ──
    // 월드 거리/반경/슬롯간격은 인게임 스케일 적용. 시간(Seconds)·비율·HP 임계는 설계값.
    static constexpr Seconds ENGAGE_REFRESH_INTERVAL{ 1.0f };
    static constexpr Seconds ORDER_REFRESH_INTERVAL{ 0.5f };
    static constexpr Seconds TACTIC_COOLDOWN_DURATION{ 8.0f };
    static constexpr float FIRST_SHIELD_WALL_HP_RATIO    = 0.66f;
    static constexpr float SECOND_SHIELD_WALL_HP_RATIO   = 0.33f;
    static constexpr float MIN_SHIELD_RING_RADIUS        = 3.f;
    // 반경 = 보스로부터 슬라임 벽까지 거리. 80마리는 최대 반경 8에서 8.0/6.6/5.2의 3겹으로 배치한다.
    static constexpr float MAX_SHIELD_RING_RADIUS        = 8.f;
    // ShieldWall 중 슬라임 상호 물리 충돌은 비활성이다. 시각적 겹침은 줄이면서 barrier 연결거리(2.9)보다
    // 충분히 촘촘하도록 원주·방사 방향 모두 1.4 간격으로 배치한다.
    static constexpr float SHIELD_RING_MIN_ARC_SPACING   = 1.4f;
    static constexpr float SHIELD_RING_LANE_SPACING      = 1.4f;
    // 클라이언트 barrier 연결 거리 2.9보다 여유 있게 각 링의 최대 원주 간격을 제한한다.
    static constexpr float SHIELD_RING_MAX_ARC_SPACING   = 2.8f;
    static constexpr float SHIELD_RING_RADIUS_SEARCH_STEP = 0.05f;
    static constexpr int32 MIN_SHIELD_WALL_SLIME_COUNT   = 10;     // 이 미만이면 ShieldWall 스킵
    static constexpr float SHIELDWALL_BOSS_DAMAGE_MULT   = 0.1f;   // 발동 중 보스 받는 피해 90% 경감
    static constexpr float SHIELDWALL_SLIME_DAMAGE_MULT  = 0.5f;   // 발동 중 슬라임 받는 피해 50% 경감
    static constexpr float SLIME_NORMAL_DAMAGE_MULT      = 0.0f;   // 평상시 슬라임 무적
    // 방패벽 형성 가속(RingGuard 오더 speedMult). 전투 속도는 불변, 형성 접근만 가속.
    static constexpr float SHIELD_WALL_APPROACH_SPEED_MULT = 2.0f;
    // 형성 중 재넉백 안전 타임아웃: 형성이 비정상 지연돼도 이 시간 후엔 재넉백 중단(플레이어 영구 락 방지).
    static constexpr Seconds SHIELD_WALL_FORM_KNOCK_MAX{ 6.0f };
    static constexpr float BOSS_CHASE_SPEED_MULT         = 1.8f;
    // 파훼: 형성 시점 슬라임의 이 비율 이상을 처치하면 ShieldWall 종료.
    static constexpr float SHIELD_BREAK_KILL_FRACTION    = 0.2f;
};

#endif // grandbaum_midboss_tactic_hpp

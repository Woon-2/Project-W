#ifndef tactical_npc_hpp
#define tactical_npc_hpp

#include "object.hpp"
#include <vector>
#include <cstdint>
#include <string>

class Room;
class GameSession;

/*--------------------------
      TacticalNpcState
--------------------------*/

enum class TacticalNpcState {
    Idle          = 0,
    Chase         = 1,
    AttackWindup  = 2,
    AttackRecover = 3,
    Flank         = 4,
    ChargeThrough = 5,
    Confused      = 6,
    Dead          = 7,
    HoldSlot      = 8,
    PressureWait  = 9,
};

/*-------------------------
      TacticalCommand
-------------------------*/

enum class TacticalCommandType {
    None,
    EngageTarget,
    FlankTarget,
    Idle,
    Confused,
    HoldSlot,
    GuardSlot,
    ChargeThrough,
};

struct TacticalCommand {
    TacticalCommandType type             = TacticalCommandType::None;
    uint32              targetId         = 0;
    mu::Vec3            slotOffset       = {};
    mu::Vec3            slotRefTargetPos = {};
    float               abandonDist      = 15.f;
    float               speedMult        = 1.f;
    std::vector<uint32> targetIds        = {};
    uint32              chargeId         = 0;
    mu::Vec3            chargeDir        = {};
    mu::Vec3            chargeCenter     = {};
    float               impactRadius     = 3.f;
    float               impactDamage     = 0.f;
    float               passDistance     = 6.f;
    float               chargeAcceleration = 0.f;   // 0이면 기존 모터 가속 유지
    bool                useHoldFacing    = false;
    mu::Vec3            holdFacing       = {};
};

/*---------------------------
      TacticalNpcConfig
---------------------------*/

struct TacticalNpcConfig {
    float maxHp             = 100.f;
    float moveSpeed         = 4.f;
    // Ground speed the Walk clip was authored for (m/s). Drives the server's locomotion
    // playback rate so bone poses match the client's foot-slide correction.
    //
    // This is a property of the *clip set*, not of the entity: variants that reuse another
    // monster's anims must carry that monster's value regardless of their own moveSpeed
    // (Hobgoblin->Goblin, Grandbaum->Treant, Isys->Birdy; see the objType switch in
    // Room::setupTacticalNpc*). Must equal the matching client constant in
    // client/object.cpp (AnimBlenderX::update, kRefSpeedWalk).
    //
    // Note tactical NPCs travel at moveSpeed * TACTICAL_SPEED_MULT, far above any authored
    // walk speed -- the rate clamp absorbs the excess; the residual slide needs a run clip.
    float animRefSpeed      = 3.f;
    // Speed at which the client's idle->walk blend weight saturates. Every monster blender
    // uses walkThreshold(0.06) + 3.f; only the boss rig differs.
    float animBandEnd       = 3.06f;
    float attackRange       = 2.8f;
    float attackDamage      = 15.f;
    Seconds attackWindupTime{ 0.4f };
    Seconds attackRecoverTime{ 0.8f };
    float separationRadius  = 3.f;
    float separationWeight  = 0.5f;
    // Multiplier applied to skill-based attack damage (oh.damage * coeff * scale).
    // Troopers stay at 1.0; bosses raise this so a baseline monster skill still hits hard.
    float attackDamageScale = 1.f;
};

// A skill-based attack option for a tactical NPC: the skill asset to cast plus the
// server anim clip key to play, so the hitbox bone transforms track the skill timeline.
// Mirrors Npc::NpcAttack (kept separate per the explicit per-class pattern).
struct TacticalNpcAttack {
    uint32      skillId = 0;
    std::string clipKey;
};

/*---------------------------------
      TacticalNpcUpdateResult
---------------------------------*/

struct TacticalNpcUpdateResult {
    struct HitInfo {
        uint16 targetId;
        int32 newHp;
    };

    std::vector<HitInfo> hits;
    bool justDied = false;   // 이번 틱에 사망 전이됨 → 시체 바디를 물리 충돌에서 제거하는 신호
};

/*---------------------
      TacticalNpc
---------------------*/

class TacticalNpc : public Object {
public:
    TacticalNpc() = default;
    TacticalNpc( Object&& base, const TacticalNpcConfig& cfg = {} );
	virtual ~TacticalNpc() = default;

    TacticalNpcUpdateResult update( Seconds dt, Room& room );

    void receiveCommand( const TacticalCommand& cmd );

    TacticalNpcState  getState()             const { return state_; }
    TacticalNpcState  getDisplayState()      const;
    uint32            getTargetId()          const { return targetId_; }
    int32             getSquadId()           const { return squadId_; }
    mu::Vec3          getSpawnPos()          const { return spawnPos_; }
    mu::Vec3          getAssignedSlot()      const { return assignedSlot_; }
    float             getAttackRange()       const { return attackRange_; }
    // 공격 슬롯 예약 자격: 타깃과의 거리 + (스테일로 막힌 경우) 접근 진척 기반.
    // Room::tryReserveTacticalAttackSlot에서 후보를 거를 때 호출하므로 public.
    bool MU_CALLCONV isEligibleForAttackReservation( uint32 targetId, mu::Vec3 targetPos ) const;
    float             getAttackDamage()      const { return attackDamage_; }
    int32             maxHp()                const { return static_cast<int32>(maxHp_); }
    float             getSeparationRadius()  const { return separationRadius_; }
    TacticalNpcConfig getConfig()            const;
    bool              isAtSlot()             const;
    // HoldSlot에서 실제 안착 래치까지 걸린 상태인지 확인한다.
    // 거리만 넓게 허용하는 isAtSlot()과 달리 밀집 대형의 정착 완료 판정에 사용한다.
    bool              isSettledAtSlot()       const { return state_ == TacticalNpcState::HoldSlot && slotSettled_; }
    bool              isChargeComplete()     const { return chargeComplete_; }

    void setSquadId( int32 id ) { squadId_ = id; }
    // Per-NPC client model identity. The encounter sets this so S_NpcSpawnBatch tells each
    // client which monster model to render (e.g. a Grandbaum squad spawns as Slime/Snake, not Goblin).
    ObjectType objType()                const { return objType_; }
    void       setObjType( ObjectType t )     { objType_ = t; }
    void MU_CALLCONV reviveAt( mu::Vec3 pos );

    // Skill-based attack roster (mirrors Npc). registerTacticalNpcBody populates this
    // per objType so a tactical NPC casts the same varied attacks as its field counterpart.
    void addAttack( uint32 skillId, std::string clipKey );   // skillId==0 -> skipped (keeps legacy fallback)
    bool hasSkillAttacks() const { return !attacks_.empty(); }
    void setAttackDamageScale( float s ) { attackDamageScale_ = s; }

protected:
    void applyConfig( const TacticalNpcConfig& cfg );
    void transitionTo( TacticalNpcState next );

    void updateIdle         ();
    void updateChase        ( Seconds dt, Room& room );
    void updateAttackWindup ( Seconds dt, Room& room );
    void updateAttackRecover( Seconds dt, Room& room );
    void updateFlank        ( Seconds dt, Room& room );
    void updateChargeThrough( Room& room );
    void updateConfused     ( Seconds dt, Room& room );
    void updateHoldSlot     ( Room& room );
    // 슬롯 이동 방향(slotDir)을 큰 장애물(플레이어/보스) 회피로 옆으로 보정. en route에서만 동작.
    void MU_CALLCONV applyBlockerAvoidance( Room& room, mu::Vec3& slotDir, float distToSlot );
    // 이웃 전술 NPC(peer) 회피로 이동 방향을 보정. 정면(head-on) 교착은 일관된 측면(veer-right) bias로 해소.
    void MU_CALLCONV applyPeerSeparation( Room& room, mu::Vec3& moveDir, float radius );
    // 슬롯 안착 래치 갱신(히스테리시스). inner 진입 시 안착, outer 이탈 시 해제. true면 안착(모터 제동 유지).
    bool MU_CALLCONV updateSlotSettled( float distToSlot );
    // 안착 처리: 모터 정지 + 잔여 XZ 속도 감쇠로 솔버 유발 미세 진동을 흡수(Y는 중력 보존).
    void MU_CALLCONV settleAtSlot();
    void updatePressureWait ( Seconds dt, Room& room );
    void updateDead         ( Room& room );

    void        consumePendingCommand( Room& room );
    GameSession* resolveTarget( Room& room ) const;
    bool        hasReservedAttackSlot() const;
    void        updateReservedAttackStaleTimer( Seconds dt, Room& room );
    bool        canEnterAttackSlot( Room& room );
    void        releaseAttackReservation( Room& room );
    void        resetAttackReservationLease();
    // 예약이 STALE_TIME 동안 진척 없이 끊길 때: 슬롯 반납 + 해당 타깃을 blocked로 표시.
    void        releaseStaleAttackReservation( Room& room, float currentDist );
    void        resetPressureWaitTarget();
    void        restoreChargeMotorAcceleration();
    void        refreshPressureWaitScatterOffsets();
    mu::Vec3    computePressureWaitDesired( mu::Vec3 targetPos, mu::Vec3 targetFacing ) const;
    void        moveTowardPressureWait( Seconds dt, Room& room, mu::Vec3 targetPos, mu::Vec3 targetFacing );
    void        recordHit( uint16 targetId, int32 newHp );
    // Uniform-random pick over the registered skill attacks (caller must ensure non-empty).
    const TacticalNpcAttack& pickAttack() const;

    TacticalNpcState state_{ TacticalNpcState::Idle };
    TacticalCommand  pendingCmd_{};
    uint32           targetId_{ 0 };
    mu::Vec3         assignedSlot_{};
    mu::Vec3         slotRefTargetPos_{};
    float            abandonDist_{ 15.f };
    uint32           chargeId_{ 0 };
    mu::Vec3         chargeDir_{};
    mu::Vec3         chargeCenter_{};
    float            impactRadius_{ 3.f };
    float            impactDamage_{ 0.f };
    float            passDistance_{ 6.f };
    bool             chargeComplete_{ false };
    bool             guardNearestPlayer_{ false };
    bool             useHoldFacing_{ false };
    mu::Vec3         holdFacing_{};
    mu::Vec3         confusedAnchor_{};
    mu::Vec3         confusedTarget_{};
    Seconds          confusedRetargetTimer_{};
    int32            confusedWanderStep_{ 0 };
    mu::Vec3         spawnPos_{};
    int32            squadId_{ -1 };
    ObjectType       objType_{ ObjectType::Goblin };   // client render model identity (default Goblin)

    float maxHp_{ 100.f };
    float moveSpeed_{ 4.f };
    float attackRange_{ 2.8f };
    float attackDamage_{ 15.f };
    Seconds attackWindupTime_{ 0.4f };
    Seconds attackRecoverTime_{ 0.8f };
    float separationRadius_{ 3.f };
    float separationWeight_{ 0.5f };

    std::vector<TacticalNpcAttack> attacks_;          // skill-based attacks (empty => legacy melee)
    bool                           attackCast_{ false };   // one skill cast per windup
    float                          attackDamageScale_{ 1.f };

    float speedMult_{ 1.f };
    bool  slotSettled_{ false };   // 슬롯 안착 래치(히스테리시스): true면 모터 제동 유지(떨림 방지)
    Seconds windupTimer_{};
    Seconds recoverTimer_{};
    Seconds pressureWaitTimer_{};
    mu::Vec3 pressureWaitDesired_{};
    mu::Vec3 pressureWaitTargetAnchor_{};
    mu::Vec3 pressureWaitFacingAnchor_{ 1.f, 0.f, 0.f };
    Seconds pressureWaitRetargetTimer_{};
    float pressureWaitAngleOffset_{ 0.f };
    float pressureWaitRadiusOffset_{ 0.f };
    uint32 pressureWaitScatterSeed_{ 0 };
    bool  pressureWaitDesiredValid_{ false };
    bool  pressureReentering_{ false };
    bool  chargeMotorOverrideActive_{ false };
    float savedChargeMotorAcceleration_{ 20.f };
    uint32 reservedAttackTargetId_{ 0 };
    Seconds reservedAttackStaleTimer_{};
    // 직전 틱까지의 타깃 접근 거리(진척 추적용). 일정량 접근하면 예약 리스를 갱신한다.
    float reservedAttackProgressDist_{ 0.f };
    // 스테일로 예약이 끊긴 타깃 — 이 타깃에는 PROGRESS_DIST만큼 더 접근해야 재예약 자격이 생긴다.
    uint32 blockedAttackReservationTargetId_{ 0 };
    float blockedAttackReservationDist_{ 0.f };

    std::vector<mu::Vec3>                  nearbyCache_;
    std::vector<TacticalNpcUpdateResult::HitInfo> frameHits_;

    static constexpr float TACTICAL_SPEED_MULT                        = 3.0f;
    // 슬롯(HoldSlot) 도착 감속: 이 반경 안에서 접근 가속을 걷어내고 속도를 0으로 수렴시킨다.
    static constexpr float TACTICAL_SLOT_ARRIVE_SLOW_RADIUS           = 1.5f;
    // 슬롯 중심 0.5m 이내에서는 모터를 정지시킨다. 슬라임 separationRadius(1.5)의 1/3에 해당한다.
    static constexpr float TACTICAL_SLOT_SETTLE_RADIUS                = 0.5f;
    // 슬롯 안착 래치(히스테리시스): settle 반경 안으로 들어오면 안착(settled)→모터 제동 유지, outer 밖으로
    // 밀려나야 래치 해제→재접근. 밀집 Dynamic 대형의 솔버 평형 변위에 모터가 재당김하는 한계 진동 방지.
    static constexpr float TACTICAL_SLOT_UNSETTLE_RADIUS_MULT         = 0.7f;
    // 안착 중 잔여 XZ 속도 감쇠 계수(솔버가 주입한 미세 속도를 흡수, Y는 중력 보존).
    static constexpr float TACTICAL_SLOT_SETTLE_VEL_DAMP              = 0.3f;
    // 슬롯 "도착" 판정 거리 배율(separationRadius_ 기준). 밀집 Dynamic 대형은 NPC끼리 물리적으로
    // 막혀 슬롯 2~3.5m에서 멈추므로, 도착 판정을 그만큼 넓혀 대형 완성(allMembersArrived)이 성립하게 한다.
    static constexpr float TACTICAL_SLOT_ARRIVE_MULT                  = 1.5f;
    // 슬롯 이동(HoldSlot) 중 큰 장애물(플레이어/보스)을 옆으로 우회하는 회피 반경.
    static constexpr float TACTICAL_BLOCKER_AVOID_RADIUS              = 3.5f;
    // 정면(head-on) 교착 해소: 이웃이 진행방향과 거의 반대(이 임계 초과)면 일관된 측면 bias를 준다.
    static constexpr float HEADON_DOT_THRESHOLD                       = 0.6f;
    static constexpr float HEADON_BIAS                                = 1.0f;
    static constexpr int   MAX_TACTICAL_ATTACKERS_PER_TARGET          = 5;
    static constexpr float TACTICAL_ATTACK_RESERVATION_MAX_DIST       = 8.0f;
    static constexpr Seconds TACTICAL_ATTACK_RESERVATION_STALE_TIME{ 3.0f };
    // 예약 갱신/재진입에 필요한 최소 접근량(히스테리시스). attackRange_(2.8) 대비 작게 둬
    // chase↔pressure 진동을 막는다. 시뮬레이터(NPCAI)의 거리+접근진척 예약 방식 포팅값.
    static constexpr float TACTICAL_ATTACK_RESERVATION_PROGRESS_DIST  = 0.4f;
    static constexpr float TACTICAL_PRESSURE_EXTRA_RADIUS             = 4.0f;
    static constexpr float TACTICAL_PRESSURE_SEPARATION_MULT          = 2.2f;
    static constexpr float TACTICAL_PRESSURE_SEPARATION_RADIUS_MULT   = 1.35f;
    static constexpr float TACTICAL_PRESSURE_SPEED_MULT               = 0.9f;
    static constexpr Seconds TACTICAL_PRESSURE_REENTER_MIN_TIME{ 0.45f };
    static constexpr Seconds TACTICAL_PRESSURE_REENTER_STAGGER{ 0.12f };
    static constexpr float TACTICAL_PRESSURE_REENTER_SPEED_MULT       = 1.0f;
    static constexpr Seconds TACTICAL_PRESSURE_RETARGET_INTERVAL{ 0.35f };
    static constexpr float TACTICAL_PRESSURE_TARGET_MOVE_REFRESH_DIST = 1.5f;
    static constexpr float TACTICAL_PRESSURE_FACING_REFRESH_DOT       = 0.9396926f;
    static constexpr float TACTICAL_PRESSURE_SLOW_RADIUS              = 4.0f;
    static constexpr float TACTICAL_PRESSURE_STOP_RADIUS              = 1.0f;
    static constexpr float TACTICAL_PRESSURE_SEPARATION_WEIGHT_MULT   = 0.35f;
    static constexpr float TACTICAL_PRESSURE_NEAR_SEPARATION_MIN_SCALE= 0.45f;
    static constexpr float TACTICAL_PRESSURE_OVERLAP_RADIUS_MIN       = 4.0f;
    static constexpr float TACTICAL_PRESSURE_OVERLAP_RADIUS_MULT      = 1.4f;
    static constexpr float TACTICAL_PRESSURE_OVERLAP_DRIFT_MULT       = 0.34f;
    static constexpr float TACTICAL_PRESSURE_FRONT_GAP_DEGREES        = 35.0f;
    static constexpr float TACTICAL_PRESSURE_RADIUS_OFFSET_MIN        = -1.0f;
    static constexpr float TACTICAL_PRESSURE_RADIUS_OFFSET_SPAN       = 3.0f;
    static constexpr float TACTICAL_RECOVER_SEPARATION_DRIFT_MULT     = 0.03f;
    static constexpr float CONFUSED_WANDER_RADIUS                     = 15.0f;
    static constexpr float CONFUSED_TARGET_MIN_RADIUS_MULT            = 0.5f;
    static constexpr float CONFUSED_SEPARATION_RADIUS                 = 4.0f;
    static constexpr float CONFUSED_SEPARATION_WEIGHT                 = 0.55f;
    static constexpr float CONFUSED_SPEED_MULT                        = 2.f;
    static constexpr float CONFUSED_MAX_SPEED                         = 8.f;
    static constexpr Seconds CONFUSED_RETARGET_MIN{ 0.6f };
    static constexpr Seconds CONFUSED_RETARGET_SPAN{ 0.8f };
};

#endif // tactical_npc_hpp

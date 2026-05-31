#ifndef tactical_npc_hpp
#define tactical_npc_hpp

#include "object.hpp"
#include <vector>
#include <cstdint>

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
    bool                useHoldFacing    = false;
    mu::Vec3            holdFacing       = {};
};

/*---------------------------
      TacticalNpcConfig
---------------------------*/

struct TacticalNpcConfig {
    float maxHp             = 100.f;
    float moveSpeed         = 4.f;
    float attackRange       = 2.8f;
    float attackDamage      = 15.f;
    Seconds attackWindupTime{ 0.4f };
    Seconds attackRecoverTime{ 0.8f };
    float separationRadius  = 3.f;
    float separationWeight  = 0.5f;
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
    float             getAttackDamage()      const { return attackDamage_; }
    float             getSeparationRadius()  const { return separationRadius_; }
    TacticalNpcConfig getConfig()            const;
    bool              isAtSlot()             const;
    bool              isChargeComplete()     const { return chargeComplete_; }

    void setSquadId( int32 id ) { squadId_ = id; }
    void MU_CALLCONV reviveAt( mu::Vec3 pos );

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
    void updatePressureWait ( Seconds dt, Room& room );
    void updateDead         ( Room& room );

    void        consumePendingCommand( Room& room );
    GameSession* resolveTarget( Room& room ) const;
    bool        hasReservedAttackSlot() const;
    void        updateReservedAttackStaleTimer( Seconds dt, Room& room );
    bool        canEnterAttackSlot( Room& room );
    void        releaseAttackReservation( Room& room );
    void        resetPressureWaitTarget();
    void        refreshPressureWaitScatterOffsets();
    mu::Vec3    computePressureWaitDesired( mu::Vec3 targetPos, mu::Vec3 targetFacing ) const;
    void        moveTowardPressureWait( Seconds dt, Room& room, mu::Vec3 targetPos, mu::Vec3 targetFacing );
    void        recordHit( uint16 targetId, int32 newHp );

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

    float maxHp_{ 100.f };
    float moveSpeed_{ 4.f };
    float attackRange_{ 2.8f };
    float attackDamage_{ 15.f };
    Seconds attackWindupTime_{ 0.4f };
    Seconds attackRecoverTime_{ 0.8f };
    float separationRadius_{ 3.f };
    float separationWeight_{ 0.5f };

    float speedMult_{ 1.f };
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
    uint32 reservedAttackTargetId_{ 0 };
    Seconds reservedAttackStaleTimer_{};

    std::vector<mu::Vec3>                  nearbyCache_;
    std::vector<TacticalNpcUpdateResult::HitInfo> frameHits_;

    static constexpr float TACTICAL_SPEED_MULT                        = 3.0f;
    static constexpr int   MAX_TACTICAL_ATTACKERS_PER_TARGET          = 5;
    static constexpr float TACTICAL_ATTACK_RESERVATION_MAX_DIST       = 18.0f;
    static constexpr Seconds TACTICAL_ATTACK_RESERVATION_STALE_TIME{ 3.0f };
    static constexpr float TACTICAL_PRESSURE_EXTRA_RADIUS             = 9.0f;
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
    static constexpr float TACTICAL_PRESSURE_RADIUS_OFFSET_SPAN       = 7.0f;
    static constexpr float TACTICAL_RECOVER_SEPARATION_DRIFT_MULT     = 0.03f;
    static constexpr float CONFUSED_WANDER_RADIUS                     = 100.0f;
    static constexpr float CONFUSED_SEPARATION_RADIUS                 = 6.0f;
    static constexpr float CONFUSED_SEPARATION_WEIGHT                 = 0.55f;
    static constexpr float CONFUSED_SPEED_MULT                        = 1.f;
    static constexpr Seconds CONFUSED_RETARGET_MIN{ 0.35f };
    static constexpr Seconds CONFUSED_RETARGET_SPAN{ 0.75f };
};

#endif // tactical_npc_hpp
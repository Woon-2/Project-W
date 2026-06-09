#ifndef tactical_squad_hpp
#define tactical_squad_hpp

#include "TacticalNpc.hpp"
#include <vector>
#include <cstdint>

class Room;

/*------------------------
      SquadOrderType
------------------------*/

enum class SquadOrderType {
    Idle,
    Engage,
    Encircle,
    DenseHold,
    BoxAdvance,
    GuardBoss,
    RetreatFormUp,
    WedgeCharge,
    FormationHold,
    FormationGuard,
    RingGuard,
    DistributedEngage,
};

/*--------------------
      SquadOrder
--------------------*/

struct SquadOrder {
    SquadOrderType type              = SquadOrderType::Idle;
    uint32         targetId          = 0;
    float          slotSpacingScale  = 1.f;
    float          slotColumnScale   = 1.f;
    int32          slotColumnCount   = 0;
    float          sectorAngle       = 0.f;
    float          sectorSpan        = 0.f;
    float          approachRadius    = 5.f;
    mu::Vec3       leaderPos         = {};
    mu::Vec3       sectorPos         = {};
    mu::Vec3       formationTargetPos = {};
    mu::Vec3       tacticCenter      = {};
    float          speedMult         = 1.f;
    float          chargeSpeedMult   = 0.f;
    float          wedgeSpacingMult  = 0.f;
    float          wedgeDamageMult   = 1.f;
    bool           reserveWedgeApex  = false;
    bool           waitForChargeRelease = false; // WedgeCharge only: 준비된 쐐기를 release 전까지 대기
    mu::Vec3       wedgeApexPos      = {};        // WedgeCharge only: 준비 정점을 명시 지정(회랑 입구)
    bool           hasWedgeApex      = false;     // true면 부대 위치 기준 대신 wedgeApexPos 사용
    std::vector<uint32> targetIds  = {};
};

/*-----------------------
      TacticalSquad
-----------------------*/

class TacticalSquad {
public:
    static constexpr float WEDGE_EXIT_DISTANCE      = 14.f;
    static constexpr float WEDGE_PREP_APEX_DISTANCE = 4.f;
    static constexpr float WEDGE_PASS_DISTANCE      = 6.f;
    static constexpr float WEDGE_IMPACT_RADIUS      = 3.f;
    static constexpr float WEDGE_SPEED_MULT         = 1.35f;
    static constexpr float WEDGE_CHARGE_DAMAGE      = 35.f;
    // 도착/완료 판정을 전원 대신 이 비율 이상으로 완화(끼인 소수 멤버로 인한 단계 stall 방지).
    static constexpr float SLOT_ARRIVE_FRACTION     = 0.85f;

    TacticalSquad( int32 squadId, float memberAttackRange, float memberSeparationRadius );

    void addMember( TacticalNpc* npc );
    void removeMember( uint32 npcId );

    void receiveOrder( const SquadOrder& order );
    void update( Seconds dt, Room& room );

    int32 getSquadId() const { return squadId_; }
    const std::vector<uint32>& getMembers() const { return memberIds_; }
    const std::vector<TacticalNpc*>& getMemberCache() const { return memberCache_; }
    bool isEmpty() const { return memberIds_.empty(); }

    void     pushConfusedToMembers( Room& room );
    void     removeDeadMembers();
    void MU_CALLCONV updateBoxLeaderPos( mu::Vec3 pos );
    mu::Vec3 calcCentroid() const;
    bool     areMembersAtSlots() const;
    bool     areChargeMembersComplete() const;
    bool     isWedgeChargeActive() const { return activeWedgeChargeId_ != 0; }
    bool     isWedgePrepared() const { return wedgePrepared_; }
    void     endActiveWedgeCharge( Room& room );
    void     releaseWedgeCharge();
    float    estimateWedgeHalfWidth( float spacingMult = 1.f, bool reserveApex = false ) const;

private:
    void     pushCommandsToMembers( Room& room );
    void     updateLeaderlessBrawl( Seconds dt, Room& room );
    uint32   selectNearestPlayerToSquad( Room& room ) const;

    std::vector<mu::Vec3> MU_CALLCONV calcEncircleSlots( mu::Vec3 targetPos, float sectorAngle, float sectorSpan, float radius, int32 count ) const;
    std::vector<mu::Vec3> MU_CALLCONV calcDenseSlots( mu::Vec3 center, mu::Vec3 forward, int32 count, float spacingScale = 1.f, float columnScale = 1.f, int32 fixedColumnCount = 0 ) const;
    std::vector<mu::Vec3> MU_CALLCONV calcWedgeSlots( mu::Vec3 apex, mu::Vec3 forward, int32 count, float spacingMult ) const;

    int32 squadId_;
    float memberAttackRange_;
    float memberSeparationRadius_;

    std::vector<uint32>       memberIds_;
    std::vector<TacticalNpc*> memberCache_;
    SquadOrder                currentOrder_{};
    bool                      orderDirty_{ false };
    bool                      wedgePrepared_{ false };
    std::vector<uint32>       wedgeMemberIds_{};
    std::vector<TacticalNpc*> wedgeMemberCache_{};
    std::vector<mu::Vec3>     wedgePrepareSlots_{};
    std::vector<mu::Vec3>     wedgeExitSlots_{};
    uint32                    activeWedgeChargeId_{ 0 };
    bool                      wedgeChargeReleased_{ true };
    bool                      leaderlessBrawlEnabled_{ false };
    Seconds                   leaderlessBrawlTimer_{};
    Seconds                   leaderlessRetargetTimer_{};
    Seconds                   boxRefreshTimer_{};

    static constexpr Seconds LEADERLESS_CONFUSED_DURATION{ 6.0f };
    static constexpr Seconds LEADERLESS_RETARGET_INTERVAL{ 1.5f };
};

#endif // tactical_squad_hpp
#ifndef platoon_leader_hpp
#define platoon_leader_hpp

#include "TacticalNpc.hpp"
#include "TacticalSquad.hpp"
#include "IMidBossTactic.hpp"
#include <memory>
#include <vector>

class Room;
class GameSession;

class PlatoonLeader : public TacticalNpc {
public:
    PlatoonLeader( Object&& base, const TacticalNpcConfig& cfg = {} );
    PlatoonLeader( Object&& base, const TacticalNpcConfig& cfg, std::unique_ptr<IMidBossTactic> tactic );
    virtual ~PlatoonLeader() = default;

    TacticalNpcUpdateResult update( Seconds dt, Room& room );

    void addSquad( TacticalSquad* squad );
    const std::vector<TacticalSquad*>& getSquads() const { return squads_; }
    void setTactic( std::unique_ptr<IMidBossTactic> tactic );

    void removeDeadMembersFromSquads();
    void pushConfusedToSquads( Room& room );
    void setTacticalTarget( uint32 targetId ) { targetId_ = targetId; }
    void transitionTacticalState( TacticalNpcState next ) { transitionTo( next ); }
    float getLeaderMoveSpeed() const { return moveSpeed_; }

    void MU_CALLCONV setFacing( mu::Vec3 dir );
    void             applyHitToSession( GameSession* session, float damage );

private:
    std::vector<TacticalSquad*>     squads_;
    std::unique_ptr<IMidBossTactic> tactic_;
    bool                            deathReported_{ false };
};

#endif // platoon_leader_hpp
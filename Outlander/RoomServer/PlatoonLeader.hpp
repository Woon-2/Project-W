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
    PlatoonLeader( Object&& base, const TacticalNpcConfig& cfg, std::unique_ptr<IMidBossTactic> tactic );
    virtual ~PlatoonLeader() = default;

    TacticalNpcUpdateResult update( Seconds dt, Room& room );

    // Mid-boss (Hobgoblin / Grandbaum / Isys): 5 gems.
    int32 rollGemDropCount() override { return 5; }

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
    // Casts a random skill attack (boss reuses its base-monster skill roster + damageScale).
    // Returns false if no skills are registered, so the tactic falls back to direct-damage melee.
    bool             castSkillAttack( Room& room );
    // Casts one NAMED skill with its cast anchor planted at anchorPos (targeted ground skills).
    // Unlike castSkillAttack this bypasses the random roster and takes an explicit damageScale,
    // so a dedicated boss skill can author its own damage instead of inheriting the boss's
    // attackDamageScale. Returns false only when skillId is 0 (name lookup failed).
    bool MU_CALLCONV castSkillAt( Room& room, uint32 skillId, std::string_view clipKey,
                                  mu::Vec3 anchorPos, float damageScale );

private:
    std::vector<TacticalSquad*>     squads_;
    std::unique_ptr<IMidBossTactic> tactic_;
    bool                            deathReported_{ false };
};

#endif // platoon_leader_hpp
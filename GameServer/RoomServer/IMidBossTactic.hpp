#ifndef interface_midboss_tactic_hpp
#define interface_midboss_tactic_hpp

class Room;
class PlatoonLeader;

class IMidBossTactic {
public:
	IMidBossTactic() = default;
    virtual ~IMidBossTactic() = default;

    virtual void update(float dt, Room& room, PlatoonLeader& leader) = 0;
    virtual void onLeaderDead(Room& room, PlatoonLeader& leader)     = 0;
};

#endif // interface_midboss_tactic_hpp
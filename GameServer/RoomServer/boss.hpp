#ifndef boss_hpp
#define boss_hpp

#include "Npc.hpp"

// Final boss: a standalone field NPC (1:1 combat, NOT a tactical/platoon unit).
// Reuses the Npc chase -> AttackWindup -> AttackRecover FSM and skill-based attacks
// (addAttack). The dedicated BehaviorTree AI lands separately; until then the boss
// fights with the plain Npc FSM.
class Boss : public Npc {
public:
    Boss() = default;
    Boss(Object&& base) : Npc(std::move(base)) {}

    void applyBossConfig();
};

#endif // boss_hpp

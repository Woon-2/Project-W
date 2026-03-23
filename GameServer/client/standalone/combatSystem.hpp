#ifndef __StandAlone_combatSystem_HPP
#define __StandAlone_combatSystem_HPP

#include "../object.hpp"
#include "../event.hpp"
#include "../collision.hpp"
#include <unordered_map>

namespace StandAlone {

// Attack parameters per combatant
struct CombatConfig {
	mu::Vec3     attackHalfExtent;  // Half-extents of the attack AABB hitbox
	float        attackOffsetFwd;   // How far forward to place the hitbox center
	int          damage;            // Damage dealt per hit
	Milliseconds cooldown;          // Attack cooldown (used by monster AI)
};

// Combat subsystem - decoupled from Game to maintain single responsibility.
//
// Responsibilities:
//   Player attack  : onPlayerAttack() builds a forward hitbox from the player
//                    and checks overlap against every registered monster -> EvHit
//   Monster AI     : update() ticks each monster's cooldown; when expired and
//                    the monster's hitbox overlaps the player -> EvAttack + EvHit
//
// Extension points (future):
//   - OBB support : virtualise or template overlapsAny()
//   - BVH broadphase : spatial partition for large monster counts
//   - Multi-hitbox  : per-weapon / per-limb hitbox lists
class CombatSystem {
public:
	// Register a combatant using obj->getId() as the key.
	void registerCombatant(Object* obj, CombatConfig config);

	// Remove a combatant by ID.
	void unregister(i32t id);

	// Process an explicit player attack (call on LButton press).
	// Builds a hitbox in front of playerId and emits EvHit for every
	// alive monster whose AABB overlaps it.
	void onPlayerAttack(i32t playerId, EventList& evList);

	// Tick monster AI (call every frame before event processing).
	// Decrements each monster's cooldown and, when expired + overlapping
	// the player, emits EvAttack(monsterId) + EvHit(playerId).
	void update(Milliseconds dt, i32t playerId, EventList& evList);

private:
	struct CombatEntry {
		Object*      obj{};
		CombatConfig config{};
		Milliseconds cooldownRemaining{ 0ms };
	};

	std::unordered_map<i32t, CombatEntry> combatants_;

	// Returns true if hitbox overlaps any AABB in target's physicState.
	static bool overlapsAny(const AABB& hitbox, const Object& target);
};

}   // namespace StandAlone

#endif  // __StandAlone_combatSystem_HPP

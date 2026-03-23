#include "pch.hpp"
#include "combatSystem.hpp"

namespace StandAlone {

void CombatSystem::registerCombatant(Object* obj, CombatConfig config) {
	combatants_[obj->getId()] = CombatEntry{ obj, config, 0ms };
}

void CombatSystem::unregister(i32t id) {
	combatants_.erase(id);
}

bool CombatSystem::overlapsAny(const AABB& hitbox, const Object& target) {
	for (const auto& vol : target.physicState().volumes) {
		if (collides(CollisionVolume{ hitbox }, vol).hit) return true;
	}
	return false;
}

void CombatSystem::onPlayerAttack(i32t playerId, EventList& evList) {
	auto it = combatants_.find(playerId);
	if (it == combatants_.end()) return;

	const auto& attacker = it->second;
	const auto hitbox = buildAttackAABB(
		attacker.obj->pos(), attacker.obj->forward(),
		attacker.config.attackHalfExtent, attacker.config.attackOffsetFwd
	);

	for (auto& [id, entry] : combatants_) {
		if (id == playerId) continue;
		if (entry.obj->hp() <= 0) continue;
		if (overlapsAny(hitbox, *entry.obj)) {
			holdEvent(evList, EvHit(id, std::max(entry.obj->hp() - entry.config.damage, 0)));
		}
	}
}

void CombatSystem::update(Milliseconds dt, i32t playerId, EventList& evList) {
	auto playerIt = combatants_.find(playerId);
	if (playerIt == combatants_.end()) return;

	auto* playerObj = playerIt->second.obj;
	if (playerObj->hp() <= 0) return;

	for (auto& [id, entry] : combatants_) {
		if (id == playerId) continue;
		if (entry.obj->hp() <= 0) continue;

		if (entry.cooldownRemaining > 0ms)
			entry.cooldownRemaining -= dt;

		if (entry.cooldownRemaining <= 0ms) {
			const auto hitbox = buildAttackAABB(
				entry.obj->pos(), entry.obj->forward(),
				entry.config.attackHalfExtent, entry.config.attackOffsetFwd
			);
			if (overlapsAny(hitbox, *playerObj)) {
				holdEvent(evList, EvAttack(id));
				holdEvent(evList, EvHit(playerId, std::max(playerObj->hp() - entry.config.damage, 0)));
				entry.cooldownRemaining = entry.config.cooldown;
			}
		}
	}
}

std::optional<AttackSpec> CombatSystem::queryAttackSpec(i32t id) const {
	auto it = combatants_.find(id);
	if (it == combatants_.end()) return std::nullopt;
	const auto& e = it->second;
	return AttackSpec{ e.obj, e.config.attackHalfExtent, e.config.attackOffsetFwd };
}

}   // namespace StandAlone

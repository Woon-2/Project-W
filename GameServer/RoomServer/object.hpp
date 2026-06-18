#ifndef room_server_object_hpp
#define room_server_object_hpp

#include "protocol.hpp"
#include "physicsWorld.hpp"
#include "terrain.hpp"
#include "serverAnimation.hpp"
#include <vector>
#include <utility>

class GameSession;

struct Model;

// Combat allegiance. A skill hitbox only damages factions in its hostile mask,
// so allies (same faction) never hit each other. Neutral attacks and is hit by
// nothing by default. Faction is set at object creation/registration sites.
enum class Faction : uint8 { Neutral = 0, Players, Monsters };

inline uint32 factionBit(Faction f) { return 1u << static_cast<uint32>(f); }
inline uint32 hostileMask(Faction f) {
	switch (f) {
	case Faction::Players:  return factionBit(Faction::Monsters);
	case Faction::Monsters: return factionBit(Faction::Players);
	default:                return 0u;   // Neutral: attacks nothing
	}
}

class Object {
public:
	void update(Milliseconds deltaTime);

	void setModel(const Model* pModel);

	void MU_CALLCONV setPos(mu::Vec3 newPos);
	mu::Vec3 pos() const { return body_.pos(); }

	void MU_CALLCONV setLinearVel(mu::Vec3 v) { body_.setLinearVel(v); }
	mu::Vec3 linearVel() const { return body_.linearVel(); }

	// velocity()/setVelocity() aliases for backward compatibility
	mu::Vec3 velocity() const { return body_.linearVel(); }
	void MU_CALLCONV setVelocity(mu::Vec3 v) { body_.setLinearVel(v); }

	// Velocity motor: AI declares desired velocity; physics converges toward it.
	void MU_CALLCONV setDesiredVel(mu::Vec3 v) { body_.setDesiredVel(v); }
	mu::Vec3 desiredVel() const { return body_.desiredVel(); }
	void enableMotor(bool v) { body_.enableMotor(v); }

	void MU_CALLCONV setOmega(mu::Vec3 newOmega) { body_.setOmega(newOmega); }
	mu::Vec3 omega() const { return body_.omega(); }

	void MU_CALLCONV setOrient(mu::NQuat newOrient);
	mu::NQuat orient() const { return body_.orient(); }

	void MU_CALLCONV setScale(mu::Vec3 newScale);
	mu::Vec3 scale() const { return body_.scale(); }

	mu::Vec3 forward() const { return forward_; }
	mu::Vec3 right() const { return right_; }
	mu::Vec3 up() const { return up_; }

	RigidBody&       body()       { return body_; }
	const RigidBody& body() const { return body_; }

	// Rebuilds the world-space BVH in body_ from the model's local-space BVH.
	// Called as onRebuildBVH callback by PhysicsWorld, and directly by setters.
	void rebuildBodyBVH();

	// Stores pre-computed world-space bone transforms and rebuilds the BVH.
	// boneWorldXforms[i] = toDress * bakedSample[i][sampleIdx] * entityWorldMatrix
	void updateBoneWorldXforms(std::vector<mu::Mat4x4>&& xforms);
	const std::vector<mu::Mat4x4>& boneWorldXforms() const { return boneWorldXforms_; }

	// Advances the animation clock by dt, computes bone model-space transforms,
	// and rebuilds the world-space BVH. Call once per frame on animated objects.
	void updateAnimBones(Seconds dt);

	AnimController&       animController()       { return animController_; }
	const AnimController& animController() const { return animController_; }

	const Model* model() const { return pModel_; }

	bool canReceiveDamage() const { return canReceiveDamage_; }
	void setCanReceiveDamage(bool v) { canReceiveDamage_ = v; }

	// 받는 피해 배율(기본 1.0). GrandBaum ShieldWall에서 보스/슬라임에 0.1을 걸어
	// 받는 피해를 90% 경감하는 데 사용. 스킬 피격 적용부(Room::updateSkillSystem)가 곱한다.
	float damageTakenMultiplier() const { return damageTakenMultiplier_; }
	void setDamageTakenMultiplier(float m) { damageTakenMultiplier_ = m; }

	Faction faction() const { return faction_; }
	void setFaction(Faction f) { faction_ = f; }

	virtual void onHitImpulse() {}

	void setMaterialSetIdx(uint32 idx) { materialSetIdx_ = idx; }
	uint32 materialSetIdx() const { return materialSetIdx_; }

	void setId(uint32 id) { id_ = id; }
	uint32 getId( ) const { return id_; }

	void setOldPos(float x, float z) {
		oldX_ = x;
		oldZ_ = z;
	}
	float oldX() const { return oldX_; }
	float oldZ() const { return oldZ_; }

	void setCameraPitch(float pitch) { cameraPitch_ = pitch; }
	float cameraPitch() const { return cameraPitch_; }

	void setHp(int32 hp) { hp_ = hp; }
	int32 hp() const { return hp_; }

	// --- Stack-charge: kill-charge reward + recent-damager attribution (monsters) ---
	// killChargeReward_ > 0 marks a chargeable monster; set at spawn from ChargeConfig.
	float killChargeReward() const { return killChargeReward_; }
	void  setKillChargeReward(float v) { killChargeReward_ = v; }

	// Record that a player dealt damage at time `now`. One entry per player
	// (party is tiny); repeat hits just refresh the timestamp.
	void noteDamager(int32 playerId, Milliseconds now) {
		for (auto& d : damagers_) { if (d.first == playerId) { d.second = now; return; } }
		damagers_.push_back({ playerId, now });
	}
	// Append player ids that damaged within [now - window, now] to `out`.
	void collectRecentDamagers(Milliseconds now, Milliseconds window,
	                           std::vector<int32>& out) const {
		for (const auto& d : damagers_)
			if (now - d.second <= window) out.push_back(d.first);
	}
	void clearDamagers() { damagers_.clear(); }

	void setLastMoveTimestamp(uint32 timestamp) { lastMoveTimestamp_ = timestamp; }
	uint32 lastMoveTimestamp() const { return lastMoveTimestamp_; }

	void         setPosUpdateMs(Milliseconds t) { posUpdateMs_ = t; }
	Milliseconds posUpdateMs()            const { return posUpdateMs_; }

	mu::Vec3 estimatedPos(Milliseconds serverNow,
	                      Milliseconds maxWindow = Milliseconds{300.f}) const {
	    float elapsed = (serverNow - posUpdateMs_).count();
	    if (elapsed > maxWindow.count()) elapsed = maxWindow.count();
	    if (elapsed < 0.f) elapsed = 0.f;
	    return pos() + linearVel() * (elapsed / 1000.f);
	}

	void setLastFireTime(Milliseconds time) { lastFireTime_ = time; }
	Milliseconds lastFireTime() const { return lastFireTime_; }
	Milliseconds fireCooldown() const { return fireCooldown_; }

protected:
	mu::Vec3 MU_CALLCONV calcSeparationForce( const std::vector<mu::Vec3>& nearby, float radius ) const;

private:
	float oldX_{};
	float oldZ_{};
	RigidBody body_{};

	mu::Vec3 forward_{};
	mu::Vec3 right_{};
	mu::Vec3 up_{};

	const Model* pModel_ = nullptr;
	AnimController animController_;
	std::vector<mu::Mat4x4> boneWorldXforms_;
	bool canReceiveDamage_ = false;
	float damageTakenMultiplier_ = 1.f;
	Faction faction_ = Faction::Neutral;

	float cameraPitch_{};

	uint32 materialSetIdx_ = 0u;
	int32 id_{ -1 };

	int32 hp_{1'000'000};

	float killChargeReward_ = 0.f;   // charge granted to attackers on this object's death
	std::vector<std::pair<int32, Milliseconds>> damagers_;  // recent (playerId, damage time)

	uint32 lastMoveTimestamp_{0u};

	Milliseconds posUpdateMs_{0ms};

	Milliseconds lastFireTime_{0ms};
	Milliseconds fireCooldown_{200ms};

	bool reloading_{false};
	Milliseconds reloadStartTime_{0ms};
	Milliseconds reloadCooldown_{2000ms};
};

class Player : public Object {
public:
	static constexpr int kSkillSlots = 3;

	Player() = default;
	Player(Object&& base) : Object(std::move(base)) {}

	void setWeaponType(PlayerWeaponType weaponType) { weaponType_ = weaponType; }
	PlayerWeaponType weaponType() const { return weaponType_; }
	void resetSkillState() {
		selectedSlot_ = 0;
		for (int i = 0; i < kSkillSlots; ++i) {
			skillCharge_[i] = 0.f;
			cooldownEnd_[i] = Milliseconds{ 0.f };
		}
		comboCount_ = 0;
		lastCreditMs_ = Milliseconds{ 0.f };
	}

	// --- Stack-charge skill state (server-authoritative) ---
	uint8 selectedSlot() const { return selectedSlot_; }
	void  setSelectedSlot(uint8 s) { selectedSlot_ = (s < kSkillSlots) ? s : 0; }

	float skillCharge(int slot) const {
		return (slot >= 0 && slot < kSkillSlots) ? skillCharge_[slot] : 0.f;
	}
	void setSkillCharge(int slot, float v) {
		if (slot >= 0 && slot < kSkillSlots) skillCharge_[slot] = v;
	}
	void addSkillCharge(int slot, float d) {
		if (slot >= 0 && slot < kSkillSlots) skillCharge_[slot] += d;
	}

	Milliseconds cooldownEnd(int slot) const {
		return (slot >= 0 && slot < kSkillSlots) ? cooldownEnd_[slot] : Milliseconds{ 0.f };
	}
	void setCooldownEnd(int slot, Milliseconds t) {
		if (slot >= 0 && slot < kSkillSlots) cooldownEnd_[slot] = t;
	}

	uint16 comboCount() const { return comboCount_; }
	void   setComboCount(uint16 c) { comboCount_ = c; }
	Milliseconds lastCreditMs() const { return lastCreditMs_; }
	void         setLastCreditMs(Milliseconds t) { lastCreditMs_ = t; }

private:
	PlayerWeaponType weaponType_ = PlayerWeaponType::HeavyArrow;

	uint8        selectedSlot_ = 0;
	float        skillCharge_[kSkillSlots] = { 0.f, 0.f, 0.f };
	Milliseconds cooldownEnd_[kSkillSlots] = {};
	uint16       comboCount_   = 0;
	Milliseconds lastCreditMs_ { 0.f };
};

class Cube : public Object {
public:
	Cube() = default;
	Cube(Object&& base) : Object(std::move(base)) {}
};

class TerrainObject : public Object {
public:
	TerrainObject() = default;
	TerrainObject(Object&& base) : Object(std::move(base)) {}

	// Non-owning: references a shared, read-only height field owned by the
	// boot-time TerrainChunkManager (mirror of the client TerrainObject holding
	// a const TerrainData*). Multiple rooms share the same height field data.
	void                      setHeightField(const TerrainHeightField* hf) { heightField_ = hf; }
	const TerrainHeightField* heightField() const { return heightField_; }

private:
	const TerrainHeightField* heightField_ = nullptr;
};

#endif // room_server_object_hpp

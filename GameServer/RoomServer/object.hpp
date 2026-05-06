#ifndef room_server_object_hpp
#define room_server_object_hpp

#include "physicsWorld.hpp"
#include "terrain.hpp"
#include <vector>

class GameSession;

struct Model;

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

	void setLastMoveTimestamp(uint32 timestamp) { lastMoveTimestamp_ = timestamp; }
	uint32 lastMoveTimestamp() const { return lastMoveTimestamp_; }

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

	float cameraPitch_{};

	uint32 materialSetIdx_ = 0u;
	int32 id_{ -1 };

	int32 hp_{1'000'000};

	uint32 lastMoveTimestamp_{0u};

	Milliseconds lastFireTime_{0ms};
	Milliseconds fireCooldown_{200ms};

	bool reloading_{false};
	Milliseconds reloadStartTime_{0ms};
	Milliseconds reloadCooldown_{2000ms};
};

class Player : public Object {
public:
	Player() = default;
	Player(Object&& base) : Object(std::move(base)) {}
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

	TerrainHeightField&       heightField()       { return heightField_; }
	const TerrainHeightField& heightField() const { return heightField_; }

private:
	TerrainHeightField heightField_;
};

#endif // room_server_object_hpp

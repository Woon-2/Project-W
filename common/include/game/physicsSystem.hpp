#ifndef __PHYSICSSYSTEM_HPP
#define __PHYSICSSYSTEM_HPP

#include "ecs.hpp"
#include "coord.hpp"

#include "keyboardXX.hpp"

#include <DirectXCollision.h>

#include <array>
#include <vector>
#include <numeric>

class RigidBody : public ecs::Component {
public:
	ENABLE_COMPONENT(RigidBody);

	RigidBody(const ecs::Entity& entity) NOEXCEPT;

	void MU_CALLCONV addForce(mu::Vec3 force) NOEXCEPT;
	void MU_CALLCONV updateRigid(float dt, float friction) NOEXCEPT;
	
	void MU_CALLCONV setPosition(mu::Vec3 pos) NOEXCEPT { position_ = pos; }
	const mu::Vec3 MU_CALLCONV position() const NOEXCEPT { return position_; }
	const mu::Vec3 MU_CALLCONV deltaPosition() const NOEXCEPT { return position_ - oldPosition_; }
	const mu::Vec3 MU_CALLCONV force() const NOEXCEPT { return force_; }
	const mu::Vec3 MU_CALLCONV velocity() const NOEXCEPT { return velocity_; }
	float mass() const NOEXCEPT { return mass_; }

private:
	void updateForce(float dt, float friction) NOEXCEPT;
	void updateAngular(float dt) NOEXCEPT;

	mu::Mat4x4 rotation_;
	mu::Mat4x4 inertialMass_;

	mu::Vec3 velocity_;	// v
	mu::Vec3 angVelocity_;	// w
	mu::Vec3 momentum_;	// p
	mu::Vec3 angMomentum_; // L
	mu::Vec3 torque_; // t
	mu::Vec3 force_;
	mu::Vec3 position_;	// curPosition
	mu::Vec3 oldPosition_;
	mu::Vec3 size_;
	std::array<mu::Vec3, 8> corner_;
	
	float mass_;
	int cornerLocation_;
};

class PhysicsSystem : public ecs::System<RigidBody> {
public:
	void update(float deltaTime);
};

struct BoundingCapsule {
	mu::Vec3 base;
	mu::Vec3 tip;
	float radius;
	float height;
};

struct BoundingOrientedBox {
	mu::Vec3 center;
	mu::Vec3 extents;
	mu::NQuat orientation;
};

struct BoundingFrustum {
	mu::Vec3 origin;
	mu::NQuat orientation;
	mu::Radian fovy;
	float aspect;
	float nearZ;
	float farZ;
};

inline mu::Vec3 MU_CALLCONV ClosestPointOnLineSegment(
	mu::Vec3 A, mu::Vec3 B, mu::Vec3 point
) {
	auto AB = B - A;
	float t = mu::dot(point - A, AB) / dot(AB, AB);
	return A + std::clamp(t, 0.f, 1.f) * AB; // saturate(t) can be written as: min((max(t, 0), 1)
}

class Collider {
public:
	enum class Type {
		Capsule,
		Box,
		Frustum
	};

	bool MU_CALLCONV intersects(const Collider& other) const;
	bool MU_CALLCONV contains(const mu::Vec3& point) const;

private:
	Type type_;
	union {
		BoundingCapsule capsule_;
		BoundingOrientedBox box_;
		BoundingFrustum frustum_;
	};
};

class BoundingVolumeNode {
public:

private:
	std::vector<Collider> colliders_;
	std::vector<BoundingVolumeNode> children_;
};

#endif
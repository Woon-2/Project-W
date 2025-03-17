#ifndef __PHYSICSSYSTEM_HPP
#define __PHYSICSSYSTEM_HPP

#include <array>

#include "ecs.hpp"
#include "coord.hpp"

#include "keyboardXX.hpp"

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

#endif
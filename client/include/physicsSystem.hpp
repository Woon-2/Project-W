#ifndef __PHYSICSSYSTEM_HPP
#define __PHYSICSSYSTEM_HPP

#define DXMATH_VEC_UTIL
#define DXMATH_MAT_UTIL
#define DXMATH_QUAT_UTIL

#include <array>

#include "mathUtil.hpp"

#include "keyboardXX.hpp"

class Rigidbody {
public:
	Rigidbody() NOEXCEPT;

	void MU_CALLCONV addForce(mu::Vec3 force) NOEXCEPT;

	void MU_CALLCONV updateRigid(float dt, float friction) NOEXCEPT;

	void MU_CALLCONV setPosition(mu::Vec3 pos) NOEXCEPT { position_ = pos; }

	const mu::Vec3 MU_CALLCONV deltaPosition() const NOEXCEPT { return position_ - oldPosition_; }
	const mu::Vec3 MU_CALLCONV force() const NOEXCEPT { return force_; }
	const mu::Vec3 MU_CALLCONV velocity() const NOEXCEPT { return velocity_; }
	float mass() const NOEXCEPT { return mass_; }

private:
	void updateForce(float dt, float friction) NOEXCEPT;
	void updateAngular(float dt) NOEXCEPT;

private:
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

class PhysicsSystem {
public:
	PhysicsSystem() NOEXCEPT : pKeyboard_(nullptr) {}
	PhysicsSystem(ic::Keyboard& keyboard) NOEXCEPT : pKeyboard_(&keyboard) {}

	void update(float deltaTime);

private:
	ic::Keyboard* pKeyboard_;
};

#endif
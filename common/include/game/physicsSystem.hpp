#ifndef __PHYSICSSYSTEM_HPP
#define __PHYSICSSYSTEM_HPP

#include "ecs.hpp"
#include "coord.hpp"

#include "keyboardXX.hpp"

#include <DirectXCollision.h>

#include <array>
#include <vector>
#include <numeric>
#include <filesystem>
#include <fstream>

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
};

struct BoundingBox {
	mu::Vec3 min;
	mu::Vec3 max;
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
	friend Collider MU_CALLCONV transformCollider(mu::Mat4x4 transform, const Collider& collider);

	enum class Type {
		Capsule,
		Box,
		OrientedBox,
		Frustum
	};

	Collider(const BoundingCapsule& capsule) : type_(Type::Capsule), capsule_(capsule) {}
	Collider(const BoundingBox& box) : type_(Type::Box), aabb_(box) {}
	Collider(const BoundingOrientedBox& box) : type_(Type::OrientedBox), obb_(box) {}
	Collider(const BoundingFrustum& frustum) : type_(Type::Frustum), frustum_(frustum) {}


	bool intersects(const Collider& other) const;
	bool MU_CALLCONV contains(const mu::Vec3 point) const;

private:
	Type type_;
	union {
		BoundingCapsule capsule_;
		BoundingBox aabb_;
		BoundingOrientedBox obb_;
		BoundingFrustum frustum_;
	};
};

Collider MU_CALLCONV transformCollider(mu::Mat4x4 transform, const Collider& collider);
BoundingCapsule MU_CALLCONV transformCollider(mu::Mat4x4 transform, const BoundingCapsule& capsule);
BoundingBox MU_CALLCONV transformCollider(mu::Mat4x4 transform, const BoundingBox& box);
BoundingOrientedBox MU_CALLCONV transformCollider(mu::Mat4x4 transform, const BoundingOrientedBox& box);
BoundingFrustum MU_CALLCONV transformCollider(mu::Mat4x4 transform, const BoundingFrustum& frustum);

class BoundingVolumeNode {
public:
	BoundingVolumeNode() = default;

	BoundingVolumeNode(const BoundingVolumeNode& other)
		: colliders_(other.colliders_), children_() {}

	BoundingVolumeNode& operator=(const BoundingVolumeNode& other) {
		colliders_ = other.colliders_;
		children_.clear();
	}

	BoundingVolumeNode(BoundingVolumeNode&& other) noexcept = default;
	BoundingVolumeNode& operator=(BoundingVolumeNode&& other) noexcept = default;

	void addCollider(const Collider& collider) {
		colliders_.push_back(collider);
	}
	
	void addCollider(const BoundingCapsule& capsule) {
		colliders_.emplace_back(capsule);
	}

	void addCollider(const BoundingOrientedBox& box) {
		colliders_.emplace_back(box);
	}

	void addCollider(const BoundingFrustum& frustum) {
		colliders_.emplace_back(frustum);
	}

	void reserveColliders(std::size_t size) {
		colliders_.reserve(size);
	}

	[[maybe_unused]] BoundingVolumeNode& addChild() {
		return children_.emplace_back();
	}

	void addChild(const BoundingVolumeNode& child) {
		children_.push_back(child);
	}

	void addChild(BoundingVolumeNode&& child) {
		children_.push_back(std::move(child));
	}

	void reserveChildren(std::size_t size) {
		children_.reserve(size);
	}

	bool collides(const BoundingVolumeNode& other) const;

	static bool MU_CALLCONV collides(
		const mu::Mat4x4 lhsTransform, const BoundingVolumeNode& lhs,
		const mu::Mat4x4& rhsTransform, const BoundingVolumeNode& rhs
	);

	BoundingVolumeNode MU_CALLCONV transform(mu::Mat4x4 transform) const;

private:
	std::vector<Collider> colliders_;
	std::vector<BoundingVolumeNode> children_;
};

class BoundingVolume : public ecs::Component {
public:
	ENABLE_COMPONENT(BoundingVolume);

	BoundingVolume(const ecs::Entity& entity) NOEXCEPT
		: Component(entity) {}

	BoundingVolume(const ecs::Entity& entity, const std::filesystem::path& bvhPath)
		: BoundingVolume(entity, std::ifstream(bvhPath)) {}
	BoundingVolume(const ecs::Entity& entity, std::ifstream& bvhStream);
	BoundingVolume(const ecs::Entity& entity, std::ifstream&& bvhStream)
		: BoundingVolume(entity, bvhStream) {}

	BoundingVolumeNode& root() NOEXCEPT { return root_; }
	const BoundingVolumeNode& root() const NOEXCEPT { return root_; }

	bool collides(const BoundingVolume& other) const {
		return root_.collides(other.root_);
	}

	void resetCollisions() {
		pCurrentlyCollidedVolumes_.clear();
	}

	void markCollision(const BoundingVolume* pVolume) {
		pCurrentlyCollidedVolumes_.push_back(pVolume);
	}

private:
	static void importBVHNode(std::ifstream& bvhStream, BoundingVolumeNode& node);
	static void readColliders(std::ifstream& bvhStream, BoundingVolumeNode& node, std::size_t colliderCnt);
	static void readCapsuleCollider(std::ifstream& bvhStream, BoundingVolumeNode& node);
	static void readOBBCollider(std::ifstream& bvhStream, BoundingVolumeNode& node);

	BoundingVolumeNode root_;
	std::vector<const BoundingVolume*> pCurrentlyCollidedVolumes_;
};

class CollisionSystem : public ecs::System<BoundingVolume> {
public:
	void update();
};

#endif
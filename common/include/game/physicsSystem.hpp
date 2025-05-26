#ifndef __PHYSICSSYSTEM_HPP
#define __PHYSICSSYSTEM_HPP

#include "stdafx.hpp"

#include "ecs.hpp"
#include "coord.hpp"

#include "keyboardXX.hpp"

#include <DirectXCollision.h>

// Collider - BoundingHeightmap
// FreeImage 필요
// BoundingOrientedBox <-> BoundingHeightmap
// extents로 꼭짓점 8개 build하고 center, orientation으로 변환
// 꼭짓점의 y좌표 중 가장 작은 값을 읽었을 때 Heightmap의 높이보다 작으면 충돌
// 
// 충돌 피드백: 높이를 Heightmap의 높이로 설정, RigidBody의 velocity.y를 0으로 설정
// 피드백 타입 지정

inline constexpr float gravityConst = 9.81f;

class RigidBody : public ecs::Component {
public:
	static constexpr auto minInvMass = 1e-7f;
	ENABLE_COMPONENT(RigidBody);

	RigidBody(const ecs::Entity& entity) NOEXCEPT;

	void MU_CALLCONV setVelocity(mu::Vec3 velocity) NOEXCEPT {
		velocity_ = velocity;
	}
	void setInvMass(float invMass) NOEXCEPT {
		invMass_ = invMass;
	}
	void setKFriction(float kFriction) NOEXCEPT {
		kFriction_ = kFriction;
	}
	void setKAirdrag(float kAirdrag) NOEXCEPT {
		kAirdrag_ = kAirdrag;
	}
	void setKConstantAirDrag(float kConstantAirDrag) NOEXCEPT {
		kConstantAirDrag_ = kConstantAirDrag;
	}
	void enableGravity() NOEXCEPT {
		willSimulateGravity_ = true;
	}
	void disableGravity() NOEXCEPT {
		willSimulateGravity_ = false;
	}

	void MU_CALLCONV accMomentum(mu::Vec3 momentum) NOEXCEPT;
	void update(MilliSeconds deltaTime);
	
	mu::Vec3 MU_CALLCONV velocity() const NOEXCEPT { return velocity_; }
	float mass() const NOEXCEPT { return (invMass_ <= minInvMass) ? std::numeric_limits<float>::max() : 1.f / invMass_; }
	float invMass() const NOEXCEPT { return invMass_; }
	float kFriction() const NOEXCEPT { return kFriction_; }
	float kAirdrag() const NOEXCEPT { return kAirdrag_; }
	float kConstantAirDrag() const NOEXCEPT { return kConstantAirDrag_; }
	bool gravityEnabled() const NOEXCEPT { return willSimulateGravity_; }

private:
	mu::Vec3 velocity_;
	mu::Vec3 momentum_;

	// 0-3: x, 4-7: z, precision: 0.00003m
	au64t compressedDeltaVelocityXZ_;
	ai32t compressedDeltaVelocityY_;
	
	float invMass_;
	float kFriction_;
	float kAirdrag_;
	float kConstantAirDrag_;
	bool willSimulateGravity_;
};

class PhysicsSystem : public ecs::System<RigidBody> {
public:
	void update(MilliSeconds deltaTime);
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
	float t = mu::dot(point - A, AB) / mu::dot(AB, AB);
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

	void MU_CALLCONV setXform(mu::Mat4x4 xform) {
		*this = transformCollider(xform, *this);
	}


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
		: colliders_(other.colliders_), worldColliders_(other.worldColliders_), children_() {}

	BoundingVolumeNode& operator=(const BoundingVolumeNode& other) {
		colliders_ = other.colliders_;
		worldColliders_ = other.worldColliders_;
		children_.clear();

		return *this;
	}

	BoundingVolumeNode(BoundingVolumeNode&& other) noexcept = default;
	BoundingVolumeNode& operator=(BoundingVolumeNode&& other) noexcept = default;

	void addCollider(const Collider& collider) {
		colliders_.push_back(collider);
		worldColliders_.push_back(collider);
	}
	
	void addCollider(const BoundingCapsule& capsule) {
		colliders_.emplace_back(capsule);
		worldColliders_.emplace_back(capsule);
	}

	void addCollider(const BoundingOrientedBox& box) {
		colliders_.emplace_back(box);
		worldColliders_.emplace_back(box);
	}

	void addCollider(const BoundingFrustum& frustum) {
		colliders_.emplace_back(frustum);
		worldColliders_.emplace_back(frustum);
	}

	void reserveColliders(std::size_t size) {
		colliders_.reserve(size);
		worldColliders_.reserve(size);
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

	void MU_CALLCONV setXform(mu::Mat4x4 xform) {
		for (auto& collider : worldColliders_) {
			collider.setXform(xform);
		}
	}

	void MU_CALLCONV setXformCascade(mu::Mat4x4 xform) {
		setXform(xform);
		for (auto& child : children_) {
			child.setXformCascade(xform);
		}
	}

	auto& colliders() NOEXCEPT {
		return colliders_;
	}	

	const auto& colliders() const NOEXCEPT {
		return colliders_;
	}

	auto& children() NOEXCEPT {
		return children_;
	}

	const auto& children() const NOEXCEPT {
		return children_;
	}

private:
	std::vector<Collider> colliders_;
	std::vector<Collider> worldColliders_;
	std::vector<BoundingVolumeNode> children_;
};

class BoundingVolume : public ecs::Component {
public:
	ENABLE_COMPONENT(BoundingVolume);

	BoundingVolume(const ecs::Entity& entity) NOEXCEPT
		: Component(entity) {}

	BoundingVolume(const ecs::Entity& entity, const std::filesystem::path& bvhPath);

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

	void MU_CALLCONV setXformCascade(mu::Mat4x4 xform) {
		root_.setXformCascade(xform);
	}

	auto& collisionCache() NOEXCEPT {
		return pCurrentlyCollidedVolumes_;
	}

	const auto& collisionCache() const NOEXCEPT {
		return pCurrentlyCollidedVolumes_;
	}

private:
	static void importBVHNode(std::ifstream& bvhStream, BoundingVolumeNode& node);
	static void copyBVH(const BoundingVolumeNode& src, BoundingVolumeNode& dst);
	static void readColliders(std::ifstream& bvhStream, BoundingVolumeNode& node, std::size_t colliderCnt);
	static void readCapsuleCollider(std::ifstream& bvhStream, BoundingVolumeNode& node);
	static void readOBBCollider(std::ifstream& bvhStream, BoundingVolumeNode& node);

	static std::unordered_map<std::filesystem::path, BoundingVolumeNode> sBvhCache_;

	BoundingVolumeNode root_;
	std::vector<const BoundingVolume*> pCurrentlyCollidedVolumes_;
};

class CollisionSystem : public ecs::System<BoundingVolume> {
public:
	void update();
};

#endif
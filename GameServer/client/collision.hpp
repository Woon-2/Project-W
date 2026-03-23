#ifndef __collision_HPP
#define __collision_HPP


struct AABB {
	mu::Vec3 center;
	mu::Vec3 size;
};

struct OBB {
	mu::Vec3  center;
	mu::Vec3  halfExtents;
	mu::NQuat orient;
};

struct Ray {
	mu::Vec3 origin;
	mu::Vec3 dir;
};

struct CollisionResult {
	bool      hit;
	mu::NVec3 normal;
	mu::Vec3  mtv;
	float     depth;
	mu::Vec3  contactPoint;
};

using AABBCollisionResult = CollisionResult;

using CollisionVolume = std::variant<AABB, OBB>;

CollisionResult collides(const AABB& a, const AABB& b);
CollisionResult collides(const OBB& a,  const OBB& b);
CollisionResult collides(const CollisionVolume& a, const CollisionVolume& b);

OBB toOBB(const AABB& aabb);

struct RayHit {
	bool      hit;
	float     t;
	mu::Vec3  point;
	mu::NVec3 normal;
};

RayHit RaycastAABB(const AABB& box, const Ray& ray);

AABB buildAttackAABB(mu::Vec3 pos, mu::Vec3 forward, mu::Vec3 halfExtent, float offsetFwd);


#endif	// __collision_HPP

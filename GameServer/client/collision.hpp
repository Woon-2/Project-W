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

// N-ary BVH node. Maximum 3 levels (LOD 0 -> LOD 1 -> LOD 2).
// Every node carries a shape (AABB or OBB) for precise collision testing.
// Leaf nodes have no children (children.empty() == true).
struct BVHNode {
	AABB                    bounds;    // AABB encompassing this subtree (fast reject)
	std::variant<AABB, OBB> shape;     // actual shape for precise collision
	std::vector<int>        children;  // child indices into BVH::nodes (empty = leaf)
	std::string             name;      // box name (debug)
	std::string             boneName;  // bone name used at load time to resolve boneIdx
	int                     boneIdx = -1; // resolved bone index; -1 = root transform only

	bool isLeaf() const { return children.empty(); }
};

// Linearized BVH. nodes[0] is the root. Empty nodes means no collision.
struct BVH {
	std::vector<BVHNode> nodes;

	bool empty() const { return nodes.empty(); }
};

using CollisionVolume = BVH;

CollisionResult collides(const AABB& a, const AABB& b);
CollisionResult collides(const OBB& a,  const OBB& b);
CollisionResult collides(const BVH& a,  const BVH& b);
CollisionResult collides(const BVH& bvh, const AABB& hitbox);

OBB  toOBB(const AABB& aabb);
AABB obbToAABB(const OBB& obb);

struct RayHit {
	bool      hit;
	float     t;
	mu::Vec3  point;
	mu::NVec3 normal;
};

RayHit RaycastAABB(const AABB& box, const Ray& ray);

AABB buildAttackAABB(mu::Vec3 pos, mu::Vec3 forward, mu::Vec3 halfExtent, float offsetFwd);


#endif	// __collision_HPP

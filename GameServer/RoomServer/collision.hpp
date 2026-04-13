#ifndef room_server_collision_hpp
#define room_server_collision_hpp

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
CollisionResult collides(const OBB& a, const OBB& b);
CollisionResult collides(const BVH& a, const BVH& b);
CollisionResult collides(const BVH& bvh, const AABB& hitbox);

OBB  toOBB(const AABB& aabb);
AABB obbToAABB(const OBB& obb);

// One contact point between two rigid bodies.
// Stores warm-start accumulators that persist within a single step.
struct ContactPoint {
	mu::Vec3  worldPos;
	mu::Vec3  localA;
	mu::Vec3  localB;
	mu::NVec3 normal;                // points from B toward A
	float     depth         = 0.f;
	float     accNormal     = 0.f;
	float     accTangent[2] = {};
};

struct RayHit {
	bool      hit;
	float     t;
	mu::Vec3  point;
	mu::NVec3 normal;
};

RayHit RaycastAABB(const AABB& box, const Ray& ray);

AABB buildAttackAABB(mu::Vec3 pos, mu::Vec3 forward, mu::Vec3 halfExtent, float offsetFwd);

// ---------------------------------------------------------------------------
// TerrainCollider
// ---------------------------------------------------------------------------

struct TerrainHeightField; // defined in terrain.hpp
class  RigidBody;          // defined in rigidBody.hpp

// Generates contact points between dynamic RigidBody instances and a
// height-field terrain. The terrain body is Static (invMass == 0) and
// carries no BVH; collision is computed directly against TerrainHeightField.
class TerrainCollider {
public:
    TerrainCollider(RigidBody* terrainBody, const TerrainHeightField* hf);

    // Collects up to 4 deepest ContactPoints into outContacts (appends).
    // Returns the number of contacts added.
    int generateContacts(const RigidBody& dynamic,
                         std::vector<ContactPoint>& outContacts) const;

    RigidBody* terrainBody() const { return terrainBody_; }

private:
    static void extractBottomVertices(const BVHNode& leaf,
                                      std::vector<mu::Vec3>& out);

    bool testVertex(mu::Vec3 worldVert, ContactPoint& outCp) const;

    RigidBody*                terrainBody_ = nullptr;
    const TerrainHeightField* heightField_ = nullptr;
};

#endif // room_server_collision_hpp
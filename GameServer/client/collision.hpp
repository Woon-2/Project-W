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
CollisionResult collides(const BVH& bvh, const OBB&  hitbox);

OBB  toOBB(const AABB& aabb);
AABB obbToAABB(const OBB& obb);

// Transforms one local BVH-node shape into world space by a rigid transform
// (no skeleton). An AABB stays an AABB when orient is identity (cheap), and
// becomes an OBB when orient carries a rotation (so an axis-aligned model box
// under a yaw'd instance is still represented exactly). OBB shapes compose
// orient with the local orientation. Shared by Object::rebuildBodyBVH()
// (non-bone path) and the static prop collider.
std::variant<AABB, OBB> MU_CALLCONV transformShapeRigid(
    const std::variant<AABB, OBB>& local,
    mu::Vec3 pos, mu::NQuat orient, mu::Vec3 scale);

// Bakes a whole model-space BVH into world space via a rigid transform.
// Every node is treated as non-bone (boneIdx is preserved but ignored), so this
// is for static geometry (props) and the non-skeletal object path.
BVH MU_CALLCONV makeWorldBVH(const BVH& localBVH,
                             mu::Vec3 pos, mu::NQuat orient, mu::Vec3 scale);

struct RayHit {
	bool      hit;
	float     t;
	mu::Vec3  point;
	mu::NVec3 normal;
};

RayHit RaycastAABB(const AABB& box, const Ray& ray);
RayHit RaycastOBB (const OBB&  obb, const Ray& ray);
RayHit RaycastBVH (const BVH&  bvh, const Ray& ray);

AABB buildAttackAABB(mu::Vec3 pos, mu::Vec3 forward, mu::Vec3 halfExtent, float offsetFwd);

// ---------------------------------------------------------------------------
// ContactPoint (used by ContactConstraint and TerrainCollider)
// ---------------------------------------------------------------------------

// One contact point between two rigid bodies.
// Stores warm-start accumulators that persist within a single step.
struct ContactPoint {
	mu::Vec3  worldPos;              // world-space contact location
	mu::Vec3  localA;                // worldPos - bodyA->pos()
	mu::Vec3  localB;                // worldPos - bodyB->pos()
	mu::NVec3 normal;                // contact normal: points from B toward A
	float     depth         = 0.f;  // penetration depth (> 0 when penetrating)
	float     accNormal     = 0.f;  // accumulated normal impulse (warm start)
	float     accTangent[2] = {};   // accumulated tangent impulses (warm start)
};

// ---------------------------------------------------------------------------
// WorldCollider — static environment collision (terrain, scatter props)
// ---------------------------------------------------------------------------

struct TerrainHeightField; // defined in terrain.hpp
class  RigidBody;          // defined in rigidBody.hpp
class  ContactConstraint;  // defined in contactConstraint.hpp
struct StaticContact;      // defined in staticDepenetration.hpp

// Output buffers + per-step parameters handed to a WorldCollider so it can emit
// its own kind of contact: terrain emits support ContactConstraints, scatter
// emits push-out StaticContacts. Both peers carry both buffers.
struct ContactSink {
	std::vector<std::unique_ptr<ContactConstraint>>* constraints    = nullptr;
	std::vector<StaticContact>*                      staticContacts = nullptr;
	mu::Vec3                                          gravity{ 0.f, -9.8f, 0.f };
	float                                             subDtSec = 0.f;
};

// A static field collider that is NOT a broad-phase body: it self-rejects bodies
// outside its footprint and is queried per Dynamic body during contact
// generation. PhysicsWorld owns these via a SlotVector and registers terrain and
// scatter chunks through the same registry / query loop.
class WorldCollider {
public:
	virtual ~WorldCollider() = default;

	// Return true when bodyPos is clearly outside this collider's footprint
	// (skip the narrow phase). pad is a generous margin.
	virtual bool footprintReject(mu::Vec3 bodyPos, float pad) const = 0;

	// Append contacts for one movable (Dynamic) body to the sink. Returns the
	// number of contacts/constraints added.
	virtual int generateContacts(RigidBody& dyn, const ContactSink& sink) const = 0;

	// Camera spring-arm occlusion: max allowed arm length this collider permits
	// along [pivot, pivot + dir*armLen]. Default: no occlusion. (Client-only use.)
	virtual float queryArm(mu::Vec3 pivot, mu::Vec3 dir, float armLen, float spherePad) const {
		return armLen;
	}
};

// ---------------------------------------------------------------------------
// TerrainCollider — height-field terrain (support contacts)
// ---------------------------------------------------------------------------

// Generates support contacts between dynamic RigidBody instances and a
// height-field terrain. The terrain body is Static (invMass == 0) and
// carries no BVH; collision is computed directly against TerrainHeightField.
class TerrainCollider : public WorldCollider {
public:
	TerrainCollider(RigidBody* terrainBody, const TerrainHeightField* hf);

	bool  footprintReject(mu::Vec3 bodyPos, float pad) const override;
	int   generateContacts(RigidBody& dyn, const ContactSink& sink) const override;
	float queryArm(mu::Vec3 pivot, mu::Vec3 dir, float armLen, float spherePad) const override;

	RigidBody* terrainBody() const { return terrainBody_; }

private:
	// Appends the bottom vertices of a BVH leaf shape into out.
	// AABB: 4 bottom corners. OBB: all 8 corners (testVertex filters them).
	static void extractBottomVertices(const BVHNode& leaf,
	                                  std::vector<mu::Vec3>& out);

	// Tests one world-space vertex against the terrain surface.
	// lookAhead: treat vertices up to this distance above terrain as contacting.
	// Always uses Y-up as the contact normal (prevents slope-induced body tilt).
	bool testVertex(mu::Vec3 worldVert, ContactPoint& outCp, float lookAhead = 0.f) const;

	// Collects up to 4 deepest terrain ContactPoints (appends). Internal helper
	// wrapped by generateContacts(); lookAhead adds free-fall snap tolerance.
	int collectContacts(const RigidBody& dynamic, float lookAhead,
	                    std::vector<ContactPoint>& outContacts) const;

	RigidBody*                terrainBody_ = nullptr;
	const TerrainHeightField* heightField_ = nullptr;
};

// ---------------------------------------------------------------------------
// ScatterCollider — static props (nature: trees, rocks) as push-out obstacles
// ---------------------------------------------------------------------------

// One static collider per terrain chunk. Bakes each collidable instance's
// model-space BVH into world space once (props never move) and answers per-body
// depenetration and camera-ray queries against those baked BVHs. A built-in XZ
// uniform grid keeps per-body cost proportional to nearby instances only.
class ScatterCollider : public WorldCollider {
public:
	// One collidable prop instance: a model-space BVH plus its instance transform.
	struct InstanceInput {
		const BVH* modelBVH = nullptr;   // points into a resident prop Model::bvh
		mu::Vec3   pos{};
		mu::NQuat  rot{};
		mu::Vec3   scale{ 1.f, 1.f, 1.f };
	};

	explicit ScatterCollider(const std::vector<InstanceInput>& instances);

	bool  footprintReject(mu::Vec3 bodyPos, float pad) const override;
	int   generateContacts(RigidBody& dyn, const ContactSink& sink) const override;
	float queryArm(mu::Vec3 pivot, mu::Vec3 dir, float armLen, float spherePad) const override;

	bool empty() const { return insts_.empty(); }

private:
	struct Inst {
		BVH  worldBVH;     // baked once at construction (props are static)
		AABB worldAABB{};  // union of worldBVH node bounds (broad-phase + grid)
	};

	// Visits candidate instance indices overlapping queryAABB (XZ uniform grid).
	// Each candidate is visited at most once per query via a stamp.
	template <class F>
	void forEachCandidate(const AABB& queryAABB, F&& f) const {
		if (insts_.empty() || nx_ == 0 || nz_ == 0) return;
		const float qx0 = queryAABB.center.x() - queryAABB.size.x() * 0.5f;
		const float qx1 = queryAABB.center.x() + queryAABB.size.x() * 0.5f;
		const float qz0 = queryAABB.center.z() - queryAABB.size.z() * 0.5f;
		const float qz1 = queryAABB.center.z() + queryAABB.size.z() * 0.5f;
		int cx0 = static_cast<int>((qx0 - gridMinX_) / cellSize_);
		int cx1 = static_cast<int>((qx1 - gridMinX_) / cellSize_);
		int cz0 = static_cast<int>((qz0 - gridMinZ_) / cellSize_);
		int cz1 = static_cast<int>((qz1 - gridMinZ_) / cellSize_);
		if (cx1 < 0 || cz1 < 0 || cx0 >= nx_ || cz0 >= nz_) return;
		cx0 = std::max(cx0, 0); cz0 = std::max(cz0, 0);
		cx1 = std::min(cx1, nx_ - 1); cz1 = std::min(cz1, nz_ - 1);
		const int e = ++epoch_;
		for (int cz = cz0; cz <= cz1; ++cz)
			for (int cx = cx0; cx <= cx1; ++cx)
				for (int i : cells_[cellIndex(cx, cz)]) {
					if (visited_[i] == e) continue;
					visited_[i] = e;
					f(i);
				}
	}

	std::vector<Inst> insts_;
	AABB              bounds_{};         // union of all worldAABB (footprint reject)

	// Uniform grid over XZ.
	float             cellSize_ = 4.f;
	float             gridMinX_ = 0.f;
	float             gridMinZ_ = 0.f;
	int               nx_       = 0;
	int               nz_       = 0;
	std::vector<std::vector<int>> cells_;
	mutable std::vector<int>      visited_;  // per-inst query stamp
	mutable int                   epoch_ = 0;

	int cellIndex(int cx, int cz) const { return cz * nx_ + cx; }
};


#endif	// __collision_HPP

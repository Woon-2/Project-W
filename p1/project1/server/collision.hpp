#ifndef __collision_HPP
#define __collision_HPP

struct BoundingRect {
    mu::Vec2 center;
    mu::Vec2 size;
};

struct AABB {
	mu::Vec3 center;
	mu::Vec3 size;
};

struct Ray {
	mu::Vec3 origin;
	mu::Vec3 dir;
};

struct AABBCollisionResult {
    bool hit;
    mu::NVec3 normal;
    mu::Vec3 mtv;
    float depth;
    mu::Vec3 contactPoint;
};

AABBCollisionResult collides(const AABB& a, const AABB& b);

struct RayHit {
    bool hit;
    float t;    // ray hit distance
    mu::Vec3 point;     // hit point
    mu::NVec3 normal;    // hit face normal
};

RayHit RaycastAABB(const AABB& box, const Ray& ray);
RayHit RaycastBoundingRect(const BoundingRect& rect, const Ray& ray);

#endif	// __collision_HPP
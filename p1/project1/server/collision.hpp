#ifndef __collision_HPP
#define __collision_HPP

// 모델에 AABB 벡터
// 충돌 처리할 때 월드 변환 적용
// 기본 박스 모델에 center, size의 로컬 변환 먼저 적용 -> 모델 변환 적용 -> 월드 변환 적용 (TRS 분리된 거에서 TS만 가져와야겠다)

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

#endif	// __collision_HPP
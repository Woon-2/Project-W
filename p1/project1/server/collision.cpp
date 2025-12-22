#include "pch.hpp"
#include "collision.hpp"

AABBCollisionResult collides(const AABB& a, const AABB& b) {
    AABBCollisionResult res{ .hit = false };

    mu::Vec3 halfA = a.size * 0.5f;
    mu::Vec3 halfB = b.size * 0.5f;

    mu::Vec3 delta = b.center - a.center;
    mu::Vec3 overlap = (halfA + halfB) - mu::abs(delta);

    // 충돌 없음
    if (overlap.x() < 0 || overlap.y() < 0 || overlap.z() < 0) {
        return res;
    }

    // 충돌 발생
    res.hit = true;

    // 최소 오버랩 축 선택
    if (overlap.x() < overlap.y() && overlap.x() < overlap.z()) {
        // X축 충돌
        res.depth = overlap.x();
        res.normal = mu::NVec3((delta.x() > 0) ? -1.0f : 1.0f, 0, 0);
        res.mtv = mu::Vec3(res.normal) * res.depth;
    }
    else if (overlap.y() < overlap.z()) {
        // Y축 충돌
        res.depth = overlap.y();
        res.normal = mu::NVec3(0, (delta.y() > 0) ? -1.0f : 1.0f, 0);
        res.mtv = mu::Vec3(res.normal) * res.depth;
    }
    else {
        // Z축 충돌
        res.depth = overlap.z();
        res.normal = mu::Vec3(0, 0, (delta.z() > 0) ? -1.0f : 1.0f);
        res.mtv = mu::Vec3(res.normal) * res.depth;
    }

    mu::Vec3 minA = a.center - halfA;
    mu::Vec3 maxA = a.center + halfA;
    mu::Vec3 minB = b.center - halfB;
    mu::Vec3 maxB = b.center + halfB;

    res.contactPoint = (mu::max(minA, minB) + mu::min(maxA, maxB)) * 0.5f;

    return res;
}

RayHit RaycastAABB(const AABB& box, const Ray& ray) {
    RayHit hit { .hit = false };

    float tmin = std::numeric_limits<float>::min();
    float tmax = std::numeric_limits<float>::max();

    mu::Vec3 normal{};

    const auto boxMin = box.center - box.size * 0.5f;
    const auto boxMax = box.center + box.size * 0.5f;

    for (int i = 0; i < 3; i++) {
        if (fabs(ray.dir[i]) < 1e-6f) {
            // ray is parallel to the slab
            if (ray.origin[i] < boxMin[i] || ray.origin[i] > boxMax[i])
                return hit; // no hit
        } else {
            float invD = 1.0f / ray.dir[i];
            float t1 = (boxMin[i] - ray.origin[i]) * invD;
            float t2 = (boxMax[i] - ray.origin[i]) * invD;

            mu::Vec3 n1{};
            mu::Vec3 n2{};
            n1.setComponent(i, -1.f); // entering normal
            n2.setComponent(i, 1.f); // exiting normal

            if (t1 > tmin) {
                tmin = t1;
                normal = n1;
            }
            if (t2 < tmax) {
                tmax = t2;
            }
            if (tmin > tmax)
                return hit; // no hit
        }
    }

    // tmin은 ray가 box에 처음 충돌한 지점
    if (tmin < 0)
        return hit; // no hit

    hit.hit = true;
    hit.t = tmin;
    hit.point = ray.origin + ray.dir * tmin;
    hit.normal = normal;

    return hit;
}

// BoundingRect는 XZ 평면에 평행
RayHit RaycastBoundingRect(const BoundingRect& rect, const Ray& ray) {
    RayHit hit{.hit = false};

    float tMin = 0.f;
    float tMax = std::numeric_limits<float>::max();

	float minX = rect.center.x() - rect.size.x() * 0.5f;
	float maxX = rect.center.x() + rect.size.x() * 0.5f;
	float minZ = rect.center.y() - rect.size.y() * 0.5f;
	float maxZ = rect.center.y() + rect.size.y() * 0.5f;

    // x slab
    if (std::fabs(ray.dir.x()) < 1e-6f) {
        if (ray.origin.x() < minX || ray.origin.x() > maxX) {
			return hit;
        }
    }
    else {
        float invD = 1.f / ray.dir.x();
        float t1 = (minX - ray.origin.x()) * invD;
        float t2 = (maxX - ray.origin.x()) * invD;

        if (t1 > t2) {
            std::swap(t1, t2);
        }

        tMin = std::max(tMin, t1);
        tMax = std::min(tMax, t2);
        if (tMin > tMax) {
            return hit;
        }
    }

	// z slab
    if (std::fabs(ray.dir.z()) < 1e-6f) {
        if (ray.origin.z() < minZ || ray.origin.z() > maxZ) {
			return hit;
        }
    }
    else {
        float invD = 1.f / ray.dir.z();
        float t1 = (minZ - ray.origin.z()) * invD;
        float t2 = (maxZ - ray.origin.z()) * invD;

        if (t1 > t2) {
            std::swap(t1, t2);
        }

        tMin = std::max(tMin, t1);
        tMax = std::min(tMax, t2);
        if (tMin > tMax) {
            return hit;
		}
    }

	hit.hit = true;
	hit.t = tMin;
	return hit;
}

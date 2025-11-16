#ifndef __physics_HPP
#define __physics_HPP

#include "pch.hpp"
#include "collision.hpp"

class Object;

struct PhysicState {
	mu::Vec3 pos{};
	mu::Vec3 omega{};
	mu::NQuat orient{};
	mu::Vec3 scale{};

	std::vector<AABB> aabbs{};
};

// PhysicSnapshot들을 조합하여 update 함수에서 최종 물리량을 결정할 때 사용할 정책
enum class PhysicEvaluationMethod {
	LinearInterpolation
};

struct CollisionManifold
{
    Object* a;
    Object* b;

    bool colliding;
	float penetrationDepth;
    mu::Vec3 mtv;
	mu::NVec3 normal;
	mu::Vec3 contactPoint;
};

class PhysicSystem {
public:
	void step(const std::vector<Object*>& objects, Seconds dt);

private:
	void integrate(const std::vector<Object*>& objects, Seconds dt);
	void broadPhase(const std::vector<Object*>& objects);
	void narrowPhase();
	void solveCollisions(std::size_t iterations);
	void checkCollision(Object* a, Object* b);

	struct PotentialPair {
		Object* a;
		Object* b;
	};

	std::vector<PotentialPair> broadPairs_{};
	std::vector<CollisionManifold> contacts_{};
};


#endif	// __physics_HPP
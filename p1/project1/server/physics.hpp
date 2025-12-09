#ifndef __physics_HPP
#define __physics_HPP

#include "collision.hpp"

class Object;

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
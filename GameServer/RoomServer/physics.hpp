#ifndef room_server_physics_hpp
#define room_server_physics_hpp

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


#endif // room_server_physics_hpp
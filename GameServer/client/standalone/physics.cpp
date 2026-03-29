#include "pch.hpp"
#include "physics.hpp"
#include "../object.hpp"

void PhysicSystem::step(const std::vector<Object*>& objects, Seconds dt)
{
    for (auto* obj : objects) {
        obj->proceedPhysicState();
    }

    integrate(objects, dt);

    broadPairs_.clear();
    broadPhase(objects);

    contacts_.clear();
    narrowPhase();

    solveCollisions(1);
}

void PhysicSystem::integrate(const std::vector<Object*>& objects, Seconds dt)
{
    for (auto* obj : objects) {
        auto& phys = obj->physicState();

        auto& pos    = phys.pos;
        auto& omega  = phys.omega;
        auto& orient = phys.orient;

        pos += phys.velocity * dt.count();

        auto wq = mu::Quat(omega, 0.f);
        auto dq = orient * wq * 0.5f;
        orient = orient + dq * dt.count();

        obj->rebuildBVH(phys);
    }
}

void PhysicSystem::broadPhase(const std::vector<Object*>& objects) {
    for (int i = 0; i < (int)objects.size(); i++) {
        for (int j = i + 1; j < (int)objects.size(); j++) {
            broadPairs_.emplace_back(objects[i], objects[j]);
        }
    }
}

void PhysicSystem::narrowPhase()
{
    for (auto& [a, b] : broadPairs_) {
        checkCollision(a, b);
    }
}

void PhysicSystem::solveCollisions(std::size_t iterations)
{
    /*for (std::size_t i = 0; i < iterations; ++i)
    {
        for (auto& c : contacts_)
        {
            if (!c.colliding) continue;

            auto* a = c.a;
            auto* b = c.b;

            a->physicState().pos += c.mtv * 0.5f;
            b->physicState().pos -= c.mtv * 0.5f;

            a->rebuildBVH(a->physicState());
            b->rebuildBVH(b->physicState());
        }
    }*/
}

void PhysicSystem::checkCollision(Object* a, Object* b) {
    const auto& bvhA = a->physicState().bvh;
    const auto& bvhB = b->physicState().bvh;
    if (bvhA.empty() || bvhB.empty()) return;

    auto c = collides(bvhA, bvhB);
    if (c.hit) {
        contacts_.emplace_back(
            /* .a = */ a,
            /* .b = */ b,
            /* .colliding = */ true,
            /* .penetrationDepth = */ c.depth,
            /* .mtv = */ c.mtv,
            /* .normal = */ c.normal,
            /* .contactPoint = */ c.contactPoint
        );
    }
}

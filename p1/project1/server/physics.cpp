#include "pch.hpp"
#include "physics.hpp"
#include "object.hpp"

void PhysicSystem::step(const std::vector<Object*>& objects, Seconds dt)
{
    // integrate (예측)
    integrate(objects, dt);

    // broad-phase (충돌 후보군 추리기)
    broadPairs_.clear();
    broadPhase(objects);

    // narrow-phase (실제 충돌 검사)
    contacts_.clear();
    narrowPhase();

    // 충돌 피드백 (반복적 MTV solver)
    solveCollisions(1); // 필수적으로 여러 번 돌려야 함
}

void PhysicSystem::integrate(const std::vector<Object*>& objects, Seconds dt)
{
    for (auto* obj : objects) {
        auto& phys = obj->physicState();

        const auto pos = phys.pos;
	    const auto omega = phys.omega;
	    auto orient = phys.orient;
	    const auto scale = phys.scale;

	    // 쿼터니언 갱신: q' = 0.5 * ω_q * q
	    auto wq = mu::Quat(omega, 0.f);
	    auto dq = orient * wq * 0.5f;
	    orient = orient + dq * dt.count();

        // 바운딩 볼륨 갱신
	    // for (std::size_t i = 0; i < phys.aabbs.size(); ++i) {
		    // phys.aabbs[i].center = obj->model()->aabbs[i].center * phys.scale + phys.pos;
		    // phys.aabbs[i].size = obj->model()->aabbs[i].size * phys.scale;
	    // }
    }
}

void PhysicSystem::broadPhase(const std::vector<Object*>& objects) {
    for (int i = 0; i < objects.size(); i++) {
        for (int j = i + 1; j < objects.size(); j++) {
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
    for (std::size_t i = 0; i < iterations; ++i)
    {
        for (auto& c : contacts_)
        {
            if (!c.colliding) continue;

            auto* a = c.a;
            auto* b = c.b;

            a->physicState().pos += c.mtv * 0.5f;
            b->physicState().pos -= c.mtv * 0.5f;

            // 바운딩 볼륨 갱신
	        /*for (std::size_t i = 0; i < a->physicState().aabbs.size(); ++i) {
		        a->physicState().aabbs[i].center = a->model()->aabbs[i].center
                    * a->physicState().scale + a->physicState().pos;
		        a->physicState().aabbs[i].size = a->model()->aabbs[i].size
                    * a->physicState().scale;
	        }
	        for (std::size_t i = 0; i < b->physicState().aabbs.size(); ++i) {
		        b->physicState().aabbs[i].center = b->model()->aabbs[i].center
                    * b->physicState().scale + b->physicState().pos;
		        b->physicState().aabbs[i].size = b->model()->aabbs[i].size
                    * b->physicState().scale;
	        }*/
        }
    }
}

void PhysicSystem::checkCollision(Object* a, Object* b) {
    //for (int i = 0; i < a->physicState().aabbs.size(); ++i) {
    //    for (int j = 0; j < b->physicState().aabbs.size(); ++j) {
    //        auto c = collides( a->physicState().aabbs[i], b->physicState().aabbs[j] );
    //        if (c.hit) {
    //            contacts_.emplace_back(
    //                /* .a = */ a,
    //                /* .b = */ b,
    //                /* .colliding = */ true,
    //                /* .penetrationDepth = */ c.depth,
    //                /* .mtv = */ c.mtv,
    //                /* .normal = */ c.normal,
    //                /* .contactPoint = */ c.contactPoint
    //            );
    //            break;
    //        }
    //    }
    //}
}
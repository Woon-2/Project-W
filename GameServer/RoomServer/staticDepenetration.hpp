#ifndef room_server_static_depenetration_hpp
#define room_server_static_depenetration_hpp

#include "rigidBody.hpp"   // pulls in ContactPoint via collision.hpp
#include <vector>

// ---------------------------------------------------------------------------
// Static-object depenetration (server mirror of the client path)
// ---------------------------------------------------------------------------
//
// Collisions involving a Static obstacle are resolved by pushing the movable
// body out along the contact normal by the penetration amount only (positional
// correction) and clamping the inward normal velocity to zero. No rotation is
// applied and no separating velocity is injected, so a body settles against the
// surface without spin or bounce. Used on the server for monster-vs-prop
// (ScatterCollider) contacts so monsters do not walk through trees/rocks.
//
// Header-only on the server (inline) to avoid a new translation unit.

// One depenetration record: a movable body overlapping static geometry. The
// contact normal points from the static side toward the movable body (push-out).
struct StaticContact {
    RigidBody*   movable    = nullptr;  // Dynamic or Kinematic; moved directly
    RigidBody*   staticBody = nullptr;  // never moved (may be null for fields)
    ContactPoint contacts[4];
    int          count = 0;

    void addContact(const ContactPoint& cp) {
        if (count < 4)
            contacts[count++] = cp;
    }
};

namespace staticdepen {
    inline constexpr float kSlop        = 0.005f;  // matches ContactConstraint::kSlop
    inline constexpr float kCorrectFrac = 0.8f;    // partial correction (geometric convergence)
    inline constexpr float kMaxCorrect  = 0.2f;    // per sub-step clamp
}

// Resolve every static contact in one pass. Bodies whose position changed are
// appended to outMoved so the caller can rebuild their world-space BVH.
inline void resolveStaticPenetration(std::vector<StaticContact>& contacts,
                                     std::vector<RigidBody*>& outMoved)
{
    if (contacts.empty())
        return;

    struct BodyPush {
        RigidBody* body = nullptr;
        mu::Vec3   delta{ 0.f, 0.f, 0.f };
    };

    std::vector<BodyPush> pushes;
    pushes.reserve(contacts.size());

    auto pushFor = [&](RigidBody* body) -> BodyPush& {
        for (auto& p : pushes)
            if (p.body == body) return p;
        pushes.push_back(BodyPush{ body, mu::Vec3(0.f, 0.f, 0.f) });
        return pushes.back();
    };

    for (StaticContact& sc : contacts) {
        if (!sc.movable) continue;
        BodyPush& push = pushFor(sc.movable);

        for (int i = 0; i < sc.count; ++i) {
            const ContactPoint& cp = sc.contacts[i];

            const float pen = cp.depth - staticdepen::kSlop;
            if (pen <= 0.f) continue;

            const mu::Vec3 n = cp.normal;   // unit, static -> movable

            const float d = std::min(staticdepen::kCorrectFrac * pen,
                                     staticdepen::kMaxCorrect);

            // Max-combine along the normal so a corner (two near-orthogonal walls)
            // pushes along each axis without double-counting a shared axis.
            const float cur = mu::dot(push.delta, n);
            if (d > cur)
                push.delta += n * (d - cur);

            // Remove only the inward (into-surface) normal velocity component.
            const mu::Vec3 v  = sc.movable->linearVel();
            const float    vn = mu::dot(v, n);
            if (vn < 0.f)
                sc.movable->setLinearVel(v - n * vn);
        }
    }

    for (BodyPush& p : pushes) {
        if (p.delta.len2() <= 0.f) continue;
        p.body->setPos(p.body->pos() + p.delta);
        outMoved.push_back(p.body);
    }
}

#endif // room_server_static_depenetration_hpp

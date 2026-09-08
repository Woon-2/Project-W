#include "pch.hpp"
#include "staticDepenetration.hpp"

namespace {

// Accumulated positional correction for a single movable body. Corrections from
// multiple contacts are max-combined along each contact normal (see below).
struct BodyPush {
    RigidBody* body  = nullptr;
    mu::Vec3   delta{ 0.f, 0.f, 0.f };
};

} // namespace

void resolveStaticPenetration(std::vector<StaticContact>& contacts,
                              std::vector<RigidBody*>& outMoved)
{
    if (contacts.empty())
        return;

    // Small linear-probed accumulator: the number of distinct movable bodies in
    // contact with static geometry in a single step is tiny, so a flat vector
    // with linear search beats a hash map and pulls in no extra headers.
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

            // Positional correction along this normal, partial and clamped.
            const float d = std::min(staticdepen::kCorrectFrac * pen,
                                     staticdepen::kMaxCorrect);

            // Max-combine along the normal: if the running push already moves the
            // body this far (or further) along n, this contact adds nothing.
            // Two near-orthogonal walls each contribute along their own axis,
            // while a shared axis is not double-counted.
            const float cur = mu::dot(push.delta, n);
            if (d > cur)
                push.delta += n * (d - cur);

            // Velocity: remove only the inward (into-surface) normal component.
            // restitution = 0, so no bounce; tangential (sliding) is preserved.
            // Clamping (only when moving inward) never injects separating
            // velocity, which is what keeps the resting body from jittering.
            const mu::Vec3 v  = sc.movable->linearVel();
            const float    vn = mu::dot(v, n);
            if (vn < 0.f)
                sc.movable->setLinearVel(v - n * vn);
        }
    }

    for (BodyPush& p : pushes) {
        if (p.delta.len2() <= 0.f) continue;
        // Direct position write. Works for Kinematic too (invMass irrelevant);
        // no rotation is applied, so the contact point offset cannot spin the body.
        p.body->setPos(p.body->pos() + p.delta);
        outMoved.push_back(p.body);
    }
}

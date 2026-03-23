#ifndef __debugBVView_HPP
#define __debugBVView_HPP

#include "object.hpp"
#include "collision.hpp"
#include "BVPipeline.hpp"

// Lightweight batched AABB renderer for debug visualization.
// Submits directly to BVPipeline without requiring a Model or Object.
//
// Two entry types:
//   StaticEntry : snapshot AABB + TTL-based expiry.
//   LiveEntry   : recomputes AABB each frame from an Object's current position.
//                 Expires when TTL runs out OR the source Object is dead (hp <= 0).
//
// Ownership: raw Object pointers in LiveEntry are non-owning.
// The caller must guarantee the pointed-to Objects outlive this instance.
// (In practice, Game's monster shared_ptrs satisfy this constraint.)
class DebugBVView {
public:
    // Push a static snapshot AABB that expires after ttl.
    void push(const AABB& box, Milliseconds ttl,
              BVPipeline::BVModel model = BVPipeline::BVModel::Box) {
        staticEntries_.push_back({ box, ttl, model });
    }

    // Push a live AABB entry whose position follows obj each frame.
    // Expires when ttl runs out OR obj->hp() <= 0.
    void pushLive(const Object* obj,
                  mu::Vec3 halfExtent, float offsetFwd,
                  Milliseconds ttl,
                  BVPipeline::BVModel model = BVPipeline::BVModel::Box) {
        liveEntries_.push_back({ obj, halfExtent, offsetFwd, ttl, model });
    }

    // Decrement TTLs and evaluate expiry conditions; remove expired entries.
    void update(Milliseconds dt) {
        auto expireStatic = [&](const StaticEntry& e) { return e.ttl <= 0ms; };
        auto expireLive   = [&](const LiveEntry&   e) { return e.ttl <= 0ms || e.obj->hp() <= 0; };

        for (auto& e : staticEntries_) e.ttl -= dt;
        for (auto& e : liveEntries_)   e.ttl -= dt;

        staticEntries_.erase(
            std::remove_if(staticEntries_.begin(), staticEntries_.end(), expireStatic),
            staticEntries_.end());
        liveEntries_.erase(
            std::remove_if(liveEntries_.begin(), liveEntries_.end(), expireLive),
            liveEntries_.end());
    }

    // Submit all live entries as BVPipeline DrawEvents.
    void render(GFX& gfx) const {
        auto submit = [&](const AABB& box, BVPipeline::BVModel model) {
            gfx.addDrawEvent(BVPipeline::DrawEvent{
                .world   = mu::Mat4x4(mu::scale(box.size)) * mu::translate(box.center),
                .bvModel = model
            });
        };

        for (const auto& e : staticEntries_) {
            submit(e.box, e.model);
        }

        for (const auto& e : liveEntries_) {
            const auto box = buildAttackAABB(
                e.obj->pos(), e.obj->forward(), e.halfExtent, e.offsetFwd);
            submit(box, e.model);
        }
    }

private:
    struct StaticEntry {
        AABB                box;
        Milliseconds        ttl;
        BVPipeline::BVModel model;
    };

    struct LiveEntry {
        const Object*       obj;
        mu::Vec3            halfExtent;
        float               offsetFwd;
        Milliseconds        ttl;
        BVPipeline::BVModel model;
    };

    std::vector<StaticEntry> staticEntries_;
    std::vector<LiveEntry>   liveEntries_;
};

#endif  // __debugBVView_HPP

#ifndef __debugBVView_HPP
#define __debugBVView_HPP

#include "object.hpp"
#include "collision.hpp"
#include "BVPipeline.hpp"

// Lightweight batched BV renderer for debug visualization.
// Submits directly to BVPipeline without requiring a Model or Object.
//
// StaticEntry stores a precomputed world transform (supports both AABB and OBB).
// LiveEntry recomputes from an Object's current position each frame.
//
// Ownership: raw Object pointers in LiveEntry are non-owning.
// The caller must guarantee the pointed-to Objects outlive this instance.
class DebugBVView {
public:
    // Push a static snapshot AABB that expires after ttl.
    void push(const AABB& box, Milliseconds ttl,
              BVPipeline::BVModel model = BVPipeline::BVModel::Box) {
        const auto xform = mu::Mat4x4(mu::scale(box.size)) * mu::translate(box.center);
        staticEntries_.push_back({ xform, ttl, model });
    }

    // Push a static snapshot OBB (with full rotation) that expires after ttl.
    void push(const OBB& obb, Milliseconds ttl,
              BVPipeline::BVModel model = BVPipeline::BVModel::Box) {
        const auto xform = mu::Mat4x4(mu::scale(obb.halfExtents * 2.f))
                         * mu::Mat4x4(obb.orient)
                         * mu::translate(obb.center);
        staticEntries_.push_back({ xform, ttl, model });
    }

    // Push a live AABB entry whose position follows obj each frame.
    // Expires when ttl runs out OR obj->hp() <= 0.
    void pushLive(const Object* obj,
                  mu::Vec3 halfExtent, float offsetFwd,
                  Milliseconds ttl,
                  BVPipeline::BVModel model = BVPipeline::BVModel::Box) {
        liveEntries_.push_back({ obj, halfExtent, offsetFwd, ttl, model });
    }

    // Push all BVH nodes (or only leaf nodes) of obj as static snapshots.
    void pushBVHNodes(const Object& obj, Milliseconds ttl, bool leavesOnly = true) {
        for (const auto& node : obj.worldBVH().nodes) {
            if (leavesOnly && !node.isLeaf()) continue;
            std::visit([&](auto&& s) { push(s, ttl); }, node.shape);
        }
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

    // Submit all entries as BVPipeline DrawEvents.
    void render(GFX& gfx) const {
        for (const auto& e : staticEntries_) {
            gfx.addDrawEvent(BVPipeline::DrawEvent{
                .world   = e.worldXform,
                .bvModel = e.model
            });
        }

        for (const auto& e : liveEntries_) {
            const auto box = buildAttackAABB(
                e.obj->pos(), e.obj->forward(), e.halfExtent, e.offsetFwd);
            gfx.addDrawEvent(BVPipeline::DrawEvent{
                .world   = mu::Mat4x4(mu::scale(box.size)) * mu::translate(box.center),
                .bvModel = e.model
            });
        }
    }

private:
    struct StaticEntry {
        mu::Mat4x4          worldXform;
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

#include "pch.hpp"
#include "pathGuideSystem.hpp"

#include "gfx.hpp"
#include "mesh.hpp"
#include "terrainChunkManager.hpp"
#include "trailPipeline.hpp"
#include "energyOrbPipeline.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <span>
#include <unordered_map>

namespace {

inline float clampf(float v, float lo, float hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

// 3D distance between two points.
inline float dist3(const mu::Vec3& a, const mu::Vec3& b) {
    const mu::Vec3 d = a - b;
    return std::sqrt(mu::dot(d, d));
}

}  // namespace

void PathGuideSystem::build(const std::vector<MarkerDef>& markers) {
    paths_.clear();
    activePath_ = activePathPrev_ = -1;
    ribbon_.clear();
    wispVisible_ = false;

    // Group "PathPt" markers by pathId (name = "<pathId>_<index>"); sort by index.
    struct RawPt { int index; mu::Vec3 pos; };
    std::unordered_map<std::string, std::vector<RawPt>> groups;
    for (const auto& m : markers) {
        if (m.type != "PathPt") continue;
        const auto us = m.name.rfind('_');
        if (us == std::string::npos || us + 1 >= m.name.size()) continue;
        const std::string id = m.name.substr(0, us);
        int idx = 0;
        try { idx = std::stoi(m.name.substr(us + 1)); }
        catch (...) { continue; }
        groups[id].push_back(RawPt{ idx, m.pos });
    }

    const float spacing = (cfg_.sampleSpacing > 1e-3f) ? cfg_.sampleSpacing : 0.5f;

    for (auto& [id, pts] : groups) {
        if (pts.size() < 2u) continue;
        std::sort(pts.begin(), pts.end(),
            [](const RawPt& a, const RawPt& b) { return a.index < b.index; });

        // Arc-length resample the raw polyline to uniform spacing.
        Path path;
        path.id = id;
        path.samples.push_back(Sample{ pts.front().pos, 0.f });
        float walked = 0.f;
        float nextS  = spacing;
        for (std::size_t i = 0; i + 1 < pts.size(); ++i) {
            const mu::Vec3 A = pts[i].pos;
            const mu::Vec3 B = pts[i + 1].pos;
            const float seg = dist3(A, B);
            if (seg < 1e-4f) continue;
            while (nextS <= walked + seg) {
                const float t = (nextS - walked) / seg;
                path.samples.push_back(Sample{ mu::lerp(A, B, t), nextS });
                nextS += spacing;
            }
            walked += seg;
        }
        // Ensure the final endpoint is present.
        if (walked - path.samples.back().dist > 1e-3f)
            path.samples.push_back(Sample{ pts.back().pos, walked });
        path.totalLen = walked;

        if (path.samples.size() >= 2u && path.totalLen > 1e-3f)
            paths_.push_back(std::move(path));
    }
}

void PathGuideSystem::buildSamplePath(const mu::Vec3& origin, const mu::Vec3& forwardDir) {
    // Flatten the heading to XZ and derive a perpendicular for the S-curve wobble.
    mu::Vec3 fwd = mu::Vec3(forwardDir.x(), 0.f, forwardDir.z());
    const float flen = std::sqrt(mu::dot(fwd, fwd));
    fwd = (flen > 1e-3f) ? fwd * (1.f / flen) : mu::Vec3(0.f, 0.f, 1.f);
    mu::Vec3 right = mu::Vec3(mu::cross(fwd, mu::Vec3(0.f, 1.f, 0.f)));
    const float rlen = std::sqrt(mu::dot(right, right));
    right = (rlen > 1e-3f) ? right * (1.f / rlen) : mu::Vec3(1.f, 0.f, 0.f);

    constexpr int   kN    = 13;     // waypoints
    constexpr float kStep = 3.5f;   // forward spacing per waypoint (m)
    constexpr float kAmp  = 4.0f;   // S-curve amplitude (m)

    std::vector<MarkerDef> markers;
    markers.reserve(kN);
    for (int i = 0; i < kN; ++i) {
        const float s   = static_cast<float>(i) * kStep;
        const float wob = std::sin(static_cast<float>(i) * 0.6f) * kAmp;
        MarkerDef m;
        m.type = "PathPt";
        m.name = "test_" + std::to_string(i);
        m.pos  = origin + fwd * s + right * wob;
        markers.push_back(std::move(m));
    }
    build(markers);
}

float PathGuideSystem::projectOntoPath(const Path& p, const mu::Vec3& playerPos, float& outDist) {
    float bestDist2 = std::numeric_limits<float>::max();
    float bestS     = 0.f;
    const float px = playerPos.x();
    const float pz = playerPos.z();

    for (std::size_t i = 0; i + 1 < p.samples.size(); ++i) {
        const mu::Vec3& A = p.samples[i].pos;
        const mu::Vec3& B = p.samples[i + 1].pos;
        const float abx = B.x() - A.x();
        const float abz = B.z() - A.z();
        const float seg2 = abx * abx + abz * abz;
        float t = 0.f;
        if (seg2 > 1e-8f)
            t = clampf(((px - A.x()) * abx + (pz - A.z()) * abz) / seg2, 0.f, 1.f);
        const float cx = A.x() + abx * t;
        const float cz = A.z() + abz * t;
        const float d2 = (px - cx) * (px - cx) + (pz - cz) * (pz - cz);
        if (d2 < bestDist2) {
            bestDist2 = d2;
            bestS = p.samples[i].dist + t * (p.samples[i + 1].dist - p.samples[i].dist);
        }
    }
    outDist = std::sqrt(bestDist2);
    return bestS;
}

mu::Vec3 PathGuideSystem::sampleAt(const Path& p, float s) {
    if (p.samples.empty()) return mu::Vec3{ 0.f, 0.f, 0.f };
    s = clampf(s, 0.f, p.totalLen);
    for (std::size_t i = 0; i + 1 < p.samples.size(); ++i) {
        if (s <= p.samples[i + 1].dist || i + 2 == p.samples.size()) {
            const float segLen = p.samples[i + 1].dist - p.samples[i].dist;
            const float t = (segLen > 1e-5f) ? clampf((s - p.samples[i].dist) / segLen, 0.f, 1.f) : 0.f;
            return mu::lerp(p.samples[i].pos, p.samples[i + 1].pos, t);
        }
    }
    return p.samples.back().pos;
}

void PathGuideSystem::update(float dtSec, const mu::Vec3& playerPos, const TerrainChunkManager& terrain) {
    flowTime_  += dtSec;
    bobPhase_  += dtSec * cfg_.wispBobFreq;
    ribbon_.clear();

    // Hard off-switch (tactical combat): drop all guidance and report inactive.
    if (suppressed_) {
        wispVisible_    = false;
        activePath_     = -1;
        activePathPrev_ = -1;
        sPlayer_        = 0.f;
        return;
    }

    // Pick the nearest not-yet-completed path within the activation radius.
    activePath_ = -1;
    float bestDist = cfg_.activateRadius;
    float sPlayer  = 0.f;
    for (std::size_t i = 0; i < paths_.size(); ++i) {
        if (paths_[i].samples.size() < 2u || paths_[i].completed) continue;
        float d = 0.f;
        const float s = projectOntoPath(paths_[i], playerPos, d);
        if (d < bestDist) {
            bestDist    = d;
            activePath_ = static_cast<int>(i);
            sPlayer     = s;
        }
    }

    // Arrival: reaching a path's end retires it for good (no more guidance for it).
    if (activePath_ >= 0) {
        const mu::Vec3 endp = paths_[static_cast<std::size_t>(activePath_)].samples.back().pos;
        const float ex = endp.x() - playerPos.x();
        const float ez = endp.z() - playerPos.z();
        if (ex * ex + ez * ez < cfg_.arriveRadius * cfg_.arriveRadius) {
            paths_[static_cast<std::size_t>(activePath_)].completed = true;
            activePath_ = -1;
        }
    }

    if (activePath_ < 0) {
        wispVisible_    = false;
        activePathPrev_ = -1;
        sPlayer_        = 0.f;
        return;
    }

    sPlayer_ = sPlayer;
    const Path& p = paths_[static_cast<std::size_t>(activePath_)];

    // Conform XZ to ground height; fall back to the authored Y when no chunk is
    // loaded under the point (heightAtWorld returns 0 in that case).
    auto conformY = [&](float x, float z, float authoredY) {
        const float h = terrain.heightAtWorld(x, z);
        return (h == 0.f) ? authoredY : h;
    };

    // --- Ribbon window (re-conformed every frame) ---
    // sStart sits ahead of the player (never behind), so the ribbon reads as
    // materializing a few meters in front of them rather than trailing from
    // underneath/behind.
    const float sStart = clampf(sPlayer + cfg_.leadGap, 0.f, p.totalLen);
    const float sEnd   = clampf(sPlayer + cfg_.windowAhead, 0.f, p.totalLen);
    if (sEnd - sStart >= 0.5f) {
        const float winLen = sEnd - sStart;
        const float Lf = (cfg_.endFade > 1e-3f) ? cfg_.endFade : 1e-3f;
        auto addVert = [&](float s) {
            const mu::Vec3 ap = sampleAt(p, s);
            const float y = conformY(ap.x(), ap.z(), ap.y()) + cfg_.ribbonYOffset;
            const float ds = s - sStart;
            const float fadeIn  = 1.f - clampf(ds / Lf, 0.f, 1.f);
            const float fadeOut = clampf((ds - (winLen - Lf)) / Lf, 0.f, 1.f);
            const float age = clampf(fadeIn > fadeOut ? fadeIn : fadeOut, 0.f, 1.f);
            ribbon_.push_back(RibbonVert{ mu::Vec3(ap.x(), y, ap.z()), ds, age });
        };
        addVert(sStart);
        for (const auto& sm : p.samples)
            if (sm.dist > sStart + 1e-3f && sm.dist < sEnd - 1e-3f) addVert(sm.dist);
        addVert(sEnd);
    }

    // --- Wisp (flies ahead of the player) ---
    if (p.totalLen > 0.5f) {
        const float sTarget = clampf(sPlayer + cfg_.wispLead, 0.f, p.totalLen);
        if (activePath_ != activePathPrev_) {
            sWisp_ = sTarget;  // snap when the active path changes (no fly-across)
        } else {
            const float k = clampf(cfg_.wispEaseRate * dtSec, 0.f, 1.f);
            sWisp_ += (sTarget - sWisp_) * k;
        }
        const mu::Vec3 wp = sampleAt(p, sWisp_);
        const float y = conformY(wp.x(), wp.z(), wp.y())
                      + cfg_.wispHoverY + std::sin(bobPhase_) * cfg_.wispBobAmp;
        wispPos_     = mu::Vec3(wp.x(), y, wp.z());
        wispVisible_ = true;
    } else {
        wispVisible_ = false;
    }

    activePathPrev_ = activePath_;
}

void PathGuideSystem::submitDrawEvents(GFX& gfx, const Texture* ribbonTex, const Mesh* orbProxy) const {
    // --- Ribbon: split the window polyline into <=32-vertex trail draw events with
    // a 1-vertex overlap so the tiling/flow stays continuous across the seams. ---
    constexpr std::size_t kChunk = 31u;  // <= TrailPipeline::kMaxVerticesPerTrail
    for (std::size_t start = 0; start + 1u < ribbon_.size(); start += (kChunk - 1u)) {
        const std::size_t end = std::min(ribbon_.size(), start + kChunk);  // [start, end)
        if (end - start < 2u) break;

        TrailPipeline::DrawEvent ev{};
        ev.vertices.reserve(end - start);
        for (std::size_t k = start; k < end; ++k) {
            ev.vertices.push_back(TrailPipeline::TrailVertexCPU{
                .pos            = ribbon_[k].pos,
                .age            = ribbon_[k].age,
                .cumulativeDist = ribbon_[k].cumulative,
            });
        }
        ev.baseColor         = mu::Vec4(cfg_.ribbonColor.x(), cfg_.ribbonColor.y(), cfg_.ribbonColor.z(), 1.f);
        ev.widthStart        = cfg_.ribbonWidth;
        ev.widthEnd          = cfg_.ribbonWidth;
        ev.widthMultiplier   = 1.f;
        ev.tileLength        = cfg_.tileLength;
        ev.textureMode       = 1u;            // Tile (flow uses cumulativeDist)
        ev.trailLifetime     = 1.f;           // age in [0,1] drives the end fades
        ev.currentSystemTime = flowTime_;
        ev.flowSpeed         = cfg_.flowSpeed;
        ev.alignMode         = 1u;            // ground-aligned ribbon
        ev.patternMode       = 1u;            // flowing directional chevrons (emphasis)
        ev.premultiplyAlpha  = 1u;            // HDR channel is always additive (One/One) -- needs
                                               // alpha premultiplied for the end-fade to be visible
        ev.pMainTex          = ribbonTex;
        ev.additive          = true;
        ev.renderOrder       = 0;
        gfx.addHDRTrailDrawEvent(std::move(ev));

        if (end == ribbon_.size()) break;
    }

    // --- Wisp: one free-orb energy-orb draw at morphT=1. ---
    if (wispVisible_ && orbProxy && !orbProxy->subMeshes.empty()) {
        const auto& sub = orbProxy->subMeshes[0];
        const u32t vtxCount = sub.ibView.SizeInBytes / static_cast<u32t>(sizeof(u16t));

        EnergyOrbPipeline::DrawEvent ev{};
        ev.world        = mu::translate(wispPos_);
        ev.pMesh        = orbProxy;
        ev.pSubMesh     = &sub;
        ev.pAlbedo      = nullptr;
        ev.boneXforms   = std::span<const mu::Mat4x4>(&identityBone_, 1u);
        ev.sphereCenter = wispPos_;
        ev.sphereRadius = cfg_.wispRadius;
        ev.colorHDR     = cfg_.wispColor;
        ev.morphT       = 1.f;
        ev.pointSize    = cfg_.wispPointSize;
        ev.vertexCount  = vtxCount;
        ev.renderOrder  = 0;
        gfx.addDrawEvent(ev);
    }
}

mu::Vec3 PathGuideSystem::goalWorld() const {
    if (activePath_ < 0) return mu::Vec3{ 0.f, 0.f, 0.f };
    const Path& p = paths_[static_cast<std::size_t>(activePath_)];
    return p.samples.empty() ? mu::Vec3{ 0.f, 0.f, 0.f } : p.samples.back().pos;
}

float PathGuideSystem::distanceToGoal() const {
    if (activePath_ < 0) return 0.f;
    const Path& p = paths_[static_cast<std::size_t>(activePath_)];
    return std::max(0.f, p.totalLen - sPlayer_);
}

void PathGuideSystem::activePathPoints(std::vector<mu::Vec3>& out) const {
    if (activePath_ < 0) return;
    const Path& p = paths_[static_cast<std::size_t>(activePath_)];
    if (p.samples.size() < 2u) return;

    // Sub-sample to ~2 m so the minimap polyline stays cheap; keep the endpoints.
    const float spacing = (cfg_.sampleSpacing > 1e-3f) ? cfg_.sampleSpacing : 0.5f;
    std::size_t stride = static_cast<std::size_t>(std::max(1.f, std::round(2.f / spacing)));
    for (std::size_t i = 0; i < p.samples.size(); i += stride)
        out.push_back(p.samples[i].pos);
    if ((p.samples.size() - 1u) % stride != 0u)
        out.push_back(p.samples.back().pos);
}

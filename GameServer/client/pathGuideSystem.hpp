#ifndef __pathGuideSystem_HPP
#define __pathGuideSystem_HPP

#include "mathUtil.hpp"
#include "../common/markerDef.hpp"

#include <vector>
#include <string>

class GFX;
class TerrainChunkManager;
struct Mesh;
struct Texture;

// Client-only cosmetic path guidance.
//
//   C) a flowing HDR light ribbon along the ground (TrailPipeline, ground-aligned +
//      UV flow, submitted to the pre-bloom HDR trail channel so it glows), and
//   D) a guiding wisp (an EnergyOrb "free orb") that flies ahead of the player
//      along the path.
//
// Paths are authored in Unity as "PathPt" LevelMarkers (name "<pathId>_<index>")
// baked into chunks_index.bin; the client groups + sorts them into polylines and
// resamples to uniform spacing. The route is purely presentational, so (like
// ZoneSystem) it is driven entirely on the game thread: update() advances state,
// submitDrawEvents() only pushes GFX draw events (no locks). The ribbon window is
// re-conformed to terrain height every frame, so it stays glued to the ground even
// as chunks stream in.
class PathGuideSystem {
public:
    struct Config {
        // Ribbon
        float    sampleSpacing = 0.5f;   // polyline resample spacing (m)
        float    windowAhead   = 25.f;   // ribbon far end, ahead of the player (m)
        float    leadGap       = 3.f;    // gap ahead of the player before the ribbon starts
                                          // fading in (m) -- nothing is drawn under/behind the
                                          // player, so the ribbon reads as starting ahead of them
        float    ribbonWidth   = 0.8f;   // ribbon width (m)
        float    ribbonYOffset = 0.10f;  // lift above terrain to avoid z-fighting (m)
        float    flowSpeed     = 0.5f;   // Tile-UV scroll toward the goal (tiles/sec)
        float    tileLength    = 2.5f;   // world units per texture tile
        float    endFade       = 3.0f;   // fade length at both window ends (m)
        mu::Vec3 ribbonColor   = { 0.5f, 1.8f, 2.6f };  // HDR cyan (bloom)

        // Wisp
        float    wispLead      = 22.0f;  // arc-length ahead of the player (m) -- kept near the
                                          // ribbon's far end (windowAhead - endFade) so it reads
                                          // as guiding toward the path's tip, not the midpoint
        float    wispEaseRate  = 2.5f;   // follow smoothing (1/sec)
        float    wispHoverY    = 1.1f;   // height above terrain (m)
        float    wispBobAmp    = 0.18f;  // vertical bob amplitude (m)
        float    wispBobFreq   = 2.2f;   // bob frequency (rad/sec)
        float    wispRadius    = 0.18f;  // orb sphere radius (m)
        float    wispPointSize  = 0.06f; // per-point sprite size (m)
        mu::Vec3 wispColor     = { 1.5f, 2.8f, 3.6f };  // HDR cyan-white

        // Activation: ignore paths whose nearest point is farther than this (m).
        float    activateRadius = 80.f;
    };

    // Parses "PathPt" markers into resampled polylines. Safe to call again on reload.
    void build(const std::vector<MarkerDef>& markers);

    // DEBUG/TEST: synthesizes a winding sample path (as "PathPt" markers parsed
    // through build()) starting at origin and heading along forwardDir. Used as a
    // fallback so the effect can be seen online before any path is authored in Unity.
    void buildSamplePath(const mu::Vec3& origin, const mu::Vec3& forwardDir);

    // Advances ribbon window + wisp. playerPos is the live local player position;
    // terrain supplies per-frame ground height for the visible window.
    void update(float dtSec, const mu::Vec3& playerPos, const TerrainChunkManager& terrain);

    // Pushes the ribbon (pre-bloom HDR trail) + wisp (energy orb) draw events.
    // ribbonTex: additive streak texture (null = untextured white). orbProxy: the
    // shared OrbProxyMesh used to render the free-orb wisp.
    void submitDrawEvents(GFX& gfx, const Texture* ribbonTex, const Mesh* orbProxy) const;

    bool hasPaths() const { return !paths_.empty(); }
    Config& config() { return cfg_; }
    const Config& config() const { return cfg_; }

private:
    struct Sample { mu::Vec3 pos; float dist = 0.f; };  // pos = authored (xz + authored y); dist = arc length
    struct Path   { std::string id; std::vector<Sample> samples; float totalLen = 0.f; };

    struct RibbonVert { mu::Vec3 pos; float cumulative = 0.f; float age = 0.f; };

    // Projects playerPos onto path p (XZ); returns arc-length s, writes perpendicular
    // XZ distance to outDist.
    static float projectOntoPath(const Path& p, const mu::Vec3& playerPos, float& outDist);
    // Authored position (xz + authored y) at arc length s along p.
    static mu::Vec3 sampleAt(const Path& p, float s);

    std::vector<Path>       paths_;
    std::vector<RibbonVert> ribbon_;        // current frame window polyline (conformed)

    int      activePath_     = -1;
    int      activePathPrev_ = -1;
    float    flowTime_       = 0.f;

    bool     wispVisible_    = false;
    float    sWisp_          = 0.f;
    float    bobPhase_       = 0.f;
    mu::Vec3 wispPos_{};

    mu::Mat4x4 identityBone_{};  // single-bone palette for the free orb (DrawEvent span source)

    Config cfg_{};
};

#endif  // __pathGuideSystem_HPP

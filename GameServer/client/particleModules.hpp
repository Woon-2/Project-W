#pragma once
#include "gfxUtil.hpp"   // ColorGradient, Texture, mu::Vec2/3/4/Mat4x4

struct Mesh;
struct SubMesh;
struct SpriteAnimationClip;

// ---------------------------------------------------------------------------
// Main Module — per-particle randomised lifetime, speed, size, gravity, etc.
// ---------------------------------------------------------------------------
struct MainModule {
    float    lifetimeMin        = 0.5f;
    float    lifetimeMax        = 1.5f;
    float    speedMin           = 1.f;
    float    speedMax           = 3.f;
    mu::Vec4 startColor         = { 1.f, 1.f, 1.f, 1.f };
    float    startSizeMin       = 1.f;   // per-particle size multiplier min
    float    startSizeMax       = 1.f;   // per-particle size multiplier max
    float    startRotationMin   = 0.f;   // radians (billboard Z-axis)
    float    startRotationMax   = 0.f;
    mu::Mat4x4 startRotation3D = mu::Mat4x4{};  // mesh: full 3D orientation at spawn (identity = unused)
    float    gravityModifierMin = 0.f;
    float    gravityModifierMax = 0.f;
    mu::Vec3 gravity            = { 0.f, -9.8f, 0.f };
    // Lifecycle controls
    float    duration        = 5.f;   // one cycle length in seconds; 0 = no time limit
    bool     looping         = true;  // restart after duration elapses (ignored when duration == 0)
    float    startDelay      = 0.f;   // seconds before first emission after startContinuous()
    float    simulationSpeed = 1.f;   // global playback speed multiplier applied to dt

    enum class SimulationSpace { World, Local };
    // World: current behaviour — particles live in world space.
    // Local: particles are simulated relative to the emitter transform.
    //        Phase 1: flag is stored; actual transform is applied from Phase 2 onward.
    SimulationSpace simulationSpace = SimulationSpace::World;
};

// ---------------------------------------------------------------------------
// Emission Module — continuous emission rate (burst scheduling is deferred).
// ---------------------------------------------------------------------------
struct EmissionModule {
    float emitRate = 0.f;   // particles/sec; 0 = manual emit only
};

// ---------------------------------------------------------------------------
// Shape Module — spawn origin + initial velocity direction.
// ---------------------------------------------------------------------------
struct ShapeModule {
    enum class Type { Point, Edge, Cone, Sphere, Box };
    Type     type      = Type::Point;
    mu::Vec3 position  = { 0.f, 0.f, 0.f };  // shape origin offset (emitter local)
    mu::Vec3 direction = { 0.f, 1.f, 0.f };  // primary emission axis (Point / Edge / Cone)

    // Edge
    float    edgeLength = 1.f;
    mu::Vec3 edgeDir    = { 1.f, 0.f, 0.f };

    // Cone — base radius 0 means apex emission
    float    coneAngle  = 0.3f;  // half-angle (radians)
    float    coneRadius = 0.f;   // base disc radius

    // Sphere
    float    sphereRadius = 1.f;

    // Box
    mu::Vec3 boxSize = { 1.f, 1.f, 1.f };
};

// ---------------------------------------------------------------------------
// VelocityOverLifetime Module — velocity decay (drag) applied each frame.
// ---------------------------------------------------------------------------
struct VelocityOverLifetimeModule {
    bool  enabled = false;
    float drag    = 0.f;   // exponential velocity decay coefficient (1/sec)
};

// ---------------------------------------------------------------------------
// ColorOverLifetime Module — RGBA multiplier curve over normalised lifetime.
// ---------------------------------------------------------------------------
struct ColorOverLifetimeModule {
    bool          enabled  = false;
    ColorGradient gradient = ColorGradient::constant({ 1.f, 1.f, 1.f, 1.f });
};

// ---------------------------------------------------------------------------
// SizeOverLifetime Module — scalar size lerp over normalised lifetime.
// ---------------------------------------------------------------------------
struct SizeOverLifetimeModule {
    bool  enabled   = false;
    float sizeBegin = 1.f;
    float sizeEnd   = 0.f;
};

// ---------------------------------------------------------------------------
// RotationOverLifetime Module — angular velocity (local Z axis) applied each frame.
// ---------------------------------------------------------------------------
struct RotationOverLifetimeModule {
    bool  enabled            = false;
    float angularVelocityMin = 0.f;   // rad/sec, local Z axis
    float angularVelocityMax = 0.f;
};

// ---------------------------------------------------------------------------
// CustomData Module — per-particle float2 channels passed to the shader.
//
// Phase 0-1 (current): Constant mode only.
// Phase 1.5 (scheduled, before Phase 2): Curve mode — independent per-channel
//   FloatCurve evaluated over normalised lifetime.  When that ships, the
//   curve fields below will be uncommented and Mode::Curve will be wired up.
// ---------------------------------------------------------------------------
struct CustomDataModule {
    bool enabled = false;

    enum class Mode { Constant /*, Curve  -- Phase 1.5 */ };
    Mode mode = Mode::Constant;

    // Constant mode (Phase 0-1)
    mu::Vec2 custom0Constant = { 0.f, 0.f };
    mu::Vec2 custom1Constant = { 0.f, 0.f };

    // Curve mode (Phase 1.5) — uncomment when FloatCurve type is available:
    // FloatCurve custom0X, custom0Y;
    // FloatCurve custom1X, custom1Y;
};

// ---------------------------------------------------------------------------
// Material Descriptor — textures + blend / feature flags for the shader.
// Contained by RendererModule; passed on to the pipeline in Phase 4.
// ---------------------------------------------------------------------------
struct MaterialDescriptor {
    const Texture* mainTex      = nullptr;
    const Texture* emissionTex  = nullptr;
    const Texture* flowTex      = nullptr;    // UV flow/distortion mask
    const Texture* dissolveTex  = nullptr;    // dissolve noise texture

    enum class BlendMode { Alpha, Additive };
    BlendMode blendMode = BlendMode::Additive;

    // Feature flags — pipeline variant or uniform control; Phase 4 activates them.
    bool softParticles       = false;   // depth-fade (requires depth buffer SRV)

    // Material scalar parameters (forwarded as shader uniforms in Phase 4)
    float emissionIntensity  = 1.f;
    float flowStrength       = 0.f;
    float dissolveThreshold  = 0.f;
};

// ---------------------------------------------------------------------------
// Renderer Module — backend selection + mesh/billboard resources + UV packing.
// ---------------------------------------------------------------------------
struct RendererModule {
    enum class Mode { Billboard, StretchedBillboard /* Phase 3 */, Mesh };
    Mode mode        = Mode::Billboard;
    int  renderOrder = 0;

    // Billboard-specific
    const SpriteAnimationClip* pClip = nullptr;

    // Mesh-specific
    const Mesh*    pMesh    = nullptr;
    const SubMesh* pSubMesh = nullptr;

    // Material / shader binding
    MaterialDescriptor material;
};

// ---------------------------------------------------------------------------
// ParticleSystemConfig — top-level config composed of modules.
// ---------------------------------------------------------------------------
struct ParticleSystemConfig {
    MainModule                  main;
    EmissionModule              emission;
    ShapeModule                 shape;
    VelocityOverLifetimeModule  velocityOverLifetime;
    ColorOverLifetimeModule     colorOverLifetime;
    SizeOverLifetimeModule      sizeOverLifetime;
    RotationOverLifetimeModule  rotationOverLifetime;
    CustomDataModule            customData;
    RendererModule              renderer;
};

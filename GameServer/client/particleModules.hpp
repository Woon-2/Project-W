#pragma once
#include "gfxUtil.hpp"   // ColorGradient, Texture, mu::Vec2/3/4/Mat4x4

struct Mesh;
struct SubMesh;

namespace ps {

// ---------------------------------------------------------------------------
// Main Module
// ---------------------------------------------------------------------------
struct MainModule {
    float    lifetimeMin        = 0.5f;
    float    lifetimeMax        = 1.5f;
    float    speedMin           = 1.f;
    float    speedMax           = 3.f;
    mu::Vec4 startColor         = { 1.f, 1.f, 1.f, 1.f };
    float    startSizeMin       = 1.f;
    float    startSizeMax       = 1.f;
    float    startRotationMin   = 0.f;   // radians (billboard Z-axis)
    float    startRotationMax   = 0.f;
    mu::Mat4x4 startRotation3D = mu::Mat4x4{};  // mesh: full 3D orientation at spawn
    float    gravityModifierMin = 0.f;
    float    gravityModifierMax = 0.f;
    mu::Vec3 gravity            = { 0.f, -9.8f, 0.f };
    float    duration           = 5.f;   // cycle length in seconds; 0 = no limit
    bool     looping            = true;
    float    startDelay         = 0.f;
    float    simulationSpeed    = 1.f;

    enum class SimulationSpace { World, Local };
    SimulationSpace simulationSpace = SimulationSpace::World;
};

// ---------------------------------------------------------------------------
// Emission Module
// ---------------------------------------------------------------------------
struct EmissionModule {
    float emitRate = 0.f;   // particles/sec; 0 = manual emit only
};

// ---------------------------------------------------------------------------
// Shape Module
// ---------------------------------------------------------------------------
struct ShapeModule {
    enum class Type { Point, Edge, Cone, Sphere, Box };
    Type     type      = Type::Point;
    mu::Vec3 position  = { 0.f, 0.f, 0.f };
    mu::Vec3 direction = { 0.f, 1.f, 0.f };

    float    edgeLength   = 1.f;
    mu::Vec3 edgeDir      = { 1.f, 0.f, 0.f };

    float    coneAngle    = 0.3f;   // half-angle (radians)
    float    coneRadius   = 0.f;    // base disc radius; 0 = apex emission

    float    sphereRadius = 1.f;

    mu::Vec3 boxSize      = { 1.f, 1.f, 1.f };
};

// ---------------------------------------------------------------------------
// VelocityOverLifetime Module
// ---------------------------------------------------------------------------
struct VelocityOverLifetimeModule {
    bool  enabled = false;
    float drag    = 0.f;   // exponential velocity decay (1/sec)
};

// ---------------------------------------------------------------------------
// ColorOverLifetime Module
// ---------------------------------------------------------------------------
struct ColorOverLifetimeModule {
    bool          enabled  = false;
    ColorGradient gradient = ColorGradient::constant({ 1.f, 1.f, 1.f, 1.f });
};

// ---------------------------------------------------------------------------
// SizeOverLifetime Module
// ---------------------------------------------------------------------------
struct SizeOverLifetimeModule {
    bool  enabled   = false;
    float sizeBegin = 1.f;
    float sizeEnd   = 0.f;
};

// ---------------------------------------------------------------------------
// RotationOverLifetime Module
// ---------------------------------------------------------------------------
struct RotationOverLifetimeModule {
    bool  enabled            = false;
    float angularVelocityMin = 0.f;   // rad/sec, local Z axis
    float angularVelocityMax = 0.f;
};

// ---------------------------------------------------------------------------
// CustomData Module
// Phase 0-1: Constant mode only.
// Phase 1.5: Curve mode (FloatCurve per channel) -- deferred.
// ---------------------------------------------------------------------------
struct CustomDataModule {
    bool enabled = false;

    enum class Mode { Constant /*, Curve -- Phase 1.5 */ };
    Mode mode = Mode::Constant;

    mu::Vec2 custom0Constant = { 0.f, 0.f };
    mu::Vec2 custom1Constant = { 0.f, 0.f };
};

// ---------------------------------------------------------------------------
// Particle Material types
// Each struct = "use this Pipeline + these values".
// Named with 'Mat' prefix to avoid collision with XxxShader::Material in shader.hpp.
// ---------------------------------------------------------------------------

// MatUnlit: BillboardPipeline (Mode::Billboard) or MeshParticlePipeline (Mode::Mesh)
struct MatUnlit {
    const Texture* mainTex = nullptr;
    bool additive = false;   // false = alpha blend, true = additive blend
};

// MatSwordSlash: SwordSlashPipeline (Mode::Mesh only)
struct MatSwordSlash {
    const Texture* mainTex     = nullptr;
    const Texture* emissionTex = nullptr;
    const Texture* dissolveTex = nullptr;
    const Texture* flowTex     = nullptr;

    mu::Vec2 speedMainTexUV    = { 0.f, 0.f };
    mu::Vec2 speedFlow         = { 0.f, 0.f };
    float    flowPower         = 0.f;
    float    emission          = 1.f;
    float    desaturation      = 0.f;
    mu::Vec2 remap             = { -2.f, 1.f };
    mu::Vec4 addColor          = { 0.f, 0.f, 0.f, 0.f };
    float    opacity           = 1.f;
    bool     useSmoothDissolve = false;
};

using AnyMat = std::variant<MatUnlit, MatSwordSlash>;

// ---------------------------------------------------------------------------
// Renderer Module
// ---------------------------------------------------------------------------
struct RendererModule {
    enum class Mode { Billboard, StretchedBillboard /* Phase 3 */, Mesh };
    Mode mode        = Mode::Billboard;
    int  renderOrder = 0;

    const Mesh*    pMesh    = nullptr;
    const SubMesh* pSubMesh = nullptr;

    AnyMat mat = MatUnlit{};
};

// ---------------------------------------------------------------------------
// TextureSheetAnimation Module
// When enabled, overrides Billboard UV with grid-based frame UV.
// Uses RendererModule::material.mainTex as the sprite sheet.
// ---------------------------------------------------------------------------
struct TextureSheetAnimationModule {
    bool enabled = false;

    // Mode: Grid (implemented), Sprite (deferred)
    enum class Mode { Grid /*, Sprite */ };
    Mode mode = Mode::Grid;

    int tilesX = 1;   // columns
    int tilesY = 1;   // rows

    // WholeSheet: all tiles form one sequence
    // SingleRow : one row is an independent sequence (RowMode deferred)
    enum class Animation { WholeSheet, SingleRow };
    Animation animation = Animation::WholeSheet;

    // TimeMode: Lifetime implemented; Speed/FPS deferred
    enum class TimeMode { Lifetime /*, Speed, FPS */ };
    TimeMode timeMode = TimeMode::Lifetime;

    // Row Mode (SingleRow only) -- deferred
    // enum class RowMode { Custom, Random /*, MeshIndex */ };
    // RowMode rowMode = RowMode::Random;
    // int     row     = 0;   // used when rowMode == Custom

    // FrameOverTime curve -- deferred (linear fixed)
    // FloatCurve frameOverTime;

    int   startFrame = 0;    // start frame offset (random deferred)
    float cycles     = 1.f;  // how many times to loop during particle lifetime

    // Affected UV Channels -- deferred
    // int affectedUVChannels = 0xFF;
};

// ---------------------------------------------------------------------------
// ParticleSystemConfig
// ---------------------------------------------------------------------------
struct ParticleSystemConfig {
    MainModule                  main;
    EmissionModule              emission;
    ShapeModule                 shape;
    VelocityOverLifetimeModule  velocityOverLifetime;
    ColorOverLifetimeModule     colorOverLifetime;
    SizeOverLifetimeModule      sizeOverLifetime;
    RotationOverLifetimeModule  rotationOverLifetime;
    TextureSheetAnimationModule textureSheetAnimation;
    CustomDataModule            customData;
    RendererModule              renderer;
};

}  // namespace ps

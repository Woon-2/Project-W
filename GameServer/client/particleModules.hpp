#pragma once
#include "gfxUtil.hpp"   // ColorGradient, Texture, mu::Vec2/3/4/Mat4x4

#include <vector>
#include <variant>

struct Mesh;
struct SubMesh;

namespace ps {

// ---------------------------------------------------------------------------
// Shared curve type used by Unity-style MinMaxCurve-backed modules.
// ---------------------------------------------------------------------------
struct FloatKey {
    float t          = 0.f;
    float value      = 0.f;
    float inTangent  = 0.f;
    float outTangent = 0.f;
};

struct FloatCurve {
    std::vector<FloatKey> keys;

    float evaluate(float t) const;
};

struct MinMaxCurveChannel {
    enum class Mode { Constant, TwoConstants, Curve, TwoCurves };
    Mode mode = Mode::Constant;

    float constant    = 0.f;
    float constantMin = 0.f;
    float constantMax = 0.f;
    float curveMultiplier = 1.f;
    FloatCurve curve;
    FloatCurve curveMin;
    FloatCurve curveMax;

    float evaluate(float t, float random01) const;
};

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
    bool     startRotation3DEnabled = false;
    mu::Vec3 startRotation3DMin = { 0.f, 0.f, 0.f };  // radians, Unity Start Rotation XYZ
    mu::Vec3 startRotation3DMax = { 0.f, 0.f, 0.f };
    mu::Mat4x4 startRotation3D = mu::Mat4x4{};  // mesh: externally supplied base orientation
    float    flipRotation      = 0.f;
    float    gravityModifierMin = 0.f;
    float    gravityModifierMax = 0.f;
    mu::Vec3 gravity            = { 0.f, -9.8f, 0.f };
    float    duration           = 5.f;   // cycle length in seconds; 0 = no limit
    bool     looping            = true;
    bool     prewarm            = false;
    bool     playOnAwake        = true;
    float    startDelay         = 0.f;
    float    simulationSpeed    = 1.f;
    int      maxParticles       = 0;      // 0 = ParticleSystem::init argument/default

    enum class SimulationSpace { World, Local };
    SimulationSpace simulationSpace = SimulationSpace::World;

    enum class ScalingMode { Hierarchy, Local, Shape };
    ScalingMode scalingMode = ScalingMode::Hierarchy;
};

// ---------------------------------------------------------------------------
// Emission Module
// ---------------------------------------------------------------------------
struct EmissionModule {
    struct Burst {
        float time           = 0.f;
        int   countMin       = 1;
        int   countMax       = 1;
        int   cycleCount     = 1;
        float repeatInterval = 0.01f;
        float probability    = 1.f;
    };

    bool  enabled          = true;
    float emitRate         = 0.f;   // particles/sec; 0 = manual emit only
    float rateOverDistance = 0.f;   // deferred: requires emitter movement tracking
    std::vector<Burst> bursts;
};

// ---------------------------------------------------------------------------
// Shape Module
// ---------------------------------------------------------------------------
struct ShapeModule {
    enum class Type { Point, Edge, Cone, Sphere, Box, Circle };
    bool     enabled   = true;
    Type     type      = Type::Point;
    mu::Vec3 position  = { 0.f, 0.f, 0.f };
    mu::Vec3 rotation  = { 0.f, 0.f, 0.f };  // radians
    mu::Vec3 scale     = { 1.f, 1.f, 1.f };
    mu::Vec3 direction = { 0.f, 1.f, 0.f };
    mu::Mat4x4 orientation = mu::Mat4x4{};   // local shape axes to world axes

    float    edgeLength   = 1.f;
    mu::Vec3 edgeDir      = { 1.f, 0.f, 0.f };

    float    coneAngle    = 0.3f;   // half-angle (radians)
    float    coneRadius   = 0.f;    // base disc radius; 0 = apex emission

    float    sphereRadius = 1.f;

    mu::Vec3 boxSize      = { 1.f, 1.f, 1.f };

    float    radiusThickness         = 1.f;
    float    arc                     = 2.f * 3.14159265f;
    bool     alignToDirection        = false;
    float    randomDirectionAmount   = 0.f;
    float    sphericalDirectionAmount = 0.f;
    float    randomPositionAmount    = 0.f;
};

// ---------------------------------------------------------------------------
// VelocityOverLifetime Module
// ---------------------------------------------------------------------------
struct VelocityOverLifetimeModule {
    bool  enabled = false;
    float drag    = 0.f;   // exponential velocity decay (1/sec)
    mu::Vec3 linear = { 0.f, 0.f, 0.f };
    mu::Vec3 orbital = { 0.f, 0.f, 0.f };
    float radial = 0.f;
    float speedModifier = 1.f;
    bool  inWorldSpace = false;
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

    bool separateAxes = false;
    FloatCurve curve;
    FloatCurve x;
    FloatCurve y;
    FloatCurve z;
};

// ---------------------------------------------------------------------------
// RotationOverLifetime Module
// ---------------------------------------------------------------------------
struct RotationOverLifetimeModule {
    bool  enabled            = false;
    float angularVelocityMin = 0.f;   // rad/sec, local Z axis
    float angularVelocityMax = 0.f;
    bool  separateAxes       = false;
    mu::Vec3 angularVelocityMin3D = { 0.f, 0.f, 0.f };
    mu::Vec3 angularVelocityMax3D = { 0.f, 0.f, 0.f };

    bool useCurves = false;
    MinMaxCurveChannel x;
    MinMaxCurveChannel y;
    MinMaxCurveChannel z;
};

// ---------------------------------------------------------------------------
// CustomData Module
// Mirrors Unity ParticleSystem Custom Data streams used by Custom Vertex
// Streams: Custom1.xy and Custom2.xy.
// ---------------------------------------------------------------------------
struct CustomDataChannel {
    enum class Mode { Constant, TwoConstants, Curve, TwoCurves };
    Mode mode = Mode::Constant;

    float constant    = 0.f;
    float constantMin = 0.f;
    float constantMax = 0.f;
    float curveMultiplier = 1.f;
    FloatCurve curve;
    FloatCurve curveMin;
    FloatCurve curveMax;

    float evaluate(float t, float random01) const;
};

struct CustomDataStream {
    CustomDataChannel x;
    CustomDataChannel y;
};

struct CustomDataModule {
    bool enabled = false;

    CustomDataStream custom1;
    CustomDataStream custom2;
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
    mu::Vec2 speedDissolveUV   = { 0.f, 0.f };
    mu::Vec2 speedFlow         = { 0.f, 0.f };
    mu::Vec4 mainTexST         = { 1.f, 1.f, 0.f, 0.f };
    mu::Vec4 emissionTexST     = { 1.f, 1.f, 0.f, 0.f };
    mu::Vec4 dissolveTexST     = { 1.f, 1.f, 0.f, 0.f };
    mu::Vec4 flowTexST         = { 1.f, 1.f, 0.f, 0.f };
    float    flowPower         = 0.f;
    float    emission          = 1.f;
    float    desaturation      = 0.f;
    mu::Vec2 remap             = { -2.f, 1.f };
    mu::Vec4 addColor          = { 0.f, 0.f, 0.f, 0.f };
    float    opacity           = 1.f;
    bool     useSmoothDissolve = false;
};

// MatSmokeBlendCG: Shader Graphs/HS_Blend_CG port used by Smoke24bcg.
struct MatSmokeBlendCG {
    const Texture* mainTex = nullptr;
    const Texture* noiseTex = nullptr;
    const Texture* flowTex = nullptr;
    const Texture* maskTex = nullptr;

    mu::Vec4 mainTexST = { 1.f, 1.f, 0.f, 0.f };
    mu::Vec4 noiseTexST = { 1.f, 1.f, 0.f, 0.f };
    mu::Vec4 flowTexST = { 1.f, 1.f, 0.f, 0.f };
    mu::Vec4 maskTexST = { 1.f, 1.f, 0.f, 0.f };
    mu::Vec4 speedMainTexUVNoiseZW = { 0.f, 0.f, 0.f, 0.f };
    mu::Vec4 distortionSpeedXYPowerZ = { 0.f, 0.f, 0.f, 0.f };
    mu::Vec4 color = { 1.f, 1.f, 1.f, 1.f };

    float emission = 2.f;
    float opacity = 1.f;
    float textureOpacity = 0.f;
    float multiplyTexture = 1.f;
    float useOnlyColor = 0.f;
    float useFresnel = 0.f;
    float fresnelPower = 3.f;
    float fresnelScale = 3.f;
    float useCenterGlow = 0.f;
    float useDepth = 1.f;
    float depthPower = 0.5f;
};

using AnyMat = std::variant<MatUnlit, MatSwordSlash, MatSmokeBlendCG>;

// ---------------------------------------------------------------------------
// Renderer Module
// ---------------------------------------------------------------------------
struct RendererModule {
    enum class Mode { Billboard, StretchedBillboard /* Phase 3 */, Mesh };
    enum class Alignment { View, World, Local, Facing };
    enum class SortMode { None, Distance, OldestInFront, YoungestInFront };

    Mode mode        = Mode::Billboard;
    Alignment alignment = Alignment::View;
    SortMode sortMode   = SortMode::None;
    int  renderOrder = 0;
    float sortingFudge = 0.f;
    float minParticleSize = 0.f;
    float maxParticleSize = 0.5f;
    float normalDirection = 1.f;
    bool  allowRoll       = true;
    mu::Vec3 pivot        = { 0.f, 0.f, 0.f };
    mu::Vec3 flip         = { 0.f, 0.f, 0.f };

    float cameraVelocityScale = 0.f;  // StretchedBillboard deferred
    float velocityScale       = 0.f;  // StretchedBillboard deferred
    float lengthScale         = 2.f;  // StretchedBillboard deferred

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

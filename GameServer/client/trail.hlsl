// trail.hlsl
// Camera-facing ribbon strips for Unity-style ParticleSystem Trails.
// One drawcall per particle trail; vertex data lives in a shared StructuredBuffer.
// VertexCount = (trailCount - 1) * 6 ; VS expands each segment into two triangles
// using SV_VertexID. No vertex buffer is bound.

#include "bindless.hlsli"

// Layout of one trail vertex inside gTrailVertices.
// pos             : world or emitter-local space (depends on simulationSpace)
// age             : seconds since this vertex was emitted (capped at trailLifetime)
// cumulativeDist  : arc length accumulated from the first vertex of THIS trail
struct TrailVertex {
    float3 pos;
    float  age;
    float  cumulativeDist;
    float3 pad;
};

cbuffer PerDrawcallData : register(b0) {
    float4x4 localToWorld;          // 64B   identity for World-space sim
    uint4    idxMainTex;            // 16B   bindless main texture index
    float4   baseColor;             // 16B   particle tint * material tint

    uint     trailStart;            // first vertex index in gTrailVertices
    uint     trailCount;            // number of vertices in this trail
    uint     textureMode;           // 0 = Stretch, 1 = Tile
    uint     inheritParticleColor;  // reserved (0/1)

    float    widthStart;            // width at trail head (newest vertex)
    float    widthEnd;              // width at trail tail (oldest vertex)
    float    widthMultiplier;
    float    tileLength;            // world units per UV tile (Tile mode)

    float    trailLifetime;
    float    currentSystemTime;
    float    flowSpeed;             // Tile-mode UV scroll along the trail; 0 = static (default)
    uint     alignMode;             // 0 = camera-facing (default), 1 = ground-aligned (world-up)
};

cbuffer PerFrameData : register(b1) {
    float4x4 matViewProj;           // 64B
    float3   cameraPos;             // 12B
    float    pad1;                  // 4B
};

StructuredBuffer<TrailVertex> gTrailVertices : register(t0);

struct PSInput {
    float4 pos   : SV_Position;
    float2 uv    : TEXCOORD0;
    float4 color : COLOR;
};

// Map SV_VertexID -> (offset along strip, side -1/+1 across strip width).
// Two triangles per segment, triangle list winding:
//   tri0 = (s, left)  (s, right)   (s+1, left)
//   tri1 = (s, right) (s+1, right) (s+1, left)
static const uint  kStripOffsets[6] = { 0u, 0u, 1u, 0u, 1u, 1u };
static const float kSides[6]        = { -1.f, 1.f, -1.f, 1.f, 1.f, -1.f };

PSInput VSMain(uint vid : SV_VertexID) {
    PSInput ret;

    uint segmentIdx  = vid / 6u;
    uint cornerInSeg = vid % 6u;

    uint  k    = segmentIdx + kStripOffsets[cornerInSeg];
    float side = kSides[cornerInSeg];

    // Boundary-clamped neighbor indices for central-difference tangent.
    uint kPrev = (k == 0u) ? 0u : (k - 1u);
    uint kNext = (k + 1u >= trailCount) ? (trailCount - 1u) : (k + 1u);

    TrailVertex vCur  = gTrailVertices[trailStart + k];
    TrailVertex vPrev = gTrailVertices[trailStart + kPrev];
    TrailVertex vNext = gTrailVertices[trailStart + kNext];

    float3 posW  = mul(float4(vCur.pos,  1.f), localToWorld).xyz;
    float3 prevW = mul(float4(vPrev.pos, 1.f), localToWorld).xyz;
    float3 nextW = mul(float4(vNext.pos, 1.f), localToWorld).xyz;

    float3 tangent = nextW - prevW;
    float  tlen    = length(tangent);
    tangent = (tlen > 1e-5f) ? (tangent / tlen) : float3(1.f, 0.f, 0.f);

    // Across-strip direction. Camera-facing (default) billboards the ribbon toward
    // the eye; ground-aligned expands across world-up so the ribbon lies flat on
    // terrain (path-on-ground look).
    float3 sideDir;
    if (alignMode == 1u) {
        sideDir = cross(tangent, float3(0.f, 1.f, 0.f));
    } else {
        float3 viewDir = cameraPos - posW;
        float  vlen    = length(viewDir);
        viewDir = (vlen > 1e-5f) ? (viewDir / vlen) : float3(0.f, 0.f, 1.f);
        sideDir = cross(tangent, viewDir);
    }
    float slen = length(sideDir);
    sideDir = (slen > 1e-5f) ? (sideDir / slen) : float3(0.f, 1.f, 0.f);

    // segmentT: 0 at the oldest vertex (tail), 1 at the newest vertex (head).
    float denom    = (trailCount > 1u) ? float(trailCount - 1u) : 1.f;
    float segmentT = float(k) / denom;

    // Width tapers from widthStart at the head to widthEnd at the tail.
    float width = lerp(widthEnd, widthStart, segmentT) * widthMultiplier;

    float3 worldPos = posW + sideDir * (side * 0.5f * width);
    ret.pos = mul(float4(worldPos, 1.f), matViewProj);

    // U: 0 on the left side, 1 on the right side.
    // V: Stretch maps the whole trail to [0,1]; Tile uses cumulative arc length.
    float u = (side + 1.f) * 0.5f;
    float v;
    if (textureMode == 0u) {
        v = 1.f - segmentT;
    } else {
        // Tile mode: scroll V over time so the texture flows toward the trail head.
        v = vCur.cumulativeDist / max(tileLength, 1e-3f) - currentSystemTime * flowSpeed;
    }
    ret.uv = float2(u, v);

    // Per-vertex alpha fade by vertex age. Older vertices fade out toward 0.
    float ageT = saturate(vCur.age / max(trailLifetime, 1e-3f));
    float alpha = 1.f - ageT;
    ret.color = float4(baseColor.rgb, baseColor.a * alpha);

    return ret;
}

float4 PSMain(PSInput input) : SV_TARGET {
    float4 src = sampleBindless(idxMainTex, input.uv);
    return float4(src.rgb * input.color.rgb, src.a * input.color.a);
}

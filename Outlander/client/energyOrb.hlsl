#include "bindless.hlsli"

// Energy orb: each mesh vertex is rendered as a small camera-facing, self-lit
// quad (point sprite). Vertices start at the death-pose mesh surface tinted by
// the submesh albedo (the corpse color) and morph toward a glowing sphere whose
// color is a random HDR value, so the corpse visibly dissolves into bright orbs.
// Rendered into SceneColorHDR with additive blending BEFORE bloom (see gfx.cpp).

struct PerInstanceData {
    float4x4 world;              // death-time object world (row-major)
    float4   colorAndSize;       // rgb = HDR sphere color (morph target), a = point size
    float4   sphereCenterRadius; // xyz = orb center (world), w = sphere radius
    uint     rootBoneOffset;     // index into gBoneData for this orb's palette
    float    morphT;             // 0 = mesh pose + albedo, 1 = sphere + HDR
    uint     vertexCount;        // submesh vertex count (hash normalization)
    float    pad;
};

cbuffer PerDrawcallData : register(b0) {
    int4 idxAlbedo;            // bindless submesh albedo (x < 0 = none)
    uint firstInstanceOffset;
    uint3 pad0;
};

cbuffer PerFrameData : register(b1) {
    float4x4 matViewProj;  // row-major (transposed before upload from CPU)
    float3   cameraPosW;   // world-space camera position
    float    padding0;
};

StructuredBuffer<PerInstanceData> gInstances : register(t0);
StructuredBuffer<float4x4>        gBoneData : register(t2); // death-pose palette

// VS -> GS carries world-space data only (clip transform happens in GS, like
// billboard.hlsl). Do NOT emit SV_Position from the VS.
struct VSOutput {
    float3 worldPos : TEXCOORD0;
    float3 hdrColor : TEXCOORD1;
    float  size     : TEXCOORD2;
    float2 uv       : TEXCOORD3;
    float  morphT   : TEXCOORD4;
};

struct PSInput {
    float4 pos      : SV_Position;
    float2 local    : TEXCOORD0; // [-1,1] quad coords for the round falloff
    float3 hdrColor : TEXCOORD1;
    float2 uv       : TEXCOORD2;
    float  morphT   : TEXCOORD3;
};

float4x4 blendBoneTransform(uint rootBoneOffset, int4 boneIndices, float4 boneWeights) {
    return
        gBoneData[rootBoneOffset + boneIndices.x] * boneWeights.x +
        gBoneData[rootBoneOffset + boneIndices.y] * boneWeights.y +
        gBoneData[rootBoneOffset + boneIndices.z] * boneWeights.z +
        gBoneData[rootBoneOffset + boneIndices.w] * boneWeights.w;
}

// Integer hash -> [0,1). Cheap, deterministic per vertex id.
float hash11(uint n) {
    n = (n << 13) ^ n;
    n = n * (n * n * 15731u + 789221u) + 1376312589u;
    return float(n & 0x7fffffffu) / 2147483647.0f;
}

// Uniformly distributed point inside a unit sphere.
float3 pointInUnitSphere(uint vid) {
    float u1 = hash11(vid * 3u + 0u);
    float u2 = hash11(vid * 3u + 1u);
    float u3 = hash11(vid * 3u + 2u);
    float r     = pow(u1, 1.0f / 3.0f);
    float cosT  = 2.0f * u2 - 1.0f;
    float sinT  = sqrt(max(0.0f, 1.0f - cosT * cosT));
    float phi   = 6.2831853f * u3;
    return r * float3(sinT * cos(phi), sinT * sin(phi), cosT);
}

VSOutput VSMain(
    float3 position    : POSITION,
    int4   boneIndices : BONE_INDICES,
    float4 boneWeights : BONE_WEIGHTS,
    float2 uv          : UV,
    uint   vid         : SV_VertexID,
    uint   instID      : SV_InstanceID
) {
    VSOutput ret;
    PerInstanceData inst = gInstances[firstInstanceOffset + instID];

    // Death-pose skinning -> world-space rest position of this vertex.
    float4x4 anim = blendBoneTransform(inst.rootBoneOffset, boneIndices, boneWeights);
    float4 animatedPos = mul(float4(position, 1.0f), anim);
    float3 startPosW = mul(animatedPos, inst.world).xyz;

    // Per-vertex target inside the orb sphere.
    float3 targetW = inst.sphereCenterRadius.xyz
                   + pointInUnitSphere(vid) * inst.sphereCenterRadius.w;

    float t = smoothstep(0.0f, 1.0f, saturate(inst.morphT));
    ret.worldPos = lerp(startPosW, targetW, t);
    ret.hdrColor = inst.colorAndSize.rgb;
    ret.size     = lerp(inst.colorAndSize.a * 0.6f, inst.colorAndSize.a, t); // grow as it glows
    ret.uv       = uv;
    ret.morphT   = t;
    return ret;
}

[maxvertexcount(4)]
void GSMain(point VSOutput input[1], inout TriangleStream<PSInput> triStream) {
    float3 pos   = input[0].worldPos;
    float3 vLook = normalize(cameraPosW - pos);

    float3 worldUp = (abs(vLook.y) < 0.999f) ? float3(0.0f, 1.0f, 0.0f)
                                             : float3(1.0f, 0.0f, 0.0f);
    float3 vRight = normalize(cross(worldUp, vLook));
    float3 vUp    = cross(vLook, vRight);

    float h = input[0].size * 0.5f;
    float2 local[4] = {
        float2(-1.0f, -1.0f), float2(1.0f, -1.0f),
        float2(-1.0f,  1.0f), float2(1.0f,  1.0f)
    };

    PSInput o;
    [unroll]
    for (int i = 0; i < 4; ++i) {
        float3 p = pos + (local[i].x * vRight + local[i].y * vUp) * h;
        o.pos      = mul(float4(p, 1.0f), matViewProj);
        o.local    = local[i];
        o.hdrColor = input[0].hdrColor;
        o.uv       = input[0].uv;
        o.morphT   = input[0].morphT;
        triStream.Append(o);
    }
}

float4 PSMain(PSInput input) : SV_TARGET {
    // Round soft dot.
    float r2 = dot(input.local, input.local);
    if (r2 > 1.0f) discard;
    float falloff = saturate(1.0f - r2);
    falloff *= falloff;

    // Start as the corpse's albedo (dim), end as the glowing HDR sphere color.
    float3 albedo = (idxAlbedo.x >= 0) ? sampleBindless(idxAlbedo, input.uv).rgb
                                       : float3(0.5f, 0.5f, 0.5f);
    float3 color = lerp(albedo, input.hdrColor, input.morphT);

    return float4(color * falloff, 1.0f);
}

#define MAX_CSM_CASCADES 4

struct PerInstanceData {
    float4x4 world;
    uint rootBoneOffset;
    uint3 padding;
};

cbuffer PerDrawcallData : register(b0) {
    uint firstInstanceOffset;
    uint3 padding;
};

cbuffer PerFrameData : register(b1) {
    float4x4 lightVP[MAX_CSM_CASCADES];
    uint     cascadeCount;
    uint3    _pfd0;
}

StructuredBuffer<PerInstanceData> gInstances : register(t0);
StructuredBuffer<float4x4> gBoneData: register(t2);

float4x4 blendBoneTransform(uint rootBoneOffset, uint4 boneIndices, float4 boneWeights) {
    return
        gBoneData[rootBoneOffset + boneIndices.x] * boneWeights.x +
        gBoneData[rootBoneOffset + boneIndices.y] * boneWeights.y +
        gBoneData[rootBoneOffset + boneIndices.z] * boneWeights.z +
        gBoneData[rootBoneOffset + boneIndices.w] * boneWeights.w;
}

// VS: applies bone transform and outputs world-space position.
// GS handles projection per cascade and routes each triangle to the correct DSV slice.
struct VSOut {
    float4 posW : POSITION_W;
};

struct GSOut {
    float4 pos      : SV_Position;
    uint   sliceIdx : SV_RenderTargetArrayIndex;
};

VSOut VSMain(
    float3 position : POSITION,
    int4 boneIndices: BONE_INDICES,
    float4 boneWeights: BONE_WEIGHTS,
    uint idxInst : SV_InstanceID
) {
    float4x4 anim = blendBoneTransform(
        gInstances[idxInst + firstInstanceOffset].rootBoneOffset,
        boneIndices, boneWeights
    );

    float4 animatedPos = mul(float4(position, 1.0f), anim);

    VSOut ret;
    ret.posW = mul(animatedPos, gInstances[idxInst + firstInstanceOffset].world);
    return ret;
}

[maxvertexcount(3 * MAX_CSM_CASCADES)]
void GSMain(triangle VSOut input[3], inout TriangleStream<GSOut> stream) {
    for (uint cascade = 0; cascade < cascadeCount; ++cascade) {
        [unroll]
        for (uint v = 0; v < 3; ++v) {
            GSOut o;
            o.pos      = mul(input[v].posW, lightVP[cascade]);
            o.sliceIdx = cascade;
            stream.Append(o);
        }
        stream.RestartStrip();
    }
}
// No PSMain: depth-only pass, NumRenderTargets = 0

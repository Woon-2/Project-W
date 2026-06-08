struct PerInstanceData {
    float4x4 world;
    float3 aabbMin;
    float padding0;
    float3 aabbMax;
    float padding1;
    uint instanceGroupId;
    uint instanceObjId;
    uint2 padding2;
};

cbuffer PerFrameData : register(b1) {
    float4x4 viewProj;
    float2 screenSize;
    float2 padding0;
    uint objCnt;
    uint3 padding1;
};

Texture2D<float> hiZMap : register(t0, space1);

void ProjectAABBToScreen(
    float4x4 world,
    float3 aabbMin, float3 aabbMax,
    out float2 minUV,
    out float2 maxUV,
    out float minDepth
) {
    float3 corners[8] = {
        float3(aabbMin.x, aabbMin.y, aabbMin.z),
        float3(aabbMax.x, aabbMin.y, aabbMin.z),
        float3(aabbMin.x, aabbMax.y, aabbMin.z),
        float3(aabbMax.x, aabbMax.y, aabbMin.z),
        float3(aabbMin.x, aabbMin.y, aabbMax.z),
        float3(aabbMax.x, aabbMin.y, aabbMax.z),
        float3(aabbMin.x, aabbMax.y, aabbMax.z),
        float3(aabbMax.x, aabbMax.y, aabbMax.z),
    };
    
    minUV = float2( 1e9,  1e9);
    maxUV = float2(-1e9, -1e9);
    minDepth = 1.0;

    [unroll]
    for (int i = 0; i < 8; i++)
    {
        float4 clip = mul(mul(float4(corners[i], 1.0), world), viewProj);
        float3 ndc = clip.xyz / clip.w;

        float2 uv = ndc.xy * 0.5f + 0.5f;
        uv.y = 1.0f - uv.y;

        minUV = min(minUV, uv);
        maxUV = max(maxUV, uv);

        minDepth = min(minDepth, ndc.z);
    }
}

uint ComputeMipLevel(float2 minUV, float2 maxUV) {
    float2 size = (maxUV - minUV) * screenSize;

    float maxDim = max(size.x, size.y);

    uint mip = (uint)clamp(floor(log2(maxDim)), 0, 10);

    return mip;
}

bool OcclusionTest(float2 minUV, float2 maxUV, float depth)
{
    uint mip = ComputeMipLevel(minUV, maxUV);
    uint _;

    int2 texSize;
    hiZMap.GetDimensions(mip, texSize.x, texSize.y, _);

    int2 minXY = int2(minUV * texSize);
    int2 maxXY = int2(maxUV * texSize);

    float maxDepth = 0.f;

    maxDepth = max(maxDepth, hiZMap.Load(int3(minXY, mip)));
    maxDepth = max(maxDepth, hiZMap.Load(int3(int2(maxXY.x, minXY.y), mip)));
    maxDepth = max(maxDepth, hiZMap.Load(int3(int2(minXY.x, maxXY.y), mip)));
    maxDepth = max(maxDepth, hiZMap.Load(int3(maxXY, mip)));

    // �ٽ� ��
    return depth <= maxDepth;
}

StructuredBuffer<PerInstanceData> gInstances : register(t0);
RWStructuredBuffer<uint> gCntPerGroups : register(u1);
RWStructuredBuffer<uint> gVisibleFlags : register(u2);
// visibility feedback: packed (objId<<1 | visBit), one entry per instance.
// The slot base is selected on the C++ side via a root-UAV byte offset,
// so gVisibility[idx] is slot-relative here.
RWStructuredBuffer<uint> gVisibility : register(u3);

[numthreads(64, 1, 1)]
void CSMain(uint3 id : SV_DispatchThreadID)
{
    uint idx = id.x;

    if (idx >= objCnt)
        return;

    float2 minUV, maxUV;
    float depth;

    ProjectAABBToScreen(gInstances[idx].world, gInstances[idx].aabbMin, gInstances[idx].aabbMax, minUV, maxUV, depth);

    minUV = saturate(minUV);
    maxUV = saturate(maxUV);

    uint visBit = OcclusionTest(minUV, maxUV, depth) ? 1u : 0u;

    if (visBit)
        InterlockedAdd(gCntPerGroups[gInstances[idx].instanceGroupId], 1);

    gVisibleFlags[idx] = visBit;

    // CPU feedback: self-describing entry, so slots stay frame-independent.
    gVisibility[idx] = (gInstances[idx].instanceObjId << 1u) | visBit;
}
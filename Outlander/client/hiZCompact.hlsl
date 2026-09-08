struct PerInstanceData {
    uint instanceGroupId;
    uint idxCnt;
    uint2 padding;
};

struct PerGroupData {
    uint instCnt;
    uint idxCnt;
    uint groupOffset;
    uint padding;
};

cbuffer PerFrameData : register(b1) {
    uint objCnt;
    uint3 padding1;
};

StructuredBuffer<PerInstanceData> gInstances : register(t0);
StructuredBuffer<uint> gGroupOffsets : register(t3);
StructuredBuffer<uint> gVisibleFlags : register(t4);
RWStructuredBuffer<uint> gVisibleIndices : register(u1);
RWStructuredBuffer<PerGroupData> gOutGroups : register(u3);

[numthreads(64, 1, 1)]
void CSMain(uint3 id : SV_DispatchThreadID)
{
    uint idx = id.x;
    if (idx >= objCnt)
        return;

    if (gVisibleFlags[idx])
    {
        uint groupId = gInstances[idx].instanceGroupId;
        gOutGroups[groupId].idxCnt = gInstances[idx].idxCnt;
        gOutGroups[groupId].groupOffset = gGroupOffsets[groupId];

        uint localOffset;
        InterlockedAdd(gOutGroups[groupId].instCnt, 1, localOffset);

        uint globalOffset = gGroupOffsets[groupId] + localOffset;

        gVisibleIndices[globalOffset] = idx;
    }
}
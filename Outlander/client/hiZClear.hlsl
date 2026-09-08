struct PerGroupData {
    uint instCnt;
    uint idxCnt;
    uint groupOffset;
    uint padding;
};

cbuffer PerFrameData : register(b1) {
    uint groupCnt;
    uint3 padding1;
};

RWStructuredBuffer<uint>         gPerGroupCnt  : register(u1);
RWStructuredBuffer<PerGroupData> gPerGroupData : register(u3);

[numthreads(64, 1, 1)]
void CSMain(uint3 id : SV_DispatchThreadID)
{
    uint idx = id.x;
    if (idx >= groupCnt)
        return;

    gPerGroupCnt[idx]        = 0;
    gPerGroupData[idx].instCnt = 0;
}

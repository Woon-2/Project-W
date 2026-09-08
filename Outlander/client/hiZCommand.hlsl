struct PerGroupData {
    uint instCnt;
    uint idxCnt;
    uint groupOffset;
    uint padding;
};

struct DrawIndexedInstancedArgs
{
    uint IndexCountPerInstance;
    uint InstanceCount;
    uint StartIndexLocation;
    int  BaseVertexLocation;
    uint StartInstanceLocation;
};

struct IndirectCommand
{
    uint groupOffset;
    DrawIndexedInstancedArgs drawArgs;
};
    
cbuffer PerFrameData : register(b1) {
    uint groupCnt;
    uint3 padding1;
};

StructuredBuffer<PerGroupData> gGroups : register(t5);
RWStructuredBuffer<IndirectCommand> gOutArgs : register(u0, space1);

[numthreads(64, 1, 1)]
void CSMain(uint3 id : SV_DispatchThreadID)
{
    uint idx = id.x;
    if (idx >= groupCnt)
        return;

    gOutArgs[idx].groupOffset = gGroups[idx].groupOffset;
    gOutArgs[idx].drawArgs.IndexCountPerInstance = gGroups[idx].idxCnt;
    gOutArgs[idx].drawArgs.InstanceCount = gGroups[idx].instCnt;
    gOutArgs[idx].drawArgs.StartIndexLocation = 0u;
    gOutArgs[idx].drawArgs.BaseVertexLocation = 0;
    gOutArgs[idx].drawArgs.StartInstanceLocation = 0;
}
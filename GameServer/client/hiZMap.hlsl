// src: mip N-1
// dst: mip N

cbuffer PushData : register(b0, space1) { uint srcMip; uint3 _pad; }

Texture2D<float> srcTex : register(t0, space1);
RWTexture2D<float> dstTex : register(u0);

[numthreads(8, 8, 1)]
void CSMain(uint3 id : SV_DispatchThreadID)
{
    uint2 base = id.xy * 2;

    float d0 = srcTex.Load(int3(base,                  srcMip));
    float d1 = srcTex.Load(int3(base + uint2(1, 0),    srcMip));
    float d2 = srcTex.Load(int3(base + uint2(0, 1),    srcMip));
    float d3 = srcTex.Load(int3(base + uint2(1, 1),    srcMip));

    float maxDepth = max(max(d0, d1), max(d2, d3));

    dstTex[id.xy] = maxDepth;
}

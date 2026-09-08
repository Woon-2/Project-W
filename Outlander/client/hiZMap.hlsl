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

    // Reversed-Z: 가까움=1.0, 멀어짐=0.0. 2x2 셀 중 가장 먼(가장 작은) occluder depth를
    // 보존해야 conservative occlusion test가 유지된다.
    float minDepth = min(min(d0, d1), min(d2, d3));

    dstTex[id.xy] = minDepth;
}

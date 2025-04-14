StructuredBuffer<float4x4> matA : register(t3);
StructuredBuffer<float4x4> matB : register(t4);
RWStructuredBuffer<float4x4> matC : register(u0);

[numthreads(256, 1, 1)]
void CSMain( uint3 dispatchThreadID : SV_DispatchThreadID ) {
    matC[dispatchThreadID.x] = mul(matA[dispatchThreadID.x], matB[dispatchThreadID.x]);
}
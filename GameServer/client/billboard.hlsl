#include "bindless.hlsli"

struct PerInstanceData {
    float4x4 world;
    float    rotation;
    float3   pad;
};

struct Material
{
    int4 idxTex;
    float3 tint;
};

// VS -> GS: world-space data only. Clip-space transform is done in GS.
// NOTE: Do NOT use SV_Position here — hardware clips points based on SV_Position
//       before the GS runs, which would cull any particle outside NDC [-1,1].
struct VSOutput
{
    float3 worldPos : TEXCOORD0;
    float  size     : TEXCOORD1;
    float  rotation : TEXCOORD2;
};

struct PSInput
{
    float4 pos : SV_Position;
    float3 normal : NORMAL;
    float2 uv : UV;
};

cbuffer PerDrawcallData : register(b0) {
    Material material;
    uint idxDrawcall;
};

cbuffer PerFrameData : register(b1) {
    float4x4 matViewProj;
    float3 cameraPosW;  // world-space camera position
    float padding0;
};

StructuredBuffer<PerInstanceData> gInstances : register(t0);

// SIZE input layout is DXGI_FORMAT_R32_FLOAT (single float), so declare float here.
VSOutput VSMain( float3 position : POSITION, float size : SIZE,
    uint idxInst : SV_InstanceID
) {
    VSOutput ret;

    PerInstanceData instance = gInstances[idxInst + idxDrawcall];
    ret.worldPos = mul(float4(position, 1.0f), instance.world).xyz;

    // Extract true X scale from world matrix column length (robust against rotation).
    float scaleX = length(float3(instance.world._m00, instance.world._m10, instance.world._m20));
    ret.size     = size * scaleX;
    ret.rotation = instance.rotation;

    return ret;
}

[maxvertexcount(4)]
void GSMain(point VSOutput input[1],
    inout TriangleStream<PSInput> triStream
) {
    float3 pos   = input[0].worldPos;
    float3 vLook = normalize(cameraPosW - pos);

    // Fallback when camera is directly above/below the particle.
    float3 worldUp = (abs(vLook.y) < 0.999f) ? float3(0.0f, 1.0f, 0.0f)
                                              : float3(1.0f, 0.0f, 0.0f);
    float3 vRight = normalize(cross(worldUp, vLook));
    float3 vUP    = cross(vLook, vRight);

    float c = cos(input[0].rotation);
    float s = sin(input[0].rotation);
    float3 vRightR = c * vRight + s * vUP;
    float3 vUPR    = -s * vRight + c * vUP;

    float half = input[0].size * 0.5f;

    // Triangle strip order: bottom-left(0), bottom-right(1), top-left(2), top-right(3)
    float3 p[4];
    p[0] = pos - half * vRightR - half * vUPR;
    p[1] = pos + half * vRightR - half * vUPR;
    p[2] = pos - half * vRightR + half * vUPR;
    p[3] = pos + half * vRightR + half * vUPR;
    float2 uv[4] = { float2(0.f, 1.f), float2(1.f, 1.f), float2(0.f, 0.f), float2(1.f, 0.f) };

    PSInput output;
    [unroll]
    for(int i = 0; i < 4; ++i) {
        output.pos    = mul(float4(p[i], 1.0f), matViewProj);
        output.normal = vLook;
        output.uv     = uv[i];
        triStream.Append(output);
    }
}

float4 PSMain(PSInput input) : SV_TARGET {
    float4 src = sampleBindless(material.idxTex, input.uv);
    return float4(src.xyz * material.tint, src.a);
}
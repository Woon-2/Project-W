#include "bindless.hlsli"

struct PerInstanceData {
    float4x4 world;
    float4x4 rotation3D;
    float4   stretchAxisAndMode; // xyz=world-space stretch axis, w=1 when enabled
    float    rotation;
    float    alignmentMode;      // 0=view-facing, 1=local/world axes
    float2   pad;
};

struct Material
{
    int4 idxTex;
    float4 tint;    // RGBA — rgb: 색 multiplier, a: 알파 multiplier
};

// VS -> GS: world-space data only. Clip-space transform is done in GS.
// NOTE: Do NOT use SV_Position here — hardware clips points based on SV_Position
//       before the GS runs, which would cull any particle outside NDC [-1,1].
struct VSOutput
{
    float3 worldPos : TEXCOORD0;
    float2 size     : TEXCOORD1;
    float  rotation : TEXCOORD2;
    float3 stretchAxisW : TEXCOORD3;
    float  stretchMode  : TEXCOORD4;
    float3 rotX : TEXCOORD5;
    float3 rotY : TEXCOORD6;
    float3 rotZ : TEXCOORD7;
    float  alignmentMode : TEXCOORD8;
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
    float2 uvOffset;    // 스프라이트 시트 내 현재 프레임의 좌상단 UV
    float pad0;         // 명시적 패딩 — C++ pad_와 대응, float2 uvScale을 register 경계(48B)에 정렬
    float2 uvScale;     // 현재 프레임의 UV 크기 (= 1/cols, 1/rows)
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

    // Extract true XY scale from world matrix column length (robust against rotation).
    float scaleX = length(float3(instance.world._m00, instance.world._m10, instance.world._m20));
    float scaleY = length(float3(instance.world._m01, instance.world._m11, instance.world._m21));
    ret.size     = float2(size * scaleX, size * scaleY);
    ret.rotation = instance.rotation;
    ret.stretchAxisW = instance.stretchAxisAndMode.xyz;
    ret.stretchMode = instance.stretchAxisAndMode.w;
    ret.rotX = mul(float4(1.0f, 0.0f, 0.0f, 0.0f), instance.rotation3D).xyz;
    ret.rotY = mul(float4(0.0f, 1.0f, 0.0f, 0.0f), instance.rotation3D).xyz;
    ret.rotZ = mul(float4(0.0f, 0.0f, 1.0f, 0.0f), instance.rotation3D).xyz;
    ret.alignmentMode = instance.alignmentMode;

    return ret;
}

float3 rotateBillboardOffset(float2 localXY, VSOutput input,
                             float3 rightW, float3 upW, float3 lookW) {
    float3 rotatedLocal = localXY.x * input.rotX + localXY.y * input.rotY;
    return rotatedLocal.x * rightW
         + rotatedLocal.y * upW
         + rotatedLocal.z * lookW;
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

    float3 vRightR = vRight;
    float3 vUPR = vUP;

    float3 stretchAxis = input[0].stretchAxisW;
    float3 stretchAxisOnPlane = stretchAxis - dot(stretchAxis, vLook) * vLook;
    float stretchLenSq = dot(stretchAxisOnPlane, stretchAxisOnPlane);
    if (input[0].stretchMode > 0.5f && stretchLenSq > 0.000001f) {
        float3 stretchUp    = stretchAxisOnPlane * rsqrt(stretchLenSq);
        float3 stretchRight = normalize(cross(stretchUp, vLook));
        float c = cos(input[0].rotation);
        float s = sin(input[0].rotation);
        vUPR    =  c * stretchUp + s * stretchRight;
        vRightR = -s * stretchUp + c * stretchRight;
    } else {
        float c = cos(input[0].rotation);
        float s = sin(input[0].rotation);
        vRightR = c * vRight + s * vUP;
        vUPR    = -s * vRight + c * vUP;
    }

    float halfWidth = input[0].size.x * 0.5f;
    float halfHeight = input[0].size.y * 0.5f;

    // Triangle strip order: bottom-left(0), bottom-right(1), top-left(2), top-right(3)
    float2 local[4];
    local[0] = float2(-halfWidth, -halfHeight);
    local[1] = float2( halfWidth, -halfHeight);
    local[2] = float2(-halfWidth,  halfHeight);
    local[3] = float2( halfWidth,  halfHeight);

    float3 p[4];
    if (input[0].alignmentMode > 0.5f && input[0].stretchMode < 0.5f) {
        float c = cos(input[0].rotation);
        float s = sin(input[0].rotation);
        float3 axisX = normalize(c * input[0].rotX + s * input[0].rotY);
        float3 axisY = normalize(-s * input[0].rotX + c * input[0].rotY);

        [unroll]
        for (int k = 0; k < 4; ++k)
            p[k] = pos + local[k].x * axisX + local[k].y * axisY;
    } else {
        [unroll]
        for (int k = 0; k < 4; ++k)
            p[k] = pos + rotateBillboardOffset(local[k], input[0], vRightR, vUPR, vLook);
    }

    float3 quadNormal = normalize(cross(p[1] - p[0], p[2] - p[0]));
    // 로컬 UV (0~1 범위) → 스프라이트 시트 UV로 변환
    float2 baseUV[4] = { float2(0.f, 1.f), float2(1.f, 1.f), float2(0.f, 0.f), float2(1.f, 0.f) };
    float2 uv[4];
    [unroll]
    for (int j = 0; j < 4; ++j)
        uv[j] = baseUV[j] * uvScale + uvOffset;

    PSInput output;
    [unroll]
    for(int i = 0; i < 4; ++i) {
        output.pos    = mul(float4(p[i], 1.0f), matViewProj);
        output.normal = quadNormal;
        output.uv     = uv[i];
        triStream.Append(output);
    }
}

float4 PSMain(PSInput input) : SV_TARGET {
    float4 src = sampleBindless(material.idxTex, input.uv);
    // finalColor = texture * tint.rgb (색 multiplier), alpha = texture.a * tint.a (알파 multiplier)
    return float4(src.xyz * material.tint.rgb, src.a * material.tint.a);
}

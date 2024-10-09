// The order of the includes is important, as some depend on others

#ifdef INCLUDE_phongLighting
#include "phongMaterial_t.hlsl"
#endif

#include "drawcallWithMaterial.hlsl"

#ifdef INCLUDE_phongLighting
#include "phongLightingShadowed_t.hlsl"
#endif

#ifdef INCLUDE_basicInstancing
#include "basicInstancing.hlsl"
#endif

// non textured shader

struct VS_INPUT {
    float3 pos : POSITION;
    float3 normal : NORMAL;
    float2 tex : TEXCOORD;
    uint instID : SV_InstanceID;
};

struct VS_OUTPUT {
    float4 pos : SV_Position;
    float4 shadowPos : TEXCOORD0;
    float3 posV : POSITION;
    float3 normalV : NORMAL;
    float2 tex : TEXCOORD1;
    nointerpolation uint instID : INSTID;
};

static matrix gMtxTexture = {
    0.5f, 0.f, 0.f, 0.f,
    0.f, -0.5f, 0.f, 0.f,
    0.f, 0.f, 1.f, 0.f,
    0.5f, 0.5f, 0.f, 1.f
};

VS_OUTPUT VSMain(VS_INPUT input) {
    VS_OUTPUT output;
    output.pos = mul( float4(input.pos, 1.f),
        getInstanceData(input.instID).wvp
    );
    output.posV = mul( float4(input.pos, 1.f),
        getInstanceData(input.instID).wv
    ).xyz;
    output.normalV = mul( input.normal,
        getInstanceData(input.instID).normalXform
    );
    output.tex = input.tex;
    output.instID = input.instID;
    output.shadowPos = mul( mul( float4(output.posV, 1.f), gView2LightProj ), gMtxTexture );

    return output;
}

float4 PSMain(VS_OUTPUT input) : SV_TARGET {
    float3 normalV = normalize(input.normalV);
    input.shadowPos.xy /= input.shadowPos.ww;
    float shadowed = gTex2DLUT[gShadowMapIdx].SampleCmpLevelZero(
        gShadowSampler, input.shadowPos.xy, input.shadowPos.z / input.shadowPos.w
    );
    return Lighting(input.posV, normalV, input.tex) * shadowed;
}
// The order of the includes is important, as some depend on others

#ifdef INCLUDE_drawcallIdxOnly
#include "drawcallIdxOnly.hlsl"
#endif

#ifdef INCLUDE_basicInstancing
#include "basicInstancing.hlsl"
#endif

#ifdef INCLUDE_phongLighting
#include "phongLighting.hlsl"
#endif

// non textured shader

struct VS_INPUT {
    float3 pos : POSITION;
    float3 normal : NORMAL;
    uint instID : SV_InstanceID;
};

struct VS_OUTPUT {
    float4 pos : SV_Position;
    float3 posV : POSITION;
    float3 normalV : NORMAL;
    nointerpolation uint instID : INSTID;
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
    output.instID = input.instID;

    return output;
}

float4 PSMain(VS_OUTPUT input) : SV_TARGET {
    float3 normalV = normalize(input.normalV);
    return Lighting(input.posV, normalV, getInstanceData(input.instID).matIdx);
}
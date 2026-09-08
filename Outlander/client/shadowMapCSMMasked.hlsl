// shadowMapCSMMasked.hlsl
// Alpha-tested ("masked") variant of the static-mesh CSM shadow pass.
// Used for foliage (tree leaves / billboard grass) whose albedo alpha defines the
// silhouette. The opaque shadow pass is depth-only (no PS) and therefore records the
// full quad, producing rectangular shadows. This variant samples the albedo alpha and
// clips so the shadow follows the cutout shape — the industry-standard masked shadow.
// Position + UV VS, alpha-test PS. Rendered two-sided (CULL_NONE) since foliage is thin.
// Uses the shared bindless texture/sampler pools (same as the GBuffer geometry pass).

#include "bindless.hlsli"

struct PerInstanceData {
    float4x4 world;
};

// Layout-compatible with ShadowMapCSMMaskedShader::PerDrawcallData (firstInstanceOffset
// at offset 0, then bindless albedo + cutoff). Bound via the shared b0 root CBV.
cbuffer PerDrawcallData : register(b0) {
    uint   firstInstanceOffset;
    uint3  _pad0;
    int4   idxAlbedo;       // x=range, y=index, z=arraySlice, w=sampler
    float  cAlphaCutoff;
    float3 _pad1;
};

cbuffer PerFrameData : register(b1) {
    float4x4 lightVP;       // current cascade's VP (maps CAMERA-RELATIVE world pos to light NDC)
    uint     cascadeIdx;
    uint3    _pfd0;
    float3   camPos;        // camera world position for the camera-relative shadow rebase
    float    _pfd1;
};

StructuredBuffer<PerInstanceData> gInstances : register(t0);

struct VSOutput {
    float4 pos : SV_Position;
    float2 uv  : UV;
};

VSOutput VSMain(
    float3 position : POSITION,
    float2 uv       : UV,
    uint   idxInst  : SV_InstanceID
) {
    VSOutput o;
    float3 posW = mul(float4(position, 1.0f), gInstances[idxInst + firstInstanceOffset].world).xyz;
    // Camera-relative shadow space: rebase by camPos to match the receiver side.
    o.pos = mul(float4(posW - camPos, 1.0f), lightVP);
    o.uv  = uv;
    return o;
}

// Depth-only target (NumRenderTargets = 0); the PS exists solely to clip cutout texels.
void PSMain(VSOutput input) {
    float alpha = sampleBindless(idxAlbedo, input.uv).a;
    clip(alpha - cAlphaCutoff);
}

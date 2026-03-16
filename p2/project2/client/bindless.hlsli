#ifndef __bindless_hlsl__
#define __bindless_hlsl__

#define IDX_RANGE_TEXTURE2D 0
#define IDX_RANGE_TEXTUREARRAY 1
#define IDX_RANGE_TEXTURECUBE 2

SamplerState gSamplers[] : register(s0, space1);
SamplerComparisonState gComparisonSamplers[] : register(s0, space2);

Texture2D gTex2Ds[] : register(t10, space1);
Texture2DArray gTex2DArrays[] : register(t10, space2);
TextureCube gTexCubes[] : register(t10, space3);

float4 loadBindless(uint4 bindlessIdx, int2 tex, int LOD) {
    if (bindlessIdx.x < 0) {
        return float4(0.f, 0.f, 0.f, 1.f);
    }
    if (bindlessIdx.x == IDX_RANGE_TEXTURE2D) {
        return gTex2Ds[bindlessIdx.y].Load(int3(tex, LOD));
    } else if (bindlessIdx.x == IDX_RANGE_TEXTUREARRAY) {
        return gTex2DArrays[bindlessIdx.y].Load(int4(tex, bindlessIdx.z, LOD));
    }
    return float4(0.f, 0.f, 0.f, 1.f);
}

float4 sampleBindless(uint4 bindlessIdx, float2 tex) {
    if (bindlessIdx.x < 0) {
        return float4(0.f, 0.f, 0.f, 1.f);
    }
    if (bindlessIdx.x == IDX_RANGE_TEXTURE2D) {
        return gTex2Ds[bindlessIdx.y].Sample(gSamplers[bindlessIdx.w], tex);
    } else if (bindlessIdx.x == IDX_RANGE_TEXTUREARRAY) {
        return gTex2DArrays[bindlessIdx.y].Sample(gSamplers[bindlessIdx.w], float3(tex, bindlessIdx.z));
    }
    return float4(0.f, 0.f, 0.f, 1.f);
}

float4 sampleBindlessCube(uint4 bindlessIdx, float3 tex) {
    if (bindlessIdx.x < 0) {
        return float4(0.f, 0.f, 0.f, 1.f);
    }
    if (bindlessIdx.x == IDX_RANGE_TEXTURECUBE) {
        return gTexCubes[bindlessIdx.y].Sample(gSamplers[bindlessIdx.w], tex);
    }
    return float4(0.f, 0.f, 0.f, 1.f);
}

float4 sampleLevelBindless(uint4 bindlessIdx, float2 tex, float LOD) {
    if (bindlessIdx.x < 0) {
        return float4(0.f, 0.f, 0.f, 1.f);
    }
    if (bindlessIdx.x == IDX_RANGE_TEXTURE2D) {
        return gTex2Ds[bindlessIdx.y].SampleLevel(gSamplers[bindlessIdx.w], tex, LOD);
    } else if (bindlessIdx.x == IDX_RANGE_TEXTUREARRAY) {
        return gTex2DArrays[bindlessIdx.y].SampleLevel(gSamplers[bindlessIdx.w], float3(tex, bindlessIdx.z), LOD);
    }
    return float4(0.f, 0.f, 0.f, 1.f);
}

float4 sampleBindless2DOffset(uint4 bindlessIdx, float2 tex, int2 offset) {
    if (bindlessIdx.x == IDX_RANGE_TEXTURE2D) {
        return gTex2Ds[bindlessIdx.y].Sample(gSamplers[bindlessIdx.w], tex, offset);
    } 
    return float4(0.f, 0.f, 0.f, 1.f);
}

float4 sampleLevelBindless2DOffset(uint4 bindlessIdx, float2 tex, float LOD, int2 offset) {
    if (bindlessIdx.x == IDX_RANGE_TEXTURE2D) {
        return gTex2Ds[bindlessIdx.y].SampleLevel(gSamplers[bindlessIdx.w], tex, LOD, offset);
    } 
    return float4(0.f, 0.f, 0.f, 1.f);
}

float sampleCmpBindless(uint4 bindlessIdx, float2 tex, float val) {
    if (bindlessIdx.x < 0) {
        return 1.f;
    }
    if (bindlessIdx.x == IDX_RANGE_TEXTURE2D) {
        return gTex2Ds[bindlessIdx.y].SampleCmpLevelZero(gComparisonSamplers[bindlessIdx.w], tex, val);
    } else if (bindlessIdx.x == IDX_RANGE_TEXTUREARRAY) {
        return gTex2DArrays[bindlessIdx.y].SampleCmpLevelZero(gComparisonSamplers[bindlessIdx.w], float3(tex, bindlessIdx.z), val);
    }
    return 0.f;
}

float sampleCmpBindless2DOffset(uint4 bindlessIdx, float2 tex, float val, int2 offset) {
    if (bindlessIdx.x == IDX_RANGE_TEXTURE2D) {
        return gTex2Ds[bindlessIdx.y].SampleCmpLevelZero(gComparisonSamplers[bindlessIdx.w], tex, val, offset);
    }
    else if (bindlessIdx.x == IDX_RANGE_TEXTUREARRAY)
    {
        return gTex2DArrays[bindlessIdx.y].SampleCmpLevelZero(gComparisonSamplers[bindlessIdx.w], float3(tex, bindlessIdx.z), val, offset);
    }
    return 0.f;
}

#endif // __bindless_hlsl__
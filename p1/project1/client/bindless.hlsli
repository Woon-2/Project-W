#ifndef __bindless_hlsl__
#define __bindless_hlsl__

#define MAP_TYPE_TEXTURE2D 0
#define MAP_TYPE_TEXTUREARRAY 1
#define MAP_TYPE_TEXTURECUBE 2

SamplerState gSamplers[] : register(s0, space1);
SamplerComparisonState gComparisonSamplers[] : register(s0, space2);

Texture2D gTex2Ds[] : register(t10, space1);
Texture2DArray gTex2DArrays[] : register(t10, space2);
TextureCube gTexCubes[] : register(t10, space3);

float4 loadBindless(uint4 bindlessIdx, int2 tex, int LOD) {
    if (bindlessIdx.x == uint(-1)) {
        return float4(0.f, 0.f, 0.f, 0.0f);
    }
    if (bindlessIdx.x == MAP_TYPE_TEXTURE2D) {
        return gTex2Ds[bindlessIdx.y].Load(int3(tex, LOD));
    } else if (bindlessIdx.x == MAP_TYPE_TEXTUREARRAY) {
        return gTex2DArrays[bindlessIdx.y].Load(int4(tex, bindlessIdx.z, LOD));
    } else /* if (bindlessIdx.x == MAP_TYPE_TEXTURECUBE) */ {
        // TODO: should return correctly  value.
        return float4(0.f, 0.f, 0.f, 0.0f);
    }
}

float4 sampleBindless(uint4 bindlessIdx, float2 tex) {
    if (bindlessIdx.x == uint(-1)) {
        return float4(0.f, 0.f, 0.f, 0.0f);
    }
    if (bindlessIdx.x == MAP_TYPE_TEXTURE2D) {
        return gTex2Ds[bindlessIdx.y].Sample(gSamplers[bindlessIdx.w], tex);
    } else if (bindlessIdx.x == MAP_TYPE_TEXTUREARRAY) {
        return gTex2DArrays[bindlessIdx.y].Sample(gSamplers[bindlessIdx.w], float3(tex, bindlessIdx.z));
    } else /* if (bindlessIdx.x == MAP_TYPE_TEXTURECUBE) */ {
        // TODO: should return correctly sampled value.
        return gTexCubes[bindlessIdx.y].Sample(gSamplers[bindlessIdx.w], float3(tex, 1.0f));
    }
}

float4 sampleLevelBindless(uint4 bindlessIdx, float2 tex, float LOD) {
    if (bindlessIdx.x == uint(-1)) {
        return float4(0.f, 0.f, 0.f, 0.0f);
    }
    if (bindlessIdx.x == MAP_TYPE_TEXTURE2D) {
        return gTex2Ds[bindlessIdx.y].SampleLevel(gSamplers[bindlessIdx.w], tex, LOD);
    } else if (bindlessIdx.x == MAP_TYPE_TEXTUREARRAY) {
        return gTex2DArrays[bindlessIdx.y].SampleLevel(gSamplers[bindlessIdx.w], float3(tex, bindlessIdx.z), LOD);
    } else /* if (bindlessIdx.x == MAP_TYPE_TEXTURECUBE) */ {
        // TODO: should return correctly sampled value.
        return gTexCubes[bindlessIdx.y].SampleLevel(gSamplers[bindlessIdx.w], float3(tex, 1.0f), LOD);
    }
}

float4 sampleBindless2DOffset(uint4 bindlessIdx, float2 tex, int2 offset) {
    if (bindlessIdx.x == MAP_TYPE_TEXTURE2D) {
        return gTex2Ds[bindlessIdx.y].Sample(gSamplers[bindlessIdx.w], tex, offset);
    } 
    return float4(0.f, 0.f, 0.f, 0.f);
}

float4 sampleLevelBindless2DOffset(uint4 bindlessIdx, float2 tex, float LOD, int2 offset) {
    if (bindlessIdx.x == MAP_TYPE_TEXTURE2D) {
        return gTex2Ds[bindlessIdx.y].SampleLevel(gSamplers[bindlessIdx.w], tex, LOD, offset);
    } 
    return float4(0.f, 0.f, 0.f, 0.f);
}

float sampleCmpBindless(uint4 bindlessIdx, float2 tex, float val) {
    if (bindlessIdx.x == uint(-1)) {
        return 1.f;
    }
    if (bindlessIdx.x == MAP_TYPE_TEXTURE2D) {
        return gTex2Ds[bindlessIdx.y].SampleCmpLevelZero(gComparisonSamplers[bindlessIdx.w], tex, val);
    } else if (bindlessIdx.x == MAP_TYPE_TEXTUREARRAY) {
        return gTex2DArrays[bindlessIdx.y].SampleCmpLevelZero(gComparisonSamplers[bindlessIdx.w], float3(tex, bindlessIdx.z), val);
    } else /* if (bindlessIdx.x == MAP_TYPE_TEXTURECUBE) */ {
        // TODO: should return correctly sampled value.
        return 0.f;
    }
}

float sampleCmpBindless2DOffset(uint4 bindlessIdx, float2 tex, float val, int2 offset) {
    if (bindlessIdx.x == MAP_TYPE_TEXTURE2D) {
        return gTex2Ds[bindlessIdx.y].SampleCmpLevelZero(gComparisonSamplers[bindlessIdx.w], tex, val, offset);
    }
    else if (bindlessIdx.x == MAP_TYPE_TEXTUREARRAY)
    {
        return gTex2DArrays[bindlessIdx.y].SampleCmpLevelZero(gComparisonSamplers[bindlessIdx.w], float3(tex, bindlessIdx.z), val, offset);
    }
    return float4(0.f, 0.f, 0.f, 0.f);
}

#endif // __bindless_hlsl__
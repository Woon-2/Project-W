#ifndef __bindless_hlsl__
#define __bindless_hlsl__

#include "samplers.hlsl"

#define MAP_TYPE_TEXTURE2D 0
#define MAP_TYPE_TEXTUREARRAY 1
#define MAP_TYPE_TEXTURECUBE 2

Texture2D gTex2Ds[] : register(t10, space1);
Texture2DArray gTex2DArrays[] : register(t10, space2);
TextureCube gTexCubes[] : register(t10, space3);

float4 sampleFromMapRef(uint4 mapRef, float2 tex, uint samIdx) {
    if (mapRef.x == uint(-1)) {
        return float4(0.f, 0.f, 0.f, 0.0f);
    }
    if (mapRef.x == MAP_TYPE_TEXTURE2D) {
        return gTex2Ds[mapRef.y].Sample(gSamplers[samIdx], tex);
    } else if (mapRef.x == MAP_TYPE_TEXTUREARRAY) {
        return gTex2DArrays[mapRef.y].Sample(gSamplers[samIdx], float3(tex, mapRef.z));
    } else /* if (mapRef.x == MAP_TYPE_TEXTURECUBE) */ {
        // TODO: should return correctly sampled value.
        return gTexCubes[mapRef.y].Sample(gSamplers[samIdx], float3(tex, 1.0f));
    }
}

float4 sampleFromMapRef2DOffset(uint4 mapRef, float2 tex, int2 offset, uint samIdx) {
    if (mapRef.x == MAP_TYPE_TEXTURE2D) {
        return gTex2Ds[mapRef.y].Sample(gSamplers[samIdx], tex, offset);
    } 
    return float4(0.f, 0.f, 0.f, 0.f);
}

float sampleCmpFromMapRef(uint4 mapRef, float2 tex, float val, uint samIdx) {
    if (mapRef.x == uint(-1)) {
        return 1.f;
    }
    if (mapRef.x == MAP_TYPE_TEXTURE2D) {
        return gTex2Ds[mapRef.y].SampleCmpLevelZero(gComparisonSamplers[samIdx], tex, val);
    } else if (mapRef.x == MAP_TYPE_TEXTUREARRAY) {
        return gTex2DArrays[mapRef.y].SampleCmpLevelZero(gComparisonSamplers[samIdx], float3(tex, mapRef.z), val);
    } else /* if (mapRef.x == MAP_TYPE_TEXTURECUBE) */ {
        // TODO: should return correctly sampled value.
        return 0.f;
    }
}

#endif // __bindless_hlsl__
#ifndef __bindless_hlsl__
#define __bindless_hlsl__

#define IDX_RANGE_TEXTURE2D 0
#define IDX_RANGE_TEXTUREARRAY 1
#define IDX_RANGE_TEXTURECUBE 2
#define IDX_RANGE_TEXTURE3D 3

SamplerState gSamplers[] : register(s0, space1);
SamplerComparisonState gComparisonSamplers[] : register(s0, space2);

Texture2D gTex2Ds[] : register(t10, space1);
Texture2DArray gTex2DArrays[] : register(t10, space2);
TextureCube gTexCubes[] : register(t10, space3);
Texture3D gTex3Ds[] : register(t10, space4);

float4 loadBindless(int4 bindlessIdx, int2 tex, int LOD) {
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

float4 sampleBindless(int4 bindlessIdx, float2 tex) {
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

float4 sampleBindlessCube(int4 bindlessIdx, float3 tex) {
    if (bindlessIdx.x < 0) {
        return float4(0.f, 0.f, 0.f, 1.f);
    }
    if (bindlessIdx.x == IDX_RANGE_TEXTURECUBE) {
        return gTexCubes[bindlessIdx.y].Sample(gSamplers[bindlessIdx.w], tex);
    }
    return float4(0.f, 0.f, 0.f, 1.f);
}

// 3D LUT(color grading 등) 샘플링. tex는 보통 [0,1]^3 큐브 좌표(예: LDR RGB 색상값)이다.
// bindlessIdx.z에 LUT 한 축의 해상도(N, LUT_3D_SIZE)가 들어있다 — Tex3D는 array slice가
// 없으므로 idxInArray 슬롯을 재활용한다.
// half-texel 보정: 텍셀 i는 값 i/(N-1)을 나타내도록 구워졌지만, 하드웨어 샘플링은
// UV=v를 텍셀 연속좌표 v*N - 0.5로 매핑한다(텍셀 중심이 (i+0.5)/N). 보정 없이 샘플링하면
// 그리드 인덱스가 어긋나 그레이딩 커브가 과장되고, identity LUT조차 완전한 passthrough가 되지 않는다.
float3 sampleBindless3D(int4 bindlessIdx, float3 tex) {
    if (bindlessIdx.x < 0) {
        return float3(0.f, 0.f, 0.f);
    }
    if (bindlessIdx.x == IDX_RANGE_TEXTURE3D) {
        float lutSize = (float)bindlessIdx.z;
        float3 uvw = tex * ((lutSize - 1.0f) / lutSize) + (0.5f / lutSize);
        return gTex3Ds[bindlessIdx.y].Sample(gSamplers[bindlessIdx.w], uvw).rgb;
    }
    return float3(0.f, 0.f, 0.f);
}

float4 sampleLevelBindless(int4 bindlessIdx, float2 tex, float LOD) {
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

float4 sampleLevelBindlessCube(int4 bindlessIdx, float3 tex, float LOD) {
    if (bindlessIdx.x < 0) {
        return float4(0.f, 0.f, 0.f, 1.f);
    }
    if (bindlessIdx.x == IDX_RANGE_TEXTURECUBE) {
        return gTexCubes[bindlessIdx.y].SampleLevel(gSamplers[bindlessIdx.w], tex, LOD);
    }
    return float4(0.f, 0.f, 0.f, 1.f);
}

float4 sampleBindless2DOffset(int4 bindlessIdx, float2 tex, int2 offset) {
    if (bindlessIdx.x == IDX_RANGE_TEXTURE2D) {
        return gTex2Ds[bindlessIdx.y].Sample(gSamplers[bindlessIdx.w], tex, offset);
    } 
    return float4(0.f, 0.f, 0.f, 1.f);
}

float4 sampleLevelBindless2DOffset(int4 bindlessIdx, float2 tex, float LOD, int2 offset) {
    if (bindlessIdx.x == IDX_RANGE_TEXTURE2D) {
        return gTex2Ds[bindlessIdx.y].SampleLevel(gSamplers[bindlessIdx.w], tex, LOD, offset);
    } 
    return float4(0.f, 0.f, 0.f, 1.f);
}

float sampleCmpBindless(int4 bindlessIdx, float2 tex, float val) {
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

float sampleCmpBindless2DOffset(int4 bindlessIdx, float2 tex, float val, int2 offset) {
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
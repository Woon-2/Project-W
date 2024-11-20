#ifndef __samplers_hlsl__
#define __samplers_hlsl__

SamplerState gSamplers[] : register(s0, space1);
SamplerComparisonState gComparisonSamplers[] : register(s0, space2);

#endif // __samplers_hlsl__
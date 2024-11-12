#ifndef __bindless_hlsl__
#define __bindless_hlsl__

Texture2D gTex2Ds[] : register(t10, space1);
Texture2DArray gTex2DArrays[] : register(t10, space2);
TextureCube gTexCubes[] : register(t10, space3);

#endif // __bindless_hlsl__
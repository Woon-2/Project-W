// Compiled shader for Windows, Mac, Linux

//////////////////////////////////////////////////////////////////////////
// 
// NOTE: This is *not* a valid shader file, the contents are provided just
// for information and for debugging purposes only.
// 
//////////////////////////////////////////////////////////////////////////
// Skipping shader variants that would not be included into build of current scene.

Shader "Shader Graphs/HS_Blend_CG" {
Properties {
 _MainTex ("MainTex", 2D) = "white" { }
 _Noise ("Noise", 2D) = "white" { }
 _Flow ("Flow", 2D) = "white" { }
 _Mask ("Mask", 2D) = "white" { }
 _SpeedMainTexUVNoiseZW ("Speed MainTex U/V + Noise Z/W", Vector) = (0.000000,0.000000,0.000000,0.000000)
 _DistortionSpeedXYPowerZ ("Distortion Speed XY Power Z", Vector) = (0.000000,0.000000,0.000000,0.000000)
 _Emission ("Emission", Float) = 1.000000
[HDR]  _Color ("Color", Color) = (1.000000,1.000000,1.000000,1.000000)
 _Opacity ("Opacity", Float) = 1.000000
[ToggleUI]  _Usecenterglow ("Use center glow?", Float) = 0.000000
[ToggleUI]  _Usedepth ("Use depth?", Float) = 0.000000
 _Depthpower ("Depth power", Float) = 1.000000
[ToggleUI]  _Use_fresnel ("Use fresnel", Float) = 0.000000
 _Fresnelpower ("Fresnel power", Float) = 3.000000
 _Fresnelscale ("Fresnel scale", Float) = 3.000000
[ToggleUI]  _Useonlycolor ("Use only color", Float) = 0.000000
 _Textureopacity ("Texture opacity", Range(0.000000,1.000000)) = 0.000000
[ToggleUI]  _Multiply_texture ("Multiply texture", Float) = 1.000000
[HideInInspector]  _CastShadows ("_CastShadows", Float) = 0.000000
[HideInInspector]  _Surface ("_Surface", Float) = 1.000000
[HideInInspector]  _Blend ("_Blend", Float) = 0.000000
[HideInInspector]  _AlphaClip ("_AlphaClip", Float) = 0.000000
[HideInInspector]  _SrcBlend ("_SrcBlend", Float) = 1.000000
[HideInInspector]  _DstBlend ("_DstBlend", Float) = 0.000000
[HideInInspector]  _SrcBlendAlpha ("_SrcBlendAlpha", Float) = 1.000000
[HideInInspector]  _DstBlendAlpha ("_DstBlendAlpha", Float) = 0.000000
[HideInInspector] [ToggleUI]  _ZWrite ("_ZWrite", Float) = 0.000000
[HideInInspector]  _ZWriteControl ("_ZWriteControl", Float) = 0.000000
[HideInInspector]  _ZTest ("_ZTest", Float) = 4.000000
[HideInInspector]  _Cull ("_Cull", Float) = 0.000000
[HideInInspector]  _AlphaToMask ("_AlphaToMask", Float) = 0.000000
[HideInInspector]  _QueueOffset ("_QueueOffset", Float) = 0.000000
[HideInInspector]  _QueueControl ("_QueueControl", Float) = -1.000000
[HideInInspector] [NoScaleOffset]  unity_Lightmaps ("unity_Lightmaps", 2DArray) = "" { }
[HideInInspector] [NoScaleOffset]  unity_LightmapsInd ("unity_LightmapsInd", 2DArray) = "" { }
[HideInInspector] [NoScaleOffset]  unity_ShadowMasks ("unity_ShadowMasks", 2DArray) = "" { }
[HideInInspector]  _BUILTIN_Surface ("Float", Float) = 1.000000
[HideInInspector]  _BUILTIN_Blend ("Float", Float) = 0.000000
[HideInInspector]  _BUILTIN_AlphaClip ("Float", Float) = 0.000000
[HideInInspector]  _BUILTIN_SrcBlend ("Float", Float) = 1.000000
[HideInInspector]  _BUILTIN_DstBlend ("Float", Float) = 0.000000
[HideInInspector]  _BUILTIN_ZWrite ("Float", Float) = 0.000000
[HideInInspector]  _BUILTIN_ZWriteControl ("Float", Float) = 0.000000
[HideInInspector]  _BUILTIN_ZTest ("Float", Float) = 4.000000
[HideInInspector]  _BUILTIN_CullMode ("Float", Float) = 0.000000
[HideInInspector]  _BUILTIN_QueueOffset ("Float", Float) = 0.000000
[HideInInspector]  _BUILTIN_QueueControl ("Float", Float) = -1.000000
}
SubShader { 
 Tags { "QUEUE"="Transparent" "RenderType"="Transparent" "DisableBatching"="False" "RenderPipeline"="UniversalPipeline" "UniversalMaterialType"="Unlit" "ShaderGraphShader"="true" "ShaderGraphTargetId"="UniversalUnlitSubTarget" }


 // Stats for Vertex shader:
 //        d3d11: 15 math
 // Stats for Fragment shader:
 //        d3d11: 76 avg math (71..81)
 Pass {
  Name "Universal Forward"
  Tags { "QUEUE"="Transparent" "RenderType"="Transparent" "DisableBatching"="False" "RenderPipeline"="UniversalPipeline" "UniversalMaterialType"="Unlit" "ShaderGraphShader"="true" "ShaderGraphTargetId"="UniversalUnlitSubTarget" }
  AlphaToMask [_AlphaToMask]
  ZTest [_ZTest]
  ZWrite [_ZWrite]
  Cull [_Cull]
  Blend [_SrcBlend] [_DstBlend], [_SrcBlendAlpha] [_DstBlendAlpha]
  //////////////////////////////////
  //                              //
  //      Compiled programs       //
  //                              //
  //////////////////////////////////
//////////////////////////////////////////////////////
Keywords: LIGHTMAP_BICUBIC_SAMPLING
-- Hardware tier variant: Tier 1
-- Vertex shader for "d3d11":
// Stats: 15 math, 2 temp registers
Uses vertex data channel "Vertex"
Uses vertex data channel "Normal"
Uses vertex data channel "TexCoord0"
Uses vertex data channel "TexCoord1"
Uses vertex data channel "Color"

Constant Buffer "$Globals" (2096 bytes) on slot 0 {
  Matrix4x4 unity_MatrixVP at 1248
}
Constant Buffer "UnityPerDraw" (720 bytes) on slot 1 {
  Matrix4x4 unity_ObjectToWorld at 0
  Matrix4x4 unity_WorldToObject at 64
}

Shader Disassembly:
//
// Generated by Microsoft (R) D3D Shader Disassembler
//
//
// Input signature:
//
// Name                 Index   Mask Register SysValue  Format   Used
// -------------------- ----- ------ -------- -------- ------- ------
// POSITION                 0   xyz         0     NONE   float   xyz 
// NORMAL                   0   xyz         1     NONE   float   xyz 
// TANGENT                  0   xyzw        2     NONE   float       
// TEXCOORD                 0   xyzw        3     NONE   float   xyzw
// TEXCOORD                 1   xyzw        4     NONE   float   xyzw
// COLOR                    0   xyzw        5     NONE   float   xyzw
//
//
// Output signature:
//
// Name                 Index   Mask Register SysValue  Format   Used
// -------------------- ----- ------ -------- -------- ------- ------
// SV_POSITION              0   xyzw        0      POS   float   xyzw
// INTERP                   0   xyzw        1     NONE   float   xyzw
// INTERP                   1   xyzw        2     NONE   float   xyzw
// INTERP                   2   xyzw        3     NONE   float   xyzw
// INTERP                   3   xyz         4     NONE   float   xyz 
// INTERP                   4   xyz         5     NONE   float   xyz 
//
      vs_4_0
      dcl_constantbuffer CB0[82], immediateIndexed
      dcl_constantbuffer CB1[7], immediateIndexed
      dcl_input v0.xyz
      dcl_input v1.xyz
      dcl_input v3.xyzw
      dcl_input v4.xyzw
      dcl_input v5.xyzw
      dcl_output_siv o0.xyzw, position
      dcl_output o1.xyzw
      dcl_output o2.xyzw
      dcl_output o3.xyzw
      dcl_output o4.xyz
      dcl_output o5.xyz
      dcl_temps 2
   0: mul r0.xyz, v0.yyyy, cb1[1].xyzx
   1: mad r0.xyz, cb1[0].xyzx, v0.xxxx, r0.xyzx
   2: mad r0.xyz, cb1[2].xyzx, v0.zzzz, r0.xyzx
   3: add r0.xyz, r0.xyzx, cb1[3].xyzx
   4: mul r1.xyzw, r0.yyyy, cb0[79].xyzw
   5: mad r1.xyzw, cb0[78].xyzw, r0.xxxx, r1.xyzw
   6: mad r1.xyzw, cb0[80].xyzw, r0.zzzz, r1.xyzw
   7: mov o4.xyz, r0.xyzx
   8: add o0.xyzw, r1.xyzw, cb0[81].xyzw
   9: mov o1.xyzw, v3.xyzw
  10: mov o2.xyzw, v4.xyzw
  11: mov o3.xyzw, v5.xyzw
  12: dp3 r0.x, v1.xyzx, cb1[4].xyzx
  13: dp3 r0.y, v1.xyzx, cb1[5].xyzx
  14: dp3 r0.z, v1.xyzx, cb1[6].xyzx
  15: dp3 r0.w, r0.xyzx, r0.xyzx
  16: max r0.w, r0.w, l(0.000000)
  17: rsq r0.w, r0.w
  18: mul o5.xyz, r0.wwww, r0.xyzx
  19: ret 
// Approximately 0 instruction slots used


-- Hardware tier variant: Tier 1
-- Fragment shader for "d3d11":
// Stats: 71 math, 4 temp registers
Set 2D Texture "_CameraDepthTexture" to slot 0 sampler slot -1
Set 2D Texture "_MainTex" to slot 1
Set 2D Texture "_Noise" to slot 2
Set 2D Texture "_Flow" to slot 3
Set 2D Texture "_Mask" to slot 4

Set Sampler PointClamp to slot 0

Constant Buffer "$Globals" (2096 bytes) on slot 0 {
  Matrix4x4 unity_MatrixV at 1056
  Matrix4x4 unity_MatrixVP at 1248
  Vector4 _ScaledScreenParams at 48
  Vector2 _GlobalMipBias at 64
  Vector4 _TimeParameters at 304
  Vector3 _WorldSpaceCameraPos at 336
  Vector4 _ProjectionParams at 352
  Vector4 _ZBufferParams at 384
  Vector4 unity_OrthoParams at 400
  Vector4 _RTHandleScale at 448
  Vector4 _CameraDepthTexture_TexelSize at 2080
}
Constant Buffer "UnityPerMaterial" (304 bytes) on slot 1 {
  Float _Fresnelpower at 4
  Float _Fresnelscale at 8
  Float _Useonlycolor at 16
  Float _Use_fresnel at 20
  Vector4 _MainTex_ST at 48
  Vector4 _Noise_ST at 80
  Vector4 _Flow_ST at 112
  Vector4 _Mask_ST at 144
  Vector4 _SpeedMainTexUVNoiseZW at 160
  Vector4 _DistortionSpeedXYPowerZ at 176
  Float _Emission at 192
  Float _Usecenterglow at 228
  Float _Depthpower at 236
}

Shader Disassembly:
//
// Generated by Microsoft (R) D3D Shader Disassembler
//
//
// Input signature:
//
// Name                 Index   Mask Register SysValue  Format   Used
// -------------------- ----- ------ -------- -------- ------- ------
// SV_POSITION              0   xyzw        0      POS   float   xy  
// INTERP                   0   xyzw        1     NONE   float   xyzw
// INTERP                   1   xyzw        2     NONE   float   x   
// INTERP                   2   xyzw        3     NONE   float   xyz 
// INTERP                   3   xyz         4     NONE   float   xyz 
// INTERP                   4   xyz         5     NONE   float   xyz 
// SV_IsFrontFace           0   x           6    FFACE    uint   x   
//
//
// Output signature:
//
// Name                 Index   Mask Register SysValue  Format   Used
// -------------------- ----- ------ -------- -------- ------- ------
// SV_Target                0   xyzw        0   TARGET   float   xyzw
//
      ps_4_0
      dcl_constantbuffer CB0[131], immediateIndexed
      dcl_constantbuffer CB1[15], immediateIndexed
      dcl_sampler s0, mode_default
      dcl_sampler s1, mode_default
      dcl_sampler s2, mode_default
      dcl_sampler s3, mode_default
      dcl_sampler s4, mode_default
      dcl_resource_texture2d (float,float,float,float) t0
      dcl_resource_texture2d (float,float,float,float) t1
      dcl_resource_texture2d (float,float,float,float) t2
      dcl_resource_texture2d (float,float,float,float) t3
      dcl_resource_texture2d (float,float,float,float) t4
      dcl_input_ps_siv linear noperspective v0.xy, position
      dcl_input_ps linear v1.xyzw
      dcl_input_ps linear v2.x
      dcl_input_ps linear v3.xyz
      dcl_input_ps linear v4.xyz
      dcl_input_ps linear v5.xyz
      dcl_input_ps_sgv constant v6.x, is_front_face
      dcl_output o0.xyzw
      dcl_temps 4
   0: dp3 r0.x, v5.xyzx, v5.xyzx
   1: sqrt r0.x, r0.x
   2: div r0.x, l(1.000000, 1.000000, 1.000000, 1.000000), r0.x
   3: mul r0.xyz, r0.xxxx, v5.xyzx
   4: dp3 r0.w, r0.xyzx, r0.xyzx
   5: rsq r0.w, r0.w
   6: mul r0.xyz, r0.wwww, r0.xyzx
   7: add r1.xyz, -v4.xyzx, cb0[21].xyzx
   8: dp3 r0.w, r1.xyzx, r1.xyzx
   9: rsq r0.w, r0.w
  10: mul r1.xyz, r0.wwww, r1.xyzx
  11: eq r0.w, cb0[25].w, l(0.000000)
  12: movc r2.x, r0.w, r1.x, cb0[66].z
  13: movc r2.y, r0.w, r1.y, cb0[67].z
  14: movc r2.z, r0.w, r1.z, cb0[68].z
  15: dp3_sat r0.x, r0.xyzx, r2.xyzx
  16: add r0.x, -r0.x, l(1.000000)
  17: log r0.x, r0.x
  18: mul r0.x, r0.x, cb1[0].y
  19: exp r0.x, r0.x
  20: mul_sat r0.x, r0.x, cb1[0].z
  21: and r0.x, r0.x, v6.x
  22: lt r0.y, cb0[22].x, l(0.000000)
  23: add r0.z, -v0.y, cb0[3].y
  24: movc r1.y, r0.y, r0.z, v0.y
  25: mov r1.x, v0.x
  26: div r1.xy, r1.xyxx, cb0[3].xyxx
  27: add r1.z, -r1.y, l(1.000000)
  28: mad r0.yz, -cb0[130].xxyx, l(0.000000, 0.500000, 0.500000, 0.000000), l(0.000000, 1.000000, 1.000000, 0.000000)
  29: min r0.yz, r0.yyzy, r1.xxzx
  30: mul r0.yz, r0.yyzy, cb0[28].xxyx
  31: sample_b r1.xyzw, r0.yzyy, t0.xyzw, s0, cb0[4].x
  32: mad r0.y, cb0[24].x, r1.x, cb0[24].y
  33: div r0.y, l(1.000000, 1.000000, 1.000000, 1.000000), r0.y
  34: mul r0.z, v4.y, cb0[79].w
  35: mad r0.z, cb0[78].w, v4.x, r0.z
  36: mad r0.z, cb0[80].w, v4.z, r0.z
  37: add r0.z, r0.z, cb0[81].w
  38: mad r0.y, r0.y, cb0[22].z, -r0.z
  39: div_sat r0.y, r0.y, cb1[14].w
  40: ge r0.z, l(0.000000), r0.y
  41: and r0.z, r0.z, l(0x3f800000)
  42: add r0.w, -r0.y, l(1.000000)
  43: mad r0.y, r0.z, r0.w, r0.y
  44: add r0.x, -r0.y, r0.x
  45: add r0.x, r0.x, l(1.000000)
  46: mul r0.y, v1.w, l(1.000100)
  47: ge r0.z, r0.y, -r0.y
  48: frc r0.y, |r0.y|
  49: movc r0.y, r0.z, r0.y, -r0.y
  50: mul r0.y, r0.y, l(0.999900)
  51: div r0.zw, cb1[10].zzzw, cb1[5].xxxy
  52: mad r0.yz, cb0[19].xxxx, r0.zzwz, r0.yyyy
  53: add r0.yz, r0.yyzy, v1.xxyx
  54: mad r0.yz, r0.yyzy, cb1[5].xxyx, cb1[5].zzwz
  55: sample_b r1.xyzw, r0.yzyy, t2.xyzw, s2, cb0[4].x
  56: div r0.yz, cb1[11].xxyx, cb1[7].xxyx
  57: mad r0.yz, r0.yyzy, cb0[19].xxxx, v1.xxyx
  58: mad r0.yz, r0.yyzy, cb1[7].xxyx, cb1[7].zzwz
  59: sample_b r2.xyzw, r0.yzyy, t3.xyzw, s3, cb0[4].x
  60: mad r0.yz, v1.xxyx, cb1[9].xxyx, cb1[9].zzwz
  61: sample_b r3.xyzw, r0.yzyy, t4.xyzw, s4, cb0[4].x
  62: mul r0.yz, r2.xxyx, r3.xxyx
  63: mul r0.yz, r0.yyzy, cb1[11].zzzz
  64: div r0.yz, r0.yyzy, cb1[3].xxyx
  65: div r2.xy, cb1[10].xyxx, cb1[3].xyxx
  66: mad r2.xy, r2.xyxx, cb0[19].xxxx, v1.xyxx
  67: add r0.yz, -r0.yyzy, r2.xxyx
  68: mad r0.yz, r0.yyzy, cb1[3].xxyx, cb1[3].zzwz
  69: sample_b r2.xyzw, r0.yzyy, t1.xyzw, s1, cb0[4].x
  70: mul r1.xyzw, r1.wxyz, r2.wxyz
  71: ne r0.yz, l(0.000000, 0.000000, 0.000000, 0.000000), cb1[1].xxyx
  72: movc r2.xyz, r0.yyyy, r1.xxxx, r1.yzwy
  73: max r0.xyw, r0.xxxx, r2.xyxz
  74: movc r0.xyz, r0.zzzz, r0.xywx, r1.yzwy
  75: add r0.w, -v1.z, l(1.000000)
  76: add_sat r1.xyz, -r0.wwww, r3.xyzx
  77: mul_sat r1.xyz, r1.xyzx, r3.xyzx
  78: mul r1.xyz, r0.xyzx, r1.xyzx
  79: ne r0.w, l(0.000000, 0.000000, 0.000000, 0.000000), cb1[14].y
  80: movc r0.xyz, r0.wwww, r1.xyzx, r0.xyzx
  81: mul r0.xyz, r0.xyzx, cb1[12].xxxx
  82: mul r0.xyz, r0.xyzx, v3.xyzx
  83: max r0.w, v2.x, l(1.000000)
  84: min r0.w, r0.w, l(1000000.000000)
  85: eq r1.x, v1.x, v2.x
  86: movc r0.w, r1.x, l(1.000000), r0.w
  87: mul o0.xyz, r0.xyzx, r0.wwww
  88: mov o0.w, l(1.000000)
  89: ret 
// Approximately 0 instruction slots used


//////////////////////////////////////////////////////
Keywords: FOG_EXP2 LIGHTMAP_BICUBIC_SAMPLING
-- Hardware tier variant: Tier 1
-- Vertex shader for "d3d11":
// Stats: 15 math, 2 temp registers
Uses vertex data channel "Vertex"
Uses vertex data channel "Normal"
Uses vertex data channel "TexCoord0"
Uses vertex data channel "TexCoord1"
Uses vertex data channel "Color"

Constant Buffer "$Globals" (2096 bytes) on slot 0 {
  Matrix4x4 unity_MatrixVP at 1248
}
Constant Buffer "UnityPerDraw" (720 bytes) on slot 1 {
  Matrix4x4 unity_ObjectToWorld at 0
  Matrix4x4 unity_WorldToObject at 64
}

Shader Disassembly:
//
// Generated by Microsoft (R) D3D Shader Disassembler
//
//
// Input signature:
//
// Name                 Index   Mask Register SysValue  Format   Used
// -------------------- ----- ------ -------- -------- ------- ------
// POSITION                 0   xyz         0     NONE   float   xyz 
// NORMAL                   0   xyz         1     NONE   float   xyz 
// TANGENT                  0   xyzw        2     NONE   float       
// TEXCOORD                 0   xyzw        3     NONE   float   xyzw
// TEXCOORD                 1   xyzw        4     NONE   float   xyzw
// COLOR                    0   xyzw        5     NONE   float   xyzw
//
//
// Output signature:
//
// Name                 Index   Mask Register SysValue  Format   Used
// -------------------- ----- ------ -------- -------- ------- ------
// SV_POSITION              0   xyzw        0      POS   float   xyzw
// INTERP                   0   xyzw        1     NONE   float   xyzw
// INTERP                   1   xyzw        2     NONE   float   xyzw
// INTERP                   2   xyzw        3     NONE   float   xyzw
// INTERP                   3   xyz         4     NONE   float   xyz 
// INTERP                   4   xyz         5     NONE   float   xyz 
//
      vs_4_0
      dcl_constantbuffer CB0[82], immediateIndexed
      dcl_constantbuffer CB1[7], immediateIndexed
      dcl_input v0.xyz
      dcl_input v1.xyz
      dcl_input v3.xyzw
      dcl_input v4.xyzw
      dcl_input v5.xyzw
      dcl_output_siv o0.xyzw, position
      dcl_output o1.xyzw
      dcl_output o2.xyzw
      dcl_output o3.xyzw
      dcl_output o4.xyz
      dcl_output o5.xyz
      dcl_temps 2
   0: mul r0.xyz, v0.yyyy, cb1[1].xyzx
   1: mad r0.xyz, cb1[0].xyzx, v0.xxxx, r0.xyzx
   2: mad r0.xyz, cb1[2].xyzx, v0.zzzz, r0.xyzx
   3: add r0.xyz, r0.xyzx, cb1[3].xyzx
   4: mul r1.xyzw, r0.yyyy, cb0[79].xyzw
   5: mad r1.xyzw, cb0[78].xyzw, r0.xxxx, r1.xyzw
   6: mad r1.xyzw, cb0[80].xyzw, r0.zzzz, r1.xyzw
   7: mov o4.xyz, r0.xyzx
   8: add o0.xyzw, r1.xyzw, cb0[81].xyzw
   9: mov o1.xyzw, v3.xyzw
  10: mov o2.xyzw, v4.xyzw
  11: mov o3.xyzw, v5.xyzw
  12: dp3 r0.x, v1.xyzx, cb1[4].xyzx
  13: dp3 r0.y, v1.xyzx, cb1[5].xyzx
  14: dp3 r0.z, v1.xyzx, cb1[6].xyzx
  15: dp3 r0.w, r0.xyzx, r0.xyzx
  16: max r0.w, r0.w, l(0.000000)
  17: rsq r0.w, r0.w
  18: mul o5.xyz, r0.wwww, r0.xyzx
  19: ret 
// Approximately 0 instruction slots used


-- Fragment shader for "d3d11":
// No shader variant for this keyword set. The closest match will be used instead.

//////////////////////////////////////////////////////
Keywords: LIGHTMAP_BICUBIC_SAMPLING _SURFACE_TYPE_TRANSPARENT
-- Vertex shader for "d3d11":
// No shader variant for this keyword set. The closest match will be used instead.

-- Hardware tier variant: Tier 1
-- Fragment shader for "d3d11":
// Stats: 76 math, 4 temp registers
Set 2D Texture "_CameraDepthTexture" to slot 0 sampler slot -1
Set 2D Texture "_MainTex" to slot 1
Set 2D Texture "_Noise" to slot 2
Set 2D Texture "_Flow" to slot 3
Set 2D Texture "_Mask" to slot 4

Set Sampler PointClamp to slot 0

Constant Buffer "$Globals" (2096 bytes) on slot 0 {
  Matrix4x4 unity_MatrixV at 1056
  Matrix4x4 unity_MatrixVP at 1248
  Vector4 _ScaledScreenParams at 48
  Vector2 _GlobalMipBias at 64
  Vector4 _TimeParameters at 304
  Vector3 _WorldSpaceCameraPos at 336
  Vector4 _ProjectionParams at 352
  Vector4 _ZBufferParams at 384
  Vector4 unity_OrthoParams at 400
  Vector4 _RTHandleScale at 448
  Vector4 _CameraDepthTexture_TexelSize at 2080
}
Constant Buffer "UnityPerMaterial" (304 bytes) on slot 1 {
  Float _Textureopacity at 0
  Float _Fresnelpower at 4
  Float _Fresnelscale at 8
  Float _Multiply_texture at 12
  Float _Useonlycolor at 16
  Float _Use_fresnel at 20
  Vector4 _MainTex_ST at 48
  Vector4 _Noise_ST at 80
  Vector4 _Flow_ST at 112
  Vector4 _Mask_ST at 144
  Vector4 _SpeedMainTexUVNoiseZW at 160
  Vector4 _DistortionSpeedXYPowerZ at 176
  Float _Emission at 192
  Float _Opacity at 224
  Float _Usecenterglow at 228
  Float _Usedepth at 232
  Float _Depthpower at 236
}

Shader Disassembly:
//
// Generated by Microsoft (R) D3D Shader Disassembler
//
//
// Input signature:
//
// Name                 Index   Mask Register SysValue  Format   Used
// -------------------- ----- ------ -------- -------- ------- ------
// SV_POSITION              0   xyzw        0      POS   float   xy  
// INTERP                   0   xyzw        1     NONE   float   xyzw
// INTERP                   1   xyzw        2     NONE   float   x   
// INTERP                   2   xyzw        3     NONE   float   xyzw
// INTERP                   3   xyz         4     NONE   float   xyz 
// INTERP                   4   xyz         5     NONE   float   xyz 
// SV_IsFrontFace           0   x           6    FFACE    uint   x   
//
//
// Output signature:
//
// Name                 Index   Mask Register SysValue  Format   Used
// -------------------- ----- ------ -------- -------- ------- ------
// SV_Target                0   xyzw        0   TARGET   float   xyzw
//
      ps_4_0
      dcl_constantbuffer CB0[131], immediateIndexed
      dcl_constantbuffer CB1[15], immediateIndexed
      dcl_sampler s0, mode_default
      dcl_sampler s1, mode_default
      dcl_sampler s2, mode_default
      dcl_sampler s3, mode_default
      dcl_sampler s4, mode_default
      dcl_resource_texture2d (float,float,float,float) t0
      dcl_resource_texture2d (float,float,float,float) t1
      dcl_resource_texture2d (float,float,float,float) t2
      dcl_resource_texture2d (float,float,float,float) t3
      dcl_resource_texture2d (float,float,float,float) t4
      dcl_input_ps_siv linear noperspective v0.xy, position
      dcl_input_ps linear v1.xyzw
      dcl_input_ps linear v2.x
      dcl_input_ps linear v3.xyzw
      dcl_input_ps linear v4.xyz
      dcl_input_ps linear v5.xyz
      dcl_input_ps_sgv constant v6.x, is_front_face
      dcl_output o0.xyzw
      dcl_temps 4
   0: dp3 r0.x, v5.xyzx, v5.xyzx
   1: sqrt r0.x, r0.x
   2: div r0.x, l(1.000000, 1.000000, 1.000000, 1.000000), r0.x
   3: mul r0.xyz, r0.xxxx, v5.xyzx
   4: dp3 r0.w, r0.xyzx, r0.xyzx
   5: rsq r0.w, r0.w
   6: mul r0.xyz, r0.wwww, r0.xyzx
   7: add r1.xyz, -v4.xyzx, cb0[21].xyzx
   8: dp3 r0.w, r1.xyzx, r1.xyzx
   9: rsq r0.w, r0.w
  10: mul r1.xyz, r0.wwww, r1.xyzx
  11: eq r0.w, cb0[25].w, l(0.000000)
  12: movc r2.x, r0.w, r1.x, cb0[66].z
  13: movc r2.y, r0.w, r1.y, cb0[67].z
  14: movc r2.z, r0.w, r1.z, cb0[68].z
  15: dp3_sat r0.x, r0.xyzx, r2.xyzx
  16: add r0.x, -r0.x, l(1.000000)
  17: log r0.x, r0.x
  18: mul r0.x, r0.x, cb1[0].y
  19: exp r0.x, r0.x
  20: mul_sat r0.x, r0.x, cb1[0].z
  21: and r0.x, r0.x, v6.x
  22: lt r0.y, cb0[22].x, l(0.000000)
  23: add r0.z, -v0.y, cb0[3].y
  24: movc r1.y, r0.y, r0.z, v0.y
  25: mov r1.x, v0.x
  26: div r1.xy, r1.xyxx, cb0[3].xyxx
  27: add r1.z, -r1.y, l(1.000000)
  28: mad r0.yz, -cb0[130].xxyx, l(0.000000, 0.500000, 0.500000, 0.000000), l(0.000000, 1.000000, 1.000000, 0.000000)
  29: min r0.yz, r0.yyzy, r1.xxzx
  30: mul r0.yz, r0.yyzy, cb0[28].xxyx
  31: sample_b r1.xyzw, r0.yzyy, t0.xyzw, s0, cb0[4].x
  32: mad r0.y, cb0[24].x, r1.x, cb0[24].y
  33: div r0.y, l(1.000000, 1.000000, 1.000000, 1.000000), r0.y
  34: mul r0.z, v4.y, cb0[79].w
  35: mad r0.z, cb0[78].w, v4.x, r0.z
  36: mad r0.z, cb0[80].w, v4.z, r0.z
  37: add r0.z, r0.z, cb0[81].w
  38: mad r0.y, r0.y, cb0[22].z, -r0.z
  39: div_sat r0.y, r0.y, cb1[14].w
  40: ge r0.z, l(0.000000), r0.y
  41: and r0.z, r0.z, l(0x3f800000)
  42: add r0.w, -r0.y, l(1.000000)
  43: mad r0.y, r0.z, r0.w, r0.y
  44: add r0.x, -r0.y, r0.x
  45: add r0.x, r0.x, l(1.000000)
  46: mul r0.z, v1.w, l(1.000100)
  47: ge r0.w, r0.z, -r0.z
  48: frc r0.z, |r0.z|
  49: movc r0.z, r0.w, r0.z, -r0.z
  50: mul r0.z, r0.z, l(0.999900)
  51: div r1.xy, cb1[10].zwzz, cb1[5].xyxx
  52: mad r0.zw, cb0[19].xxxx, r1.xxxy, r0.zzzz
  53: add r0.zw, r0.zzzw, v1.xxxy
  54: mad r0.zw, r0.zzzw, cb1[5].xxxy, cb1[5].zzzw
  55: sample_b r1.xyzw, r0.zwzz, t2.xyzw, s2, cb0[4].x
  56: div r0.zw, cb1[11].xxxy, cb1[7].xxxy
  57: mad r0.zw, r0.zzzw, cb0[19].xxxx, v1.xxxy
  58: mad r0.zw, r0.zzzw, cb1[7].xxxy, cb1[7].zzzw
  59: sample_b r2.xyzw, r0.zwzz, t3.xyzw, s3, cb0[4].x
  60: mad r0.zw, v1.xxxy, cb1[9].xxxy, cb1[9].zzzw
  61: sample_b r3.xyzw, r0.zwzz, t4.xyzw, s4, cb0[4].x
  62: mul r0.zw, r2.xxxy, r3.xxxy
  63: mul r0.zw, r0.zzzw, cb1[11].zzzz
  64: div r0.zw, r0.zzzw, cb1[3].xxxy
  65: div r2.xy, cb1[10].xyxx, cb1[3].xyxx
  66: mad r2.xy, r2.xyxx, cb0[19].xxxx, v1.xyxx
  67: add r0.zw, -r0.zzzw, r2.xxxy
  68: mad r0.zw, r0.zzzw, cb1[3].xxxy, cb1[3].zzzw
  69: sample_b r2.xyzw, r0.zwzz, t1.xyzw, s1, cb0[4].x
  70: mul r1.xyzw, r1.wxyz, r2.wxyz
  71: ne r0.zw, l(0.000000, 0.000000, 0.000000, 0.000000), cb1[1].xxxy
  72: movc r2.xyz, r0.zzzz, r1.xxxx, r1.yzwy
  73: max r2.xyz, r0.xxxx, r2.xyzx
  74: mad_sat r0.x, r1.x, cb1[0].x, r0.x
  75: movc r1.yzw, r0.wwww, r2.xxyz, r1.yyzw
  76: add r0.z, -v1.z, l(1.000000)
  77: add_sat r2.xyz, -r0.zzzz, r3.xyzx
  78: mul_sat r2.xyz, r2.xyzx, r3.xyzx
  79: mul r2.xyz, r1.yzwy, r2.xyzx
  80: ne r3.xy, l(0.000000, 0.000000, 0.000000, 0.000000), cb1[14].yzyy
  81: movc r1.yzw, r3.xxxx, r2.xxyz, r1.yyzw
  82: mul r1.yzw, r1.yyzw, cb1[12].xxxx
  83: mul r1.yzw, r1.yyzw, v3.xxyz
  84: max r0.z, v2.x, l(1.000000)
  85: min r0.z, r0.z, l(1000000.000000)
  86: eq r2.x, v1.x, v2.x
  87: movc r0.z, r2.x, l(1.000000), r0.z
  88: mul o0.xyz, r1.yzwy, r0.zzzz
  89: mul r0.z, r0.x, r1.x
  90: ne r1.y, l(0.000000, 0.000000, 0.000000, 0.000000), cb1[0].w
  91: movc r0.x, r1.y, r0.z, r0.x
  92: movc r0.x, r0.w, r0.x, r1.x
  93: mul r0.y, r0.y, r0.x
  94: movc r0.x, r3.y, r0.y, r0.x
  95: mul r0.x, r0.x, v3.w
  96: mul_sat o0.w, r0.x, cb1[14].x
  97: ret 
// Approximately 0 instruction slots used


//////////////////////////////////////////////////////
Keywords: FOG_EXP2 LIGHTMAP_BICUBIC_SAMPLING
-- Vertex shader for "d3d11":
// No shader variant for this keyword set. The closest match will be used instead.

-- Hardware tier variant: Tier 1
-- Fragment shader for "d3d11":
// Stats: 71 math, 4 temp registers
Set 2D Texture "_CameraDepthTexture" to slot 0 sampler slot -1
Set 2D Texture "_MainTex" to slot 1
Set 2D Texture "_Noise" to slot 2
Set 2D Texture "_Flow" to slot 3
Set 2D Texture "_Mask" to slot 4

Set Sampler PointClamp to slot 0

Constant Buffer "$Globals" (2096 bytes) on slot 0 {
  Matrix4x4 unity_MatrixV at 1056
  Matrix4x4 unity_MatrixVP at 1248
  Vector4 _ScaledScreenParams at 48
  Vector2 _GlobalMipBias at 64
  Vector4 _TimeParameters at 304
  Vector3 _WorldSpaceCameraPos at 336
  Vector4 _ProjectionParams at 352
  Vector4 _ZBufferParams at 384
  Vector4 unity_OrthoParams at 400
  Vector4 _RTHandleScale at 448
  Vector4 _CameraDepthTexture_TexelSize at 2080
}
Constant Buffer "UnityPerMaterial" (304 bytes) on slot 1 {
  Float _Fresnelpower at 4
  Float _Fresnelscale at 8
  Float _Useonlycolor at 16
  Float _Use_fresnel at 20
  Vector4 _MainTex_ST at 48
  Vector4 _Noise_ST at 80
  Vector4 _Flow_ST at 112
  Vector4 _Mask_ST at 144
  Vector4 _SpeedMainTexUVNoiseZW at 160
  Vector4 _DistortionSpeedXYPowerZ at 176
  Float _Emission at 192
  Float _Usecenterglow at 228
  Float _Depthpower at 236
}

Shader Disassembly:
//
// Generated by Microsoft (R) D3D Shader Disassembler
//
//
// Input signature:
//
// Name                 Index   Mask Register SysValue  Format   Used
// -------------------- ----- ------ -------- -------- ------- ------
// SV_POSITION              0   xyzw        0      POS   float   xy  
// INTERP                   0   xyzw        1     NONE   float   xyzw
// INTERP                   1   xyzw        2     NONE   float   x   
// INTERP                   2   xyzw        3     NONE   float   xyz 
// INTERP                   3   xyz         4     NONE   float   xyz 
// INTERP                   4   xyz         5     NONE   float   xyz 
// SV_IsFrontFace           0   x           6    FFACE    uint   x   
//
//
// Output signature:
//
// Name                 Index   Mask Register SysValue  Format   Used
// -------------------- ----- ------ -------- -------- ------- ------
// SV_Target                0   xyzw        0   TARGET   float   xyzw
//
      ps_4_0
      dcl_constantbuffer CB0[131], immediateIndexed
      dcl_constantbuffer CB1[15], immediateIndexed
      dcl_sampler s0, mode_default
      dcl_sampler s1, mode_default
      dcl_sampler s2, mode_default
      dcl_sampler s3, mode_default
      dcl_sampler s4, mode_default
      dcl_resource_texture2d (float,float,float,float) t0
      dcl_resource_texture2d (float,float,float,float) t1
      dcl_resource_texture2d (float,float,float,float) t2
      dcl_resource_texture2d (float,float,float,float) t3
      dcl_resource_texture2d (float,float,float,float) t4
      dcl_input_ps_siv linear noperspective v0.xy, position
      dcl_input_ps linear v1.xyzw
      dcl_input_ps linear v2.x
      dcl_input_ps linear v3.xyz
      dcl_input_ps linear v4.xyz
      dcl_input_ps linear v5.xyz
      dcl_input_ps_sgv constant v6.x, is_front_face
      dcl_output o0.xyzw
      dcl_temps 4
   0: dp3 r0.x, v5.xyzx, v5.xyzx
   1: sqrt r0.x, r0.x
   2: div r0.x, l(1.000000, 1.000000, 1.000000, 1.000000), r0.x
   3: mul r0.xyz, r0.xxxx, v5.xyzx
   4: dp3 r0.w, r0.xyzx, r0.xyzx
   5: rsq r0.w, r0.w
   6: mul r0.xyz, r0.wwww, r0.xyzx
   7: add r1.xyz, -v4.xyzx, cb0[21].xyzx
   8: dp3 r0.w, r1.xyzx, r1.xyzx
   9: rsq r0.w, r0.w
  10: mul r1.xyz, r0.wwww, r1.xyzx
  11: eq r0.w, cb0[25].w, l(0.000000)
  12: movc r2.x, r0.w, r1.x, cb0[66].z
  13: movc r2.y, r0.w, r1.y, cb0[67].z
  14: movc r2.z, r0.w, r1.z, cb0[68].z
  15: dp3_sat r0.x, r0.xyzx, r2.xyzx
  16: add r0.x, -r0.x, l(1.000000)
  17: log r0.x, r0.x
  18: mul r0.x, r0.x, cb1[0].y
  19: exp r0.x, r0.x
  20: mul_sat r0.x, r0.x, cb1[0].z
  21: and r0.x, r0.x, v6.x
  22: lt r0.y, cb0[22].x, l(0.000000)
  23: add r0.z, -v0.y, cb0[3].y
  24: movc r1.y, r0.y, r0.z, v0.y
  25: mov r1.x, v0.x
  26: div r1.xy, r1.xyxx, cb0[3].xyxx
  27: add r1.z, -r1.y, l(1.000000)
  28: mad r0.yz, -cb0[130].xxyx, l(0.000000, 0.500000, 0.500000, 0.000000), l(0.000000, 1.000000, 1.000000, 0.000000)
  29: min r0.yz, r0.yyzy, r1.xxzx
  30: mul r0.yz, r0.yyzy, cb0[28].xxyx
  31: sample_b r1.xyzw, r0.yzyy, t0.xyzw, s0, cb0[4].x
  32: mad r0.y, cb0[24].x, r1.x, cb0[24].y
  33: div r0.y, l(1.000000, 1.000000, 1.000000, 1.000000), r0.y
  34: mul r0.z, v4.y, cb0[79].w
  35: mad r0.z, cb0[78].w, v4.x, r0.z
  36: mad r0.z, cb0[80].w, v4.z, r0.z
  37: add r0.z, r0.z, cb0[81].w
  38: mad r0.y, r0.y, cb0[22].z, -r0.z
  39: div_sat r0.y, r0.y, cb1[14].w
  40: ge r0.z, l(0.000000), r0.y
  41: and r0.z, r0.z, l(0x3f800000)
  42: add r0.w, -r0.y, l(1.000000)
  43: mad r0.y, r0.z, r0.w, r0.y
  44: add r0.x, -r0.y, r0.x
  45: add r0.x, r0.x, l(1.000000)
  46: mul r0.y, v1.w, l(1.000100)
  47: ge r0.z, r0.y, -r0.y
  48: frc r0.y, |r0.y|
  49: movc r0.y, r0.z, r0.y, -r0.y
  50: mul r0.y, r0.y, l(0.999900)
  51: div r0.zw, cb1[10].zzzw, cb1[5].xxxy
  52: mad r0.yz, cb0[19].xxxx, r0.zzwz, r0.yyyy
  53: add r0.yz, r0.yyzy, v1.xxyx
  54: mad r0.yz, r0.yyzy, cb1[5].xxyx, cb1[5].zzwz
  55: sample_b r1.xyzw, r0.yzyy, t2.xyzw, s2, cb0[4].x
  56: div r0.yz, cb1[11].xxyx, cb1[7].xxyx
  57: mad r0.yz, r0.yyzy, cb0[19].xxxx, v1.xxyx
  58: mad r0.yz, r0.yyzy, cb1[7].xxyx, cb1[7].zzwz
  59: sample_b r2.xyzw, r0.yzyy, t3.xyzw, s3, cb0[4].x
  60: mad r0.yz, v1.xxyx, cb1[9].xxyx, cb1[9].zzwz
  61: sample_b r3.xyzw, r0.yzyy, t4.xyzw, s4, cb0[4].x
  62: mul r0.yz, r2.xxyx, r3.xxyx
  63: mul r0.yz, r0.yyzy, cb1[11].zzzz
  64: div r0.yz, r0.yyzy, cb1[3].xxyx
  65: div r2.xy, cb1[10].xyxx, cb1[3].xyxx
  66: mad r2.xy, r2.xyxx, cb0[19].xxxx, v1.xyxx
  67: add r0.yz, -r0.yyzy, r2.xxyx
  68: mad r0.yz, r0.yyzy, cb1[3].xxyx, cb1[3].zzwz
  69: sample_b r2.xyzw, r0.yzyy, t1.xyzw, s1, cb0[4].x
  70: mul r1.xyzw, r1.wxyz, r2.wxyz
  71: ne r0.yz, l(0.000000, 0.000000, 0.000000, 0.000000), cb1[1].xxyx
  72: movc r2.xyz, r0.yyyy, r1.xxxx, r1.yzwy
  73: max r0.xyw, r0.xxxx, r2.xyxz
  74: movc r0.xyz, r0.zzzz, r0.xywx, r1.yzwy
  75: add r0.w, -v1.z, l(1.000000)
  76: add_sat r1.xyz, -r0.wwww, r3.xyzx
  77: mul_sat r1.xyz, r1.xyzx, r3.xyzx
  78: mul r1.xyz, r0.xyzx, r1.xyzx
  79: ne r0.w, l(0.000000, 0.000000, 0.000000, 0.000000), cb1[14].y
  80: movc r0.xyz, r0.wwww, r1.xyzx, r0.xyzx
  81: mul r0.xyz, r0.xyzx, cb1[12].xxxx
  82: mul r0.xyz, r0.xyzx, v3.xyzx
  83: max r0.w, v2.x, l(1.000000)
  84: min r0.w, r0.w, l(1000000.000000)
  85: eq r1.x, v1.x, v2.x
  86: movc r0.w, r1.x, l(1.000000), r0.w
  87: mul o0.xyz, r0.xyzx, r0.wwww
  88: mov o0.w, l(1.000000)
  89: ret 
// Approximately 0 instruction slots used


//////////////////////////////////////////////////////
Keywords: FOG_EXP2 LIGHTMAP_BICUBIC_SAMPLING _SURFACE_TYPE_TRANSPARENT
-- Vertex shader for "d3d11":
// No shader variant for this keyword set. The closest match will be used instead.

-- Hardware tier variant: Tier 1
-- Fragment shader for "d3d11":
// Stats: 76 math, 4 temp registers
Set 2D Texture "_CameraDepthTexture" to slot 0 sampler slot -1
Set 2D Texture "_MainTex" to slot 1
Set 2D Texture "_Noise" to slot 2
Set 2D Texture "_Flow" to slot 3
Set 2D Texture "_Mask" to slot 4

Set Sampler PointClamp to slot 0

Constant Buffer "$Globals" (2096 bytes) on slot 0 {
  Matrix4x4 unity_MatrixV at 1056
  Matrix4x4 unity_MatrixVP at 1248
  Vector4 _ScaledScreenParams at 48
  Vector2 _GlobalMipBias at 64
  Vector4 _TimeParameters at 304
  Vector3 _WorldSpaceCameraPos at 336
  Vector4 _ProjectionParams at 352
  Vector4 _ZBufferParams at 384
  Vector4 unity_OrthoParams at 400
  Vector4 _RTHandleScale at 448
  Vector4 _CameraDepthTexture_TexelSize at 2080
}
Constant Buffer "UnityPerMaterial" (304 bytes) on slot 1 {
  Float _Textureopacity at 0
  Float _Fresnelpower at 4
  Float _Fresnelscale at 8
  Float _Multiply_texture at 12
  Float _Useonlycolor at 16
  Float _Use_fresnel at 20
  Vector4 _MainTex_ST at 48
  Vector4 _Noise_ST at 80
  Vector4 _Flow_ST at 112
  Vector4 _Mask_ST at 144
  Vector4 _SpeedMainTexUVNoiseZW at 160
  Vector4 _DistortionSpeedXYPowerZ at 176
  Float _Emission at 192
  Float _Opacity at 224
  Float _Usecenterglow at 228
  Float _Usedepth at 232
  Float _Depthpower at 236
}

Shader Disassembly:
//
// Generated by Microsoft (R) D3D Shader Disassembler
//
//
// Input signature:
//
// Name                 Index   Mask Register SysValue  Format   Used
// -------------------- ----- ------ -------- -------- ------- ------
// SV_POSITION              0   xyzw        0      POS   float   xy  
// INTERP                   0   xyzw        1     NONE   float   xyzw
// INTERP                   1   xyzw        2     NONE   float   x   
// INTERP                   2   xyzw        3     NONE   float   xyzw
// INTERP                   3   xyz         4     NONE   float   xyz 
// INTERP                   4   xyz         5     NONE   float   xyz 
// SV_IsFrontFace           0   x           6    FFACE    uint   x   
//
//
// Output signature:
//
// Name                 Index   Mask Register SysValue  Format   Used
// -------------------- ----- ------ -------- -------- ------- ------
// SV_Target                0   xyzw        0   TARGET   float   xyzw
//
      ps_4_0
      dcl_constantbuffer CB0[131], immediateIndexed
      dcl_constantbuffer CB1[15], immediateIndexed
      dcl_sampler s0, mode_default
      dcl_sampler s1, mode_default
      dcl_sampler s2, mode_default
      dcl_sampler s3, mode_default
      dcl_sampler s4, mode_default
      dcl_resource_texture2d (float,float,float,float) t0
      dcl_resource_texture2d (float,float,float,float) t1
      dcl_resource_texture2d (float,float,float,float) t2
      dcl_resource_texture2d (float,float,float,float) t3
      dcl_resource_texture2d (float,float,float,float) t4
      dcl_input_ps_siv linear noperspective v0.xy, position
      dcl_input_ps linear v1.xyzw
      dcl_input_ps linear v2.x
      dcl_input_ps linear v3.xyzw
      dcl_input_ps linear v4.xyz
      dcl_input_ps linear v5.xyz
      dcl_input_ps_sgv constant v6.x, is_front_face
      dcl_output o0.xyzw
      dcl_temps 4
   0: dp3 r0.x, v5.xyzx, v5.xyzx
   1: sqrt r0.x, r0.x
   2: div r0.x, l(1.000000, 1.000000, 1.000000, 1.000000), r0.x
   3: mul r0.xyz, r0.xxxx, v5.xyzx
   4: dp3 r0.w, r0.xyzx, r0.xyzx
   5: rsq r0.w, r0.w
   6: mul r0.xyz, r0.wwww, r0.xyzx
   7: add r1.xyz, -v4.xyzx, cb0[21].xyzx
   8: dp3 r0.w, r1.xyzx, r1.xyzx
   9: rsq r0.w, r0.w
  10: mul r1.xyz, r0.wwww, r1.xyzx
  11: eq r0.w, cb0[25].w, l(0.000000)
  12: movc r2.x, r0.w, r1.x, cb0[66].z
  13: movc r2.y, r0.w, r1.y, cb0[67].z
  14: movc r2.z, r0.w, r1.z, cb0[68].z
  15: dp3_sat r0.x, r0.xyzx, r2.xyzx
  16: add r0.x, -r0.x, l(1.000000)
  17: log r0.x, r0.x
  18: mul r0.x, r0.x, cb1[0].y
  19: exp r0.x, r0.x
  20: mul_sat r0.x, r0.x, cb1[0].z
  21: and r0.x, r0.x, v6.x
  22: lt r0.y, cb0[22].x, l(0.000000)
  23: add r0.z, -v0.y, cb0[3].y
  24: movc r1.y, r0.y, r0.z, v0.y
  25: mov r1.x, v0.x
  26: div r1.xy, r1.xyxx, cb0[3].xyxx
  27: add r1.z, -r1.y, l(1.000000)
  28: mad r0.yz, -cb0[130].xxyx, l(0.000000, 0.500000, 0.500000, 0.000000), l(0.000000, 1.000000, 1.000000, 0.000000)
  29: min r0.yz, r0.yyzy, r1.xxzx
  30: mul r0.yz, r0.yyzy, cb0[28].xxyx
  31: sample_b r1.xyzw, r0.yzyy, t0.xyzw, s0, cb0[4].x
  32: mad r0.y, cb0[24].x, r1.x, cb0[24].y
  33: div r0.y, l(1.000000, 1.000000, 1.000000, 1.000000), r0.y
  34: mul r0.z, v4.y, cb0[79].w
  35: mad r0.z, cb0[78].w, v4.x, r0.z
  36: mad r0.z, cb0[80].w, v4.z, r0.z
  37: add r0.z, r0.z, cb0[81].w
  38: mad r0.y, r0.y, cb0[22].z, -r0.z
  39: div_sat r0.y, r0.y, cb1[14].w
  40: ge r0.z, l(0.000000), r0.y
  41: and r0.z, r0.z, l(0x3f800000)
  42: add r0.w, -r0.y, l(1.000000)
  43: mad r0.y, r0.z, r0.w, r0.y
  44: add r0.x, -r0.y, r0.x
  45: add r0.x, r0.x, l(1.000000)
  46: mul r0.z, v1.w, l(1.000100)
  47: ge r0.w, r0.z, -r0.z
  48: frc r0.z, |r0.z|
  49: movc r0.z, r0.w, r0.z, -r0.z
  50: mul r0.z, r0.z, l(0.999900)
  51: div r1.xy, cb1[10].zwzz, cb1[5].xyxx
  52: mad r0.zw, cb0[19].xxxx, r1.xxxy, r0.zzzz
  53: add r0.zw, r0.zzzw, v1.xxxy
  54: mad r0.zw, r0.zzzw, cb1[5].xxxy, cb1[5].zzzw
  55: sample_b r1.xyzw, r0.zwzz, t2.xyzw, s2, cb0[4].x
  56: div r0.zw, cb1[11].xxxy, cb1[7].xxxy
  57: mad r0.zw, r0.zzzw, cb0[19].xxxx, v1.xxxy
  58: mad r0.zw, r0.zzzw, cb1[7].xxxy, cb1[7].zzzw
  59: sample_b r2.xyzw, r0.zwzz, t3.xyzw, s3, cb0[4].x
  60: mad r0.zw, v1.xxxy, cb1[9].xxxy, cb1[9].zzzw
  61: sample_b r3.xyzw, r0.zwzz, t4.xyzw, s4, cb0[4].x
  62: mul r0.zw, r2.xxxy, r3.xxxy
  63: mul r0.zw, r0.zzzw, cb1[11].zzzz
  64: div r0.zw, r0.zzzw, cb1[3].xxxy
  65: div r2.xy, cb1[10].xyxx, cb1[3].xyxx
  66: mad r2.xy, r2.xyxx, cb0[19].xxxx, v1.xyxx
  67: add r0.zw, -r0.zzzw, r2.xxxy
  68: mad r0.zw, r0.zzzw, cb1[3].xxxy, cb1[3].zzzw
  69: sample_b r2.xyzw, r0.zwzz, t1.xyzw, s1, cb0[4].x
  70: mul r1.xyzw, r1.wxyz, r2.wxyz
  71: ne r0.zw, l(0.000000, 0.000000, 0.000000, 0.000000), cb1[1].xxxy
  72: movc r2.xyz, r0.zzzz, r1.xxxx, r1.yzwy
  73: max r2.xyz, r0.xxxx, r2.xyzx
  74: mad_sat r0.x, r1.x, cb1[0].x, r0.x
  75: movc r1.yzw, r0.wwww, r2.xxyz, r1.yyzw
  76: add r0.z, -v1.z, l(1.000000)
  77: add_sat r2.xyz, -r0.zzzz, r3.xyzx
  78: mul_sat r2.xyz, r2.xyzx, r3.xyzx
  79: mul r2.xyz, r1.yzwy, r2.xyzx
  80: ne r3.xy, l(0.000000, 0.000000, 0.000000, 0.000000), cb1[14].yzyy
  81: movc r1.yzw, r3.xxxx, r2.xxyz, r1.yyzw
  82: mul r1.yzw, r1.yyzw, cb1[12].xxxx
  83: mul r1.yzw, r1.yyzw, v3.xxyz
  84: max r0.z, v2.x, l(1.000000)
  85: min r0.z, r0.z, l(1000000.000000)
  86: eq r2.x, v1.x, v2.x
  87: movc r0.z, r2.x, l(1.000000), r0.z
  88: mul o0.xyz, r1.yzwy, r0.zzzz
  89: mul r0.z, r0.x, r1.x
  90: ne r1.y, l(0.000000, 0.000000, 0.000000, 0.000000), cb1[0].w
  91: movc r0.x, r1.y, r0.z, r0.x
  92: movc r0.x, r0.w, r0.x, r1.x
  93: mul r0.y, r0.y, r0.x
  94: movc r0.x, r3.y, r0.y, r0.x
  95: mul r0.x, r0.x, v3.w
  96: mul_sat o0.w, r0.x, cb1[14].x
  97: ret 
// Approximately 0 instruction slots used


//////////////////////////////////////////////////////
Keywords: LIGHTMAP_BICUBIC_SAMPLING _WRITE_RENDERING_LAYERS
-- Vertex shader for "d3d11":
// No shader variant for this keyword set. The closest match will be used instead.

-- Hardware tier variant: Tier 1
-- Fragment shader for "d3d11":
// Stats: 72 math, 4 temp registers
Set 2D Texture "_CameraDepthTexture" to slot 0 sampler slot -1
Set 2D Texture "_MainTex" to slot 1
Set 2D Texture "_Noise" to slot 2
Set 2D Texture "_Flow" to slot 3
Set 2D Texture "_Mask" to slot 4

Set Sampler PointClamp to slot 0

Constant Buffer "$Globals" (2096 bytes) on slot 0 {
  Matrix4x4 unity_MatrixV at 1056
  Matrix4x4 unity_MatrixVP at 1248
  Vector4 _ScaledScreenParams at 48
  Vector2 _GlobalMipBias at 64
  ScalarInt _RenderingLayerMaxInt at 176
  Vector4 _TimeParameters at 304
  Vector3 _WorldSpaceCameraPos at 336
  Vector4 _ProjectionParams at 352
  Vector4 _ZBufferParams at 384
  Vector4 unity_OrthoParams at 400
  Vector4 _RTHandleScale at 448
  Vector4 _CameraDepthTexture_TexelSize at 2080
}
Constant Buffer "UnityPerDraw" (720 bytes) on slot 1 {
  Vector4 unity_RenderingLayer at 160
}
Constant Buffer "UnityPerMaterial" (304 bytes) on slot 2 {
  Float _Fresnelpower at 4
  Float _Fresnelscale at 8
  Float _Useonlycolor at 16
  Float _Use_fresnel at 20
  Vector4 _MainTex_ST at 48
  Vector4 _Noise_ST at 80
  Vector4 _Flow_ST at 112
  Vector4 _Mask_ST at 144
  Vector4 _SpeedMainTexUVNoiseZW at 160
  Vector4 _DistortionSpeedXYPowerZ at 176
  Float _Emission at 192
  Float _Usecenterglow at 228
  Float _Depthpower at 236
}

Shader Disassembly:
//
// Generated by Microsoft (R) D3D Shader Disassembler
//
//
// Input signature:
//
// Name                 Index   Mask Register SysValue  Format   Used
// -------------------- ----- ------ -------- -------- ------- ------
// SV_POSITION              0   xyzw        0      POS   float   xy  
// INTERP                   0   xyzw        1     NONE   float   xyzw
// INTERP                   1   xyzw        2     NONE   float   x   
// INTERP                   2   xyzw        3     NONE   float   xyz 
// INTERP                   3   xyz         4     NONE   float   xyz 
// INTERP                   4   xyz         5     NONE   float   xyz 
// SV_IsFrontFace           0   x           6    FFACE    uint   x   
//
//
// Output signature:
//
// Name                 Index   Mask Register SysValue  Format   Used
// -------------------- ----- ------ -------- -------- ------- ------
// SV_Target                0   xyzw        0   TARGET   float   xyzw
// SV_Target                1   x           1   TARGET    uint   x   
//
      ps_5_0
      dcl_globalFlags refactoringAllowed
      dcl_constantbuffer CB0[131], immediateIndexed
      dcl_constantbuffer CB1[11], immediateIndexed
      dcl_constantbuffer CB2[15], immediateIndexed
      dcl_sampler s0, mode_default
      dcl_sampler s1, mode_default
      dcl_sampler s2, mode_default
      dcl_sampler s3, mode_default
      dcl_sampler s4, mode_default
      dcl_resource_texture2d (float,float,float,float) t0
      dcl_resource_texture2d (float,float,float,float) t1
      dcl_resource_texture2d (float,float,float,float) t2
      dcl_resource_texture2d (float,float,float,float) t3
      dcl_resource_texture2d (float,float,float,float) t4
      dcl_input_ps_siv linear noperspective v0.xy, position
      dcl_input_ps linear v1.xyzw
      dcl_input_ps linear v2.x
      dcl_input_ps linear v3.xyz
      dcl_input_ps linear v4.xyz
      dcl_input_ps linear v5.xyz
      dcl_input_ps_sgv constant v6.x, is_front_face
      dcl_output o0.xyzw
      dcl_output o1.x
      dcl_temps 4
   0: dp3 r0.x, v5.xyzx, v5.xyzx
   1: sqrt r0.x, r0.x
   2: div r0.x, l(1.000000, 1.000000, 1.000000, 1.000000), r0.x
   3: mul r0.xyz, r0.xxxx, v5.xyzx
   4: dp3 r0.w, r0.xyzx, r0.xyzx
   5: rsq r0.w, r0.w
   6: mul r0.xyz, r0.wwww, r0.xyzx
   7: add r1.xyz, -v4.xyzx, cb0[21].xyzx
   8: dp3 r0.w, r1.xyzx, r1.xyzx
   9: rsq r0.w, r0.w
  10: mul r1.xyz, r0.wwww, r1.xyzx
  11: eq r0.w, cb0[25].w, l(0.000000)
  12: movc r2.x, r0.w, r1.x, cb0[66].z
  13: movc r2.y, r0.w, r1.y, cb0[67].z
  14: movc r2.z, r0.w, r1.z, cb0[68].z
  15: dp3_sat r0.x, r0.xyzx, r2.xyzx
  16: add r0.x, -r0.x, l(1.000000)
  17: log r0.x, r0.x
  18: mul r0.x, r0.x, cb2[0].y
  19: exp r0.x, r0.x
  20: mul_sat r0.x, r0.x, cb2[0].z
  21: and r0.x, r0.x, v6.x
  22: lt r0.y, cb0[22].x, l(0.000000)
  23: add r0.z, -v0.y, cb0[3].y
  24: movc r1.y, r0.y, r0.z, v0.y
  25: mov r1.x, v0.x
  26: div r1.xy, r1.xyxx, cb0[3].xyxx
  27: add r1.z, -r1.y, l(1.000000)
  28: mad r0.yz, -cb0[130].xxyx, l(0.000000, 0.500000, 0.500000, 0.000000), l(0.000000, 1.000000, 1.000000, 0.000000)
  29: min r0.yz, r0.yyzy, r1.xxzx
  30: mul r0.yz, r0.yyzy, cb0[28].xxyx
  31: sample_b_indexable(texture2d)(float,float,float,float) r0.y, r0.yzyy, t0.yxzw, s0, cb0[4].x
  32: mad r0.y, cb0[24].x, r0.y, cb0[24].y
  33: div r0.y, l(1.000000, 1.000000, 1.000000, 1.000000), r0.y
  34: mul r0.z, v4.y, cb0[79].w
  35: mad r0.z, cb0[78].w, v4.x, r0.z
  36: mad r0.z, cb0[80].w, v4.z, r0.z
  37: add r0.z, r0.z, cb0[81].w
  38: mad r0.y, r0.y, cb0[22].z, -r0.z
  39: div_sat r0.y, r0.y, cb2[14].w
  40: ge r0.z, l(0.000000), r0.y
  41: and r0.z, r0.z, l(0x3f800000)
  42: add r0.w, -r0.y, l(1.000000)
  43: mad r0.y, r0.z, r0.w, r0.y
  44: add r0.x, -r0.y, r0.x
  45: add r0.x, r0.x, l(1.000000)
  46: mul r0.y, v1.w, l(1.000100)
  47: ge r0.z, r0.y, -r0.y
  48: frc r0.y, |r0.y|
  49: movc r0.y, r0.z, r0.y, -r0.y
  50: mul r0.y, r0.y, l(0.999900)
  51: div r0.zw, cb2[10].zzzw, cb2[5].xxxy
  52: mad r0.yz, cb0[19].xxxx, r0.zzwz, r0.yyyy
  53: add r0.yz, r0.yyzy, v1.xxyx
  54: mad r0.yz, r0.yyzy, cb2[5].xxyx, cb2[5].zzwz
  55: sample_b_indexable(texture2d)(float,float,float,float) r1.xyzw, r0.yzyy, t2.xyzw, s2, cb0[4].x
  56: div r0.yz, cb2[11].xxyx, cb2[7].xxyx
  57: mad r0.yz, r0.yyzy, cb0[19].xxxx, v1.xxyx
  58: mad r0.yz, r0.yyzy, cb2[7].xxyx, cb2[7].zzwz
  59: sample_b_indexable(texture2d)(float,float,float,float) r0.yz, r0.yzyy, t3.zxyw, s3, cb0[4].x
  60: mad r2.xy, v1.xyxx, cb2[9].xyxx, cb2[9].zwzz
  61: sample_b_indexable(texture2d)(float,float,float,float) r2.xyz, r2.xyxx, t4.xyzw, s4, cb0[4].x
  62: mul r0.yz, r0.yyzy, r2.xxyx
  63: mul r0.yz, r0.yyzy, cb2[11].zzzz
  64: div r0.yz, r0.yyzy, cb2[3].xxyx
  65: div r3.xy, cb2[10].xyxx, cb2[3].xyxx
  66: mad r3.xy, r3.xyxx, cb0[19].xxxx, v1.xyxx
  67: add r0.yz, -r0.yyzy, r3.xxyx
  68: mad r0.yz, r0.yyzy, cb2[3].xxyx, cb2[3].zzwz
  69: sample_b_indexable(texture2d)(float,float,float,float) r3.xyzw, r0.yzyy, t1.xyzw, s1, cb0[4].x
  70: mul r1.xyzw, r1.wxyz, r3.wxyz
  71: ne r0.yz, l(0.000000, 0.000000, 0.000000, 0.000000), cb2[1].xxyx
  72: movc r3.xyz, r0.yyyy, r1.xxxx, r1.yzwy
  73: max r0.xyw, r0.xxxx, r3.xyxz
  74: movc r0.xyz, r0.zzzz, r0.xywx, r1.yzwy
  75: add r0.w, -v1.z, l(1.000000)
  76: add_sat r1.xyz, -r0.wwww, r2.xyzx
  77: mul_sat r1.xyz, r1.xyzx, r2.xyzx
  78: mul r1.xyz, r0.xyzx, r1.xyzx
  79: ne r0.w, l(0.000000, 0.000000, 0.000000, 0.000000), cb2[14].y
  80: movc r0.xyz, r0.wwww, r1.xyzx, r0.xyzx
  81: mul r0.xyz, r0.xyzx, cb2[12].xxxx
  82: mul r0.xyz, r0.xyzx, v3.xyzx
  83: max r0.w, v2.x, l(1.000000)
  84: min r0.w, r0.w, l(1000000.000000)
  85: eq r1.x, v1.x, v2.x
  86: movc r0.w, r1.x, l(1.000000), r0.w
  87: mul o0.xyz, r0.xyzx, r0.wwww
  88: mov o0.w, l(1.000000)
  89: and o1.x, cb0[11].x, cb1[10].x
  90: ret 
// Approximately 0 instruction slots used


//////////////////////////////////////////////////////
Keywords: LIGHTMAP_BICUBIC_SAMPLING _SURFACE_TYPE_TRANSPARENT _WRITE_RENDERING_LAYERS
-- Vertex shader for "d3d11":
// No shader variant for this keyword set. The closest match will be used instead.

-- Hardware tier variant: Tier 1
-- Fragment shader for "d3d11":
// Stats: 77 math, 4 temp registers
Set 2D Texture "_CameraDepthTexture" to slot 0 sampler slot -1
Set 2D Texture "_MainTex" to slot 1
Set 2D Texture "_Noise" to slot 2
Set 2D Texture "_Flow" to slot 3
Set 2D Texture "_Mask" to slot 4

Set Sampler PointClamp to slot 0

Constant Buffer "$Globals" (2096 bytes) on slot 0 {
  Matrix4x4 unity_MatrixV at 1056
  Matrix4x4 unity_MatrixVP at 1248
  Vector4 _ScaledScreenParams at 48
  Vector2 _GlobalMipBias at 64
  ScalarInt _RenderingLayerMaxInt at 176
  Vector4 _TimeParameters at 304
  Vector3 _WorldSpaceCameraPos at 336
  Vector4 _ProjectionParams at 352
  Vector4 _ZBufferParams at 384
  Vector4 unity_OrthoParams at 400
  Vector4 _RTHandleScale at 448
  Vector4 _CameraDepthTexture_TexelSize at 2080
}
Constant Buffer "UnityPerDraw" (720 bytes) on slot 1 {
  Vector4 unity_RenderingLayer at 160
}
Constant Buffer "UnityPerMaterial" (304 bytes) on slot 2 {
  Float _Textureopacity at 0
  Float _Fresnelpower at 4
  Float _Fresnelscale at 8
  Float _Multiply_texture at 12
  Float _Useonlycolor at 16
  Float _Use_fresnel at 20
  Vector4 _MainTex_ST at 48
  Vector4 _Noise_ST at 80
  Vector4 _Flow_ST at 112
  Vector4 _Mask_ST at 144
  Vector4 _SpeedMainTexUVNoiseZW at 160
  Vector4 _DistortionSpeedXYPowerZ at 176
  Float _Emission at 192
  Float _Opacity at 224
  Float _Usecenterglow at 228
  Float _Usedepth at 232
  Float _Depthpower at 236
}

Shader Disassembly:
//
// Generated by Microsoft (R) D3D Shader Disassembler
//
//
// Input signature:
//
// Name                 Index   Mask Register SysValue  Format   Used
// -------------------- ----- ------ -------- -------- ------- ------
// SV_POSITION              0   xyzw        0      POS   float   xy  
// INTERP                   0   xyzw        1     NONE   float   xyzw
// INTERP                   1   xyzw        2     NONE   float   x   
// INTERP                   2   xyzw        3     NONE   float   xyzw
// INTERP                   3   xyz         4     NONE   float   xyz 
// INTERP                   4   xyz         5     NONE   float   xyz 
// SV_IsFrontFace           0   x           6    FFACE    uint   x   
//
//
// Output signature:
//
// Name                 Index   Mask Register SysValue  Format   Used
// -------------------- ----- ------ -------- -------- ------- ------
// SV_Target                0   xyzw        0   TARGET   float   xyzw
// SV_Target                1   x           1   TARGET    uint   x   
//
      ps_5_0
      dcl_globalFlags refactoringAllowed
      dcl_constantbuffer CB0[131], immediateIndexed
      dcl_constantbuffer CB1[11], immediateIndexed
      dcl_constantbuffer CB2[15], immediateIndexed
      dcl_sampler s0, mode_default
      dcl_sampler s1, mode_default
      dcl_sampler s2, mode_default
      dcl_sampler s3, mode_default
      dcl_sampler s4, mode_default
      dcl_resource_texture2d (float,float,float,float) t0
      dcl_resource_texture2d (float,float,float,float) t1
      dcl_resource_texture2d (float,float,float,float) t2
      dcl_resource_texture2d (float,float,float,float) t3
      dcl_resource_texture2d (float,float,float,float) t4
      dcl_input_ps_siv linear noperspective v0.xy, position
      dcl_input_ps linear v1.xyzw
      dcl_input_ps linear v2.x
      dcl_input_ps linear v3.xyzw
      dcl_input_ps linear v4.xyz
      dcl_input_ps linear v5.xyz
      dcl_input_ps_sgv constant v6.x, is_front_face
      dcl_output o0.xyzw
      dcl_output o1.x
      dcl_temps 4
   0: dp3 r0.x, v5.xyzx, v5.xyzx
   1: sqrt r0.x, r0.x
   2: div r0.x, l(1.000000, 1.000000, 1.000000, 1.000000), r0.x
   3: mul r0.xyz, r0.xxxx, v5.xyzx
   4: dp3 r0.w, r0.xyzx, r0.xyzx
   5: rsq r0.w, r0.w
   6: mul r0.xyz, r0.wwww, r0.xyzx
   7: add r1.xyz, -v4.xyzx, cb0[21].xyzx
   8: dp3 r0.w, r1.xyzx, r1.xyzx
   9: rsq r0.w, r0.w
  10: mul r1.xyz, r0.wwww, r1.xyzx
  11: eq r0.w, cb0[25].w, l(0.000000)
  12: movc r2.x, r0.w, r1.x, cb0[66].z
  13: movc r2.y, r0.w, r1.y, cb0[67].z
  14: movc r2.z, r0.w, r1.z, cb0[68].z
  15: dp3_sat r0.x, r0.xyzx, r2.xyzx
  16: add r0.x, -r0.x, l(1.000000)
  17: log r0.x, r0.x
  18: mul r0.x, r0.x, cb2[0].y
  19: exp r0.x, r0.x
  20: mul_sat r0.x, r0.x, cb2[0].z
  21: and r0.x, r0.x, v6.x
  22: lt r0.y, cb0[22].x, l(0.000000)
  23: add r0.z, -v0.y, cb0[3].y
  24: movc r1.y, r0.y, r0.z, v0.y
  25: mov r1.x, v0.x
  26: div r1.xy, r1.xyxx, cb0[3].xyxx
  27: add r1.z, -r1.y, l(1.000000)
  28: mad r0.yz, -cb0[130].xxyx, l(0.000000, 0.500000, 0.500000, 0.000000), l(0.000000, 1.000000, 1.000000, 0.000000)
  29: min r0.yz, r0.yyzy, r1.xxzx
  30: mul r0.yz, r0.yyzy, cb0[28].xxyx
  31: sample_b_indexable(texture2d)(float,float,float,float) r0.y, r0.yzyy, t0.yxzw, s0, cb0[4].x
  32: mad r0.y, cb0[24].x, r0.y, cb0[24].y
  33: div r0.y, l(1.000000, 1.000000, 1.000000, 1.000000), r0.y
  34: mul r0.z, v4.y, cb0[79].w
  35: mad r0.z, cb0[78].w, v4.x, r0.z
  36: mad r0.z, cb0[80].w, v4.z, r0.z
  37: add r0.z, r0.z, cb0[81].w
  38: mad r0.y, r0.y, cb0[22].z, -r0.z
  39: div_sat r0.y, r0.y, cb2[14].w
  40: ge r0.z, l(0.000000), r0.y
  41: and r0.z, r0.z, l(0x3f800000)
  42: add r0.w, -r0.y, l(1.000000)
  43: mad r0.y, r0.z, r0.w, r0.y
  44: add r0.x, -r0.y, r0.x
  45: add r0.x, r0.x, l(1.000000)
  46: mul r0.z, v1.w, l(1.000100)
  47: ge r0.w, r0.z, -r0.z
  48: frc r0.z, |r0.z|
  49: movc r0.z, r0.w, r0.z, -r0.z
  50: mul r0.z, r0.z, l(0.999900)
  51: div r1.xy, cb2[10].zwzz, cb2[5].xyxx
  52: mad r0.zw, cb0[19].xxxx, r1.xxxy, r0.zzzz
  53: add r0.zw, r0.zzzw, v1.xxxy
  54: mad r0.zw, r0.zzzw, cb2[5].xxxy, cb2[5].zzzw
  55: sample_b_indexable(texture2d)(float,float,float,float) r1.xyzw, r0.zwzz, t2.xyzw, s2, cb0[4].x
  56: div r0.zw, cb2[11].xxxy, cb2[7].xxxy
  57: mad r0.zw, r0.zzzw, cb0[19].xxxx, v1.xxxy
  58: mad r0.zw, r0.zzzw, cb2[7].xxxy, cb2[7].zzzw
  59: sample_b_indexable(texture2d)(float,float,float,float) r0.zw, r0.zwzz, t3.zwxy, s3, cb0[4].x
  60: mad r2.xy, v1.xyxx, cb2[9].xyxx, cb2[9].zwzz
  61: sample_b_indexable(texture2d)(float,float,float,float) r2.xyz, r2.xyxx, t4.xyzw, s4, cb0[4].x
  62: mul r0.zw, r0.zzzw, r2.xxxy
  63: mul r0.zw, r0.zzzw, cb2[11].zzzz
  64: div r0.zw, r0.zzzw, cb2[3].xxxy
  65: div r3.xy, cb2[10].xyxx, cb2[3].xyxx
  66: mad r3.xy, r3.xyxx, cb0[19].xxxx, v1.xyxx
  67: add r0.zw, -r0.zzzw, r3.xxxy
  68: mad r0.zw, r0.zzzw, cb2[3].xxxy, cb2[3].zzzw
  69: sample_b_indexable(texture2d)(float,float,float,float) r3.xyzw, r0.zwzz, t1.xyzw, s1, cb0[4].x
  70: mul r1.xyzw, r1.wxyz, r3.wxyz
  71: ne r0.zw, l(0.000000, 0.000000, 0.000000, 0.000000), cb2[1].xxxy
  72: movc r3.xyz, r0.zzzz, r1.xxxx, r1.yzwy
  73: max r3.xyz, r0.xxxx, r3.xyzx
  74: mad_sat r0.x, r1.x, cb2[0].x, r0.x
  75: movc r1.yzw, r0.wwww, r3.xxyz, r1.yyzw
  76: add r0.z, -v1.z, l(1.000000)
  77: add_sat r3.xyz, -r0.zzzz, r2.xyzx
  78: mul_sat r2.xyz, r2.xyzx, r3.xyzx
  79: mul r2.xyz, r1.yzwy, r2.xyzx
  80: ne r3.xy, l(0.000000, 0.000000, 0.000000, 0.000000), cb2[14].yzyy
  81: movc r1.yzw, r3.xxxx, r2.xxyz, r1.yyzw
  82: mul r1.yzw, r1.yyzw, cb2[12].xxxx
  83: mul r1.yzw, r1.yyzw, v3.xxyz
  84: max r0.z, v2.x, l(1.000000)
  85: min r0.z, r0.z, l(1000000.000000)
  86: eq r2.x, v1.x, v2.x
  87: movc r0.z, r2.x, l(1.000000), r0.z
  88: mul o0.xyz, r1.yzwy, r0.zzzz
  89: mul r0.z, r0.x, r1.x
  90: ne r1.y, l(0.000000, 0.000000, 0.000000, 0.000000), cb2[0].w
  91: movc r0.x, r1.y, r0.z, r0.x
  92: movc r0.x, r0.w, r0.x, r1.x
  93: mul r0.y, r0.y, r0.x
  94: movc r0.x, r3.y, r0.y, r0.x
  95: mul r0.x, r0.x, v3.w
  96: mul_sat o0.w, r0.x, cb2[14].x
  97: and o1.x, cb0[11].x, cb1[10].x
  98: ret 
// Approximately 0 instruction slots used


//////////////////////////////////////////////////////
Keywords: FOG_EXP2 LIGHTMAP_BICUBIC_SAMPLING _WRITE_RENDERING_LAYERS
-- Vertex shader for "d3d11":
// No shader variant for this keyword set. The closest match will be used instead.

-- Hardware tier variant: Tier 1
-- Fragment shader for "d3d11":
// Stats: 72 math, 4 temp registers
Set 2D Texture "_CameraDepthTexture" to slot 0 sampler slot -1
Set 2D Texture "_MainTex" to slot 1
Set 2D Texture "_Noise" to slot 2
Set 2D Texture "_Flow" to slot 3
Set 2D Texture "_Mask" to slot 4

Set Sampler PointClamp to slot 0

Constant Buffer "$Globals" (2096 bytes) on slot 0 {
  Matrix4x4 unity_MatrixV at 1056
  Matrix4x4 unity_MatrixVP at 1248
  Vector4 _ScaledScreenParams at 48
  Vector2 _GlobalMipBias at 64
  ScalarInt _RenderingLayerMaxInt at 176
  Vector4 _TimeParameters at 304
  Vector3 _WorldSpaceCameraPos at 336
  Vector4 _ProjectionParams at 352
  Vector4 _ZBufferParams at 384
  Vector4 unity_OrthoParams at 400
  Vector4 _RTHandleScale at 448
  Vector4 _CameraDepthTexture_TexelSize at 2080
}
Constant Buffer "UnityPerDraw" (720 bytes) on slot 1 {
  Vector4 unity_RenderingLayer at 160
}
Constant Buffer "UnityPerMaterial" (304 bytes) on slot 2 {
  Float _Fresnelpower at 4
  Float _Fresnelscale at 8
  Float _Useonlycolor at 16
  Float _Use_fresnel at 20
  Vector4 _MainTex_ST at 48
  Vector4 _Noise_ST at 80
  Vector4 _Flow_ST at 112
  Vector4 _Mask_ST at 144
  Vector4 _SpeedMainTexUVNoiseZW at 160
  Vector4 _DistortionSpeedXYPowerZ at 176
  Float _Emission at 192
  Float _Usecenterglow at 228
  Float _Depthpower at 236
}

Shader Disassembly:
//
// Generated by Microsoft (R) D3D Shader Disassembler
//
//
// Input signature:
//
// Name                 Index   Mask Register SysValue  Format   Used
// -------------------- ----- ------ -------- -------- ------- ------
// SV_POSITION              0   xyzw        0      POS   float   xy  
// INTERP                   0   xyzw        1     NONE   float   xyzw
// INTERP                   1   xyzw        2     NONE   float   x   
// INTERP                   2   xyzw        3     NONE   float   xyz 
// INTERP                   3   xyz         4     NONE   float   xyz 
// INTERP                   4   xyz         5     NONE   float   xyz 
// SV_IsFrontFace           0   x           6    FFACE    uint   x   
//
//
// Output signature:
//
// Name                 Index   Mask Register SysValue  Format   Used
// -------------------- ----- ------ -------- -------- ------- ------
// SV_Target                0   xyzw        0   TARGET   float   xyzw
// SV_Target                1   x           1   TARGET    uint   x   
//
      ps_5_0
      dcl_globalFlags refactoringAllowed
      dcl_constantbuffer CB0[131], immediateIndexed
      dcl_constantbuffer CB1[11], immediateIndexed
      dcl_constantbuffer CB2[15], immediateIndexed
      dcl_sampler s0, mode_default
      dcl_sampler s1, mode_default
      dcl_sampler s2, mode_default
      dcl_sampler s3, mode_default
      dcl_sampler s4, mode_default
      dcl_resource_texture2d (float,float,float,float) t0
      dcl_resource_texture2d (float,float,float,float) t1
      dcl_resource_texture2d (float,float,float,float) t2
      dcl_resource_texture2d (float,float,float,float) t3
      dcl_resource_texture2d (float,float,float,float) t4
      dcl_input_ps_siv linear noperspective v0.xy, position
      dcl_input_ps linear v1.xyzw
      dcl_input_ps linear v2.x
      dcl_input_ps linear v3.xyz
      dcl_input_ps linear v4.xyz
      dcl_input_ps linear v5.xyz
      dcl_input_ps_sgv constant v6.x, is_front_face
      dcl_output o0.xyzw
      dcl_output o1.x
      dcl_temps 4
   0: dp3 r0.x, v5.xyzx, v5.xyzx
   1: sqrt r0.x, r0.x
   2: div r0.x, l(1.000000, 1.000000, 1.000000, 1.000000), r0.x
   3: mul r0.xyz, r0.xxxx, v5.xyzx
   4: dp3 r0.w, r0.xyzx, r0.xyzx
   5: rsq r0.w, r0.w
   6: mul r0.xyz, r0.wwww, r0.xyzx
   7: add r1.xyz, -v4.xyzx, cb0[21].xyzx
   8: dp3 r0.w, r1.xyzx, r1.xyzx
   9: rsq r0.w, r0.w
  10: mul r1.xyz, r0.wwww, r1.xyzx
  11: eq r0.w, cb0[25].w, l(0.000000)
  12: movc r2.x, r0.w, r1.x, cb0[66].z
  13: movc r2.y, r0.w, r1.y, cb0[67].z
  14: movc r2.z, r0.w, r1.z, cb0[68].z
  15: dp3_sat r0.x, r0.xyzx, r2.xyzx
  16: add r0.x, -r0.x, l(1.000000)
  17: log r0.x, r0.x
  18: mul r0.x, r0.x, cb2[0].y
  19: exp r0.x, r0.x
  20: mul_sat r0.x, r0.x, cb2[0].z
  21: and r0.x, r0.x, v6.x
  22: lt r0.y, cb0[22].x, l(0.000000)
  23: add r0.z, -v0.y, cb0[3].y
  24: movc r1.y, r0.y, r0.z, v0.y
  25: mov r1.x, v0.x
  26: div r1.xy, r1.xyxx, cb0[3].xyxx
  27: add r1.z, -r1.y, l(1.000000)
  28: mad r0.yz, -cb0[130].xxyx, l(0.000000, 0.500000, 0.500000, 0.000000), l(0.000000, 1.000000, 1.000000, 0.000000)
  29: min r0.yz, r0.yyzy, r1.xxzx
  30: mul r0.yz, r0.yyzy, cb0[28].xxyx
  31: sample_b_indexable(texture2d)(float,float,float,float) r0.y, r0.yzyy, t0.yxzw, s0, cb0[4].x
  32: mad r0.y, cb0[24].x, r0.y, cb0[24].y
  33: div r0.y, l(1.000000, 1.000000, 1.000000, 1.000000), r0.y
  34: mul r0.z, v4.y, cb0[79].w
  35: mad r0.z, cb0[78].w, v4.x, r0.z
  36: mad r0.z, cb0[80].w, v4.z, r0.z
  37: add r0.z, r0.z, cb0[81].w
  38: mad r0.y, r0.y, cb0[22].z, -r0.z
  39: div_sat r0.y, r0.y, cb2[14].w
  40: ge r0.z, l(0.000000), r0.y
  41: and r0.z, r0.z, l(0x3f800000)
  42: add r0.w, -r0.y, l(1.000000)
  43: mad r0.y, r0.z, r0.w, r0.y
  44: add r0.x, -r0.y, r0.x
  45: add r0.x, r0.x, l(1.000000)
  46: mul r0.y, v1.w, l(1.000100)
  47: ge r0.z, r0.y, -r0.y
  48: frc r0.y, |r0.y|
  49: movc r0.y, r0.z, r0.y, -r0.y
  50: mul r0.y, r0.y, l(0.999900)
  51: div r0.zw, cb2[10].zzzw, cb2[5].xxxy
  52: mad r0.yz, cb0[19].xxxx, r0.zzwz, r0.yyyy
  53: add r0.yz, r0.yyzy, v1.xxyx
  54: mad r0.yz, r0.yyzy, cb2[5].xxyx, cb2[5].zzwz
  55: sample_b_indexable(texture2d)(float,float,float,float) r1.xyzw, r0.yzyy, t2.xyzw, s2, cb0[4].x
  56: div r0.yz, cb2[11].xxyx, cb2[7].xxyx
  57: mad r0.yz, r0.yyzy, cb0[19].xxxx, v1.xxyx
  58: mad r0.yz, r0.yyzy, cb2[7].xxyx, cb2[7].zzwz
  59: sample_b_indexable(texture2d)(float,float,float,float) r0.yz, r0.yzyy, t3.zxyw, s3, cb0[4].x
  60: mad r2.xy, v1.xyxx, cb2[9].xyxx, cb2[9].zwzz
  61: sample_b_indexable(texture2d)(float,float,float,float) r2.xyz, r2.xyxx, t4.xyzw, s4, cb0[4].x
  62: mul r0.yz, r0.yyzy, r2.xxyx
  63: mul r0.yz, r0.yyzy, cb2[11].zzzz
  64: div r0.yz, r0.yyzy, cb2[3].xxyx
  65: div r3.xy, cb2[10].xyxx, cb2[3].xyxx
  66: mad r3.xy, r3.xyxx, cb0[19].xxxx, v1.xyxx
  67: add r0.yz, -r0.yyzy, r3.xxyx
  68: mad r0.yz, r0.yyzy, cb2[3].xxyx, cb2[3].zzwz
  69: sample_b_indexable(texture2d)(float,float,float,float) r3.xyzw, r0.yzyy, t1.xyzw, s1, cb0[4].x
  70: mul r1.xyzw, r1.wxyz, r3.wxyz
  71: ne r0.yz, l(0.000000, 0.000000, 0.000000, 0.000000), cb2[1].xxyx
  72: movc r3.xyz, r0.yyyy, r1.xxxx, r1.yzwy
  73: max r0.xyw, r0.xxxx, r3.xyxz
  74: movc r0.xyz, r0.zzzz, r0.xywx, r1.yzwy
  75: add r0.w, -v1.z, l(1.000000)
  76: add_sat r1.xyz, -r0.wwww, r2.xyzx
  77: mul_sat r1.xyz, r1.xyzx, r2.xyzx
  78: mul r1.xyz, r0.xyzx, r1.xyzx
  79: ne r0.w, l(0.000000, 0.000000, 0.000000, 0.000000), cb2[14].y
  80: movc r0.xyz, r0.wwww, r1.xyzx, r0.xyzx
  81: mul r0.xyz, r0.xyzx, cb2[12].xxxx
  82: mul r0.xyz, r0.xyzx, v3.xyzx
  83: max r0.w, v2.x, l(1.000000)
  84: min r0.w, r0.w, l(1000000.000000)
  85: eq r1.x, v1.x, v2.x
  86: movc r0.w, r1.x, l(1.000000), r0.w
  87: mul o0.xyz, r0.xyzx, r0.wwww
  88: mov o0.w, l(1.000000)
  89: and o1.x, cb0[11].x, cb1[10].x
  90: ret 
// Approximately 0 instruction slots used


//////////////////////////////////////////////////////
Keywords: FOG_EXP2 LIGHTMAP_BICUBIC_SAMPLING _SURFACE_TYPE_TRANSPARENT _WRITE_RENDERING_LAYERS
-- Vertex shader for "d3d11":
// No shader variant for this keyword set. The closest match will be used instead.

-- Hardware tier variant: Tier 1
-- Fragment shader for "d3d11":
// Stats: 77 math, 4 temp registers
Set 2D Texture "_CameraDepthTexture" to slot 0 sampler slot -1
Set 2D Texture "_MainTex" to slot 1
Set 2D Texture "_Noise" to slot 2
Set 2D Texture "_Flow" to slot 3
Set 2D Texture "_Mask" to slot 4

Set Sampler PointClamp to slot 0

Constant Buffer "$Globals" (2096 bytes) on slot 0 {
  Matrix4x4 unity_MatrixV at 1056
  Matrix4x4 unity_MatrixVP at 1248
  Vector4 _ScaledScreenParams at 48
  Vector2 _GlobalMipBias at 64
  ScalarInt _RenderingLayerMaxInt at 176
  Vector4 _TimeParameters at 304
  Vector3 _WorldSpaceCameraPos at 336
  Vector4 _ProjectionParams at 352
  Vector4 _ZBufferParams at 384
  Vector4 unity_OrthoParams at 400
  Vector4 _RTHandleScale at 448
  Vector4 _CameraDepthTexture_TexelSize at 2080
}
Constant Buffer "UnityPerDraw" (720 bytes) on slot 1 {
  Vector4 unity_RenderingLayer at 160
}
Constant Buffer "UnityPerMaterial" (304 bytes) on slot 2 {
  Float _Textureopacity at 0
  Float _Fresnelpower at 4
  Float _Fresnelscale at 8
  Float _Multiply_texture at 12
  Float _Useonlycolor at 16
  Float _Use_fresnel at 20
  Vector4 _MainTex_ST at 48
  Vector4 _Noise_ST at 80
  Vector4 _Flow_ST at 112
  Vector4 _Mask_ST at 144
  Vector4 _SpeedMainTexUVNoiseZW at 160
  Vector4 _DistortionSpeedXYPowerZ at 176
  Float _Emission at 192
  Float _Opacity at 224
  Float _Usecenterglow at 228
  Float _Usedepth at 232
  Float _Depthpower at 236
}

Shader Disassembly:
//
// Generated by Microsoft (R) D3D Shader Disassembler
//
//
// Input signature:
//
// Name                 Index   Mask Register SysValue  Format   Used
// -------------------- ----- ------ -------- -------- ------- ------
// SV_POSITION              0   xyzw        0      POS   float   xy  
// INTERP                   0   xyzw        1     NONE   float   xyzw
// INTERP                   1   xyzw        2     NONE   float   x   
// INTERP                   2   xyzw        3     NONE   float   xyzw
// INTERP                   3   xyz         4     NONE   float   xyz 
// INTERP                   4   xyz         5     NONE   float   xyz 
// SV_IsFrontFace           0   x           6    FFACE    uint   x   
//
//
// Output signature:
//
// Name                 Index   Mask Register SysValue  Format   Used
// -------------------- ----- ------ -------- -------- ------- ------
// SV_Target                0   xyzw        0   TARGET   float   xyzw
// SV_Target                1   x           1   TARGET    uint   x   
//
      ps_5_0
      dcl_globalFlags refactoringAllowed
      dcl_constantbuffer CB0[131], immediateIndexed
      dcl_constantbuffer CB1[11], immediateIndexed
      dcl_constantbuffer CB2[15], immediateIndexed
      dcl_sampler s0, mode_default
      dcl_sampler s1, mode_default
      dcl_sampler s2, mode_default
      dcl_sampler s3, mode_default
      dcl_sampler s4, mode_default
      dcl_resource_texture2d (float,float,float,float) t0
      dcl_resource_texture2d (float,float,float,float) t1
      dcl_resource_texture2d (float,float,float,float) t2
      dcl_resource_texture2d (float,float,float,float) t3
      dcl_resource_texture2d (float,float,float,float) t4
      dcl_input_ps_siv linear noperspective v0.xy, position
      dcl_input_ps linear v1.xyzw
      dcl_input_ps linear v2.x
      dcl_input_ps linear v3.xyzw
      dcl_input_ps linear v4.xyz
      dcl_input_ps linear v5.xyz
      dcl_input_ps_sgv constant v6.x, is_front_face
      dcl_output o0.xyzw
      dcl_output o1.x
      dcl_temps 4
   0: dp3 r0.x, v5.xyzx, v5.xyzx
   1: sqrt r0.x, r0.x
   2: div r0.x, l(1.000000, 1.000000, 1.000000, 1.000000), r0.x
   3: mul r0.xyz, r0.xxxx, v5.xyzx
   4: dp3 r0.w, r0.xyzx, r0.xyzx
   5: rsq r0.w, r0.w
   6: mul r0.xyz, r0.wwww, r0.xyzx
   7: add r1.xyz, -v4.xyzx, cb0[21].xyzx
   8: dp3 r0.w, r1.xyzx, r1.xyzx
   9: rsq r0.w, r0.w
  10: mul r1.xyz, r0.wwww, r1.xyzx
  11: eq r0.w, cb0[25].w, l(0.000000)
  12: movc r2.x, r0.w, r1.x, cb0[66].z
  13: movc r2.y, r0.w, r1.y, cb0[67].z
  14: movc r2.z, r0.w, r1.z, cb0[68].z
  15: dp3_sat r0.x, r0.xyzx, r2.xyzx
  16: add r0.x, -r0.x, l(1.000000)
  17: log r0.x, r0.x
  18: mul r0.x, r0.x, cb2[0].y
  19: exp r0.x, r0.x
  20: mul_sat r0.x, r0.x, cb2[0].z
  21: and r0.x, r0.x, v6.x
  22: lt r0.y, cb0[22].x, l(0.000000)
  23: add r0.z, -v0.y, cb0[3].y
  24: movc r1.y, r0.y, r0.z, v0.y
  25: mov r1.x, v0.x
  26: div r1.xy, r1.xyxx, cb0[3].xyxx
  27: add r1.z, -r1.y, l(1.000000)
  28: mad r0.yz, -cb0[130].xxyx, l(0.000000, 0.500000, 0.500000, 0.000000), l(0.000000, 1.000000, 1.000000, 0.000000)
  29: min r0.yz, r0.yyzy, r1.xxzx
  30: mul r0.yz, r0.yyzy, cb0[28].xxyx
  31: sample_b_indexable(texture2d)(float,float,float,float) r0.y, r0.yzyy, t0.yxzw, s0, cb0[4].x
  32: mad r0.y, cb0[24].x, r0.y, cb0[24].y
  33: div r0.y, l(1.000000, 1.000000, 1.000000, 1.000000), r0.y
  34: mul r0.z, v4.y, cb0[79].w
  35: mad r0.z, cb0[78].w, v4.x, r0.z
  36: mad r0.z, cb0[80].w, v4.z, r0.z
  37: add r0.z, r0.z, cb0[81].w
  38: mad r0.y, r0.y, cb0[22].z, -r0.z
  39: div_sat r0.y, r0.y, cb2[14].w
  40: ge r0.z, l(0.000000), r0.y
  41: and r0.z, r0.z, l(0x3f800000)
  42: add r0.w, -r0.y, l(1.000000)
  43: mad r0.y, r0.z, r0.w, r0.y
  44: add r0.x, -r0.y, r0.x
  45: add r0.x, r0.x, l(1.000000)
  46: mul r0.z, v1.w, l(1.000100)
  47: ge r0.w, r0.z, -r0.z
  48: frc r0.z, |r0.z|
  49: movc r0.z, r0.w, r0.z, -r0.z
  50: mul r0.z, r0.z, l(0.999900)
  51: div r1.xy, cb2[10].zwzz, cb2[5].xyxx
  52: mad r0.zw, cb0[19].xxxx, r1.xxxy, r0.zzzz
  53: add r0.zw, r0.zzzw, v1.xxxy
  54: mad r0.zw, r0.zzzw, cb2[5].xxxy, cb2[5].zzzw
  55: sample_b_indexable(texture2d)(float,float,float,float) r1.xyzw, r0.zwzz, t2.xyzw, s2, cb0[4].x
  56: div r0.zw, cb2[11].xxxy, cb2[7].xxxy
  57: mad r0.zw, r0.zzzw, cb0[19].xxxx, v1.xxxy
  58: mad r0.zw, r0.zzzw, cb2[7].xxxy, cb2[7].zzzw
  59: sample_b_indexable(texture2d)(float,float,float,float) r0.zw, r0.zwzz, t3.zwxy, s3, cb0[4].x
  60: mad r2.xy, v1.xyxx, cb2[9].xyxx, cb2[9].zwzz
  61: sample_b_indexable(texture2d)(float,float,float,float) r2.xyz, r2.xyxx, t4.xyzw, s4, cb0[4].x
  62: mul r0.zw, r0.zzzw, r2.xxxy
  63: mul r0.zw, r0.zzzw, cb2[11].zzzz
  64: div r0.zw, r0.zzzw, cb2[3].xxxy
  65: div r3.xy, cb2[10].xyxx, cb2[3].xyxx
  66: mad r3.xy, r3.xyxx, cb0[19].xxxx, v1.xyxx
  67: add r0.zw, -r0.zzzw, r3.xxxy
  68: mad r0.zw, r0.zzzw, cb2[3].xxxy, cb2[3].zzzw
  69: sample_b_indexable(texture2d)(float,float,float,float) r3.xyzw, r0.zwzz, t1.xyzw, s1, cb0[4].x
  70: mul r1.xyzw, r1.wxyz, r3.wxyz
  71: ne r0.zw, l(0.000000, 0.000000, 0.000000, 0.000000), cb2[1].xxxy
  72: movc r3.xyz, r0.zzzz, r1.xxxx, r1.yzwy
  73: max r3.xyz, r0.xxxx, r3.xyzx
  74: mad_sat r0.x, r1.x, cb2[0].x, r0.x
  75: movc r1.yzw, r0.wwww, r3.xxyz, r1.yyzw
  76: add r0.z, -v1.z, l(1.000000)
  77: add_sat r3.xyz, -r0.zzzz, r2.xyzx
  78: mul_sat r2.xyz, r2.xyzx, r3.xyzx
  79: mul r2.xyz, r1.yzwy, r2.xyzx
  80: ne r3.xy, l(0.000000, 0.000000, 0.000000, 0.000000), cb2[14].yzyy
  81: movc r1.yzw, r3.xxxx, r2.xxyz, r1.yyzw
  82: mul r1.yzw, r1.yyzw, cb2[12].xxxx
  83: mul r1.yzw, r1.yyzw, v3.xxyz
  84: max r0.z, v2.x, l(1.000000)
  85: min r0.z, r0.z, l(1000000.000000)
  86: eq r2.x, v1.x, v2.x
  87: movc r0.z, r2.x, l(1.000000), r0.z
  88: mul o0.xyz, r1.yzwy, r0.zzzz
  89: mul r0.z, r0.x, r1.x
  90: ne r1.y, l(0.000000, 0.000000, 0.000000, 0.000000), cb2[0].w
  91: movc r0.x, r1.y, r0.z, r0.x
  92: movc r0.x, r0.w, r0.x, r1.x
  93: mul r0.y, r0.y, r0.x
  94: movc r0.x, r3.y, r0.y, r0.x
  95: mul r0.x, r0.x, v3.w
  96: mul_sat o0.w, r0.x, cb2[14].x
  97: and o1.x, cb0[11].x, cb1[10].x
  98: ret 
// Approximately 0 instruction slots used


//////////////////////////////////////////////////////
Keywords: LIGHTMAP_BICUBIC_SAMPLING _SCREEN_SPACE_OCCLUSION
-- Vertex shader for "d3d11":
// No shader variant for this keyword set. The closest match will be used instead.

-- Hardware tier variant: Tier 1
-- Fragment shader for "d3d11":
// Stats: 80 math, 4 temp registers
Set 2D Texture "_ScreenSpaceOcclusionTexture" to slot 0 sampler slot -1
Set 2D Texture "_CameraDepthTexture" to slot 1 sampler slot -1
Set 2D Texture "_MainTex" to slot 2
Set 2D Texture "_Noise" to slot 3
Set 2D Texture "_Flow" to slot 4
Set 2D Texture "_Mask" to slot 5

Set Sampler PointClamp to slot 0
Set Sampler LinearClamp to slot 1

Constant Buffer "$Globals" (2096 bytes) on slot 0 {
  Matrix4x4 unity_MatrixV at 1056
  Matrix4x4 unity_MatrixVP at 1248
  Vector4 _ScaledScreenParams at 48
  Vector2 _GlobalMipBias at 64
  Vector4 _AmbientOcclusionParam at 144
  Vector4 _TimeParameters at 304
  Vector3 _WorldSpaceCameraPos at 336
  Vector4 _ProjectionParams at 352
  Vector4 _ZBufferParams at 384
  Vector4 unity_OrthoParams at 400
  Vector4 _ScaleBiasRt at 432
  Vector4 _RTHandleScale at 448
  Vector4 _CameraDepthTexture_TexelSize at 2080
}
Constant Buffer "UnityPerMaterial" (304 bytes) on slot 1 {
  Float _Fresnelpower at 4
  Float _Fresnelscale at 8
  Float _Useonlycolor at 16
  Float _Use_fresnel at 20
  Vector4 _MainTex_ST at 48
  Vector4 _Noise_ST at 80
  Vector4 _Flow_ST at 112
  Vector4 _Mask_ST at 144
  Vector4 _SpeedMainTexUVNoiseZW at 160
  Vector4 _DistortionSpeedXYPowerZ at 176
  Float _Emission at 192
  Float _Usecenterglow at 228
  Float _Depthpower at 236
}

Shader Disassembly:
//
// Generated by Microsoft (R) D3D Shader Disassembler
//
//
// Input signature:
//
// Name                 Index   Mask Register SysValue  Format   Used
// -------------------- ----- ------ -------- -------- ------- ------
// SV_POSITION              0   xyzw        0      POS   float   xy  
// INTERP                   0   xyzw        1     NONE   float   xyzw
// INTERP                   1   xyzw        2     NONE   float   x   
// INTERP                   2   xyzw        3     NONE   float   xyz 
// INTERP                   3   xyz         4     NONE   float   xyz 
// INTERP                   4   xyz         5     NONE   float   xyz 
// SV_IsFrontFace           0   x           6    FFACE    uint   x   
//
//
// Output signature:
//
// Name                 Index   Mask Register SysValue  Format   Used
// -------------------- ----- ------ -------- -------- ------- ------
// SV_Target                0   xyzw        0   TARGET   float   xyzw
//
      ps_4_0
      dcl_constantbuffer CB0[131], immediateIndexed
      dcl_constantbuffer CB1[15], immediateIndexed
      dcl_sampler s0, mode_default
      dcl_sampler s1, mode_default
      dcl_sampler s2, mode_default
      dcl_sampler s3, mode_default
      dcl_sampler s4, mode_default
      dcl_sampler s5, mode_default
      dcl_resource_texture2d (float,float,float,float) t0
      dcl_resource_texture2d (float,float,float,float) t1
      dcl_resource_texture2d (float,float,float,float) t2
      dcl_resource_texture2d (float,float,float,float) t3
      dcl_resource_texture2d (float,float,float,float) t4
      dcl_resource_texture2d (float,float,float,float) t5
      dcl_input_ps_siv linear noperspective v0.xy, position
      dcl_input_ps linear v1.xyzw
      dcl_input_ps linear v2.x
      dcl_input_ps linear v3.xyz
      dcl_input_ps linear v4.xyz
      dcl_input_ps linear v5.xyz
      dcl_input_ps_sgv constant v6.x, is_front_face
      dcl_output o0.xyzw
      dcl_temps 4
   0: dp3 r0.x, v5.xyzx, v5.xyzx
   1: sqrt r0.x, r0.x
   2: div r0.x, l(1.000000, 1.000000, 1.000000, 1.000000), r0.x
   3: mul r0.xyz, r0.xxxx, v5.xyzx
   4: dp3 r0.w, r0.xyzx, r0.xyzx
   5: rsq r0.w, r0.w
   6: mul r0.xyz, r0.wwww, r0.xyzx
   7: add r1.xyz, -v4.xyzx, cb0[21].xyzx
   8: dp3 r0.w, r1.xyzx, r1.xyzx
   9: rsq r0.w, r0.w
  10: mul r1.xyz, r0.wwww, r1.xyzx
  11: eq r0.w, cb0[25].w, l(0.000000)
  12: movc r2.x, r0.w, r1.x, cb0[66].z
  13: movc r2.y, r0.w, r1.y, cb0[67].z
  14: movc r2.z, r0.w, r1.z, cb0[68].z
  15: dp3_sat r0.x, r0.xyzx, r2.xyzx
  16: add r0.x, -r0.x, l(1.000000)
  17: log r0.x, r0.x
  18: mul r0.x, r0.x, cb1[0].y
  19: exp r0.x, r0.x
  20: mul_sat r0.x, r0.x, cb1[0].z
  21: and r0.x, r0.x, v6.x
  22: lt r0.y, cb0[22].x, l(0.000000)
  23: add r0.z, -v0.y, cb0[3].y
  24: movc r1.y, r0.y, r0.z, v0.y
  25: mov r1.x, v0.x
  26: div r1.xy, r1.xyxx, cb0[3].xyxx
  27: add r1.z, -r1.y, l(1.000000)
  28: mad r0.yz, -cb0[130].xxyx, l(0.000000, 0.500000, 0.500000, 0.000000), l(0.000000, 1.000000, 1.000000, 0.000000)
  29: min r0.yz, r0.yyzy, r1.xxzx
  30: mul r0.yz, r0.yyzy, cb0[28].xxyx
  31: sample_b r1.xyzw, r0.yzyy, t1.xyzw, s0, cb0[4].x
  32: mad r0.y, cb0[24].x, r1.x, cb0[24].y
  33: div r0.y, l(1.000000, 1.000000, 1.000000, 1.000000), r0.y
  34: mul r0.z, v4.y, cb0[79].w
  35: mad r0.z, cb0[78].w, v4.x, r0.z
  36: mad r0.z, cb0[80].w, v4.z, r0.z
  37: add r0.z, r0.z, cb0[81].w
  38: mad r0.y, r0.y, cb0[22].z, -r0.z
  39: div_sat r0.y, r0.y, cb1[14].w
  40: ge r0.z, l(0.000000), r0.y
  41: and r0.z, r0.z, l(0x3f800000)
  42: add r0.w, -r0.y, l(1.000000)
  43: mad r0.y, r0.z, r0.w, r0.y
  44: add r0.x, -r0.y, r0.x
  45: add r0.x, r0.x, l(1.000000)
  46: mul r0.y, v1.w, l(1.000100)
  47: ge r0.z, r0.y, -r0.y
  48: frc r0.y, |r0.y|
  49: movc r0.y, r0.z, r0.y, -r0.y
  50: mul r0.y, r0.y, l(0.999900)
  51: div r0.zw, cb1[10].zzzw, cb1[5].xxxy
  52: mad r0.yz, cb0[19].xxxx, r0.zzwz, r0.yyyy
  53: add r0.yz, r0.yyzy, v1.xxyx
  54: mad r0.yz, r0.yyzy, cb1[5].xxyx, cb1[5].zzwz
  55: sample_b r1.xyzw, r0.yzyy, t3.xyzw, s3, cb0[4].x
  56: div r0.yz, cb1[11].xxyx, cb1[7].xxyx
  57: mad r0.yz, r0.yyzy, cb0[19].xxxx, v1.xxyx
  58: mad r0.yz, r0.yyzy, cb1[7].xxyx, cb1[7].zzwz
  59: sample_b r2.xyzw, r0.yzyy, t4.xyzw, s4, cb0[4].x
  60: mad r0.yz, v1.xxyx, cb1[9].xxyx, cb1[9].zzwz
  61: sample_b r3.xyzw, r0.yzyy, t5.xyzw, s5, cb0[4].x
  62: mul r0.yz, r2.xxyx, r3.xxyx
  63: mul r0.yz, r0.yyzy, cb1[11].zzzz
  64: div r0.yz, r0.yyzy, cb1[3].xxyx
  65: div r2.xy, cb1[10].xyxx, cb1[3].xyxx
  66: mad r2.xy, r2.xyxx, cb0[19].xxxx, v1.xyxx
  67: add r0.yz, -r0.yyzy, r2.xxyx
  68: mad r0.yz, r0.yyzy, cb1[3].xxyx, cb1[3].zzwz
  69: sample_b r2.xyzw, r0.yzyy, t2.xyzw, s2, cb0[4].x
  70: mul r1.xyzw, r1.wxyz, r2.wxyz
  71: ne r0.yz, l(0.000000, 0.000000, 0.000000, 0.000000), cb1[1].xxyx
  72: movc r2.xyz, r0.yyyy, r1.xxxx, r1.yzwy
  73: max r0.xyw, r0.xxxx, r2.xyxz
  74: movc r0.xyz, r0.zzzz, r0.xywx, r1.yzwy
  75: add r0.w, -v1.z, l(1.000000)
  76: add_sat r1.xyz, -r0.wwww, r3.xyzx
  77: mul_sat r1.xyz, r1.xyzx, r3.xyzx
  78: mul r1.xyz, r0.xyzx, r1.xyzx
  79: ne r0.w, l(0.000000, 0.000000, 0.000000, 0.000000), cb1[14].y
  80: movc r0.xyz, r0.wwww, r1.xyzx, r0.xyzx
  81: mul r0.xyz, r0.xyzx, cb1[12].xxxx
  82: mul r0.xyz, r0.xyzx, v3.xyzx
  83: max r0.w, v2.x, l(1.000000)
  84: min r0.w, r0.w, l(1000000.000000)
  85: eq r1.x, v1.x, v2.x
  86: movc r0.w, r1.x, l(1.000000), r0.w
  87: mul r0.xyz, r0.xyzx, r0.wwww
  88: div r1.xy, l(1.000000, 1.000000, 1.000000, 1.000000), cb0[3].xyxx
  89: mul r1.xy, r1.xyxx, v0.xyxx
  90: mad r0.w, r1.y, cb0[27].x, cb0[27].y
  91: add r1.z, -r0.w, l(1.000000)
  92: sample_b r1.xyzw, r1.xzxx, t0.xyzw, s1, cb0[4].x
  93: add r0.w, -cb0[9].x, l(1.000000)
  94: add_sat r0.w, r0.w, r1.x
  95: add r0.w, r0.w, l(-1.000000)
  96: mad r0.w, cb0[9].w, r0.w, l(1.000000)
  97: mul o0.xyz, r0.wwww, r0.xyzx
  98: mov o0.w, l(1.000000)
  99: ret 
// Approximately 0 instruction slots used


//////////////////////////////////////////////////////
Keywords: LIGHTMAP_BICUBIC_SAMPLING _SCREEN_SPACE_OCCLUSION _SURFACE_TYPE_TRANSPARENT
-- Vertex shader for "d3d11":
// No shader variant for this keyword set. The closest match will be used instead.

-- Hardware tier variant: Tier 1
-- Fragment shader for "d3d11":
// Stats: 76 math, 4 temp registers
Set 2D Texture "_CameraDepthTexture" to slot 0 sampler slot -1
Set 2D Texture "_MainTex" to slot 1
Set 2D Texture "_Noise" to slot 2
Set 2D Texture "_Flow" to slot 3
Set 2D Texture "_Mask" to slot 4

Set Sampler PointClamp to slot 0

Constant Buffer "$Globals" (2096 bytes) on slot 0 {
  Matrix4x4 unity_MatrixV at 1056
  Matrix4x4 unity_MatrixVP at 1248
  Vector4 _ScaledScreenParams at 48
  Vector2 _GlobalMipBias at 64
  Vector4 _TimeParameters at 304
  Vector3 _WorldSpaceCameraPos at 336
  Vector4 _ProjectionParams at 352
  Vector4 _ZBufferParams at 384
  Vector4 unity_OrthoParams at 400
  Vector4 _RTHandleScale at 448
  Vector4 _CameraDepthTexture_TexelSize at 2080
}
Constant Buffer "UnityPerMaterial" (304 bytes) on slot 1 {
  Float _Textureopacity at 0
  Float _Fresnelpower at 4
  Float _Fresnelscale at 8
  Float _Multiply_texture at 12
  Float _Useonlycolor at 16
  Float _Use_fresnel at 20
  Vector4 _MainTex_ST at 48
  Vector4 _Noise_ST at 80
  Vector4 _Flow_ST at 112
  Vector4 _Mask_ST at 144
  Vector4 _SpeedMainTexUVNoiseZW at 160
  Vector4 _DistortionSpeedXYPowerZ at 176
  Float _Emission at 192
  Float _Opacity at 224
  Float _Usecenterglow at 228
  Float _Usedepth at 232
  Float _Depthpower at 236
}

Shader Disassembly:
//
// Generated by Microsoft (R) D3D Shader Disassembler
//
//
// Input signature:
//
// Name                 Index   Mask Register SysValue  Format   Used
// -------------------- ----- ------ -------- -------- ------- ------
// SV_POSITION              0   xyzw        0      POS   float   xy  
// INTERP                   0   xyzw        1     NONE   float   xyzw
// INTERP                   1   xyzw        2     NONE   float   x   
// INTERP                   2   xyzw        3     NONE   float   xyzw
// INTERP                   3   xyz         4     NONE   float   xyz 
// INTERP                   4   xyz         5     NONE   float   xyz 
// SV_IsFrontFace           0   x           6    FFACE    uint   x   
//
//
// Output signature:
//
// Name                 Index   Mask Register SysValue  Format   Used
// -------------------- ----- ------ -------- -------- ------- ------
// SV_Target                0   xyzw        0   TARGET   float   xyzw
//
      ps_4_0
      dcl_constantbuffer CB0[131], immediateIndexed
      dcl_constantbuffer CB1[15], immediateIndexed
      dcl_sampler s0, mode_default
      dcl_sampler s1, mode_default
      dcl_sampler s2, mode_default
      dcl_sampler s3, mode_default
      dcl_sampler s4, mode_default
      dcl_resource_texture2d (float,float,float,float) t0
      dcl_resource_texture2d (float,float,float,float) t1
      dcl_resource_texture2d (float,float,float,float) t2
      dcl_resource_texture2d (float,float,float,float) t3
      dcl_resource_texture2d (float,float,float,float) t4
      dcl_input_ps_siv linear noperspective v0.xy, position
      dcl_input_ps linear v1.xyzw
      dcl_input_ps linear v2.x
      dcl_input_ps linear v3.xyzw
      dcl_input_ps linear v4.xyz
      dcl_input_ps linear v5.xyz
      dcl_input_ps_sgv constant v6.x, is_front_face
      dcl_output o0.xyzw
      dcl_temps 4
   0: dp3 r0.x, v5.xyzx, v5.xyzx
   1: sqrt r0.x, r0.x
   2: div r0.x, l(1.000000, 1.000000, 1.000000, 1.000000), r0.x
   3: mul r0.xyz, r0.xxxx, v5.xyzx
   4: dp3 r0.w, r0.xyzx, r0.xyzx
   5: rsq r0.w, r0.w
   6: mul r0.xyz, r0.wwww, r0.xyzx
   7: add r1.xyz, -v4.xyzx, cb0[21].xyzx
   8: dp3 r0.w, r1.xyzx, r1.xyzx
   9: rsq r0.w, r0.w
  10: mul r1.xyz, r0.wwww, r1.xyzx
  11: eq r0.w, cb0[25].w, l(0.000000)
  12: movc r2.x, r0.w, r1.x, cb0[66].z
  13: movc r2.y, r0.w, r1.y, cb0[67].z
  14: movc r2.z, r0.w, r1.z, cb0[68].z
  15: dp3_sat r0.x, r0.xyzx, r2.xyzx
  16: add r0.x, -r0.x, l(1.000000)
  17: log r0.x, r0.x
  18: mul r0.x, r0.x, cb1[0].y
  19: exp r0.x, r0.x
  20: mul_sat r0.x, r0.x, cb1[0].z
  21: and r0.x, r0.x, v6.x
  22: lt r0.y, cb0[22].x, l(0.000000)
  23: add r0.z, -v0.y, cb0[3].y
  24: movc r1.y, r0.y, r0.z, v0.y
  25: mov r1.x, v0.x
  26: div r1.xy, r1.xyxx, cb0[3].xyxx
  27: add r1.z, -r1.y, l(1.000000)
  28: mad r0.yz, -cb0[130].xxyx, l(0.000000, 0.500000, 0.500000, 0.000000), l(0.000000, 1.000000, 1.000000, 0.000000)
  29: min r0.yz, r0.yyzy, r1.xxzx
  30: mul r0.yz, r0.yyzy, cb0[28].xxyx
  31: sample_b r1.xyzw, r0.yzyy, t0.xyzw, s0, cb0[4].x
  32: mad r0.y, cb0[24].x, r1.x, cb0[24].y
  33: div r0.y, l(1.000000, 1.000000, 1.000000, 1.000000), r0.y
  34: mul r0.z, v4.y, cb0[79].w
  35: mad r0.z, cb0[78].w, v4.x, r0.z
  36: mad r0.z, cb0[80].w, v4.z, r0.z
  37: add r0.z, r0.z, cb0[81].w
  38: mad r0.y, r0.y, cb0[22].z, -r0.z
  39: div_sat r0.y, r0.y, cb1[14].w
  40: ge r0.z, l(0.000000), r0.y
  41: and r0.z, r0.z, l(0x3f800000)
  42: add r0.w, -r0.y, l(1.000000)
  43: mad r0.y, r0.z, r0.w, r0.y
  44: add r0.x, -r0.y, r0.x
  45: add r0.x, r0.x, l(1.000000)
  46: mul r0.z, v1.w, l(1.000100)
  47: ge r0.w, r0.z, -r0.z
  48: frc r0.z, |r0.z|
  49: movc r0.z, r0.w, r0.z, -r0.z
  50: mul r0.z, r0.z, l(0.999900)
  51: div r1.xy, cb1[10].zwzz, cb1[5].xyxx
  52: mad r0.zw, cb0[19].xxxx, r1.xxxy, r0.zzzz
  53: add r0.zw, r0.zzzw, v1.xxxy
  54: mad r0.zw, r0.zzzw, cb1[5].xxxy, cb1[5].zzzw
  55: sample_b r1.xyzw, r0.zwzz, t2.xyzw, s2, cb0[4].x
  56: div r0.zw, cb1[11].xxxy, cb1[7].xxxy
  57: mad r0.zw, r0.zzzw, cb0[19].xxxx, v1.xxxy
  58: mad r0.zw, r0.zzzw, cb1[7].xxxy, cb1[7].zzzw
  59: sample_b r2.xyzw, r0.zwzz, t3.xyzw, s3, cb0[4].x
  60: mad r0.zw, v1.xxxy, cb1[9].xxxy, cb1[9].zzzw
  61: sample_b r3.xyzw, r0.zwzz, t4.xyzw, s4, cb0[4].x
  62: mul r0.zw, r2.xxxy, r3.xxxy
  63: mul r0.zw, r0.zzzw, cb1[11].zzzz
  64: div r0.zw, r0.zzzw, cb1[3].xxxy
  65: div r2.xy, cb1[10].xyxx, cb1[3].xyxx
  66: mad r2.xy, r2.xyxx, cb0[19].xxxx, v1.xyxx
  67: add r0.zw, -r0.zzzw, r2.xxxy
  68: mad r0.zw, r0.zzzw, cb1[3].xxxy, cb1[3].zzzw
  69: sample_b r2.xyzw, r0.zwzz, t1.xyzw, s1, cb0[4].x
  70: mul r1.xyzw, r1.wxyz, r2.wxyz
  71: ne r0.zw, l(0.000000, 0.000000, 0.000000, 0.000000), cb1[1].xxxy
  72: movc r2.xyz, r0.zzzz, r1.xxxx, r1.yzwy
  73: max r2.xyz, r0.xxxx, r2.xyzx
  74: mad_sat r0.x, r1.x, cb1[0].x, r0.x
  75: movc r1.yzw, r0.wwww, r2.xxyz, r1.yyzw
  76: add r0.z, -v1.z, l(1.000000)
  77: add_sat r2.xyz, -r0.zzzz, r3.xyzx
  78: mul_sat r2.xyz, r2.xyzx, r3.xyzx
  79: mul r2.xyz, r1.yzwy, r2.xyzx
  80: ne r3.xy, l(0.000000, 0.000000, 0.000000, 0.000000), cb1[14].yzyy
  81: movc r1.yzw, r3.xxxx, r2.xxyz, r1.yyzw
  82: mul r1.yzw, r1.yyzw, cb1[12].xxxx
  83: mul r1.yzw, r1.yyzw, v3.xxyz
  84: max r0.z, v2.x, l(1.000000)
  85: min r0.z, r0.z, l(1000000.000000)
  86: eq r2.x, v1.x, v2.x
  87: movc r0.z, r2.x, l(1.000000), r0.z
  88: mul o0.xyz, r1.yzwy, r0.zzzz
  89: mul r0.z, r0.x, r1.x
  90: ne r1.y, l(0.000000, 0.000000, 0.000000, 0.000000), cb1[0].w
  91: movc r0.x, r1.y, r0.z, r0.x
  92: movc r0.x, r0.w, r0.x, r1.x
  93: mul r0.y, r0.y, r0.x
  94: movc r0.x, r3.y, r0.y, r0.x
  95: mul r0.x, r0.x, v3.w
  96: mul_sat o0.w, r0.x, cb1[14].x
  97: ret 
// Approximately 0 instruction slots used


//////////////////////////////////////////////////////
Keywords: FOG_EXP2 LIGHTMAP_BICUBIC_SAMPLING _SCREEN_SPACE_OCCLUSION
-- Vertex shader for "d3d11":
// No shader variant for this keyword set. The closest match will be used instead.

-- Hardware tier variant: Tier 1
-- Fragment shader for "d3d11":
// Stats: 80 math, 4 temp registers
Set 2D Texture "_ScreenSpaceOcclusionTexture" to slot 0 sampler slot -1
Set 2D Texture "_CameraDepthTexture" to slot 1 sampler slot -1
Set 2D Texture "_MainTex" to slot 2
Set 2D Texture "_Noise" to slot 3
Set 2D Texture "_Flow" to slot 4
Set 2D Texture "_Mask" to slot 5

Set Sampler PointClamp to slot 0
Set Sampler LinearClamp to slot 1

Constant Buffer "$Globals" (2096 bytes) on slot 0 {
  Matrix4x4 unity_MatrixV at 1056
  Matrix4x4 unity_MatrixVP at 1248
  Vector4 _ScaledScreenParams at 48
  Vector2 _GlobalMipBias at 64
  Vector4 _AmbientOcclusionParam at 144
  Vector4 _TimeParameters at 304
  Vector3 _WorldSpaceCameraPos at 336
  Vector4 _ProjectionParams at 352
  Vector4 _ZBufferParams at 384
  Vector4 unity_OrthoParams at 400
  Vector4 _ScaleBiasRt at 432
  Vector4 _RTHandleScale at 448
  Vector4 _CameraDepthTexture_TexelSize at 2080
}
Constant Buffer "UnityPerMaterial" (304 bytes) on slot 1 {
  Float _Fresnelpower at 4
  Float _Fresnelscale at 8
  Float _Useonlycolor at 16
  Float _Use_fresnel at 20
  Vector4 _MainTex_ST at 48
  Vector4 _Noise_ST at 80
  Vector4 _Flow_ST at 112
  Vector4 _Mask_ST at 144
  Vector4 _SpeedMainTexUVNoiseZW at 160
  Vector4 _DistortionSpeedXYPowerZ at 176
  Float _Emission at 192
  Float _Usecenterglow at 228
  Float _Depthpower at 236
}

Shader Disassembly:
//
// Generated by Microsoft (R) D3D Shader Disassembler
//
//
// Input signature:
//
// Name                 Index   Mask Register SysValue  Format   Used
// -------------------- ----- ------ -------- -------- ------- ------
// SV_POSITION              0   xyzw        0      POS   float   xy  
// INTERP                   0   xyzw        1     NONE   float   xyzw
// INTERP                   1   xyzw        2     NONE   float   x   
// INTERP                   2   xyzw        3     NONE   float   xyz 
// INTERP                   3   xyz         4     NONE   float   xyz 
// INTERP                   4   xyz         5     NONE   float   xyz 
// SV_IsFrontFace           0   x           6    FFACE    uint   x   
//
//
// Output signature:
//
// Name                 Index   Mask Register SysValue  Format   Used
// -------------------- ----- ------ -------- -------- ------- ------
// SV_Target                0   xyzw        0   TARGET   float   xyzw
//
      ps_4_0
      dcl_constantbuffer CB0[131], immediateIndexed
      dcl_constantbuffer CB1[15], immediateIndexed
      dcl_sampler s0, mode_default
      dcl_sampler s1, mode_default
      dcl_sampler s2, mode_default
      dcl_sampler s3, mode_default
      dcl_sampler s4, mode_default
      dcl_sampler s5, mode_default
      dcl_resource_texture2d (float,float,float,float) t0
      dcl_resource_texture2d (float,float,float,float) t1
      dcl_resource_texture2d (float,float,float,float) t2
      dcl_resource_texture2d (float,float,float,float) t3
      dcl_resource_texture2d (float,float,float,float) t4
      dcl_resource_texture2d (float,float,float,float) t5
      dcl_input_ps_siv linear noperspective v0.xy, position
      dcl_input_ps linear v1.xyzw
      dcl_input_ps linear v2.x
      dcl_input_ps linear v3.xyz
      dcl_input_ps linear v4.xyz
      dcl_input_ps linear v5.xyz
      dcl_input_ps_sgv constant v6.x, is_front_face
      dcl_output o0.xyzw
      dcl_temps 4
   0: dp3 r0.x, v5.xyzx, v5.xyzx
   1: sqrt r0.x, r0.x
   2: div r0.x, l(1.000000, 1.000000, 1.000000, 1.000000), r0.x
   3: mul r0.xyz, r0.xxxx, v5.xyzx
   4: dp3 r0.w, r0.xyzx, r0.xyzx
   5: rsq r0.w, r0.w
   6: mul r0.xyz, r0.wwww, r0.xyzx
   7: add r1.xyz, -v4.xyzx, cb0[21].xyzx
   8: dp3 r0.w, r1.xyzx, r1.xyzx
   9: rsq r0.w, r0.w
  10: mul r1.xyz, r0.wwww, r1.xyzx
  11: eq r0.w, cb0[25].w, l(0.000000)
  12: movc r2.x, r0.w, r1.x, cb0[66].z
  13: movc r2.y, r0.w, r1.y, cb0[67].z
  14: movc r2.z, r0.w, r1.z, cb0[68].z
  15: dp3_sat r0.x, r0.xyzx, r2.xyzx
  16: add r0.x, -r0.x, l(1.000000)
  17: log r0.x, r0.x
  18: mul r0.x, r0.x, cb1[0].y
  19: exp r0.x, r0.x
  20: mul_sat r0.x, r0.x, cb1[0].z
  21: and r0.x, r0.x, v6.x
  22: lt r0.y, cb0[22].x, l(0.000000)
  23: add r0.z, -v0.y, cb0[3].y
  24: movc r1.y, r0.y, r0.z, v0.y
  25: mov r1.x, v0.x
  26: div r1.xy, r1.xyxx, cb0[3].xyxx
  27: add r1.z, -r1.y, l(1.000000)
  28: mad r0.yz, -cb0[130].xxyx, l(0.000000, 0.500000, 0.500000, 0.000000), l(0.000000, 1.000000, 1.000000, 0.000000)
  29: min r0.yz, r0.yyzy, r1.xxzx
  30: mul r0.yz, r0.yyzy, cb0[28].xxyx
  31: sample_b r1.xyzw, r0.yzyy, t1.xyzw, s0, cb0[4].x
  32: mad r0.y, cb0[24].x, r1.x, cb0[24].y
  33: div r0.y, l(1.000000, 1.000000, 1.000000, 1.000000), r0.y
  34: mul r0.z, v4.y, cb0[79].w
  35: mad r0.z, cb0[78].w, v4.x, r0.z
  36: mad r0.z, cb0[80].w, v4.z, r0.z
  37: add r0.z, r0.z, cb0[81].w
  38: mad r0.y, r0.y, cb0[22].z, -r0.z
  39: div_sat r0.y, r0.y, cb1[14].w
  40: ge r0.z, l(0.000000), r0.y
  41: and r0.z, r0.z, l(0x3f800000)
  42: add r0.w, -r0.y, l(1.000000)
  43: mad r0.y, r0.z, r0.w, r0.y
  44: add r0.x, -r0.y, r0.x
  45: add r0.x, r0.x, l(1.000000)
  46: mul r0.y, v1.w, l(1.000100)
  47: ge r0.z, r0.y, -r0.y
  48: frc r0.y, |r0.y|
  49: movc r0.y, r0.z, r0.y, -r0.y
  50: mul r0.y, r0.y, l(0.999900)
  51: div r0.zw, cb1[10].zzzw, cb1[5].xxxy
  52: mad r0.yz, cb0[19].xxxx, r0.zzwz, r0.yyyy
  53: add r0.yz, r0.yyzy, v1.xxyx
  54: mad r0.yz, r0.yyzy, cb1[5].xxyx, cb1[5].zzwz
  55: sample_b r1.xyzw, r0.yzyy, t3.xyzw, s3, cb0[4].x
  56: div r0.yz, cb1[11].xxyx, cb1[7].xxyx
  57: mad r0.yz, r0.yyzy, cb0[19].xxxx, v1.xxyx
  58: mad r0.yz, r0.yyzy, cb1[7].xxyx, cb1[7].zzwz
  59: sample_b r2.xyzw, r0.yzyy, t4.xyzw, s4, cb0[4].x
  60: mad r0.yz, v1.xxyx, cb1[9].xxyx, cb1[9].zzwz
  61: sample_b r3.xyzw, r0.yzyy, t5.xyzw, s5, cb0[4].x
  62: mul r0.yz, r2.xxyx, r3.xxyx
  63: mul r0.yz, r0.yyzy, cb1[11].zzzz
  64: div r0.yz, r0.yyzy, cb1[3].xxyx
  65: div r2.xy, cb1[10].xyxx, cb1[3].xyxx
  66: mad r2.xy, r2.xyxx, cb0[19].xxxx, v1.xyxx
  67: add r0.yz, -r0.yyzy, r2.xxyx
  68: mad r0.yz, r0.yyzy, cb1[3].xxyx, cb1[3].zzwz
  69: sample_b r2.xyzw, r0.yzyy, t2.xyzw, s2, cb0[4].x
  70: mul r1.xyzw, r1.wxyz, r2.wxyz
  71: ne r0.yz, l(0.000000, 0.000000, 0.000000, 0.000000), cb1[1].xxyx
  72: movc r2.xyz, r0.yyyy, r1.xxxx, r1.yzwy
  73: max r0.xyw, r0.xxxx, r2.xyxz
  74: movc r0.xyz, r0.zzzz, r0.xywx, r1.yzwy
  75: add r0.w, -v1.z, l(1.000000)
  76: add_sat r1.xyz, -r0.wwww, r3.xyzx
  77: mul_sat r1.xyz, r1.xyzx, r3.xyzx
  78: mul r1.xyz, r0.xyzx, r1.xyzx
  79: ne r0.w, l(0.000000, 0.000000, 0.000000, 0.000000), cb1[14].y
  80: movc r0.xyz, r0.wwww, r1.xyzx, r0.xyzx
  81: mul r0.xyz, r0.xyzx, cb1[12].xxxx
  82: mul r0.xyz, r0.xyzx, v3.xyzx
  83: max r0.w, v2.x, l(1.000000)
  84: min r0.w, r0.w, l(1000000.000000)
  85: eq r1.x, v1.x, v2.x
  86: movc r0.w, r1.x, l(1.000000), r0.w
  87: mul r0.xyz, r0.xyzx, r0.wwww
  88: div r1.xy, l(1.000000, 1.000000, 1.000000, 1.000000), cb0[3].xyxx
  89: mul r1.xy, r1.xyxx, v0.xyxx
  90: mad r0.w, r1.y, cb0[27].x, cb0[27].y
  91: add r1.z, -r0.w, l(1.000000)
  92: sample_b r1.xyzw, r1.xzxx, t0.xyzw, s1, cb0[4].x
  93: add r0.w, -cb0[9].x, l(1.000000)
  94: add_sat r0.w, r0.w, r1.x
  95: add r0.w, r0.w, l(-1.000000)
  96: mad r0.w, cb0[9].w, r0.w, l(1.000000)
  97: mul o0.xyz, r0.wwww, r0.xyzx
  98: mov o0.w, l(1.000000)
  99: ret 
// Approximately 0 instruction slots used


//////////////////////////////////////////////////////
Keywords: FOG_EXP2 LIGHTMAP_BICUBIC_SAMPLING _SCREEN_SPACE_OCCLUSION _SURFACE_TYPE_TRANSPARENT
-- Vertex shader for "d3d11":
// No shader variant for this keyword set. The closest match will be used instead.

-- Hardware tier variant: Tier 1
-- Fragment shader for "d3d11":
// Stats: 76 math, 4 temp registers
Set 2D Texture "_CameraDepthTexture" to slot 0 sampler slot -1
Set 2D Texture "_MainTex" to slot 1
Set 2D Texture "_Noise" to slot 2
Set 2D Texture "_Flow" to slot 3
Set 2D Texture "_Mask" to slot 4

Set Sampler PointClamp to slot 0

Constant Buffer "$Globals" (2096 bytes) on slot 0 {
  Matrix4x4 unity_MatrixV at 1056
  Matrix4x4 unity_MatrixVP at 1248
  Vector4 _ScaledScreenParams at 48
  Vector2 _GlobalMipBias at 64
  Vector4 _TimeParameters at 304
  Vector3 _WorldSpaceCameraPos at 336
  Vector4 _ProjectionParams at 352
  Vector4 _ZBufferParams at 384
  Vector4 unity_OrthoParams at 400
  Vector4 _RTHandleScale at 448
  Vector4 _CameraDepthTexture_TexelSize at 2080
}
Constant Buffer "UnityPerMaterial" (304 bytes) on slot 1 {
  Float _Textureopacity at 0
  Float _Fresnelpower at 4
  Float _Fresnelscale at 8
  Float _Multiply_texture at 12
  Float _Useonlycolor at 16
  Float _Use_fresnel at 20
  Vector4 _MainTex_ST at 48
  Vector4 _Noise_ST at 80
  Vector4 _Flow_ST at 112
  Vector4 _Mask_ST at 144
  Vector4 _SpeedMainTexUVNoiseZW at 160
  Vector4 _DistortionSpeedXYPowerZ at 176
  Float _Emission at 192
  Float _Opacity at 224
  Float _Usecenterglow at 228
  Float _Usedepth at 232
  Float _Depthpower at 236
}

Shader Disassembly:
//
// Generated by Microsoft (R) D3D Shader Disassembler
//
//
// Input signature:
//
// Name                 Index   Mask Register SysValue  Format   Used
// -------------------- ----- ------ -------- -------- ------- ------
// SV_POSITION              0   xyzw        0      POS   float   xy  
// INTERP                   0   xyzw        1     NONE   float   xyzw
// INTERP                   1   xyzw        2     NONE   float   x   
// INTERP                   2   xyzw        3     NONE   float   xyzw
// INTERP                   3   xyz         4     NONE   float   xyz 
// INTERP                   4   xyz         5     NONE   float   xyz 
// SV_IsFrontFace           0   x           6    FFACE    uint   x   
//
//
// Output signature:
//
// Name                 Index   Mask Register SysValue  Format   Used
// -------------------- ----- ------ -------- -------- ------- ------
// SV_Target                0   xyzw        0   TARGET   float   xyzw
//
      ps_4_0
      dcl_constantbuffer CB0[131], immediateIndexed
      dcl_constantbuffer CB1[15], immediateIndexed
      dcl_sampler s0, mode_default
      dcl_sampler s1, mode_default
      dcl_sampler s2, mode_default
      dcl_sampler s3, mode_default
      dcl_sampler s4, mode_default
      dcl_resource_texture2d (float,float,float,float) t0
      dcl_resource_texture2d (float,float,float,float) t1
      dcl_resource_texture2d (float,float,float,float) t2
      dcl_resource_texture2d (float,float,float,float) t3
      dcl_resource_texture2d (float,float,float,float) t4
      dcl_input_ps_siv linear noperspective v0.xy, position
      dcl_input_ps linear v1.xyzw
      dcl_input_ps linear v2.x
      dcl_input_ps linear v3.xyzw
      dcl_input_ps linear v4.xyz
      dcl_input_ps linear v5.xyz
      dcl_input_ps_sgv constant v6.x, is_front_face
      dcl_output o0.xyzw
      dcl_temps 4
   0: dp3 r0.x, v5.xyzx, v5.xyzx
   1: sqrt r0.x, r0.x
   2: div r0.x, l(1.000000, 1.000000, 1.000000, 1.000000), r0.x
   3: mul r0.xyz, r0.xxxx, v5.xyzx
   4: dp3 r0.w, r0.xyzx, r0.xyzx
   5: rsq r0.w, r0.w
   6: mul r0.xyz, r0.wwww, r0.xyzx
   7: add r1.xyz, -v4.xyzx, cb0[21].xyzx
   8: dp3 r0.w, r1.xyzx, r1.xyzx
   9: rsq r0.w, r0.w
  10: mul r1.xyz, r0.wwww, r1.xyzx
  11: eq r0.w, cb0[25].w, l(0.000000)
  12: movc r2.x, r0.w, r1.x, cb0[66].z
  13: movc r2.y, r0.w, r1.y, cb0[67].z
  14: movc r2.z, r0.w, r1.z, cb0[68].z
  15: dp3_sat r0.x, r0.xyzx, r2.xyzx
  16: add r0.x, -r0.x, l(1.000000)
  17: log r0.x, r0.x
  18: mul r0.x, r0.x, cb1[0].y
  19: exp r0.x, r0.x
  20: mul_sat r0.x, r0.x, cb1[0].z
  21: and r0.x, r0.x, v6.x
  22: lt r0.y, cb0[22].x, l(0.000000)
  23: add r0.z, -v0.y, cb0[3].y
  24: movc r1.y, r0.y, r0.z, v0.y
  25: mov r1.x, v0.x
  26: div r1.xy, r1.xyxx, cb0[3].xyxx
  27: add r1.z, -r1.y, l(1.000000)
  28: mad r0.yz, -cb0[130].xxyx, l(0.000000, 0.500000, 0.500000, 0.000000), l(0.000000, 1.000000, 1.000000, 0.000000)
  29: min r0.yz, r0.yyzy, r1.xxzx
  30: mul r0.yz, r0.yyzy, cb0[28].xxyx
  31: sample_b r1.xyzw, r0.yzyy, t0.xyzw, s0, cb0[4].x
  32: mad r0.y, cb0[24].x, r1.x, cb0[24].y
  33: div r0.y, l(1.000000, 1.000000, 1.000000, 1.000000), r0.y
  34: mul r0.z, v4.y, cb0[79].w
  35: mad r0.z, cb0[78].w, v4.x, r0.z
  36: mad r0.z, cb0[80].w, v4.z, r0.z
  37: add r0.z, r0.z, cb0[81].w
  38: mad r0.y, r0.y, cb0[22].z, -r0.z
  39: div_sat r0.y, r0.y, cb1[14].w
  40: ge r0.z, l(0.000000), r0.y
  41: and r0.z, r0.z, l(0x3f800000)
  42: add r0.w, -r0.y, l(1.000000)
  43: mad r0.y, r0.z, r0.w, r0.y
  44: add r0.x, -r0.y, r0.x
  45: add r0.x, r0.x, l(1.000000)
  46: mul r0.z, v1.w, l(1.000100)
  47: ge r0.w, r0.z, -r0.z
  48: frc r0.z, |r0.z|
  49: movc r0.z, r0.w, r0.z, -r0.z
  50: mul r0.z, r0.z, l(0.999900)
  51: div r1.xy, cb1[10].zwzz, cb1[5].xyxx
  52: mad r0.zw, cb0[19].xxxx, r1.xxxy, r0.zzzz
  53: add r0.zw, r0.zzzw, v1.xxxy
  54: mad r0.zw, r0.zzzw, cb1[5].xxxy, cb1[5].zzzw
  55: sample_b r1.xyzw, r0.zwzz, t2.xyzw, s2, cb0[4].x
  56: div r0.zw, cb1[11].xxxy, cb1[7].xxxy
  57: mad r0.zw, r0.zzzw, cb0[19].xxxx, v1.xxxy
  58: mad r0.zw, r0.zzzw, cb1[7].xxxy, cb1[7].zzzw
  59: sample_b r2.xyzw, r0.zwzz, t3.xyzw, s3, cb0[4].x
  60: mad r0.zw, v1.xxxy, cb1[9].xxxy, cb1[9].zzzw
  61: sample_b r3.xyzw, r0.zwzz, t4.xyzw, s4, cb0[4].x
  62: mul r0.zw, r2.xxxy, r3.xxxy
  63: mul r0.zw, r0.zzzw, cb1[11].zzzz
  64: div r0.zw, r0.zzzw, cb1[3].xxxy
  65: div r2.xy, cb1[10].xyxx, cb1[3].xyxx
  66: mad r2.xy, r2.xyxx, cb0[19].xxxx, v1.xyxx
  67: add r0.zw, -r0.zzzw, r2.xxxy
  68: mad r0.zw, r0.zzzw, cb1[3].xxxy, cb1[3].zzzw
  69: sample_b r2.xyzw, r0.zwzz, t1.xyzw, s1, cb0[4].x
  70: mul r1.xyzw, r1.wxyz, r2.wxyz
  71: ne r0.zw, l(0.000000, 0.000000, 0.000000, 0.000000), cb1[1].xxxy
  72: movc r2.xyz, r0.zzzz, r1.xxxx, r1.yzwy
  73: max r2.xyz, r0.xxxx, r2.xyzx
  74: mad_sat r0.x, r1.x, cb1[0].x, r0.x
  75: movc r1.yzw, r0.wwww, r2.xxyz, r1.yyzw
  76: add r0.z, -v1.z, l(1.000000)
  77: add_sat r2.xyz, -r0.zzzz, r3.xyzx
  78: mul_sat r2.xyz, r2.xyzx, r3.xyzx
  79: mul r2.xyz, r1.yzwy, r2.xyzx
  80: ne r3.xy, l(0.000000, 0.000000, 0.000000, 0.000000), cb1[14].yzyy
  81: movc r1.yzw, r3.xxxx, r2.xxyz, r1.yyzw
  82: mul r1.yzw, r1.yyzw, cb1[12].xxxx
  83: mul r1.yzw, r1.yyzw, v3.xxyz
  84: max r0.z, v2.x, l(1.000000)
  85: min r0.z, r0.z, l(1000000.000000)
  86: eq r2.x, v1.x, v2.x
  87: movc r0.z, r2.x, l(1.000000), r0.z
  88: mul o0.xyz, r1.yzwy, r0.zzzz
  89: mul r0.z, r0.x, r1.x
  90: ne r1.y, l(0.000000, 0.000000, 0.000000, 0.000000), cb1[0].w
  91: movc r0.x, r1.y, r0.z, r0.x
  92: movc r0.x, r0.w, r0.x, r1.x
  93: mul r0.y, r0.y, r0.x
  94: movc r0.x, r3.y, r0.y, r0.x
  95: mul r0.x, r0.x, v3.w
  96: mul_sat o0.w, r0.x, cb1[14].x
  97: ret 
// Approximately 0 instruction slots used


//////////////////////////////////////////////////////
Keywords: LIGHTMAP_BICUBIC_SAMPLING _SCREEN_SPACE_OCCLUSION _WRITE_RENDERING_LAYERS
-- Vertex shader for "d3d11":
// No shader variant for this keyword set. The closest match will be used instead.

-- Hardware tier variant: Tier 1
-- Fragment shader for "d3d11":
// Stats: 80 math, 4 temp registers
Set 2D Texture "_ScreenSpaceOcclusionTexture" to slot 0 sampler slot -1
Set 2D Texture "_CameraDepthTexture" to slot 1 sampler slot -1
Set 2D Texture "_MainTex" to slot 2
Set 2D Texture "_Noise" to slot 3
Set 2D Texture "_Flow" to slot 4
Set 2D Texture "_Mask" to slot 5

Set Sampler PointClamp to slot 0
Set Sampler LinearClamp to slot 1

Constant Buffer "$Globals" (2096 bytes) on slot 0 {
  Matrix4x4 unity_MatrixV at 1056
  Matrix4x4 unity_MatrixVP at 1248
  Vector4 _ScaledScreenParams at 48
  Vector2 _GlobalMipBias at 64
  Vector4 _AmbientOcclusionParam at 144
  ScalarInt _RenderingLayerMaxInt at 176
  Vector4 _TimeParameters at 304
  Vector3 _WorldSpaceCameraPos at 336
  Vector4 _ProjectionParams at 352
  Vector4 _ZBufferParams at 384
  Vector4 unity_OrthoParams at 400
  Vector4 _ScaleBiasRt at 432
  Vector4 _RTHandleScale at 448
  Vector4 _CameraDepthTexture_TexelSize at 2080
}
Constant Buffer "UnityPerDraw" (720 bytes) on slot 1 {
  Vector4 unity_RenderingLayer at 160
}
Constant Buffer "UnityPerMaterial" (304 bytes) on slot 2 {
  Float _Fresnelpower at 4
  Float _Fresnelscale at 8
  Float _Useonlycolor at 16
  Float _Use_fresnel at 20
  Vector4 _MainTex_ST at 48
  Vector4 _Noise_ST at 80
  Vector4 _Flow_ST at 112
  Vector4 _Mask_ST at 144
  Vector4 _SpeedMainTexUVNoiseZW at 160
  Vector4 _DistortionSpeedXYPowerZ at 176
  Float _Emission at 192
  Float _Usecenterglow at 228
  Float _Depthpower at 236
}

Shader Disassembly:
//
// Generated by Microsoft (R) D3D Shader Disassembler
//
//
// Input signature:
//
// Name                 Index   Mask Register SysValue  Format   Used
// -------------------- ----- ------ -------- -------- ------- ------
// SV_POSITION              0   xyzw        0      POS   float   xy  
// INTERP                   0   xyzw        1     NONE   float   xyzw
// INTERP                   1   xyzw        2     NONE   float   x   
// INTERP                   2   xyzw        3     NONE   float   xyz 
// INTERP                   3   xyz         4     NONE   float   xyz 
// INTERP                   4   xyz         5     NONE   float   xyz 
// SV_IsFrontFace           0   x           6    FFACE    uint   x   
//
//
// Output signature:
//
// Name                 Index   Mask Register SysValue  Format   Used
// -------------------- ----- ------ -------- -------- ------- ------
// SV_Target                0   xyzw        0   TARGET   float   xyzw
// SV_Target                1   x           1   TARGET    uint   x   
//
      ps_5_0
      dcl_globalFlags refactoringAllowed
      dcl_constantbuffer CB0[131], immediateIndexed
      dcl_constantbuffer CB1[11], immediateIndexed
      dcl_constantbuffer CB2[15], immediateIndexed
      dcl_sampler s0, mode_default
      dcl_sampler s1, mode_default
      dcl_sampler s2, mode_default
      dcl_sampler s3, mode_default
      dcl_sampler s4, mode_default
      dcl_sampler s5, mode_default
      dcl_resource_texture2d (float,float,float,float) t0
      dcl_resource_texture2d (float,float,float,float) t1
      dcl_resource_texture2d (float,float,float,float) t2
      dcl_resource_texture2d (float,float,float,float) t3
      dcl_resource_texture2d (float,float,float,float) t4
      dcl_resource_texture2d (float,float,float,float) t5
      dcl_input_ps_siv linear noperspective v0.xy, position
      dcl_input_ps linear v1.xyzw
      dcl_input_ps linear v2.x
      dcl_input_ps linear v3.xyz
      dcl_input_ps linear v4.xyz
      dcl_input_ps linear v5.xyz
      dcl_input_ps_sgv constant v6.x, is_front_face
      dcl_output o0.xyzw
      dcl_output o1.x
      dcl_temps 4
   0: dp3 r0.x, v5.xyzx, v5.xyzx
   1: sqrt r0.x, r0.x
   2: div r0.x, l(1.000000, 1.000000, 1.000000, 1.000000), r0.x
   3: mul r0.xyz, r0.xxxx, v5.xyzx
   4: dp3 r0.w, r0.xyzx, r0.xyzx
   5: rsq r0.w, r0.w
   6: mul r0.xyz, r0.wwww, r0.xyzx
   7: add r1.xyz, -v4.xyzx, cb0[21].xyzx
   8: dp3 r0.w, r1.xyzx, r1.xyzx
   9: rsq r0.w, r0.w
  10: mul r1.xyz, r0.wwww, r1.xyzx
  11: eq r0.w, cb0[25].w, l(0.000000)
  12: movc r2.x, r0.w, r1.x, cb0[66].z
  13: movc r2.y, r0.w, r1.y, cb0[67].z
  14: movc r2.z, r0.w, r1.z, cb0[68].z
  15: dp3_sat r0.x, r0.xyzx, r2.xyzx
  16: add r0.x, -r0.x, l(1.000000)
  17: log r0.x, r0.x
  18: mul r0.x, r0.x, cb2[0].y
  19: exp r0.x, r0.x
  20: mul_sat r0.x, r0.x, cb2[0].z
  21: and r0.x, r0.x, v6.x
  22: lt r0.y, cb0[22].x, l(0.000000)
  23: add r0.z, -v0.y, cb0[3].y
  24: movc r1.y, r0.y, r0.z, v0.y
  25: mov r1.x, v0.x
  26: div r1.xy, r1.xyxx, cb0[3].xyxx
  27: add r1.z, -r1.y, l(1.000000)
  28: mad r0.yz, -cb0[130].xxyx, l(0.000000, 0.500000, 0.500000, 0.000000), l(0.000000, 1.000000, 1.000000, 0.000000)
  29: min r0.yz, r0.yyzy, r1.xxzx
  30: mul r0.yz, r0.yyzy, cb0[28].xxyx
  31: sample_b_indexable(texture2d)(float,float,float,float) r0.y, r0.yzyy, t1.yxzw, s0, cb0[4].x
  32: mad r0.y, cb0[24].x, r0.y, cb0[24].y
  33: div r0.y, l(1.000000, 1.000000, 1.000000, 1.000000), r0.y
  34: mul r0.z, v4.y, cb0[79].w
  35: mad r0.z, cb0[78].w, v4.x, r0.z
  36: mad r0.z, cb0[80].w, v4.z, r0.z
  37: add r0.z, r0.z, cb0[81].w
  38: mad r0.y, r0.y, cb0[22].z, -r0.z
  39: div_sat r0.y, r0.y, cb2[14].w
  40: ge r0.z, l(0.000000), r0.y
  41: and r0.z, r0.z, l(0x3f800000)
  42: add r0.w, -r0.y, l(1.000000)
  43: mad r0.y, r0.z, r0.w, r0.y
  44: add r0.x, -r0.y, r0.x
  45: add r0.x, r0.x, l(1.000000)
  46: mul r0.y, v1.w, l(1.000100)
  47: ge r0.z, r0.y, -r0.y
  48: frc r0.y, |r0.y|
  49: movc r0.y, r0.z, r0.y, -r0.y
  50: mul r0.y, r0.y, l(0.999900)
  51: div r0.zw, cb2[10].zzzw, cb2[5].xxxy
  52: mad r0.yz, cb0[19].xxxx, r0.zzwz, r0.yyyy
  53: add r0.yz, r0.yyzy, v1.xxyx
  54: mad r0.yz, r0.yyzy, cb2[5].xxyx, cb2[5].zzwz
  55: sample_b_indexable(texture2d)(float,float,float,float) r1.xyzw, r0.yzyy, t3.xyzw, s3, cb0[4].x
  56: div r0.yz, cb2[11].xxyx, cb2[7].xxyx
  57: mad r0.yz, r0.yyzy, cb0[19].xxxx, v1.xxyx
  58: mad r0.yz, r0.yyzy, cb2[7].xxyx, cb2[7].zzwz
  59: sample_b_indexable(texture2d)(float,float,float,float) r0.yz, r0.yzyy, t4.zxyw, s4, cb0[4].x
  60: mad r2.xy, v1.xyxx, cb2[9].xyxx, cb2[9].zwzz
  61: sample_b_indexable(texture2d)(float,float,float,float) r2.xyz, r2.xyxx, t5.xyzw, s5, cb0[4].x
  62: mul r0.yz, r0.yyzy, r2.xxyx
  63: mul r0.yz, r0.yyzy, cb2[11].zzzz
  64: div r0.yz, r0.yyzy, cb2[3].xxyx
  65: div r3.xy, cb2[10].xyxx, cb2[3].xyxx
  66: mad r3.xy, r3.xyxx, cb0[19].xxxx, v1.xyxx
  67: add r0.yz, -r0.yyzy, r3.xxyx
  68: mad r0.yz, r0.yyzy, cb2[3].xxyx, cb2[3].zzwz
  69: sample_b_indexable(texture2d)(float,float,float,float) r3.xyzw, r0.yzyy, t2.xyzw, s2, cb0[4].x
  70: mul r1.xyzw, r1.wxyz, r3.wxyz
  71: ne r0.yz, l(0.000000, 0.000000, 0.000000, 0.000000), cb2[1].xxyx
  72: movc r3.xyz, r0.yyyy, r1.xxxx, r1.yzwy
  73: max r0.xyw, r0.xxxx, r3.xyxz
  74: movc r0.xyz, r0.zzzz, r0.xywx, r1.yzwy
  75: add r0.w, -v1.z, l(1.000000)
  76: add_sat r1.xyz, -r0.wwww, r2.xyzx
  77: mul_sat r1.xyz, r1.xyzx, r2.xyzx
  78: mul r1.xyz, r0.xyzx, r1.xyzx
  79: ne r0.w, l(0.000000, 0.000000, 0.000000, 0.000000), cb2[14].y
  80: movc r0.xyz, r0.wwww, r1.xyzx, r0.xyzx
  81: mul r0.xyz, r0.xyzx, cb2[12].xxxx
  82: mul r0.xyz, r0.xyzx, v3.xyzx
  83: max r0.w, v2.x, l(1.000000)
  84: min r0.w, r0.w, l(1000000.000000)
  85: eq r1.x, v1.x, v2.x
  86: movc r0.w, r1.x, l(1.000000), r0.w
  87: mul r0.xyz, r0.xyzx, r0.wwww
  88: rcp r1.xy, cb0[3].xyxx
  89: mul r1.xy, r1.xyxx, v0.xyxx
  90: mad r0.w, r1.y, cb0[27].x, cb0[27].y
  91: add r1.z, -r0.w, l(1.000000)
  92: sample_b_indexable(texture2d)(float,float,float,float) r0.w, r1.xzxx, t0.yzwx, s1, cb0[4].x
  93: add r1.x, -cb0[9].x, l(1.000000)
  94: add_sat r0.w, r0.w, r1.x
  95: add r0.w, r0.w, l(-1.000000)
  96: mad r0.w, cb0[9].w, r0.w, l(1.000000)
  97: mul o0.xyz, r0.wwww, r0.xyzx
  98: mov o0.w, l(1.000000)
  99: and o1.x, cb0[11].x, cb1[10].x
 100: ret 
// Approximately 0 instruction slots used


//////////////////////////////////////////////////////
Keywords: LIGHTMAP_BICUBIC_SAMPLING _SCREEN_SPACE_OCCLUSION _SURFACE_TYPE_TRANSPARENT _WRITE_RENDERING_LAYERS
-- Vertex shader for "d3d11":
// No shader variant for this keyword set. The closest match will be used instead.

-- Hardware tier variant: Tier 1
-- Fragment shader for "d3d11":
// Stats: 77 math, 4 temp registers
Set 2D Texture "_CameraDepthTexture" to slot 0 sampler slot -1
Set 2D Texture "_MainTex" to slot 1
Set 2D Texture "_Noise" to slot 2
Set 2D Texture "_Flow" to slot 3
Set 2D Texture "_Mask" to slot 4

Set Sampler PointClamp to slot 0

Constant Buffer "$Globals" (2096 bytes) on slot 0 {
  Matrix4x4 unity_MatrixV at 1056
  Matrix4x4 unity_MatrixVP at 1248
  Vector4 _ScaledScreenParams at 48
  Vector2 _GlobalMipBias at 64
  ScalarInt _RenderingLayerMaxInt at 176
  Vector4 _TimeParameters at 304
  Vector3 _WorldSpaceCameraPos at 336
  Vector4 _ProjectionParams at 352
  Vector4 _ZBufferParams at 384
  Vector4 unity_OrthoParams at 400
  Vector4 _RTHandleScale at 448
  Vector4 _CameraDepthTexture_TexelSize at 2080
}
Constant Buffer "UnityPerDraw" (720 bytes) on slot 1 {
  Vector4 unity_RenderingLayer at 160
}
Constant Buffer "UnityPerMaterial" (304 bytes) on slot 2 {
  Float _Textureopacity at 0
  Float _Fresnelpower at 4
  Float _Fresnelscale at 8
  Float _Multiply_texture at 12
  Float _Useonlycolor at 16
  Float _Use_fresnel at 20
  Vector4 _MainTex_ST at 48
  Vector4 _Noise_ST at 80
  Vector4 _Flow_ST at 112
  Vector4 _Mask_ST at 144
  Vector4 _SpeedMainTexUVNoiseZW at 160
  Vector4 _DistortionSpeedXYPowerZ at 176
  Float _Emission at 192
  Float _Opacity at 224
  Float _Usecenterglow at 228
  Float _Usedepth at 232
  Float _Depthpower at 236
}

Shader Disassembly:
//
// Generated by Microsoft (R) D3D Shader Disassembler
//
//
// Input signature:
//
// Name                 Index   Mask Register SysValue  Format   Used
// -------------------- ----- ------ -------- -------- ------- ------
// SV_POSITION              0   xyzw        0      POS   float   xy  
// INTERP                   0   xyzw        1     NONE   float   xyzw
// INTERP                   1   xyzw        2     NONE   float   x   
// INTERP                   2   xyzw        3     NONE   float   xyzw
// INTERP                   3   xyz         4     NONE   float   xyz 
// INTERP                   4   xyz         5     NONE   float   xyz 
// SV_IsFrontFace           0   x           6    FFACE    uint   x   
//
//
// Output signature:
//
// Name                 Index   Mask Register SysValue  Format   Used
// -------------------- ----- ------ -------- -------- ------- ------
// SV_Target                0   xyzw        0   TARGET   float   xyzw
// SV_Target                1   x           1   TARGET    uint   x   
//
      ps_5_0
      dcl_globalFlags refactoringAllowed
      dcl_constantbuffer CB0[131], immediateIndexed
      dcl_constantbuffer CB1[11], immediateIndexed
      dcl_constantbuffer CB2[15], immediateIndexed
      dcl_sampler s0, mode_default
      dcl_sampler s1, mode_default
      dcl_sampler s2, mode_default
      dcl_sampler s3, mode_default
      dcl_sampler s4, mode_default
      dcl_resource_texture2d (float,float,float,float) t0
      dcl_resource_texture2d (float,float,float,float) t1
      dcl_resource_texture2d (float,float,float,float) t2
      dcl_resource_texture2d (float,float,float,float) t3
      dcl_resource_texture2d (float,float,float,float) t4
      dcl_input_ps_siv linear noperspective v0.xy, position
      dcl_input_ps linear v1.xyzw
      dcl_input_ps linear v2.x
      dcl_input_ps linear v3.xyzw
      dcl_input_ps linear v4.xyz
      dcl_input_ps linear v5.xyz
      dcl_input_ps_sgv constant v6.x, is_front_face
      dcl_output o0.xyzw
      dcl_output o1.x
      dcl_temps 4
   0: dp3 r0.x, v5.xyzx, v5.xyzx
   1: sqrt r0.x, r0.x
   2: div r0.x, l(1.000000, 1.000000, 1.000000, 1.000000), r0.x
   3: mul r0.xyz, r0.xxxx, v5.xyzx
   4: dp3 r0.w, r0.xyzx, r0.xyzx
   5: rsq r0.w, r0.w
   6: mul r0.xyz, r0.wwww, r0.xyzx
   7: add r1.xyz, -v4.xyzx, cb0[21].xyzx
   8: dp3 r0.w, r1.xyzx, r1.xyzx
   9: rsq r0.w, r0.w
  10: mul r1.xyz, r0.wwww, r1.xyzx
  11: eq r0.w, cb0[25].w, l(0.000000)
  12: movc r2.x, r0.w, r1.x, cb0[66].z
  13: movc r2.y, r0.w, r1.y, cb0[67].z
  14: movc r2.z, r0.w, r1.z, cb0[68].z
  15: dp3_sat r0.x, r0.xyzx, r2.xyzx
  16: add r0.x, -r0.x, l(1.000000)
  17: log r0.x, r0.x
  18: mul r0.x, r0.x, cb2[0].y
  19: exp r0.x, r0.x
  20: mul_sat r0.x, r0.x, cb2[0].z
  21: and r0.x, r0.x, v6.x
  22: lt r0.y, cb0[22].x, l(0.000000)
  23: add r0.z, -v0.y, cb0[3].y
  24: movc r1.y, r0.y, r0.z, v0.y
  25: mov r1.x, v0.x
  26: div r1.xy, r1.xyxx, cb0[3].xyxx
  27: add r1.z, -r1.y, l(1.000000)
  28: mad r0.yz, -cb0[130].xxyx, l(0.000000, 0.500000, 0.500000, 0.000000), l(0.000000, 1.000000, 1.000000, 0.000000)
  29: min r0.yz, r0.yyzy, r1.xxzx
  30: mul r0.yz, r0.yyzy, cb0[28].xxyx
  31: sample_b_indexable(texture2d)(float,float,float,float) r0.y, r0.yzyy, t0.yxzw, s0, cb0[4].x
  32: mad r0.y, cb0[24].x, r0.y, cb0[24].y
  33: div r0.y, l(1.000000, 1.000000, 1.000000, 1.000000), r0.y
  34: mul r0.z, v4.y, cb0[79].w
  35: mad r0.z, cb0[78].w, v4.x, r0.z
  36: mad r0.z, cb0[80].w, v4.z, r0.z
  37: add r0.z, r0.z, cb0[81].w
  38: mad r0.y, r0.y, cb0[22].z, -r0.z
  39: div_sat r0.y, r0.y, cb2[14].w
  40: ge r0.z, l(0.000000), r0.y
  41: and r0.z, r0.z, l(0x3f800000)
  42: add r0.w, -r0.y, l(1.000000)
  43: mad r0.y, r0.z, r0.w, r0.y
  44: add r0.x, -r0.y, r0.x
  45: add r0.x, r0.x, l(1.000000)
  46: mul r0.z, v1.w, l(1.000100)
  47: ge r0.w, r0.z, -r0.z
  48: frc r0.z, |r0.z|
  49: movc r0.z, r0.w, r0.z, -r0.z
  50: mul r0.z, r0.z, l(0.999900)
  51: div r1.xy, cb2[10].zwzz, cb2[5].xyxx
  52: mad r0.zw, cb0[19].xxxx, r1.xxxy, r0.zzzz
  53: add r0.zw, r0.zzzw, v1.xxxy
  54: mad r0.zw, r0.zzzw, cb2[5].xxxy, cb2[5].zzzw
  55: sample_b_indexable(texture2d)(float,float,float,float) r1.xyzw, r0.zwzz, t2.xyzw, s2, cb0[4].x
  56: div r0.zw, cb2[11].xxxy, cb2[7].xxxy
  57: mad r0.zw, r0.zzzw, cb0[19].xxxx, v1.xxxy
  58: mad r0.zw, r0.zzzw, cb2[7].xxxy, cb2[7].zzzw
  59: sample_b_indexable(texture2d)(float,float,float,float) r0.zw, r0.zwzz, t3.zwxy, s3, cb0[4].x
  60: mad r2.xy, v1.xyxx, cb2[9].xyxx, cb2[9].zwzz
  61: sample_b_indexable(texture2d)(float,float,float,float) r2.xyz, r2.xyxx, t4.xyzw, s4, cb0[4].x
  62: mul r0.zw, r0.zzzw, r2.xxxy
  63: mul r0.zw, r0.zzzw, cb2[11].zzzz
  64: div r0.zw, r0.zzzw, cb2[3].xxxy
  65: div r3.xy, cb2[10].xyxx, cb2[3].xyxx
  66: mad r3.xy, r3.xyxx, cb0[19].xxxx, v1.xyxx
  67: add r0.zw, -r0.zzzw, r3.xxxy
  68: mad r0.zw, r0.zzzw, cb2[3].xxxy, cb2[3].zzzw
  69: sample_b_indexable(texture2d)(float,float,float,float) r3.xyzw, r0.zwzz, t1.xyzw, s1, cb0[4].x
  70: mul r1.xyzw, r1.wxyz, r3.wxyz
  71: ne r0.zw, l(0.000000, 0.000000, 0.000000, 0.000000), cb2[1].xxxy
  72: movc r3.xyz, r0.zzzz, r1.xxxx, r1.yzwy
  73: max r3.xyz, r0.xxxx, r3.xyzx
  74: mad_sat r0.x, r1.x, cb2[0].x, r0.x
  75: movc r1.yzw, r0.wwww, r3.xxyz, r1.yyzw
  76: add r0.z, -v1.z, l(1.000000)
  77: add_sat r3.xyz, -r0.zzzz, r2.xyzx
  78: mul_sat r2.xyz, r2.xyzx, r3.xyzx
  79: mul r2.xyz, r1.yzwy, r2.xyzx
  80: ne r3.xy, l(0.000000, 0.000000, 0.000000, 0.000000), cb2[14].yzyy
  81: movc r1.yzw, r3.xxxx, r2.xxyz, r1.yyzw
  82: mul r1.yzw, r1.yyzw, cb2[12].xxxx
  83: mul r1.yzw, r1.yyzw, v3.xxyz
  84: max r0.z, v2.x, l(1.000000)
  85: min r0.z, r0.z, l(1000000.000000)
  86: eq r2.x, v1.x, v2.x
  87: movc r0.z, r2.x, l(1.000000), r0.z
  88: mul o0.xyz, r1.yzwy, r0.zzzz
  89: mul r0.z, r0.x, r1.x
  90: ne r1.y, l(0.000000, 0.000000, 0.000000, 0.000000), cb2[0].w
  91: movc r0.x, r1.y, r0.z, r0.x
  92: movc r0.x, r0.w, r0.x, r1.x
  93: mul r0.y, r0.y, r0.x
  94: movc r0.x, r3.y, r0.y, r0.x
  95: mul r0.x, r0.x, v3.w
  96: mul_sat o0.w, r0.x, cb2[14].x
  97: and o1.x, cb0[11].x, cb1[10].x
  98: ret 
// Approximately 0 instruction slots used


//////////////////////////////////////////////////////
Keywords: FOG_EXP2 LIGHTMAP_BICUBIC_SAMPLING _SCREEN_SPACE_OCCLUSION _WRITE_RENDERING_LAYERS
-- Vertex shader for "d3d11":
// No shader variant for this keyword set. The closest match will be used instead.

-- Hardware tier variant: Tier 1
-- Fragment shader for "d3d11":
// Stats: 80 math, 4 temp registers
Set 2D Texture "_ScreenSpaceOcclusionTexture" to slot 0 sampler slot -1
Set 2D Texture "_CameraDepthTexture" to slot 1 sampler slot -1
Set 2D Texture "_MainTex" to slot 2
Set 2D Texture "_Noise" to slot 3
Set 2D Texture "_Flow" to slot 4
Set 2D Texture "_Mask" to slot 5

Set Sampler PointClamp to slot 0
Set Sampler LinearClamp to slot 1

Constant Buffer "$Globals" (2096 bytes) on slot 0 {
  Matrix4x4 unity_MatrixV at 1056
  Matrix4x4 unity_MatrixVP at 1248
  Vector4 _ScaledScreenParams at 48
  Vector2 _GlobalMipBias at 64
  Vector4 _AmbientOcclusionParam at 144
  ScalarInt _RenderingLayerMaxInt at 176
  Vector4 _TimeParameters at 304
  Vector3 _WorldSpaceCameraPos at 336
  Vector4 _ProjectionParams at 352
  Vector4 _ZBufferParams at 384
  Vector4 unity_OrthoParams at 400
  Vector4 _ScaleBiasRt at 432
  Vector4 _RTHandleScale at 448
  Vector4 _CameraDepthTexture_TexelSize at 2080
}
Constant Buffer "UnityPerDraw" (720 bytes) on slot 1 {
  Vector4 unity_RenderingLayer at 160
}
Constant Buffer "UnityPerMaterial" (304 bytes) on slot 2 {
  Float _Fresnelpower at 4
  Float _Fresnelscale at 8
  Float _Useonlycolor at 16
  Float _Use_fresnel at 20
  Vector4 _MainTex_ST at 48
  Vector4 _Noise_ST at 80
  Vector4 _Flow_ST at 112
  Vector4 _Mask_ST at 144
  Vector4 _SpeedMainTexUVNoiseZW at 160
  Vector4 _DistortionSpeedXYPowerZ at 176
  Float _Emission at 192
  Float _Usecenterglow at 228
  Float _Depthpower at 236
}

Shader Disassembly:
//
// Generated by Microsoft (R) D3D Shader Disassembler
//
//
// Input signature:
//
// Name                 Index   Mask Register SysValue  Format   Used
// -------------------- ----- ------ -------- -------- ------- ------
// SV_POSITION              0   xyzw        0      POS   float   xy  
// INTERP                   0   xyzw        1     NONE   float   xyzw
// INTERP                   1   xyzw        2     NONE   float   x   
// INTERP                   2   xyzw        3     NONE   float   xyz 
// INTERP                   3   xyz         4     NONE   float   xyz 
// INTERP                   4   xyz         5     NONE   float   xyz 
// SV_IsFrontFace           0   x           6    FFACE    uint   x   
//
//
// Output signature:
//
// Name                 Index   Mask Register SysValue  Format   Used
// -------------------- ----- ------ -------- -------- ------- ------
// SV_Target                0   xyzw        0   TARGET   float   xyzw
// SV_Target                1   x           1   TARGET    uint   x   
//
      ps_5_0
      dcl_globalFlags refactoringAllowed
      dcl_constantbuffer CB0[131], immediateIndexed
      dcl_constantbuffer CB1[11], immediateIndexed
      dcl_constantbuffer CB2[15], immediateIndexed
      dcl_sampler s0, mode_default
      dcl_sampler s1, mode_default
      dcl_sampler s2, mode_default
      dcl_sampler s3, mode_default
      dcl_sampler s4, mode_default
      dcl_sampler s5, mode_default
      dcl_resource_texture2d (float,float,float,float) t0
      dcl_resource_texture2d (float,float,float,float) t1
      dcl_resource_texture2d (float,float,float,float) t2
      dcl_resource_texture2d (float,float,float,float) t3
      dcl_resource_texture2d (float,float,float,float) t4
      dcl_resource_texture2d (float,float,float,float) t5
      dcl_input_ps_siv linear noperspective v0.xy, position
      dcl_input_ps linear v1.xyzw
      dcl_input_ps linear v2.x
      dcl_input_ps linear v3.xyz
      dcl_input_ps linear v4.xyz
      dcl_input_ps linear v5.xyz
      dcl_input_ps_sgv constant v6.x, is_front_face
      dcl_output o0.xyzw
      dcl_output o1.x
      dcl_temps 4
   0: dp3 r0.x, v5.xyzx, v5.xyzx
   1: sqrt r0.x, r0.x
   2: div r0.x, l(1.000000, 1.000000, 1.000000, 1.000000), r0.x
   3: mul r0.xyz, r0.xxxx, v5.xyzx
   4: dp3 r0.w, r0.xyzx, r0.xyzx
   5: rsq r0.w, r0.w
   6: mul r0.xyz, r0.wwww, r0.xyzx
   7: add r1.xyz, -v4.xyzx, cb0[21].xyzx
   8: dp3 r0.w, r1.xyzx, r1.xyzx
   9: rsq r0.w, r0.w
  10: mul r1.xyz, r0.wwww, r1.xyzx
  11: eq r0.w, cb0[25].w, l(0.000000)
  12: movc r2.x, r0.w, r1.x, cb0[66].z
  13: movc r2.y, r0.w, r1.y, cb0[67].z
  14: movc r2.z, r0.w, r1.z, cb0[68].z
  15: dp3_sat r0.x, r0.xyzx, r2.xyzx
  16: add r0.x, -r0.x, l(1.000000)
  17: log r0.x, r0.x
  18: mul r0.x, r0.x, cb2[0].y
  19: exp r0.x, r0.x
  20: mul_sat r0.x, r0.x, cb2[0].z
  21: and r0.x, r0.x, v6.x
  22: lt r0.y, cb0[22].x, l(0.000000)
  23: add r0.z, -v0.y, cb0[3].y
  24: movc r1.y, r0.y, r0.z, v0.y
  25: mov r1.x, v0.x
  26: div r1.xy, r1.xyxx, cb0[3].xyxx
  27: add r1.z, -r1.y, l(1.000000)
  28: mad r0.yz, -cb0[130].xxyx, l(0.000000, 0.500000, 0.500000, 0.000000), l(0.000000, 1.000000, 1.000000, 0.000000)
  29: min r0.yz, r0.yyzy, r1.xxzx
  30: mul r0.yz, r0.yyzy, cb0[28].xxyx
  31: sample_b_indexable(texture2d)(float,float,float,float) r0.y, r0.yzyy, t1.yxzw, s0, cb0[4].x
  32: mad r0.y, cb0[24].x, r0.y, cb0[24].y
  33: div r0.y, l(1.000000, 1.000000, 1.000000, 1.000000), r0.y
  34: mul r0.z, v4.y, cb0[79].w
  35: mad r0.z, cb0[78].w, v4.x, r0.z
  36: mad r0.z, cb0[80].w, v4.z, r0.z
  37: add r0.z, r0.z, cb0[81].w
  38: mad r0.y, r0.y, cb0[22].z, -r0.z
  39: div_sat r0.y, r0.y, cb2[14].w
  40: ge r0.z, l(0.000000), r0.y
  41: and r0.z, r0.z, l(0x3f800000)
  42: add r0.w, -r0.y, l(1.000000)
  43: mad r0.y, r0.z, r0.w, r0.y
  44: add r0.x, -r0.y, r0.x
  45: add r0.x, r0.x, l(1.000000)
  46: mul r0.y, v1.w, l(1.000100)
  47: ge r0.z, r0.y, -r0.y
  48: frc r0.y, |r0.y|
  49: movc r0.y, r0.z, r0.y, -r0.y
  50: mul r0.y, r0.y, l(0.999900)
  51: div r0.zw, cb2[10].zzzw, cb2[5].xxxy
  52: mad r0.yz, cb0[19].xxxx, r0.zzwz, r0.yyyy
  53: add r0.yz, r0.yyzy, v1.xxyx
  54: mad r0.yz, r0.yyzy, cb2[5].xxyx, cb2[5].zzwz
  55: sample_b_indexable(texture2d)(float,float,float,float) r1.xyzw, r0.yzyy, t3.xyzw, s3, cb0[4].x
  56: div r0.yz, cb2[11].xxyx, cb2[7].xxyx
  57: mad r0.yz, r0.yyzy, cb0[19].xxxx, v1.xxyx
  58: mad r0.yz, r0.yyzy, cb2[7].xxyx, cb2[7].zzwz
  59: sample_b_indexable(texture2d)(float,float,float,float) r0.yz, r0.yzyy, t4.zxyw, s4, cb0[4].x
  60: mad r2.xy, v1.xyxx, cb2[9].xyxx, cb2[9].zwzz
  61: sample_b_indexable(texture2d)(float,float,float,float) r2.xyz, r2.xyxx, t5.xyzw, s5, cb0[4].x
  62: mul r0.yz, r0.yyzy, r2.xxyx
  63: mul r0.yz, r0.yyzy, cb2[11].zzzz
  64: div r0.yz, r0.yyzy, cb2[3].xxyx
  65: div r3.xy, cb2[10].xyxx, cb2[3].xyxx
  66: mad r3.xy, r3.xyxx, cb0[19].xxxx, v1.xyxx
  67: add r0.yz, -r0.yyzy, r3.xxyx
  68: mad r0.yz, r0.yyzy, cb2[3].xxyx, cb2[3].zzwz
  69: sample_b_indexable(texture2d)(float,float,float,float) r3.xyzw, r0.yzyy, t2.xyzw, s2, cb0[4].x
  70: mul r1.xyzw, r1.wxyz, r3.wxyz
  71: ne r0.yz, l(0.000000, 0.000000, 0.000000, 0.000000), cb2[1].xxyx
  72: movc r3.xyz, r0.yyyy, r1.xxxx, r1.yzwy
  73: max r0.xyw, r0.xxxx, r3.xyxz
  74: movc r0.xyz, r0.zzzz, r0.xywx, r1.yzwy
  75: add r0.w, -v1.z, l(1.000000)
  76: add_sat r1.xyz, -r0.wwww, r2.xyzx
  77: mul_sat r1.xyz, r1.xyzx, r2.xyzx
  78: mul r1.xyz, r0.xyzx, r1.xyzx
  79: ne r0.w, l(0.000000, 0.000000, 0.000000, 0.000000), cb2[14].y
  80: movc r0.xyz, r0.wwww, r1.xyzx, r0.xyzx
  81: mul r0.xyz, r0.xyzx, cb2[12].xxxx
  82: mul r0.xyz, r0.xyzx, v3.xyzx
  83: max r0.w, v2.x, l(1.000000)
  84: min r0.w, r0.w, l(1000000.000000)
  85: eq r1.x, v1.x, v2.x
  86: movc r0.w, r1.x, l(1.000000), r0.w
  87: mul r0.xyz, r0.xyzx, r0.wwww
  88: rcp r1.xy, cb0[3].xyxx
  89: mul r1.xy, r1.xyxx, v0.xyxx
  90: mad r0.w, r1.y, cb0[27].x, cb0[27].y
  91: add r1.z, -r0.w, l(1.000000)
  92: sample_b_indexable(texture2d)(float,float,float,float) r0.w, r1.xzxx, t0.yzwx, s1, cb0[4].x
  93: add r1.x, -cb0[9].x, l(1.000000)
  94: add_sat r0.w, r0.w, r1.x
  95: add r0.w, r0.w, l(-1.000000)
  96: mad r0.w, cb0[9].w, r0.w, l(1.000000)
  97: mul o0.xyz, r0.wwww, r0.xyzx
  98: mov o0.w, l(1.000000)
  99: and o1.x, cb0[11].x, cb1[10].x
 100: ret 
// Approximately 0 instruction slots used


//////////////////////////////////////////////////////
Keywords: FOG_EXP2 LIGHTMAP_BICUBIC_SAMPLING _SCREEN_SPACE_OCCLUSION _SURFACE_TYPE_TRANSPARENT _WRITE_RENDERING_LAYERS
-- Vertex shader for "d3d11":
// No shader variant for this keyword set. The closest match will be used instead.

-- Hardware tier variant: Tier 1
-- Fragment shader for "d3d11":
// Stats: 77 math, 4 temp registers
Set 2D Texture "_CameraDepthTexture" to slot 0 sampler slot -1
Set 2D Texture "_MainTex" to slot 1
Set 2D Texture "_Noise" to slot 2
Set 2D Texture "_Flow" to slot 3
Set 2D Texture "_Mask" to slot 4

Set Sampler PointClamp to slot 0

Constant Buffer "$Globals" (2096 bytes) on slot 0 {
  Matrix4x4 unity_MatrixV at 1056
  Matrix4x4 unity_MatrixVP at 1248
  Vector4 _ScaledScreenParams at 48
  Vector2 _GlobalMipBias at 64
  ScalarInt _RenderingLayerMaxInt at 176
  Vector4 _TimeParameters at 304
  Vector3 _WorldSpaceCameraPos at 336
  Vector4 _ProjectionParams at 352
  Vector4 _ZBufferParams at 384
  Vector4 unity_OrthoParams at 400
  Vector4 _RTHandleScale at 448
  Vector4 _CameraDepthTexture_TexelSize at 2080
}
Constant Buffer "UnityPerDraw" (720 bytes) on slot 1 {
  Vector4 unity_RenderingLayer at 160
}
Constant Buffer "UnityPerMaterial" (304 bytes) on slot 2 {
  Float _Textureopacity at 0
  Float _Fresnelpower at 4
  Float _Fresnelscale at 8
  Float _Multiply_texture at 12
  Float _Useonlycolor at 16
  Float _Use_fresnel at 20
  Vector4 _MainTex_ST at 48
  Vector4 _Noise_ST at 80
  Vector4 _Flow_ST at 112
  Vector4 _Mask_ST at 144
  Vector4 _SpeedMainTexUVNoiseZW at 160
  Vector4 _DistortionSpeedXYPowerZ at 176
  Float _Emission at 192
  Float _Opacity at 224
  Float _Usecenterglow at 228
  Float _Usedepth at 232
  Float _Depthpower at 236
}

Shader Disassembly:
//
// Generated by Microsoft (R) D3D Shader Disassembler
//
//
// Input signature:
//
// Name                 Index   Mask Register SysValue  Format   Used
// -------------------- ----- ------ -------- -------- ------- ------
// SV_POSITION              0   xyzw        0      POS   float   xy  
// INTERP                   0   xyzw        1     NONE   float   xyzw
// INTERP                   1   xyzw        2     NONE   float   x   
// INTERP                   2   xyzw        3     NONE   float   xyzw
// INTERP                   3   xyz         4     NONE   float   xyz 
// INTERP                   4   xyz         5     NONE   float   xyz 
// SV_IsFrontFace           0   x           6    FFACE    uint   x   
//
//
// Output signature:
//
// Name                 Index   Mask Register SysValue  Format   Used
// -------------------- ----- ------ -------- -------- ------- ------
// SV_Target                0   xyzw        0   TARGET   float   xyzw
// SV_Target                1   x           1   TARGET    uint   x   
//
      ps_5_0
      dcl_globalFlags refactoringAllowed
      dcl_constantbuffer CB0[131], immediateIndexed
      dcl_constantbuffer CB1[11], immediateIndexed
      dcl_constantbuffer CB2[15], immediateIndexed
      dcl_sampler s0, mode_default
      dcl_sampler s1, mode_default
      dcl_sampler s2, mode_default
      dcl_sampler s3, mode_default
      dcl_sampler s4, mode_default
      dcl_resource_texture2d (float,float,float,float) t0
      dcl_resource_texture2d (float,float,float,float) t1
      dcl_resource_texture2d (float,float,float,float) t2
      dcl_resource_texture2d (float,float,float,float) t3
      dcl_resource_texture2d (float,float,float,float) t4
      dcl_input_ps_siv linear noperspective v0.xy, position
      dcl_input_ps linear v1.xyzw
      dcl_input_ps linear v2.x
      dcl_input_ps linear v3.xyzw
      dcl_input_ps linear v4.xyz
      dcl_input_ps linear v5.xyz
      dcl_input_ps_sgv constant v6.x, is_front_face
      dcl_output o0.xyzw
      dcl_output o1.x
      dcl_temps 4
   0: dp3 r0.x, v5.xyzx, v5.xyzx
   1: sqrt r0.x, r0.x
   2: div r0.x, l(1.000000, 1.000000, 1.000000, 1.000000), r0.x
   3: mul r0.xyz, r0.xxxx, v5.xyzx
   4: dp3 r0.w, r0.xyzx, r0.xyzx
   5: rsq r0.w, r0.w
   6: mul r0.xyz, r0.wwww, r0.xyzx
   7: add r1.xyz, -v4.xyzx, cb0[21].xyzx
   8: dp3 r0.w, r1.xyzx, r1.xyzx
   9: rsq r0.w, r0.w
  10: mul r1.xyz, r0.wwww, r1.xyzx
  11: eq r0.w, cb0[25].w, l(0.000000)
  12: movc r2.x, r0.w, r1.x, cb0[66].z
  13: movc r2.y, r0.w, r1.y, cb0[67].z
  14: movc r2.z, r0.w, r1.z, cb0[68].z
  15: dp3_sat r0.x, r0.xyzx, r2.xyzx
  16: add r0.x, -r0.x, l(1.000000)
  17: log r0.x, r0.x
  18: mul r0.x, r0.x, cb2[0].y
  19: exp r0.x, r0.x
  20: mul_sat r0.x, r0.x, cb2[0].z
  21: and r0.x, r0.x, v6.x
  22: lt r0.y, cb0[22].x, l(0.000000)
  23: add r0.z, -v0.y, cb0[3].y
  24: movc r1.y, r0.y, r0.z, v0.y
  25: mov r1.x, v0.x
  26: div r1.xy, r1.xyxx, cb0[3].xyxx
  27: add r1.z, -r1.y, l(1.000000)
  28: mad r0.yz, -cb0[130].xxyx, l(0.000000, 0.500000, 0.500000, 0.000000), l(0.000000, 1.000000, 1.000000, 0.000000)
  29: min r0.yz, r0.yyzy, r1.xxzx
  30: mul r0.yz, r0.yyzy, cb0[28].xxyx
  31: sample_b_indexable(texture2d)(float,float,float,float) r0.y, r0.yzyy, t0.yxzw, s0, cb0[4].x
  32: mad r0.y, cb0[24].x, r0.y, cb0[24].y
  33: div r0.y, l(1.000000, 1.000000, 1.000000, 1.000000), r0.y
  34: mul r0.z, v4.y, cb0[79].w
  35: mad r0.z, cb0[78].w, v4.x, r0.z
  36: mad r0.z, cb0[80].w, v4.z, r0.z
  37: add r0.z, r0.z, cb0[81].w
  38: mad r0.y, r0.y, cb0[22].z, -r0.z
  39: div_sat r0.y, r0.y, cb2[14].w
  40: ge r0.z, l(0.000000), r0.y
  41: and r0.z, r0.z, l(0x3f800000)
  42: add r0.w, -r0.y, l(1.000000)
  43: mad r0.y, r0.z, r0.w, r0.y
  44: add r0.x, -r0.y, r0.x
  45: add r0.x, r0.x, l(1.000000)
  46: mul r0.z, v1.w, l(1.000100)
  47: ge r0.w, r0.z, -r0.z
  48: frc r0.z, |r0.z|
  49: movc r0.z, r0.w, r0.z, -r0.z
  50: mul r0.z, r0.z, l(0.999900)
  51: div r1.xy, cb2[10].zwzz, cb2[5].xyxx
  52: mad r0.zw, cb0[19].xxxx, r1.xxxy, r0.zzzz
  53: add r0.zw, r0.zzzw, v1.xxxy
  54: mad r0.zw, r0.zzzw, cb2[5].xxxy, cb2[5].zzzw
  55: sample_b_indexable(texture2d)(float,float,float,float) r1.xyzw, r0.zwzz, t2.xyzw, s2, cb0[4].x
  56: div r0.zw, cb2[11].xxxy, cb2[7].xxxy
  57: mad r0.zw, r0.zzzw, cb0[19].xxxx, v1.xxxy
  58: mad r0.zw, r0.zzzw, cb2[7].xxxy, cb2[7].zzzw
  59: sample_b_indexable(texture2d)(float,float,float,float) r0.zw, r0.zwzz, t3.zwxy, s3, cb0[4].x
  60: mad r2.xy, v1.xyxx, cb2[9].xyxx, cb2[9].zwzz
  61: sample_b_indexable(texture2d)(float,float,float,float) r2.xyz, r2.xyxx, t4.xyzw, s4, cb0[4].x
  62: mul r0.zw, r0.zzzw, r2.xxxy
  63: mul r0.zw, r0.zzzw, cb2[11].zzzz
  64: div r0.zw, r0.zzzw, cb2[3].xxxy
  65: div r3.xy, cb2[10].xyxx, cb2[3].xyxx
  66: mad r3.xy, r3.xyxx, cb0[19].xxxx, v1.xyxx
  67: add r0.zw, -r0.zzzw, r3.xxxy
  68: mad r0.zw, r0.zzzw, cb2[3].xxxy, cb2[3].zzzw
  69: sample_b_indexable(texture2d)(float,float,float,float) r3.xyzw, r0.zwzz, t1.xyzw, s1, cb0[4].x
  70: mul r1.xyzw, r1.wxyz, r3.wxyz
  71: ne r0.zw, l(0.000000, 0.000000, 0.000000, 0.000000), cb2[1].xxxy
  72: movc r3.xyz, r0.zzzz, r1.xxxx, r1.yzwy
  73: max r3.xyz, r0.xxxx, r3.xyzx
  74: mad_sat r0.x, r1.x, cb2[0].x, r0.x
  75: movc r1.yzw, r0.wwww, r3.xxyz, r1.yyzw
  76: add r0.z, -v1.z, l(1.000000)
  77: add_sat r3.xyz, -r0.zzzz, r2.xyzx
  78: mul_sat r2.xyz, r2.xyzx, r3.xyzx
  79: mul r2.xyz, r1.yzwy, r2.xyzx
  80: ne r3.xy, l(0.000000, 0.000000, 0.000000, 0.000000), cb2[14].yzyy
  81: movc r1.yzw, r3.xxxx, r2.xxyz, r1.yyzw
  82: mul r1.yzw, r1.yyzw, cb2[12].xxxx
  83: mul r1.yzw, r1.yyzw, v3.xxyz
  84: max r0.z, v2.x, l(1.000000)
  85: min r0.z, r0.z, l(1000000.000000)
  86: eq r2.x, v1.x, v2.x
  87: movc r0.z, r2.x, l(1.000000), r0.z
  88: mul o0.xyz, r1.yzwy, r0.zzzz
  89: mul r0.z, r0.x, r1.x
  90: ne r1.y, l(0.000000, 0.000000, 0.000000, 0.000000), cb2[0].w
  91: movc r0.x, r1.y, r0.z, r0.x
  92: movc r0.x, r0.w, r0.x, r1.x
  93: mul r0.y, r0.y, r0.x
  94: movc r0.x, r3.y, r0.y, r0.x
  95: mul r0.x, r0.x, v3.w
  96: mul_sat o0.w, r0.x, cb2[14].x
  97: and o1.x, cb0[11].x, cb1[10].x
  98: ret 
// Approximately 0 instruction slots used


//////////////////////////////////////////////////////
Keywords: LIGHTMAP_BICUBIC_SAMPLING _DBUFFER_MRT3
-- Vertex shader for "d3d11":
// No shader variant for this keyword set. The closest match will be used instead.

-- Hardware tier variant: Tier 1
-- Fragment shader for "d3d11":
// Stats: 72 math, 4 temp registers
Set 2D Texture "_DBufferTexture0" to slot 0 sampler slot -1
Set 2D Texture "_CameraDepthTexture" to slot 1 sampler slot -1
Set 2D Texture "_MainTex" to slot 2 sampler slot 1
Set 2D Texture "_Noise" to slot 3 sampler slot 2
Set 2D Texture "_Flow" to slot 4 sampler slot 3
Set 2D Texture "_Mask" to slot 5 sampler slot 4

Set Sampler PointClamp to slot 0

Constant Buffer "$Globals" (2096 bytes) on slot 0 {
  Matrix4x4 unity_MatrixV at 1056
  Matrix4x4 unity_MatrixVP at 1248
  Vector4 _ScaledScreenParams at 48
  Vector2 _GlobalMipBias at 64
  Vector4 _TimeParameters at 304
  Vector3 _WorldSpaceCameraPos at 336
  Vector4 _ProjectionParams at 352
  Vector4 _ZBufferParams at 384
  Vector4 unity_OrthoParams at 400
  Vector4 _RTHandleScale at 448
  Vector4 _CameraDepthTexture_TexelSize at 2080
}
Constant Buffer "UnityPerMaterial" (304 bytes) on slot 1 {
  Float _Fresnelpower at 4
  Float _Fresnelscale at 8
  Float _Useonlycolor at 16
  Float _Use_fresnel at 20
  Vector4 _MainTex_ST at 48
  Vector4 _Noise_ST at 80
  Vector4 _Flow_ST at 112
  Vector4 _Mask_ST at 144
  Vector4 _SpeedMainTexUVNoiseZW at 160
  Vector4 _DistortionSpeedXYPowerZ at 176
  Float _Emission at 192
  Float _Usecenterglow at 228
  Float _Depthpower at 236
}

Shader Disassembly:
//
// Generated by Microsoft (R) D3D Shader Disassembler
//
//
// Input signature:
//
// Name                 Index   Mask Register SysValue  Format   Used
// -------------------- ----- ------ -------- -------- ------- ------
// SV_POSITION              0   xyzw        0      POS   float   xy  
// INTERP                   0   xyzw        1     NONE   float   xyzw
// INTERP                   1   xyzw        2     NONE   float   x   
// INTERP                   2   xyzw        3     NONE   float   xyz 
// INTERP                   3   xyz         4     NONE   float   xyz 
// INTERP                   4   xyz         5     NONE   float   xyz 
// SV_IsFrontFace           0   x           6    FFACE    uint   x   
//
//
// Output signature:
//
// Name                 Index   Mask Register SysValue  Format   Used
// -------------------- ----- ------ -------- -------- ------- ------
// SV_Target                0   xyzw        0   TARGET   float   xyzw
//
      ps_4_0
      dcl_constantbuffer CB0[131], immediateIndexed
      dcl_constantbuffer CB1[15], immediateIndexed
      dcl_sampler s0, mode_default
      dcl_sampler s1, mode_default
      dcl_sampler s2, mode_default
      dcl_sampler s3, mode_default
      dcl_sampler s4, mode_default
      dcl_resource_texture2d (float,float,float,float) t0
      dcl_resource_texture2d (float,float,float,float) t1
      dcl_resource_texture2d (float,float,float,float) t2
      dcl_resource_texture2d (float,float,float,float) t3
      dcl_resource_texture2d (float,float,float,float) t4
      dcl_resource_texture2d (float,float,float,float) t5
      dcl_input_ps_siv linear noperspective v0.xy, position
      dcl_input_ps linear v1.xyzw
      dcl_input_ps linear v2.x
      dcl_input_ps linear v3.xyz
      dcl_input_ps linear v4.xyz
      dcl_input_ps linear v5.xyz
      dcl_input_ps_sgv constant v6.x, is_front_face
      dcl_output o0.xyzw
      dcl_temps 4
   0: dp3 r0.x, v5.xyzx, v5.xyzx
   1: sqrt r0.x, r0.x
   2: div r0.x, l(1.000000, 1.000000, 1.000000, 1.000000), r0.x
   3: mul r0.xyz, r0.xxxx, v5.xyzx
   4: dp3 r0.w, r0.xyzx, r0.xyzx
   5: rsq r0.w, r0.w
   6: mul r0.xyz, r0.wwww, r0.xyzx
   7: add r1.xyz, -v4.xyzx, cb0[21].xyzx
   8: dp3 r0.w, r1.xyzx, r1.xyzx
   9: rsq r0.w, r0.w
  10: mul r1.xyz, r0.wwww, r1.xyzx
  11: eq r0.w, cb0[25].w, l(0.000000)
  12: movc r2.x, r0.w, r1.x, cb0[66].z
  13: movc r2.y, r0.w, r1.y, cb0[67].z
  14: movc r2.z, r0.w, r1.z, cb0[68].z
  15: dp3_sat r0.x, r0.xyzx, r2.xyzx
  16: add r0.x, -r0.x, l(1.000000)
  17: log r0.x, r0.x
  18: mul r0.x, r0.x, cb1[0].y
  19: exp r0.x, r0.x
  20: mul_sat r0.x, r0.x, cb1[0].z
  21: and r0.x, r0.x, v6.x
  22: lt r0.y, cb0[22].x, l(0.000000)
  23: add r0.z, -v0.y, cb0[3].y
  24: movc r1.y, r0.y, r0.z, v0.y
  25: mov r1.x, v0.x
  26: div r1.xy, r1.xyxx, cb0[3].xyxx
  27: add r1.z, -r1.y, l(1.000000)
  28: mad r0.yz, -cb0[130].xxyx, l(0.000000, 0.500000, 0.500000, 0.000000), l(0.000000, 1.000000, 1.000000, 0.000000)
  29: min r0.yz, r0.yyzy, r1.xxzx
  30: mul r0.yz, r0.yyzy, cb0[28].xxyx
  31: sample_b r1.xyzw, r0.yzyy, t1.xyzw, s0, cb0[4].x
  32: mad r0.y, cb0[24].x, r1.x, cb0[24].y
  33: div r0.y, l(1.000000, 1.000000, 1.000000, 1.000000), r0.y
  34: mul r0.z, v4.y, cb0[79].w
  35: mad r0.z, cb0[78].w, v4.x, r0.z
  36: mad r0.z, cb0[80].w, v4.z, r0.z
  37: add r0.z, r0.z, cb0[81].w
  38: mad r0.y, r0.y, cb0[22].z, -r0.z
  39: div_sat r0.y, r0.y, cb1[14].w
  40: ge r0.z, l(0.000000), r0.y
  41: and r0.z, r0.z, l(0x3f800000)
  42: add r0.w, -r0.y, l(1.000000)
  43: mad r0.y, r0.z, r0.w, r0.y
  44: add r0.x, -r0.y, r0.x
  45: add r0.x, r0.x, l(1.000000)
  46: mul r0.y, v1.w, l(1.000100)
  47: ge r0.z, r0.y, -r0.y
  48: frc r0.y, |r0.y|
  49: movc r0.y, r0.z, r0.y, -r0.y
  50: mul r0.y, r0.y, l(0.999900)
  51: div r0.zw, cb1[10].zzzw, cb1[5].xxxy
  52: mad r0.yz, cb0[19].xxxx, r0.zzwz, r0.yyyy
  53: add r0.yz, r0.yyzy, v1.xxyx
  54: mad r0.yz, r0.yyzy, cb1[5].xxyx, cb1[5].zzwz
  55: sample_b r1.xyzw, r0.yzyy, t3.xyzw, s2, cb0[4].x
  56: div r0.yz, cb1[11].xxyx, cb1[7].xxyx
  57: mad r0.yz, r0.yyzy, cb0[19].xxxx, v1.xxyx
  58: mad r0.yz, r0.yyzy, cb1[7].xxyx, cb1[7].zzwz
  59: sample_b r2.xyzw, r0.yzyy, t4.xyzw, s3, cb0[4].x
  60: mad r0.yz, v1.xxyx, cb1[9].xxyx, cb1[9].zzwz
  61: sample_b r3.xyzw, r0.yzyy, t5.xyzw, s4, cb0[4].x
  62: mul r0.yz, r2.xxyx, r3.xxyx
  63: mul r0.yz, r0.yyzy, cb1[11].zzzz
  64: div r0.yz, r0.yyzy, cb1[3].xxyx
  65: div r2.xy, cb1[10].xyxx, cb1[3].xyxx
  66: mad r2.xy, r2.xyxx, cb0[19].xxxx, v1.xyxx
  67: add r0.yz, -r0.yyzy, r2.xxyx
  68: mad r0.yz, r0.yyzy, cb1[3].xxyx, cb1[3].zzwz
  69: sample_b r2.xyzw, r0.yzyy, t2.xyzw, s1, cb0[4].x
  70: mul r1.xyzw, r1.wxyz, r2.wxyz
  71: ne r0.yz, l(0.000000, 0.000000, 0.000000, 0.000000), cb1[1].xxyx
  72: movc r2.xyz, r0.yyyy, r1.xxxx, r1.yzwy
  73: max r0.xyw, r0.xxxx, r2.xyxz
  74: movc r0.xyz, r0.zzzz, r0.xywx, r1.yzwy
  75: add r0.w, -v1.z, l(1.000000)
  76: add_sat r1.xyz, -r0.wwww, r3.xyzx
  77: mul_sat r1.xyz, r1.xyzx, r3.xyzx
  78: mul r1.xyz, r0.xyzx, r1.xyzx
  79: ne r0.w, l(0.000000, 0.000000, 0.000000, 0.000000), cb1[14].y
  80: movc r0.xyz, r0.wwww, r1.xyzx, r0.xyzx
  81: mul r0.xyz, r0.xyzx, cb1[12].xxxx
  82: mul r0.xyz, r0.xyzx, v3.xyzx
  83: max r0.w, v2.x, l(1.000000)
  84: min r0.w, r0.w, l(1000000.000000)
  85: eq r1.x, v1.x, v2.x
  86: movc r0.w, r1.x, l(1.000000), r0.w
  87: mul r0.xyz, r0.xyzx, r0.wwww
  88: ftoi r1.xy, v0.xyxx
  89: mov r1.zw, l(0,0,0,0)
  90: ld r1.xyzw, r1.xyzw, t0.xyzw
  91: mad o0.xyz, r0.xyzx, r1.wwww, r1.xyzx
  92: mov o0.w, l(1.000000)
  93: ret 
// Approximately 0 instruction slots used


//////////////////////////////////////////////////////
Keywords: LIGHTMAP_BICUBIC_SAMPLING _DBUFFER_MRT3 _SURFACE_TYPE_TRANSPARENT
-- Vertex shader for "d3d11":
// No shader variant for this keyword set. The closest match will be used instead.

-- Hardware tier variant: Tier 1
-- Fragment shader for "d3d11":
// Stats: 76 math, 4 temp registers
Set 2D Texture "_CameraDepthTexture" to slot 0 sampler slot -1
Set 2D Texture "_MainTex" to slot 1
Set 2D Texture "_Noise" to slot 2
Set 2D Texture "_Flow" to slot 3
Set 2D Texture "_Mask" to slot 4

Set Sampler PointClamp to slot 0

Constant Buffer "$Globals" (2096 bytes) on slot 0 {
  Matrix4x4 unity_MatrixV at 1056
  Matrix4x4 unity_MatrixVP at 1248
  Vector4 _ScaledScreenParams at 48
  Vector2 _GlobalMipBias at 64
  Vector4 _TimeParameters at 304
  Vector3 _WorldSpaceCameraPos at 336
  Vector4 _ProjectionParams at 352
  Vector4 _ZBufferParams at 384
  Vector4 unity_OrthoParams at 400
  Vector4 _RTHandleScale at 448
  Vector4 _CameraDepthTexture_TexelSize at 2080
}
Constant Buffer "UnityPerMaterial" (304 bytes) on slot 1 {
  Float _Textureopacity at 0
  Float _Fresnelpower at 4
  Float _Fresnelscale at 8
  Float _Multiply_texture at 12
  Float _Useonlycolor at 16
  Float _Use_fresnel at 20
  Vector4 _MainTex_ST at 48
  Vector4 _Noise_ST at 80
  Vector4 _Flow_ST at 112
  Vector4 _Mask_ST at 144
  Vector4 _SpeedMainTexUVNoiseZW at 160
  Vector4 _DistortionSpeedXYPowerZ at 176
  Float _Emission at 192
  Float _Opacity at 224
  Float _Usecenterglow at 228
  Float _Usedepth at 232
  Float _Depthpower at 236
}

Shader Disassembly:
//
// Generated by Microsoft (R) D3D Shader Disassembler
//
//
// Input signature:
//
// Name                 Index   Mask Register SysValue  Format   Used
// -------------------- ----- ------ -------- -------- ------- ------
// SV_POSITION              0   xyzw        0      POS   float   xy  
// INTERP                   0   xyzw        1     NONE   float   xyzw
// INTERP                   1   xyzw        2     NONE   float   x   
// INTERP                   2   xyzw        3     NONE   float   xyzw
// INTERP                   3   xyz         4     NONE   float   xyz 
// INTERP                   4   xyz         5     NONE   float   xyz 
// SV_IsFrontFace           0   x           6    FFACE    uint   x   
//
//
// Output signature:
//
// Name                 Index   Mask Register SysValue  Format   Used
// -------------------- ----- ------ -------- -------- ------- ------
// SV_Target                0   xyzw        0   TARGET   float   xyzw
//
      ps_4_0
      dcl_constantbuffer CB0[131], immediateIndexed
      dcl_constantbuffer CB1[15], immediateIndexed
      dcl_sampler s0, mode_default
      dcl_sampler s1, mode_default
      dcl_sampler s2, mode_default
      dcl_sampler s3, mode_default
      dcl_sampler s4, mode_default
      dcl_resource_texture2d (float,float,float,float) t0
      dcl_resource_texture2d (float,float,float,float) t1
      dcl_resource_texture2d (float,float,float,float) t2
      dcl_resource_texture2d (float,float,float,float) t3
      dcl_resource_texture2d (float,float,float,float) t4
      dcl_input_ps_siv linear noperspective v0.xy, position
      dcl_input_ps linear v1.xyzw
      dcl_input_ps linear v2.x
      dcl_input_ps linear v3.xyzw
      dcl_input_ps linear v4.xyz
      dcl_input_ps linear v5.xyz
      dcl_input_ps_sgv constant v6.x, is_front_face
      dcl_output o0.xyzw
      dcl_temps 4
   0: dp3 r0.x, v5.xyzx, v5.xyzx
   1: sqrt r0.x, r0.x
   2: div r0.x, l(1.000000, 1.000000, 1.000000, 1.000000), r0.x
   3: mul r0.xyz, r0.xxxx, v5.xyzx
   4: dp3 r0.w, r0.xyzx, r0.xyzx
   5: rsq r0.w, r0.w
   6: mul r0.xyz, r0.wwww, r0.xyzx
   7: add r1.xyz, -v4.xyzx, cb0[21].xyzx
   8: dp3 r0.w, r1.xyzx, r1.xyzx
   9: rsq r0.w, r0.w
  10: mul r1.xyz, r0.wwww, r1.xyzx
  11: eq r0.w, cb0[25].w, l(0.000000)
  12: movc r2.x, r0.w, r1.x, cb0[66].z
  13: movc r2.y, r0.w, r1.y, cb0[67].z
  14: movc r2.z, r0.w, r1.z, cb0[68].z
  15: dp3_sat r0.x, r0.xyzx, r2.xyzx
  16: add r0.x, -r0.x, l(1.000000)
  17: log r0.x, r0.x
  18: mul r0.x, r0.x, cb1[0].y
  19: exp r0.x, r0.x
  20: mul_sat r0.x, r0.x, cb1[0].z
  21: and r0.x, r0.x, v6.x
  22: lt r0.y, cb0[22].x, l(0.000000)
  23: add r0.z, -v0.y, cb0[3].y
  24: movc r1.y, r0.y, r0.z, v0.y
  25: mov r1.x, v0.x
  26: div r1.xy, r1.xyxx, cb0[3].xyxx
  27: add r1.z, -r1.y, l(1.000000)
  28: mad r0.yz, -cb0[130].xxyx, l(0.000000, 0.500000, 0.500000, 0.000000), l(0.000000, 1.000000, 1.000000, 0.000000)
  29: min r0.yz, r0.yyzy, r1.xxzx
  30: mul r0.yz, r0.yyzy, cb0[28].xxyx
  31: sample_b r1.xyzw, r0.yzyy, t0.xyzw, s0, cb0[4].x
  32: mad r0.y, cb0[24].x, r1.x, cb0[24].y
  33: div r0.y, l(1.000000, 1.000000, 1.000000, 1.000000), r0.y
  34: mul r0.z, v4.y, cb0[79].w
  35: mad r0.z, cb0[78].w, v4.x, r0.z
  36: mad r0.z, cb0[80].w, v4.z, r0.z
  37: add r0.z, r0.z, cb0[81].w
  38: mad r0.y, r0.y, cb0[22].z, -r0.z
  39: div_sat r0.y, r0.y, cb1[14].w
  40: ge r0.z, l(0.000000), r0.y
  41: and r0.z, r0.z, l(0x3f800000)
  42: add r0.w, -r0.y, l(1.000000)
  43: mad r0.y, r0.z, r0.w, r0.y
  44: add r0.x, -r0.y, r0.x
  45: add r0.x, r0.x, l(1.000000)
  46: mul r0.z, v1.w, l(1.000100)
  47: ge r0.w, r0.z, -r0.z
  48: frc r0.z, |r0.z|
  49: movc r0.z, r0.w, r0.z, -r0.z
  50: mul r0.z, r0.z, l(0.999900)
  51: div r1.xy, cb1[10].zwzz, cb1[5].xyxx
  52: mad r0.zw, cb0[19].xxxx, r1.xxxy, r0.zzzz
  53: add r0.zw, r0.zzzw, v1.xxxy
  54: mad r0.zw, r0.zzzw, cb1[5].xxxy, cb1[5].zzzw
  55: sample_b r1.xyzw, r0.zwzz, t2.xyzw, s2, cb0[4].x
  56: div r0.zw, cb1[11].xxxy, cb1[7].xxxy
  57: mad r0.zw, r0.zzzw, cb0[19].xxxx, v1.xxxy
  58: mad r0.zw, r0.zzzw, cb1[7].xxxy, cb1[7].zzzw
  59: sample_b r2.xyzw, r0.zwzz, t3.xyzw, s3, cb0[4].x
  60: mad r0.zw, v1.xxxy, cb1[9].xxxy, cb1[9].zzzw
  61: sample_b r3.xyzw, r0.zwzz, t4.xyzw, s4, cb0[4].x
  62: mul r0.zw, r2.xxxy, r3.xxxy
  63: mul r0.zw, r0.zzzw, cb1[11].zzzz
  64: div r0.zw, r0.zzzw, cb1[3].xxxy
  65: div r2.xy, cb1[10].xyxx, cb1[3].xyxx
  66: mad r2.xy, r2.xyxx, cb0[19].xxxx, v1.xyxx
  67: add r0.zw, -r0.zzzw, r2.xxxy
  68: mad r0.zw, r0.zzzw, cb1[3].xxxy, cb1[3].zzzw
  69: sample_b r2.xyzw, r0.zwzz, t1.xyzw, s1, cb0[4].x
  70: mul r1.xyzw, r1.wxyz, r2.wxyz
  71: ne r0.zw, l(0.000000, 0.000000, 0.000000, 0.000000), cb1[1].xxxy
  72: movc r2.xyz, r0.zzzz, r1.xxxx, r1.yzwy
  73: max r2.xyz, r0.xxxx, r2.xyzx
  74: mad_sat r0.x, r1.x, cb1[0].x, r0.x
  75: movc r1.yzw, r0.wwww, r2.xxyz, r1.yyzw
  76: add r0.z, -v1.z, l(1.000000)
  77: add_sat r2.xyz, -r0.zzzz, r3.xyzx
  78: mul_sat r2.xyz, r2.xyzx, r3.xyzx
  79: mul r2.xyz, r1.yzwy, r2.xyzx
  80: ne r3.xy, l(0.000000, 0.000000, 0.000000, 0.000000), cb1[14].yzyy
  81: movc r1.yzw, r3.xxxx, r2.xxyz, r1.yyzw
  82: mul r1.yzw, r1.yyzw, cb1[12].xxxx
  83: mul r1.yzw, r1.yyzw, v3.xxyz
  84: max r0.z, v2.x, l(1.000000)
  85: min r0.z, r0.z, l(1000000.000000)
  86: eq r2.x, v1.x, v2.x
  87: movc r0.z, r2.x, l(1.000000), r0.z
  88: mul o0.xyz, r1.yzwy, r0.zzzz
  89: mul r0.z, r0.x, r1.x
  90: ne r1.y, l(0.000000, 0.000000, 0.000000, 0.000000), cb1[0].w
  91: movc r0.x, r1.y, r0.z, r0.x
  92: movc r0.x, r0.w, r0.x, r1.x
  93: mul r0.y, r0.y, r0.x
  94: movc r0.x, r3.y, r0.y, r0.x
  95: mul r0.x, r0.x, v3.w
  96: mul_sat o0.w, r0.x, cb1[14].x
  97: ret 
// Approximately 0 instruction slots used


//////////////////////////////////////////////////////
Keywords: FOG_EXP2 LIGHTMAP_BICUBIC_SAMPLING _DBUFFER_MRT3
-- Vertex shader for "d3d11":
// No shader variant for this keyword set. The closest match will be used instead.

-- Hardware tier variant: Tier 1
-- Fragment shader for "d3d11":
// Stats: 72 math, 4 temp registers
Set 2D Texture "_DBufferTexture0" to slot 0 sampler slot -1
Set 2D Texture "_CameraDepthTexture" to slot 1 sampler slot -1
Set 2D Texture "_MainTex" to slot 2 sampler slot 1
Set 2D Texture "_Noise" to slot 3 sampler slot 2
Set 2D Texture "_Flow" to slot 4 sampler slot 3
Set 2D Texture "_Mask" to slot 5 sampler slot 4

Set Sampler PointClamp to slot 0

Constant Buffer "$Globals" (2096 bytes) on slot 0 {
  Matrix4x4 unity_MatrixV at 1056
  Matrix4x4 unity_MatrixVP at 1248
  Vector4 _ScaledScreenParams at 48
  Vector2 _GlobalMipBias at 64
  Vector4 _TimeParameters at 304
  Vector3 _WorldSpaceCameraPos at 336
  Vector4 _ProjectionParams at 352
  Vector4 _ZBufferParams at 384
  Vector4 unity_OrthoParams at 400
  Vector4 _RTHandleScale at 448
  Vector4 _CameraDepthTexture_TexelSize at 2080
}
Constant Buffer "UnityPerMaterial" (304 bytes) on slot 1 {
  Float _Fresnelpower at 4
  Float _Fresnelscale at 8
  Float _Useonlycolor at 16
  Float _Use_fresnel at 20
  Vector4 _MainTex_ST at 48
  Vector4 _Noise_ST at 80
  Vector4 _Flow_ST at 112
  Vector4 _Mask_ST at 144
  Vector4 _SpeedMainTexUVNoiseZW at 160
  Vector4 _DistortionSpeedXYPowerZ at 176
  Float _Emission at 192
  Float _Usecenterglow at 228
  Float _Depthpower at 236
}

Shader Disassembly:
//
// Generated by Microsoft (R) D3D Shader Disassembler
//
//
// Input signature:
//
// Name                 Index   Mask Register SysValue  Format   Used
// -------------------- ----- ------ -------- -------- ------- ------
// SV_POSITION              0   xyzw        0      POS   float   xy  
// INTERP                   0   xyzw        1     NONE   float   xyzw
// INTERP                   1   xyzw        2     NONE   float   x   
// INTERP                   2   xyzw        3     NONE   float   xyz 
// INTERP                   3   xyz         4     NONE   float   xyz 
// INTERP                   4   xyz         5     NONE   float   xyz 
// SV_IsFrontFace           0   x           6    FFACE    uint   x   
//
//
// Output signature:
//
// Name                 Index   Mask Register SysValue  Format   Used
// -------------------- ----- ------ -------- -------- ------- ------
// SV_Target                0   xyzw        0   TARGET   float   xyzw
//
      ps_4_0
      dcl_constantbuffer CB0[131], immediateIndexed
      dcl_constantbuffer CB1[15], immediateIndexed
      dcl_sampler s0, mode_default
      dcl_sampler s1, mode_default
      dcl_sampler s2, mode_default
      dcl_sampler s3, mode_default
      dcl_sampler s4, mode_default
      dcl_resource_texture2d (float,float,float,float) t0
      dcl_resource_texture2d (float,float,float,float) t1
      dcl_resource_texture2d (float,float,float,float) t2
      dcl_resource_texture2d (float,float,float,float) t3
      dcl_resource_texture2d (float,float,float,float) t4
      dcl_resource_texture2d (float,float,float,float) t5
      dcl_input_ps_siv linear noperspective v0.xy, position
      dcl_input_ps linear v1.xyzw
      dcl_input_ps linear v2.x
      dcl_input_ps linear v3.xyz
      dcl_input_ps linear v4.xyz
      dcl_input_ps linear v5.xyz
      dcl_input_ps_sgv constant v6.x, is_front_face
      dcl_output o0.xyzw
      dcl_temps 4
   0: dp3 r0.x, v5.xyzx, v5.xyzx
   1: sqrt r0.x, r0.x
   2: div r0.x, l(1.000000, 1.000000, 1.000000, 1.000000), r0.x
   3: mul r0.xyz, r0.xxxx, v5.xyzx
   4: dp3 r0.w, r0.xyzx, r0.xyzx
   5: rsq r0.w, r0.w
   6: mul r0.xyz, r0.wwww, r0.xyzx
   7: add r1.xyz, -v4.xyzx, cb0[21].xyzx
   8: dp3 r0.w, r1.xyzx, r1.xyzx
   9: rsq r0.w, r0.w
  10: mul r1.xyz, r0.wwww, r1.xyzx
  11: eq r0.w, cb0[25].w, l(0.000000)
  12: movc r2.x, r0.w, r1.x, cb0[66].z
  13: movc r2.y, r0.w, r1.y, cb0[67].z
  14: movc r2.z, r0.w, r1.z, cb0[68].z
  15: dp3_sat r0.x, r0.xyzx, r2.xyzx
  16: add r0.x, -r0.x, l(1.000000)
  17: log r0.x, r0.x
  18: mul r0.x, r0.x, cb1[0].y
  19: exp r0.x, r0.x
  20: mul_sat r0.x, r0.x, cb1[0].z
  21: and r0.x, r0.x, v6.x
  22: lt r0.y, cb0[22].x, l(0.000000)
  23: add r0.z, -v0.y, cb0[3].y
  24: movc r1.y, r0.y, r0.z, v0.y
  25: mov r1.x, v0.x
  26: div r1.xy, r1.xyxx, cb0[3].xyxx
  27: add r1.z, -r1.y, l(1.000000)
  28: mad r0.yz, -cb0[130].xxyx, l(0.000000, 0.500000, 0.500000, 0.000000), l(0.000000, 1.000000, 1.000000, 0.000000)
  29: min r0.yz, r0.yyzy, r1.xxzx
  30: mul r0.yz, r0.yyzy, cb0[28].xxyx
  31: sample_b r1.xyzw, r0.yzyy, t1.xyzw, s0, cb0[4].x
  32: mad r0.y, cb0[24].x, r1.x, cb0[24].y
  33: div r0.y, l(1.000000, 1.000000, 1.000000, 1.000000), r0.y
  34: mul r0.z, v4.y, cb0[79].w
  35: mad r0.z, cb0[78].w, v4.x, r0.z
  36: mad r0.z, cb0[80].w, v4.z, r0.z
  37: add r0.z, r0.z, cb0[81].w
  38: mad r0.y, r0.y, cb0[22].z, -r0.z
  39: div_sat r0.y, r0.y, cb1[14].w
  40: ge r0.z, l(0.000000), r0.y
  41: and r0.z, r0.z, l(0x3f800000)
  42: add r0.w, -r0.y, l(1.000000)
  43: mad r0.y, r0.z, r0.w, r0.y
  44: add r0.x, -r0.y, r0.x
  45: add r0.x, r0.x, l(1.000000)
  46: mul r0.y, v1.w, l(1.000100)
  47: ge r0.z, r0.y, -r0.y
  48: frc r0.y, |r0.y|
  49: movc r0.y, r0.z, r0.y, -r0.y
  50: mul r0.y, r0.y, l(0.999900)
  51: div r0.zw, cb1[10].zzzw, cb1[5].xxxy
  52: mad r0.yz, cb0[19].xxxx, r0.zzwz, r0.yyyy
  53: add r0.yz, r0.yyzy, v1.xxyx
  54: mad r0.yz, r0.yyzy, cb1[5].xxyx, cb1[5].zzwz
  55: sample_b r1.xyzw, r0.yzyy, t3.xyzw, s2, cb0[4].x
  56: div r0.yz, cb1[11].xxyx, cb1[7].xxyx
  57: mad r0.yz, r0.yyzy, cb0[19].xxxx, v1.xxyx
  58: mad r0.yz, r0.yyzy, cb1[7].xxyx, cb1[7].zzwz
  59: sample_b r2.xyzw, r0.yzyy, t4.xyzw, s3, cb0[4].x
  60: mad r0.yz, v1.xxyx, cb1[9].xxyx, cb1[9].zzwz
  61: sample_b r3.xyzw, r0.yzyy, t5.xyzw, s4, cb0[4].x
  62: mul r0.yz, r2.xxyx, r3.xxyx
  63: mul r0.yz, r0.yyzy, cb1[11].zzzz
  64: div r0.yz, r0.yyzy, cb1[3].xxyx
  65: div r2.xy, cb1[10].xyxx, cb1[3].xyxx
  66: mad r2.xy, r2.xyxx, cb0[19].xxxx, v1.xyxx
  67: add r0.yz, -r0.yyzy, r2.xxyx
  68: mad r0.yz, r0.yyzy, cb1[3].xxyx, cb1[3].zzwz
  69: sample_b r2.xyzw, r0.yzyy, t2.xyzw, s1, cb0[4].x
  70: mul r1.xyzw, r1.wxyz, r2.wxyz
  71: ne r0.yz, l(0.000000, 0.000000, 0.000000, 0.000000), cb1[1].xxyx
  72: movc r2.xyz, r0.yyyy, r1.xxxx, r1.yzwy
  73: max r0.xyw, r0.xxxx, r2.xyxz
  74: movc r0.xyz, r0.zzzz, r0.xywx, r1.yzwy
  75: add r0.w, -v1.z, l(1.000000)
  76: add_sat r1.xyz, -r0.wwww, r3.xyzx
  77: mul_sat r1.xyz, r1.xyzx, r3.xyzx
  78: mul r1.xyz, r0.xyzx, r1.xyzx
  79: ne r0.w, l(0.000000, 0.000000, 0.000000, 0.000000), cb1[14].y
  80: movc r0.xyz, r0.wwww, r1.xyzx, r0.xyzx
  81: mul r0.xyz, r0.xyzx, cb1[12].xxxx
  82: mul r0.xyz, r0.xyzx, v3.xyzx
  83: max r0.w, v2.x, l(1.000000)
  84: min r0.w, r0.w, l(1000000.000000)
  85: eq r1.x, v1.x, v2.x
  86: movc r0.w, r1.x, l(1.000000), r0.w
  87: mul r0.xyz, r0.xyzx, r0.wwww
  88: ftoi r1.xy, v0.xyxx
  89: mov r1.zw, l(0,0,0,0)
  90: ld r1.xyzw, r1.xyzw, t0.xyzw
  91: mad o0.xyz, r0.xyzx, r1.wwww, r1.xyzx
  92: mov o0.w, l(1.000000)
  93: ret 
// Approximately 0 instruction slots used


//////////////////////////////////////////////////////
Keywords: FOG_EXP2 LIGHTMAP_BICUBIC_SAMPLING _DBUFFER_MRT3 _SURFACE_TYPE_TRANSPARENT
-- Vertex shader for "d3d11":
// No shader variant for this keyword set. The closest match will be used instead.

-- Hardware tier variant: Tier 1
-- Fragment shader for "d3d11":
// Stats: 76 math, 4 temp registers
Set 2D Texture "_CameraDepthTexture" to slot 0 sampler slot -1
Set 2D Texture "_MainTex" to slot 1
Set 2D Texture "_Noise" to slot 2
Set 2D Texture "_Flow" to slot 3
Set 2D Texture "_Mask" to slot 4

Set Sampler PointClamp to slot 0

Constant Buffer "$Globals" (2096 bytes) on slot 0 {
  Matrix4x4 unity_MatrixV at 1056
  Matrix4x4 unity_MatrixVP at 1248
  Vector4 _ScaledScreenParams at 48
  Vector2 _GlobalMipBias at 64
  Vector4 _TimeParameters at 304
  Vector3 _WorldSpaceCameraPos at 336
  Vector4 _ProjectionParams at 352
  Vector4 _ZBufferParams at 384
  Vector4 unity_OrthoParams at 400
  Vector4 _RTHandleScale at 448
  Vector4 _CameraDepthTexture_TexelSize at 2080
}
Constant Buffer "UnityPerMaterial" (304 bytes) on slot 1 {
  Float _Textureopacity at 0
  Float _Fresnelpower at 4
  Float _Fresnelscale at 8
  Float _Multiply_texture at 12
  Float _Useonlycolor at 16
  Float _Use_fresnel at 20
  Vector4 _MainTex_ST at 48
  Vector4 _Noise_ST at 80
  Vector4 _Flow_ST at 112
  Vector4 _Mask_ST at 144
  Vector4 _SpeedMainTexUVNoiseZW at 160
  Vector4 _DistortionSpeedXYPowerZ at 176
  Float _Emission at 192
  Float _Opacity at 224
  Float _Usecenterglow at 228
  Float _Usedepth at 232
  Float _Depthpower at 236
}

Shader Disassembly:
//
// Generated by Microsoft (R) D3D Shader Disassembler
//
//
// Input signature:
//
// Name                 Index   Mask Register SysValue  Format   Used
// -------------------- ----- ------ -------- -------- ------- ------
// SV_POSITION              0   xyzw        0      POS   float   xy  
// INTERP                   0   xyzw        1     NONE   float   xyzw
// INTERP                   1   xyzw        2     NONE   float   x   
// INTERP                   2   xyzw        3     NONE   float   xyzw
// INTERP                   3   xyz         4     NONE   float   xyz 
// INTERP                   4   xyz         5     NONE   float   xyz 
// SV_IsFrontFace           0   x           6    FFACE    uint   x   
//
//
// Output signature:
//
// Name                 Index   Mask Register SysValue  Format   Used
// -------------------- ----- ------ -------- -------- ------- ------
// SV_Target                0   xyzw        0   TARGET   float   xyzw
//
      ps_4_0
      dcl_constantbuffer CB0[131], immediateIndexed
      dcl_constantbuffer CB1[15], immediateIndexed
      dcl_sampler s0, mode_default
      dcl_sampler s1, mode_default
      dcl_sampler s2, mode_default
      dcl_sampler s3, mode_default
      dcl_sampler s4, mode_default
      dcl_resource_texture2d (float,float,float,float) t0
      dcl_resource_texture2d (float,float,float,float) t1
      dcl_resource_texture2d (float,float,float,float) t2
      dcl_resource_texture2d (float,float,float,float) t3
      dcl_resource_texture2d (float,float,float,float) t4
      dcl_input_ps_siv linear noperspective v0.xy, position
      dcl_input_ps linear v1.xyzw
      dcl_input_ps linear v2.x
      dcl_input_ps linear v3.xyzw
      dcl_input_ps linear v4.xyz
      dcl_input_ps linear v5.xyz
      dcl_input_ps_sgv constant v6.x, is_front_face
      dcl_output o0.xyzw
      dcl_temps 4
   0: dp3 r0.x, v5.xyzx, v5.xyzx
   1: sqrt r0.x, r0.x
   2: div r0.x, l(1.000000, 1.000000, 1.000000, 1.000000), r0.x
   3: mul r0.xyz, r0.xxxx, v5.xyzx
   4: dp3 r0.w, r0.xyzx, r0.xyzx
   5: rsq r0.w, r0.w
   6: mul r0.xyz, r0.wwww, r0.xyzx
   7: add r1.xyz, -v4.xyzx, cb0[21].xyzx
   8: dp3 r0.w, r1.xyzx, r1.xyzx
   9: rsq r0.w, r0.w
  10: mul r1.xyz, r0.wwww, r1.xyzx
  11: eq r0.w, cb0[25].w, l(0.000000)
  12: movc r2.x, r0.w, r1.x, cb0[66].z
  13: movc r2.y, r0.w, r1.y, cb0[67].z
  14: movc r2.z, r0.w, r1.z, cb0[68].z
  15: dp3_sat r0.x, r0.xyzx, r2.xyzx
  16: add r0.x, -r0.x, l(1.000000)
  17: log r0.x, r0.x
  18: mul r0.x, r0.x, cb1[0].y
  19: exp r0.x, r0.x
  20: mul_sat r0.x, r0.x, cb1[0].z
  21: and r0.x, r0.x, v6.x
  22: lt r0.y, cb0[22].x, l(0.000000)
  23: add r0.z, -v0.y, cb0[3].y
  24: movc r1.y, r0.y, r0.z, v0.y
  25: mov r1.x, v0.x
  26: div r1.xy, r1.xyxx, cb0[3].xyxx
  27: add r1.z, -r1.y, l(1.000000)
  28: mad r0.yz, -cb0[130].xxyx, l(0.000000, 0.500000, 0.500000, 0.000000), l(0.000000, 1.000000, 1.000000, 0.000000)
  29: min r0.yz, r0.yyzy, r1.xxzx
  30: mul r0.yz, r0.yyzy, cb0[28].xxyx
  31: sample_b r1.xyzw, r0.yzyy, t0.xyzw, s0, cb0[4].x
  32: mad r0.y, cb0[24].x, r1.x, cb0[24].y
  33: div r0.y, l(1.000000, 1.000000, 1.000000, 1.000000), r0.y
  34: mul r0.z, v4.y, cb0[79].w
  35: mad r0.z, cb0[78].w, v4.x, r0.z
  36: mad r0.z, cb0[80].w, v4.z, r0.z
  37: add r0.z, r0.z, cb0[81].w
  38: mad r0.y, r0.y, cb0[22].z, -r0.z
  39: div_sat r0.y, r0.y, cb1[14].w
  40: ge r0.z, l(0.000000), r0.y
  41: and r0.z, r0.z, l(0x3f800000)
  42: add r0.w, -r0.y, l(1.000000)
  43: mad r0.y, r0.z, r0.w, r0.y
  44: add r0.x, -r0.y, r0.x
  45: add r0.x, r0.x, l(1.000000)
  46: mul r0.z, v1.w, l(1.000100)
  47: ge r0.w, r0.z, -r0.z
  48: frc r0.z, |r0.z|
  49: movc r0.z, r0.w, r0.z, -r0.z
  50: mul r0.z, r0.z, l(0.999900)
  51: div r1.xy, cb1[10].zwzz, cb1[5].xyxx
  52: mad r0.zw, cb0[19].xxxx, r1.xxxy, r0.zzzz
  53: add r0.zw, r0.zzzw, v1.xxxy
  54: mad r0.zw, r0.zzzw, cb1[5].xxxy, cb1[5].zzzw
  55: sample_b r1.xyzw, r0.zwzz, t2.xyzw, s2, cb0[4].x
  56: div r0.zw, cb1[11].xxxy, cb1[7].xxxy
  57: mad r0.zw, r0.zzzw, cb0[19].xxxx, v1.xxxy
  58: mad r0.zw, r0.zzzw, cb1[7].xxxy, cb1[7].zzzw
  59: sample_b r2.xyzw, r0.zwzz, t3.xyzw, s3, cb0[4].x
  60: mad r0.zw, v1.xxxy, cb1[9].xxxy, cb1[9].zzzw
  61: sample_b r3.xyzw, r0.zwzz, t4.xyzw, s4, cb0[4].x
  62: mul r0.zw, r2.xxxy, r3.xxxy
  63: mul r0.zw, r0.zzzw, cb1[11].zzzz
  64: div r0.zw, r0.zzzw, cb1[3].xxxy
  65: div r2.xy, cb1[10].xyxx, cb1[3].xyxx
  66: mad r2.xy, r2.xyxx, cb0[19].xxxx, v1.xyxx
  67: add r0.zw, -r0.zzzw, r2.xxxy
  68: mad r0.zw, r0.zzzw, cb1[3].xxxy, cb1[3].zzzw
  69: sample_b r2.xyzw, r0.zwzz, t1.xyzw, s1, cb0[4].x
  70: mul r1.xyzw, r1.wxyz, r2.wxyz
  71: ne r0.zw, l(0.000000, 0.000000, 0.000000, 0.000000), cb1[1].xxxy
  72: movc r2.xyz, r0.zzzz, r1.xxxx, r1.yzwy
  73: max r2.xyz, r0.xxxx, r2.xyzx
  74: mad_sat r0.x, r1.x, cb1[0].x, r0.x
  75: movc r1.yzw, r0.wwww, r2.xxyz, r1.yyzw
  76: add r0.z, -v1.z, l(1.000000)
  77: add_sat r2.xyz, -r0.zzzz, r3.xyzx
  78: mul_sat r2.xyz, r2.xyzx, r3.xyzx
  79: mul r2.xyz, r1.yzwy, r2.xyzx
  80: ne r3.xy, l(0.000000, 0.000000, 0.000000, 0.000000), cb1[14].yzyy
  81: movc r1.yzw, r3.xxxx, r2.xxyz, r1.yyzw
  82: mul r1.yzw, r1.yyzw, cb1[12].xxxx
  83: mul r1.yzw, r1.yyzw, v3.xxyz
  84: max r0.z, v2.x, l(1.000000)
  85: min r0.z, r0.z, l(1000000.000000)
  86: eq r2.x, v1.x, v2.x
  87: movc r0.z, r2.x, l(1.000000), r0.z
  88: mul o0.xyz, r1.yzwy, r0.zzzz
  89: mul r0.z, r0.x, r1.x
  90: ne r1.y, l(0.000000, 0.000000, 0.000000, 0.000000), cb1[0].w
  91: movc r0.x, r1.y, r0.z, r0.x
  92: movc r0.x, r0.w, r0.x, r1.x
  93: mul r0.y, r0.y, r0.x
  94: movc r0.x, r3.y, r0.y, r0.x
  95: mul r0.x, r0.x, v3.w
  96: mul_sat o0.w, r0.x, cb1[14].x
  97: ret 
// Approximately 0 instruction slots used


//////////////////////////////////////////////////////
Keywords: LIGHTMAP_BICUBIC_SAMPLING _DBUFFER_MRT3 _WRITE_RENDERING_LAYERS
-- Vertex shader for "d3d11":
// No shader variant for this keyword set. The closest match will be used instead.

-- Hardware tier variant: Tier 1
-- Fragment shader for "d3d11":
// Stats: 73 math, 4 temp registers
Set 2D Texture "_DBufferTexture0" to slot 0 sampler slot -1
Set 2D Texture "_CameraDepthTexture" to slot 1 sampler slot -1
Set 2D Texture "_MainTex" to slot 2 sampler slot 1
Set 2D Texture "_Noise" to slot 3 sampler slot 2
Set 2D Texture "_Flow" to slot 4 sampler slot 3
Set 2D Texture "_Mask" to slot 5 sampler slot 4

Set Sampler PointClamp to slot 0

Constant Buffer "$Globals" (2096 bytes) on slot 0 {
  Matrix4x4 unity_MatrixV at 1056
  Matrix4x4 unity_MatrixVP at 1248
  Vector4 _ScaledScreenParams at 48
  Vector2 _GlobalMipBias at 64
  ScalarInt _RenderingLayerMaxInt at 176
  Vector4 _TimeParameters at 304
  Vector3 _WorldSpaceCameraPos at 336
  Vector4 _ProjectionParams at 352
  Vector4 _ZBufferParams at 384
  Vector4 unity_OrthoParams at 400
  Vector4 _RTHandleScale at 448
  Vector4 _CameraDepthTexture_TexelSize at 2080
}
Constant Buffer "UnityPerDraw" (720 bytes) on slot 1 {
  Vector4 unity_RenderingLayer at 160
}
Constant Buffer "UnityPerMaterial" (304 bytes) on slot 2 {
  Float _Fresnelpower at 4
  Float _Fresnelscale at 8
  Float _Useonlycolor at 16
  Float _Use_fresnel at 20
  Vector4 _MainTex_ST at 48
  Vector4 _Noise_ST at 80
  Vector4 _Flow_ST at 112
  Vector4 _Mask_ST at 144
  Vector4 _SpeedMainTexUVNoiseZW at 160
  Vector4 _DistortionSpeedXYPowerZ at 176
  Float _Emission at 192
  Float _Usecenterglow at 228
  Float _Depthpower at 236
}

Shader Disassembly:
//
// Generated by Microsoft (R) D3D Shader Disassembler
//
//
// Input signature:
//
// Name                 Index   Mask Register SysValue  Format   Used
// -------------------- ----- ------ -------- -------- ------- ------
// SV_POSITION              0   xyzw        0      POS   float   xy  
// INTERP                   0   xyzw        1     NONE   float   xyzw
// INTERP                   1   xyzw        2     NONE   float   x   
// INTERP                   2   xyzw        3     NONE   float   xyz 
// INTERP                   3   xyz         4     NONE   float   xyz 
// INTERP                   4   xyz         5     NONE   float   xyz 
// SV_IsFrontFace           0   x           6    FFACE    uint   x   
//
//
// Output signature:
//
// Name                 Index   Mask Register SysValue  Format   Used
// -------------------- ----- ------ -------- -------- ------- ------
// SV_Target                0   xyzw        0   TARGET   float   xyzw
// SV_Target                1   x           1   TARGET    uint   x   
//
      ps_5_0
      dcl_globalFlags refactoringAllowed
      dcl_constantbuffer CB0[131], immediateIndexed
      dcl_constantbuffer CB1[11], immediateIndexed
      dcl_constantbuffer CB2[15], immediateIndexed
      dcl_sampler s0, mode_default
      dcl_sampler s1, mode_default
      dcl_sampler s2, mode_default
      dcl_sampler s3, mode_default
      dcl_sampler s4, mode_default
      dcl_resource_texture2d (float,float,float,float) t0
      dcl_resource_texture2d (float,float,float,float) t1
      dcl_resource_texture2d (float,float,float,float) t2
      dcl_resource_texture2d (float,float,float,float) t3
      dcl_resource_texture2d (float,float,float,float) t4
      dcl_resource_texture2d (float,float,float,float) t5
      dcl_input_ps_siv linear noperspective v0.xy, position
      dcl_input_ps linear v1.xyzw
      dcl_input_ps linear v2.x
      dcl_input_ps linear v3.xyz
      dcl_input_ps linear v4.xyz
      dcl_input_ps linear v5.xyz
      dcl_input_ps_sgv constant v6.x, is_front_face
      dcl_output o0.xyzw
      dcl_output o1.x
      dcl_temps 4
   0: dp3 r0.x, v5.xyzx, v5.xyzx
   1: sqrt r0.x, r0.x
   2: div r0.x, l(1.000000, 1.000000, 1.000000, 1.000000), r0.x
   3: mul r0.xyz, r0.xxxx, v5.xyzx
   4: dp3 r0.w, r0.xyzx, r0.xyzx
   5: rsq r0.w, r0.w
   6: mul r0.xyz, r0.wwww, r0.xyzx
   7: add r1.xyz, -v4.xyzx, cb0[21].xyzx
   8: dp3 r0.w, r1.xyzx, r1.xyzx
   9: rsq r0.w, r0.w
  10: mul r1.xyz, r0.wwww, r1.xyzx
  11: eq r0.w, cb0[25].w, l(0.000000)
  12: movc r2.x, r0.w, r1.x, cb0[66].z
  13: movc r2.y, r0.w, r1.y, cb0[67].z
  14: movc r2.z, r0.w, r1.z, cb0[68].z
  15: dp3_sat r0.x, r0.xyzx, r2.xyzx
  16: add r0.x, -r0.x, l(1.000000)
  17: log r0.x, r0.x
  18: mul r0.x, r0.x, cb2[0].y
  19: exp r0.x, r0.x
  20: mul_sat r0.x, r0.x, cb2[0].z
  21: and r0.x, r0.x, v6.x
  22: lt r0.y, cb0[22].x, l(0.000000)
  23: add r0.z, -v0.y, cb0[3].y
  24: movc r1.y, r0.y, r0.z, v0.y
  25: mov r1.x, v0.x
  26: div r1.xy, r1.xyxx, cb0[3].xyxx
  27: add r1.z, -r1.y, l(1.000000)
  28: mad r0.yz, -cb0[130].xxyx, l(0.000000, 0.500000, 0.500000, 0.000000), l(0.000000, 1.000000, 1.000000, 0.000000)
  29: min r0.yz, r0.yyzy, r1.xxzx
  30: mul r0.yz, r0.yyzy, cb0[28].xxyx
  31: sample_b_indexable(texture2d)(float,float,float,float) r0.y, r0.yzyy, t1.yxzw, s0, cb0[4].x
  32: mad r0.y, cb0[24].x, r0.y, cb0[24].y
  33: div r0.y, l(1.000000, 1.000000, 1.000000, 1.000000), r0.y
  34: mul r0.z, v4.y, cb0[79].w
  35: mad r0.z, cb0[78].w, v4.x, r0.z
  36: mad r0.z, cb0[80].w, v4.z, r0.z
  37: add r0.z, r0.z, cb0[81].w
  38: mad r0.y, r0.y, cb0[22].z, -r0.z
  39: div_sat r0.y, r0.y, cb2[14].w
  40: ge r0.z, l(0.000000), r0.y
  41: and r0.z, r0.z, l(0x3f800000)
  42: add r0.w, -r0.y, l(1.000000)
  43: mad r0.y, r0.z, r0.w, r0.y
  44: add r0.x, -r0.y, r0.x
  45: add r0.x, r0.x, l(1.000000)
  46: mul r0.y, v1.w, l(1.000100)
  47: ge r0.z, r0.y, -r0.y
  48: frc r0.y, |r0.y|
  49: movc r0.y, r0.z, r0.y, -r0.y
  50: mul r0.y, r0.y, l(0.999900)
  51: div r0.zw, cb2[10].zzzw, cb2[5].xxxy
  52: mad r0.yz, cb0[19].xxxx, r0.zzwz, r0.yyyy
  53: add r0.yz, r0.yyzy, v1.xxyx
  54: mad r0.yz, r0.yyzy, cb2[5].xxyx, cb2[5].zzwz
  55: sample_b_indexable(texture2d)(float,float,float,float) r1.xyzw, r0.yzyy, t3.xyzw, s2, cb0[4].x
  56: div r0.yz, cb2[11].xxyx, cb2[7].xxyx
  57: mad r0.yz, r0.yyzy, cb0[19].xxxx, v1.xxyx
  58: mad r0.yz, r0.yyzy, cb2[7].xxyx, cb2[7].zzwz
  59: sample_b_indexable(texture2d)(float,float,float,float) r0.yz, r0.yzyy, t4.zxyw, s3, cb0[4].x
  60: mad r2.xy, v1.xyxx, cb2[9].xyxx, cb2[9].zwzz
  61: sample_b_indexable(texture2d)(float,float,float,float) r2.xyz, r2.xyxx, t5.xyzw, s4, cb0[4].x
  62: mul r0.yz, r0.yyzy, r2.xxyx
  63: mul r0.yz, r0.yyzy, cb2[11].zzzz
  64: div r0.yz, r0.yyzy, cb2[3].xxyx
  65: div r3.xy, cb2[10].xyxx, cb2[3].xyxx
  66: mad r3.xy, r3.xyxx, cb0[19].xxxx, v1.xyxx
  67: add r0.yz, -r0.yyzy, r3.xxyx
  68: mad r0.yz, r0.yyzy, cb2[3].xxyx, cb2[3].zzwz
  69: sample_b_indexable(texture2d)(float,float,float,float) r3.xyzw, r0.yzyy, t2.xyzw, s1, cb0[4].x
  70: mul r1.xyzw, r1.wxyz, r3.wxyz
  71: ne r0.yz, l(0.000000, 0.000000, 0.000000, 0.000000), cb2[1].xxyx
  72: movc r3.xyz, r0.yyyy, r1.xxxx, r1.yzwy
  73: max r0.xyw, r0.xxxx, r3.xyxz
  74: movc r0.xyz, r0.zzzz, r0.xywx, r1.yzwy
  75: add r0.w, -v1.z, l(1.000000)
  76: add_sat r1.xyz, -r0.wwww, r2.xyzx
  77: mul_sat r1.xyz, r1.xyzx, r2.xyzx
  78: mul r1.xyz, r0.xyzx, r1.xyzx
  79: ne r0.w, l(0.000000, 0.000000, 0.000000, 0.000000), cb2[14].y
  80: movc r0.xyz, r0.wwww, r1.xyzx, r0.xyzx
  81: mul r0.xyz, r0.xyzx, cb2[12].xxxx
  82: mul r0.xyz, r0.xyzx, v3.xyzx
  83: max r0.w, v2.x, l(1.000000)
  84: min r0.w, r0.w, l(1000000.000000)
  85: eq r1.x, v1.x, v2.x
  86: movc r0.w, r1.x, l(1.000000), r0.w
  87: mul r0.xyz, r0.xyzx, r0.wwww
  88: ftoi r1.xy, v0.xyxx
  89: mov r1.zw, l(0,0,0,0)
  90: ld_indexable(texture2d)(float,float,float,float) r1.xyzw, r1.xyzw, t0.xyzw
  91: mad o0.xyz, r0.xyzx, r1.wwww, r1.xyzx
  92: mov o0.w, l(1.000000)
  93: and o1.x, cb0[11].x, cb1[10].x
  94: ret 
// Approximately 0 instruction slots used


//////////////////////////////////////////////////////
Keywords: LIGHTMAP_BICUBIC_SAMPLING _DBUFFER_MRT3 _SURFACE_TYPE_TRANSPARENT _WRITE_RENDERING_LAYERS
-- Vertex shader for "d3d11":
// No shader variant for this keyword set. The closest match will be used instead.

-- Hardware tier variant: Tier 1
-- Fragment shader for "d3d11":
// Stats: 77 math, 4 temp registers
Set 2D Texture "_CameraDepthTexture" to slot 0 sampler slot -1
Set 2D Texture "_MainTex" to slot 1
Set 2D Texture "_Noise" to slot 2
Set 2D Texture "_Flow" to slot 3
Set 2D Texture "_Mask" to slot 4

Set Sampler PointClamp to slot 0

Constant Buffer "$Globals" (2096 bytes) on slot 0 {
  Matrix4x4 unity_MatrixV at 1056
  Matrix4x4 unity_MatrixVP at 1248
  Vector4 _ScaledScreenParams at 48
  Vector2 _GlobalMipBias at 64
  ScalarInt _RenderingLayerMaxInt at 176
  Vector4 _TimeParameters at 304
  Vector3 _WorldSpaceCameraPos at 336
  Vector4 _ProjectionParams at 352
  Vector4 _ZBufferParams at 384
  Vector4 unity_OrthoParams at 400
  Vector4 _RTHandleScale at 448
  Vector4 _CameraDepthTexture_TexelSize at 2080
}
Constant Buffer "UnityPerDraw" (720 bytes) on slot 1 {
  Vector4 unity_RenderingLayer at 160
}
Constant Buffer "UnityPerMaterial" (304 bytes) on slot 2 {
  Float _Textureopacity at 0
  Float _Fresnelpower at 4
  Float _Fresnelscale at 8
  Float _Multiply_texture at 12
  Float _Useonlycolor at 16
  Float _Use_fresnel at 20
  Vector4 _MainTex_ST at 48
  Vector4 _Noise_ST at 80
  Vector4 _Flow_ST at 112
  Vector4 _Mask_ST at 144
  Vector4 _SpeedMainTexUVNoiseZW at 160
  Vector4 _DistortionSpeedXYPowerZ at 176
  Float _Emission at 192
  Float _Opacity at 224
  Float _Usecenterglow at 228
  Float _Usedepth at 232
  Float _Depthpower at 236
}

Shader Disassembly:
//
// Generated by Microsoft (R) D3D Shader Disassembler
//
//
// Input signature:
//
// Name                 Index   Mask Register SysValue  Format   Used
// -------------------- ----- ------ -------- -------- ------- ------
// SV_POSITION              0   xyzw        0      POS   float   xy  
// INTERP                   0   xyzw        1     NONE   float   xyzw
// INTERP                   1   xyzw        2     NONE   float   x   
// INTERP                   2   xyzw        3     NONE   float   xyzw
// INTERP                   3   xyz         4     NONE   float   xyz 
// INTERP                   4   xyz         5     NONE   float   xyz 
// SV_IsFrontFace           0   x           6    FFACE    uint   x   
//
//
// Output signature:
//
// Name                 Index   Mask Register SysValue  Format   Used
// -------------------- ----- ------ -------- -------- ------- ------
// SV_Target                0   xyzw        0   TARGET   float   xyzw
// SV_Target                1   x           1   TARGET    uint   x   
//
      ps_5_0
      dcl_globalFlags refactoringAllowed
      dcl_constantbuffer CB0[131], immediateIndexed
      dcl_constantbuffer CB1[11], immediateIndexed
      dcl_constantbuffer CB2[15], immediateIndexed
      dcl_sampler s0, mode_default
      dcl_sampler s1, mode_default
      dcl_sampler s2, mode_default
      dcl_sampler s3, mode_default
      dcl_sampler s4, mode_default
      dcl_resource_texture2d (float,float,float,float) t0
      dcl_resource_texture2d (float,float,float,float) t1
      dcl_resource_texture2d (float,float,float,float) t2
      dcl_resource_texture2d (float,float,float,float) t3
      dcl_resource_texture2d (float,float,float,float) t4
      dcl_input_ps_siv linear noperspective v0.xy, position
      dcl_input_ps linear v1.xyzw
      dcl_input_ps linear v2.x
      dcl_input_ps linear v3.xyzw
      dcl_input_ps linear v4.xyz
      dcl_input_ps linear v5.xyz
      dcl_input_ps_sgv constant v6.x, is_front_face
      dcl_output o0.xyzw
      dcl_output o1.x
      dcl_temps 4
   0: dp3 r0.x, v5.xyzx, v5.xyzx
   1: sqrt r0.x, r0.x
   2: div r0.x, l(1.000000, 1.000000, 1.000000, 1.000000), r0.x
   3: mul r0.xyz, r0.xxxx, v5.xyzx
   4: dp3 r0.w, r0.xyzx, r0.xyzx
   5: rsq r0.w, r0.w
   6: mul r0.xyz, r0.wwww, r0.xyzx
   7: add r1.xyz, -v4.xyzx, cb0[21].xyzx
   8: dp3 r0.w, r1.xyzx, r1.xyzx
   9: rsq r0.w, r0.w
  10: mul r1.xyz, r0.wwww, r1.xyzx
  11: eq r0.w, cb0[25].w, l(0.000000)
  12: movc r2.x, r0.w, r1.x, cb0[66].z
  13: movc r2.y, r0.w, r1.y, cb0[67].z
  14: movc r2.z, r0.w, r1.z, cb0[68].z
  15: dp3_sat r0.x, r0.xyzx, r2.xyzx
  16: add r0.x, -r0.x, l(1.000000)
  17: log r0.x, r0.x
  18: mul r0.x, r0.x, cb2[0].y
  19: exp r0.x, r0.x
  20: mul_sat r0.x, r0.x, cb2[0].z
  21: and r0.x, r0.x, v6.x
  22: lt r0.y, cb0[22].x, l(0.000000)
  23: add r0.z, -v0.y, cb0[3].y
  24: movc r1.y, r0.y, r0.z, v0.y
  25: mov r1.x, v0.x
  26: div r1.xy, r1.xyxx, cb0[3].xyxx
  27: add r1.z, -r1.y, l(1.000000)
  28: mad r0.yz, -cb0[130].xxyx, l(0.000000, 0.500000, 0.500000, 0.000000), l(0.000000, 1.000000, 1.000000, 0.000000)
  29: min r0.yz, r0.yyzy, r1.xxzx
  30: mul r0.yz, r0.yyzy, cb0[28].xxyx
  31: sample_b_indexable(texture2d)(float,float,float,float) r0.y, r0.yzyy, t0.yxzw, s0, cb0[4].x
  32: mad r0.y, cb0[24].x, r0.y, cb0[24].y
  33: div r0.y, l(1.000000, 1.000000, 1.000000, 1.000000), r0.y
  34: mul r0.z, v4.y, cb0[79].w
  35: mad r0.z, cb0[78].w, v4.x, r0.z
  36: mad r0.z, cb0[80].w, v4.z, r0.z
  37: add r0.z, r0.z, cb0[81].w
  38: mad r0.y, r0.y, cb0[22].z, -r0.z
  39: div_sat r0.y, r0.y, cb2[14].w
  40: ge r0.z, l(0.000000), r0.y
  41: and r0.z, r0.z, l(0x3f800000)
  42: add r0.w, -r0.y, l(1.000000)
  43: mad r0.y, r0.z, r0.w, r0.y
  44: add r0.x, -r0.y, r0.x
  45: add r0.x, r0.x, l(1.000000)
  46: mul r0.z, v1.w, l(1.000100)
  47: ge r0.w, r0.z, -r0.z
  48: frc r0.z, |r0.z|
  49: movc r0.z, r0.w, r0.z, -r0.z
  50: mul r0.z, r0.z, l(0.999900)
  51: div r1.xy, cb2[10].zwzz, cb2[5].xyxx
  52: mad r0.zw, cb0[19].xxxx, r1.xxxy, r0.zzzz
  53: add r0.zw, r0.zzzw, v1.xxxy
  54: mad r0.zw, r0.zzzw, cb2[5].xxxy, cb2[5].zzzw
  55: sample_b_indexable(texture2d)(float,float,float,float) r1.xyzw, r0.zwzz, t2.xyzw, s2, cb0[4].x
  56: div r0.zw, cb2[11].xxxy, cb2[7].xxxy
  57: mad r0.zw, r0.zzzw, cb0[19].xxxx, v1.xxxy
  58: mad r0.zw, r0.zzzw, cb2[7].xxxy, cb2[7].zzzw
  59: sample_b_indexable(texture2d)(float,float,float,float) r0.zw, r0.zwzz, t3.zwxy, s3, cb0[4].x
  60: mad r2.xy, v1.xyxx, cb2[9].xyxx, cb2[9].zwzz
  61: sample_b_indexable(texture2d)(float,float,float,float) r2.xyz, r2.xyxx, t4.xyzw, s4, cb0[4].x
  62: mul r0.zw, r0.zzzw, r2.xxxy
  63: mul r0.zw, r0.zzzw, cb2[11].zzzz
  64: div r0.zw, r0.zzzw, cb2[3].xxxy
  65: div r3.xy, cb2[10].xyxx, cb2[3].xyxx
  66: mad r3.xy, r3.xyxx, cb0[19].xxxx, v1.xyxx
  67: add r0.zw, -r0.zzzw, r3.xxxy
  68: mad r0.zw, r0.zzzw, cb2[3].xxxy, cb2[3].zzzw
  69: sample_b_indexable(texture2d)(float,float,float,float) r3.xyzw, r0.zwzz, t1.xyzw, s1, cb0[4].x
  70: mul r1.xyzw, r1.wxyz, r3.wxyz
  71: ne r0.zw, l(0.000000, 0.000000, 0.000000, 0.000000), cb2[1].xxxy
  72: movc r3.xyz, r0.zzzz, r1.xxxx, r1.yzwy
  73: max r3.xyz, r0.xxxx, r3.xyzx
  74: mad_sat r0.x, r1.x, cb2[0].x, r0.x
  75: movc r1.yzw, r0.wwww, r3.xxyz, r1.yyzw
  76: add r0.z, -v1.z, l(1.000000)
  77: add_sat r3.xyz, -r0.zzzz, r2.xyzx
  78: mul_sat r2.xyz, r2.xyzx, r3.xyzx
  79: mul r2.xyz, r1.yzwy, r2.xyzx
  80: ne r3.xy, l(0.000000, 0.000000, 0.000000, 0.000000), cb2[14].yzyy
  81: movc r1.yzw, r3.xxxx, r2.xxyz, r1.yyzw
  82: mul r1.yzw, r1.yyzw, cb2[12].xxxx
  83: mul r1.yzw, r1.yyzw, v3.xxyz
  84: max r0.z, v2.x, l(1.000000)
  85: min r0.z, r0.z, l(1000000.000000)
  86: eq r2.x, v1.x, v2.x
  87: movc r0.z, r2.x, l(1.000000), r0.z
  88: mul o0.xyz, r1.yzwy, r0.zzzz
  89: mul r0.z, r0.x, r1.x
  90: ne r1.y, l(0.000000, 0.000000, 0.000000, 0.000000), cb2[0].w
  91: movc r0.x, r1.y, r0.z, r0.x
  92: movc r0.x, r0.w, r0.x, r1.x
  93: mul r0.y, r0.y, r0.x
  94: movc r0.x, r3.y, r0.y, r0.x
  95: mul r0.x, r0.x, v3.w
  96: mul_sat o0.w, r0.x, cb2[14].x
  97: and o1.x, cb0[11].x, cb1[10].x
  98: ret 
// Approximately 0 instruction slots used


//////////////////////////////////////////////////////
Keywords: FOG_EXP2 LIGHTMAP_BICUBIC_SAMPLING _DBUFFER_MRT3 _WRITE_RENDERING_LAYERS
-- Vertex shader for "d3d11":
// No shader variant for this keyword set. The closest match will be used instead.

-- Hardware tier variant: Tier 1
-- Fragment shader for "d3d11":
// Stats: 73 math, 4 temp registers
Set 2D Texture "_DBufferTexture0" to slot 0 sampler slot -1
Set 2D Texture "_CameraDepthTexture" to slot 1 sampler slot -1
Set 2D Texture "_MainTex" to slot 2 sampler slot 1
Set 2D Texture "_Noise" to slot 3 sampler slot 2
Set 2D Texture "_Flow" to slot 4 sampler slot 3
Set 2D Texture "_Mask" to slot 5 sampler slot 4

Set Sampler PointClamp to slot 0

Constant Buffer "$Globals" (2096 bytes) on slot 0 {
  Matrix4x4 unity_MatrixV at 1056
  Matrix4x4 unity_MatrixVP at 1248
  Vector4 _ScaledScreenParams at 48
  Vector2 _GlobalMipBias at 64
  ScalarInt _RenderingLayerMaxInt at 176
  Vector4 _TimeParameters at 304
  Vector3 _WorldSpaceCameraPos at 336
  Vector4 _ProjectionParams at 352
  Vector4 _ZBufferParams at 384
  Vector4 unity_OrthoParams at 400
  Vector4 _RTHandleScale at 448
  Vector4 _CameraDepthTexture_TexelSize at 2080
}
Constant Buffer "UnityPerDraw" (720 bytes) on slot 1 {
  Vector4 unity_RenderingLayer at 160
}
Constant Buffer "UnityPerMaterial" (304 bytes) on slot 2 {
  Float _Fresnelpower at 4
  Float _Fresnelscale at 8
  Float _Useonlycolor at 16
  Float _Use_fresnel at 20
  Vector4 _MainTex_ST at 48
  Vector4 _Noise_ST at 80
  Vector4 _Flow_ST at 112
  Vector4 _Mask_ST at 144
  Vector4 _SpeedMainTexUVNoiseZW at 160
  Vector4 _DistortionSpeedXYPowerZ at 176
  Float _Emission at 192
  Float _Usecenterglow at 228
  Float _Depthpower at 236
}

Shader Disassembly:
//
// Generated by Microsoft (R) D3D Shader Disassembler
//
//
// Input signature:
//
// Name                 Index   Mask Register SysValue  Format   Used
// -------------------- ----- ------ -------- -------- ------- ------
// SV_POSITION              0   xyzw        0      POS   float   xy  
// INTERP                   0   xyzw        1     NONE   float   xyzw
// INTERP                   1   xyzw        2     NONE   float   x   
// INTERP                   2   xyzw        3     NONE   float   xyz 
// INTERP                   3   xyz         4     NONE   float   xyz 
// INTERP                   4   xyz         5     NONE   float   xyz 
// SV_IsFrontFace           0   x           6    FFACE    uint   x   
//
//
// Output signature:
//
// Name                 Index   Mask Register SysValue  Format   Used
// -------------------- ----- ------ -------- -------- ------- ------
// SV_Target                0   xyzw        0   TARGET   float   xyzw
// SV_Target                1   x           1   TARGET    uint   x   
//
      ps_5_0
      dcl_globalFlags refactoringAllowed
      dcl_constantbuffer CB0[131], immediateIndexed
      dcl_constantbuffer CB1[11], immediateIndexed
      dcl_constantbuffer CB2[15], immediateIndexed
      dcl_sampler s0, mode_default
      dcl_sampler s1, mode_default
      dcl_sampler s2, mode_default
      dcl_sampler s3, mode_default
      dcl_sampler s4, mode_default
      dcl_resource_texture2d (float,float,float,float) t0
      dcl_resource_texture2d (float,float,float,float) t1
      dcl_resource_texture2d (float,float,float,float) t2
      dcl_resource_texture2d (float,float,float,float) t3
      dcl_resource_texture2d (float,float,float,float) t4
      dcl_resource_texture2d (float,float,float,float) t5
      dcl_input_ps_siv linear noperspective v0.xy, position
      dcl_input_ps linear v1.xyzw
      dcl_input_ps linear v2.x
      dcl_input_ps linear v3.xyz
      dcl_input_ps linear v4.xyz
      dcl_input_ps linear v5.xyz
      dcl_input_ps_sgv constant v6.x, is_front_face
      dcl_output o0.xyzw
      dcl_output o1.x
      dcl_temps 4
   0: dp3 r0.x, v5.xyzx, v5.xyzx
   1: sqrt r0.x, r0.x
   2: div r0.x, l(1.000000, 1.000000, 1.000000, 1.000000), r0.x
   3: mul r0.xyz, r0.xxxx, v5.xyzx
   4: dp3 r0.w, r0.xyzx, r0.xyzx
   5: rsq r0.w, r0.w
   6: mul r0.xyz, r0.wwww, r0.xyzx
   7: add r1.xyz, -v4.xyzx, cb0[21].xyzx
   8: dp3 r0.w, r1.xyzx, r1.xyzx
   9: rsq r0.w, r0.w
  10: mul r1.xyz, r0.wwww, r1.xyzx
  11: eq r0.w, cb0[25].w, l(0.000000)
  12: movc r2.x, r0.w, r1.x, cb0[66].z
  13: movc r2.y, r0.w, r1.y, cb0[67].z
  14: movc r2.z, r0.w, r1.z, cb0[68].z
  15: dp3_sat r0.x, r0.xyzx, r2.xyzx
  16: add r0.x, -r0.x, l(1.000000)
  17: log r0.x, r0.x
  18: mul r0.x, r0.x, cb2[0].y
  19: exp r0.x, r0.x
  20: mul_sat r0.x, r0.x, cb2[0].z
  21: and r0.x, r0.x, v6.x
  22: lt r0.y, cb0[22].x, l(0.000000)
  23: add r0.z, -v0.y, cb0[3].y
  24: movc r1.y, r0.y, r0.z, v0.y
  25: mov r1.x, v0.x
  26: div r1.xy, r1.xyxx, cb0[3].xyxx
  27: add r1.z, -r1.y, l(1.000000)
  28: mad r0.yz, -cb0[130].xxyx, l(0.000000, 0.500000, 0.500000, 0.000000), l(0.000000, 1.000000, 1.000000, 0.000000)
  29: min r0.yz, r0.yyzy, r1.xxzx
  30: mul r0.yz, r0.yyzy, cb0[28].xxyx
  31: sample_b_indexable(texture2d)(float,float,float,float) r0.y, r0.yzyy, t1.yxzw, s0, cb0[4].x
  32: mad r0.y, cb0[24].x, r0.y, cb0[24].y
  33: div r0.y, l(1.000000, 1.000000, 1.000000, 1.000000), r0.y
  34: mul r0.z, v4.y, cb0[79].w
  35: mad r0.z, cb0[78].w, v4.x, r0.z
  36: mad r0.z, cb0[80].w, v4.z, r0.z
  37: add r0.z, r0.z, cb0[81].w
  38: mad r0.y, r0.y, cb0[22].z, -r0.z
  39: div_sat r0.y, r0.y, cb2[14].w
  40: ge r0.z, l(0.000000), r0.y
  41: and r0.z, r0.z, l(0x3f800000)
  42: add r0.w, -r0.y, l(1.000000)
  43: mad r0.y, r0.z, r0.w, r0.y
  44: add r0.x, -r0.y, r0.x
  45: add r0.x, r0.x, l(1.000000)
  46: mul r0.y, v1.w, l(1.000100)
  47: ge r0.z, r0.y, -r0.y
  48: frc r0.y, |r0.y|
  49: movc r0.y, r0.z, r0.y, -r0.y
  50: mul r0.y, r0.y, l(0.999900)
  51: div r0.zw, cb2[10].zzzw, cb2[5].xxxy
  52: mad r0.yz, cb0[19].xxxx, r0.zzwz, r0.yyyy
  53: add r0.yz, r0.yyzy, v1.xxyx
  54: mad r0.yz, r0.yyzy, cb2[5].xxyx, cb2[5].zzwz
  55: sample_b_indexable(texture2d)(float,float,float,float) r1.xyzw, r0.yzyy, t3.xyzw, s2, cb0[4].x
  56: div r0.yz, cb2[11].xxyx, cb2[7].xxyx
  57: mad r0.yz, r0.yyzy, cb0[19].xxxx, v1.xxyx
  58: mad r0.yz, r0.yyzy, cb2[7].xxyx, cb2[7].zzwz
  59: sample_b_indexable(texture2d)(float,float,float,float) r0.yz, r0.yzyy, t4.zxyw, s3, cb0[4].x
  60: mad r2.xy, v1.xyxx, cb2[9].xyxx, cb2[9].zwzz
  61: sample_b_indexable(texture2d)(float,float,float,float) r2.xyz, r2.xyxx, t5.xyzw, s4, cb0[4].x
  62: mul r0.yz, r0.yyzy, r2.xxyx
  63: mul r0.yz, r0.yyzy, cb2[11].zzzz
  64: div r0.yz, r0.yyzy, cb2[3].xxyx
  65: div r3.xy, cb2[10].xyxx, cb2[3].xyxx
  66: mad r3.xy, r3.xyxx, cb0[19].xxxx, v1.xyxx
  67: add r0.yz, -r0.yyzy, r3.xxyx
  68: mad r0.yz, r0.yyzy, cb2[3].xxyx, cb2[3].zzwz
  69: sample_b_indexable(texture2d)(float,float,float,float) r3.xyzw, r0.yzyy, t2.xyzw, s1, cb0[4].x
  70: mul r1.xyzw, r1.wxyz, r3.wxyz
  71: ne r0.yz, l(0.000000, 0.000000, 0.000000, 0.000000), cb2[1].xxyx
  72: movc r3.xyz, r0.yyyy, r1.xxxx, r1.yzwy
  73: max r0.xyw, r0.xxxx, r3.xyxz
  74: movc r0.xyz, r0.zzzz, r0.xywx, r1.yzwy
  75: add r0.w, -v1.z, l(1.000000)
  76: add_sat r1.xyz, -r0.wwww, r2.xyzx
  77: mul_sat r1.xyz, r1.xyzx, r2.xyzx
  78: mul r1.xyz, r0.xyzx, r1.xyzx
  79: ne r0.w, l(0.000000, 0.000000, 0.000000, 0.000000), cb2[14].y
  80: movc r0.xyz, r0.wwww, r1.xyzx, r0.xyzx
  81: mul r0.xyz, r0.xyzx, cb2[12].xxxx
  82: mul r0.xyz, r0.xyzx, v3.xyzx
  83: max r0.w, v2.x, l(1.000000)
  84: min r0.w, r0.w, l(1000000.000000)
  85: eq r1.x, v1.x, v2.x
  86: movc r0.w, r1.x, l(1.000000), r0.w
  87: mul r0.xyz, r0.xyzx, r0.wwww
  88: ftoi r1.xy, v0.xyxx
  89: mov r1.zw, l(0,0,0,0)
  90: ld_indexable(texture2d)(float,float,float,float) r1.xyzw, r1.xyzw, t0.xyzw
  91: mad o0.xyz, r0.xyzx, r1.wwww, r1.xyzx
  92: mov o0.w, l(1.000000)
  93: and o1.x, cb0[11].x, cb1[10].x
  94: ret 
// Approximately 0 instruction slots used


//////////////////////////////////////////////////////
Keywords: FOG_EXP2 LIGHTMAP_BICUBIC_SAMPLING _DBUFFER_MRT3 _SURFACE_TYPE_TRANSPARENT _WRITE_RENDERING_LAYERS
-- Vertex shader for "d3d11":
// No shader variant for this keyword set. The closest match will be used instead.

-- Hardware tier variant: Tier 1
-- Fragment shader for "d3d11":
// Stats: 77 math, 4 temp registers
Set 2D Texture "_CameraDepthTexture" to slot 0 sampler slot -1
Set 2D Texture "_MainTex" to slot 1
Set 2D Texture "_Noise" to slot 2
Set 2D Texture "_Flow" to slot 3
Set 2D Texture "_Mask" to slot 4

Set Sampler PointClamp to slot 0

Constant Buffer "$Globals" (2096 bytes) on slot 0 {
  Matrix4x4 unity_MatrixV at 1056
  Matrix4x4 unity_MatrixVP at 1248
  Vector4 _ScaledScreenParams at 48
  Vector2 _GlobalMipBias at 64
  ScalarInt _RenderingLayerMaxInt at 176
  Vector4 _TimeParameters at 304
  Vector3 _WorldSpaceCameraPos at 336
  Vector4 _ProjectionParams at 352
  Vector4 _ZBufferParams at 384
  Vector4 unity_OrthoParams at 400
  Vector4 _RTHandleScale at 448
  Vector4 _CameraDepthTexture_TexelSize at 2080
}
Constant Buffer "UnityPerDraw" (720 bytes) on slot 1 {
  Vector4 unity_RenderingLayer at 160
}
Constant Buffer "UnityPerMaterial" (304 bytes) on slot 2 {
  Float _Textureopacity at 0
  Float _Fresnelpower at 4
  Float _Fresnelscale at 8
  Float _Multiply_texture at 12
  Float _Useonlycolor at 16
  Float _Use_fresnel at 20
  Vector4 _MainTex_ST at 48
  Vector4 _Noise_ST at 80
  Vector4 _Flow_ST at 112
  Vector4 _Mask_ST at 144
  Vector4 _SpeedMainTexUVNoiseZW at 160
  Vector4 _DistortionSpeedXYPowerZ at 176
  Float _Emission at 192
  Float _Opacity at 224
  Float _Usecenterglow at 228
  Float _Usedepth at 232
  Float _Depthpower at 236
}

Shader Disassembly:
//
// Generated by Microsoft (R) D3D Shader Disassembler
//
//
// Input signature:
//
// Name                 Index   Mask Register SysValue  Format   Used
// -------------------- ----- ------ -------- -------- ------- ------
// SV_POSITION              0   xyzw        0      POS   float   xy  
// INTERP                   0   xyzw        1     NONE   float   xyzw
// INTERP                   1   xyzw        2     NONE   float   x   
// INTERP                   2   xyzw        3     NONE   float   xyzw
// INTERP                   3   xyz         4     NONE   float   xyz 
// INTERP                   4   xyz         5     NONE   float   xyz 
// SV_IsFrontFace           0   x           6    FFACE    uint   x   
//
//
// Output signature:
//
// Name                 Index   Mask Register SysValue  Format   Used
// -------------------- ----- ------ -------- -------- ------- ------
// SV_Target                0   xyzw        0   TARGET   float   xyzw
// SV_Target                1   x           1   TARGET    uint   x   
//
      ps_5_0
      dcl_globalFlags refactoringAllowed
      dcl_constantbuffer CB0[131], immediateIndexed
      dcl_constantbuffer CB1[11], immediateIndexed
      dcl_constantbuffer CB2[15], immediateIndexed
      dcl_sampler s0, mode_default
      dcl_sampler s1, mode_default
      dcl_sampler s2, mode_default
      dcl_sampler s3, mode_default
      dcl_sampler s4, mode_default
      dcl_resource_texture2d (float,float,float,float) t0
      dcl_resource_texture2d (float,float,float,float) t1
      dcl_resource_texture2d (float,float,float,float) t2
      dcl_resource_texture2d (float,float,float,float) t3
      dcl_resource_texture2d (float,float,float,float) t4
      dcl_input_ps_siv linear noperspective v0.xy, position
      dcl_input_ps linear v1.xyzw
      dcl_input_ps linear v2.x
      dcl_input_ps linear v3.xyzw
      dcl_input_ps linear v4.xyz
      dcl_input_ps linear v5.xyz
      dcl_input_ps_sgv constant v6.x, is_front_face
      dcl_output o0.xyzw
      dcl_output o1.x
      dcl_temps 4
   0: dp3 r0.x, v5.xyzx, v5.xyzx
   1: sqrt r0.x, r0.x
   2: div r0.x, l(1.000000, 1.000000, 1.000000, 1.000000), r0.x
   3: mul r0.xyz, r0.xxxx, v5.xyzx
   4: dp3 r0.w, r0.xyzx, r0.xyzx
   5: rsq r0.w, r0.w
   6: mul r0.xyz, r0.wwww, r0.xyzx
   7: add r1.xyz, -v4.xyzx, cb0[21].xyzx
   8: dp3 r0.w, r1.xyzx, r1.xyzx
   9: rsq r0.w, r0.w
  10: mul r1.xyz, r0.wwww, r1.xyzx
  11: eq r0.w, cb0[25].w, l(0.000000)
  12: movc r2.x, r0.w, r1.x, cb0[66].z
  13: movc r2.y, r0.w, r1.y, cb0[67].z
  14: movc r2.z, r0.w, r1.z, cb0[68].z
  15: dp3_sat r0.x, r0.xyzx, r2.xyzx
  16: add r0.x, -r0.x, l(1.000000)
  17: log r0.x, r0.x
  18: mul r0.x, r0.x, cb2[0].y
  19: exp r0.x, r0.x
  20: mul_sat r0.x, r0.x, cb2[0].z
  21: and r0.x, r0.x, v6.x
  22: lt r0.y, cb0[22].x, l(0.000000)
  23: add r0.z, -v0.y, cb0[3].y
  24: movc r1.y, r0.y, r0.z, v0.y
  25: mov r1.x, v0.x
  26: div r1.xy, r1.xyxx, cb0[3].xyxx
  27: add r1.z, -r1.y, l(1.000000)
  28: mad r0.yz, -cb0[130].xxyx, l(0.000000, 0.500000, 0.500000, 0.000000), l(0.000000, 1.000000, 1.000000, 0.000000)
  29: min r0.yz, r0.yyzy, r1.xxzx
  30: mul r0.yz, r0.yyzy, cb0[28].xxyx
  31: sample_b_indexable(texture2d)(float,float,float,float) r0.y, r0.yzyy, t0.yxzw, s0, cb0[4].x
  32: mad r0.y, cb0[24].x, r0.y, cb0[24].y
  33: div r0.y, l(1.000000, 1.000000, 1.000000, 1.000000), r0.y
  34: mul r0.z, v4.y, cb0[79].w
  35: mad r0.z, cb0[78].w, v4.x, r0.z
  36: mad r0.z, cb0[80].w, v4.z, r0.z
  37: add r0.z, r0.z, cb0[81].w
  38: mad r0.y, r0.y, cb0[22].z, -r0.z
  39: div_sat r0.y, r0.y, cb2[14].w
  40: ge r0.z, l(0.000000), r0.y
  41: and r0.z, r0.z, l(0x3f800000)
  42: add r0.w, -r0.y, l(1.000000)
  43: mad r0.y, r0.z, r0.w, r0.y
  44: add r0.x, -r0.y, r0.x
  45: add r0.x, r0.x, l(1.000000)
  46: mul r0.z, v1.w, l(1.000100)
  47: ge r0.w, r0.z, -r0.z
  48: frc r0.z, |r0.z|
  49: movc r0.z, r0.w, r0.z, -r0.z
  50: mul r0.z, r0.z, l(0.999900)
  51: div r1.xy, cb2[10].zwzz, cb2[5].xyxx
  52: mad r0.zw, cb0[19].xxxx, r1.xxxy, r0.zzzz
  53: add r0.zw, r0.zzzw, v1.xxxy
  54: mad r0.zw, r0.zzzw, cb2[5].xxxy, cb2[5].zzzw
  55: sample_b_indexable(texture2d)(float,float,float,float) r1.xyzw, r0.zwzz, t2.xyzw, s2, cb0[4].x
  56: div r0.zw, cb2[11].xxxy, cb2[7].xxxy
  57: mad r0.zw, r0.zzzw, cb0[19].xxxx, v1.xxxy
  58: mad r0.zw, r0.zzzw, cb2[7].xxxy, cb2[7].zzzw
  59: sample_b_indexable(texture2d)(float,float,float,float) r0.zw, r0.zwzz, t3.zwxy, s3, cb0[4].x
  60: mad r2.xy, v1.xyxx, cb2[9].xyxx, cb2[9].zwzz
  61: sample_b_indexable(texture2d)(float,float,float,float) r2.xyz, r2.xyxx, t4.xyzw, s4, cb0[4].x
  62: mul r0.zw, r0.zzzw, r2.xxxy
  63: mul r0.zw, r0.zzzw, cb2[11].zzzz
  64: div r0.zw, r0.zzzw, cb2[3].xxxy
  65: div r3.xy, cb2[10].xyxx, cb2[3].xyxx
  66: mad r3.xy, r3.xyxx, cb0[19].xxxx, v1.xyxx
  67: add r0.zw, -r0.zzzw, r3.xxxy
  68: mad r0.zw, r0.zzzw, cb2[3].xxxy, cb2[3].zzzw
  69: sample_b_indexable(texture2d)(float,float,float,float) r3.xyzw, r0.zwzz, t1.xyzw, s1, cb0[4].x
  70: mul r1.xyzw, r1.wxyz, r3.wxyz
  71: ne r0.zw, l(0.000000, 0.000000, 0.000000, 0.000000), cb2[1].xxxy
  72: movc r3.xyz, r0.zzzz, r1.xxxx, r1.yzwy
  73: max r3.xyz, r0.xxxx, r3.xyzx
  74: mad_sat r0.x, r1.x, cb2[0].x, r0.x
  75: movc r1.yzw, r0.wwww, r3.xxyz, r1.yyzw
  76: add r0.z, -v1.z, l(1.000000)
  77: add_sat r3.xyz, -r0.zzzz, r2.xyzx
  78: mul_sat r2.xyz, r2.xyzx, r3.xyzx
  79: mul r2.xyz, r1.yzwy, r2.xyzx
  80: ne r3.xy, l(0.000000, 0.000000, 0.000000, 0.000000), cb2[14].yzyy
  81: movc r1.yzw, r3.xxxx, r2.xxyz, r1.yyzw
  82: mul r1.yzw, r1.yyzw, cb2[12].xxxx
  83: mul r1.yzw, r1.yyzw, v3.xxyz
  84: max r0.z, v2.x, l(1.000000)
  85: min r0.z, r0.z, l(1000000.000000)
  86: eq r2.x, v1.x, v2.x
  87: movc r0.z, r2.x, l(1.000000), r0.z
  88: mul o0.xyz, r1.yzwy, r0.zzzz
  89: mul r0.z, r0.x, r1.x
  90: ne r1.y, l(0.000000, 0.000000, 0.000000, 0.000000), cb2[0].w
  91: movc r0.x, r1.y, r0.z, r0.x
  92: movc r0.x, r0.w, r0.x, r1.x
  93: mul r0.y, r0.y, r0.x
  94: movc r0.x, r3.y, r0.y, r0.x
  95: mul r0.x, r0.x, v3.w
  96: mul_sat o0.w, r0.x, cb2[14].x
  97: and o1.x, cb0[11].x, cb1[10].x
  98: ret 
// Approximately 0 instruction slots used


//////////////////////////////////////////////////////
Keywords: LIGHTMAP_BICUBIC_SAMPLING _DBUFFER_MRT3 _SCREEN_SPACE_OCCLUSION
-- Vertex shader for "d3d11":
// No shader variant for this keyword set. The closest match will be used instead.

-- Hardware tier variant: Tier 1
-- Fragment shader for "d3d11":
// Stats: 81 math, 4 temp registers
Set 2D Texture "_ScreenSpaceOcclusionTexture" to slot 0 sampler slot -1
Set 2D Texture "_DBufferTexture0" to slot 1 sampler slot -1
Set 2D Texture "_CameraDepthTexture" to slot 2 sampler slot -1
Set 2D Texture "_MainTex" to slot 3 sampler slot 2
Set 2D Texture "_Noise" to slot 4 sampler slot 3
Set 2D Texture "_Flow" to slot 5 sampler slot 4
Set 2D Texture "_Mask" to slot 6 sampler slot 5

Set Sampler PointClamp to slot 0
Set Sampler LinearClamp to slot 1

Constant Buffer "$Globals" (2096 bytes) on slot 0 {
  Matrix4x4 unity_MatrixV at 1056
  Matrix4x4 unity_MatrixVP at 1248
  Vector4 _ScaledScreenParams at 48
  Vector2 _GlobalMipBias at 64
  Vector4 _AmbientOcclusionParam at 144
  Vector4 _TimeParameters at 304
  Vector3 _WorldSpaceCameraPos at 336
  Vector4 _ProjectionParams at 352
  Vector4 _ZBufferParams at 384
  Vector4 unity_OrthoParams at 400
  Vector4 _ScaleBiasRt at 432
  Vector4 _RTHandleScale at 448
  Vector4 _CameraDepthTexture_TexelSize at 2080
}
Constant Buffer "UnityPerMaterial" (304 bytes) on slot 1 {
  Float _Fresnelpower at 4
  Float _Fresnelscale at 8
  Float _Useonlycolor at 16
  Float _Use_fresnel at 20
  Vector4 _MainTex_ST at 48
  Vector4 _Noise_ST at 80
  Vector4 _Flow_ST at 112
  Vector4 _Mask_ST at 144
  Vector4 _SpeedMainTexUVNoiseZW at 160
  Vector4 _DistortionSpeedXYPowerZ at 176
  Float _Emission at 192
  Float _Usecenterglow at 228
  Float _Depthpower at 236
}

Shader Disassembly:
//
// Generated by Microsoft (R) D3D Shader Disassembler
//
//
// Input signature:
//
// Name                 Index   Mask Register SysValue  Format   Used
// -------------------- ----- ------ -------- -------- ------- ------
// SV_POSITION              0   xyzw        0      POS   float   xy  
// INTERP                   0   xyzw        1     NONE   float   xyzw
// INTERP                   1   xyzw        2     NONE   float   x   
// INTERP                   2   xyzw        3     NONE   float   xyz 
// INTERP                   3   xyz         4     NONE   float   xyz 
// INTERP                   4   xyz         5     NONE   float   xyz 
// SV_IsFrontFace           0   x           6    FFACE    uint   x   
//
//
// Output signature:
//
// Name                 Index   Mask Register SysValue  Format   Used
// -------------------- ----- ------ -------- -------- ------- ------
// SV_Target                0   xyzw        0   TARGET   float   xyzw
//
      ps_4_0
      dcl_constantbuffer CB0[131], immediateIndexed
      dcl_constantbuffer CB1[15], immediateIndexed
      dcl_sampler s0, mode_default
      dcl_sampler s1, mode_default
      dcl_sampler s2, mode_default
      dcl_sampler s3, mode_default
      dcl_sampler s4, mode_default
      dcl_sampler s5, mode_default
      dcl_resource_texture2d (float,float,float,float) t0
      dcl_resource_texture2d (float,float,float,float) t1
      dcl_resource_texture2d (float,float,float,float) t2
      dcl_resource_texture2d (float,float,float,float) t3
      dcl_resource_texture2d (float,float,float,float) t4
      dcl_resource_texture2d (float,float,float,float) t5
      dcl_resource_texture2d (float,float,float,float) t6
      dcl_input_ps_siv linear noperspective v0.xy, position
      dcl_input_ps linear v1.xyzw
      dcl_input_ps linear v2.x
      dcl_input_ps linear v3.xyz
      dcl_input_ps linear v4.xyz
      dcl_input_ps linear v5.xyz
      dcl_input_ps_sgv constant v6.x, is_front_face
      dcl_output o0.xyzw
      dcl_temps 4
   0: dp3 r0.x, v5.xyzx, v5.xyzx
   1: sqrt r0.x, r0.x
   2: div r0.x, l(1.000000, 1.000000, 1.000000, 1.000000), r0.x
   3: mul r0.xyz, r0.xxxx, v5.xyzx
   4: dp3 r0.w, r0.xyzx, r0.xyzx
   5: rsq r0.w, r0.w
   6: mul r0.xyz, r0.wwww, r0.xyzx
   7: add r1.xyz, -v4.xyzx, cb0[21].xyzx
   8: dp3 r0.w, r1.xyzx, r1.xyzx
   9: rsq r0.w, r0.w
  10: mul r1.xyz, r0.wwww, r1.xyzx
  11: eq r0.w, cb0[25].w, l(0.000000)
  12: movc r2.x, r0.w, r1.x, cb0[66].z
  13: movc r2.y, r0.w, r1.y, cb0[67].z
  14: movc r2.z, r0.w, r1.z, cb0[68].z
  15: dp3_sat r0.x, r0.xyzx, r2.xyzx
  16: add r0.x, -r0.x, l(1.000000)
  17: log r0.x, r0.x
  18: mul r0.x, r0.x, cb1[0].y
  19: exp r0.x, r0.x
  20: mul_sat r0.x, r0.x, cb1[0].z
  21: and r0.x, r0.x, v6.x
  22: lt r0.y, cb0[22].x, l(0.000000)
  23: add r0.z, -v0.y, cb0[3].y
  24: movc r1.y, r0.y, r0.z, v0.y
  25: mov r1.x, v0.x
  26: div r1.xy, r1.xyxx, cb0[3].xyxx
  27: add r1.z, -r1.y, l(1.000000)
  28: mad r0.yz, -cb0[130].xxyx, l(0.000000, 0.500000, 0.500000, 0.000000), l(0.000000, 1.000000, 1.000000, 0.000000)
  29: min r0.yz, r0.yyzy, r1.xxzx
  30: mul r0.yz, r0.yyzy, cb0[28].xxyx
  31: sample_b r1.xyzw, r0.yzyy, t2.xyzw, s0, cb0[4].x
  32: mad r0.y, cb0[24].x, r1.x, cb0[24].y
  33: div r0.y, l(1.000000, 1.000000, 1.000000, 1.000000), r0.y
  34: mul r0.z, v4.y, cb0[79].w
  35: mad r0.z, cb0[78].w, v4.x, r0.z
  36: mad r0.z, cb0[80].w, v4.z, r0.z
  37: add r0.z, r0.z, cb0[81].w
  38: mad r0.y, r0.y, cb0[22].z, -r0.z
  39: div_sat r0.y, r0.y, cb1[14].w
  40: ge r0.z, l(0.000000), r0.y
  41: and r0.z, r0.z, l(0x3f800000)
  42: add r0.w, -r0.y, l(1.000000)
  43: mad r0.y, r0.z, r0.w, r0.y
  44: add r0.x, -r0.y, r0.x
  45: add r0.x, r0.x, l(1.000000)
  46: mul r0.y, v1.w, l(1.000100)
  47: ge r0.z, r0.y, -r0.y
  48: frc r0.y, |r0.y|
  49: movc r0.y, r0.z, r0.y, -r0.y
  50: mul r0.y, r0.y, l(0.999900)
  51: div r0.zw, cb1[10].zzzw, cb1[5].xxxy
  52: mad r0.yz, cb0[19].xxxx, r0.zzwz, r0.yyyy
  53: add r0.yz, r0.yyzy, v1.xxyx
  54: mad r0.yz, r0.yyzy, cb1[5].xxyx, cb1[5].zzwz
  55: sample_b r1.xyzw, r0.yzyy, t4.xyzw, s3, cb0[4].x
  56: div r0.yz, cb1[11].xxyx, cb1[7].xxyx
  57: mad r0.yz, r0.yyzy, cb0[19].xxxx, v1.xxyx
  58: mad r0.yz, r0.yyzy, cb1[7].xxyx, cb1[7].zzwz
  59: sample_b r2.xyzw, r0.yzyy, t5.xyzw, s4, cb0[4].x
  60: mad r0.yz, v1.xxyx, cb1[9].xxyx, cb1[9].zzwz
  61: sample_b r3.xyzw, r0.yzyy, t6.xyzw, s5, cb0[4].x
  62: mul r0.yz, r2.xxyx, r3.xxyx
  63: mul r0.yz, r0.yyzy, cb1[11].zzzz
  64: div r0.yz, r0.yyzy, cb1[3].xxyx
  65: div r2.xy, cb1[10].xyxx, cb1[3].xyxx
  66: mad r2.xy, r2.xyxx, cb0[19].xxxx, v1.xyxx
  67: add r0.yz, -r0.yyzy, r2.xxyx
  68: mad r0.yz, r0.yyzy, cb1[3].xxyx, cb1[3].zzwz
  69: sample_b r2.xyzw, r0.yzyy, t3.xyzw, s2, cb0[4].x
  70: mul r1.xyzw, r1.wxyz, r2.wxyz
  71: ne r0.yz, l(0.000000, 0.000000, 0.000000, 0.000000), cb1[1].xxyx
  72: movc r2.xyz, r0.yyyy, r1.xxxx, r1.yzwy
  73: max r0.xyw, r0.xxxx, r2.xyxz
  74: movc r0.xyz, r0.zzzz, r0.xywx, r1.yzwy
  75: add r0.w, -v1.z, l(1.000000)
  76: add_sat r1.xyz, -r0.wwww, r3.xyzx
  77: mul_sat r1.xyz, r1.xyzx, r3.xyzx
  78: mul r1.xyz, r0.xyzx, r1.xyzx
  79: ne r0.w, l(0.000000, 0.000000, 0.000000, 0.000000), cb1[14].y
  80: movc r0.xyz, r0.wwww, r1.xyzx, r0.xyzx
  81: mul r0.xyz, r0.xyzx, cb1[12].xxxx
  82: mul r0.xyz, r0.xyzx, v3.xyzx
  83: max r0.w, v2.x, l(1.000000)
  84: min r0.w, r0.w, l(1000000.000000)
  85: eq r1.x, v1.x, v2.x
  86: movc r0.w, r1.x, l(1.000000), r0.w
  87: mul r0.xyz, r0.xyzx, r0.wwww
  88: ftoi r1.xy, v0.xyxx
  89: mov r1.zw, l(0,0,0,0)
  90: ld r1.xyzw, r1.xyzw, t1.xyzw
  91: mad r0.xyz, r0.xyzx, r1.wwww, r1.xyzx
  92: div r1.xy, l(1.000000, 1.000000, 1.000000, 1.000000), cb0[3].xyxx
  93: mul r1.xy, r1.xyxx, v0.xyxx
  94: mad r0.w, r1.y, cb0[27].x, cb0[27].y
  95: add r1.z, -r0.w, l(1.000000)
  96: sample_b r1.xyzw, r1.xzxx, t0.xyzw, s1, cb0[4].x
  97: add r0.w, -cb0[9].x, l(1.000000)
  98: add_sat r0.w, r0.w, r1.x
  99: add r0.w, r0.w, l(-1.000000)
 100: mad r0.w, cb0[9].w, r0.w, l(1.000000)
 101: mul o0.xyz, r0.wwww, r0.xyzx
 102: mov o0.w, l(1.000000)
 103: ret 
// Approximately 0 instruction slots used


//////////////////////////////////////////////////////
Keywords: LIGHTMAP_BICUBIC_SAMPLING _DBUFFER_MRT3 _SCREEN_SPACE_OCCLUSION _SURFACE_TYPE_TRANSPARENT
-- Vertex shader for "d3d11":
// No shader variant for this keyword set. The closest match will be used instead.

-- Hardware tier variant: Tier 1
-- Fragment shader for "d3d11":
// Stats: 76 math, 4 temp registers
Set 2D Texture "_CameraDepthTexture" to slot 0 sampler slot -1
Set 2D Texture "_MainTex" to slot 1
Set 2D Texture "_Noise" to slot 2
Set 2D Texture "_Flow" to slot 3
Set 2D Texture "_Mask" to slot 4

Set Sampler PointClamp to slot 0

Constant Buffer "$Globals" (2096 bytes) on slot 0 {
  Matrix4x4 unity_MatrixV at 1056
  Matrix4x4 unity_MatrixVP at 1248
  Vector4 _ScaledScreenParams at 48
  Vector2 _GlobalMipBias at 64
  Vector4 _TimeParameters at 304
  Vector3 _WorldSpaceCameraPos at 336
  Vector4 _ProjectionParams at 352
  Vector4 _ZBufferParams at 384
  Vector4 unity_OrthoParams at 400
  Vector4 _RTHandleScale at 448
  Vector4 _CameraDepthTexture_TexelSize at 2080
}
Constant Buffer "UnityPerMaterial" (304 bytes) on slot 1 {
  Float _Textureopacity at 0
  Float _Fresnelpower at 4
  Float _Fresnelscale at 8
  Float _Multiply_texture at 12
  Float _Useonlycolor at 16
  Float _Use_fresnel at 20
  Vector4 _MainTex_ST at 48
  Vector4 _Noise_ST at 80
  Vector4 _Flow_ST at 112
  Vector4 _Mask_ST at 144
  Vector4 _SpeedMainTexUVNoiseZW at 160
  Vector4 _DistortionSpeedXYPowerZ at 176
  Float _Emission at 192
  Float _Opacity at 224
  Float _Usecenterglow at 228
  Float _Usedepth at 232
  Float _Depthpower at 236
}

Shader Disassembly:
//
// Generated by Microsoft (R) D3D Shader Disassembler
//
//
// Input signature:
//
// Name                 Index   Mask Register SysValue  Format   Used
// -------------------- ----- ------ -------- -------- ------- ------
// SV_POSITION              0   xyzw        0      POS   float   xy  
// INTERP                   0   xyzw        1     NONE   float   xyzw
// INTERP                   1   xyzw        2     NONE   float   x   
// INTERP                   2   xyzw        3     NONE   float   xyzw
// INTERP                   3   xyz         4     NONE   float   xyz 
// INTERP                   4   xyz         5     NONE   float   xyz 
// SV_IsFrontFace           0   x           6    FFACE    uint   x   
//
//
// Output signature:
//
// Name                 Index   Mask Register SysValue  Format   Used
// -------------------- ----- ------ -------- -------- ------- ------
// SV_Target                0   xyzw        0   TARGET   float   xyzw
//
      ps_4_0
      dcl_constantbuffer CB0[131], immediateIndexed
      dcl_constantbuffer CB1[15], immediateIndexed
      dcl_sampler s0, mode_default
      dcl_sampler s1, mode_default
      dcl_sampler s2, mode_default
      dcl_sampler s3, mode_default
      dcl_sampler s4, mode_default
      dcl_resource_texture2d (float,float,float,float) t0
      dcl_resource_texture2d (float,float,float,float) t1
      dcl_resource_texture2d (float,float,float,float) t2
      dcl_resource_texture2d (float,float,float,float) t3
      dcl_resource_texture2d (float,float,float,float) t4
      dcl_input_ps_siv linear noperspective v0.xy, position
      dcl_input_ps linear v1.xyzw
      dcl_input_ps linear v2.x
      dcl_input_ps linear v3.xyzw
      dcl_input_ps linear v4.xyz
      dcl_input_ps linear v5.xyz
      dcl_input_ps_sgv constant v6.x, is_front_face
      dcl_output o0.xyzw
      dcl_temps 4
   0: dp3 r0.x, v5.xyzx, v5.xyzx
   1: sqrt r0.x, r0.x
   2: div r0.x, l(1.000000, 1.000000, 1.000000, 1.000000), r0.x
   3: mul r0.xyz, r0.xxxx, v5.xyzx
   4: dp3 r0.w, r0.xyzx, r0.xyzx
   5: rsq r0.w, r0.w
   6: mul r0.xyz, r0.wwww, r0.xyzx
   7: add r1.xyz, -v4.xyzx, cb0[21].xyzx
   8: dp3 r0.w, r1.xyzx, r1.xyzx
   9: rsq r0.w, r0.w
  10: mul r1.xyz, r0.wwww, r1.xyzx
  11: eq r0.w, cb0[25].w, l(0.000000)
  12: movc r2.x, r0.w, r1.x, cb0[66].z
  13: movc r2.y, r0.w, r1.y, cb0[67].z
  14: movc r2.z, r0.w, r1.z, cb0[68].z
  15: dp3_sat r0.x, r0.xyzx, r2.xyzx
  16: add r0.x, -r0.x, l(1.000000)
  17: log r0.x, r0.x
  18: mul r0.x, r0.x, cb1[0].y
  19: exp r0.x, r0.x
  20: mul_sat r0.x, r0.x, cb1[0].z
  21: and r0.x, r0.x, v6.x
  22: lt r0.y, cb0[22].x, l(0.000000)
  23: add r0.z, -v0.y, cb0[3].y
  24: movc r1.y, r0.y, r0.z, v0.y
  25: mov r1.x, v0.x
  26: div r1.xy, r1.xyxx, cb0[3].xyxx
  27: add r1.z, -r1.y, l(1.000000)
  28: mad r0.yz, -cb0[130].xxyx, l(0.000000, 0.500000, 0.500000, 0.000000), l(0.000000, 1.000000, 1.000000, 0.000000)
  29: min r0.yz, r0.yyzy, r1.xxzx
  30: mul r0.yz, r0.yyzy, cb0[28].xxyx
  31: sample_b r1.xyzw, r0.yzyy, t0.xyzw, s0, cb0[4].x
  32: mad r0.y, cb0[24].x, r1.x, cb0[24].y
  33: div r0.y, l(1.000000, 1.000000, 1.000000, 1.000000), r0.y
  34: mul r0.z, v4.y, cb0[79].w
  35: mad r0.z, cb0[78].w, v4.x, r0.z
  36: mad r0.z, cb0[80].w, v4.z, r0.z
  37: add r0.z, r0.z, cb0[81].w
  38: mad r0.y, r0.y, cb0[22].z, -r0.z
  39: div_sat r0.y, r0.y, cb1[14].w
  40: ge r0.z, l(0.000000), r0.y
  41: and r0.z, r0.z, l(0x3f800000)
  42: add r0.w, -r0.y, l(1.000000)
  43: mad r0.y, r0.z, r0.w, r0.y
  44: add r0.x, -r0.y, r0.x
  45: add r0.x, r0.x, l(1.000000)
  46: mul r0.z, v1.w, l(1.000100)
  47: ge r0.w, r0.z, -r0.z
  48: frc r0.z, |r0.z|
  49: movc r0.z, r0.w, r0.z, -r0.z
  50: mul r0.z, r0.z, l(0.999900)
  51: div r1.xy, cb1[10].zwzz, cb1[5].xyxx
  52: mad r0.zw, cb0[19].xxxx, r1.xxxy, r0.zzzz
  53: add r0.zw, r0.zzzw, v1.xxxy
  54: mad r0.zw, r0.zzzw, cb1[5].xxxy, cb1[5].zzzw
  55: sample_b r1.xyzw, r0.zwzz, t2.xyzw, s2, cb0[4].x
  56: div r0.zw, cb1[11].xxxy, cb1[7].xxxy
  57: mad r0.zw, r0.zzzw, cb0[19].xxxx, v1.xxxy
  58: mad r0.zw, r0.zzzw, cb1[7].xxxy, cb1[7].zzzw
  59: sample_b r2.xyzw, r0.zwzz, t3.xyzw, s3, cb0[4].x
  60: mad r0.zw, v1.xxxy, cb1[9].xxxy, cb1[9].zzzw
  61: sample_b r3.xyzw, r0.zwzz, t4.xyzw, s4, cb0[4].x
  62: mul r0.zw, r2.xxxy, r3.xxxy
  63: mul r0.zw, r0.zzzw, cb1[11].zzzz
  64: div r0.zw, r0.zzzw, cb1[3].xxxy
  65: div r2.xy, cb1[10].xyxx, cb1[3].xyxx
  66: mad r2.xy, r2.xyxx, cb0[19].xxxx, v1.xyxx
  67: add r0.zw, -r0.zzzw, r2.xxxy
  68: mad r0.zw, r0.zzzw, cb1[3].xxxy, cb1[3].zzzw
  69: sample_b r2.xyzw, r0.zwzz, t1.xyzw, s1, cb0[4].x
  70: mul r1.xyzw, r1.wxyz, r2.wxyz
  71: ne r0.zw, l(0.000000, 0.000000, 0.000000, 0.000000), cb1[1].xxxy
  72: movc r2.xyz, r0.zzzz, r1.xxxx, r1.yzwy
  73: max r2.xyz, r0.xxxx, r2.xyzx
  74: mad_sat r0.x, r1.x, cb1[0].x, r0.x
  75: movc r1.yzw, r0.wwww, r2.xxyz, r1.yyzw
  76: add r0.z, -v1.z, l(1.000000)
  77: add_sat r2.xyz, -r0.zzzz, r3.xyzx
  78: mul_sat r2.xyz, r2.xyzx, r3.xyzx
  79: mul r2.xyz, r1.yzwy, r2.xyzx
  80: ne r3.xy, l(0.000000, 0.000000, 0.000000, 0.000000), cb1[14].yzyy
  81: movc r1.yzw, r3.xxxx, r2.xxyz, r1.yyzw
  82: mul r1.yzw, r1.yyzw, cb1[12].xxxx
  83: mul r1.yzw, r1.yyzw, v3.xxyz
  84: max r0.z, v2.x, l(1.000000)
  85: min r0.z, r0.z, l(1000000.000000)
  86: eq r2.x, v1.x, v2.x
  87: movc r0.z, r2.x, l(1.000000), r0.z
  88: mul o0.xyz, r1.yzwy, r0.zzzz
  89: mul r0.z, r0.x, r1.x
  90: ne r1.y, l(0.000000, 0.000000, 0.000000, 0.000000), cb1[0].w
  91: movc r0.x, r1.y, r0.z, r0.x
  92: movc r0.x, r0.w, r0.x, r1.x
  93: mul r0.y, r0.y, r0.x
  94: movc r0.x, r3.y, r0.y, r0.x
  95: mul r0.x, r0.x, v3.w
  96: mul_sat o0.w, r0.x, cb1[14].x
  97: ret 
// Approximately 0 instruction slots used


//////////////////////////////////////////////////////
Keywords: FOG_EXP2 LIGHTMAP_BICUBIC_SAMPLING _DBUFFER_MRT3 _SCREEN_SPACE_OCCLUSION
-- Vertex shader for "d3d11":
// No shader variant for this keyword set. The closest match will be used instead.

-- Hardware tier variant: Tier 1
-- Fragment shader for "d3d11":
// Stats: 81 math, 4 temp registers
Set 2D Texture "_ScreenSpaceOcclusionTexture" to slot 0 sampler slot -1
Set 2D Texture "_DBufferTexture0" to slot 1 sampler slot -1
Set 2D Texture "_CameraDepthTexture" to slot 2 sampler slot -1
Set 2D Texture "_MainTex" to slot 3 sampler slot 2
Set 2D Texture "_Noise" to slot 4 sampler slot 3
Set 2D Texture "_Flow" to slot 5 sampler slot 4
Set 2D Texture "_Mask" to slot 6 sampler slot 5

Set Sampler PointClamp to slot 0
Set Sampler LinearClamp to slot 1

Constant Buffer "$Globals" (2096 bytes) on slot 0 {
  Matrix4x4 unity_MatrixV at 1056
  Matrix4x4 unity_MatrixVP at 1248
  Vector4 _ScaledScreenParams at 48
  Vector2 _GlobalMipBias at 64
  Vector4 _AmbientOcclusionParam at 144
  Vector4 _TimeParameters at 304
  Vector3 _WorldSpaceCameraPos at 336
  Vector4 _ProjectionParams at 352
  Vector4 _ZBufferParams at 384
  Vector4 unity_OrthoParams at 400
  Vector4 _ScaleBiasRt at 432
  Vector4 _RTHandleScale at 448
  Vector4 _CameraDepthTexture_TexelSize at 2080
}
Constant Buffer "UnityPerMaterial" (304 bytes) on slot 1 {
  Float _Fresnelpower at 4
  Float _Fresnelscale at 8
  Float _Useonlycolor at 16
  Float _Use_fresnel at 20
  Vector4 _MainTex_ST at 48
  Vector4 _Noise_ST at 80
  Vector4 _Flow_ST at 112
  Vector4 _Mask_ST at 144
  Vector4 _SpeedMainTexUVNoiseZW at 160
  Vector4 _DistortionSpeedXYPowerZ at 176
  Float _Emission at 192
  Float _Usecenterglow at 228
  Float _Depthpower at 236
}

Shader Disassembly:
//
// Generated by Microsoft (R) D3D Shader Disassembler
//
//
// Input signature:
//
// Name                 Index   Mask Register SysValue  Format   Used
// -------------------- ----- ------ -------- -------- ------- ------
// SV_POSITION              0   xyzw        0      POS   float   xy  
// INTERP                   0   xyzw        1     NONE   float   xyzw
// INTERP                   1   xyzw        2     NONE   float   x   
// INTERP                   2   xyzw        3     NONE   float   xyz 
// INTERP                   3   xyz         4     NONE   float   xyz 
// INTERP                   4   xyz         5     NONE   float   xyz 
// SV_IsFrontFace           0   x           6    FFACE    uint   x   
//
//
// Output signature:
//
// Name                 Index   Mask Register SysValue  Format   Used
// -------------------- ----- ------ -------- -------- ------- ------
// SV_Target                0   xyzw        0   TARGET   float   xyzw
//
      ps_4_0
      dcl_constantbuffer CB0[131], immediateIndexed
      dcl_constantbuffer CB1[15], immediateIndexed
      dcl_sampler s0, mode_default
      dcl_sampler s1, mode_default
      dcl_sampler s2, mode_default
      dcl_sampler s3, mode_default
      dcl_sampler s4, mode_default
      dcl_sampler s5, mode_default
      dcl_resource_texture2d (float,float,float,float) t0
      dcl_resource_texture2d (float,float,float,float) t1
      dcl_resource_texture2d (float,float,float,float) t2
      dcl_resource_texture2d (float,float,float,float) t3
      dcl_resource_texture2d (float,float,float,float) t4
      dcl_resource_texture2d (float,float,float,float) t5
      dcl_resource_texture2d (float,float,float,float) t6
      dcl_input_ps_siv linear noperspective v0.xy, position
      dcl_input_ps linear v1.xyzw
      dcl_input_ps linear v2.x
      dcl_input_ps linear v3.xyz
      dcl_input_ps linear v4.xyz
      dcl_input_ps linear v5.xyz
      dcl_input_ps_sgv constant v6.x, is_front_face
      dcl_output o0.xyzw
      dcl_temps 4
   0: dp3 r0.x, v5.xyzx, v5.xyzx
   1: sqrt r0.x, r0.x
   2: div r0.x, l(1.000000, 1.000000, 1.000000, 1.000000), r0.x
   3: mul r0.xyz, r0.xxxx, v5.xyzx
   4: dp3 r0.w, r0.xyzx, r0.xyzx
   5: rsq r0.w, r0.w
   6: mul r0.xyz, r0.wwww, r0.xyzx
   7: add r1.xyz, -v4.xyzx, cb0[21].xyzx
   8: dp3 r0.w, r1.xyzx, r1.xyzx
   9: rsq r0.w, r0.w
  10: mul r1.xyz, r0.wwww, r1.xyzx
  11: eq r0.w, cb0[25].w, l(0.000000)
  12: movc r2.x, r0.w, r1.x, cb0[66].z
  13: movc r2.y, r0.w, r1.y, cb0[67].z
  14: movc r2.z, r0.w, r1.z, cb0[68].z
  15: dp3_sat r0.x, r0.xyzx, r2.xyzx
  16: add r0.x, -r0.x, l(1.000000)
  17: log r0.x, r0.x
  18: mul r0.x, r0.x, cb1[0].y
  19: exp r0.x, r0.x
  20: mul_sat r0.x, r0.x, cb1[0].z
  21: and r0.x, r0.x, v6.x
  22: lt r0.y, cb0[22].x, l(0.000000)
  23: add r0.z, -v0.y, cb0[3].y
  24: movc r1.y, r0.y, r0.z, v0.y
  25: mov r1.x, v0.x
  26: div r1.xy, r1.xyxx, cb0[3].xyxx
  27: add r1.z, -r1.y, l(1.000000)
  28: mad r0.yz, -cb0[130].xxyx, l(0.000000, 0.500000, 0.500000, 0.000000), l(0.000000, 1.000000, 1.000000, 0.000000)
  29: min r0.yz, r0.yyzy, r1.xxzx
  30: mul r0.yz, r0.yyzy, cb0[28].xxyx
  31: sample_b r1.xyzw, r0.yzyy, t2.xyzw, s0, cb0[4].x
  32: mad r0.y, cb0[24].x, r1.x, cb0[24].y
  33: div r0.y, l(1.000000, 1.000000, 1.000000, 1.000000), r0.y
  34: mul r0.z, v4.y, cb0[79].w
  35: mad r0.z, cb0[78].w, v4.x, r0.z
  36: mad r0.z, cb0[80].w, v4.z, r0.z
  37: add r0.z, r0.z, cb0[81].w
  38: mad r0.y, r0.y, cb0[22].z, -r0.z
  39: div_sat r0.y, r0.y, cb1[14].w
  40: ge r0.z, l(0.000000), r0.y
  41: and r0.z, r0.z, l(0x3f800000)
  42: add r0.w, -r0.y, l(1.000000)
  43: mad r0.y, r0.z, r0.w, r0.y
  44: add r0.x, -r0.y, r0.x
  45: add r0.x, r0.x, l(1.000000)
  46: mul r0.y, v1.w, l(1.000100)
  47: ge r0.z, r0.y, -r0.y
  48: frc r0.y, |r0.y|
  49: movc r0.y, r0.z, r0.y, -r0.y
  50: mul r0.y, r0.y, l(0.999900)
  51: div r0.zw, cb1[10].zzzw, cb1[5].xxxy
  52: mad r0.yz, cb0[19].xxxx, r0.zzwz, r0.yyyy
  53: add r0.yz, r0.yyzy, v1.xxyx
  54: mad r0.yz, r0.yyzy, cb1[5].xxyx, cb1[5].zzwz
  55: sample_b r1.xyzw, r0.yzyy, t4.xyzw, s3, cb0[4].x
  56: div r0.yz, cb1[11].xxyx, cb1[7].xxyx
  57: mad r0.yz, r0.yyzy, cb0[19].xxxx, v1.xxyx
  58: mad r0.yz, r0.yyzy, cb1[7].xxyx, cb1[7].zzwz
  59: sample_b r2.xyzw, r0.yzyy, t5.xyzw, s4, cb0[4].x
  60: mad r0.yz, v1.xxyx, cb1[9].xxyx, cb1[9].zzwz
  61: sample_b r3.xyzw, r0.yzyy, t6.xyzw, s5, cb0[4].x
  62: mul r0.yz, r2.xxyx, r3.xxyx
  63: mul r0.yz, r0.yyzy, cb1[11].zzzz
  64: div r0.yz, r0.yyzy, cb1[3].xxyx
  65: div r2.xy, cb1[10].xyxx, cb1[3].xyxx
  66: mad r2.xy, r2.xyxx, cb0[19].xxxx, v1.xyxx
  67: add r0.yz, -r0.yyzy, r2.xxyx
  68: mad r0.yz, r0.yyzy, cb1[3].xxyx, cb1[3].zzwz
  69: sample_b r2.xyzw, r0.yzyy, t3.xyzw, s2, cb0[4].x
  70: mul r1.xyzw, r1.wxyz, r2.wxyz
  71: ne r0.yz, l(0.000000, 0.000000, 0.000000, 0.000000), cb1[1].xxyx
  72: movc r2.xyz, r0.yyyy, r1.xxxx, r1.yzwy
  73: max r0.xyw, r0.xxxx, r2.xyxz
  74: movc r0.xyz, r0.zzzz, r0.xywx, r1.yzwy
  75: add r0.w, -v1.z, l(1.000000)
  76: add_sat r1.xyz, -r0.wwww, r3.xyzx
  77: mul_sat r1.xyz, r1.xyzx, r3.xyzx
  78: mul r1.xyz, r0.xyzx, r1.xyzx
  79: ne r0.w, l(0.000000, 0.000000, 0.000000, 0.000000), cb1[14].y
  80: movc r0.xyz, r0.wwww, r1.xyzx, r0.xyzx
  81: mul r0.xyz, r0.xyzx, cb1[12].xxxx
  82: mul r0.xyz, r0.xyzx, v3.xyzx
  83: max r0.w, v2.x, l(1.000000)
  84: min r0.w, r0.w, l(1000000.000000)
  85: eq r1.x, v1.x, v2.x
  86: movc r0.w, r1.x, l(1.000000), r0.w
  87: mul r0.xyz, r0.xyzx, r0.wwww
  88: ftoi r1.xy, v0.xyxx
  89: mov r1.zw, l(0,0,0,0)
  90: ld r1.xyzw, r1.xyzw, t1.xyzw
  91: mad r0.xyz, r0.xyzx, r1.wwww, r1.xyzx
  92: div r1.xy, l(1.000000, 1.000000, 1.000000, 1.000000), cb0[3].xyxx
  93: mul r1.xy, r1.xyxx, v0.xyxx
  94: mad r0.w, r1.y, cb0[27].x, cb0[27].y
  95: add r1.z, -r0.w, l(1.000000)
  96: sample_b r1.xyzw, r1.xzxx, t0.xyzw, s1, cb0[4].x
  97: add r0.w, -cb0[9].x, l(1.000000)
  98: add_sat r0.w, r0.w, r1.x
  99: add r0.w, r0.w, l(-1.000000)
 100: mad r0.w, cb0[9].w, r0.w, l(1.000000)
 101: mul o0.xyz, r0.wwww, r0.xyzx
 102: mov o0.w, l(1.000000)
 103: ret 
// Approximately 0 instruction slots used


//////////////////////////////////////////////////////
Keywords: FOG_EXP2 LIGHTMAP_BICUBIC_SAMPLING _DBUFFER_MRT3 _SCREEN_SPACE_OCCLUSION _SURFACE_TYPE_TRANSPARENT
-- Vertex shader for "d3d11":
// No shader variant for this keyword set. The closest match will be used instead.

-- Hardware tier variant: Tier 1
-- Fragment shader for "d3d11":
// Stats: 76 math, 4 temp registers
Set 2D Texture "_CameraDepthTexture" to slot 0 sampler slot -1
Set 2D Texture "_MainTex" to slot 1
Set 2D Texture "_Noise" to slot 2
Set 2D Texture "_Flow" to slot 3
Set 2D Texture "_Mask" to slot 4

Set Sampler PointClamp to slot 0

Constant Buffer "$Globals" (2096 bytes) on slot 0 {
  Matrix4x4 unity_MatrixV at 1056
  Matrix4x4 unity_MatrixVP at 1248
  Vector4 _ScaledScreenParams at 48
  Vector2 _GlobalMipBias at 64
  Vector4 _TimeParameters at 304
  Vector3 _WorldSpaceCameraPos at 336
  Vector4 _ProjectionParams at 352
  Vector4 _ZBufferParams at 384
  Vector4 unity_OrthoParams at 400
  Vector4 _RTHandleScale at 448
  Vector4 _CameraDepthTexture_TexelSize at 2080
}
Constant Buffer "UnityPerMaterial" (304 bytes) on slot 1 {
  Float _Textureopacity at 0
  Float _Fresnelpower at 4
  Float _Fresnelscale at 8
  Float _Multiply_texture at 12
  Float _Useonlycolor at 16
  Float _Use_fresnel at 20
  Vector4 _MainTex_ST at 48
  Vector4 _Noise_ST at 80
  Vector4 _Flow_ST at 112
  Vector4 _Mask_ST at 144
  Vector4 _SpeedMainTexUVNoiseZW at 160
  Vector4 _DistortionSpeedXYPowerZ at 176
  Float _Emission at 192
  Float _Opacity at 224
  Float _Usecenterglow at 228
  Float _Usedepth at 232
  Float _Depthpower at 236
}

Shader Disassembly:
//
// Generated by Microsoft (R) D3D Shader Disassembler
//
//
// Input signature:
//
// Name                 Index   Mask Register SysValue  Format   Used
// -------------------- ----- ------ -------- -------- ------- ------
// SV_POSITION              0   xyzw        0      POS   float   xy  
// INTERP                   0   xyzw        1     NONE   float   xyzw
// INTERP                   1   xyzw        2     NONE   float   x   
// INTERP                   2   xyzw        3     NONE   float   xyzw
// INTERP                   3   xyz         4     NONE   float   xyz 
// INTERP                   4   xyz         5     NONE   float   xyz 
// SV_IsFrontFace           0   x           6    FFACE    uint   x   
//
//
// Output signature:
//
// Name                 Index   Mask Register SysValue  Format   Used
// -------------------- ----- ------ -------- -------- ------- ------
// SV_Target                0   xyzw        0   TARGET   float   xyzw
//
      ps_4_0
      dcl_constantbuffer CB0[131], immediateIndexed
      dcl_constantbuffer CB1[15], immediateIndexed
      dcl_sampler s0, mode_default
      dcl_sampler s1, mode_default
      dcl_sampler s2, mode_default
      dcl_sampler s3, mode_default
      dcl_sampler s4, mode_default
      dcl_resource_texture2d (float,float,float,float) t0
      dcl_resource_texture2d (float,float,float,float) t1
      dcl_resource_texture2d (float,float,float,float) t2
      dcl_resource_texture2d (float,float,float,float) t3
      dcl_resource_texture2d (float,float,float,float) t4
      dcl_input_ps_siv linear noperspective v0.xy, position
      dcl_input_ps linear v1.xyzw
      dcl_input_ps linear v2.x
      dcl_input_ps linear v3.xyzw
      dcl_input_ps linear v4.xyz
      dcl_input_ps linear v5.xyz
      dcl_input_ps_sgv constant v6.x, is_front_face
      dcl_output o0.xyzw
      dcl_temps 4
   0: dp3 r0.x, v5.xyzx, v5.xyzx
   1: sqrt r0.x, r0.x
   2: div r0.x, l(1.000000, 1.000000, 1.000000, 1.000000), r0.x
   3: mul r0.xyz, r0.xxxx, v5.xyzx
   4: dp3 r0.w, r0.xyzx, r0.xyzx
   5: rsq r0.w, r0.w
   6: mul r0.xyz, r0.wwww, r0.xyzx
   7: add r1.xyz, -v4.xyzx, cb0[21].xyzx
   8: dp3 r0.w, r1.xyzx, r1.xyzx
   9: rsq r0.w, r0.w
  10: mul r1.xyz, r0.wwww, r1.xyzx
  11: eq r0.w, cb0[25].w, l(0.000000)
  12: movc r2.x, r0.w, r1.x, cb0[66].z
  13: movc r2.y, r0.w, r1.y, cb0[67].z
  14: movc r2.z, r0.w, r1.z, cb0[68].z
  15: dp3_sat r0.x, r0.xyzx, r2.xyzx
  16: add r0.x, -r0.x, l(1.000000)
  17: log r0.x, r0.x
  18: mul r0.x, r0.x, cb1[0].y
  19: exp r0.x, r0.x
  20: mul_sat r0.x, r0.x, cb1[0].z
  21: and r0.x, r0.x, v6.x
  22: lt r0.y, cb0[22].x, l(0.000000)
  23: add r0.z, -v0.y, cb0[3].y
  24: movc r1.y, r0.y, r0.z, v0.y
  25: mov r1.x, v0.x
  26: div r1.xy, r1.xyxx, cb0[3].xyxx
  27: add r1.z, -r1.y, l(1.000000)
  28: mad r0.yz, -cb0[130].xxyx, l(0.000000, 0.500000, 0.500000, 0.000000), l(0.000000, 1.000000, 1.000000, 0.000000)
  29: min r0.yz, r0.yyzy, r1.xxzx
  30: mul r0.yz, r0.yyzy, cb0[28].xxyx
  31: sample_b r1.xyzw, r0.yzyy, t0.xyzw, s0, cb0[4].x
  32: mad r0.y, cb0[24].x, r1.x, cb0[24].y
  33: div r0.y, l(1.000000, 1.000000, 1.000000, 1.000000), r0.y
  34: mul r0.z, v4.y, cb0[79].w
  35: mad r0.z, cb0[78].w, v4.x, r0.z
  36: mad r0.z, cb0[80].w, v4.z, r0.z
  37: add r0.z, r0.z, cb0[81].w
  38: mad r0.y, r0.y, cb0[22].z, -r0.z
  39: div_sat r0.y, r0.y, cb1[14].w
  40: ge r0.z, l(0.000000), r0.y
  41: and r0.z, r0.z, l(0x3f800000)
  42: add r0.w, -r0.y, l(1.000000)
  43: mad r0.y, r0.z, r0.w, r0.y
  44: add r0.x, -r0.y, r0.x
  45: add r0.x, r0.x, l(1.000000)
  46: mul r0.z, v1.w, l(1.000100)
  47: ge r0.w, r0.z, -r0.z
  48: frc r0.z, |r0.z|
  49: movc r0.z, r0.w, r0.z, -r0.z
  50: mul r0.z, r0.z, l(0.999900)
  51: div r1.xy, cb1[10].zwzz, cb1[5].xyxx
  52: mad r0.zw, cb0[19].xxxx, r1.xxxy, r0.zzzz
  53: add r0.zw, r0.zzzw, v1.xxxy
  54: mad r0.zw, r0.zzzw, cb1[5].xxxy, cb1[5].zzzw
  55: sample_b r1.xyzw, r0.zwzz, t2.xyzw, s2, cb0[4].x
  56: div r0.zw, cb1[11].xxxy, cb1[7].xxxy
  57: mad r0.zw, r0.zzzw, cb0[19].xxxx, v1.xxxy
  58: mad r0.zw, r0.zzzw, cb1[7].xxxy, cb1[7].zzzw
  59: sample_b r2.xyzw, r0.zwzz, t3.xyzw, s3, cb0[4].x
  60: mad r0.zw, v1.xxxy, cb1[9].xxxy, cb1[9].zzzw
  61: sample_b r3.xyzw, r0.zwzz, t4.xyzw, s4, cb0[4].x
  62: mul r0.zw, r2.xxxy, r3.xxxy
  63: mul r0.zw, r0.zzzw, cb1[11].zzzz
  64: div r0.zw, r0.zzzw, cb1[3].xxxy
  65: div r2.xy, cb1[10].xyxx, cb1[3].xyxx
  66: mad r2.xy, r2.xyxx, cb0[19].xxxx, v1.xyxx
  67: add r0.zw, -r0.zzzw, r2.xxxy
  68: mad r0.zw, r0.zzzw, cb1[3].xxxy, cb1[3].zzzw
  69: sample_b r2.xyzw, r0.zwzz, t1.xyzw, s1, cb0[4].x
  70: mul r1.xyzw, r1.wxyz, r2.wxyz
  71: ne r0.zw, l(0.000000, 0.000000, 0.000000, 0.000000), cb1[1].xxxy
  72: movc r2.xyz, r0.zzzz, r1.xxxx, r1.yzwy
  73: max r2.xyz, r0.xxxx, r2.xyzx
  74: mad_sat r0.x, r1.x, cb1[0].x, r0.x
  75: movc r1.yzw, r0.wwww, r2.xxyz, r1.yyzw
  76: add r0.z, -v1.z, l(1.000000)
  77: add_sat r2.xyz, -r0.zzzz, r3.xyzx
  78: mul_sat r2.xyz, r2.xyzx, r3.xyzx
  79: mul r2.xyz, r1.yzwy, r2.xyzx
  80: ne r3.xy, l(0.000000, 0.000000, 0.000000, 0.000000), cb1[14].yzyy
  81: movc r1.yzw, r3.xxxx, r2.xxyz, r1.yyzw
  82: mul r1.yzw, r1.yyzw, cb1[12].xxxx
  83: mul r1.yzw, r1.yyzw, v3.xxyz
  84: max r0.z, v2.x, l(1.000000)
  85: min r0.z, r0.z, l(1000000.000000)
  86: eq r2.x, v1.x, v2.x
  87: movc r0.z, r2.x, l(1.000000), r0.z
  88: mul o0.xyz, r1.yzwy, r0.zzzz
  89: mul r0.z, r0.x, r1.x
  90: ne r1.y, l(0.000000, 0.000000, 0.000000, 0.000000), cb1[0].w
  91: movc r0.x, r1.y, r0.z, r0.x
  92: movc r0.x, r0.w, r0.x, r1.x
  93: mul r0.y, r0.y, r0.x
  94: movc r0.x, r3.y, r0.y, r0.x
  95: mul r0.x, r0.x, v3.w
  96: mul_sat o0.w, r0.x, cb1[14].x
  97: ret 
// Approximately 0 instruction slots used


//////////////////////////////////////////////////////
Keywords: LIGHTMAP_BICUBIC_SAMPLING _DBUFFER_MRT3 _SCREEN_SPACE_OCCLUSION _WRITE_RENDERING_LAYERS
-- Vertex shader for "d3d11":
// No shader variant for this keyword set. The closest match will be used instead.

-- Hardware tier variant: Tier 1
-- Fragment shader for "d3d11":
// Stats: 81 math, 4 temp registers
Set 2D Texture "_ScreenSpaceOcclusionTexture" to slot 0 sampler slot -1
Set 2D Texture "_DBufferTexture0" to slot 1 sampler slot -1
Set 2D Texture "_CameraDepthTexture" to slot 2 sampler slot -1
Set 2D Texture "_MainTex" to slot 3 sampler slot 2
Set 2D Texture "_Noise" to slot 4 sampler slot 3
Set 2D Texture "_Flow" to slot 5 sampler slot 4
Set 2D Texture "_Mask" to slot 6 sampler slot 5

Set Sampler PointClamp to slot 0
Set Sampler LinearClamp to slot 1

Constant Buffer "$Globals" (2096 bytes) on slot 0 {
  Matrix4x4 unity_MatrixV at 1056
  Matrix4x4 unity_MatrixVP at 1248
  Vector4 _ScaledScreenParams at 48
  Vector2 _GlobalMipBias at 64
  Vector4 _AmbientOcclusionParam at 144
  ScalarInt _RenderingLayerMaxInt at 176
  Vector4 _TimeParameters at 304
  Vector3 _WorldSpaceCameraPos at 336
  Vector4 _ProjectionParams at 352
  Vector4 _ZBufferParams at 384
  Vector4 unity_OrthoParams at 400
  Vector4 _ScaleBiasRt at 432
  Vector4 _RTHandleScale at 448
  Vector4 _CameraDepthTexture_TexelSize at 2080
}
Constant Buffer "UnityPerDraw" (720 bytes) on slot 1 {
  Vector4 unity_RenderingLayer at 160
}
Constant Buffer "UnityPerMaterial" (304 bytes) on slot 2 {
  Float _Fresnelpower at 4
  Float _Fresnelscale at 8
  Float _Useonlycolor at 16
  Float _Use_fresnel at 20
  Vector4 _MainTex_ST at 48
  Vector4 _Noise_ST at 80
  Vector4 _Flow_ST at 112
  Vector4 _Mask_ST at 144
  Vector4 _SpeedMainTexUVNoiseZW at 160
  Vector4 _DistortionSpeedXYPowerZ at 176
  Float _Emission at 192
  Float _Usecenterglow at 228
  Float _Depthpower at 236
}

Shader Disassembly:
//
// Generated by Microsoft (R) D3D Shader Disassembler
//
//
// Input signature:
//
// Name                 Index   Mask Register SysValue  Format   Used
// -------------------- ----- ------ -------- -------- ------- ------
// SV_POSITION              0   xyzw        0      POS   float   xy  
// INTERP                   0   xyzw        1     NONE   float   xyzw
// INTERP                   1   xyzw        2     NONE   float   x   
// INTERP                   2   xyzw        3     NONE   float   xyz 
// INTERP                   3   xyz         4     NONE   float   xyz 
// INTERP                   4   xyz         5     NONE   float   xyz 
// SV_IsFrontFace           0   x           6    FFACE    uint   x   
//
//
// Output signature:
//
// Name                 Index   Mask Register SysValue  Format   Used
// -------------------- ----- ------ -------- -------- ------- ------
// SV_Target                0   xyzw        0   TARGET   float   xyzw
// SV_Target                1   x           1   TARGET    uint   x   
//
      ps_5_0
      dcl_globalFlags refactoringAllowed
      dcl_constantbuffer CB0[131], immediateIndexed
      dcl_constantbuffer CB1[11], immediateIndexed
      dcl_constantbuffer CB2[15], immediateIndexed
      dcl_sampler s0, mode_default
      dcl_sampler s1, mode_default
      dcl_sampler s2, mode_default
      dcl_sampler s3, mode_default
      dcl_sampler s4, mode_default
      dcl_sampler s5, mode_default
      dcl_resource_texture2d (float,float,float,float) t0
      dcl_resource_texture2d (float,float,float,float) t1
      dcl_resource_texture2d (float,float,float,float) t2
      dcl_resource_texture2d (float,float,float,float) t3
      dcl_resource_texture2d (float,float,float,float) t4
      dcl_resource_texture2d (float,float,float,float) t5
      dcl_resource_texture2d (float,float,float,float) t6
      dcl_input_ps_siv linear noperspective v0.xy, position
      dcl_input_ps linear v1.xyzw
      dcl_input_ps linear v2.x
      dcl_input_ps linear v3.xyz
      dcl_input_ps linear v4.xyz
      dcl_input_ps linear v5.xyz
      dcl_input_ps_sgv constant v6.x, is_front_face
      dcl_output o0.xyzw
      dcl_output o1.x
      dcl_temps 4
   0: dp3 r0.x, v5.xyzx, v5.xyzx
   1: sqrt r0.x, r0.x
   2: div r0.x, l(1.000000, 1.000000, 1.000000, 1.000000), r0.x
   3: mul r0.xyz, r0.xxxx, v5.xyzx
   4: dp3 r0.w, r0.xyzx, r0.xyzx
   5: rsq r0.w, r0.w
   6: mul r0.xyz, r0.wwww, r0.xyzx
   7: add r1.xyz, -v4.xyzx, cb0[21].xyzx
   8: dp3 r0.w, r1.xyzx, r1.xyzx
   9: rsq r0.w, r0.w
  10: mul r1.xyz, r0.wwww, r1.xyzx
  11: eq r0.w, cb0[25].w, l(0.000000)
  12: movc r2.x, r0.w, r1.x, cb0[66].z
  13: movc r2.y, r0.w, r1.y, cb0[67].z
  14: movc r2.z, r0.w, r1.z, cb0[68].z
  15: dp3_sat r0.x, r0.xyzx, r2.xyzx
  16: add r0.x, -r0.x, l(1.000000)
  17: log r0.x, r0.x
  18: mul r0.x, r0.x, cb2[0].y
  19: exp r0.x, r0.x
  20: mul_sat r0.x, r0.x, cb2[0].z
  21: and r0.x, r0.x, v6.x
  22: lt r0.y, cb0[22].x, l(0.000000)
  23: add r0.z, -v0.y, cb0[3].y
  24: movc r1.y, r0.y, r0.z, v0.y
  25: mov r1.x, v0.x
  26: div r1.xy, r1.xyxx, cb0[3].xyxx
  27: add r1.z, -r1.y, l(1.000000)
  28: mad r0.yz, -cb0[130].xxyx, l(0.000000, 0.500000, 0.500000, 0.000000), l(0.000000, 1.000000, 1.000000, 0.000000)
  29: min r0.yz, r0.yyzy, r1.xxzx
  30: mul r0.yz, r0.yyzy, cb0[28].xxyx
  31: sample_b_indexable(texture2d)(float,float,float,float) r0.y, r0.yzyy, t2.yxzw, s0, cb0[4].x
  32: mad r0.y, cb0[24].x, r0.y, cb0[24].y
  33: div r0.y, l(1.000000, 1.000000, 1.000000, 1.000000), r0.y
  34: mul r0.z, v4.y, cb0[79].w
  35: mad r0.z, cb0[78].w, v4.x, r0.z
  36: mad r0.z, cb0[80].w, v4.z, r0.z
  37: add r0.z, r0.z, cb0[81].w
  38: mad r0.y, r0.y, cb0[22].z, -r0.z
  39: div_sat r0.y, r0.y, cb2[14].w
  40: ge r0.z, l(0.000000), r0.y
  41: and r0.z, r0.z, l(0x3f800000)
  42: add r0.w, -r0.y, l(1.000000)
  43: mad r0.y, r0.z, r0.w, r0.y
  44: add r0.x, -r0.y, r0.x
  45: add r0.x, r0.x, l(1.000000)
  46: mul r0.y, v1.w, l(1.000100)
  47: ge r0.z, r0.y, -r0.y
  48: frc r0.y, |r0.y|
  49: movc r0.y, r0.z, r0.y, -r0.y
  50: mul r0.y, r0.y, l(0.999900)
  51: div r0.zw, cb2[10].zzzw, cb2[5].xxxy
  52: mad r0.yz, cb0[19].xxxx, r0.zzwz, r0.yyyy
  53: add r0.yz, r0.yyzy, v1.xxyx
  54: mad r0.yz, r0.yyzy, cb2[5].xxyx, cb2[5].zzwz
  55: sample_b_indexable(texture2d)(float,float,float,float) r1.xyzw, r0.yzyy, t4.xyzw, s3, cb0[4].x
  56: div r0.yz, cb2[11].xxyx, cb2[7].xxyx
  57: mad r0.yz, r0.yyzy, cb0[19].xxxx, v1.xxyx
  58: mad r0.yz, r0.yyzy, cb2[7].xxyx, cb2[7].zzwz
  59: sample_b_indexable(texture2d)(float,float,float,float) r0.yz, r0.yzyy, t5.zxyw, s4, cb0[4].x
  60: mad r2.xy, v1.xyxx, cb2[9].xyxx, cb2[9].zwzz
  61: sample_b_indexable(texture2d)(float,float,float,float) r2.xyz, r2.xyxx, t6.xyzw, s5, cb0[4].x
  62: mul r0.yz, r0.yyzy, r2.xxyx
  63: mul r0.yz, r0.yyzy, cb2[11].zzzz
  64: div r0.yz, r0.yyzy, cb2[3].xxyx
  65: div r3.xy, cb2[10].xyxx, cb2[3].xyxx
  66: mad r3.xy, r3.xyxx, cb0[19].xxxx, v1.xyxx
  67: add r0.yz, -r0.yyzy, r3.xxyx
  68: mad r0.yz, r0.yyzy, cb2[3].xxyx, cb2[3].zzwz
  69: sample_b_indexable(texture2d)(float,float,float,float) r3.xyzw, r0.yzyy, t3.xyzw, s2, cb0[4].x
  70: mul r1.xyzw, r1.wxyz, r3.wxyz
  71: ne r0.yz, l(0.000000, 0.000000, 0.000000, 0.000000), cb2[1].xxyx
  72: movc r3.xyz, r0.yyyy, r1.xxxx, r1.yzwy
  73: max r0.xyw, r0.xxxx, r3.xyxz
  74: movc r0.xyz, r0.zzzz, r0.xywx, r1.yzwy
  75: add r0.w, -v1.z, l(1.000000)
  76: add_sat r1.xyz, -r0.wwww, r2.xyzx
  77: mul_sat r1.xyz, r1.xyzx, r2.xyzx
  78: mul r1.xyz, r0.xyzx, r1.xyzx
  79: ne r0.w, l(0.000000, 0.000000, 0.000000, 0.000000), cb2[14].y
  80: movc r0.xyz, r0.wwww, r1.xyzx, r0.xyzx
  81: mul r0.xyz, r0.xyzx, cb2[12].xxxx
  82: mul r0.xyz, r0.xyzx, v3.xyzx
  83: max r0.w, v2.x, l(1.000000)
  84: min r0.w, r0.w, l(1000000.000000)
  85: eq r1.x, v1.x, v2.x
  86: movc r0.w, r1.x, l(1.000000), r0.w
  87: mul r0.xyz, r0.xyzx, r0.wwww
  88: ftoi r1.xy, v0.xyxx
  89: mov r1.zw, l(0,0,0,0)
  90: ld_indexable(texture2d)(float,float,float,float) r1.xyzw, r1.xyzw, t1.xyzw
  91: mad r0.xyz, r0.xyzx, r1.wwww, r1.xyzx
  92: rcp r1.xy, cb0[3].xyxx
  93: mul r1.xy, r1.xyxx, v0.xyxx
  94: mad r0.w, r1.y, cb0[27].x, cb0[27].y
  95: add r1.z, -r0.w, l(1.000000)
  96: sample_b_indexable(texture2d)(float,float,float,float) r0.w, r1.xzxx, t0.yzwx, s1, cb0[4].x
  97: add r1.x, -cb0[9].x, l(1.000000)
  98: add_sat r0.w, r0.w, r1.x
  99: add r0.w, r0.w, l(-1.000000)
 100: mad r0.w, cb0[9].w, r0.w, l(1.000000)
 101: mul o0.xyz, r0.wwww, r0.xyzx
 102: mov o0.w, l(1.000000)
 103: and o1.x, cb0[11].x, cb1[10].x
 104: ret 
// Approximately 0 instruction slots used


//////////////////////////////////////////////////////
Keywords: LIGHTMAP_BICUBIC_SAMPLING _DBUFFER_MRT3 _SCREEN_SPACE_OCCLUSION _SURFACE_TYPE_TRANSPARENT _WRITE_RENDERING_LAYERS
-- Vertex shader for "d3d11":
// No shader variant for this keyword set. The closest match will be used instead.

-- Hardware tier variant: Tier 1
-- Fragment shader for "d3d11":
// Stats: 77 math, 4 temp registers
Set 2D Texture "_CameraDepthTexture" to slot 0 sampler slot -1
Set 2D Texture "_MainTex" to slot 1
Set 2D Texture "_Noise" to slot 2
Set 2D Texture "_Flow" to slot 3
Set 2D Texture "_Mask" to slot 4

Set Sampler PointClamp to slot 0

Constant Buffer "$Globals" (2096 bytes) on slot 0 {
  Matrix4x4 unity_MatrixV at 1056
  Matrix4x4 unity_MatrixVP at 1248
  Vector4 _ScaledScreenParams at 48
  Vector2 _GlobalMipBias at 64
  ScalarInt _RenderingLayerMaxInt at 176
  Vector4 _TimeParameters at 304
  Vector3 _WorldSpaceCameraPos at 336
  Vector4 _ProjectionParams at 352
  Vector4 _ZBufferParams at 384
  Vector4 unity_OrthoParams at 400
  Vector4 _RTHandleScale at 448
  Vector4 _CameraDepthTexture_TexelSize at 2080
}
Constant Buffer "UnityPerDraw" (720 bytes) on slot 1 {
  Vector4 unity_RenderingLayer at 160
}
Constant Buffer "UnityPerMaterial" (304 bytes) on slot 2 {
  Float _Textureopacity at 0
  Float _Fresnelpower at 4
  Float _Fresnelscale at 8
  Float _Multiply_texture at 12
  Float _Useonlycolor at 16
  Float _Use_fresnel at 20
  Vector4 _MainTex_ST at 48
  Vector4 _Noise_ST at 80
  Vector4 _Flow_ST at 112
  Vector4 _Mask_ST at 144
  Vector4 _SpeedMainTexUVNoiseZW at 160
  Vector4 _DistortionSpeedXYPowerZ at 176
  Float _Emission at 192
  Float _Opacity at 224
  Float _Usecenterglow at 228
  Float _Usedepth at 232
  Float _Depthpower at 236
}

Shader Disassembly:
//
// Generated by Microsoft (R) D3D Shader Disassembler
//
//
// Input signature:
//
// Name                 Index   Mask Register SysValue  Format   Used
// -------------------- ----- ------ -------- -------- ------- ------
// SV_POSITION              0   xyzw        0      POS   float   xy  
// INTERP                   0   xyzw        1     NONE   float   xyzw
// INTERP                   1   xyzw        2     NONE   float   x   
// INTERP                   2   xyzw        3     NONE   float   xyzw
// INTERP                   3   xyz         4     NONE   float   xyz 
// INTERP                   4   xyz         5     NONE   float   xyz 
// SV_IsFrontFace           0   x           6    FFACE    uint   x   
//
//
// Output signature:
//
// Name                 Index   Mask Register SysValue  Format   Used
// -------------------- ----- ------ -------- -------- ------- ------
// SV_Target                0   xyzw        0   TARGET   float   xyzw
// SV_Target                1   x           1   TARGET    uint   x   
//
      ps_5_0
      dcl_globalFlags refactoringAllowed
      dcl_constantbuffer CB0[131], immediateIndexed
      dcl_constantbuffer CB1[11], immediateIndexed
      dcl_constantbuffer CB2[15], immediateIndexed
      dcl_sampler s0, mode_default
      dcl_sampler s1, mode_default
      dcl_sampler s2, mode_default
      dcl_sampler s3, mode_default
      dcl_sampler s4, mode_default
      dcl_resource_texture2d (float,float,float,float) t0
      dcl_resource_texture2d (float,float,float,float) t1
      dcl_resource_texture2d (float,float,float,float) t2
      dcl_resource_texture2d (float,float,float,float) t3
      dcl_resource_texture2d (float,float,float,float) t4
      dcl_input_ps_siv linear noperspective v0.xy, position
      dcl_input_ps linear v1.xyzw
      dcl_input_ps linear v2.x
      dcl_input_ps linear v3.xyzw
      dcl_input_ps linear v4.xyz
      dcl_input_ps linear v5.xyz
      dcl_input_ps_sgv constant v6.x, is_front_face
      dcl_output o0.xyzw
      dcl_output o1.x
      dcl_temps 4
   0: dp3 r0.x, v5.xyzx, v5.xyzx
   1: sqrt r0.x, r0.x
   2: div r0.x, l(1.000000, 1.000000, 1.000000, 1.000000), r0.x
   3: mul r0.xyz, r0.xxxx, v5.xyzx
   4: dp3 r0.w, r0.xyzx, r0.xyzx
   5: rsq r0.w, r0.w
   6: mul r0.xyz, r0.wwww, r0.xyzx
   7: add r1.xyz, -v4.xyzx, cb0[21].xyzx
   8: dp3 r0.w, r1.xyzx, r1.xyzx
   9: rsq r0.w, r0.w
  10: mul r1.xyz, r0.wwww, r1.xyzx
  11: eq r0.w, cb0[25].w, l(0.000000)
  12: movc r2.x, r0.w, r1.x, cb0[66].z
  13: movc r2.y, r0.w, r1.y, cb0[67].z
  14: movc r2.z, r0.w, r1.z, cb0[68].z
  15: dp3_sat r0.x, r0.xyzx, r2.xyzx
  16: add r0.x, -r0.x, l(1.000000)
  17: log r0.x, r0.x
  18: mul r0.x, r0.x, cb2[0].y
  19: exp r0.x, r0.x
  20: mul_sat r0.x, r0.x, cb2[0].z
  21: and r0.x, r0.x, v6.x
  22: lt r0.y, cb0[22].x, l(0.000000)
  23: add r0.z, -v0.y, cb0[3].y
  24: movc r1.y, r0.y, r0.z, v0.y
  25: mov r1.x, v0.x
  26: div r1.xy, r1.xyxx, cb0[3].xyxx
  27: add r1.z, -r1.y, l(1.000000)
  28: mad r0.yz, -cb0[130].xxyx, l(0.000000, 0.500000, 0.500000, 0.000000), l(0.000000, 1.000000, 1.000000, 0.000000)
  29: min r0.yz, r0.yyzy, r1.xxzx
  30: mul r0.yz, r0.yyzy, cb0[28].xxyx
  31: sample_b_indexable(texture2d)(float,float,float,float) r0.y, r0.yzyy, t0.yxzw, s0, cb0[4].x
  32: mad r0.y, cb0[24].x, r0.y, cb0[24].y
  33: div r0.y, l(1.000000, 1.000000, 1.000000, 1.000000), r0.y
  34: mul r0.z, v4.y, cb0[79].w
  35: mad r0.z, cb0[78].w, v4.x, r0.z
  36: mad r0.z, cb0[80].w, v4.z, r0.z
  37: add r0.z, r0.z, cb0[81].w
  38: mad r0.y, r0.y, cb0[22].z, -r0.z
  39: div_sat r0.y, r0.y, cb2[14].w
  40: ge r0.z, l(0.000000), r0.y
  41: and r0.z, r0.z, l(0x3f800000)
  42: add r0.w, -r0.y, l(1.000000)
  43: mad r0.y, r0.z, r0.w, r0.y
  44: add r0.x, -r0.y, r0.x
  45: add r0.x, r0.x, l(1.000000)
  46: mul r0.z, v1.w, l(1.000100)
  47: ge r0.w, r0.z, -r0.z
  48: frc r0.z, |r0.z|
  49: movc r0.z, r0.w, r0.z, -r0.z
  50: mul r0.z, r0.z, l(0.999900)
  51: div r1.xy, cb2[10].zwzz, cb2[5].xyxx
  52: mad r0.zw, cb0[19].xxxx, r1.xxxy, r0.zzzz
  53: add r0.zw, r0.zzzw, v1.xxxy
  54: mad r0.zw, r0.zzzw, cb2[5].xxxy, cb2[5].zzzw
  55: sample_b_indexable(texture2d)(float,float,float,float) r1.xyzw, r0.zwzz, t2.xyzw, s2, cb0[4].x
  56: div r0.zw, cb2[11].xxxy, cb2[7].xxxy
  57: mad r0.zw, r0.zzzw, cb0[19].xxxx, v1.xxxy
  58: mad r0.zw, r0.zzzw, cb2[7].xxxy, cb2[7].zzzw
  59: sample_b_indexable(texture2d)(float,float,float,float) r0.zw, r0.zwzz, t3.zwxy, s3, cb0[4].x
  60: mad r2.xy, v1.xyxx, cb2[9].xyxx, cb2[9].zwzz
  61: sample_b_indexable(texture2d)(float,float,float,float) r2.xyz, r2.xyxx, t4.xyzw, s4, cb0[4].x
  62: mul r0.zw, r0.zzzw, r2.xxxy
  63: mul r0.zw, r0.zzzw, cb2[11].zzzz
  64: div r0.zw, r0.zzzw, cb2[3].xxxy
  65: div r3.xy, cb2[10].xyxx, cb2[3].xyxx
  66: mad r3.xy, r3.xyxx, cb0[19].xxxx, v1.xyxx
  67: add r0.zw, -r0.zzzw, r3.xxxy
  68: mad r0.zw, r0.zzzw, cb2[3].xxxy, cb2[3].zzzw
  69: sample_b_indexable(texture2d)(float,float,float,float) r3.xyzw, r0.zwzz, t1.xyzw, s1, cb0[4].x
  70: mul r1.xyzw, r1.wxyz, r3.wxyz
  71: ne r0.zw, l(0.000000, 0.000000, 0.000000, 0.000000), cb2[1].xxxy
  72: movc r3.xyz, r0.zzzz, r1.xxxx, r1.yzwy
  73: max r3.xyz, r0.xxxx, r3.xyzx
  74: mad_sat r0.x, r1.x, cb2[0].x, r0.x
  75: movc r1.yzw, r0.wwww, r3.xxyz, r1.yyzw
  76: add r0.z, -v1.z, l(1.000000)
  77: add_sat r3.xyz, -r0.zzzz, r2.xyzx
  78: mul_sat r2.xyz, r2.xyzx, r3.xyzx
  79: mul r2.xyz, r1.yzwy, r2.xyzx
  80: ne r3.xy, l(0.000000, 0.000000, 0.000000, 0.000000), cb2[14].yzyy
  81: movc r1.yzw, r3.xxxx, r2.xxyz, r1.yyzw
  82: mul r1.yzw, r1.yyzw, cb2[12].xxxx
  83: mul r1.yzw, r1.yyzw, v3.xxyz
  84: max r0.z, v2.x, l(1.000000)
  85: min r0.z, r0.z, l(1000000.000000)
  86: eq r2.x, v1.x, v2.x
  87: movc r0.z, r2.x, l(1.000000), r0.z
  88: mul o0.xyz, r1.yzwy, r0.zzzz
  89: mul r0.z, r0.x, r1.x
  90: ne r1.y, l(0.000000, 0.000000, 0.000000, 0.000000), cb2[0].w
  91: movc r0.x, r1.y, r0.z, r0.x
  92: movc r0.x, r0.w, r0.x, r1.x
  93: mul r0.y, r0.y, r0.x
  94: movc r0.x, r3.y, r0.y, r0.x
  95: mul r0.x, r0.x, v3.w
  96: mul_sat o0.w, r0.x, cb2[14].x
  97: and o1.x, cb0[11].x, cb1[10].x
  98: ret 
// Approximately 0 instruction slots used


//////////////////////////////////////////////////////
Keywords: FOG_EXP2 LIGHTMAP_BICUBIC_SAMPLING _DBUFFER_MRT3 _SCREEN_SPACE_OCCLUSION _WRITE_RENDERING_LAYERS
-- Vertex shader for "d3d11":
// No shader variant for this keyword set. The closest match will be used instead.

-- Hardware tier variant: Tier 1
-- Fragment shader for "d3d11":
// Stats: 81 math, 4 temp registers
Set 2D Texture "_ScreenSpaceOcclusionTexture" to slot 0 sampler slot -1
Set 2D Texture "_DBufferTexture0" to slot 1 sampler slot -1
Set 2D Texture "_CameraDepthTexture" to slot 2 sampler slot -1
Set 2D Texture "_MainTex" to slot 3 sampler slot 2
Set 2D Texture "_Noise" to slot 4 sampler slot 3
Set 2D Texture "_Flow" to slot 5 sampler slot 4
Set 2D Texture "_Mask" to slot 6 sampler slot 5

Set Sampler PointClamp to slot 0
Set Sampler LinearClamp to slot 1

Constant Buffer "$Globals" (2096 bytes) on slot 0 {
  Matrix4x4 unity_MatrixV at 1056
  Matrix4x4 unity_MatrixVP at 1248
  Vector4 _ScaledScreenParams at 48
  Vector2 _GlobalMipBias at 64
  Vector4 _AmbientOcclusionParam at 144
  ScalarInt _RenderingLayerMaxInt at 176
  Vector4 _TimeParameters at 304
  Vector3 _WorldSpaceCameraPos at 336
  Vector4 _ProjectionParams at 352
  Vector4 _ZBufferParams at 384
  Vector4 unity_OrthoParams at 400
  Vector4 _ScaleBiasRt at 432
  Vector4 _RTHandleScale at 448
  Vector4 _CameraDepthTexture_TexelSize at 2080
}
Constant Buffer "UnityPerDraw" (720 bytes) on slot 1 {
  Vector4 unity_RenderingLayer at 160
}
Constant Buffer "UnityPerMaterial" (304 bytes) on slot 2 {
  Float _Fresnelpower at 4
  Float _Fresnelscale at 8
  Float _Useonlycolor at 16
  Float _Use_fresnel at 20
  Vector4 _MainTex_ST at 48
  Vector4 _Noise_ST at 80
  Vector4 _Flow_ST at 112
  Vector4 _Mask_ST at 144
  Vector4 _SpeedMainTexUVNoiseZW at 160
  Vector4 _DistortionSpeedXYPowerZ at 176
  Float _Emission at 192
  Float _Usecenterglow at 228
  Float _Depthpower at 236
}

Shader Disassembly:
//
// Generated by Microsoft (R) D3D Shader Disassembler
//
//
// Input signature:
//
// Name                 Index   Mask Register SysValue  Format   Used
// -------------------- ----- ------ -------- -------- ------- ------
// SV_POSITION              0   xyzw        0      POS   float   xy  
// INTERP                   0   xyzw        1     NONE   float   xyzw
// INTERP                   1   xyzw        2     NONE   float   x   
// INTERP                   2   xyzw        3     NONE   float   xyz 
// INTERP                   3   xyz         4     NONE   float   xyz 
// INTERP                   4   xyz         5     NONE   float   xyz 
// SV_IsFrontFace           0   x           6    FFACE    uint   x   
//
//
// Output signature:
//
// Name                 Index   Mask Register SysValue  Format   Used
// -------------------- ----- ------ -------- -------- ------- ------
// SV_Target                0   xyzw        0   TARGET   float   xyzw
// SV_Target                1   x           1   TARGET    uint   x   
//
      ps_5_0
      dcl_globalFlags refactoringAllowed
      dcl_constantbuffer CB0[131], immediateIndexed
      dcl_constantbuffer CB1[11], immediateIndexed
      dcl_constantbuffer CB2[15], immediateIndexed
      dcl_sampler s0, mode_default
      dcl_sampler s1, mode_default
      dcl_sampler s2, mode_default
      dcl_sampler s3, mode_default
      dcl_sampler s4, mode_default
      dcl_sampler s5, mode_default
      dcl_resource_texture2d (float,float,float,float) t0
      dcl_resource_texture2d (float,float,float,float) t1
      dcl_resource_texture2d (float,float,float,float) t2
      dcl_resource_texture2d (float,float,float,float) t3
      dcl_resource_texture2d (float,float,float,float) t4
      dcl_resource_texture2d (float,float,float,float) t5
      dcl_resource_texture2d (float,float,float,float) t6
      dcl_input_ps_siv linear noperspective v0.xy, position
      dcl_input_ps linear v1.xyzw
      dcl_input_ps linear v2.x
      dcl_input_ps linear v3.xyz
      dcl_input_ps linear v4.xyz
      dcl_input_ps linear v5.xyz
      dcl_input_ps_sgv constant v6.x, is_front_face
      dcl_output o0.xyzw
      dcl_output o1.x
      dcl_temps 4
   0: dp3 r0.x, v5.xyzx, v5.xyzx
   1: sqrt r0.x, r0.x
   2: div r0.x, l(1.000000, 1.000000, 1.000000, 1.000000), r0.x
   3: mul r0.xyz, r0.xxxx, v5.xyzx
   4: dp3 r0.w, r0.xyzx, r0.xyzx
   5: rsq r0.w, r0.w
   6: mul r0.xyz, r0.wwww, r0.xyzx
   7: add r1.xyz, -v4.xyzx, cb0[21].xyzx
   8: dp3 r0.w, r1.xyzx, r1.xyzx
   9: rsq r0.w, r0.w
  10: mul r1.xyz, r0.wwww, r1.xyzx
  11: eq r0.w, cb0[25].w, l(0.000000)
  12: movc r2.x, r0.w, r1.x, cb0[66].z
  13: movc r2.y, r0.w, r1.y, cb0[67].z
  14: movc r2.z, r0.w, r1.z, cb0[68].z
  15: dp3_sat r0.x, r0.xyzx, r2.xyzx
  16: add r0.x, -r0.x, l(1.000000)
  17: log r0.x, r0.x
  18: mul r0.x, r0.x, cb2[0].y
  19: exp r0.x, r0.x
  20: mul_sat r0.x, r0.x, cb2[0].z
  21: and r0.x, r0.x, v6.x
  22: lt r0.y, cb0[22].x, l(0.000000)
  23: add r0.z, -v0.y, cb0[3].y
  24: movc r1.y, r0.y, r0.z, v0.y
  25: mov r1.x, v0.x
  26: div r1.xy, r1.xyxx, cb0[3].xyxx
  27: add r1.z, -r1.y, l(1.000000)
  28: mad r0.yz, -cb0[130].xxyx, l(0.000000, 0.500000, 0.500000, 0.000000), l(0.000000, 1.000000, 1.000000, 0.000000)
  29: min r0.yz, r0.yyzy, r1.xxzx
  30: mul r0.yz, r0.yyzy, cb0[28].xxyx
  31: sample_b_indexable(texture2d)(float,float,float,float) r0.y, r0.yzyy, t2.yxzw, s0, cb0[4].x
  32: mad r0.y, cb0[24].x, r0.y, cb0[24].y
  33: div r0.y, l(1.000000, 1.000000, 1.000000, 1.000000), r0.y
  34: mul r0.z, v4.y, cb0[79].w
  35: mad r0.z, cb0[78].w, v4.x, r0.z
  36: mad r0.z, cb0[80].w, v4.z, r0.z
  37: add r0.z, r0.z, cb0[81].w
  38: mad r0.y, r0.y, cb0[22].z, -r0.z
  39: div_sat r0.y, r0.y, cb2[14].w
  40: ge r0.z, l(0.000000), r0.y
  41: and r0.z, r0.z, l(0x3f800000)
  42: add r0.w, -r0.y, l(1.000000)
  43: mad r0.y, r0.z, r0.w, r0.y
  44: add r0.x, -r0.y, r0.x
  45: add r0.x, r0.x, l(1.000000)
  46: mul r0.y, v1.w, l(1.000100)
  47: ge r0.z, r0.y, -r0.y
  48: frc r0.y, |r0.y|
  49: movc r0.y, r0.z, r0.y, -r0.y
  50: mul r0.y, r0.y, l(0.999900)
  51: div r0.zw, cb2[10].zzzw, cb2[5].xxxy
  52: mad r0.yz, cb0[19].xxxx, r0.zzwz, r0.yyyy
  53: add r0.yz, r0.yyzy, v1.xxyx
  54: mad r0.yz, r0.yyzy, cb2[5].xxyx, cb2[5].zzwz
  55: sample_b_indexable(texture2d)(float,float,float,float) r1.xyzw, r0.yzyy, t4.xyzw, s3, cb0[4].x
  56: div r0.yz, cb2[11].xxyx, cb2[7].xxyx
  57: mad r0.yz, r0.yyzy, cb0[19].xxxx, v1.xxyx
  58: mad r0.yz, r0.yyzy, cb2[7].xxyx, cb2[7].zzwz
  59: sample_b_indexable(texture2d)(float,float,float,float) r0.yz, r0.yzyy, t5.zxyw, s4, cb0[4].x
  60: mad r2.xy, v1.xyxx, cb2[9].xyxx, cb2[9].zwzz
  61: sample_b_indexable(texture2d)(float,float,float,float) r2.xyz, r2.xyxx, t6.xyzw, s5, cb0[4].x
  62: mul r0.yz, r0.yyzy, r2.xxyx
  63: mul r0.yz, r0.yyzy, cb2[11].zzzz
  64: div r0.yz, r0.yyzy, cb2[3].xxyx
  65: div r3.xy, cb2[10].xyxx, cb2[3].xyxx
  66: mad r3.xy, r3.xyxx, cb0[19].xxxx, v1.xyxx
  67: add r0.yz, -r0.yyzy, r3.xxyx
  68: mad r0.yz, r0.yyzy, cb2[3].xxyx, cb2[3].zzwz
  69: sample_b_indexable(texture2d)(float,float,float,float) r3.xyzw, r0.yzyy, t3.xyzw, s2, cb0[4].x
  70: mul r1.xyzw, r1.wxyz, r3.wxyz
  71: ne r0.yz, l(0.000000, 0.000000, 0.000000, 0.000000), cb2[1].xxyx
  72: movc r3.xyz, r0.yyyy, r1.xxxx, r1.yzwy
  73: max r0.xyw, r0.xxxx, r3.xyxz
  74: movc r0.xyz, r0.zzzz, r0.xywx, r1.yzwy
  75: add r0.w, -v1.z, l(1.000000)
  76: add_sat r1.xyz, -r0.wwww, r2.xyzx
  77: mul_sat r1.xyz, r1.xyzx, r2.xyzx
  78: mul r1.xyz, r0.xyzx, r1.xyzx
  79: ne r0.w, l(0.000000, 0.000000, 0.000000, 0.000000), cb2[14].y
  80: movc r0.xyz, r0.wwww, r1.xyzx, r0.xyzx
  81: mul r0.xyz, r0.xyzx, cb2[12].xxxx
  82: mul r0.xyz, r0.xyzx, v3.xyzx
  83: max r0.w, v2.x, l(1.000000)
  84: min r0.w, r0.w, l(1000000.000000)
  85: eq r1.x, v1.x, v2.x
  86: movc r0.w, r1.x, l(1.000000), r0.w
  87: mul r0.xyz, r0.xyzx, r0.wwww
  88: ftoi r1.xy, v0.xyxx
  89: mov r1.zw, l(0,0,0,0)
  90: ld_indexable(texture2d)(float,float,float,float) r1.xyzw, r1.xyzw, t1.xyzw
  91: mad r0.xyz, r0.xyzx, r1.wwww, r1.xyzx
  92: rcp r1.xy, cb0[3].xyxx
  93: mul r1.xy, r1.xyxx, v0.xyxx
  94: mad r0.w, r1.y, cb0[27].x, cb0[27].y
  95: add r1.z, -r0.w, l(1.000000)
  96: sample_b_indexable(texture2d)(float,float,float,float) r0.w, r1.xzxx, t0.yzwx, s1, cb0[4].x
  97: add r1.x, -cb0[9].x, l(1.000000)
  98: add_sat r0.w, r0.w, r1.x
  99: add r0.w, r0.w, l(-1.000000)
 100: mad r0.w, cb0[9].w, r0.w, l(1.000000)
 101: mul o0.xyz, r0.wwww, r0.xyzx
 102: mov o0.w, l(1.000000)
 103: and o1.x, cb0[11].x, cb1[10].x
 104: ret 
// Approximately 0 instruction slots used


//////////////////////////////////////////////////////
Keywords: FOG_EXP2 LIGHTMAP_BICUBIC_SAMPLING _DBUFFER_MRT3 _SCREEN_SPACE_OCCLUSION _SURFACE_TYPE_TRANSPARENT _WRITE_RENDERING_LAYERS
-- Vertex shader for "d3d11":
// No shader variant for this keyword set. The closest match will be used instead.

-- Hardware tier variant: Tier 1
-- Fragment shader for "d3d11":
// Stats: 77 math, 4 temp registers
Set 2D Texture "_CameraDepthTexture" to slot 0 sampler slot -1
Set 2D Texture "_MainTex" to slot 1
Set 2D Texture "_Noise" to slot 2
Set 2D Texture "_Flow" to slot 3
Set 2D Texture "_Mask" to slot 4

Set Sampler PointClamp to slot 0

Constant Buffer "$Globals" (2096 bytes) on slot 0 {
  Matrix4x4 unity_MatrixV at 1056
  Matrix4x4 unity_MatrixVP at 1248
  Vector4 _ScaledScreenParams at 48
  Vector2 _GlobalMipBias at 64
  ScalarInt _RenderingLayerMaxInt at 176
  Vector4 _TimeParameters at 304
  Vector3 _WorldSpaceCameraPos at 336
  Vector4 _ProjectionParams at 352
  Vector4 _ZBufferParams at 384
  Vector4 unity_OrthoParams at 400
  Vector4 _RTHandleScale at 448
  Vector4 _CameraDepthTexture_TexelSize at 2080
}
Constant Buffer "UnityPerDraw" (720 bytes) on slot 1 {
  Vector4 unity_RenderingLayer at 160
}
Constant Buffer "UnityPerMaterial" (304 bytes) on slot 2 {
  Float _Textureopacity at 0
  Float _Fresnelpower at 4
  Float _Fresnelscale at 8
  Float _Multiply_texture at 12
  Float _Useonlycolor at 16
  Float _Use_fresnel at 20
  Vector4 _MainTex_ST at 48
  Vector4 _Noise_ST at 80
  Vector4 _Flow_ST at 112
  Vector4 _Mask_ST at 144
  Vector4 _SpeedMainTexUVNoiseZW at 160
  Vector4 _DistortionSpeedXYPowerZ at 176
  Float _Emission at 192
  Float _Opacity at 224
  Float _Usecenterglow at 228
  Float _Usedepth at 232
  Float _Depthpower at 236
}

Shader Disassembly:
//
// Generated by Microsoft (R) D3D Shader Disassembler
//
//
// Input signature:
//
// Name                 Index   Mask Register SysValue  Format   Used
// -------------------- ----- ------ -------- -------- ------- ------
// SV_POSITION              0   xyzw        0      POS   float   xy  
// INTERP                   0   xyzw        1     NONE   float   xyzw
// INTERP                   1   xyzw        2     NONE   float   x   
// INTERP                   2   xyzw        3     NONE   float   xyzw
// INTERP                   3   xyz         4     NONE   float   xyz 
// INTERP                   4   xyz         5     NONE   float   xyz 
// SV_IsFrontFace           0   x           6    FFACE    uint   x   
//
//
// Output signature:
//
// Name                 Index   Mask Register SysValue  Format   Used
// -------------------- ----- ------ -------- -------- ------- ------
// SV_Target                0   xyzw        0   TARGET   float   xyzw
// SV_Target                1   x           1   TARGET    uint   x   
//
      ps_5_0
      dcl_globalFlags refactoringAllowed
      dcl_constantbuffer CB0[131], immediateIndexed
      dcl_constantbuffer CB1[11], immediateIndexed
      dcl_constantbuffer CB2[15], immediateIndexed
      dcl_sampler s0, mode_default
      dcl_sampler s1, mode_default
      dcl_sampler s2, mode_default
      dcl_sampler s3, mode_default
      dcl_sampler s4, mode_default
      dcl_resource_texture2d (float,float,float,float) t0
      dcl_resource_texture2d (float,float,float,float) t1
      dcl_resource_texture2d (float,float,float,float) t2
      dcl_resource_texture2d (float,float,float,float) t3
      dcl_resource_texture2d (float,float,float,float) t4
      dcl_input_ps_siv linear noperspective v0.xy, position
      dcl_input_ps linear v1.xyzw
      dcl_input_ps linear v2.x
      dcl_input_ps linear v3.xyzw
      dcl_input_ps linear v4.xyz
      dcl_input_ps linear v5.xyz
      dcl_input_ps_sgv constant v6.x, is_front_face
      dcl_output o0.xyzw
      dcl_output o1.x
      dcl_temps 4
   0: dp3 r0.x, v5.xyzx, v5.xyzx
   1: sqrt r0.x, r0.x
   2: div r0.x, l(1.000000, 1.000000, 1.000000, 1.000000), r0.x
   3: mul r0.xyz, r0.xxxx, v5.xyzx
   4: dp3 r0.w, r0.xyzx, r0.xyzx
   5: rsq r0.w, r0.w
   6: mul r0.xyz, r0.wwww, r0.xyzx
   7: add r1.xyz, -v4.xyzx, cb0[21].xyzx
   8: dp3 r0.w, r1.xyzx, r1.xyzx
   9: rsq r0.w, r0.w
  10: mul r1.xyz, r0.wwww, r1.xyzx
  11: eq r0.w, cb0[25].w, l(0.000000)
  12: movc r2.x, r0.w, r1.x, cb0[66].z
  13: movc r2.y, r0.w, r1.y, cb0[67].z
  14: movc r2.z, r0.w, r1.z, cb0[68].z
  15: dp3_sat r0.x, r0.xyzx, r2.xyzx
  16: add r0.x, -r0.x, l(1.000000)
  17: log r0.x, r0.x
  18: mul r0.x, r0.x, cb2[0].y
  19: exp r0.x, r0.x
  20: mul_sat r0.x, r0.x, cb2[0].z
  21: and r0.x, r0.x, v6.x
  22: lt r0.y, cb0[22].x, l(0.000000)
  23: add r0.z, -v0.y, cb0[3].y
  24: movc r1.y, r0.y, r0.z, v0.y
  25: mov r1.x, v0.x
  26: div r1.xy, r1.xyxx, cb0[3].xyxx
  27: add r1.z, -r1.y, l(1.000000)
  28: mad r0.yz, -cb0[130].xxyx, l(0.000000, 0.500000, 0.500000, 0.000000), l(0.000000, 1.000000, 1.000000, 0.000000)
  29: min r0.yz, r0.yyzy, r1.xxzx
  30: mul r0.yz, r0.yyzy, cb0[28].xxyx
  31: sample_b_indexable(texture2d)(float,float,float,float) r0.y, r0.yzyy, t0.yxzw, s0, cb0[4].x
  32: mad r0.y, cb0[24].x, r0.y, cb0[24].y
  33: div r0.y, l(1.000000, 1.000000, 1.000000, 1.000000), r0.y
  34: mul r0.z, v4.y, cb0[79].w
  35: mad r0.z, cb0[78].w, v4.x, r0.z
  36: mad r0.z, cb0[80].w, v4.z, r0.z
  37: add r0.z, r0.z, cb0[81].w
  38: mad r0.y, r0.y, cb0[22].z, -r0.z
  39: div_sat r0.y, r0.y, cb2[14].w
  40: ge r0.z, l(0.000000), r0.y
  41: and r0.z, r0.z, l(0x3f800000)
  42: add r0.w, -r0.y, l(1.000000)
  43: mad r0.y, r0.z, r0.w, r0.y
  44: add r0.x, -r0.y, r0.x
  45: add r0.x, r0.x, l(1.000000)
  46: mul r0.z, v1.w, l(1.000100)
  47: ge r0.w, r0.z, -r0.z
  48: frc r0.z, |r0.z|
  49: movc r0.z, r0.w, r0.z, -r0.z
  50: mul r0.z, r0.z, l(0.999900)
  51: div r1.xy, cb2[10].zwzz, cb2[5].xyxx
  52: mad r0.zw, cb0[19].xxxx, r1.xxxy, r0.zzzz
  53: add r0.zw, r0.zzzw, v1.xxxy
  54: mad r0.zw, r0.zzzw, cb2[5].xxxy, cb2[5].zzzw
  55: sample_b_indexable(texture2d)(float,float,float,float) r1.xyzw, r0.zwzz, t2.xyzw, s2, cb0[4].x
  56: div r0.zw, cb2[11].xxxy, cb2[7].xxxy
  57: mad r0.zw, r0.zzzw, cb0[19].xxxx, v1.xxxy
  58: mad r0.zw, r0.zzzw, cb2[7].xxxy, cb2[7].zzzw
  59: sample_b_indexable(texture2d)(float,float,float,float) r0.zw, r0.zwzz, t3.zwxy, s3, cb0[4].x
  60: mad r2.xy, v1.xyxx, cb2[9].xyxx, cb2[9].zwzz
  61: sample_b_indexable(texture2d)(float,float,float,float) r2.xyz, r2.xyxx, t4.xyzw, s4, cb0[4].x
  62: mul r0.zw, r0.zzzw, r2.xxxy
  63: mul r0.zw, r0.zzzw, cb2[11].zzzz
  64: div r0.zw, r0.zzzw, cb2[3].xxxy
  65: div r3.xy, cb2[10].xyxx, cb2[3].xyxx
  66: mad r3.xy, r3.xyxx, cb0[19].xxxx, v1.xyxx
  67: add r0.zw, -r0.zzzw, r3.xxxy
  68: mad r0.zw, r0.zzzw, cb2[3].xxxy, cb2[3].zzzw
  69: sample_b_indexable(texture2d)(float,float,float,float) r3.xyzw, r0.zwzz, t1.xyzw, s1, cb0[4].x
  70: mul r1.xyzw, r1.wxyz, r3.wxyz
  71: ne r0.zw, l(0.000000, 0.000000, 0.000000, 0.000000), cb2[1].xxxy
  72: movc r3.xyz, r0.zzzz, r1.xxxx, r1.yzwy
  73: max r3.xyz, r0.xxxx, r3.xyzx
  74: mad_sat r0.x, r1.x, cb2[0].x, r0.x
  75: movc r1.yzw, r0.wwww, r3.xxyz, r1.yyzw
  76: add r0.z, -v1.z, l(1.000000)
  77: add_sat r3.xyz, -r0.zzzz, r2.xyzx
  78: mul_sat r2.xyz, r2.xyzx, r3.xyzx
  79: mul r2.xyz, r1.yzwy, r2.xyzx
  80: ne r3.xy, l(0.000000, 0.000000, 0.000000, 0.000000), cb2[14].yzyy
  81: movc r1.yzw, r3.xxxx, r2.xxyz, r1.yyzw
  82: mul r1.yzw, r1.yyzw, cb2[12].xxxx
  83: mul r1.yzw, r1.yyzw, v3.xxyz
  84: max r0.z, v2.x, l(1.000000)
  85: min r0.z, r0.z, l(1000000.000000)
  86: eq r2.x, v1.x, v2.x
  87: movc r0.z, r2.x, l(1.000000), r0.z
  88: mul o0.xyz, r1.yzwy, r0.zzzz
  89: mul r0.z, r0.x, r1.x
  90: ne r1.y, l(0.000000, 0.000000, 0.000000, 0.000000), cb2[0].w
  91: movc r0.x, r1.y, r0.z, r0.x
  92: movc r0.x, r0.w, r0.x, r1.x
  93: mul r0.y, r0.y, r0.x
  94: movc r0.x, r3.y, r0.y, r0.x
  95: mul r0.x, r0.x, v3.w
  96: mul_sat o0.w, r0.x, cb2[14].x
  97: and o1.x, cb0[11].x, cb1[10].x
  98: ret 
// Approximately 0 instruction slots used


 }


 // Stats for Vertex shader:
 //        d3d11: 15 math
 Pass {
  Name "DepthOnly"
  Tags { "LIGHTMODE"="DepthOnly" "QUEUE"="Transparent" "RenderType"="Transparent" "DisableBatching"="False" "RenderPipeline"="UniversalPipeline" "UniversalMaterialType"="Unlit" "ShaderGraphShader"="true" "ShaderGraphTargetId"="UniversalUnlitSubTarget" }
  Cull [_Cull]
  ColorMask R
  //////////////////////////////////
  //                              //
  //      Compiled programs       //
  //                              //
  //////////////////////////////////
//////////////////////////////////////////////////////
Keywords: <none>
-- Hardware tier variant: Tier 1
-- Vertex shader for "d3d11":
// Stats: 15 math, 2 temp registers
Uses vertex data channel "Vertex"
Uses vertex data channel "Normal"
Uses vertex data channel "TexCoord0"
Uses vertex data channel "Color"

Constant Buffer "$Globals" (2096 bytes) on slot 0 {
  Matrix4x4 unity_MatrixVP at 1248
}
Constant Buffer "UnityPerDraw" (720 bytes) on slot 1 {
  Matrix4x4 unity_ObjectToWorld at 0
  Matrix4x4 unity_WorldToObject at 64
}

Shader Disassembly:
//
// Generated by Microsoft (R) D3D Shader Disassembler
//
//
// Input signature:
//
// Name                 Index   Mask Register SysValue  Format   Used
// -------------------- ----- ------ -------- -------- ------- ------
// POSITION                 0   xyz         0     NONE   float   xyz 
// NORMAL                   0   xyz         1     NONE   float   xyz 
// TANGENT                  0   xyzw        2     NONE   float       
// TEXCOORD                 0   xyzw        3     NONE   float   xyzw
// COLOR                    0   xyzw        4     NONE   float   xyzw
//
//
// Output signature:
//
// Name                 Index   Mask Register SysValue  Format   Used
// -------------------- ----- ------ -------- -------- ------- ------
// SV_POSITION              0   xyzw        0      POS   float   xyzw
// INTERP                   0   xyzw        1     NONE   float   xyzw
// INTERP                   1   xyzw        2     NONE   float   xyzw
// INTERP                   2   xyz         3     NONE   float   xyz 
// INTERP                   3   xyz         4     NONE   float   xyz 
//
      vs_4_0
      dcl_constantbuffer CB0[82], immediateIndexed
      dcl_constantbuffer CB1[7], immediateIndexed
      dcl_input v0.xyz
      dcl_input v1.xyz
      dcl_input v3.xyzw
      dcl_input v4.xyzw
      dcl_output_siv o0.xyzw, position
      dcl_output o1.xyzw
      dcl_output o2.xyzw
      dcl_output o3.xyz
      dcl_output o4.xyz
      dcl_temps 2
   0: mul r0.xyz, v0.yyyy, cb1[1].xyzx
   1: mad r0.xyz, cb1[0].xyzx, v0.xxxx, r0.xyzx
   2: mad r0.xyz, cb1[2].xyzx, v0.zzzz, r0.xyzx
   3: add r0.xyz, r0.xyzx, cb1[3].xyzx
   4: mul r1.xyzw, r0.yyyy, cb0[79].xyzw
   5: mad r1.xyzw, cb0[78].xyzw, r0.xxxx, r1.xyzw
   6: mad r1.xyzw, cb0[80].xyzw, r0.zzzz, r1.xyzw
   7: mov o3.xyz, r0.xyzx
   8: add o0.xyzw, r1.xyzw, cb0[81].xyzw
   9: mov o1.xyzw, v3.xyzw
  10: mov o2.xyzw, v4.xyzw
  11: dp3 r0.x, v1.xyzx, cb1[4].xyzx
  12: dp3 r0.y, v1.xyzx, cb1[5].xyzx
  13: dp3 r0.z, v1.xyzx, cb1[6].xyzx
  14: dp3 r0.w, r0.xyzx, r0.xyzx
  15: max r0.w, r0.w, l(0.000000)
  16: rsq r0.w, r0.w
  17: mul o4.xyz, r0.wwww, r0.xyzx
  18: ret 
// Approximately 0 instruction slots used


-- Hardware tier variant: Tier 1
-- Fragment shader for "d3d11":
Shader Disassembly:
//
// Generated by Microsoft (R) D3D Shader Disassembler
//
//
// Input signature:
//
// Name                 Index   Mask Register SysValue  Format   Used
// -------------------- ----- ------ -------- -------- ------- ------
// SV_POSITION              0   xyzw        0      POS   float     z 
// INTERP                   0   xyzw        1     NONE   float       
// INTERP                   1   xyzw        2     NONE   float       
// INTERP                   2   xyz         3     NONE   float       
// INTERP                   3   xyz         4     NONE   float       
// SV_IsFrontFace           0   x           5    FFACE    uint       
//
//
// Output signature:
//
// Name                 Index   Mask Register SysValue  Format   Used
// -------------------- ----- ------ -------- -------- ------- ------
// SV_TARGET                0   xyzw        0   TARGET   float   xyzw
//
      ps_4_0
      dcl_input_ps_siv linear noperspective v0.z, position
      dcl_output o0.xyzw
   0: mov o0.xyzw, v0.zzzz
   1: ret 
// Approximately 0 instruction slots used


 }


 // Stats for Vertex shader:
 //        d3d11: 29 math, 2 branch
 // Stats for Fragment shader:
 //        d3d11: 6 avg math (5..7)
 Pass {
  Name "MotionVectors"
  Tags { "LIGHTMODE"="MOTIONVECTORS" "QUEUE"="Transparent" "RenderType"="Transparent" "DisableBatching"="False" "RenderPipeline"="UniversalPipeline" "UniversalMaterialType"="Unlit" "ShaderGraphShader"="true" "ShaderGraphTargetId"="UniversalUnlitSubTarget" }
  Cull [_Cull]
  ColorMask RG
  //////////////////////////////////
  //                              //
  //      Compiled programs       //
  //                              //
  //////////////////////////////////
//////////////////////////////////////////////////////
Keywords: <none>
-- Hardware tier variant: Tier 1
-- Vertex shader for "d3d11":
// Stats: 29 math, 3 temp registers, 2 branches
Uses vertex data channel "Vertex"
Uses vertex data channel "Normal"
Uses vertex data channel "TexCoord0"
Uses vertex data channel "Color"
Uses vertex data channel "TexCoord4"

Constant Buffer "$Globals" (2112 bytes) on slot 0 {
  Matrix4x4 unity_MatrixVP at 1248
  Matrix4x4 _PrevViewProjMatrix at 1424
  Matrix4x4 _NonJitteredViewProjMatrix at 1488
}
Constant Buffer "UnityPerDraw" (720 bytes) on slot 1 {
  Matrix4x4 unity_ObjectToWorld at 0
  Matrix4x4 unity_WorldToObject at 64
  Matrix4x4 unity_MatrixPreviousM at 544
  Vector4 unity_MotionVectorsParams at 672
}

Shader Disassembly:
//
// Generated by Microsoft (R) D3D Shader Disassembler
//
//
// Input signature:
//
// Name                 Index   Mask Register SysValue  Format   Used
// -------------------- ----- ------ -------- -------- ------- ------
// POSITION                 0   xyz         0     NONE   float   xyz 
// NORMAL                   0   xyz         1     NONE   float   xyz 
// TEXCOORD                 0   xyzw        2     NONE   float   xyzw
// COLOR                    0   xyzw        3     NONE   float   xyzw
// TEXCOORD                 4   xyz         4     NONE   float   xyz 
//
//
// Output signature:
//
// Name                 Index   Mask Register SysValue  Format   Used
// -------------------- ----- ------ -------- -------- ------- ------
// CLIP_POSITION_NO_JITTER     0   xyz         0     NONE   float   xyz 
// PREVIOUS_CLIP_POSITION_NO_JITTER     0   xyz         1     NONE   float   xyz 
// SV_POSITION              0   xyzw        2      POS   float   xyzw
// INTERP                   0   xyzw        3     NONE   float   xyzw
// INTERP                   1   xyzw        4     NONE   float   xyzw
// INTERP                   2   xyz         5     NONE   float   xyz 
// INTERP                   3   xyz         6     NONE   float   xyz 
//
      vs_4_0
      dcl_constantbuffer CB0[97], immediateIndexed
      dcl_constantbuffer CB1[43], immediateIndexed
      dcl_input v0.xyz
      dcl_input v1.xyz
      dcl_input v2.xyzw
      dcl_input v3.xyzw
      dcl_input v4.xyz
      dcl_output o0.xyz
      dcl_output o1.xyz
      dcl_output_siv o2.xyzw, position
      dcl_output o3.xyzw
      dcl_output o4.xyzw
      dcl_output o5.xyz
      dcl_output o6.xyz
      dcl_temps 3
   0: mul r0.xyz, v0.yyyy, cb1[1].xyzx
   1: mad r0.xyz, cb1[0].xyzx, v0.xxxx, r0.xyzx
   2: mad r0.xyz, cb1[2].xyzx, v0.zzzz, r0.xyzx
   3: add r0.xyz, r0.xyzx, cb1[3].xyzx
   4: dp3 r1.x, v1.xyzx, cb1[4].xyzx
   5: dp3 r1.y, v1.xyzx, cb1[5].xyzx
   6: dp3 r1.z, v1.xyzx, cb1[6].xyzx
   7: dp3 r0.w, r1.xyzx, r1.xyzx
   8: max r0.w, r0.w, l(0.000000)
   9: rsq r0.w, r0.w
  10: mul o6.xyz, r0.wwww, r1.xyzx
  11: mul r1.xyzw, r0.yyyy, cb0[79].xyzw
  12: mad r1.xyzw, cb0[78].xyzw, r0.xxxx, r1.xyzw
  13: mad r1.xyzw, cb0[80].xyzw, r0.zzzz, r1.xyzw
  14: add o2.xyzw, r1.xyzw, cb0[81].xyzw
  15: ne r0.w, cb1[42].y, l(0.000000)
  16: if_nz r0.w
  17:   eq r0.w, cb1[42].x, l(1.000000)
  18:   movc r1.xyz, r0.wwww, v4.xyzx, v0.xyzx
  19:   mul r2.xyz, r0.yyyy, cb0[94].xywx
  20:   mad r2.xyz, cb0[93].xywx, r0.xxxx, r2.xyzx
  21:   mad r2.xyz, cb0[95].xywx, r0.zzzz, r2.xyzx
  22:   add o0.xyz, r2.xyzx, cb0[96].xywx
  23:   mul r2.xyzw, r1.yyyy, cb1[35].xyzw
  24:   mad r2.xyzw, cb1[34].xyzw, r1.xxxx, r2.xyzw
  25:   mad r1.xyzw, cb1[36].xyzw, r1.zzzz, r2.xyzw
  26:   add r1.xyzw, r1.xyzw, cb1[37].xyzw
  27:   mul r2.xyz, r1.yyyy, cb0[90].xywx
  28:   mad r2.xyz, cb0[89].xywx, r1.xxxx, r2.xyzx
  29:   mad r1.xyz, cb0[91].xywx, r1.zzzz, r2.xyzx
  30:   mad o1.xyz, cb0[92].xywx, r1.wwww, r1.xyzx
  31: else 
  32:   mov o0.xyz, l(0,0,0,0)
  33:   mov o1.xyz, l(0,0,0,0)
  34: endif 
  35: mov o3.xyzw, v2.xyzw
  36: mov o4.xyzw, v3.xyzw
  37: mov o5.xyz, r0.xyzx
  38: ret 
// Approximately 0 instruction slots used


-- Hardware tier variant: Tier 1
-- Fragment shader for "d3d11":
// Stats: 7 math, 1 temp registers
Constant Buffer "UnityPerDraw" (720 bytes) on slot 0 {
  Vector4 unity_MotionVectorsParams at 672
}

Shader Disassembly:
//
// Generated by Microsoft (R) D3D Shader Disassembler
//
//
// Input signature:
//
// Name                 Index   Mask Register SysValue  Format   Used
// -------------------- ----- ------ -------- -------- ------- ------
// CLIP_POSITION_NO_JITTER     0   xyz         0     NONE   float   xyz 
// PREVIOUS_CLIP_POSITION_NO_JITTER     0   xyz         1     NONE   float   xyz 
// SV_POSITION              0   xyzw        2      POS   float       
// INTERP                   0   xyzw        3     NONE   float       
// INTERP                   1   xyzw        4     NONE   float       
// INTERP                   2   xyz         5     NONE   float       
// INTERP                   3   xyz         6     NONE   float       
// SV_IsFrontFace           0   x           7    FFACE    uint       
//
//
// Output signature:
//
// Name                 Index   Mask Register SysValue  Format   Used
// -------------------- ----- ------ -------- -------- ------- ------
// SV_Target                0   xyzw        0   TARGET   float   xyzw
//
      ps_4_0
      dcl_constantbuffer CB0[43], immediateIndexed
      dcl_input_ps linear v0.xyz
      dcl_input_ps linear v1.xyz
      dcl_output o0.xyzw
      dcl_temps 1
   0: div r0.x, l(1.000000, 1.000000, 1.000000, 1.000000), v1.z
   1: mul r0.xy, r0.xxxx, v1.xyxx
   2: div r0.z, l(1.000000, 1.000000, 1.000000, 1.000000), v0.z
   3: mad r0.xy, v0.xyxx, r0.zzzz, -r0.xyxx
   4: mul r0.xy, r0.xyxx, l(0.500000, -0.500000, 0.000000, 0.000000)
   5: ne r0.z, cb0[42].y, l(0.000000)
   6: and o0.xy, r0.xyxx, r0.zzzz
   7: mov o0.zw, l(0,0,0,0)
   8: ret 
// Approximately 0 instruction slots used


//////////////////////////////////////////////////////
Keywords: _WRITE_RENDERING_LAYERS
-- Vertex shader for "d3d11":
// No shader variant for this keyword set. The closest match will be used instead.

-- Hardware tier variant: Tier 1
-- Fragment shader for "d3d11":
// Stats: 5 math, 1 temp registers
Constant Buffer "UnityPerDraw" (720 bytes) on slot 0 {
  Vector4 unity_MotionVectorsParams at 672
}

Shader Disassembly:
//
// Generated by Microsoft (R) D3D Shader Disassembler
//
//
// Input signature:
//
// Name                 Index   Mask Register SysValue  Format   Used
// -------------------- ----- ------ -------- -------- ------- ------
// CLIP_POSITION_NO_JITTER     0   xyz         0     NONE   float   xyz 
// PREVIOUS_CLIP_POSITION_NO_JITTER     0   xyz         1     NONE   float   xyz 
// SV_POSITION              0   xyzw        2      POS   float       
// INTERP                   0   xyzw        3     NONE   float       
// INTERP                   1   xyzw        4     NONE   float       
// INTERP                   2   xyz         5     NONE   float       
// INTERP                   3   xyz         6     NONE   float       
// SV_IsFrontFace           0   x           7    FFACE    uint       
//
//
// Output signature:
//
// Name                 Index   Mask Register SysValue  Format   Used
// -------------------- ----- ------ -------- -------- ------- ------
// SV_Target                0   xyzw        0   TARGET   float   xyzw
//
      ps_5_0
      dcl_globalFlags refactoringAllowed
      dcl_constantbuffer CB0[43], immediateIndexed
      dcl_input_ps linear v0.xyz
      dcl_input_ps linear v1.xyz
      dcl_output o0.xyzw
      dcl_temps 1
   0: rcp r0.x, v1.z
   1: mul r0.xy, r0.xxxx, v1.xyxx
   2: rcp r0.z, v0.z
   3: mad r0.xy, v0.xyxx, r0.zzzz, -r0.xyxx
   4: mul r0.xy, r0.xyxx, l(0.500000, -0.500000, 0.000000, 0.000000)
   5: ne r0.z, cb0[42].y, l(0.000000)
   6: and o0.xy, r0.xyxx, r0.zzzz
   7: mov o0.zw, l(0,0,0,0)
   8: ret 
// Approximately 0 instruction slots used


 }


 // Stats for Vertex shader:
 //        d3d11: 15 math
 // Stats for Fragment shader:
 //        d3d11: 3 avg math (3..4)
 Pass {
  Name "DepthNormalsOnly"
  Tags { "LIGHTMODE"="DepthNormalsOnly" "QUEUE"="Transparent" "RenderType"="Transparent" "DisableBatching"="False" "RenderPipeline"="UniversalPipeline" "UniversalMaterialType"="Unlit" "ShaderGraphShader"="true" "ShaderGraphTargetId"="UniversalUnlitSubTarget" }
  Cull [_Cull]
  //////////////////////////////////
  //                              //
  //      Compiled programs       //
  //                              //
  //////////////////////////////////
//////////////////////////////////////////////////////
Keywords: <none>
-- Hardware tier variant: Tier 1
-- Vertex shader for "d3d11":
// Stats: 15 math, 2 temp registers
Uses vertex data channel "Vertex"
Uses vertex data channel "Normal"
Uses vertex data channel "TexCoord0"
Uses vertex data channel "Color"

Constant Buffer "$Globals" (2096 bytes) on slot 0 {
  Matrix4x4 unity_MatrixVP at 1248
}
Constant Buffer "UnityPerDraw" (720 bytes) on slot 1 {
  Matrix4x4 unity_ObjectToWorld at 0
  Matrix4x4 unity_WorldToObject at 64
}

Shader Disassembly:
//
// Generated by Microsoft (R) D3D Shader Disassembler
//
//
// Input signature:
//
// Name                 Index   Mask Register SysValue  Format   Used
// -------------------- ----- ------ -------- -------- ------- ------
// POSITION                 0   xyz         0     NONE   float   xyz 
// NORMAL                   0   xyz         1     NONE   float   xyz 
// TANGENT                  0   xyzw        2     NONE   float       
// TEXCOORD                 0   xyzw        3     NONE   float   xyzw
// COLOR                    0   xyzw        4     NONE   float   xyzw
//
//
// Output signature:
//
// Name                 Index   Mask Register SysValue  Format   Used
// -------------------- ----- ------ -------- -------- ------- ------
// SV_POSITION              0   xyzw        0      POS   float   xyzw
// INTERP                   0   xyzw        1     NONE   float   xyzw
// INTERP                   1   xyzw        2     NONE   float   xyzw
// INTERP                   2   xyz         3     NONE   float   xyz 
// INTERP                   3   xyz         4     NONE   float   xyz 
//
      vs_4_0
      dcl_constantbuffer CB0[82], immediateIndexed
      dcl_constantbuffer CB1[7], immediateIndexed
      dcl_input v0.xyz
      dcl_input v1.xyz
      dcl_input v3.xyzw
      dcl_input v4.xyzw
      dcl_output_siv o0.xyzw, position
      dcl_output o1.xyzw
      dcl_output o2.xyzw
      dcl_output o3.xyz
      dcl_output o4.xyz
      dcl_temps 2
   0: mul r0.xyz, v0.yyyy, cb1[1].xyzx
   1: mad r0.xyz, cb1[0].xyzx, v0.xxxx, r0.xyzx
   2: mad r0.xyz, cb1[2].xyzx, v0.zzzz, r0.xyzx
   3: add r0.xyz, r0.xyzx, cb1[3].xyzx
   4: mul r1.xyzw, r0.yyyy, cb0[79].xyzw
   5: mad r1.xyzw, cb0[78].xyzw, r0.xxxx, r1.xyzw
   6: mad r1.xyzw, cb0[80].xyzw, r0.zzzz, r1.xyzw
   7: mov o3.xyz, r0.xyzx
   8: add o0.xyzw, r1.xyzw, cb0[81].xyzw
   9: mov o1.xyzw, v3.xyzw
  10: mov o2.xyzw, v4.xyzw
  11: dp3 r0.x, v1.xyzx, cb1[4].xyzx
  12: dp3 r0.y, v1.xyzx, cb1[5].xyzx
  13: dp3 r0.z, v1.xyzx, cb1[6].xyzx
  14: dp3 r0.w, r0.xyzx, r0.xyzx
  15: max r0.w, r0.w, l(0.000000)
  16: rsq r0.w, r0.w
  17: mul o4.xyz, r0.wwww, r0.xyzx
  18: ret 
// Approximately 0 instruction slots used


-- Hardware tier variant: Tier 1
-- Fragment shader for "d3d11":
// Stats: 3 math, 1 temp registers
Shader Disassembly:
//
// Generated by Microsoft (R) D3D Shader Disassembler
//
//
// Input signature:
//
// Name                 Index   Mask Register SysValue  Format   Used
// -------------------- ----- ------ -------- -------- ------- ------
// SV_POSITION              0   xyzw        0      POS   float       
// INTERP                   0   xyzw        1     NONE   float       
// INTERP                   1   xyzw        2     NONE   float       
// INTERP                   2   xyz         3     NONE   float       
// INTERP                   3   xyz         4     NONE   float   xyz 
// SV_IsFrontFace           0   x           5    FFACE    uint       
//
//
// Output signature:
//
// Name                 Index   Mask Register SysValue  Format   Used
// -------------------- ----- ------ -------- -------- ------- ------
// SV_Target                0   xyzw        0   TARGET   float   xyzw
//
      ps_4_0
      dcl_input_ps linear v4.xyz
      dcl_output o0.xyzw
      dcl_temps 1
   0: dp3 r0.x, v4.xyzx, v4.xyzx
   1: rsq r0.x, r0.x
   2: mul o0.xyz, r0.xxxx, v4.xyzx
   3: mov o0.w, l(0)
   4: ret 
// Approximately 0 instruction slots used


//////////////////////////////////////////////////////
Keywords: _SURFACE_TYPE_TRANSPARENT
-- Vertex shader for "d3d11":
// No shader variant for this keyword set. The closest match will be used instead.

-- Hardware tier variant: Tier 1
-- Fragment shader for "d3d11":
// Stats: 3 math, 1 temp registers
Shader Disassembly:
//
// Generated by Microsoft (R) D3D Shader Disassembler
//
//
// Input signature:
//
// Name                 Index   Mask Register SysValue  Format   Used
// -------------------- ----- ------ -------- -------- ------- ------
// SV_POSITION              0   xyzw        0      POS   float       
// INTERP                   0   xyzw        1     NONE   float       
// INTERP                   1   xyzw        2     NONE   float       
// INTERP                   2   xyz         3     NONE   float       
// INTERP                   3   xyz         4     NONE   float   xyz 
// SV_IsFrontFace           0   x           5    FFACE    uint       
//
//
// Output signature:
//
// Name                 Index   Mask Register SysValue  Format   Used
// -------------------- ----- ------ -------- -------- ------- ------
// SV_Target                0   xyzw        0   TARGET   float   xyzw
//
      ps_4_0
      dcl_input_ps linear v4.xyz
      dcl_output o0.xyzw
      dcl_temps 1
   0: dp3 r0.x, v4.xyzx, v4.xyzx
   1: rsq r0.x, r0.x
   2: mul o0.xyz, r0.xxxx, v4.xyzx
   3: mov o0.w, l(0)
   4: ret 
// Approximately 0 instruction slots used


//////////////////////////////////////////////////////
Keywords: _WRITE_RENDERING_LAYERS
-- Vertex shader for "d3d11":
// No shader variant for this keyword set. The closest match will be used instead.

-- Hardware tier variant: Tier 1
-- Fragment shader for "d3d11":
// Stats: 4 math, 1 temp registers
Constant Buffer "$Globals" (2096 bytes) on slot 0 {
  ScalarInt _RenderingLayerMaxInt at 176
}
Constant Buffer "UnityPerDraw" (720 bytes) on slot 1 {
  Vector4 unity_RenderingLayer at 160
}

Shader Disassembly:
//
// Generated by Microsoft (R) D3D Shader Disassembler
//
//
// Input signature:
//
// Name                 Index   Mask Register SysValue  Format   Used
// -------------------- ----- ------ -------- -------- ------- ------
// SV_POSITION              0   xyzw        0      POS   float       
// INTERP                   0   xyzw        1     NONE   float       
// INTERP                   1   xyzw        2     NONE   float       
// INTERP                   2   xyz         3     NONE   float       
// INTERP                   3   xyz         4     NONE   float   xyz 
// SV_IsFrontFace           0   x           5    FFACE    uint       
//
//
// Output signature:
//
// Name                 Index   Mask Register SysValue  Format   Used
// -------------------- ----- ------ -------- -------- ------- ------
// SV_Target                0   xyzw        0   TARGET   float   xyzw
// SV_Target                1   x           1   TARGET    uint   x   
//
      ps_5_0
      dcl_globalFlags refactoringAllowed
      dcl_constantbuffer CB0[12], immediateIndexed
      dcl_constantbuffer CB1[11], immediateIndexed
      dcl_input_ps linear v4.xyz
      dcl_output o0.xyzw
      dcl_output o1.x
      dcl_temps 1
   0: dp3 r0.x, v4.xyzx, v4.xyzx
   1: rsq r0.x, r0.x
   2: mul o0.xyz, r0.xxxx, v4.xyzx
   3: mov o0.w, l(0)
   4: and o1.x, cb0[11].x, cb1[10].x
   5: ret 
// Approximately 0 instruction slots used


//////////////////////////////////////////////////////
Keywords: _SURFACE_TYPE_TRANSPARENT _WRITE_RENDERING_LAYERS
-- Vertex shader for "d3d11":
// No shader variant for this keyword set. The closest match will be used instead.

-- Hardware tier variant: Tier 1
-- Fragment shader for "d3d11":
// Stats: 4 math, 1 temp registers
Constant Buffer "$Globals" (2096 bytes) on slot 0 {
  ScalarInt _RenderingLayerMaxInt at 176
}
Constant Buffer "UnityPerDraw" (720 bytes) on slot 1 {
  Vector4 unity_RenderingLayer at 160
}

Shader Disassembly:
//
// Generated by Microsoft (R) D3D Shader Disassembler
//
//
// Input signature:
//
// Name                 Index   Mask Register SysValue  Format   Used
// -------------------- ----- ------ -------- -------- ------- ------
// SV_POSITION              0   xyzw        0      POS   float       
// INTERP                   0   xyzw        1     NONE   float       
// INTERP                   1   xyzw        2     NONE   float       
// INTERP                   2   xyz         3     NONE   float       
// INTERP                   3   xyz         4     NONE   float   xyz 
// SV_IsFrontFace           0   x           5    FFACE    uint       
//
//
// Output signature:
//
// Name                 Index   Mask Register SysValue  Format   Used
// -------------------- ----- ------ -------- -------- ------- ------
// SV_Target                0   xyzw        0   TARGET   float   xyzw
// SV_Target                1   x           1   TARGET    uint   x   
//
      ps_5_0
      dcl_globalFlags refactoringAllowed
      dcl_constantbuffer CB0[12], immediateIndexed
      dcl_constantbuffer CB1[11], immediateIndexed
      dcl_input_ps linear v4.xyz
      dcl_output o0.xyzw
      dcl_output o1.x
      dcl_temps 1
   0: dp3 r0.x, v4.xyzx, v4.xyzx
   1: rsq r0.x, r0.x
   2: mul o0.xyz, r0.xxxx, v4.xyzx
   3: mov o0.w, l(0)
   4: and o1.x, cb0[11].x, cb1[10].x
   5: ret 
// Approximately 0 instruction slots used


 }


 // Stats for Vertex shader:
 //        d3d11: 28 avg math (26..30)
 Pass {
  Name "ShadowCaster"
  Tags { "LIGHTMODE"="SHADOWCASTER" "QUEUE"="Transparent" "RenderType"="Transparent" "DisableBatching"="False" "RenderPipeline"="UniversalPipeline" "UniversalMaterialType"="Unlit" "ShaderGraphShader"="true" "ShaderGraphTargetId"="UniversalUnlitSubTarget" }
  Cull [_Cull]
  ColorMask 0
  //////////////////////////////////
  //                              //
  //      Compiled programs       //
  //                              //
  //////////////////////////////////
//////////////////////////////////////////////////////
Keywords: <none>
-- Hardware tier variant: Tier 1
-- Vertex shader for "d3d11":
// Stats: 26 math, 3 temp registers
Uses vertex data channel "Vertex"
Uses vertex data channel "Normal"
Uses vertex data channel "TexCoord0"
Uses vertex data channel "Color"

Constant Buffer "$Globals" (2128 bytes) on slot 0 {
  Matrix4x4 unity_MatrixVP at 1248
  Vector4 _ShadowBias at 2064
  Vector3 _LightDirection at 2096
}
Constant Buffer "UnityPerDraw" (720 bytes) on slot 1 {
  Matrix4x4 unity_ObjectToWorld at 0
  Matrix4x4 unity_WorldToObject at 64
}

Shader Disassembly:
//
// Generated by Microsoft (R) D3D Shader Disassembler
//
//
// Input signature:
//
// Name                 Index   Mask Register SysValue  Format   Used
// -------------------- ----- ------ -------- -------- ------- ------
// POSITION                 0   xyz         0     NONE   float   xyz 
// NORMAL                   0   xyz         1     NONE   float   xyz 
// TANGENT                  0   xyzw        2     NONE   float       
// TEXCOORD                 0   xyzw        3     NONE   float   xyzw
// COLOR                    0   xyzw        4     NONE   float   xyzw
//
//
// Output signature:
//
// Name                 Index   Mask Register SysValue  Format   Used
// -------------------- ----- ------ -------- -------- ------- ------
// SV_POSITION              0   xyzw        0      POS   float   xyzw
// INTERP                   0   xyzw        1     NONE   float   xyzw
// INTERP                   1   xyzw        2     NONE   float   xyzw
// INTERP                   2   xyz         3     NONE   float   xyz 
// INTERP                   3   xyz         4     NONE   float   xyz 
//
      vs_4_0
      dcl_constantbuffer CB0[132], immediateIndexed
      dcl_constantbuffer CB1[7], immediateIndexed
      dcl_input v0.xyz
      dcl_input v1.xyz
      dcl_input v3.xyzw
      dcl_input v4.xyzw
      dcl_output_siv o0.xyzw, position
      dcl_output o1.xyzw
      dcl_output o2.xyzw
      dcl_output o3.xyz
      dcl_output o4.xyz
      dcl_temps 3
   0: dp3 r0.x, v1.xyzx, cb1[4].xyzx
   1: dp3 r0.y, v1.xyzx, cb1[5].xyzx
   2: dp3 r0.z, v1.xyzx, cb1[6].xyzx
   3: dp3 r0.w, r0.xyzx, r0.xyzx
   4: max r0.w, r0.w, l(0.000000)
   5: rsq r0.w, r0.w
   6: mul r0.xyz, r0.wwww, r0.xyzx
   7: dp3_sat r0.w, cb0[131].xyzx, r0.xyzx
   8: add r0.w, -r0.w, l(1.000000)
   9: mul r0.w, r0.w, cb0[129].y
  10: mul r1.xyz, v0.yyyy, cb1[1].xyzx
  11: mad r1.xyz, cb1[0].xyzx, v0.xxxx, r1.xyzx
  12: mad r1.xyz, cb1[2].xyzx, v0.zzzz, r1.xyzx
  13: add r1.xyz, r1.xyzx, cb1[3].xyzx
  14: mad r2.xyz, cb0[131].xyzx, cb0[129].xxxx, r1.xyzx
  15: mov o3.xyz, r1.xyzx
  16: mad r1.xyz, r0.xyzx, r0.wwww, r2.xyzx
  17: mov o4.xyz, r0.xyzx
  18: mul r0.xyzw, r1.yyyy, cb0[79].xyzw
  19: mad r0.xyzw, cb0[78].xyzw, r1.xxxx, r0.xyzw
  20: mad r0.xyzw, cb0[80].xyzw, r1.zzzz, r0.xyzw
  21: add r0.xyzw, r0.xyzw, cb0[81].xyzw
  22: min r1.x, r0.w, r0.z
  23: add r1.x, -r0.z, r1.x
  24: round_ne r1.y, cb0[129].z
  25: eq r1.y, r1.y, l(1.000000)
  26: and r1.y, r1.y, l(0x3f800000)
  27: mad o0.z, r1.y, r1.x, r0.z
  28: mov o0.xyw, r0.xyxw
  29: mov o1.xyzw, v3.xyzw
  30: mov o2.xyzw, v4.xyzw
  31: ret 
// Approximately 0 instruction slots used


-- Hardware tier variant: Tier 1
-- Fragment shader for "d3d11":
Shader Disassembly:
//
// Generated by Microsoft (R) D3D Shader Disassembler
//
//
// Input signature:
//
// Name                 Index   Mask Register SysValue  Format   Used
// -------------------- ----- ------ -------- -------- ------- ------
// SV_POSITION              0   xyzw        0      POS   float       
// INTERP                   0   xyzw        1     NONE   float       
// INTERP                   1   xyzw        2     NONE   float       
// INTERP                   2   xyz         3     NONE   float       
// INTERP                   3   xyz         4     NONE   float       
// SV_IsFrontFace           0   x           5    FFACE    uint       
//
//
// Output signature:
//
// Name                 Index   Mask Register SysValue  Format   Used
// -------------------- ----- ------ -------- -------- ------- ------
// SV_TARGET                0   xyzw        0   TARGET   float   xyzw
//
      ps_4_0
      dcl_output o0.xyzw
   0: mov o0.xyzw, l(0,0,0,0)
   1: ret 
// Approximately 0 instruction slots used


//////////////////////////////////////////////////////
Keywords: _CASTING_PUNCTUAL_LIGHT_SHADOW
-- Hardware tier variant: Tier 1
-- Vertex shader for "d3d11":
// Stats: 30 math, 3 temp registers
Uses vertex data channel "Vertex"
Uses vertex data channel "Normal"
Uses vertex data channel "TexCoord0"
Uses vertex data channel "Color"

Constant Buffer "$Globals" (2128 bytes) on slot 0 {
  Matrix4x4 unity_MatrixVP at 1248
  Vector4 _ShadowBias at 2064
  Vector3 _LightPosition at 2112
}
Constant Buffer "UnityPerDraw" (720 bytes) on slot 1 {
  Matrix4x4 unity_ObjectToWorld at 0
  Matrix4x4 unity_WorldToObject at 64
}

Shader Disassembly:
//
// Generated by Microsoft (R) D3D Shader Disassembler
//
//
// Input signature:
//
// Name                 Index   Mask Register SysValue  Format   Used
// -------------------- ----- ------ -------- -------- ------- ------
// POSITION                 0   xyz         0     NONE   float   xyz 
// NORMAL                   0   xyz         1     NONE   float   xyz 
// TANGENT                  0   xyzw        2     NONE   float       
// TEXCOORD                 0   xyzw        3     NONE   float   xyzw
// COLOR                    0   xyzw        4     NONE   float   xyzw
//
//
// Output signature:
//
// Name                 Index   Mask Register SysValue  Format   Used
// -------------------- ----- ------ -------- -------- ------- ------
// SV_POSITION              0   xyzw        0      POS   float   xyzw
// INTERP                   0   xyzw        1     NONE   float   xyzw
// INTERP                   1   xyzw        2     NONE   float   xyzw
// INTERP                   2   xyz         3     NONE   float   xyz 
// INTERP                   3   xyz         4     NONE   float   xyz 
//
      vs_4_0
      dcl_constantbuffer CB0[133], immediateIndexed
      dcl_constantbuffer CB1[7], immediateIndexed
      dcl_input v0.xyz
      dcl_input v1.xyz
      dcl_input v3.xyzw
      dcl_input v4.xyzw
      dcl_output_siv o0.xyzw, position
      dcl_output o1.xyzw
      dcl_output o2.xyzw
      dcl_output o3.xyz
      dcl_output o4.xyz
      dcl_temps 3
   0: mul r0.xyz, v0.yyyy, cb1[1].xyzx
   1: mad r0.xyz, cb1[0].xyzx, v0.xxxx, r0.xyzx
   2: mad r0.xyz, cb1[2].xyzx, v0.zzzz, r0.xyzx
   3: add r0.xyz, r0.xyzx, cb1[3].xyzx
   4: add r1.xyz, -r0.xyzx, cb0[132].xyzx
   5: dp3 r0.w, r1.xyzx, r1.xyzx
   6: rsq r0.w, r0.w
   7: mul r1.xyz, r0.wwww, r1.xyzx
   8: mad r2.xyz, r1.xyzx, cb0[129].xxxx, r0.xyzx
   9: mov o3.xyz, r0.xyzx
  10: dp3 r0.x, v1.xyzx, cb1[4].xyzx
  11: dp3 r0.y, v1.xyzx, cb1[5].xyzx
  12: dp3 r0.z, v1.xyzx, cb1[6].xyzx
  13: dp3 r0.w, r0.xyzx, r0.xyzx
  14: max r0.w, r0.w, l(0.000000)
  15: rsq r0.w, r0.w
  16: mul r0.xyz, r0.wwww, r0.xyzx
  17: dp3_sat r0.w, r1.xyzx, r0.xyzx
  18: add r0.w, -r0.w, l(1.000000)
  19: mul r0.w, r0.w, cb0[129].y
  20: mad r1.xyz, r0.xyzx, r0.wwww, r2.xyzx
  21: mov o4.xyz, r0.xyzx
  22: mul r0.xyzw, r1.yyyy, cb0[79].xyzw
  23: mad r0.xyzw, cb0[78].xyzw, r1.xxxx, r0.xyzw
  24: mad r0.xyzw, cb0[80].xyzw, r1.zzzz, r0.xyzw
  25: add r0.xyzw, r0.xyzw, cb0[81].xyzw
  26: min r1.x, r0.w, r0.z
  27: add r1.x, -r0.z, r1.x
  28: round_ne r1.y, cb0[129].z
  29: eq r1.y, r1.y, l(1.000000)
  30: and r1.y, r1.y, l(0x3f800000)
  31: mad o0.z, r1.y, r1.x, r0.z
  32: mov o0.xyw, r0.xyxw
  33: mov o1.xyzw, v3.xyzw
  34: mov o2.xyzw, v4.xyzw
  35: ret 
// Approximately 0 instruction slots used


-- Fragment shader for "d3d11":
// No shader variant for this keyword set. The closest match will be used instead.

 }


 // Stats for Vertex shader:
 //        d3d11: 15 math
 // Stats for Fragment shader:
 //        d3d11: 75 avg math (73..83)
 Pass {
  Name "GBuffer"
  Tags { "LIGHTMODE"="UniversalGBuffer" "QUEUE"="Transparent" "RenderType"="Transparent" "DisableBatching"="False" "RenderPipeline"="UniversalPipeline" "UniversalMaterialType"="Unlit" "ShaderGraphShader"="true" "ShaderGraphTargetId"="UniversalUnlitSubTarget" }
  ZTest [_ZTest]
  ZWrite [_ZWrite]
  Cull [_Cull]
  Blend [_SrcBlend] [_DstBlend], [_SrcBlendAlpha] [_DstBlendAlpha]
  //////////////////////////////////
  //                              //
  //      Compiled programs       //
  //                              //
  //////////////////////////////////
//////////////////////////////////////////////////////
Keywords: <none>
-- Hardware tier variant: Tier 1
-- Vertex shader for "d3d11":
// Stats: 15 math, 2 temp registers
Uses vertex data channel "Vertex"
Uses vertex data channel "Normal"
Uses vertex data channel "TexCoord0"
Uses vertex data channel "TexCoord1"
Uses vertex data channel "Color"

Constant Buffer "$Globals" (2096 bytes) on slot 0 {
  Matrix4x4 unity_MatrixVP at 1248
}
Constant Buffer "UnityPerDraw" (720 bytes) on slot 1 {
  Matrix4x4 unity_ObjectToWorld at 0
  Matrix4x4 unity_WorldToObject at 64
}

Shader Disassembly:
//
// Generated by Microsoft (R) D3D Shader Disassembler
//
//
// Input signature:
//
// Name                 Index   Mask Register SysValue  Format   Used
// -------------------- ----- ------ -------- -------- ------- ------
// POSITION                 0   xyz         0     NONE   float   xyz 
// NORMAL                   0   xyz         1     NONE   float   xyz 
// TANGENT                  0   xyzw        2     NONE   float       
// TEXCOORD                 0   xyzw        3     NONE   float   xyzw
// TEXCOORD                 1   xyzw        4     NONE   float   xyzw
// COLOR                    0   xyzw        5     NONE   float   xyzw
//
//
// Output signature:
//
// Name                 Index   Mask Register SysValue  Format   Used
// -------------------- ----- ------ -------- -------- ------- ------
// SV_POSITION              0   xyzw        0      POS   float   xyzw
// INTERP                   0   xyz         1     NONE   float   xyz 
// INTERP                   2   xyzw        2     NONE   float   xyzw
// INTERP                   3   xyzw        3     NONE   float   xyzw
// INTERP                   4   xyzw        4     NONE   float   xyzw
// INTERP                   5   xyz         5     NONE   float   xyz 
// INTERP                   6   xyz         6     NONE   float   xyz 
//
      vs_5_0
      dcl_globalFlags refactoringAllowed
      dcl_constantbuffer CB0[82], immediateIndexed
      dcl_constantbuffer CB1[7], immediateIndexed
      dcl_input v0.xyz
      dcl_input v1.xyz
      dcl_input v3.xyzw
      dcl_input v4.xyzw
      dcl_input v5.xyzw
      dcl_output_siv o0.xyzw, position
      dcl_output o1.xyz
      dcl_output o2.xyzw
      dcl_output o3.xyzw
      dcl_output o4.xyzw
      dcl_output o5.xyz
      dcl_output o6.xyz
      dcl_temps 2
   0: mul r0.xyz, v0.yyyy, cb1[1].xyzx
   1: mad r0.xyz, cb1[0].xyzx, v0.xxxx, r0.xyzx
   2: mad r0.xyz, cb1[2].xyzx, v0.zzzz, r0.xyzx
   3: add r0.xyz, r0.xyzx, cb1[3].xyzx
   4: mul r1.xyzw, r0.yyyy, cb0[79].xyzw
   5: mad r1.xyzw, cb0[78].xyzw, r0.xxxx, r1.xyzw
   6: mad r1.xyzw, cb0[80].xyzw, r0.zzzz, r1.xyzw
   7: mov o5.xyz, r0.xyzx
   8: add o0.xyzw, r1.xyzw, cb0[81].xyzw
   9: mov o1.xyz, l(0,0,0,0)
  10: mov o2.xyzw, v3.xyzw
  11: mov o3.xyzw, v4.xyzw
  12: mov o4.xyzw, v5.xyzw
  13: dp3 r0.x, v1.xyzx, cb1[4].xyzx
  14: dp3 r0.y, v1.xyzx, cb1[5].xyzx
  15: dp3 r0.z, v1.xyzx, cb1[6].xyzx
  16: dp3 r0.w, r0.xyzx, r0.xyzx
  17: max r0.w, r0.w, l(0.000000)
  18: rsq r0.w, r0.w
  19: mul o6.xyz, r0.wwww, r0.xyzx
  20: ret 
// Approximately 0 instruction slots used


-- Hardware tier variant: Tier 1
-- Fragment shader for "d3d11":
// Stats: 73 math, 4 temp registers
Set 2D Texture "_CameraDepthTexture" to slot 0 sampler slot -1
Set 2D Texture "_MainTex" to slot 1
Set 2D Texture "_Noise" to slot 2
Set 2D Texture "_Flow" to slot 3
Set 2D Texture "_Mask" to slot 4

Set Sampler PointClamp to slot 0

Constant Buffer "$Globals" (2096 bytes) on slot 0 {
  Matrix4x4 unity_MatrixV at 1056
  Matrix4x4 unity_MatrixVP at 1248
  Vector4 _ScaledScreenParams at 48
  Vector2 _GlobalMipBias at 64
  Vector4 _TimeParameters at 304
  Vector3 _WorldSpaceCameraPos at 336
  Vector4 _ProjectionParams at 352
  Vector4 _ZBufferParams at 384
  Vector4 unity_OrthoParams at 400
  Vector4 _RTHandleScale at 448
  Vector4 _CameraDepthTexture_TexelSize at 2080
}
Constant Buffer "UnityPerMaterial" (304 bytes) on slot 1 {
  Float _Fresnelpower at 4
  Float _Fresnelscale at 8
  Float _Useonlycolor at 16
  Float _Use_fresnel at 20
  Vector4 _MainTex_ST at 48
  Vector4 _Noise_ST at 80
  Vector4 _Flow_ST at 112
  Vector4 _Mask_ST at 144
  Vector4 _SpeedMainTexUVNoiseZW at 160
  Vector4 _DistortionSpeedXYPowerZ at 176
  Float _Emission at 192
  Float _Usecenterglow at 228
  Float _Depthpower at 236
}

Shader Disassembly:
//
// Generated by Microsoft (R) D3D Shader Disassembler
//
//
// Input signature:
//
// Name                 Index   Mask Register SysValue  Format   Used
// -------------------- ----- ------ -------- -------- ------- ------
// SV_POSITION              0   xyzw        0      POS   float   xy  
// INTERP                   0   xyz         1     NONE   float       
// INTERP                   2   xyzw        2     NONE   float   xyzw
// INTERP                   3   xyzw        3     NONE   float   x   
// INTERP                   4   xyzw        4     NONE   float   xyz 
// INTERP                   5   xyz         5     NONE   float   xyz 
// INTERP                   6   xyz         6     NONE   float   xyz 
// SV_IsFrontFace           0   x           7    FFACE    uint   x   
//
//
// Output signature:
//
// Name                 Index   Mask Register SysValue  Format   Used
// -------------------- ----- ------ -------- -------- ------- ------
// SV_Target                0   xyzw        0   TARGET   float   xyzw
// SV_Target                1   xyzw        1   TARGET   float   xyzw
// SV_Target                2   xyzw        2   TARGET   float   xyzw
// SV_Target                3   xyzw        3   TARGET   float   xyzw
//
      ps_5_0
      dcl_globalFlags refactoringAllowed
      dcl_constantbuffer CB0[131], immediateIndexed
      dcl_constantbuffer CB1[15], immediateIndexed
      dcl_sampler s0, mode_default
      dcl_sampler s1, mode_default
      dcl_sampler s2, mode_default
      dcl_sampler s3, mode_default
      dcl_sampler s4, mode_default
      dcl_resource_texture2d (float,float,float,float) t0
      dcl_resource_texture2d (float,float,float,float) t1
      dcl_resource_texture2d (float,float,float,float) t2
      dcl_resource_texture2d (float,float,float,float) t3
      dcl_resource_texture2d (float,float,float,float) t4
      dcl_input_ps_siv linear noperspective v0.xy, position
      dcl_input_ps linear v2.xyzw
      dcl_input_ps linear v3.x
      dcl_input_ps linear v4.xyz
      dcl_input_ps linear v5.xyz
      dcl_input_ps linear v6.xyz
      dcl_input_ps_sgv constant v7.x, is_front_face
      dcl_output o0.xyzw
      dcl_output o1.xyzw
      dcl_output o2.xyzw
      dcl_output o3.xyzw
      dcl_temps 4
   0: lt r0.x, cb0[22].x, l(0.000000)
   1: add r0.y, -v0.y, cb0[3].y
   2: movc r0.y, r0.x, r0.y, v0.y
   3: mov r0.x, v0.x
   4: div r0.xy, r0.xyxx, cb0[3].xyxx
   5: add r0.z, -r0.y, l(1.000000)
   6: mad r0.yw, -cb0[130].xxxy, l(0.000000, 0.500000, 0.000000, 0.500000), l(0.000000, 1.000000, 0.000000, 1.000000)
   7: min r0.xy, r0.ywyy, r0.xzxx
   8: mul r0.xy, r0.xyxx, cb0[28].xyxx
   9: sample_b_indexable(texture2d)(float,float,float,float) r0.x, r0.xyxx, t0.xyzw, s0, cb0[4].x
  10: mad r0.x, cb0[24].x, r0.x, cb0[24].y
  11: div r0.x, l(1.000000, 1.000000, 1.000000, 1.000000), r0.x
  12: mul r0.y, v5.y, cb0[79].w
  13: mad r0.y, cb0[78].w, v5.x, r0.y
  14: mad r0.y, cb0[80].w, v5.z, r0.y
  15: add r0.y, r0.y, cb0[81].w
  16: mad r0.x, r0.x, cb0[22].z, -r0.y
  17: div_sat r0.x, r0.x, cb1[14].w
  18: ge r0.y, l(0.000000), r0.x
  19: and r0.y, r0.y, l(0x3f800000)
  20: add r0.z, -r0.x, l(1.000000)
  21: mad r0.x, r0.y, r0.z, r0.x
  22: dp3 r0.y, v6.xyzx, v6.xyzx
  23: sqrt r0.z, r0.y
  24: rsq r0.y, r0.y
  25: mul o2.xyz, r0.yyyy, v6.xyzx
  26: div r0.y, l(1.000000, 1.000000, 1.000000, 1.000000), r0.z
  27: mul r0.yzw, r0.yyyy, v6.xxyz
  28: dp3 r1.x, r0.yzwy, r0.yzwy
  29: rsq r1.x, r1.x
  30: mul r0.yzw, r0.yyzw, r1.xxxx
  31: add r1.xyz, -v5.xyzx, cb0[21].xyzx
  32: dp3 r1.w, r1.xyzx, r1.xyzx
  33: rsq r1.w, r1.w
  34: mul r1.xyz, r1.wwww, r1.xyzx
  35: eq r1.w, cb0[25].w, l(0.000000)
  36: movc r2.x, r1.w, r1.x, cb0[66].z
  37: movc r2.y, r1.w, r1.y, cb0[67].z
  38: movc r2.z, r1.w, r1.z, cb0[68].z
  39: dp3_sat r0.y, r0.yzwy, r2.xyzx
  40: add r0.y, -r0.y, l(1.000000)
  41: log r0.y, r0.y
  42: mul r0.y, r0.y, cb1[0].y
  43: exp r0.y, r0.y
  44: mul_sat r0.y, r0.y, cb1[0].z
  45: and r0.y, r0.y, v7.x
  46: add r0.x, -r0.x, r0.y
  47: add r0.x, r0.x, l(1.000000)
  48: mul r0.y, v2.w, l(1.000100)
  49: ge r0.z, r0.y, -r0.y
  50: frc r0.y, |r0.y|
  51: movc r0.y, r0.z, r0.y, -r0.y
  52: mul r0.y, r0.y, l(0.999900)
  53: div r0.zw, cb1[10].zzzw, cb1[5].xxxy
  54: mad r0.yz, cb0[19].xxxx, r0.zzwz, r0.yyyy
  55: add r0.yz, r0.yyzy, v2.xxyx
  56: mad r0.yz, r0.yyzy, cb1[5].xxyx, cb1[5].zzwz
  57: sample_b_indexable(texture2d)(float,float,float,float) r1.xyzw, r0.yzyy, t2.xyzw, s2, cb0[4].x
  58: div r0.yz, cb1[11].xxyx, cb1[7].xxyx
  59: mad r0.yz, r0.yyzy, cb0[19].xxxx, v2.xxyx
  60: mad r0.yz, r0.yyzy, cb1[7].xxyx, cb1[7].zzwz
  61: sample_b_indexable(texture2d)(float,float,float,float) r0.yz, r0.yzyy, t3.zxyw, s3, cb0[4].x
  62: mad r2.xy, v2.xyxx, cb1[9].xyxx, cb1[9].zwzz
  63: sample_b_indexable(texture2d)(float,float,float,float) r2.xyz, r2.xyxx, t4.xyzw, s4, cb0[4].x
  64: mul r0.yz, r0.yyzy, r2.xxyx
  65: mul r0.yz, r0.yyzy, cb1[11].zzzz
  66: div r0.yz, r0.yyzy, cb1[3].xxyx
  67: div r3.xy, cb1[10].xyxx, cb1[3].xyxx
  68: mad r3.xy, r3.xyxx, cb0[19].xxxx, v2.xyxx
  69: add r0.yz, -r0.yyzy, r3.xxyx
  70: mad r0.yz, r0.yyzy, cb1[3].xxyx, cb1[3].zzwz
  71: sample_b_indexable(texture2d)(float,float,float,float) r3.xyzw, r0.yzyy, t1.xyzw, s1, cb0[4].x
  72: mul r1.xyzw, r1.wxyz, r3.wxyz
  73: ne r0.yz, l(0.000000, 0.000000, 0.000000, 0.000000), cb1[1].xxyx
  74: movc r3.xyz, r0.yyyy, r1.xxxx, r1.yzwy
  75: max r0.xyw, r0.xxxx, r3.xyxz
  76: movc r0.xyz, r0.zzzz, r0.xywx, r1.yzwy
  77: add r0.w, -v2.z, l(1.000000)
  78: add_sat r1.xyz, -r0.wwww, r2.xyzx
  79: mul_sat r1.xyz, r1.xyzx, r2.xyzx
  80: mul r1.xyz, r0.xyzx, r1.xyzx
  81: ne r0.w, l(0.000000, 0.000000, 0.000000, 0.000000), cb1[14].y
  82: movc r0.xyz, r0.wwww, r1.xyzx, r0.xyzx
  83: mul r0.xyz, r0.xyzx, cb1[12].xxxx
  84: mul r0.xyz, r0.xyzx, v4.xyzx
  85: max r0.w, v3.x, l(1.000000)
  86: min r0.w, r0.w, l(1000000.000000)
  87: eq r1.x, v2.x, v3.x
  88: movc r0.w, r1.x, l(1.000000), r0.w
  89: mul o0.xyz, r0.xyzx, r0.wwww
  90: mov o0.w, l(0)
  91: mov o1.xyzw, l(0,0,0,1.000000)
  92: mov o2.w, l(0)
  93: mov o3.xyzw, l(0,0,0,1.000000)
  94: ret 
// Approximately 0 instruction slots used


//////////////////////////////////////////////////////
Keywords: _SURFACE_TYPE_TRANSPARENT
-- Vertex shader for "d3d11":
// No shader variant for this keyword set. The closest match will be used instead.

-- Hardware tier variant: Tier 1
-- Fragment shader for "d3d11":
// Stats: 73 math, 4 temp registers
Set 2D Texture "_CameraDepthTexture" to slot 0 sampler slot -1
Set 2D Texture "_MainTex" to slot 1
Set 2D Texture "_Noise" to slot 2
Set 2D Texture "_Flow" to slot 3
Set 2D Texture "_Mask" to slot 4

Set Sampler PointClamp to slot 0

Constant Buffer "$Globals" (2096 bytes) on slot 0 {
  Matrix4x4 unity_MatrixV at 1056
  Matrix4x4 unity_MatrixVP at 1248
  Vector4 _ScaledScreenParams at 48
  Vector2 _GlobalMipBias at 64
  Vector4 _TimeParameters at 304
  Vector3 _WorldSpaceCameraPos at 336
  Vector4 _ProjectionParams at 352
  Vector4 _ZBufferParams at 384
  Vector4 unity_OrthoParams at 400
  Vector4 _RTHandleScale at 448
  Vector4 _CameraDepthTexture_TexelSize at 2080
}
Constant Buffer "UnityPerMaterial" (304 bytes) on slot 1 {
  Float _Fresnelpower at 4
  Float _Fresnelscale at 8
  Float _Useonlycolor at 16
  Float _Use_fresnel at 20
  Vector4 _MainTex_ST at 48
  Vector4 _Noise_ST at 80
  Vector4 _Flow_ST at 112
  Vector4 _Mask_ST at 144
  Vector4 _SpeedMainTexUVNoiseZW at 160
  Vector4 _DistortionSpeedXYPowerZ at 176
  Float _Emission at 192
  Float _Usecenterglow at 228
  Float _Depthpower at 236
}

Shader Disassembly:
//
// Generated by Microsoft (R) D3D Shader Disassembler
//
//
// Input signature:
//
// Name                 Index   Mask Register SysValue  Format   Used
// -------------------- ----- ------ -------- -------- ------- ------
// SV_POSITION              0   xyzw        0      POS   float   xy  
// INTERP                   0   xyz         1     NONE   float       
// INTERP                   2   xyzw        2     NONE   float   xyzw
// INTERP                   3   xyzw        3     NONE   float   x   
// INTERP                   4   xyzw        4     NONE   float   xyz 
// INTERP                   5   xyz         5     NONE   float   xyz 
// INTERP                   6   xyz         6     NONE   float   xyz 
// SV_IsFrontFace           0   x           7    FFACE    uint   x   
//
//
// Output signature:
//
// Name                 Index   Mask Register SysValue  Format   Used
// -------------------- ----- ------ -------- -------- ------- ------
// SV_Target                0   xyzw        0   TARGET   float   xyzw
// SV_Target                1   xyzw        1   TARGET   float   xyzw
// SV_Target                2   xyzw        2   TARGET   float   xyzw
// SV_Target                3   xyzw        3   TARGET   float   xyzw
//
      ps_5_0
      dcl_globalFlags refactoringAllowed
      dcl_constantbuffer CB0[131], immediateIndexed
      dcl_constantbuffer CB1[15], immediateIndexed
      dcl_sampler s0, mode_default
      dcl_sampler s1, mode_default
      dcl_sampler s2, mode_default
      dcl_sampler s3, mode_default
      dcl_sampler s4, mode_default
      dcl_resource_texture2d (float,float,float,float) t0
      dcl_resource_texture2d (float,float,float,float) t1
      dcl_resource_texture2d (float,float,float,float) t2
      dcl_resource_texture2d (float,float,float,float) t3
      dcl_resource_texture2d (float,float,float,float) t4
      dcl_input_ps_siv linear noperspective v0.xy, position
      dcl_input_ps linear v2.xyzw
      dcl_input_ps linear v3.x
      dcl_input_ps linear v4.xyz
      dcl_input_ps linear v5.xyz
      dcl_input_ps linear v6.xyz
      dcl_input_ps_sgv constant v7.x, is_front_face
      dcl_output o0.xyzw
      dcl_output o1.xyzw
      dcl_output o2.xyzw
      dcl_output o3.xyzw
      dcl_temps 4
   0: lt r0.x, cb0[22].x, l(0.000000)
   1: add r0.y, -v0.y, cb0[3].y
   2: movc r0.y, r0.x, r0.y, v0.y
   3: mov r0.x, v0.x
   4: div r0.xy, r0.xyxx, cb0[3].xyxx
   5: add r0.z, -r0.y, l(1.000000)
   6: mad r0.yw, -cb0[130].xxxy, l(0.000000, 0.500000, 0.000000, 0.500000), l(0.000000, 1.000000, 0.000000, 1.000000)
   7: min r0.xy, r0.ywyy, r0.xzxx
   8: mul r0.xy, r0.xyxx, cb0[28].xyxx
   9: sample_b_indexable(texture2d)(float,float,float,float) r0.x, r0.xyxx, t0.xyzw, s0, cb0[4].x
  10: mad r0.x, cb0[24].x, r0.x, cb0[24].y
  11: div r0.x, l(1.000000, 1.000000, 1.000000, 1.000000), r0.x
  12: mul r0.y, v5.y, cb0[79].w
  13: mad r0.y, cb0[78].w, v5.x, r0.y
  14: mad r0.y, cb0[80].w, v5.z, r0.y
  15: add r0.y, r0.y, cb0[81].w
  16: mad r0.x, r0.x, cb0[22].z, -r0.y
  17: div_sat r0.x, r0.x, cb1[14].w
  18: ge r0.y, l(0.000000), r0.x
  19: and r0.y, r0.y, l(0x3f800000)
  20: add r0.z, -r0.x, l(1.000000)
  21: mad r0.x, r0.y, r0.z, r0.x
  22: dp3 r0.y, v6.xyzx, v6.xyzx
  23: sqrt r0.z, r0.y
  24: rsq r0.y, r0.y
  25: mul o2.xyz, r0.yyyy, v6.xyzx
  26: div r0.y, l(1.000000, 1.000000, 1.000000, 1.000000), r0.z
  27: mul r0.yzw, r0.yyyy, v6.xxyz
  28: dp3 r1.x, r0.yzwy, r0.yzwy
  29: rsq r1.x, r1.x
  30: mul r0.yzw, r0.yyzw, r1.xxxx
  31: add r1.xyz, -v5.xyzx, cb0[21].xyzx
  32: dp3 r1.w, r1.xyzx, r1.xyzx
  33: rsq r1.w, r1.w
  34: mul r1.xyz, r1.wwww, r1.xyzx
  35: eq r1.w, cb0[25].w, l(0.000000)
  36: movc r2.x, r1.w, r1.x, cb0[66].z
  37: movc r2.y, r1.w, r1.y, cb0[67].z
  38: movc r2.z, r1.w, r1.z, cb0[68].z
  39: dp3_sat r0.y, r0.yzwy, r2.xyzx
  40: add r0.y, -r0.y, l(1.000000)
  41: log r0.y, r0.y
  42: mul r0.y, r0.y, cb1[0].y
  43: exp r0.y, r0.y
  44: mul_sat r0.y, r0.y, cb1[0].z
  45: and r0.y, r0.y, v7.x
  46: add r0.x, -r0.x, r0.y
  47: add r0.x, r0.x, l(1.000000)
  48: mul r0.y, v2.w, l(1.000100)
  49: ge r0.z, r0.y, -r0.y
  50: frc r0.y, |r0.y|
  51: movc r0.y, r0.z, r0.y, -r0.y
  52: mul r0.y, r0.y, l(0.999900)
  53: div r0.zw, cb1[10].zzzw, cb1[5].xxxy
  54: mad r0.yz, cb0[19].xxxx, r0.zzwz, r0.yyyy
  55: add r0.yz, r0.yyzy, v2.xxyx
  56: mad r0.yz, r0.yyzy, cb1[5].xxyx, cb1[5].zzwz
  57: sample_b_indexable(texture2d)(float,float,float,float) r1.xyzw, r0.yzyy, t2.xyzw, s2, cb0[4].x
  58: div r0.yz, cb1[11].xxyx, cb1[7].xxyx
  59: mad r0.yz, r0.yyzy, cb0[19].xxxx, v2.xxyx
  60: mad r0.yz, r0.yyzy, cb1[7].xxyx, cb1[7].zzwz
  61: sample_b_indexable(texture2d)(float,float,float,float) r0.yz, r0.yzyy, t3.zxyw, s3, cb0[4].x
  62: mad r2.xy, v2.xyxx, cb1[9].xyxx, cb1[9].zwzz
  63: sample_b_indexable(texture2d)(float,float,float,float) r2.xyz, r2.xyxx, t4.xyzw, s4, cb0[4].x
  64: mul r0.yz, r0.yyzy, r2.xxyx
  65: mul r0.yz, r0.yyzy, cb1[11].zzzz
  66: div r0.yz, r0.yyzy, cb1[3].xxyx
  67: div r3.xy, cb1[10].xyxx, cb1[3].xyxx
  68: mad r3.xy, r3.xyxx, cb0[19].xxxx, v2.xyxx
  69: add r0.yz, -r0.yyzy, r3.xxyx
  70: mad r0.yz, r0.yyzy, cb1[3].xxyx, cb1[3].zzwz
  71: sample_b_indexable(texture2d)(float,float,float,float) r3.xyzw, r0.yzyy, t1.xyzw, s1, cb0[4].x
  72: mul r1.xyzw, r1.wxyz, r3.wxyz
  73: ne r0.yz, l(0.000000, 0.000000, 0.000000, 0.000000), cb1[1].xxyx
  74: movc r3.xyz, r0.yyyy, r1.xxxx, r1.yzwy
  75: max r0.xyw, r0.xxxx, r3.xyxz
  76: movc r0.xyz, r0.zzzz, r0.xywx, r1.yzwy
  77: add r0.w, -v2.z, l(1.000000)
  78: add_sat r1.xyz, -r0.wwww, r2.xyzx
  79: mul_sat r1.xyz, r1.xyzx, r2.xyzx
  80: mul r1.xyz, r0.xyzx, r1.xyzx
  81: ne r0.w, l(0.000000, 0.000000, 0.000000, 0.000000), cb1[14].y
  82: movc r0.xyz, r0.wwww, r1.xyzx, r0.xyzx
  83: mul r0.xyz, r0.xyzx, cb1[12].xxxx
  84: mul r0.xyz, r0.xyzx, v4.xyzx
  85: max r0.w, v3.x, l(1.000000)
  86: min r0.w, r0.w, l(1000000.000000)
  87: eq r1.x, v2.x, v3.x
  88: movc r0.w, r1.x, l(1.000000), r0.w
  89: mul o0.xyz, r0.xyzx, r0.wwww
  90: mov o0.w, l(0)
  91: mov o1.xyzw, l(0,0,0,1.000000)
  92: mov o2.w, l(0)
  93: mov o3.xyzw, l(0,0,0,1.000000)
  94: ret 
// Approximately 0 instruction slots used


//////////////////////////////////////////////////////
Keywords: _WRITE_RENDERING_LAYERS
-- Vertex shader for "d3d11":
// No shader variant for this keyword set. The closest match will be used instead.

-- Hardware tier variant: Tier 1
-- Fragment shader for "d3d11":
// Stats: 74 math, 4 temp registers
Set 2D Texture "_CameraDepthTexture" to slot 0 sampler slot -1
Set 2D Texture "_MainTex" to slot 1
Set 2D Texture "_Noise" to slot 2
Set 2D Texture "_Flow" to slot 3
Set 2D Texture "_Mask" to slot 4

Set Sampler PointClamp to slot 0

Constant Buffer "$Globals" (2096 bytes) on slot 0 {
  Matrix4x4 unity_MatrixV at 1056
  Matrix4x4 unity_MatrixVP at 1248
  Vector4 _ScaledScreenParams at 48
  Vector2 _GlobalMipBias at 64
  ScalarInt _RenderingLayerMaxInt at 176
  Vector4 _TimeParameters at 304
  Vector3 _WorldSpaceCameraPos at 336
  Vector4 _ProjectionParams at 352
  Vector4 _ZBufferParams at 384
  Vector4 unity_OrthoParams at 400
  Vector4 _RTHandleScale at 448
  Vector4 _CameraDepthTexture_TexelSize at 2080
}
Constant Buffer "UnityPerDraw" (720 bytes) on slot 1 {
  Vector4 unity_RenderingLayer at 160
}
Constant Buffer "UnityPerMaterial" (304 bytes) on slot 2 {
  Float _Fresnelpower at 4
  Float _Fresnelscale at 8
  Float _Useonlycolor at 16
  Float _Use_fresnel at 20
  Vector4 _MainTex_ST at 48
  Vector4 _Noise_ST at 80
  Vector4 _Flow_ST at 112
  Vector4 _Mask_ST at 144
  Vector4 _SpeedMainTexUVNoiseZW at 160
  Vector4 _DistortionSpeedXYPowerZ at 176
  Float _Emission at 192
  Float _Usecenterglow at 228
  Float _Depthpower at 236
}

Shader Disassembly:
//
// Generated by Microsoft (R) D3D Shader Disassembler
//
//
// Input signature:
//
// Name                 Index   Mask Register SysValue  Format   Used
// -------------------- ----- ------ -------- -------- ------- ------
// SV_POSITION              0   xyzw        0      POS   float   xy  
// INTERP                   0   xyz         1     NONE   float       
// INTERP                   2   xyzw        2     NONE   float   xyzw
// INTERP                   3   xyzw        3     NONE   float   x   
// INTERP                   4   xyzw        4     NONE   float   xyz 
// INTERP                   5   xyz         5     NONE   float   xyz 
// INTERP                   6   xyz         6     NONE   float   xyz 
// SV_IsFrontFace           0   x           7    FFACE    uint   x   
//
//
// Output signature:
//
// Name                 Index   Mask Register SysValue  Format   Used
// -------------------- ----- ------ -------- -------- ------- ------
// SV_Target                0   xyzw        0   TARGET   float   xyzw
// SV_Target                1   xyzw        1   TARGET   float   xyzw
// SV_Target                2   xyzw        2   TARGET   float   xyzw
// SV_Target                3   xyzw        3   TARGET   float   xyzw
// SV_Target                4   x           4   TARGET    uint   x   
//
      ps_5_0
      dcl_globalFlags refactoringAllowed
      dcl_constantbuffer CB0[131], immediateIndexed
      dcl_constantbuffer CB1[11], immediateIndexed
      dcl_constantbuffer CB2[15], immediateIndexed
      dcl_sampler s0, mode_default
      dcl_sampler s1, mode_default
      dcl_sampler s2, mode_default
      dcl_sampler s3, mode_default
      dcl_sampler s4, mode_default
      dcl_resource_texture2d (float,float,float,float) t0
      dcl_resource_texture2d (float,float,float,float) t1
      dcl_resource_texture2d (float,float,float,float) t2
      dcl_resource_texture2d (float,float,float,float) t3
      dcl_resource_texture2d (float,float,float,float) t4
      dcl_input_ps_siv linear noperspective v0.xy, position
      dcl_input_ps linear v2.xyzw
      dcl_input_ps linear v3.x
      dcl_input_ps linear v4.xyz
      dcl_input_ps linear v5.xyz
      dcl_input_ps linear v6.xyz
      dcl_input_ps_sgv constant v7.x, is_front_face
      dcl_output o0.xyzw
      dcl_output o1.xyzw
      dcl_output o2.xyzw
      dcl_output o3.xyzw
      dcl_output o4.x
      dcl_temps 4
   0: lt r0.x, cb0[22].x, l(0.000000)
   1: add r0.y, -v0.y, cb0[3].y
   2: movc r0.y, r0.x, r0.y, v0.y
   3: mov r0.x, v0.x
   4: div r0.xy, r0.xyxx, cb0[3].xyxx
   5: add r0.z, -r0.y, l(1.000000)
   6: mad r0.yw, -cb0[130].xxxy, l(0.000000, 0.500000, 0.000000, 0.500000), l(0.000000, 1.000000, 0.000000, 1.000000)
   7: min r0.xy, r0.ywyy, r0.xzxx
   8: mul r0.xy, r0.xyxx, cb0[28].xyxx
   9: sample_b_indexable(texture2d)(float,float,float,float) r0.x, r0.xyxx, t0.xyzw, s0, cb0[4].x
  10: mad r0.x, cb0[24].x, r0.x, cb0[24].y
  11: div r0.x, l(1.000000, 1.000000, 1.000000, 1.000000), r0.x
  12: mul r0.y, v5.y, cb0[79].w
  13: mad r0.y, cb0[78].w, v5.x, r0.y
  14: mad r0.y, cb0[80].w, v5.z, r0.y
  15: add r0.y, r0.y, cb0[81].w
  16: mad r0.x, r0.x, cb0[22].z, -r0.y
  17: div_sat r0.x, r0.x, cb2[14].w
  18: ge r0.y, l(0.000000), r0.x
  19: and r0.y, r0.y, l(0x3f800000)
  20: add r0.z, -r0.x, l(1.000000)
  21: mad r0.x, r0.y, r0.z, r0.x
  22: dp3 r0.y, v6.xyzx, v6.xyzx
  23: sqrt r0.z, r0.y
  24: rsq r0.y, r0.y
  25: mul o2.xyz, r0.yyyy, v6.xyzx
  26: div r0.y, l(1.000000, 1.000000, 1.000000, 1.000000), r0.z
  27: mul r0.yzw, r0.yyyy, v6.xxyz
  28: dp3 r1.x, r0.yzwy, r0.yzwy
  29: rsq r1.x, r1.x
  30: mul r0.yzw, r0.yyzw, r1.xxxx
  31: add r1.xyz, -v5.xyzx, cb0[21].xyzx
  32: dp3 r1.w, r1.xyzx, r1.xyzx
  33: rsq r1.w, r1.w
  34: mul r1.xyz, r1.wwww, r1.xyzx
  35: eq r1.w, cb0[25].w, l(0.000000)
  36: movc r2.x, r1.w, r1.x, cb0[66].z
  37: movc r2.y, r1.w, r1.y, cb0[67].z
  38: movc r2.z, r1.w, r1.z, cb0[68].z
  39: dp3_sat r0.y, r0.yzwy, r2.xyzx
  40: add r0.y, -r0.y, l(1.000000)
  41: log r0.y, r0.y
  42: mul r0.y, r0.y, cb2[0].y
  43: exp r0.y, r0.y
  44: mul_sat r0.y, r0.y, cb2[0].z
  45: and r0.y, r0.y, v7.x
  46: add r0.x, -r0.x, r0.y
  47: add r0.x, r0.x, l(1.000000)
  48: mul r0.y, v2.w, l(1.000100)
  49: ge r0.z, r0.y, -r0.y
  50: frc r0.y, |r0.y|
  51: movc r0.y, r0.z, r0.y, -r0.y
  52: mul r0.y, r0.y, l(0.999900)
  53: div r0.zw, cb2[10].zzzw, cb2[5].xxxy
  54: mad r0.yz, cb0[19].xxxx, r0.zzwz, r0.yyyy
  55: add r0.yz, r0.yyzy, v2.xxyx
  56: mad r0.yz, r0.yyzy, cb2[5].xxyx, cb2[5].zzwz
  57: sample_b_indexable(texture2d)(float,float,float,float) r1.xyzw, r0.yzyy, t2.xyzw, s2, cb0[4].x
  58: div r0.yz, cb2[11].xxyx, cb2[7].xxyx
  59: mad r0.yz, r0.yyzy, cb0[19].xxxx, v2.xxyx
  60: mad r0.yz, r0.yyzy, cb2[7].xxyx, cb2[7].zzwz
  61: sample_b_indexable(texture2d)(float,float,float,float) r0.yz, r0.yzyy, t3.zxyw, s3, cb0[4].x
  62: mad r2.xy, v2.xyxx, cb2[9].xyxx, cb2[9].zwzz
  63: sample_b_indexable(texture2d)(float,float,float,float) r2.xyz, r2.xyxx, t4.xyzw, s4, cb0[4].x
  64: mul r0.yz, r0.yyzy, r2.xxyx
  65: mul r0.yz, r0.yyzy, cb2[11].zzzz
  66: div r0.yz, r0.yyzy, cb2[3].xxyx
  67: div r3.xy, cb2[10].xyxx, cb2[3].xyxx
  68: mad r3.xy, r3.xyxx, cb0[19].xxxx, v2.xyxx
  69: add r0.yz, -r0.yyzy, r3.xxyx
  70: mad r0.yz, r0.yyzy, cb2[3].xxyx, cb2[3].zzwz
  71: sample_b_indexable(texture2d)(float,float,float,float) r3.xyzw, r0.yzyy, t1.xyzw, s1, cb0[4].x
  72: mul r1.xyzw, r1.wxyz, r3.wxyz
  73: ne r0.yz, l(0.000000, 0.000000, 0.000000, 0.000000), cb2[1].xxyx
  74: movc r3.xyz, r0.yyyy, r1.xxxx, r1.yzwy
  75: max r0.xyw, r0.xxxx, r3.xyxz
  76: movc r0.xyz, r0.zzzz, r0.xywx, r1.yzwy
  77: add r0.w, -v2.z, l(1.000000)
  78: add_sat r1.xyz, -r0.wwww, r2.xyzx
  79: mul_sat r1.xyz, r1.xyzx, r2.xyzx
  80: mul r1.xyz, r0.xyzx, r1.xyzx
  81: ne r0.w, l(0.000000, 0.000000, 0.000000, 0.000000), cb2[14].y
  82: movc r0.xyz, r0.wwww, r1.xyzx, r0.xyzx
  83: mul r0.xyz, r0.xyzx, cb2[12].xxxx
  84: mul r0.xyz, r0.xyzx, v4.xyzx
  85: max r0.w, v3.x, l(1.000000)
  86: min r0.w, r0.w, l(1000000.000000)
  87: eq r1.x, v2.x, v3.x
  88: movc r0.w, r1.x, l(1.000000), r0.w
  89: mul o0.xyz, r0.xyzx, r0.wwww
  90: mov o0.w, l(0)
  91: mov o1.xyzw, l(0,0,0,1.000000)
  92: mov o2.w, l(0)
  93: mov o3.xyzw, l(0,0,0,1.000000)
  94: and o4.x, cb0[11].x, cb1[10].x
  95: ret 
// Approximately 0 instruction slots used


//////////////////////////////////////////////////////
Keywords: _SURFACE_TYPE_TRANSPARENT _WRITE_RENDERING_LAYERS
-- Vertex shader for "d3d11":
// No shader variant for this keyword set. The closest match will be used instead.

-- Hardware tier variant: Tier 1
-- Fragment shader for "d3d11":
// Stats: 74 math, 4 temp registers
Set 2D Texture "_CameraDepthTexture" to slot 0 sampler slot -1
Set 2D Texture "_MainTex" to slot 1
Set 2D Texture "_Noise" to slot 2
Set 2D Texture "_Flow" to slot 3
Set 2D Texture "_Mask" to slot 4

Set Sampler PointClamp to slot 0

Constant Buffer "$Globals" (2096 bytes) on slot 0 {
  Matrix4x4 unity_MatrixV at 1056
  Matrix4x4 unity_MatrixVP at 1248
  Vector4 _ScaledScreenParams at 48
  Vector2 _GlobalMipBias at 64
  ScalarInt _RenderingLayerMaxInt at 176
  Vector4 _TimeParameters at 304
  Vector3 _WorldSpaceCameraPos at 336
  Vector4 _ProjectionParams at 352
  Vector4 _ZBufferParams at 384
  Vector4 unity_OrthoParams at 400
  Vector4 _RTHandleScale at 448
  Vector4 _CameraDepthTexture_TexelSize at 2080
}
Constant Buffer "UnityPerDraw" (720 bytes) on slot 1 {
  Vector4 unity_RenderingLayer at 160
}
Constant Buffer "UnityPerMaterial" (304 bytes) on slot 2 {
  Float _Fresnelpower at 4
  Float _Fresnelscale at 8
  Float _Useonlycolor at 16
  Float _Use_fresnel at 20
  Vector4 _MainTex_ST at 48
  Vector4 _Noise_ST at 80
  Vector4 _Flow_ST at 112
  Vector4 _Mask_ST at 144
  Vector4 _SpeedMainTexUVNoiseZW at 160
  Vector4 _DistortionSpeedXYPowerZ at 176
  Float _Emission at 192
  Float _Usecenterglow at 228
  Float _Depthpower at 236
}

Shader Disassembly:
//
// Generated by Microsoft (R) D3D Shader Disassembler
//
//
// Input signature:
//
// Name                 Index   Mask Register SysValue  Format   Used
// -------------------- ----- ------ -------- -------- ------- ------
// SV_POSITION              0   xyzw        0      POS   float   xy  
// INTERP                   0   xyz         1     NONE   float       
// INTERP                   2   xyzw        2     NONE   float   xyzw
// INTERP                   3   xyzw        3     NONE   float   x   
// INTERP                   4   xyzw        4     NONE   float   xyz 
// INTERP                   5   xyz         5     NONE   float   xyz 
// INTERP                   6   xyz         6     NONE   float   xyz 
// SV_IsFrontFace           0   x           7    FFACE    uint   x   
//
//
// Output signature:
//
// Name                 Index   Mask Register SysValue  Format   Used
// -------------------- ----- ------ -------- -------- ------- ------
// SV_Target                0   xyzw        0   TARGET   float   xyzw
// SV_Target                1   xyzw        1   TARGET   float   xyzw
// SV_Target                2   xyzw        2   TARGET   float   xyzw
// SV_Target                3   xyzw        3   TARGET   float   xyzw
// SV_Target                4   x           4   TARGET    uint   x   
//
      ps_5_0
      dcl_globalFlags refactoringAllowed
      dcl_constantbuffer CB0[131], immediateIndexed
      dcl_constantbuffer CB1[11], immediateIndexed
      dcl_constantbuffer CB2[15], immediateIndexed
      dcl_sampler s0, mode_default
      dcl_sampler s1, mode_default
      dcl_sampler s2, mode_default
      dcl_sampler s3, mode_default
      dcl_sampler s4, mode_default
      dcl_resource_texture2d (float,float,float,float) t0
      dcl_resource_texture2d (float,float,float,float) t1
      dcl_resource_texture2d (float,float,float,float) t2
      dcl_resource_texture2d (float,float,float,float) t3
      dcl_resource_texture2d (float,float,float,float) t4
      dcl_input_ps_siv linear noperspective v0.xy, position
      dcl_input_ps linear v2.xyzw
      dcl_input_ps linear v3.x
      dcl_input_ps linear v4.xyz
      dcl_input_ps linear v5.xyz
      dcl_input_ps linear v6.xyz
      dcl_input_ps_sgv constant v7.x, is_front_face
      dcl_output o0.xyzw
      dcl_output o1.xyzw
      dcl_output o2.xyzw
      dcl_output o3.xyzw
      dcl_output o4.x
      dcl_temps 4
   0: lt r0.x, cb0[22].x, l(0.000000)
   1: add r0.y, -v0.y, cb0[3].y
   2: movc r0.y, r0.x, r0.y, v0.y
   3: mov r0.x, v0.x
   4: div r0.xy, r0.xyxx, cb0[3].xyxx
   5: add r0.z, -r0.y, l(1.000000)
   6: mad r0.yw, -cb0[130].xxxy, l(0.000000, 0.500000, 0.000000, 0.500000), l(0.000000, 1.000000, 0.000000, 1.000000)
   7: min r0.xy, r0.ywyy, r0.xzxx
   8: mul r0.xy, r0.xyxx, cb0[28].xyxx
   9: sample_b_indexable(texture2d)(float,float,float,float) r0.x, r0.xyxx, t0.xyzw, s0, cb0[4].x
  10: mad r0.x, cb0[24].x, r0.x, cb0[24].y
  11: div r0.x, l(1.000000, 1.000000, 1.000000, 1.000000), r0.x
  12: mul r0.y, v5.y, cb0[79].w
  13: mad r0.y, cb0[78].w, v5.x, r0.y
  14: mad r0.y, cb0[80].w, v5.z, r0.y
  15: add r0.y, r0.y, cb0[81].w
  16: mad r0.x, r0.x, cb0[22].z, -r0.y
  17: div_sat r0.x, r0.x, cb2[14].w
  18: ge r0.y, l(0.000000), r0.x
  19: and r0.y, r0.y, l(0x3f800000)
  20: add r0.z, -r0.x, l(1.000000)
  21: mad r0.x, r0.y, r0.z, r0.x
  22: dp3 r0.y, v6.xyzx, v6.xyzx
  23: sqrt r0.z, r0.y
  24: rsq r0.y, r0.y
  25: mul o2.xyz, r0.yyyy, v6.xyzx
  26: div r0.y, l(1.000000, 1.000000, 1.000000, 1.000000), r0.z
  27: mul r0.yzw, r0.yyyy, v6.xxyz
  28: dp3 r1.x, r0.yzwy, r0.yzwy
  29: rsq r1.x, r1.x
  30: mul r0.yzw, r0.yyzw, r1.xxxx
  31: add r1.xyz, -v5.xyzx, cb0[21].xyzx
  32: dp3 r1.w, r1.xyzx, r1.xyzx
  33: rsq r1.w, r1.w
  34: mul r1.xyz, r1.wwww, r1.xyzx
  35: eq r1.w, cb0[25].w, l(0.000000)
  36: movc r2.x, r1.w, r1.x, cb0[66].z
  37: movc r2.y, r1.w, r1.y, cb0[67].z
  38: movc r2.z, r1.w, r1.z, cb0[68].z
  39: dp3_sat r0.y, r0.yzwy, r2.xyzx
  40: add r0.y, -r0.y, l(1.000000)
  41: log r0.y, r0.y
  42: mul r0.y, r0.y, cb2[0].y
  43: exp r0.y, r0.y
  44: mul_sat r0.y, r0.y, cb2[0].z
  45: and r0.y, r0.y, v7.x
  46: add r0.x, -r0.x, r0.y
  47: add r0.x, r0.x, l(1.000000)
  48: mul r0.y, v2.w, l(1.000100)
  49: ge r0.z, r0.y, -r0.y
  50: frc r0.y, |r0.y|
  51: movc r0.y, r0.z, r0.y, -r0.y
  52: mul r0.y, r0.y, l(0.999900)
  53: div r0.zw, cb2[10].zzzw, cb2[5].xxxy
  54: mad r0.yz, cb0[19].xxxx, r0.zzwz, r0.yyyy
  55: add r0.yz, r0.yyzy, v2.xxyx
  56: mad r0.yz, r0.yyzy, cb2[5].xxyx, cb2[5].zzwz
  57: sample_b_indexable(texture2d)(float,float,float,float) r1.xyzw, r0.yzyy, t2.xyzw, s2, cb0[4].x
  58: div r0.yz, cb2[11].xxyx, cb2[7].xxyx
  59: mad r0.yz, r0.yyzy, cb0[19].xxxx, v2.xxyx
  60: mad r0.yz, r0.yyzy, cb2[7].xxyx, cb2[7].zzwz
  61: sample_b_indexable(texture2d)(float,float,float,float) r0.yz, r0.yzyy, t3.zxyw, s3, cb0[4].x
  62: mad r2.xy, v2.xyxx, cb2[9].xyxx, cb2[9].zwzz
  63: sample_b_indexable(texture2d)(float,float,float,float) r2.xyz, r2.xyxx, t4.xyzw, s4, cb0[4].x
  64: mul r0.yz, r0.yyzy, r2.xxyx
  65: mul r0.yz, r0.yyzy, cb2[11].zzzz
  66: div r0.yz, r0.yyzy, cb2[3].xxyx
  67: div r3.xy, cb2[10].xyxx, cb2[3].xyxx
  68: mad r3.xy, r3.xyxx, cb0[19].xxxx, v2.xyxx
  69: add r0.yz, -r0.yyzy, r3.xxyx
  70: mad r0.yz, r0.yyzy, cb2[3].xxyx, cb2[3].zzwz
  71: sample_b_indexable(texture2d)(float,float,float,float) r3.xyzw, r0.yzyy, t1.xyzw, s1, cb0[4].x
  72: mul r1.xyzw, r1.wxyz, r3.wxyz
  73: ne r0.yz, l(0.000000, 0.000000, 0.000000, 0.000000), cb2[1].xxyx
  74: movc r3.xyz, r0.yyyy, r1.xxxx, r1.yzwy
  75: max r0.xyw, r0.xxxx, r3.xyxz
  76: movc r0.xyz, r0.zzzz, r0.xywx, r1.yzwy
  77: add r0.w, -v2.z, l(1.000000)
  78: add_sat r1.xyz, -r0.wwww, r2.xyzx
  79: mul_sat r1.xyz, r1.xyzx, r2.xyzx
  80: mul r1.xyz, r0.xyzx, r1.xyzx
  81: ne r0.w, l(0.000000, 0.000000, 0.000000, 0.000000), cb2[14].y
  82: movc r0.xyz, r0.wwww, r1.xyzx, r0.xyzx
  83: mul r0.xyz, r0.xyzx, cb2[12].xxxx
  84: mul r0.xyz, r0.xyzx, v4.xyzx
  85: max r0.w, v3.x, l(1.000000)
  86: min r0.w, r0.w, l(1000000.000000)
  87: eq r1.x, v2.x, v3.x
  88: movc r0.w, r1.x, l(1.000000), r0.w
  89: mul o0.xyz, r0.xyzx, r0.wwww
  90: mov o0.w, l(0)
  91: mov o1.xyzw, l(0,0,0,1.000000)
  92: mov o2.w, l(0)
  93: mov o3.xyzw, l(0,0,0,1.000000)
  94: and o4.x, cb0[11].x, cb1[10].x
  95: ret 
// Approximately 0 instruction slots used


//////////////////////////////////////////////////////
Keywords: _SCREEN_SPACE_OCCLUSION
-- Vertex shader for "d3d11":
// No shader variant for this keyword set. The closest match will be used instead.

-- Hardware tier variant: Tier 1
-- Fragment shader for "d3d11":
// Stats: 81 math, 4 temp registers
Set 2D Texture "_ScreenSpaceOcclusionTexture" to slot 0 sampler slot -1
Set 2D Texture "_CameraDepthTexture" to slot 1 sampler slot -1
Set 2D Texture "_MainTex" to slot 2
Set 2D Texture "_Noise" to slot 3
Set 2D Texture "_Flow" to slot 4
Set 2D Texture "_Mask" to slot 5

Set Sampler PointClamp to slot 0
Set Sampler LinearClamp to slot 1

Constant Buffer "$Globals" (2096 bytes) on slot 0 {
  Matrix4x4 unity_MatrixV at 1056
  Matrix4x4 unity_MatrixVP at 1248
  Vector4 _ScaledScreenParams at 48
  Vector2 _GlobalMipBias at 64
  Vector4 _AmbientOcclusionParam at 144
  Vector4 _TimeParameters at 304
  Vector3 _WorldSpaceCameraPos at 336
  Vector4 _ProjectionParams at 352
  Vector4 _ZBufferParams at 384
  Vector4 unity_OrthoParams at 400
  Vector4 _ScaleBiasRt at 432
  Vector4 _RTHandleScale at 448
  Vector4 _CameraDepthTexture_TexelSize at 2080
}
Constant Buffer "UnityPerMaterial" (304 bytes) on slot 1 {
  Float _Fresnelpower at 4
  Float _Fresnelscale at 8
  Float _Useonlycolor at 16
  Float _Use_fresnel at 20
  Vector4 _MainTex_ST at 48
  Vector4 _Noise_ST at 80
  Vector4 _Flow_ST at 112
  Vector4 _Mask_ST at 144
  Vector4 _SpeedMainTexUVNoiseZW at 160
  Vector4 _DistortionSpeedXYPowerZ at 176
  Float _Emission at 192
  Float _Usecenterglow at 228
  Float _Depthpower at 236
}

Shader Disassembly:
//
// Generated by Microsoft (R) D3D Shader Disassembler
//
//
// Input signature:
//
// Name                 Index   Mask Register SysValue  Format   Used
// -------------------- ----- ------ -------- -------- ------- ------
// SV_POSITION              0   xyzw        0      POS   float   xy  
// INTERP                   0   xyz         1     NONE   float       
// INTERP                   2   xyzw        2     NONE   float   xyzw
// INTERP                   3   xyzw        3     NONE   float   x   
// INTERP                   4   xyzw        4     NONE   float   xyz 
// INTERP                   5   xyz         5     NONE   float   xyz 
// INTERP                   6   xyz         6     NONE   float   xyz 
// SV_IsFrontFace           0   x           7    FFACE    uint   x   
//
//
// Output signature:
//
// Name                 Index   Mask Register SysValue  Format   Used
// -------------------- ----- ------ -------- -------- ------- ------
// SV_Target                0   xyzw        0   TARGET   float   xyzw
// SV_Target                1   xyzw        1   TARGET   float   xyzw
// SV_Target                2   xyzw        2   TARGET   float   xyzw
// SV_Target                3   xyzw        3   TARGET   float   xyzw
//
      ps_5_0
      dcl_globalFlags refactoringAllowed
      dcl_constantbuffer CB0[131], immediateIndexed
      dcl_constantbuffer CB1[15], immediateIndexed
      dcl_sampler s0, mode_default
      dcl_sampler s1, mode_default
      dcl_sampler s2, mode_default
      dcl_sampler s3, mode_default
      dcl_sampler s4, mode_default
      dcl_sampler s5, mode_default
      dcl_resource_texture2d (float,float,float,float) t0
      dcl_resource_texture2d (float,float,float,float) t1
      dcl_resource_texture2d (float,float,float,float) t2
      dcl_resource_texture2d (float,float,float,float) t3
      dcl_resource_texture2d (float,float,float,float) t4
      dcl_resource_texture2d (float,float,float,float) t5
      dcl_input_ps_siv linear noperspective v0.xy, position
      dcl_input_ps linear v2.xyzw
      dcl_input_ps linear v3.x
      dcl_input_ps linear v4.xyz
      dcl_input_ps linear v5.xyz
      dcl_input_ps linear v6.xyz
      dcl_input_ps_sgv constant v7.x, is_front_face
      dcl_output o0.xyzw
      dcl_output o1.xyzw
      dcl_output o2.xyzw
      dcl_output o3.xyzw
      dcl_temps 4
   0: lt r0.x, cb0[22].x, l(0.000000)
   1: add r0.y, -v0.y, cb0[3].y
   2: movc r0.y, r0.x, r0.y, v0.y
   3: mov r0.x, v0.x
   4: div r0.xy, r0.xyxx, cb0[3].xyxx
   5: add r0.z, -r0.y, l(1.000000)
   6: mad r0.yw, -cb0[130].xxxy, l(0.000000, 0.500000, 0.000000, 0.500000), l(0.000000, 1.000000, 0.000000, 1.000000)
   7: min r0.xy, r0.ywyy, r0.xzxx
   8: mul r0.xy, r0.xyxx, cb0[28].xyxx
   9: sample_b_indexable(texture2d)(float,float,float,float) r0.x, r0.xyxx, t1.xyzw, s0, cb0[4].x
  10: mad r0.x, cb0[24].x, r0.x, cb0[24].y
  11: div r0.x, l(1.000000, 1.000000, 1.000000, 1.000000), r0.x
  12: mul r0.y, v5.y, cb0[79].w
  13: mad r0.y, cb0[78].w, v5.x, r0.y
  14: mad r0.y, cb0[80].w, v5.z, r0.y
  15: add r0.y, r0.y, cb0[81].w
  16: mad r0.x, r0.x, cb0[22].z, -r0.y
  17: div_sat r0.x, r0.x, cb1[14].w
  18: ge r0.y, l(0.000000), r0.x
  19: and r0.y, r0.y, l(0x3f800000)
  20: add r0.z, -r0.x, l(1.000000)
  21: mad r0.x, r0.y, r0.z, r0.x
  22: dp3 r0.y, v6.xyzx, v6.xyzx
  23: sqrt r0.z, r0.y
  24: rsq r0.y, r0.y
  25: mul o2.xyz, r0.yyyy, v6.xyzx
  26: div r0.y, l(1.000000, 1.000000, 1.000000, 1.000000), r0.z
  27: mul r0.yzw, r0.yyyy, v6.xxyz
  28: dp3 r1.x, r0.yzwy, r0.yzwy
  29: rsq r1.x, r1.x
  30: mul r0.yzw, r0.yyzw, r1.xxxx
  31: add r1.xyz, -v5.xyzx, cb0[21].xyzx
  32: dp3 r1.w, r1.xyzx, r1.xyzx
  33: rsq r1.w, r1.w
  34: mul r1.xyz, r1.wwww, r1.xyzx
  35: eq r1.w, cb0[25].w, l(0.000000)
  36: movc r2.x, r1.w, r1.x, cb0[66].z
  37: movc r2.y, r1.w, r1.y, cb0[67].z
  38: movc r2.z, r1.w, r1.z, cb0[68].z
  39: dp3_sat r0.y, r0.yzwy, r2.xyzx
  40: add r0.y, -r0.y, l(1.000000)
  41: log r0.y, r0.y
  42: mul r0.y, r0.y, cb1[0].y
  43: exp r0.y, r0.y
  44: mul_sat r0.y, r0.y, cb1[0].z
  45: and r0.y, r0.y, v7.x
  46: add r0.x, -r0.x, r0.y
  47: add r0.x, r0.x, l(1.000000)
  48: mul r0.y, v2.w, l(1.000100)
  49: ge r0.z, r0.y, -r0.y
  50: frc r0.y, |r0.y|
  51: movc r0.y, r0.z, r0.y, -r0.y
  52: mul r0.y, r0.y, l(0.999900)
  53: div r0.zw, cb1[10].zzzw, cb1[5].xxxy
  54: mad r0.yz, cb0[19].xxxx, r0.zzwz, r0.yyyy
  55: add r0.yz, r0.yyzy, v2.xxyx
  56: mad r0.yz, r0.yyzy, cb1[5].xxyx, cb1[5].zzwz
  57: sample_b_indexable(texture2d)(float,float,float,float) r1.xyzw, r0.yzyy, t3.xyzw, s3, cb0[4].x
  58: div r0.yz, cb1[11].xxyx, cb1[7].xxyx
  59: mad r0.yz, r0.yyzy, cb0[19].xxxx, v2.xxyx
  60: mad r0.yz, r0.yyzy, cb1[7].xxyx, cb1[7].zzwz
  61: sample_b_indexable(texture2d)(float,float,float,float) r0.yz, r0.yzyy, t4.zxyw, s4, cb0[4].x
  62: mad r2.xy, v2.xyxx, cb1[9].xyxx, cb1[9].zwzz
  63: sample_b_indexable(texture2d)(float,float,float,float) r2.xyz, r2.xyxx, t5.xyzw, s5, cb0[4].x
  64: mul r0.yz, r0.yyzy, r2.xxyx
  65: mul r0.yz, r0.yyzy, cb1[11].zzzz
  66: div r0.yz, r0.yyzy, cb1[3].xxyx
  67: div r3.xy, cb1[10].xyxx, cb1[3].xyxx
  68: mad r3.xy, r3.xyxx, cb0[19].xxxx, v2.xyxx
  69: add r0.yz, -r0.yyzy, r3.xxyx
  70: mad r0.yz, r0.yyzy, cb1[3].xxyx, cb1[3].zzwz
  71: sample_b_indexable(texture2d)(float,float,float,float) r3.xyzw, r0.yzyy, t2.xyzw, s2, cb0[4].x
  72: mul r1.xyzw, r1.wxyz, r3.wxyz
  73: ne r0.yz, l(0.000000, 0.000000, 0.000000, 0.000000), cb1[1].xxyx
  74: movc r3.xyz, r0.yyyy, r1.xxxx, r1.yzwy
  75: max r0.xyw, r0.xxxx, r3.xyxz
  76: movc r0.xyz, r0.zzzz, r0.xywx, r1.yzwy
  77: add r0.w, -v2.z, l(1.000000)
  78: add_sat r1.xyz, -r0.wwww, r2.xyzx
  79: mul_sat r1.xyz, r1.xyzx, r2.xyzx
  80: mul r1.xyz, r0.xyzx, r1.xyzx
  81: ne r0.w, l(0.000000, 0.000000, 0.000000, 0.000000), cb1[14].y
  82: movc r0.xyz, r0.wwww, r1.xyzx, r0.xyzx
  83: mul r0.xyz, r0.xyzx, cb1[12].xxxx
  84: mul r0.xyz, r0.xyzx, v4.xyzx
  85: max r0.w, v3.x, l(1.000000)
  86: min r0.w, r0.w, l(1000000.000000)
  87: eq r1.x, v2.x, v3.x
  88: movc r0.w, r1.x, l(1.000000), r0.w
  89: mul r0.xyz, r0.xyzx, r0.wwww
  90: rcp r1.xy, cb0[3].xyxx
  91: mul r1.xy, r1.xyxx, v0.xyxx
  92: mad r0.w, r1.y, cb0[27].x, cb0[27].y
  93: add r1.z, -r0.w, l(1.000000)
  94: sample_b_indexable(texture2d)(float,float,float,float) r0.w, r1.xzxx, t0.yzwx, s1, cb0[4].x
  95: add r1.x, -cb0[9].x, l(1.000000)
  96: add_sat r0.w, r0.w, r1.x
  97: add r0.w, r0.w, l(-1.000000)
  98: mad r0.w, cb0[9].w, r0.w, l(1.000000)
  99: mul o0.xyz, r0.wwww, r0.xyzx
 100: mov o0.w, l(0)
 101: mov o1.xyzw, l(0,0,0,0)
 102: mov o2.w, l(0)
 103: mov o3.xyzw, l(0,0,0,1.000000)
 104: ret 
// Approximately 0 instruction slots used


//////////////////////////////////////////////////////
Keywords: _SCREEN_SPACE_OCCLUSION _SURFACE_TYPE_TRANSPARENT
-- Vertex shader for "d3d11":
// No shader variant for this keyword set. The closest match will be used instead.

-- Hardware tier variant: Tier 1
-- Fragment shader for "d3d11":
// Stats: 73 math, 4 temp registers
Set 2D Texture "_CameraDepthTexture" to slot 0 sampler slot -1
Set 2D Texture "_MainTex" to slot 1
Set 2D Texture "_Noise" to slot 2
Set 2D Texture "_Flow" to slot 3
Set 2D Texture "_Mask" to slot 4

Set Sampler PointClamp to slot 0

Constant Buffer "$Globals" (2096 bytes) on slot 0 {
  Matrix4x4 unity_MatrixV at 1056
  Matrix4x4 unity_MatrixVP at 1248
  Vector4 _ScaledScreenParams at 48
  Vector2 _GlobalMipBias at 64
  Vector4 _TimeParameters at 304
  Vector3 _WorldSpaceCameraPos at 336
  Vector4 _ProjectionParams at 352
  Vector4 _ZBufferParams at 384
  Vector4 unity_OrthoParams at 400
  Vector4 _RTHandleScale at 448
  Vector4 _CameraDepthTexture_TexelSize at 2080
}
Constant Buffer "UnityPerMaterial" (304 bytes) on slot 1 {
  Float _Fresnelpower at 4
  Float _Fresnelscale at 8
  Float _Useonlycolor at 16
  Float _Use_fresnel at 20
  Vector4 _MainTex_ST at 48
  Vector4 _Noise_ST at 80
  Vector4 _Flow_ST at 112
  Vector4 _Mask_ST at 144
  Vector4 _SpeedMainTexUVNoiseZW at 160
  Vector4 _DistortionSpeedXYPowerZ at 176
  Float _Emission at 192
  Float _Usecenterglow at 228
  Float _Depthpower at 236
}

Shader Disassembly:
//
// Generated by Microsoft (R) D3D Shader Disassembler
//
//
// Input signature:
//
// Name                 Index   Mask Register SysValue  Format   Used
// -------------------- ----- ------ -------- -------- ------- ------
// SV_POSITION              0   xyzw        0      POS   float   xy  
// INTERP                   0   xyz         1     NONE   float       
// INTERP                   2   xyzw        2     NONE   float   xyzw
// INTERP                   3   xyzw        3     NONE   float   x   
// INTERP                   4   xyzw        4     NONE   float   xyz 
// INTERP                   5   xyz         5     NONE   float   xyz 
// INTERP                   6   xyz         6     NONE   float   xyz 
// SV_IsFrontFace           0   x           7    FFACE    uint   x   
//
//
// Output signature:
//
// Name                 Index   Mask Register SysValue  Format   Used
// -------------------- ----- ------ -------- -------- ------- ------
// SV_Target                0   xyzw        0   TARGET   float   xyzw
// SV_Target                1   xyzw        1   TARGET   float   xyzw
// SV_Target                2   xyzw        2   TARGET   float   xyzw
// SV_Target                3   xyzw        3   TARGET   float   xyzw
//
      ps_5_0
      dcl_globalFlags refactoringAllowed
      dcl_constantbuffer CB0[131], immediateIndexed
      dcl_constantbuffer CB1[15], immediateIndexed
      dcl_sampler s0, mode_default
      dcl_sampler s1, mode_default
      dcl_sampler s2, mode_default
      dcl_sampler s3, mode_default
      dcl_sampler s4, mode_default
      dcl_resource_texture2d (float,float,float,float) t0
      dcl_resource_texture2d (float,float,float,float) t1
      dcl_resource_texture2d (float,float,float,float) t2
      dcl_resource_texture2d (float,float,float,float) t3
      dcl_resource_texture2d (float,float,float,float) t4
      dcl_input_ps_siv linear noperspective v0.xy, position
      dcl_input_ps linear v2.xyzw
      dcl_input_ps linear v3.x
      dcl_input_ps linear v4.xyz
      dcl_input_ps linear v5.xyz
      dcl_input_ps linear v6.xyz
      dcl_input_ps_sgv constant v7.x, is_front_face
      dcl_output o0.xyzw
      dcl_output o1.xyzw
      dcl_output o2.xyzw
      dcl_output o3.xyzw
      dcl_temps 4
   0: lt r0.x, cb0[22].x, l(0.000000)
   1: add r0.y, -v0.y, cb0[3].y
   2: movc r0.y, r0.x, r0.y, v0.y
   3: mov r0.x, v0.x
   4: div r0.xy, r0.xyxx, cb0[3].xyxx
   5: add r0.z, -r0.y, l(1.000000)
   6: mad r0.yw, -cb0[130].xxxy, l(0.000000, 0.500000, 0.000000, 0.500000), l(0.000000, 1.000000, 0.000000, 1.000000)
   7: min r0.xy, r0.ywyy, r0.xzxx
   8: mul r0.xy, r0.xyxx, cb0[28].xyxx
   9: sample_b_indexable(texture2d)(float,float,float,float) r0.x, r0.xyxx, t0.xyzw, s0, cb0[4].x
  10: mad r0.x, cb0[24].x, r0.x, cb0[24].y
  11: div r0.x, l(1.000000, 1.000000, 1.000000, 1.000000), r0.x
  12: mul r0.y, v5.y, cb0[79].w
  13: mad r0.y, cb0[78].w, v5.x, r0.y
  14: mad r0.y, cb0[80].w, v5.z, r0.y
  15: add r0.y, r0.y, cb0[81].w
  16: mad r0.x, r0.x, cb0[22].z, -r0.y
  17: div_sat r0.x, r0.x, cb1[14].w
  18: ge r0.y, l(0.000000), r0.x
  19: and r0.y, r0.y, l(0x3f800000)
  20: add r0.z, -r0.x, l(1.000000)
  21: mad r0.x, r0.y, r0.z, r0.x
  22: dp3 r0.y, v6.xyzx, v6.xyzx
  23: sqrt r0.z, r0.y
  24: rsq r0.y, r0.y
  25: mul o2.xyz, r0.yyyy, v6.xyzx
  26: div r0.y, l(1.000000, 1.000000, 1.000000, 1.000000), r0.z
  27: mul r0.yzw, r0.yyyy, v6.xxyz
  28: dp3 r1.x, r0.yzwy, r0.yzwy
  29: rsq r1.x, r1.x
  30: mul r0.yzw, r0.yyzw, r1.xxxx
  31: add r1.xyz, -v5.xyzx, cb0[21].xyzx
  32: dp3 r1.w, r1.xyzx, r1.xyzx
  33: rsq r1.w, r1.w
  34: mul r1.xyz, r1.wwww, r1.xyzx
  35: eq r1.w, cb0[25].w, l(0.000000)
  36: movc r2.x, r1.w, r1.x, cb0[66].z
  37: movc r2.y, r1.w, r1.y, cb0[67].z
  38: movc r2.z, r1.w, r1.z, cb0[68].z
  39: dp3_sat r0.y, r0.yzwy, r2.xyzx
  40: add r0.y, -r0.y, l(1.000000)
  41: log r0.y, r0.y
  42: mul r0.y, r0.y, cb1[0].y
  43: exp r0.y, r0.y
  44: mul_sat r0.y, r0.y, cb1[0].z
  45: and r0.y, r0.y, v7.x
  46: add r0.x, -r0.x, r0.y
  47: add r0.x, r0.x, l(1.000000)
  48: mul r0.y, v2.w, l(1.000100)
  49: ge r0.z, r0.y, -r0.y
  50: frc r0.y, |r0.y|
  51: movc r0.y, r0.z, r0.y, -r0.y
  52: mul r0.y, r0.y, l(0.999900)
  53: div r0.zw, cb1[10].zzzw, cb1[5].xxxy
  54: mad r0.yz, cb0[19].xxxx, r0.zzwz, r0.yyyy
  55: add r0.yz, r0.yyzy, v2.xxyx
  56: mad r0.yz, r0.yyzy, cb1[5].xxyx, cb1[5].zzwz
  57: sample_b_indexable(texture2d)(float,float,float,float) r1.xyzw, r0.yzyy, t2.xyzw, s2, cb0[4].x
  58: div r0.yz, cb1[11].xxyx, cb1[7].xxyx
  59: mad r0.yz, r0.yyzy, cb0[19].xxxx, v2.xxyx
  60: mad r0.yz, r0.yyzy, cb1[7].xxyx, cb1[7].zzwz
  61: sample_b_indexable(texture2d)(float,float,float,float) r0.yz, r0.yzyy, t3.zxyw, s3, cb0[4].x
  62: mad r2.xy, v2.xyxx, cb1[9].xyxx, cb1[9].zwzz
  63: sample_b_indexable(texture2d)(float,float,float,float) r2.xyz, r2.xyxx, t4.xyzw, s4, cb0[4].x
  64: mul r0.yz, r0.yyzy, r2.xxyx
  65: mul r0.yz, r0.yyzy, cb1[11].zzzz
  66: div r0.yz, r0.yyzy, cb1[3].xxyx
  67: div r3.xy, cb1[10].xyxx, cb1[3].xyxx
  68: mad r3.xy, r3.xyxx, cb0[19].xxxx, v2.xyxx
  69: add r0.yz, -r0.yyzy, r3.xxyx
  70: mad r0.yz, r0.yyzy, cb1[3].xxyx, cb1[3].zzwz
  71: sample_b_indexable(texture2d)(float,float,float,float) r3.xyzw, r0.yzyy, t1.xyzw, s1, cb0[4].x
  72: mul r1.xyzw, r1.wxyz, r3.wxyz
  73: ne r0.yz, l(0.000000, 0.000000, 0.000000, 0.000000), cb1[1].xxyx
  74: movc r3.xyz, r0.yyyy, r1.xxxx, r1.yzwy
  75: max r0.xyw, r0.xxxx, r3.xyxz
  76: movc r0.xyz, r0.zzzz, r0.xywx, r1.yzwy
  77: add r0.w, -v2.z, l(1.000000)
  78: add_sat r1.xyz, -r0.wwww, r2.xyzx
  79: mul_sat r1.xyz, r1.xyzx, r2.xyzx
  80: mul r1.xyz, r0.xyzx, r1.xyzx
  81: ne r0.w, l(0.000000, 0.000000, 0.000000, 0.000000), cb1[14].y
  82: movc r0.xyz, r0.wwww, r1.xyzx, r0.xyzx
  83: mul r0.xyz, r0.xyzx, cb1[12].xxxx
  84: mul r0.xyz, r0.xyzx, v4.xyzx
  85: max r0.w, v3.x, l(1.000000)
  86: min r0.w, r0.w, l(1000000.000000)
  87: eq r1.x, v2.x, v3.x
  88: movc r0.w, r1.x, l(1.000000), r0.w
  89: mul o0.xyz, r0.xyzx, r0.wwww
  90: mov o0.w, l(0)
  91: mov o1.xyzw, l(0,0,0,1.000000)
  92: mov o2.w, l(0)
  93: mov o3.xyzw, l(0,0,0,1.000000)
  94: ret 
// Approximately 0 instruction slots used


//////////////////////////////////////////////////////
Keywords: _SCREEN_SPACE_OCCLUSION _WRITE_RENDERING_LAYERS
-- Vertex shader for "d3d11":
// No shader variant for this keyword set. The closest match will be used instead.

-- Hardware tier variant: Tier 1
-- Fragment shader for "d3d11":
// Stats: 82 math, 4 temp registers
Set 2D Texture "_ScreenSpaceOcclusionTexture" to slot 0 sampler slot -1
Set 2D Texture "_CameraDepthTexture" to slot 1 sampler slot -1
Set 2D Texture "_MainTex" to slot 2
Set 2D Texture "_Noise" to slot 3
Set 2D Texture "_Flow" to slot 4
Set 2D Texture "_Mask" to slot 5

Set Sampler PointClamp to slot 0
Set Sampler LinearClamp to slot 1

Constant Buffer "$Globals" (2096 bytes) on slot 0 {
  Matrix4x4 unity_MatrixV at 1056
  Matrix4x4 unity_MatrixVP at 1248
  Vector4 _ScaledScreenParams at 48
  Vector2 _GlobalMipBias at 64
  Vector4 _AmbientOcclusionParam at 144
  ScalarInt _RenderingLayerMaxInt at 176
  Vector4 _TimeParameters at 304
  Vector3 _WorldSpaceCameraPos at 336
  Vector4 _ProjectionParams at 352
  Vector4 _ZBufferParams at 384
  Vector4 unity_OrthoParams at 400
  Vector4 _ScaleBiasRt at 432
  Vector4 _RTHandleScale at 448
  Vector4 _CameraDepthTexture_TexelSize at 2080
}
Constant Buffer "UnityPerDraw" (720 bytes) on slot 1 {
  Vector4 unity_RenderingLayer at 160
}
Constant Buffer "UnityPerMaterial" (304 bytes) on slot 2 {
  Float _Fresnelpower at 4
  Float _Fresnelscale at 8
  Float _Useonlycolor at 16
  Float _Use_fresnel at 20
  Vector4 _MainTex_ST at 48
  Vector4 _Noise_ST at 80
  Vector4 _Flow_ST at 112
  Vector4 _Mask_ST at 144
  Vector4 _SpeedMainTexUVNoiseZW at 160
  Vector4 _DistortionSpeedXYPowerZ at 176
  Float _Emission at 192
  Float _Usecenterglow at 228
  Float _Depthpower at 236
}

Shader Disassembly:
//
// Generated by Microsoft (R) D3D Shader Disassembler
//
//
// Input signature:
//
// Name                 Index   Mask Register SysValue  Format   Used
// -------------------- ----- ------ -------- -------- ------- ------
// SV_POSITION              0   xyzw        0      POS   float   xy  
// INTERP                   0   xyz         1     NONE   float       
// INTERP                   2   xyzw        2     NONE   float   xyzw
// INTERP                   3   xyzw        3     NONE   float   x   
// INTERP                   4   xyzw        4     NONE   float   xyz 
// INTERP                   5   xyz         5     NONE   float   xyz 
// INTERP                   6   xyz         6     NONE   float   xyz 
// SV_IsFrontFace           0   x           7    FFACE    uint   x   
//
//
// Output signature:
//
// Name                 Index   Mask Register SysValue  Format   Used
// -------------------- ----- ------ -------- -------- ------- ------
// SV_Target                0   xyzw        0   TARGET   float   xyzw
// SV_Target                1   xyzw        1   TARGET   float   xyzw
// SV_Target                2   xyzw        2   TARGET   float   xyzw
// SV_Target                3   xyzw        3   TARGET   float   xyzw
// SV_Target                4   x           4   TARGET    uint   x   
//
      ps_5_0
      dcl_globalFlags refactoringAllowed
      dcl_constantbuffer CB0[131], immediateIndexed
      dcl_constantbuffer CB1[11], immediateIndexed
      dcl_constantbuffer CB2[15], immediateIndexed
      dcl_sampler s0, mode_default
      dcl_sampler s1, mode_default
      dcl_sampler s2, mode_default
      dcl_sampler s3, mode_default
      dcl_sampler s4, mode_default
      dcl_sampler s5, mode_default
      dcl_resource_texture2d (float,float,float,float) t0
      dcl_resource_texture2d (float,float,float,float) t1
      dcl_resource_texture2d (float,float,float,float) t2
      dcl_resource_texture2d (float,float,float,float) t3
      dcl_resource_texture2d (float,float,float,float) t4
      dcl_resource_texture2d (float,float,float,float) t5
      dcl_input_ps_siv linear noperspective v0.xy, position
      dcl_input_ps linear v2.xyzw
      dcl_input_ps linear v3.x
      dcl_input_ps linear v4.xyz
      dcl_input_ps linear v5.xyz
      dcl_input_ps linear v6.xyz
      dcl_input_ps_sgv constant v7.x, is_front_face
      dcl_output o0.xyzw
      dcl_output o1.xyzw
      dcl_output o2.xyzw
      dcl_output o3.xyzw
      dcl_output o4.x
      dcl_temps 4
   0: lt r0.x, cb0[22].x, l(0.000000)
   1: add r0.y, -v0.y, cb0[3].y
   2: movc r0.y, r0.x, r0.y, v0.y
   3: mov r0.x, v0.x
   4: div r0.xy, r0.xyxx, cb0[3].xyxx
   5: add r0.z, -r0.y, l(1.000000)
   6: mad r0.yw, -cb0[130].xxxy, l(0.000000, 0.500000, 0.000000, 0.500000), l(0.000000, 1.000000, 0.000000, 1.000000)
   7: min r0.xy, r0.ywyy, r0.xzxx
   8: mul r0.xy, r0.xyxx, cb0[28].xyxx
   9: sample_b_indexable(texture2d)(float,float,float,float) r0.x, r0.xyxx, t1.xyzw, s0, cb0[4].x
  10: mad r0.x, cb0[24].x, r0.x, cb0[24].y
  11: div r0.x, l(1.000000, 1.000000, 1.000000, 1.000000), r0.x
  12: mul r0.y, v5.y, cb0[79].w
  13: mad r0.y, cb0[78].w, v5.x, r0.y
  14: mad r0.y, cb0[80].w, v5.z, r0.y
  15: add r0.y, r0.y, cb0[81].w
  16: mad r0.x, r0.x, cb0[22].z, -r0.y
  17: div_sat r0.x, r0.x, cb2[14].w
  18: ge r0.y, l(0.000000), r0.x
  19: and r0.y, r0.y, l(0x3f800000)
  20: add r0.z, -r0.x, l(1.000000)
  21: mad r0.x, r0.y, r0.z, r0.x
  22: dp3 r0.y, v6.xyzx, v6.xyzx
  23: sqrt r0.z, r0.y
  24: rsq r0.y, r0.y
  25: mul o2.xyz, r0.yyyy, v6.xyzx
  26: div r0.y, l(1.000000, 1.000000, 1.000000, 1.000000), r0.z
  27: mul r0.yzw, r0.yyyy, v6.xxyz
  28: dp3 r1.x, r0.yzwy, r0.yzwy
  29: rsq r1.x, r1.x
  30: mul r0.yzw, r0.yyzw, r1.xxxx
  31: add r1.xyz, -v5.xyzx, cb0[21].xyzx
  32: dp3 r1.w, r1.xyzx, r1.xyzx
  33: rsq r1.w, r1.w
  34: mul r1.xyz, r1.wwww, r1.xyzx
  35: eq r1.w, cb0[25].w, l(0.000000)
  36: movc r2.x, r1.w, r1.x, cb0[66].z
  37: movc r2.y, r1.w, r1.y, cb0[67].z
  38: movc r2.z, r1.w, r1.z, cb0[68].z
  39: dp3_sat r0.y, r0.yzwy, r2.xyzx
  40: add r0.y, -r0.y, l(1.000000)
  41: log r0.y, r0.y
  42: mul r0.y, r0.y, cb2[0].y
  43: exp r0.y, r0.y
  44: mul_sat r0.y, r0.y, cb2[0].z
  45: and r0.y, r0.y, v7.x
  46: add r0.x, -r0.x, r0.y
  47: add r0.x, r0.x, l(1.000000)
  48: mul r0.y, v2.w, l(1.000100)
  49: ge r0.z, r0.y, -r0.y
  50: frc r0.y, |r0.y|
  51: movc r0.y, r0.z, r0.y, -r0.y
  52: mul r0.y, r0.y, l(0.999900)
  53: div r0.zw, cb2[10].zzzw, cb2[5].xxxy
  54: mad r0.yz, cb0[19].xxxx, r0.zzwz, r0.yyyy
  55: add r0.yz, r0.yyzy, v2.xxyx
  56: mad r0.yz, r0.yyzy, cb2[5].xxyx, cb2[5].zzwz
  57: sample_b_indexable(texture2d)(float,float,float,float) r1.xyzw, r0.yzyy, t3.xyzw, s3, cb0[4].x
  58: div r0.yz, cb2[11].xxyx, cb2[7].xxyx
  59: mad r0.yz, r0.yyzy, cb0[19].xxxx, v2.xxyx
  60: mad r0.yz, r0.yyzy, cb2[7].xxyx, cb2[7].zzwz
  61: sample_b_indexable(texture2d)(float,float,float,float) r0.yz, r0.yzyy, t4.zxyw, s4, cb0[4].x
  62: mad r2.xy, v2.xyxx, cb2[9].xyxx, cb2[9].zwzz
  63: sample_b_indexable(texture2d)(float,float,float,float) r2.xyz, r2.xyxx, t5.xyzw, s5, cb0[4].x
  64: mul r0.yz, r0.yyzy, r2.xxyx
  65: mul r0.yz, r0.yyzy, cb2[11].zzzz
  66: div r0.yz, r0.yyzy, cb2[3].xxyx
  67: div r3.xy, cb2[10].xyxx, cb2[3].xyxx
  68: mad r3.xy, r3.xyxx, cb0[19].xxxx, v2.xyxx
  69: add r0.yz, -r0.yyzy, r3.xxyx
  70: mad r0.yz, r0.yyzy, cb2[3].xxyx, cb2[3].zzwz
  71: sample_b_indexable(texture2d)(float,float,float,float) r3.xyzw, r0.yzyy, t2.xyzw, s2, cb0[4].x
  72: mul r1.xyzw, r1.wxyz, r3.wxyz
  73: ne r0.yz, l(0.000000, 0.000000, 0.000000, 0.000000), cb2[1].xxyx
  74: movc r3.xyz, r0.yyyy, r1.xxxx, r1.yzwy
  75: max r0.xyw, r0.xxxx, r3.xyxz
  76: movc r0.xyz, r0.zzzz, r0.xywx, r1.yzwy
  77: add r0.w, -v2.z, l(1.000000)
  78: add_sat r1.xyz, -r0.wwww, r2.xyzx
  79: mul_sat r1.xyz, r1.xyzx, r2.xyzx
  80: mul r1.xyz, r0.xyzx, r1.xyzx
  81: ne r0.w, l(0.000000, 0.000000, 0.000000, 0.000000), cb2[14].y
  82: movc r0.xyz, r0.wwww, r1.xyzx, r0.xyzx
  83: mul r0.xyz, r0.xyzx, cb2[12].xxxx
  84: mul r0.xyz, r0.xyzx, v4.xyzx
  85: max r0.w, v3.x, l(1.000000)
  86: min r0.w, r0.w, l(1000000.000000)
  87: eq r1.x, v2.x, v3.x
  88: movc r0.w, r1.x, l(1.000000), r0.w
  89: mul r0.xyz, r0.xyzx, r0.wwww
  90: rcp r1.xy, cb0[3].xyxx
  91: mul r1.xy, r1.xyxx, v0.xyxx
  92: mad r0.w, r1.y, cb0[27].x, cb0[27].y
  93: add r1.z, -r0.w, l(1.000000)
  94: sample_b_indexable(texture2d)(float,float,float,float) r0.w, r1.xzxx, t0.yzwx, s1, cb0[4].x
  95: add r1.x, -cb0[9].x, l(1.000000)
  96: add_sat r0.w, r0.w, r1.x
  97: add r0.w, r0.w, l(-1.000000)
  98: mad r0.w, cb0[9].w, r0.w, l(1.000000)
  99: mul o0.xyz, r0.wwww, r0.xyzx
 100: mov o0.w, l(0)
 101: mov o1.xyzw, l(0,0,0,0)
 102: mov o2.w, l(0)
 103: mov o3.xyzw, l(0,0,0,1.000000)
 104: and o4.x, cb0[11].x, cb1[10].x
 105: ret 
// Approximately 0 instruction slots used


//////////////////////////////////////////////////////
Keywords: _SCREEN_SPACE_OCCLUSION _SURFACE_TYPE_TRANSPARENT _WRITE_RENDERING_LAYERS
-- Vertex shader for "d3d11":
// No shader variant for this keyword set. The closest match will be used instead.

-- Hardware tier variant: Tier 1
-- Fragment shader for "d3d11":
// Stats: 74 math, 4 temp registers
Set 2D Texture "_CameraDepthTexture" to slot 0 sampler slot -1
Set 2D Texture "_MainTex" to slot 1
Set 2D Texture "_Noise" to slot 2
Set 2D Texture "_Flow" to slot 3
Set 2D Texture "_Mask" to slot 4

Set Sampler PointClamp to slot 0

Constant Buffer "$Globals" (2096 bytes) on slot 0 {
  Matrix4x4 unity_MatrixV at 1056
  Matrix4x4 unity_MatrixVP at 1248
  Vector4 _ScaledScreenParams at 48
  Vector2 _GlobalMipBias at 64
  ScalarInt _RenderingLayerMaxInt at 176
  Vector4 _TimeParameters at 304
  Vector3 _WorldSpaceCameraPos at 336
  Vector4 _ProjectionParams at 352
  Vector4 _ZBufferParams at 384
  Vector4 unity_OrthoParams at 400
  Vector4 _RTHandleScale at 448
  Vector4 _CameraDepthTexture_TexelSize at 2080
}
Constant Buffer "UnityPerDraw" (720 bytes) on slot 1 {
  Vector4 unity_RenderingLayer at 160
}
Constant Buffer "UnityPerMaterial" (304 bytes) on slot 2 {
  Float _Fresnelpower at 4
  Float _Fresnelscale at 8
  Float _Useonlycolor at 16
  Float _Use_fresnel at 20
  Vector4 _MainTex_ST at 48
  Vector4 _Noise_ST at 80
  Vector4 _Flow_ST at 112
  Vector4 _Mask_ST at 144
  Vector4 _SpeedMainTexUVNoiseZW at 160
  Vector4 _DistortionSpeedXYPowerZ at 176
  Float _Emission at 192
  Float _Usecenterglow at 228
  Float _Depthpower at 236
}

Shader Disassembly:
//
// Generated by Microsoft (R) D3D Shader Disassembler
//
//
// Input signature:
//
// Name                 Index   Mask Register SysValue  Format   Used
// -------------------- ----- ------ -------- -------- ------- ------
// SV_POSITION              0   xyzw        0      POS   float   xy  
// INTERP                   0   xyz         1     NONE   float       
// INTERP                   2   xyzw        2     NONE   float   xyzw
// INTERP                   3   xyzw        3     NONE   float   x   
// INTERP                   4   xyzw        4     NONE   float   xyz 
// INTERP                   5   xyz         5     NONE   float   xyz 
// INTERP                   6   xyz         6     NONE   float   xyz 
// SV_IsFrontFace           0   x           7    FFACE    uint   x   
//
//
// Output signature:
//
// Name                 Index   Mask Register SysValue  Format   Used
// -------------------- ----- ------ -------- -------- ------- ------
// SV_Target                0   xyzw        0   TARGET   float   xyzw
// SV_Target                1   xyzw        1   TARGET   float   xyzw
// SV_Target                2   xyzw        2   TARGET   float   xyzw
// SV_Target                3   xyzw        3   TARGET   float   xyzw
// SV_Target                4   x           4   TARGET    uint   x   
//
      ps_5_0
      dcl_globalFlags refactoringAllowed
      dcl_constantbuffer CB0[131], immediateIndexed
      dcl_constantbuffer CB1[11], immediateIndexed
      dcl_constantbuffer CB2[15], immediateIndexed
      dcl_sampler s0, mode_default
      dcl_sampler s1, mode_default
      dcl_sampler s2, mode_default
      dcl_sampler s3, mode_default
      dcl_sampler s4, mode_default
      dcl_resource_texture2d (float,float,float,float) t0
      dcl_resource_texture2d (float,float,float,float) t1
      dcl_resource_texture2d (float,float,float,float) t2
      dcl_resource_texture2d (float,float,float,float) t3
      dcl_resource_texture2d (float,float,float,float) t4
      dcl_input_ps_siv linear noperspective v0.xy, position
      dcl_input_ps linear v2.xyzw
      dcl_input_ps linear v3.x
      dcl_input_ps linear v4.xyz
      dcl_input_ps linear v5.xyz
      dcl_input_ps linear v6.xyz
      dcl_input_ps_sgv constant v7.x, is_front_face
      dcl_output o0.xyzw
      dcl_output o1.xyzw
      dcl_output o2.xyzw
      dcl_output o3.xyzw
      dcl_output o4.x
      dcl_temps 4
   0: lt r0.x, cb0[22].x, l(0.000000)
   1: add r0.y, -v0.y, cb0[3].y
   2: movc r0.y, r0.x, r0.y, v0.y
   3: mov r0.x, v0.x
   4: div r0.xy, r0.xyxx, cb0[3].xyxx
   5: add r0.z, -r0.y, l(1.000000)
   6: mad r0.yw, -cb0[130].xxxy, l(0.000000, 0.500000, 0.000000, 0.500000), l(0.000000, 1.000000, 0.000000, 1.000000)
   7: min r0.xy, r0.ywyy, r0.xzxx
   8: mul r0.xy, r0.xyxx, cb0[28].xyxx
   9: sample_b_indexable(texture2d)(float,float,float,float) r0.x, r0.xyxx, t0.xyzw, s0, cb0[4].x
  10: mad r0.x, cb0[24].x, r0.x, cb0[24].y
  11: div r0.x, l(1.000000, 1.000000, 1.000000, 1.000000), r0.x
  12: mul r0.y, v5.y, cb0[79].w
  13: mad r0.y, cb0[78].w, v5.x, r0.y
  14: mad r0.y, cb0[80].w, v5.z, r0.y
  15: add r0.y, r0.y, cb0[81].w
  16: mad r0.x, r0.x, cb0[22].z, -r0.y
  17: div_sat r0.x, r0.x, cb2[14].w
  18: ge r0.y, l(0.000000), r0.x
  19: and r0.y, r0.y, l(0x3f800000)
  20: add r0.z, -r0.x, l(1.000000)
  21: mad r0.x, r0.y, r0.z, r0.x
  22: dp3 r0.y, v6.xyzx, v6.xyzx
  23: sqrt r0.z, r0.y
  24: rsq r0.y, r0.y
  25: mul o2.xyz, r0.yyyy, v6.xyzx
  26: div r0.y, l(1.000000, 1.000000, 1.000000, 1.000000), r0.z
  27: mul r0.yzw, r0.yyyy, v6.xxyz
  28: dp3 r1.x, r0.yzwy, r0.yzwy
  29: rsq r1.x, r1.x
  30: mul r0.yzw, r0.yyzw, r1.xxxx
  31: add r1.xyz, -v5.xyzx, cb0[21].xyzx
  32: dp3 r1.w, r1.xyzx, r1.xyzx
  33: rsq r1.w, r1.w
  34: mul r1.xyz, r1.wwww, r1.xyzx
  35: eq r1.w, cb0[25].w, l(0.000000)
  36: movc r2.x, r1.w, r1.x, cb0[66].z
  37: movc r2.y, r1.w, r1.y, cb0[67].z
  38: movc r2.z, r1.w, r1.z, cb0[68].z
  39: dp3_sat r0.y, r0.yzwy, r2.xyzx
  40: add r0.y, -r0.y, l(1.000000)
  41: log r0.y, r0.y
  42: mul r0.y, r0.y, cb2[0].y
  43: exp r0.y, r0.y
  44: mul_sat r0.y, r0.y, cb2[0].z
  45: and r0.y, r0.y, v7.x
  46: add r0.x, -r0.x, r0.y
  47: add r0.x, r0.x, l(1.000000)
  48: mul r0.y, v2.w, l(1.000100)
  49: ge r0.z, r0.y, -r0.y
  50: frc r0.y, |r0.y|
  51: movc r0.y, r0.z, r0.y, -r0.y
  52: mul r0.y, r0.y, l(0.999900)
  53: div r0.zw, cb2[10].zzzw, cb2[5].xxxy
  54: mad r0.yz, cb0[19].xxxx, r0.zzwz, r0.yyyy
  55: add r0.yz, r0.yyzy, v2.xxyx
  56: mad r0.yz, r0.yyzy, cb2[5].xxyx, cb2[5].zzwz
  57: sample_b_indexable(texture2d)(float,float,float,float) r1.xyzw, r0.yzyy, t2.xyzw, s2, cb0[4].x
  58: div r0.yz, cb2[11].xxyx, cb2[7].xxyx
  59: mad r0.yz, r0.yyzy, cb0[19].xxxx, v2.xxyx
  60: mad r0.yz, r0.yyzy, cb2[7].xxyx, cb2[7].zzwz
  61: sample_b_indexable(texture2d)(float,float,float,float) r0.yz, r0.yzyy, t3.zxyw, s3, cb0[4].x
  62: mad r2.xy, v2.xyxx, cb2[9].xyxx, cb2[9].zwzz
  63: sample_b_indexable(texture2d)(float,float,float,float) r2.xyz, r2.xyxx, t4.xyzw, s4, cb0[4].x
  64: mul r0.yz, r0.yyzy, r2.xxyx
  65: mul r0.yz, r0.yyzy, cb2[11].zzzz
  66: div r0.yz, r0.yyzy, cb2[3].xxyx
  67: div r3.xy, cb2[10].xyxx, cb2[3].xyxx
  68: mad r3.xy, r3.xyxx, cb0[19].xxxx, v2.xyxx
  69: add r0.yz, -r0.yyzy, r3.xxyx
  70: mad r0.yz, r0.yyzy, cb2[3].xxyx, cb2[3].zzwz
  71: sample_b_indexable(texture2d)(float,float,float,float) r3.xyzw, r0.yzyy, t1.xyzw, s1, cb0[4].x
  72: mul r1.xyzw, r1.wxyz, r3.wxyz
  73: ne r0.yz, l(0.000000, 0.000000, 0.000000, 0.000000), cb2[1].xxyx
  74: movc r3.xyz, r0.yyyy, r1.xxxx, r1.yzwy
  75: max r0.xyw, r0.xxxx, r3.xyxz
  76: movc r0.xyz, r0.zzzz, r0.xywx, r1.yzwy
  77: add r0.w, -v2.z, l(1.000000)
  78: add_sat r1.xyz, -r0.wwww, r2.xyzx
  79: mul_sat r1.xyz, r1.xyzx, r2.xyzx
  80: mul r1.xyz, r0.xyzx, r1.xyzx
  81: ne r0.w, l(0.000000, 0.000000, 0.000000, 0.000000), cb2[14].y
  82: movc r0.xyz, r0.wwww, r1.xyzx, r0.xyzx
  83: mul r0.xyz, r0.xyzx, cb2[12].xxxx
  84: mul r0.xyz, r0.xyzx, v4.xyzx
  85: max r0.w, v3.x, l(1.000000)
  86: min r0.w, r0.w, l(1000000.000000)
  87: eq r1.x, v2.x, v3.x
  88: movc r0.w, r1.x, l(1.000000), r0.w
  89: mul o0.xyz, r0.xyzx, r0.wwww
  90: mov o0.w, l(0)
  91: mov o1.xyzw, l(0,0,0,1.000000)
  92: mov o2.w, l(0)
  93: mov o3.xyzw, l(0,0,0,1.000000)
  94: and o4.x, cb0[11].x, cb1[10].x
  95: ret 
// Approximately 0 instruction slots used


//////////////////////////////////////////////////////
Keywords: _DBUFFER_MRT3
-- Vertex shader for "d3d11":
// No shader variant for this keyword set. The closest match will be used instead.

-- Hardware tier variant: Tier 1
-- Fragment shader for "d3d11":
// Stats: 74 math, 4 temp registers
Set 2D Texture "_DBufferTexture0" to slot 0 sampler slot -1
Set 2D Texture "_CameraDepthTexture" to slot 1 sampler slot -1
Set 2D Texture "_MainTex" to slot 2 sampler slot 1
Set 2D Texture "_Noise" to slot 3 sampler slot 2
Set 2D Texture "_Flow" to slot 4 sampler slot 3
Set 2D Texture "_Mask" to slot 5 sampler slot 4

Set Sampler PointClamp to slot 0

Constant Buffer "$Globals" (2096 bytes) on slot 0 {
  Matrix4x4 unity_MatrixV at 1056
  Matrix4x4 unity_MatrixVP at 1248
  Vector4 _ScaledScreenParams at 48
  Vector2 _GlobalMipBias at 64
  Vector4 _TimeParameters at 304
  Vector3 _WorldSpaceCameraPos at 336
  Vector4 _ProjectionParams at 352
  Vector4 _ZBufferParams at 384
  Vector4 unity_OrthoParams at 400
  Vector4 _RTHandleScale at 448
  Vector4 _CameraDepthTexture_TexelSize at 2080
}
Constant Buffer "UnityPerMaterial" (304 bytes) on slot 1 {
  Float _Fresnelpower at 4
  Float _Fresnelscale at 8
  Float _Useonlycolor at 16
  Float _Use_fresnel at 20
  Vector4 _MainTex_ST at 48
  Vector4 _Noise_ST at 80
  Vector4 _Flow_ST at 112
  Vector4 _Mask_ST at 144
  Vector4 _SpeedMainTexUVNoiseZW at 160
  Vector4 _DistortionSpeedXYPowerZ at 176
  Float _Emission at 192
  Float _Usecenterglow at 228
  Float _Depthpower at 236
}

Shader Disassembly:
//
// Generated by Microsoft (R) D3D Shader Disassembler
//
//
// Input signature:
//
// Name                 Index   Mask Register SysValue  Format   Used
// -------------------- ----- ------ -------- -------- ------- ------
// SV_POSITION              0   xyzw        0      POS   float   xy  
// INTERP                   0   xyz         1     NONE   float       
// INTERP                   2   xyzw        2     NONE   float   xyzw
// INTERP                   3   xyzw        3     NONE   float   x   
// INTERP                   4   xyzw        4     NONE   float   xyz 
// INTERP                   5   xyz         5     NONE   float   xyz 
// INTERP                   6   xyz         6     NONE   float   xyz 
// SV_IsFrontFace           0   x           7    FFACE    uint   x   
//
//
// Output signature:
//
// Name                 Index   Mask Register SysValue  Format   Used
// -------------------- ----- ------ -------- -------- ------- ------
// SV_Target                0   xyzw        0   TARGET   float   xyzw
// SV_Target                1   xyzw        1   TARGET   float   xyzw
// SV_Target                2   xyzw        2   TARGET   float   xyzw
// SV_Target                3   xyzw        3   TARGET   float   xyzw
//
      ps_5_0
      dcl_globalFlags refactoringAllowed
      dcl_constantbuffer CB0[131], immediateIndexed
      dcl_constantbuffer CB1[15], immediateIndexed
      dcl_sampler s0, mode_default
      dcl_sampler s1, mode_default
      dcl_sampler s2, mode_default
      dcl_sampler s3, mode_default
      dcl_sampler s4, mode_default
      dcl_resource_texture2d (float,float,float,float) t0
      dcl_resource_texture2d (float,float,float,float) t1
      dcl_resource_texture2d (float,float,float,float) t2
      dcl_resource_texture2d (float,float,float,float) t3
      dcl_resource_texture2d (float,float,float,float) t4
      dcl_resource_texture2d (float,float,float,float) t5
      dcl_input_ps_siv linear noperspective v0.xy, position
      dcl_input_ps linear v2.xyzw
      dcl_input_ps linear v3.x
      dcl_input_ps linear v4.xyz
      dcl_input_ps linear v5.xyz
      dcl_input_ps linear v6.xyz
      dcl_input_ps_sgv constant v7.x, is_front_face
      dcl_output o0.xyzw
      dcl_output o1.xyzw
      dcl_output o2.xyzw
      dcl_output o3.xyzw
      dcl_temps 4
   0: lt r0.x, cb0[22].x, l(0.000000)
   1: add r0.y, -v0.y, cb0[3].y
   2: movc r0.y, r0.x, r0.y, v0.y
   3: mov r0.x, v0.x
   4: div r0.xy, r0.xyxx, cb0[3].xyxx
   5: add r0.z, -r0.y, l(1.000000)
   6: mad r0.yw, -cb0[130].xxxy, l(0.000000, 0.500000, 0.000000, 0.500000), l(0.000000, 1.000000, 0.000000, 1.000000)
   7: min r0.xy, r0.ywyy, r0.xzxx
   8: mul r0.xy, r0.xyxx, cb0[28].xyxx
   9: sample_b_indexable(texture2d)(float,float,float,float) r0.x, r0.xyxx, t1.xyzw, s0, cb0[4].x
  10: mad r0.x, cb0[24].x, r0.x, cb0[24].y
  11: div r0.x, l(1.000000, 1.000000, 1.000000, 1.000000), r0.x
  12: mul r0.y, v5.y, cb0[79].w
  13: mad r0.y, cb0[78].w, v5.x, r0.y
  14: mad r0.y, cb0[80].w, v5.z, r0.y
  15: add r0.y, r0.y, cb0[81].w
  16: mad r0.x, r0.x, cb0[22].z, -r0.y
  17: div_sat r0.x, r0.x, cb1[14].w
  18: ge r0.y, l(0.000000), r0.x
  19: and r0.y, r0.y, l(0x3f800000)
  20: add r0.z, -r0.x, l(1.000000)
  21: mad r0.x, r0.y, r0.z, r0.x
  22: dp3 r0.y, v6.xyzx, v6.xyzx
  23: sqrt r0.z, r0.y
  24: rsq r0.y, r0.y
  25: mul o2.xyz, r0.yyyy, v6.xyzx
  26: div r0.y, l(1.000000, 1.000000, 1.000000, 1.000000), r0.z
  27: mul r0.yzw, r0.yyyy, v6.xxyz
  28: dp3 r1.x, r0.yzwy, r0.yzwy
  29: rsq r1.x, r1.x
  30: mul r0.yzw, r0.yyzw, r1.xxxx
  31: add r1.xyz, -v5.xyzx, cb0[21].xyzx
  32: dp3 r1.w, r1.xyzx, r1.xyzx
  33: rsq r1.w, r1.w
  34: mul r1.xyz, r1.wwww, r1.xyzx
  35: eq r1.w, cb0[25].w, l(0.000000)
  36: movc r2.x, r1.w, r1.x, cb0[66].z
  37: movc r2.y, r1.w, r1.y, cb0[67].z
  38: movc r2.z, r1.w, r1.z, cb0[68].z
  39: dp3_sat r0.y, r0.yzwy, r2.xyzx
  40: add r0.y, -r0.y, l(1.000000)
  41: log r0.y, r0.y
  42: mul r0.y, r0.y, cb1[0].y
  43: exp r0.y, r0.y
  44: mul_sat r0.y, r0.y, cb1[0].z
  45: and r0.y, r0.y, v7.x
  46: add r0.x, -r0.x, r0.y
  47: add r0.x, r0.x, l(1.000000)
  48: mul r0.y, v2.w, l(1.000100)
  49: ge r0.z, r0.y, -r0.y
  50: frc r0.y, |r0.y|
  51: movc r0.y, r0.z, r0.y, -r0.y
  52: mul r0.y, r0.y, l(0.999900)
  53: div r0.zw, cb1[10].zzzw, cb1[5].xxxy
  54: mad r0.yz, cb0[19].xxxx, r0.zzwz, r0.yyyy
  55: add r0.yz, r0.yyzy, v2.xxyx
  56: mad r0.yz, r0.yyzy, cb1[5].xxyx, cb1[5].zzwz
  57: sample_b_indexable(texture2d)(float,float,float,float) r1.xyzw, r0.yzyy, t3.xyzw, s2, cb0[4].x
  58: div r0.yz, cb1[11].xxyx, cb1[7].xxyx
  59: mad r0.yz, r0.yyzy, cb0[19].xxxx, v2.xxyx
  60: mad r0.yz, r0.yyzy, cb1[7].xxyx, cb1[7].zzwz
  61: sample_b_indexable(texture2d)(float,float,float,float) r0.yz, r0.yzyy, t4.zxyw, s3, cb0[4].x
  62: mad r2.xy, v2.xyxx, cb1[9].xyxx, cb1[9].zwzz
  63: sample_b_indexable(texture2d)(float,float,float,float) r2.xyz, r2.xyxx, t5.xyzw, s4, cb0[4].x
  64: mul r0.yz, r0.yyzy, r2.xxyx
  65: mul r0.yz, r0.yyzy, cb1[11].zzzz
  66: div r0.yz, r0.yyzy, cb1[3].xxyx
  67: div r3.xy, cb1[10].xyxx, cb1[3].xyxx
  68: mad r3.xy, r3.xyxx, cb0[19].xxxx, v2.xyxx
  69: add r0.yz, -r0.yyzy, r3.xxyx
  70: mad r0.yz, r0.yyzy, cb1[3].xxyx, cb1[3].zzwz
  71: sample_b_indexable(texture2d)(float,float,float,float) r3.xyzw, r0.yzyy, t2.xyzw, s1, cb0[4].x
  72: mul r1.xyzw, r1.wxyz, r3.wxyz
  73: ne r0.yz, l(0.000000, 0.000000, 0.000000, 0.000000), cb1[1].xxyx
  74: movc r3.xyz, r0.yyyy, r1.xxxx, r1.yzwy
  75: max r0.xyw, r0.xxxx, r3.xyxz
  76: movc r0.xyz, r0.zzzz, r0.xywx, r1.yzwy
  77: add r0.w, -v2.z, l(1.000000)
  78: add_sat r1.xyz, -r0.wwww, r2.xyzx
  79: mul_sat r1.xyz, r1.xyzx, r2.xyzx
  80: mul r1.xyz, r0.xyzx, r1.xyzx
  81: ne r0.w, l(0.000000, 0.000000, 0.000000, 0.000000), cb1[14].y
  82: movc r0.xyz, r0.wwww, r1.xyzx, r0.xyzx
  83: mul r0.xyz, r0.xyzx, cb1[12].xxxx
  84: mul r0.xyz, r0.xyzx, v4.xyzx
  85: max r0.w, v3.x, l(1.000000)
  86: min r0.w, r0.w, l(1000000.000000)
  87: eq r1.x, v2.x, v3.x
  88: movc r0.w, r1.x, l(1.000000), r0.w
  89: mul r0.xyz, r0.xyzx, r0.wwww
  90: ftoi r1.xy, v0.xyxx
  91: mov r1.zw, l(0,0,0,0)
  92: ld_indexable(texture2d)(float,float,float,float) r1.xyzw, r1.xyzw, t0.xyzw
  93: mad o0.xyz, r0.xyzx, r1.wwww, r1.xyzx
  94: mov o0.w, l(0)
  95: mov o1.xyzw, l(0,0,0,1.000000)
  96: mov o2.w, l(0)
  97: mov o3.xyzw, l(0,0,0,1.000000)
  98: ret 
// Approximately 0 instruction slots used


//////////////////////////////////////////////////////
Keywords: _DBUFFER_MRT3 _SURFACE_TYPE_TRANSPARENT
-- Vertex shader for "d3d11":
// No shader variant for this keyword set. The closest match will be used instead.

-- Hardware tier variant: Tier 1
-- Fragment shader for "d3d11":
// Stats: 73 math, 4 temp registers
Set 2D Texture "_CameraDepthTexture" to slot 0 sampler slot -1
Set 2D Texture "_MainTex" to slot 1
Set 2D Texture "_Noise" to slot 2
Set 2D Texture "_Flow" to slot 3
Set 2D Texture "_Mask" to slot 4

Set Sampler PointClamp to slot 0

Constant Buffer "$Globals" (2096 bytes) on slot 0 {
  Matrix4x4 unity_MatrixV at 1056
  Matrix4x4 unity_MatrixVP at 1248
  Vector4 _ScaledScreenParams at 48
  Vector2 _GlobalMipBias at 64
  Vector4 _TimeParameters at 304
  Vector3 _WorldSpaceCameraPos at 336
  Vector4 _ProjectionParams at 352
  Vector4 _ZBufferParams at 384
  Vector4 unity_OrthoParams at 400
  Vector4 _RTHandleScale at 448
  Vector4 _CameraDepthTexture_TexelSize at 2080
}
Constant Buffer "UnityPerMaterial" (304 bytes) on slot 1 {
  Float _Fresnelpower at 4
  Float _Fresnelscale at 8
  Float _Useonlycolor at 16
  Float _Use_fresnel at 20
  Vector4 _MainTex_ST at 48
  Vector4 _Noise_ST at 80
  Vector4 _Flow_ST at 112
  Vector4 _Mask_ST at 144
  Vector4 _SpeedMainTexUVNoiseZW at 160
  Vector4 _DistortionSpeedXYPowerZ at 176
  Float _Emission at 192
  Float _Usecenterglow at 228
  Float _Depthpower at 236
}

Shader Disassembly:
//
// Generated by Microsoft (R) D3D Shader Disassembler
//
//
// Input signature:
//
// Name                 Index   Mask Register SysValue  Format   Used
// -------------------- ----- ------ -------- -------- ------- ------
// SV_POSITION              0   xyzw        0      POS   float   xy  
// INTERP                   0   xyz         1     NONE   float       
// INTERP                   2   xyzw        2     NONE   float   xyzw
// INTERP                   3   xyzw        3     NONE   float   x   
// INTERP                   4   xyzw        4     NONE   float   xyz 
// INTERP                   5   xyz         5     NONE   float   xyz 
// INTERP                   6   xyz         6     NONE   float   xyz 
// SV_IsFrontFace           0   x           7    FFACE    uint   x   
//
//
// Output signature:
//
// Name                 Index   Mask Register SysValue  Format   Used
// -------------------- ----- ------ -------- -------- ------- ------
// SV_Target                0   xyzw        0   TARGET   float   xyzw
// SV_Target                1   xyzw        1   TARGET   float   xyzw
// SV_Target                2   xyzw        2   TARGET   float   xyzw
// SV_Target                3   xyzw        3   TARGET   float   xyzw
//
      ps_5_0
      dcl_globalFlags refactoringAllowed
      dcl_constantbuffer CB0[131], immediateIndexed
      dcl_constantbuffer CB1[15], immediateIndexed
      dcl_sampler s0, mode_default
      dcl_sampler s1, mode_default
      dcl_sampler s2, mode_default
      dcl_sampler s3, mode_default
      dcl_sampler s4, mode_default
      dcl_resource_texture2d (float,float,float,float) t0
      dcl_resource_texture2d (float,float,float,float) t1
      dcl_resource_texture2d (float,float,float,float) t2
      dcl_resource_texture2d (float,float,float,float) t3
      dcl_resource_texture2d (float,float,float,float) t4
      dcl_input_ps_siv linear noperspective v0.xy, position
      dcl_input_ps linear v2.xyzw
      dcl_input_ps linear v3.x
      dcl_input_ps linear v4.xyz
      dcl_input_ps linear v5.xyz
      dcl_input_ps linear v6.xyz
      dcl_input_ps_sgv constant v7.x, is_front_face
      dcl_output o0.xyzw
      dcl_output o1.xyzw
      dcl_output o2.xyzw
      dcl_output o3.xyzw
      dcl_temps 4
   0: lt r0.x, cb0[22].x, l(0.000000)
   1: add r0.y, -v0.y, cb0[3].y
   2: movc r0.y, r0.x, r0.y, v0.y
   3: mov r0.x, v0.x
   4: div r0.xy, r0.xyxx, cb0[3].xyxx
   5: add r0.z, -r0.y, l(1.000000)
   6: mad r0.yw, -cb0[130].xxxy, l(0.000000, 0.500000, 0.000000, 0.500000), l(0.000000, 1.000000, 0.000000, 1.000000)
   7: min r0.xy, r0.ywyy, r0.xzxx
   8: mul r0.xy, r0.xyxx, cb0[28].xyxx
   9: sample_b_indexable(texture2d)(float,float,float,float) r0.x, r0.xyxx, t0.xyzw, s0, cb0[4].x
  10: mad r0.x, cb0[24].x, r0.x, cb0[24].y
  11: div r0.x, l(1.000000, 1.000000, 1.000000, 1.000000), r0.x
  12: mul r0.y, v5.y, cb0[79].w
  13: mad r0.y, cb0[78].w, v5.x, r0.y
  14: mad r0.y, cb0[80].w, v5.z, r0.y
  15: add r0.y, r0.y, cb0[81].w
  16: mad r0.x, r0.x, cb0[22].z, -r0.y
  17: div_sat r0.x, r0.x, cb1[14].w
  18: ge r0.y, l(0.000000), r0.x
  19: and r0.y, r0.y, l(0x3f800000)
  20: add r0.z, -r0.x, l(1.000000)
  21: mad r0.x, r0.y, r0.z, r0.x
  22: dp3 r0.y, v6.xyzx, v6.xyzx
  23: sqrt r0.z, r0.y
  24: rsq r0.y, r0.y
  25: mul o2.xyz, r0.yyyy, v6.xyzx
  26: div r0.y, l(1.000000, 1.000000, 1.000000, 1.000000), r0.z
  27: mul r0.yzw, r0.yyyy, v6.xxyz
  28: dp3 r1.x, r0.yzwy, r0.yzwy
  29: rsq r1.x, r1.x
  30: mul r0.yzw, r0.yyzw, r1.xxxx
  31: add r1.xyz, -v5.xyzx, cb0[21].xyzx
  32: dp3 r1.w, r1.xyzx, r1.xyzx
  33: rsq r1.w, r1.w
  34: mul r1.xyz, r1.wwww, r1.xyzx
  35: eq r1.w, cb0[25].w, l(0.000000)
  36: movc r2.x, r1.w, r1.x, cb0[66].z
  37: movc r2.y, r1.w, r1.y, cb0[67].z
  38: movc r2.z, r1.w, r1.z, cb0[68].z
  39: dp3_sat r0.y, r0.yzwy, r2.xyzx
  40: add r0.y, -r0.y, l(1.000000)
  41: log r0.y, r0.y
  42: mul r0.y, r0.y, cb1[0].y
  43: exp r0.y, r0.y
  44: mul_sat r0.y, r0.y, cb1[0].z
  45: and r0.y, r0.y, v7.x
  46: add r0.x, -r0.x, r0.y
  47: add r0.x, r0.x, l(1.000000)
  48: mul r0.y, v2.w, l(1.000100)
  49: ge r0.z, r0.y, -r0.y
  50: frc r0.y, |r0.y|
  51: movc r0.y, r0.z, r0.y, -r0.y
  52: mul r0.y, r0.y, l(0.999900)
  53: div r0.zw, cb1[10].zzzw, cb1[5].xxxy
  54: mad r0.yz, cb0[19].xxxx, r0.zzwz, r0.yyyy
  55: add r0.yz, r0.yyzy, v2.xxyx
  56: mad r0.yz, r0.yyzy, cb1[5].xxyx, cb1[5].zzwz
  57: sample_b_indexable(texture2d)(float,float,float,float) r1.xyzw, r0.yzyy, t2.xyzw, s2, cb0[4].x
  58: div r0.yz, cb1[11].xxyx, cb1[7].xxyx
  59: mad r0.yz, r0.yyzy, cb0[19].xxxx, v2.xxyx
  60: mad r0.yz, r0.yyzy, cb1[7].xxyx, cb1[7].zzwz
  61: sample_b_indexable(texture2d)(float,float,float,float) r0.yz, r0.yzyy, t3.zxyw, s3, cb0[4].x
  62: mad r2.xy, v2.xyxx, cb1[9].xyxx, cb1[9].zwzz
  63: sample_b_indexable(texture2d)(float,float,float,float) r2.xyz, r2.xyxx, t4.xyzw, s4, cb0[4].x
  64: mul r0.yz, r0.yyzy, r2.xxyx
  65: mul r0.yz, r0.yyzy, cb1[11].zzzz
  66: div r0.yz, r0.yyzy, cb1[3].xxyx
  67: div r3.xy, cb1[10].xyxx, cb1[3].xyxx
  68: mad r3.xy, r3.xyxx, cb0[19].xxxx, v2.xyxx
  69: add r0.yz, -r0.yyzy, r3.xxyx
  70: mad r0.yz, r0.yyzy, cb1[3].xxyx, cb1[3].zzwz
  71: sample_b_indexable(texture2d)(float,float,float,float) r3.xyzw, r0.yzyy, t1.xyzw, s1, cb0[4].x
  72: mul r1.xyzw, r1.wxyz, r3.wxyz
  73: ne r0.yz, l(0.000000, 0.000000, 0.000000, 0.000000), cb1[1].xxyx
  74: movc r3.xyz, r0.yyyy, r1.xxxx, r1.yzwy
  75: max r0.xyw, r0.xxxx, r3.xyxz
  76: movc r0.xyz, r0.zzzz, r0.xywx, r1.yzwy
  77: add r0.w, -v2.z, l(1.000000)
  78: add_sat r1.xyz, -r0.wwww, r2.xyzx
  79: mul_sat r1.xyz, r1.xyzx, r2.xyzx
  80: mul r1.xyz, r0.xyzx, r1.xyzx
  81: ne r0.w, l(0.000000, 0.000000, 0.000000, 0.000000), cb1[14].y
  82: movc r0.xyz, r0.wwww, r1.xyzx, r0.xyzx
  83: mul r0.xyz, r0.xyzx, cb1[12].xxxx
  84: mul r0.xyz, r0.xyzx, v4.xyzx
  85: max r0.w, v3.x, l(1.000000)
  86: min r0.w, r0.w, l(1000000.000000)
  87: eq r1.x, v2.x, v3.x
  88: movc r0.w, r1.x, l(1.000000), r0.w
  89: mul o0.xyz, r0.xyzx, r0.wwww
  90: mov o0.w, l(0)
  91: mov o1.xyzw, l(0,0,0,1.000000)
  92: mov o2.w, l(0)
  93: mov o3.xyzw, l(0,0,0,1.000000)
  94: ret 
// Approximately 0 instruction slots used


//////////////////////////////////////////////////////
Keywords: _DBUFFER_MRT3 _WRITE_RENDERING_LAYERS
-- Vertex shader for "d3d11":
// No shader variant for this keyword set. The closest match will be used instead.

-- Hardware tier variant: Tier 1
-- Fragment shader for "d3d11":
// Stats: 75 math, 4 temp registers
Set 2D Texture "_DBufferTexture0" to slot 0 sampler slot -1
Set 2D Texture "_CameraDepthTexture" to slot 1 sampler slot -1
Set 2D Texture "_MainTex" to slot 2 sampler slot 1
Set 2D Texture "_Noise" to slot 3 sampler slot 2
Set 2D Texture "_Flow" to slot 4 sampler slot 3
Set 2D Texture "_Mask" to slot 5 sampler slot 4

Set Sampler PointClamp to slot 0

Constant Buffer "$Globals" (2096 bytes) on slot 0 {
  Matrix4x4 unity_MatrixV at 1056
  Matrix4x4 unity_MatrixVP at 1248
  Vector4 _ScaledScreenParams at 48
  Vector2 _GlobalMipBias at 64
  ScalarInt _RenderingLayerMaxInt at 176
  Vector4 _TimeParameters at 304
  Vector3 _WorldSpaceCameraPos at 336
  Vector4 _ProjectionParams at 352
  Vector4 _ZBufferParams at 384
  Vector4 unity_OrthoParams at 400
  Vector4 _RTHandleScale at 448
  Vector4 _CameraDepthTexture_TexelSize at 2080
}
Constant Buffer "UnityPerDraw" (720 bytes) on slot 1 {
  Vector4 unity_RenderingLayer at 160
}
Constant Buffer "UnityPerMaterial" (304 bytes) on slot 2 {
  Float _Fresnelpower at 4
  Float _Fresnelscale at 8
  Float _Useonlycolor at 16
  Float _Use_fresnel at 20
  Vector4 _MainTex_ST at 48
  Vector4 _Noise_ST at 80
  Vector4 _Flow_ST at 112
  Vector4 _Mask_ST at 144
  Vector4 _SpeedMainTexUVNoiseZW at 160
  Vector4 _DistortionSpeedXYPowerZ at 176
  Float _Emission at 192
  Float _Usecenterglow at 228
  Float _Depthpower at 236
}

Shader Disassembly:
//
// Generated by Microsoft (R) D3D Shader Disassembler
//
//
// Input signature:
//
// Name                 Index   Mask Register SysValue  Format   Used
// -------------------- ----- ------ -------- -------- ------- ------
// SV_POSITION              0   xyzw        0      POS   float   xy  
// INTERP                   0   xyz         1     NONE   float       
// INTERP                   2   xyzw        2     NONE   float   xyzw
// INTERP                   3   xyzw        3     NONE   float   x   
// INTERP                   4   xyzw        4     NONE   float   xyz 
// INTERP                   5   xyz         5     NONE   float   xyz 
// INTERP                   6   xyz         6     NONE   float   xyz 
// SV_IsFrontFace           0   x           7    FFACE    uint   x   
//
//
// Output signature:
//
// Name                 Index   Mask Register SysValue  Format   Used
// -------------------- ----- ------ -------- -------- ------- ------
// SV_Target                0   xyzw        0   TARGET   float   xyzw
// SV_Target                1   xyzw        1   TARGET   float   xyzw
// SV_Target                2   xyzw        2   TARGET   float   xyzw
// SV_Target                3   xyzw        3   TARGET   float   xyzw
// SV_Target                4   x           4   TARGET    uint   x   
//
      ps_5_0
      dcl_globalFlags refactoringAllowed
      dcl_constantbuffer CB0[131], immediateIndexed
      dcl_constantbuffer CB1[11], immediateIndexed
      dcl_constantbuffer CB2[15], immediateIndexed
      dcl_sampler s0, mode_default
      dcl_sampler s1, mode_default
      dcl_sampler s2, mode_default
      dcl_sampler s3, mode_default
      dcl_sampler s4, mode_default
      dcl_resource_texture2d (float,float,float,float) t0
      dcl_resource_texture2d (float,float,float,float) t1
      dcl_resource_texture2d (float,float,float,float) t2
      dcl_resource_texture2d (float,float,float,float) t3
      dcl_resource_texture2d (float,float,float,float) t4
      dcl_resource_texture2d (float,float,float,float) t5
      dcl_input_ps_siv linear noperspective v0.xy, position
      dcl_input_ps linear v2.xyzw
      dcl_input_ps linear v3.x
      dcl_input_ps linear v4.xyz
      dcl_input_ps linear v5.xyz
      dcl_input_ps linear v6.xyz
      dcl_input_ps_sgv constant v7.x, is_front_face
      dcl_output o0.xyzw
      dcl_output o1.xyzw
      dcl_output o2.xyzw
      dcl_output o3.xyzw
      dcl_output o4.x
      dcl_temps 4
   0: lt r0.x, cb0[22].x, l(0.000000)
   1: add r0.y, -v0.y, cb0[3].y
   2: movc r0.y, r0.x, r0.y, v0.y
   3: mov r0.x, v0.x
   4: div r0.xy, r0.xyxx, cb0[3].xyxx
   5: add r0.z, -r0.y, l(1.000000)
   6: mad r0.yw, -cb0[130].xxxy, l(0.000000, 0.500000, 0.000000, 0.500000), l(0.000000, 1.000000, 0.000000, 1.000000)
   7: min r0.xy, r0.ywyy, r0.xzxx
   8: mul r0.xy, r0.xyxx, cb0[28].xyxx
   9: sample_b_indexable(texture2d)(float,float,float,float) r0.x, r0.xyxx, t1.xyzw, s0, cb0[4].x
  10: mad r0.x, cb0[24].x, r0.x, cb0[24].y
  11: div r0.x, l(1.000000, 1.000000, 1.000000, 1.000000), r0.x
  12: mul r0.y, v5.y, cb0[79].w
  13: mad r0.y, cb0[78].w, v5.x, r0.y
  14: mad r0.y, cb0[80].w, v5.z, r0.y
  15: add r0.y, r0.y, cb0[81].w
  16: mad r0.x, r0.x, cb0[22].z, -r0.y
  17: div_sat r0.x, r0.x, cb2[14].w
  18: ge r0.y, l(0.000000), r0.x
  19: and r0.y, r0.y, l(0x3f800000)
  20: add r0.z, -r0.x, l(1.000000)
  21: mad r0.x, r0.y, r0.z, r0.x
  22: dp3 r0.y, v6.xyzx, v6.xyzx
  23: sqrt r0.z, r0.y
  24: rsq r0.y, r0.y
  25: mul o2.xyz, r0.yyyy, v6.xyzx
  26: div r0.y, l(1.000000, 1.000000, 1.000000, 1.000000), r0.z
  27: mul r0.yzw, r0.yyyy, v6.xxyz
  28: dp3 r1.x, r0.yzwy, r0.yzwy
  29: rsq r1.x, r1.x
  30: mul r0.yzw, r0.yyzw, r1.xxxx
  31: add r1.xyz, -v5.xyzx, cb0[21].xyzx
  32: dp3 r1.w, r1.xyzx, r1.xyzx
  33: rsq r1.w, r1.w
  34: mul r1.xyz, r1.wwww, r1.xyzx
  35: eq r1.w, cb0[25].w, l(0.000000)
  36: movc r2.x, r1.w, r1.x, cb0[66].z
  37: movc r2.y, r1.w, r1.y, cb0[67].z
  38: movc r2.z, r1.w, r1.z, cb0[68].z
  39: dp3_sat r0.y, r0.yzwy, r2.xyzx
  40: add r0.y, -r0.y, l(1.000000)
  41: log r0.y, r0.y
  42: mul r0.y, r0.y, cb2[0].y
  43: exp r0.y, r0.y
  44: mul_sat r0.y, r0.y, cb2[0].z
  45: and r0.y, r0.y, v7.x
  46: add r0.x, -r0.x, r0.y
  47: add r0.x, r0.x, l(1.000000)
  48: mul r0.y, v2.w, l(1.000100)
  49: ge r0.z, r0.y, -r0.y
  50: frc r0.y, |r0.y|
  51: movc r0.y, r0.z, r0.y, -r0.y
  52: mul r0.y, r0.y, l(0.999900)
  53: div r0.zw, cb2[10].zzzw, cb2[5].xxxy
  54: mad r0.yz, cb0[19].xxxx, r0.zzwz, r0.yyyy
  55: add r0.yz, r0.yyzy, v2.xxyx
  56: mad r0.yz, r0.yyzy, cb2[5].xxyx, cb2[5].zzwz
  57: sample_b_indexable(texture2d)(float,float,float,float) r1.xyzw, r0.yzyy, t3.xyzw, s2, cb0[4].x
  58: div r0.yz, cb2[11].xxyx, cb2[7].xxyx
  59: mad r0.yz, r0.yyzy, cb0[19].xxxx, v2.xxyx
  60: mad r0.yz, r0.yyzy, cb2[7].xxyx, cb2[7].zzwz
  61: sample_b_indexable(texture2d)(float,float,float,float) r0.yz, r0.yzyy, t4.zxyw, s3, cb0[4].x
  62: mad r2.xy, v2.xyxx, cb2[9].xyxx, cb2[9].zwzz
  63: sample_b_indexable(texture2d)(float,float,float,float) r2.xyz, r2.xyxx, t5.xyzw, s4, cb0[4].x
  64: mul r0.yz, r0.yyzy, r2.xxyx
  65: mul r0.yz, r0.yyzy, cb2[11].zzzz
  66: div r0.yz, r0.yyzy, cb2[3].xxyx
  67: div r3.xy, cb2[10].xyxx, cb2[3].xyxx
  68: mad r3.xy, r3.xyxx, cb0[19].xxxx, v2.xyxx
  69: add r0.yz, -r0.yyzy, r3.xxyx
  70: mad r0.yz, r0.yyzy, cb2[3].xxyx, cb2[3].zzwz
  71: sample_b_indexable(texture2d)(float,float,float,float) r3.xyzw, r0.yzyy, t2.xyzw, s1, cb0[4].x
  72: mul r1.xyzw, r1.wxyz, r3.wxyz
  73: ne r0.yz, l(0.000000, 0.000000, 0.000000, 0.000000), cb2[1].xxyx
  74: movc r3.xyz, r0.yyyy, r1.xxxx, r1.yzwy
  75: max r0.xyw, r0.xxxx, r3.xyxz
  76: movc r0.xyz, r0.zzzz, r0.xywx, r1.yzwy
  77: add r0.w, -v2.z, l(1.000000)
  78: add_sat r1.xyz, -r0.wwww, r2.xyzx
  79: mul_sat r1.xyz, r1.xyzx, r2.xyzx
  80: mul r1.xyz, r0.xyzx, r1.xyzx
  81: ne r0.w, l(0.000000, 0.000000, 0.000000, 0.000000), cb2[14].y
  82: movc r0.xyz, r0.wwww, r1.xyzx, r0.xyzx
  83: mul r0.xyz, r0.xyzx, cb2[12].xxxx
  84: mul r0.xyz, r0.xyzx, v4.xyzx
  85: max r0.w, v3.x, l(1.000000)
  86: min r0.w, r0.w, l(1000000.000000)
  87: eq r1.x, v2.x, v3.x
  88: movc r0.w, r1.x, l(1.000000), r0.w
  89: mul r0.xyz, r0.xyzx, r0.wwww
  90: ftoi r1.xy, v0.xyxx
  91: mov r1.zw, l(0,0,0,0)
  92: ld_indexable(texture2d)(float,float,float,float) r1.xyzw, r1.xyzw, t0.xyzw
  93: mad o0.xyz, r0.xyzx, r1.wwww, r1.xyzx
  94: mov o0.w, l(0)
  95: mov o1.xyzw, l(0,0,0,1.000000)
  96: mov o2.w, l(0)
  97: mov o3.xyzw, l(0,0,0,1.000000)
  98: and o4.x, cb0[11].x, cb1[10].x
  99: ret 
// Approximately 0 instruction slots used


//////////////////////////////////////////////////////
Keywords: _DBUFFER_MRT3 _SURFACE_TYPE_TRANSPARENT _WRITE_RENDERING_LAYERS
-- Vertex shader for "d3d11":
// No shader variant for this keyword set. The closest match will be used instead.

-- Hardware tier variant: Tier 1
-- Fragment shader for "d3d11":
// Stats: 74 math, 4 temp registers
Set 2D Texture "_CameraDepthTexture" to slot 0 sampler slot -1
Set 2D Texture "_MainTex" to slot 1
Set 2D Texture "_Noise" to slot 2
Set 2D Texture "_Flow" to slot 3
Set 2D Texture "_Mask" to slot 4

Set Sampler PointClamp to slot 0

Constant Buffer "$Globals" (2096 bytes) on slot 0 {
  Matrix4x4 unity_MatrixV at 1056
  Matrix4x4 unity_MatrixVP at 1248
  Vector4 _ScaledScreenParams at 48
  Vector2 _GlobalMipBias at 64
  ScalarInt _RenderingLayerMaxInt at 176
  Vector4 _TimeParameters at 304
  Vector3 _WorldSpaceCameraPos at 336
  Vector4 _ProjectionParams at 352
  Vector4 _ZBufferParams at 384
  Vector4 unity_OrthoParams at 400
  Vector4 _RTHandleScale at 448
  Vector4 _CameraDepthTexture_TexelSize at 2080
}
Constant Buffer "UnityPerDraw" (720 bytes) on slot 1 {
  Vector4 unity_RenderingLayer at 160
}
Constant Buffer "UnityPerMaterial" (304 bytes) on slot 2 {
  Float _Fresnelpower at 4
  Float _Fresnelscale at 8
  Float _Useonlycolor at 16
  Float _Use_fresnel at 20
  Vector4 _MainTex_ST at 48
  Vector4 _Noise_ST at 80
  Vector4 _Flow_ST at 112
  Vector4 _Mask_ST at 144
  Vector4 _SpeedMainTexUVNoiseZW at 160
  Vector4 _DistortionSpeedXYPowerZ at 176
  Float _Emission at 192
  Float _Usecenterglow at 228
  Float _Depthpower at 236
}

Shader Disassembly:
//
// Generated by Microsoft (R) D3D Shader Disassembler
//
//
// Input signature:
//
// Name                 Index   Mask Register SysValue  Format   Used
// -------------------- ----- ------ -------- -------- ------- ------
// SV_POSITION              0   xyzw        0      POS   float   xy  
// INTERP                   0   xyz         1     NONE   float       
// INTERP                   2   xyzw        2     NONE   float   xyzw
// INTERP                   3   xyzw        3     NONE   float   x   
// INTERP                   4   xyzw        4     NONE   float   xyz 
// INTERP                   5   xyz         5     NONE   float   xyz 
// INTERP                   6   xyz         6     NONE   float   xyz 
// SV_IsFrontFace           0   x           7    FFACE    uint   x   
//
//
// Output signature:
//
// Name                 Index   Mask Register SysValue  Format   Used
// -------------------- ----- ------ -------- -------- ------- ------
// SV_Target                0   xyzw        0   TARGET   float   xyzw
// SV_Target                1   xyzw        1   TARGET   float   xyzw
// SV_Target                2   xyzw        2   TARGET   float   xyzw
// SV_Target                3   xyzw        3   TARGET   float   xyzw
// SV_Target                4   x           4   TARGET    uint   x   
//
      ps_5_0
      dcl_globalFlags refactoringAllowed
      dcl_constantbuffer CB0[131], immediateIndexed
      dcl_constantbuffer CB1[11], immediateIndexed
      dcl_constantbuffer CB2[15], immediateIndexed
      dcl_sampler s0, mode_default
      dcl_sampler s1, mode_default
      dcl_sampler s2, mode_default
      dcl_sampler s3, mode_default
      dcl_sampler s4, mode_default
      dcl_resource_texture2d (float,float,float,float) t0
      dcl_resource_texture2d (float,float,float,float) t1
      dcl_resource_texture2d (float,float,float,float) t2
      dcl_resource_texture2d (float,float,float,float) t3
      dcl_resource_texture2d (float,float,float,float) t4
      dcl_input_ps_siv linear noperspective v0.xy, position
      dcl_input_ps linear v2.xyzw
      dcl_input_ps linear v3.x
      dcl_input_ps linear v4.xyz
      dcl_input_ps linear v5.xyz
      dcl_input_ps linear v6.xyz
      dcl_input_ps_sgv constant v7.x, is_front_face
      dcl_output o0.xyzw
      dcl_output o1.xyzw
      dcl_output o2.xyzw
      dcl_output o3.xyzw
      dcl_output o4.x
      dcl_temps 4
   0: lt r0.x, cb0[22].x, l(0.000000)
   1: add r0.y, -v0.y, cb0[3].y
   2: movc r0.y, r0.x, r0.y, v0.y
   3: mov r0.x, v0.x
   4: div r0.xy, r0.xyxx, cb0[3].xyxx
   5: add r0.z, -r0.y, l(1.000000)
   6: mad r0.yw, -cb0[130].xxxy, l(0.000000, 0.500000, 0.000000, 0.500000), l(0.000000, 1.000000, 0.000000, 1.000000)
   7: min r0.xy, r0.ywyy, r0.xzxx
   8: mul r0.xy, r0.xyxx, cb0[28].xyxx
   9: sample_b_indexable(texture2d)(float,float,float,float) r0.x, r0.xyxx, t0.xyzw, s0, cb0[4].x
  10: mad r0.x, cb0[24].x, r0.x, cb0[24].y
  11: div r0.x, l(1.000000, 1.000000, 1.000000, 1.000000), r0.x
  12: mul r0.y, v5.y, cb0[79].w
  13: mad r0.y, cb0[78].w, v5.x, r0.y
  14: mad r0.y, cb0[80].w, v5.z, r0.y
  15: add r0.y, r0.y, cb0[81].w
  16: mad r0.x, r0.x, cb0[22].z, -r0.y
  17: div_sat r0.x, r0.x, cb2[14].w
  18: ge r0.y, l(0.000000), r0.x
  19: and r0.y, r0.y, l(0x3f800000)
  20: add r0.z, -r0.x, l(1.000000)
  21: mad r0.x, r0.y, r0.z, r0.x
  22: dp3 r0.y, v6.xyzx, v6.xyzx
  23: sqrt r0.z, r0.y
  24: rsq r0.y, r0.y
  25: mul o2.xyz, r0.yyyy, v6.xyzx
  26: div r0.y, l(1.000000, 1.000000, 1.000000, 1.000000), r0.z
  27: mul r0.yzw, r0.yyyy, v6.xxyz
  28: dp3 r1.x, r0.yzwy, r0.yzwy
  29: rsq r1.x, r1.x
  30: mul r0.yzw, r0.yyzw, r1.xxxx
  31: add r1.xyz, -v5.xyzx, cb0[21].xyzx
  32: dp3 r1.w, r1.xyzx, r1.xyzx
  33: rsq r1.w, r1.w
  34: mul r1.xyz, r1.wwww, r1.xyzx
  35: eq r1.w, cb0[25].w, l(0.000000)
  36: movc r2.x, r1.w, r1.x, cb0[66].z
  37: movc r2.y, r1.w, r1.y, cb0[67].z
  38: movc r2.z, r1.w, r1.z, cb0[68].z
  39: dp3_sat r0.y, r0.yzwy, r2.xyzx
  40: add r0.y, -r0.y, l(1.000000)
  41: log r0.y, r0.y
  42: mul r0.y, r0.y, cb2[0].y
  43: exp r0.y, r0.y
  44: mul_sat r0.y, r0.y, cb2[0].z
  45: and r0.y, r0.y, v7.x
  46: add r0.x, -r0.x, r0.y
  47: add r0.x, r0.x, l(1.000000)
  48: mul r0.y, v2.w, l(1.000100)
  49: ge r0.z, r0.y, -r0.y
  50: frc r0.y, |r0.y|
  51: movc r0.y, r0.z, r0.y, -r0.y
  52: mul r0.y, r0.y, l(0.999900)
  53: div r0.zw, cb2[10].zzzw, cb2[5].xxxy
  54: mad r0.yz, cb0[19].xxxx, r0.zzwz, r0.yyyy
  55: add r0.yz, r0.yyzy, v2.xxyx
  56: mad r0.yz, r0.yyzy, cb2[5].xxyx, cb2[5].zzwz
  57: sample_b_indexable(texture2d)(float,float,float,float) r1.xyzw, r0.yzyy, t2.xyzw, s2, cb0[4].x
  58: div r0.yz, cb2[11].xxyx, cb2[7].xxyx
  59: mad r0.yz, r0.yyzy, cb0[19].xxxx, v2.xxyx
  60: mad r0.yz, r0.yyzy, cb2[7].xxyx, cb2[7].zzwz
  61: sample_b_indexable(texture2d)(float,float,float,float) r0.yz, r0.yzyy, t3.zxyw, s3, cb0[4].x
  62: mad r2.xy, v2.xyxx, cb2[9].xyxx, cb2[9].zwzz
  63: sample_b_indexable(texture2d)(float,float,float,float) r2.xyz, r2.xyxx, t4.xyzw, s4, cb0[4].x
  64: mul r0.yz, r0.yyzy, r2.xxyx
  65: mul r0.yz, r0.yyzy, cb2[11].zzzz
  66: div r0.yz, r0.yyzy, cb2[3].xxyx
  67: div r3.xy, cb2[10].xyxx, cb2[3].xyxx
  68: mad r3.xy, r3.xyxx, cb0[19].xxxx, v2.xyxx
  69: add r0.yz, -r0.yyzy, r3.xxyx
  70: mad r0.yz, r0.yyzy, cb2[3].xxyx, cb2[3].zzwz
  71: sample_b_indexable(texture2d)(float,float,float,float) r3.xyzw, r0.yzyy, t1.xyzw, s1, cb0[4].x
  72: mul r1.xyzw, r1.wxyz, r3.wxyz
  73: ne r0.yz, l(0.000000, 0.000000, 0.000000, 0.000000), cb2[1].xxyx
  74: movc r3.xyz, r0.yyyy, r1.xxxx, r1.yzwy
  75: max r0.xyw, r0.xxxx, r3.xyxz
  76: movc r0.xyz, r0.zzzz, r0.xywx, r1.yzwy
  77: add r0.w, -v2.z, l(1.000000)
  78: add_sat r1.xyz, -r0.wwww, r2.xyzx
  79: mul_sat r1.xyz, r1.xyzx, r2.xyzx
  80: mul r1.xyz, r0.xyzx, r1.xyzx
  81: ne r0.w, l(0.000000, 0.000000, 0.000000, 0.000000), cb2[14].y
  82: movc r0.xyz, r0.wwww, r1.xyzx, r0.xyzx
  83: mul r0.xyz, r0.xyzx, cb2[12].xxxx
  84: mul r0.xyz, r0.xyzx, v4.xyzx
  85: max r0.w, v3.x, l(1.000000)
  86: min r0.w, r0.w, l(1000000.000000)
  87: eq r1.x, v2.x, v3.x
  88: movc r0.w, r1.x, l(1.000000), r0.w
  89: mul o0.xyz, r0.xyzx, r0.wwww
  90: mov o0.w, l(0)
  91: mov o1.xyzw, l(0,0,0,1.000000)
  92: mov o2.w, l(0)
  93: mov o3.xyzw, l(0,0,0,1.000000)
  94: and o4.x, cb0[11].x, cb1[10].x
  95: ret 
// Approximately 0 instruction slots used


//////////////////////////////////////////////////////
Keywords: _DBUFFER_MRT3 _SCREEN_SPACE_OCCLUSION
-- Vertex shader for "d3d11":
// No shader variant for this keyword set. The closest match will be used instead.

-- Hardware tier variant: Tier 1
-- Fragment shader for "d3d11":
// Stats: 82 math, 4 temp registers
Set 2D Texture "_ScreenSpaceOcclusionTexture" to slot 0 sampler slot -1
Set 2D Texture "_DBufferTexture0" to slot 1 sampler slot -1
Set 2D Texture "_CameraDepthTexture" to slot 2 sampler slot -1
Set 2D Texture "_MainTex" to slot 3 sampler slot 2
Set 2D Texture "_Noise" to slot 4 sampler slot 3
Set 2D Texture "_Flow" to slot 5 sampler slot 4
Set 2D Texture "_Mask" to slot 6 sampler slot 5

Set Sampler PointClamp to slot 0
Set Sampler LinearClamp to slot 1

Constant Buffer "$Globals" (2096 bytes) on slot 0 {
  Matrix4x4 unity_MatrixV at 1056
  Matrix4x4 unity_MatrixVP at 1248
  Vector4 _ScaledScreenParams at 48
  Vector2 _GlobalMipBias at 64
  Vector4 _AmbientOcclusionParam at 144
  Vector4 _TimeParameters at 304
  Vector3 _WorldSpaceCameraPos at 336
  Vector4 _ProjectionParams at 352
  Vector4 _ZBufferParams at 384
  Vector4 unity_OrthoParams at 400
  Vector4 _ScaleBiasRt at 432
  Vector4 _RTHandleScale at 448
  Vector4 _CameraDepthTexture_TexelSize at 2080
}
Constant Buffer "UnityPerMaterial" (304 bytes) on slot 1 {
  Float _Fresnelpower at 4
  Float _Fresnelscale at 8
  Float _Useonlycolor at 16
  Float _Use_fresnel at 20
  Vector4 _MainTex_ST at 48
  Vector4 _Noise_ST at 80
  Vector4 _Flow_ST at 112
  Vector4 _Mask_ST at 144
  Vector4 _SpeedMainTexUVNoiseZW at 160
  Vector4 _DistortionSpeedXYPowerZ at 176
  Float _Emission at 192
  Float _Usecenterglow at 228
  Float _Depthpower at 236
}

Shader Disassembly:
//
// Generated by Microsoft (R) D3D Shader Disassembler
//
//
// Input signature:
//
// Name                 Index   Mask Register SysValue  Format   Used
// -------------------- ----- ------ -------- -------- ------- ------
// SV_POSITION              0   xyzw        0      POS   float   xy  
// INTERP                   0   xyz         1     NONE   float       
// INTERP                   2   xyzw        2     NONE   float   xyzw
// INTERP                   3   xyzw        3     NONE   float   x   
// INTERP                   4   xyzw        4     NONE   float   xyz 
// INTERP                   5   xyz         5     NONE   float   xyz 
// INTERP                   6   xyz         6     NONE   float   xyz 
// SV_IsFrontFace           0   x           7    FFACE    uint   x   
//
//
// Output signature:
//
// Name                 Index   Mask Register SysValue  Format   Used
// -------------------- ----- ------ -------- -------- ------- ------
// SV_Target                0   xyzw        0   TARGET   float   xyzw
// SV_Target                1   xyzw        1   TARGET   float   xyzw
// SV_Target                2   xyzw        2   TARGET   float   xyzw
// SV_Target                3   xyzw        3   TARGET   float   xyzw
//
      ps_5_0
      dcl_globalFlags refactoringAllowed
      dcl_constantbuffer CB0[131], immediateIndexed
      dcl_constantbuffer CB1[15], immediateIndexed
      dcl_sampler s0, mode_default
      dcl_sampler s1, mode_default
      dcl_sampler s2, mode_default
      dcl_sampler s3, mode_default
      dcl_sampler s4, mode_default
      dcl_sampler s5, mode_default
      dcl_resource_texture2d (float,float,float,float) t0
      dcl_resource_texture2d (float,float,float,float) t1
      dcl_resource_texture2d (float,float,float,float) t2
      dcl_resource_texture2d (float,float,float,float) t3
      dcl_resource_texture2d (float,float,float,float) t4
      dcl_resource_texture2d (float,float,float,float) t5
      dcl_resource_texture2d (float,float,float,float) t6
      dcl_input_ps_siv linear noperspective v0.xy, position
      dcl_input_ps linear v2.xyzw
      dcl_input_ps linear v3.x
      dcl_input_ps linear v4.xyz
      dcl_input_ps linear v5.xyz
      dcl_input_ps linear v6.xyz
      dcl_input_ps_sgv constant v7.x, is_front_face
      dcl_output o0.xyzw
      dcl_output o1.xyzw
      dcl_output o2.xyzw
      dcl_output o3.xyzw
      dcl_temps 4
   0: lt r0.x, cb0[22].x, l(0.000000)
   1: add r0.y, -v0.y, cb0[3].y
   2: movc r0.y, r0.x, r0.y, v0.y
   3: mov r0.x, v0.x
   4: div r0.xy, r0.xyxx, cb0[3].xyxx
   5: add r0.z, -r0.y, l(1.000000)
   6: mad r0.yw, -cb0[130].xxxy, l(0.000000, 0.500000, 0.000000, 0.500000), l(0.000000, 1.000000, 0.000000, 1.000000)
   7: min r0.xy, r0.ywyy, r0.xzxx
   8: mul r0.xy, r0.xyxx, cb0[28].xyxx
   9: sample_b_indexable(texture2d)(float,float,float,float) r0.x, r0.xyxx, t2.xyzw, s0, cb0[4].x
  10: mad r0.x, cb0[24].x, r0.x, cb0[24].y
  11: div r0.x, l(1.000000, 1.000000, 1.000000, 1.000000), r0.x
  12: mul r0.y, v5.y, cb0[79].w
  13: mad r0.y, cb0[78].w, v5.x, r0.y
  14: mad r0.y, cb0[80].w, v5.z, r0.y
  15: add r0.y, r0.y, cb0[81].w
  16: mad r0.x, r0.x, cb0[22].z, -r0.y
  17: div_sat r0.x, r0.x, cb1[14].w
  18: ge r0.y, l(0.000000), r0.x
  19: and r0.y, r0.y, l(0x3f800000)
  20: add r0.z, -r0.x, l(1.000000)
  21: mad r0.x, r0.y, r0.z, r0.x
  22: dp3 r0.y, v6.xyzx, v6.xyzx
  23: sqrt r0.z, r0.y
  24: rsq r0.y, r0.y
  25: mul o2.xyz, r0.yyyy, v6.xyzx
  26: div r0.y, l(1.000000, 1.000000, 1.000000, 1.000000), r0.z
  27: mul r0.yzw, r0.yyyy, v6.xxyz
  28: dp3 r1.x, r0.yzwy, r0.yzwy
  29: rsq r1.x, r1.x
  30: mul r0.yzw, r0.yyzw, r1.xxxx
  31: add r1.xyz, -v5.xyzx, cb0[21].xyzx
  32: dp3 r1.w, r1.xyzx, r1.xyzx
  33: rsq r1.w, r1.w
  34: mul r1.xyz, r1.wwww, r1.xyzx
  35: eq r1.w, cb0[25].w, l(0.000000)
  36: movc r2.x, r1.w, r1.x, cb0[66].z
  37: movc r2.y, r1.w, r1.y, cb0[67].z
  38: movc r2.z, r1.w, r1.z, cb0[68].z
  39: dp3_sat r0.y, r0.yzwy, r2.xyzx
  40: add r0.y, -r0.y, l(1.000000)
  41: log r0.y, r0.y
  42: mul r0.y, r0.y, cb1[0].y
  43: exp r0.y, r0.y
  44: mul_sat r0.y, r0.y, cb1[0].z
  45: and r0.y, r0.y, v7.x
  46: add r0.x, -r0.x, r0.y
  47: add r0.x, r0.x, l(1.000000)
  48: mul r0.y, v2.w, l(1.000100)
  49: ge r0.z, r0.y, -r0.y
  50: frc r0.y, |r0.y|
  51: movc r0.y, r0.z, r0.y, -r0.y
  52: mul r0.y, r0.y, l(0.999900)
  53: div r0.zw, cb1[10].zzzw, cb1[5].xxxy
  54: mad r0.yz, cb0[19].xxxx, r0.zzwz, r0.yyyy
  55: add r0.yz, r0.yyzy, v2.xxyx
  56: mad r0.yz, r0.yyzy, cb1[5].xxyx, cb1[5].zzwz
  57: sample_b_indexable(texture2d)(float,float,float,float) r1.xyzw, r0.yzyy, t4.xyzw, s3, cb0[4].x
  58: div r0.yz, cb1[11].xxyx, cb1[7].xxyx
  59: mad r0.yz, r0.yyzy, cb0[19].xxxx, v2.xxyx
  60: mad r0.yz, r0.yyzy, cb1[7].xxyx, cb1[7].zzwz
  61: sample_b_indexable(texture2d)(float,float,float,float) r0.yz, r0.yzyy, t5.zxyw, s4, cb0[4].x
  62: mad r2.xy, v2.xyxx, cb1[9].xyxx, cb1[9].zwzz
  63: sample_b_indexable(texture2d)(float,float,float,float) r2.xyz, r2.xyxx, t6.xyzw, s5, cb0[4].x
  64: mul r0.yz, r0.yyzy, r2.xxyx
  65: mul r0.yz, r0.yyzy, cb1[11].zzzz
  66: div r0.yz, r0.yyzy, cb1[3].xxyx
  67: div r3.xy, cb1[10].xyxx, cb1[3].xyxx
  68: mad r3.xy, r3.xyxx, cb0[19].xxxx, v2.xyxx
  69: add r0.yz, -r0.yyzy, r3.xxyx
  70: mad r0.yz, r0.yyzy, cb1[3].xxyx, cb1[3].zzwz
  71: sample_b_indexable(texture2d)(float,float,float,float) r3.xyzw, r0.yzyy, t3.xyzw, s2, cb0[4].x
  72: mul r1.xyzw, r1.wxyz, r3.wxyz
  73: ne r0.yz, l(0.000000, 0.000000, 0.000000, 0.000000), cb1[1].xxyx
  74: movc r3.xyz, r0.yyyy, r1.xxxx, r1.yzwy
  75: max r0.xyw, r0.xxxx, r3.xyxz
  76: movc r0.xyz, r0.zzzz, r0.xywx, r1.yzwy
  77: add r0.w, -v2.z, l(1.000000)
  78: add_sat r1.xyz, -r0.wwww, r2.xyzx
  79: mul_sat r1.xyz, r1.xyzx, r2.xyzx
  80: mul r1.xyz, r0.xyzx, r1.xyzx
  81: ne r0.w, l(0.000000, 0.000000, 0.000000, 0.000000), cb1[14].y
  82: movc r0.xyz, r0.wwww, r1.xyzx, r0.xyzx
  83: mul r0.xyz, r0.xyzx, cb1[12].xxxx
  84: mul r0.xyz, r0.xyzx, v4.xyzx
  85: max r0.w, v3.x, l(1.000000)
  86: min r0.w, r0.w, l(1000000.000000)
  87: eq r1.x, v2.x, v3.x
  88: movc r0.w, r1.x, l(1.000000), r0.w
  89: mul r0.xyz, r0.xyzx, r0.wwww
  90: ftoi r1.xy, v0.xyxx
  91: mov r1.zw, l(0,0,0,0)
  92: ld_indexable(texture2d)(float,float,float,float) r1.xyzw, r1.xyzw, t1.xyzw
  93: mad r0.xyz, r0.xyzx, r1.wwww, r1.xyzx
  94: rcp r1.xy, cb0[3].xyxx
  95: mul r1.xy, r1.xyxx, v0.xyxx
  96: mad r0.w, r1.y, cb0[27].x, cb0[27].y
  97: add r1.z, -r0.w, l(1.000000)
  98: sample_b_indexable(texture2d)(float,float,float,float) r0.w, r1.xzxx, t0.yzwx, s1, cb0[4].x
  99: add r1.x, -cb0[9].x, l(1.000000)
 100: add_sat r0.w, r0.w, r1.x
 101: add r0.w, r0.w, l(-1.000000)
 102: mad r0.w, cb0[9].w, r0.w, l(1.000000)
 103: mul o0.xyz, r0.wwww, r0.xyzx
 104: mov o0.w, l(0)
 105: mov o1.xyzw, l(0,0,0,0)
 106: mov o2.w, l(0)
 107: mov o3.xyzw, l(0,0,0,1.000000)
 108: ret 
// Approximately 0 instruction slots used


//////////////////////////////////////////////////////
Keywords: _DBUFFER_MRT3 _SCREEN_SPACE_OCCLUSION _SURFACE_TYPE_TRANSPARENT
-- Vertex shader for "d3d11":
// No shader variant for this keyword set. The closest match will be used instead.

-- Hardware tier variant: Tier 1
-- Fragment shader for "d3d11":
// Stats: 73 math, 4 temp registers
Set 2D Texture "_CameraDepthTexture" to slot 0 sampler slot -1
Set 2D Texture "_MainTex" to slot 1
Set 2D Texture "_Noise" to slot 2
Set 2D Texture "_Flow" to slot 3
Set 2D Texture "_Mask" to slot 4

Set Sampler PointClamp to slot 0

Constant Buffer "$Globals" (2096 bytes) on slot 0 {
  Matrix4x4 unity_MatrixV at 1056
  Matrix4x4 unity_MatrixVP at 1248
  Vector4 _ScaledScreenParams at 48
  Vector2 _GlobalMipBias at 64
  Vector4 _TimeParameters at 304
  Vector3 _WorldSpaceCameraPos at 336
  Vector4 _ProjectionParams at 352
  Vector4 _ZBufferParams at 384
  Vector4 unity_OrthoParams at 400
  Vector4 _RTHandleScale at 448
  Vector4 _CameraDepthTexture_TexelSize at 2080
}
Constant Buffer "UnityPerMaterial" (304 bytes) on slot 1 {
  Float _Fresnelpower at 4
  Float _Fresnelscale at 8
  Float _Useonlycolor at 16
  Float _Use_fresnel at 20
  Vector4 _MainTex_ST at 48
  Vector4 _Noise_ST at 80
  Vector4 _Flow_ST at 112
  Vector4 _Mask_ST at 144
  Vector4 _SpeedMainTexUVNoiseZW at 160
  Vector4 _DistortionSpeedXYPowerZ at 176
  Float _Emission at 192
  Float _Usecenterglow at 228
  Float _Depthpower at 236
}

Shader Disassembly:
//
// Generated by Microsoft (R) D3D Shader Disassembler
//
//
// Input signature:
//
// Name                 Index   Mask Register SysValue  Format   Used
// -------------------- ----- ------ -------- -------- ------- ------
// SV_POSITION              0   xyzw        0      POS   float   xy  
// INTERP                   0   xyz         1     NONE   float       
// INTERP                   2   xyzw        2     NONE   float   xyzw
// INTERP                   3   xyzw        3     NONE   float   x   
// INTERP                   4   xyzw        4     NONE   float   xyz 
// INTERP                   5   xyz         5     NONE   float   xyz 
// INTERP                   6   xyz         6     NONE   float   xyz 
// SV_IsFrontFace           0   x           7    FFACE    uint   x   
//
//
// Output signature:
//
// Name                 Index   Mask Register SysValue  Format   Used
// -------------------- ----- ------ -------- -------- ------- ------
// SV_Target                0   xyzw        0   TARGET   float   xyzw
// SV_Target                1   xyzw        1   TARGET   float   xyzw
// SV_Target                2   xyzw        2   TARGET   float   xyzw
// SV_Target                3   xyzw        3   TARGET   float   xyzw
//
      ps_5_0
      dcl_globalFlags refactoringAllowed
      dcl_constantbuffer CB0[131], immediateIndexed
      dcl_constantbuffer CB1[15], immediateIndexed
      dcl_sampler s0, mode_default
      dcl_sampler s1, mode_default
      dcl_sampler s2, mode_default
      dcl_sampler s3, mode_default
      dcl_sampler s4, mode_default
      dcl_resource_texture2d (float,float,float,float) t0
      dcl_resource_texture2d (float,float,float,float) t1
      dcl_resource_texture2d (float,float,float,float) t2
      dcl_resource_texture2d (float,float,float,float) t3
      dcl_resource_texture2d (float,float,float,float) t4
      dcl_input_ps_siv linear noperspective v0.xy, position
      dcl_input_ps linear v2.xyzw
      dcl_input_ps linear v3.x
      dcl_input_ps linear v4.xyz
      dcl_input_ps linear v5.xyz
      dcl_input_ps linear v6.xyz
      dcl_input_ps_sgv constant v7.x, is_front_face
      dcl_output o0.xyzw
      dcl_output o1.xyzw
      dcl_output o2.xyzw
      dcl_output o3.xyzw
      dcl_temps 4
   0: lt r0.x, cb0[22].x, l(0.000000)
   1: add r0.y, -v0.y, cb0[3].y
   2: movc r0.y, r0.x, r0.y, v0.y
   3: mov r0.x, v0.x
   4: div r0.xy, r0.xyxx, cb0[3].xyxx
   5: add r0.z, -r0.y, l(1.000000)
   6: mad r0.yw, -cb0[130].xxxy, l(0.000000, 0.500000, 0.000000, 0.500000), l(0.000000, 1.000000, 0.000000, 1.000000)
   7: min r0.xy, r0.ywyy, r0.xzxx
   8: mul r0.xy, r0.xyxx, cb0[28].xyxx
   9: sample_b_indexable(texture2d)(float,float,float,float) r0.x, r0.xyxx, t0.xyzw, s0, cb0[4].x
  10: mad r0.x, cb0[24].x, r0.x, cb0[24].y
  11: div r0.x, l(1.000000, 1.000000, 1.000000, 1.000000), r0.x
  12: mul r0.y, v5.y, cb0[79].w
  13: mad r0.y, cb0[78].w, v5.x, r0.y
  14: mad r0.y, cb0[80].w, v5.z, r0.y
  15: add r0.y, r0.y, cb0[81].w
  16: mad r0.x, r0.x, cb0[22].z, -r0.y
  17: div_sat r0.x, r0.x, cb1[14].w
  18: ge r0.y, l(0.000000), r0.x
  19: and r0.y, r0.y, l(0x3f800000)
  20: add r0.z, -r0.x, l(1.000000)
  21: mad r0.x, r0.y, r0.z, r0.x
  22: dp3 r0.y, v6.xyzx, v6.xyzx
  23: sqrt r0.z, r0.y
  24: rsq r0.y, r0.y
  25: mul o2.xyz, r0.yyyy, v6.xyzx
  26: div r0.y, l(1.000000, 1.000000, 1.000000, 1.000000), r0.z
  27: mul r0.yzw, r0.yyyy, v6.xxyz
  28: dp3 r1.x, r0.yzwy, r0.yzwy
  29: rsq r1.x, r1.x
  30: mul r0.yzw, r0.yyzw, r1.xxxx
  31: add r1.xyz, -v5.xyzx, cb0[21].xyzx
  32: dp3 r1.w, r1.xyzx, r1.xyzx
  33: rsq r1.w, r1.w
  34: mul r1.xyz, r1.wwww, r1.xyzx
  35: eq r1.w, cb0[25].w, l(0.000000)
  36: movc r2.x, r1.w, r1.x, cb0[66].z
  37: movc r2.y, r1.w, r1.y, cb0[67].z
  38: movc r2.z, r1.w, r1.z, cb0[68].z
  39: dp3_sat r0.y, r0.yzwy, r2.xyzx
  40: add r0.y, -r0.y, l(1.000000)
  41: log r0.y, r0.y
  42: mul r0.y, r0.y, cb1[0].y
  43: exp r0.y, r0.y
  44: mul_sat r0.y, r0.y, cb1[0].z
  45: and r0.y, r0.y, v7.x
  46: add r0.x, -r0.x, r0.y
  47: add r0.x, r0.x, l(1.000000)
  48: mul r0.y, v2.w, l(1.000100)
  49: ge r0.z, r0.y, -r0.y
  50: frc r0.y, |r0.y|
  51: movc r0.y, r0.z, r0.y, -r0.y
  52: mul r0.y, r0.y, l(0.999900)
  53: div r0.zw, cb1[10].zzzw, cb1[5].xxxy
  54: mad r0.yz, cb0[19].xxxx, r0.zzwz, r0.yyyy
  55: add r0.yz, r0.yyzy, v2.xxyx
  56: mad r0.yz, r0.yyzy, cb1[5].xxyx, cb1[5].zzwz
  57: sample_b_indexable(texture2d)(float,float,float,float) r1.xyzw, r0.yzyy, t2.xyzw, s2, cb0[4].x
  58: div r0.yz, cb1[11].xxyx, cb1[7].xxyx
  59: mad r0.yz, r0.yyzy, cb0[19].xxxx, v2.xxyx
  60: mad r0.yz, r0.yyzy, cb1[7].xxyx, cb1[7].zzwz
  61: sample_b_indexable(texture2d)(float,float,float,float) r0.yz, r0.yzyy, t3.zxyw, s3, cb0[4].x
  62: mad r2.xy, v2.xyxx, cb1[9].xyxx, cb1[9].zwzz
  63: sample_b_indexable(texture2d)(float,float,float,float) r2.xyz, r2.xyxx, t4.xyzw, s4, cb0[4].x
  64: mul r0.yz, r0.yyzy, r2.xxyx
  65: mul r0.yz, r0.yyzy, cb1[11].zzzz
  66: div r0.yz, r0.yyzy, cb1[3].xxyx
  67: div r3.xy, cb1[10].xyxx, cb1[3].xyxx
  68: mad r3.xy, r3.xyxx, cb0[19].xxxx, v2.xyxx
  69: add r0.yz, -r0.yyzy, r3.xxyx
  70: mad r0.yz, r0.yyzy, cb1[3].xxyx, cb1[3].zzwz
  71: sample_b_indexable(texture2d)(float,float,float,float) r3.xyzw, r0.yzyy, t1.xyzw, s1, cb0[4].x
  72: mul r1.xyzw, r1.wxyz, r3.wxyz
  73: ne r0.yz, l(0.000000, 0.000000, 0.000000, 0.000000), cb1[1].xxyx
  74: movc r3.xyz, r0.yyyy, r1.xxxx, r1.yzwy
  75: max r0.xyw, r0.xxxx, r3.xyxz
  76: movc r0.xyz, r0.zzzz, r0.xywx, r1.yzwy
  77: add r0.w, -v2.z, l(1.000000)
  78: add_sat r1.xyz, -r0.wwww, r2.xyzx
  79: mul_sat r1.xyz, r1.xyzx, r2.xyzx
  80: mul r1.xyz, r0.xyzx, r1.xyzx
  81: ne r0.w, l(0.000000, 0.000000, 0.000000, 0.000000), cb1[14].y
  82: movc r0.xyz, r0.wwww, r1.xyzx, r0.xyzx
  83: mul r0.xyz, r0.xyzx, cb1[12].xxxx
  84: mul r0.xyz, r0.xyzx, v4.xyzx
  85: max r0.w, v3.x, l(1.000000)
  86: min r0.w, r0.w, l(1000000.000000)
  87: eq r1.x, v2.x, v3.x
  88: movc r0.w, r1.x, l(1.000000), r0.w
  89: mul o0.xyz, r0.xyzx, r0.wwww
  90: mov o0.w, l(0)
  91: mov o1.xyzw, l(0,0,0,1.000000)
  92: mov o2.w, l(0)
  93: mov o3.xyzw, l(0,0,0,1.000000)
  94: ret 
// Approximately 0 instruction slots used


//////////////////////////////////////////////////////
Keywords: _DBUFFER_MRT3 _SCREEN_SPACE_OCCLUSION _WRITE_RENDERING_LAYERS
-- Vertex shader for "d3d11":
// No shader variant for this keyword set. The closest match will be used instead.

-- Hardware tier variant: Tier 1
-- Fragment shader for "d3d11":
// Stats: 83 math, 4 temp registers
Set 2D Texture "_ScreenSpaceOcclusionTexture" to slot 0 sampler slot -1
Set 2D Texture "_DBufferTexture0" to slot 1 sampler slot -1
Set 2D Texture "_CameraDepthTexture" to slot 2 sampler slot -1
Set 2D Texture "_MainTex" to slot 3 sampler slot 2
Set 2D Texture "_Noise" to slot 4 sampler slot 3
Set 2D Texture "_Flow" to slot 5 sampler slot 4
Set 2D Texture "_Mask" to slot 6 sampler slot 5

Set Sampler PointClamp to slot 0
Set Sampler LinearClamp to slot 1

Constant Buffer "$Globals" (2096 bytes) on slot 0 {
  Matrix4x4 unity_MatrixV at 1056
  Matrix4x4 unity_MatrixVP at 1248
  Vector4 _ScaledScreenParams at 48
  Vector2 _GlobalMipBias at 64
  Vector4 _AmbientOcclusionParam at 144
  ScalarInt _RenderingLayerMaxInt at 176
  Vector4 _TimeParameters at 304
  Vector3 _WorldSpaceCameraPos at 336
  Vector4 _ProjectionParams at 352
  Vector4 _ZBufferParams at 384
  Vector4 unity_OrthoParams at 400
  Vector4 _ScaleBiasRt at 432
  Vector4 _RTHandleScale at 448
  Vector4 _CameraDepthTexture_TexelSize at 2080
}
Constant Buffer "UnityPerDraw" (720 bytes) on slot 1 {
  Vector4 unity_RenderingLayer at 160
}
Constant Buffer "UnityPerMaterial" (304 bytes) on slot 2 {
  Float _Fresnelpower at 4
  Float _Fresnelscale at 8
  Float _Useonlycolor at 16
  Float _Use_fresnel at 20
  Vector4 _MainTex_ST at 48
  Vector4 _Noise_ST at 80
  Vector4 _Flow_ST at 112
  Vector4 _Mask_ST at 144
  Vector4 _SpeedMainTexUVNoiseZW at 160
  Vector4 _DistortionSpeedXYPowerZ at 176
  Float _Emission at 192
  Float _Usecenterglow at 228
  Float _Depthpower at 236
}

Shader Disassembly:
//
// Generated by Microsoft (R) D3D Shader Disassembler
//
//
// Input signature:
//
// Name                 Index   Mask Register SysValue  Format   Used
// -------------------- ----- ------ -------- -------- ------- ------
// SV_POSITION              0   xyzw        0      POS   float   xy  
// INTERP                   0   xyz         1     NONE   float       
// INTERP                   2   xyzw        2     NONE   float   xyzw
// INTERP                   3   xyzw        3     NONE   float   x   
// INTERP                   4   xyzw        4     NONE   float   xyz 
// INTERP                   5   xyz         5     NONE   float   xyz 
// INTERP                   6   xyz         6     NONE   float   xyz 
// SV_IsFrontFace           0   x           7    FFACE    uint   x   
//
//
// Output signature:
//
// Name                 Index   Mask Register SysValue  Format   Used
// -------------------- ----- ------ -------- -------- ------- ------
// SV_Target                0   xyzw        0   TARGET   float   xyzw
// SV_Target                1   xyzw        1   TARGET   float   xyzw
// SV_Target                2   xyzw        2   TARGET   float   xyzw
// SV_Target                3   xyzw        3   TARGET   float   xyzw
// SV_Target                4   x           4   TARGET    uint   x   
//
      ps_5_0
      dcl_globalFlags refactoringAllowed
      dcl_constantbuffer CB0[131], immediateIndexed
      dcl_constantbuffer CB1[11], immediateIndexed
      dcl_constantbuffer CB2[15], immediateIndexed
      dcl_sampler s0, mode_default
      dcl_sampler s1, mode_default
      dcl_sampler s2, mode_default
      dcl_sampler s3, mode_default
      dcl_sampler s4, mode_default
      dcl_sampler s5, mode_default
      dcl_resource_texture2d (float,float,float,float) t0
      dcl_resource_texture2d (float,float,float,float) t1
      dcl_resource_texture2d (float,float,float,float) t2
      dcl_resource_texture2d (float,float,float,float) t3
      dcl_resource_texture2d (float,float,float,float) t4
      dcl_resource_texture2d (float,float,float,float) t5
      dcl_resource_texture2d (float,float,float,float) t6
      dcl_input_ps_siv linear noperspective v0.xy, position
      dcl_input_ps linear v2.xyzw
      dcl_input_ps linear v3.x
      dcl_input_ps linear v4.xyz
      dcl_input_ps linear v5.xyz
      dcl_input_ps linear v6.xyz
      dcl_input_ps_sgv constant v7.x, is_front_face
      dcl_output o0.xyzw
      dcl_output o1.xyzw
      dcl_output o2.xyzw
      dcl_output o3.xyzw
      dcl_output o4.x
      dcl_temps 4
   0: lt r0.x, cb0[22].x, l(0.000000)
   1: add r0.y, -v0.y, cb0[3].y
   2: movc r0.y, r0.x, r0.y, v0.y
   3: mov r0.x, v0.x
   4: div r0.xy, r0.xyxx, cb0[3].xyxx
   5: add r0.z, -r0.y, l(1.000000)
   6: mad r0.yw, -cb0[130].xxxy, l(0.000000, 0.500000, 0.000000, 0.500000), l(0.000000, 1.000000, 0.000000, 1.000000)
   7: min r0.xy, r0.ywyy, r0.xzxx
   8: mul r0.xy, r0.xyxx, cb0[28].xyxx
   9: sample_b_indexable(texture2d)(float,float,float,float) r0.x, r0.xyxx, t2.xyzw, s0, cb0[4].x
  10: mad r0.x, cb0[24].x, r0.x, cb0[24].y
  11: div r0.x, l(1.000000, 1.000000, 1.000000, 1.000000), r0.x
  12: mul r0.y, v5.y, cb0[79].w
  13: mad r0.y, cb0[78].w, v5.x, r0.y
  14: mad r0.y, cb0[80].w, v5.z, r0.y
  15: add r0.y, r0.y, cb0[81].w
  16: mad r0.x, r0.x, cb0[22].z, -r0.y
  17: div_sat r0.x, r0.x, cb2[14].w
  18: ge r0.y, l(0.000000), r0.x
  19: and r0.y, r0.y, l(0x3f800000)
  20: add r0.z, -r0.x, l(1.000000)
  21: mad r0.x, r0.y, r0.z, r0.x
  22: dp3 r0.y, v6.xyzx, v6.xyzx
  23: sqrt r0.z, r0.y
  24: rsq r0.y, r0.y
  25: mul o2.xyz, r0.yyyy, v6.xyzx
  26: div r0.y, l(1.000000, 1.000000, 1.000000, 1.000000), r0.z
  27: mul r0.yzw, r0.yyyy, v6.xxyz
  28: dp3 r1.x, r0.yzwy, r0.yzwy
  29: rsq r1.x, r1.x
  30: mul r0.yzw, r0.yyzw, r1.xxxx
  31: add r1.xyz, -v5.xyzx, cb0[21].xyzx
  32: dp3 r1.w, r1.xyzx, r1.xyzx
  33: rsq r1.w, r1.w
  34: mul r1.xyz, r1.wwww, r1.xyzx
  35: eq r1.w, cb0[25].w, l(0.000000)
  36: movc r2.x, r1.w, r1.x, cb0[66].z
  37: movc r2.y, r1.w, r1.y, cb0[67].z
  38: movc r2.z, r1.w, r1.z, cb0[68].z
  39: dp3_sat r0.y, r0.yzwy, r2.xyzx
  40: add r0.y, -r0.y, l(1.000000)
  41: log r0.y, r0.y
  42: mul r0.y, r0.y, cb2[0].y
  43: exp r0.y, r0.y
  44: mul_sat r0.y, r0.y, cb2[0].z
  45: and r0.y, r0.y, v7.x
  46: add r0.x, -r0.x, r0.y
  47: add r0.x, r0.x, l(1.000000)
  48: mul r0.y, v2.w, l(1.000100)
  49: ge r0.z, r0.y, -r0.y
  50: frc r0.y, |r0.y|
  51: movc r0.y, r0.z, r0.y, -r0.y
  52: mul r0.y, r0.y, l(0.999900)
  53: div r0.zw, cb2[10].zzzw, cb2[5].xxxy
  54: mad r0.yz, cb0[19].xxxx, r0.zzwz, r0.yyyy
  55: add r0.yz, r0.yyzy, v2.xxyx
  56: mad r0.yz, r0.yyzy, cb2[5].xxyx, cb2[5].zzwz
  57: sample_b_indexable(texture2d)(float,float,float,float) r1.xyzw, r0.yzyy, t4.xyzw, s3, cb0[4].x
  58: div r0.yz, cb2[11].xxyx, cb2[7].xxyx
  59: mad r0.yz, r0.yyzy, cb0[19].xxxx, v2.xxyx
  60: mad r0.yz, r0.yyzy, cb2[7].xxyx, cb2[7].zzwz
  61: sample_b_indexable(texture2d)(float,float,float,float) r0.yz, r0.yzyy, t5.zxyw, s4, cb0[4].x
  62: mad r2.xy, v2.xyxx, cb2[9].xyxx, cb2[9].zwzz
  63: sample_b_indexable(texture2d)(float,float,float,float) r2.xyz, r2.xyxx, t6.xyzw, s5, cb0[4].x
  64: mul r0.yz, r0.yyzy, r2.xxyx
  65: mul r0.yz, r0.yyzy, cb2[11].zzzz
  66: div r0.yz, r0.yyzy, cb2[3].xxyx
  67: div r3.xy, cb2[10].xyxx, cb2[3].xyxx
  68: mad r3.xy, r3.xyxx, cb0[19].xxxx, v2.xyxx
  69: add r0.yz, -r0.yyzy, r3.xxyx
  70: mad r0.yz, r0.yyzy, cb2[3].xxyx, cb2[3].zzwz
  71: sample_b_indexable(texture2d)(float,float,float,float) r3.xyzw, r0.yzyy, t3.xyzw, s2, cb0[4].x
  72: mul r1.xyzw, r1.wxyz, r3.wxyz
  73: ne r0.yz, l(0.000000, 0.000000, 0.000000, 0.000000), cb2[1].xxyx
  74: movc r3.xyz, r0.yyyy, r1.xxxx, r1.yzwy
  75: max r0.xyw, r0.xxxx, r3.xyxz
  76: movc r0.xyz, r0.zzzz, r0.xywx, r1.yzwy
  77: add r0.w, -v2.z, l(1.000000)
  78: add_sat r1.xyz, -r0.wwww, r2.xyzx
  79: mul_sat r1.xyz, r1.xyzx, r2.xyzx
  80: mul r1.xyz, r0.xyzx, r1.xyzx
  81: ne r0.w, l(0.000000, 0.000000, 0.000000, 0.000000), cb2[14].y
  82: movc r0.xyz, r0.wwww, r1.xyzx, r0.xyzx
  83: mul r0.xyz, r0.xyzx, cb2[12].xxxx
  84: mul r0.xyz, r0.xyzx, v4.xyzx
  85: max r0.w, v3.x, l(1.000000)
  86: min r0.w, r0.w, l(1000000.000000)
  87: eq r1.x, v2.x, v3.x
  88: movc r0.w, r1.x, l(1.000000), r0.w
  89: mul r0.xyz, r0.xyzx, r0.wwww
  90: ftoi r1.xy, v0.xyxx
  91: mov r1.zw, l(0,0,0,0)
  92: ld_indexable(texture2d)(float,float,float,float) r1.xyzw, r1.xyzw, t1.xyzw
  93: mad r0.xyz, r0.xyzx, r1.wwww, r1.xyzx
  94: rcp r1.xy, cb0[3].xyxx
  95: mul r1.xy, r1.xyxx, v0.xyxx
  96: mad r0.w, r1.y, cb0[27].x, cb0[27].y
  97: add r1.z, -r0.w, l(1.000000)
  98: sample_b_indexable(texture2d)(float,float,float,float) r0.w, r1.xzxx, t0.yzwx, s1, cb0[4].x
  99: add r1.x, -cb0[9].x, l(1.000000)
 100: add_sat r0.w, r0.w, r1.x
 101: add r0.w, r0.w, l(-1.000000)
 102: mad r0.w, cb0[9].w, r0.w, l(1.000000)
 103: mul o0.xyz, r0.wwww, r0.xyzx
 104: mov o0.w, l(0)
 105: mov o1.xyzw, l(0,0,0,0)
 106: mov o2.w, l(0)
 107: mov o3.xyzw, l(0,0,0,1.000000)
 108: and o4.x, cb0[11].x, cb1[10].x
 109: ret 
// Approximately 0 instruction slots used


//////////////////////////////////////////////////////
Keywords: _DBUFFER_MRT3 _SCREEN_SPACE_OCCLUSION _SURFACE_TYPE_TRANSPARENT _WRITE_RENDERING_LAYERS
-- Vertex shader for "d3d11":
// No shader variant for this keyword set. The closest match will be used instead.

-- Hardware tier variant: Tier 1
-- Fragment shader for "d3d11":
// Stats: 74 math, 4 temp registers
Set 2D Texture "_CameraDepthTexture" to slot 0 sampler slot -1
Set 2D Texture "_MainTex" to slot 1
Set 2D Texture "_Noise" to slot 2
Set 2D Texture "_Flow" to slot 3
Set 2D Texture "_Mask" to slot 4

Set Sampler PointClamp to slot 0

Constant Buffer "$Globals" (2096 bytes) on slot 0 {
  Matrix4x4 unity_MatrixV at 1056
  Matrix4x4 unity_MatrixVP at 1248
  Vector4 _ScaledScreenParams at 48
  Vector2 _GlobalMipBias at 64
  ScalarInt _RenderingLayerMaxInt at 176
  Vector4 _TimeParameters at 304
  Vector3 _WorldSpaceCameraPos at 336
  Vector4 _ProjectionParams at 352
  Vector4 _ZBufferParams at 384
  Vector4 unity_OrthoParams at 400
  Vector4 _RTHandleScale at 448
  Vector4 _CameraDepthTexture_TexelSize at 2080
}
Constant Buffer "UnityPerDraw" (720 bytes) on slot 1 {
  Vector4 unity_RenderingLayer at 160
}
Constant Buffer "UnityPerMaterial" (304 bytes) on slot 2 {
  Float _Fresnelpower at 4
  Float _Fresnelscale at 8
  Float _Useonlycolor at 16
  Float _Use_fresnel at 20
  Vector4 _MainTex_ST at 48
  Vector4 _Noise_ST at 80
  Vector4 _Flow_ST at 112
  Vector4 _Mask_ST at 144
  Vector4 _SpeedMainTexUVNoiseZW at 160
  Vector4 _DistortionSpeedXYPowerZ at 176
  Float _Emission at 192
  Float _Usecenterglow at 228
  Float _Depthpower at 236
}

Shader Disassembly:
//
// Generated by Microsoft (R) D3D Shader Disassembler
//
//
// Input signature:
//
// Name                 Index   Mask Register SysValue  Format   Used
// -------------------- ----- ------ -------- -------- ------- ------
// SV_POSITION              0   xyzw        0      POS   float   xy  
// INTERP                   0   xyz         1     NONE   float       
// INTERP                   2   xyzw        2     NONE   float   xyzw
// INTERP                   3   xyzw        3     NONE   float   x   
// INTERP                   4   xyzw        4     NONE   float   xyz 
// INTERP                   5   xyz         5     NONE   float   xyz 
// INTERP                   6   xyz         6     NONE   float   xyz 
// SV_IsFrontFace           0   x           7    FFACE    uint   x   
//
//
// Output signature:
//
// Name                 Index   Mask Register SysValue  Format   Used
// -------------------- ----- ------ -------- -------- ------- ------
// SV_Target                0   xyzw        0   TARGET   float   xyzw
// SV_Target                1   xyzw        1   TARGET   float   xyzw
// SV_Target                2   xyzw        2   TARGET   float   xyzw
// SV_Target                3   xyzw        3   TARGET   float   xyzw
// SV_Target                4   x           4   TARGET    uint   x   
//
      ps_5_0
      dcl_globalFlags refactoringAllowed
      dcl_constantbuffer CB0[131], immediateIndexed
      dcl_constantbuffer CB1[11], immediateIndexed
      dcl_constantbuffer CB2[15], immediateIndexed
      dcl_sampler s0, mode_default
      dcl_sampler s1, mode_default
      dcl_sampler s2, mode_default
      dcl_sampler s3, mode_default
      dcl_sampler s4, mode_default
      dcl_resource_texture2d (float,float,float,float) t0
      dcl_resource_texture2d (float,float,float,float) t1
      dcl_resource_texture2d (float,float,float,float) t2
      dcl_resource_texture2d (float,float,float,float) t3
      dcl_resource_texture2d (float,float,float,float) t4
      dcl_input_ps_siv linear noperspective v0.xy, position
      dcl_input_ps linear v2.xyzw
      dcl_input_ps linear v3.x
      dcl_input_ps linear v4.xyz
      dcl_input_ps linear v5.xyz
      dcl_input_ps linear v6.xyz
      dcl_input_ps_sgv constant v7.x, is_front_face
      dcl_output o0.xyzw
      dcl_output o1.xyzw
      dcl_output o2.xyzw
      dcl_output o3.xyzw
      dcl_output o4.x
      dcl_temps 4
   0: lt r0.x, cb0[22].x, l(0.000000)
   1: add r0.y, -v0.y, cb0[3].y
   2: movc r0.y, r0.x, r0.y, v0.y
   3: mov r0.x, v0.x
   4: div r0.xy, r0.xyxx, cb0[3].xyxx
   5: add r0.z, -r0.y, l(1.000000)
   6: mad r0.yw, -cb0[130].xxxy, l(0.000000, 0.500000, 0.000000, 0.500000), l(0.000000, 1.000000, 0.000000, 1.000000)
   7: min r0.xy, r0.ywyy, r0.xzxx
   8: mul r0.xy, r0.xyxx, cb0[28].xyxx
   9: sample_b_indexable(texture2d)(float,float,float,float) r0.x, r0.xyxx, t0.xyzw, s0, cb0[4].x
  10: mad r0.x, cb0[24].x, r0.x, cb0[24].y
  11: div r0.x, l(1.000000, 1.000000, 1.000000, 1.000000), r0.x
  12: mul r0.y, v5.y, cb0[79].w
  13: mad r0.y, cb0[78].w, v5.x, r0.y
  14: mad r0.y, cb0[80].w, v5.z, r0.y
  15: add r0.y, r0.y, cb0[81].w
  16: mad r0.x, r0.x, cb0[22].z, -r0.y
  17: div_sat r0.x, r0.x, cb2[14].w
  18: ge r0.y, l(0.000000), r0.x
  19: and r0.y, r0.y, l(0x3f800000)
  20: add r0.z, -r0.x, l(1.000000)
  21: mad r0.x, r0.y, r0.z, r0.x
  22: dp3 r0.y, v6.xyzx, v6.xyzx
  23: sqrt r0.z, r0.y
  24: rsq r0.y, r0.y
  25: mul o2.xyz, r0.yyyy, v6.xyzx
  26: div r0.y, l(1.000000, 1.000000, 1.000000, 1.000000), r0.z
  27: mul r0.yzw, r0.yyyy, v6.xxyz
  28: dp3 r1.x, r0.yzwy, r0.yzwy
  29: rsq r1.x, r1.x
  30: mul r0.yzw, r0.yyzw, r1.xxxx
  31: add r1.xyz, -v5.xyzx, cb0[21].xyzx
  32: dp3 r1.w, r1.xyzx, r1.xyzx
  33: rsq r1.w, r1.w
  34: mul r1.xyz, r1.wwww, r1.xyzx
  35: eq r1.w, cb0[25].w, l(0.000000)
  36: movc r2.x, r1.w, r1.x, cb0[66].z
  37: movc r2.y, r1.w, r1.y, cb0[67].z
  38: movc r2.z, r1.w, r1.z, cb0[68].z
  39: dp3_sat r0.y, r0.yzwy, r2.xyzx
  40: add r0.y, -r0.y, l(1.000000)
  41: log r0.y, r0.y
  42: mul r0.y, r0.y, cb2[0].y
  43: exp r0.y, r0.y
  44: mul_sat r0.y, r0.y, cb2[0].z
  45: and r0.y, r0.y, v7.x
  46: add r0.x, -r0.x, r0.y
  47: add r0.x, r0.x, l(1.000000)
  48: mul r0.y, v2.w, l(1.000100)
  49: ge r0.z, r0.y, -r0.y
  50: frc r0.y, |r0.y|
  51: movc r0.y, r0.z, r0.y, -r0.y
  52: mul r0.y, r0.y, l(0.999900)
  53: div r0.zw, cb2[10].zzzw, cb2[5].xxxy
  54: mad r0.yz, cb0[19].xxxx, r0.zzwz, r0.yyyy
  55: add r0.yz, r0.yyzy, v2.xxyx
  56: mad r0.yz, r0.yyzy, cb2[5].xxyx, cb2[5].zzwz
  57: sample_b_indexable(texture2d)(float,float,float,float) r1.xyzw, r0.yzyy, t2.xyzw, s2, cb0[4].x
  58: div r0.yz, cb2[11].xxyx, cb2[7].xxyx
  59: mad r0.yz, r0.yyzy, cb0[19].xxxx, v2.xxyx
  60: mad r0.yz, r0.yyzy, cb2[7].xxyx, cb2[7].zzwz
  61: sample_b_indexable(texture2d)(float,float,float,float) r0.yz, r0.yzyy, t3.zxyw, s3, cb0[4].x
  62: mad r2.xy, v2.xyxx, cb2[9].xyxx, cb2[9].zwzz
  63: sample_b_indexable(texture2d)(float,float,float,float) r2.xyz, r2.xyxx, t4.xyzw, s4, cb0[4].x
  64: mul r0.yz, r0.yyzy, r2.xxyx
  65: mul r0.yz, r0.yyzy, cb2[11].zzzz
  66: div r0.yz, r0.yyzy, cb2[3].xxyx
  67: div r3.xy, cb2[10].xyxx, cb2[3].xyxx
  68: mad r3.xy, r3.xyxx, cb0[19].xxxx, v2.xyxx
  69: add r0.yz, -r0.yyzy, r3.xxyx
  70: mad r0.yz, r0.yyzy, cb2[3].xxyx, cb2[3].zzwz
  71: sample_b_indexable(texture2d)(float,float,float,float) r3.xyzw, r0.yzyy, t1.xyzw, s1, cb0[4].x
  72: mul r1.xyzw, r1.wxyz, r3.wxyz
  73: ne r0.yz, l(0.000000, 0.000000, 0.000000, 0.000000), cb2[1].xxyx
  74: movc r3.xyz, r0.yyyy, r1.xxxx, r1.yzwy
  75: max r0.xyw, r0.xxxx, r3.xyxz
  76: movc r0.xyz, r0.zzzz, r0.xywx, r1.yzwy
  77: add r0.w, -v2.z, l(1.000000)
  78: add_sat r1.xyz, -r0.wwww, r2.xyzx
  79: mul_sat r1.xyz, r1.xyzx, r2.xyzx
  80: mul r1.xyz, r0.xyzx, r1.xyzx
  81: ne r0.w, l(0.000000, 0.000000, 0.000000, 0.000000), cb2[14].y
  82: movc r0.xyz, r0.wwww, r1.xyzx, r0.xyzx
  83: mul r0.xyz, r0.xyzx, cb2[12].xxxx
  84: mul r0.xyz, r0.xyzx, v4.xyzx
  85: max r0.w, v3.x, l(1.000000)
  86: min r0.w, r0.w, l(1000000.000000)
  87: eq r1.x, v2.x, v3.x
  88: movc r0.w, r1.x, l(1.000000), r0.w
  89: mul o0.xyz, r0.xyzx, r0.wwww
  90: mov o0.w, l(0)
  91: mov o1.xyzw, l(0,0,0,1.000000)
  92: mov o2.w, l(0)
  93: mov o3.xyzw, l(0,0,0,1.000000)
  94: and o4.x, cb0[11].x, cb1[10].x
  95: ret 
// Approximately 0 instruction slots used


 }
}
SubShader { 
 Tags { "QUEUE"="Transparent" "RenderType"="Transparent" "ShaderGraphShader"="true" "ShaderGraphTargetId"="BuiltInUnlitSubTarget" "BuiltInMaterialType"="Unlit" }


 // Stats for Vertex shader:
 //        d3d11: 15 math
 // Stats for Fragment shader:
 //        d3d11: 70 avg math (68..73), 5 texture
 Pass {
  Name "Pass"
  Tags { "LIGHTMODE"="FORWARDBASE" "QUEUE"="Transparent" "SHADOWSUPPORT"="true" "RenderType"="Transparent" "ShaderGraphShader"="true" "ShaderGraphTargetId"="BuiltInUnlitSubTarget" "BuiltInMaterialType"="Unlit" }
  ZTest [_BUILTIN_ZTest]
  ZWrite [_BUILTIN_ZWrite]
  Cull [_BUILTIN_CullMode]
  Blend [_BUILTIN_SrcBlend] [_BUILTIN_DstBlend]
  //////////////////////////////////
  //                              //
  //      Compiled programs       //
  //                              //
  //////////////////////////////////
//////////////////////////////////////////////////////
Keywords: DIRECTIONAL
-- Hardware tier variant: Tier 1
-- Vertex shader for "d3d11":
// Stats: 15 math, 2 temp registers
Uses vertex data channel "Vertex"
Uses vertex data channel "Normal"
Uses vertex data channel "TexCoord0"
Uses vertex data channel "TexCoord1"
Uses vertex data channel "Color"

Constant Buffer "UnityPerDraw" (176 bytes) on slot 0 {
  Matrix4x4 unity_ObjectToWorld at 0
  Matrix4x4 unity_WorldToObject at 64
}
Constant Buffer "UnityPerFrame" (368 bytes) on slot 1 {
  Matrix4x4 unity_MatrixVP at 272
}

Shader Disassembly:
//
// Generated by Microsoft (R) D3D Shader Disassembler
//
//
// Input signature:
//
// Name                 Index   Mask Register SysValue  Format   Used
// -------------------- ----- ------ -------- -------- ------- ------
// POSITION                 0   xyz         0     NONE   float   xyz 
// NORMAL                   0   xyz         1     NONE   float   xyz 
// TANGENT                  0   xyzw        2     NONE   float       
// TEXCOORD                 0   xyzw        3     NONE   float   xyzw
// TEXCOORD                 1   xyzw        4     NONE   float   xyzw
// COLOR                    0   xyzw        5     NONE   float   xyzw
//
//
// Output signature:
//
// Name                 Index   Mask Register SysValue  Format   Used
// -------------------- ----- ------ -------- -------- ------- ------
// SV_POSITION              0   xyzw        0      POS   float   xyzw
// INTERP                   0   xyzw        1     NONE   float   xyzw
// INTERP                   1   xyzw        2     NONE   float   xyzw
// INTERP                   2   xyzw        3     NONE   float   xyzw
// INTERP                   3   xyz         4     NONE   float   xyz 
// INTERP                   4   xyz         5     NONE   float   xyz 
//
      vs_4_0
      dcl_constantbuffer CB0[7], immediateIndexed
      dcl_constantbuffer CB1[21], immediateIndexed
      dcl_input v0.xyz
      dcl_input v1.xyz
      dcl_input v3.xyzw
      dcl_input v4.xyzw
      dcl_input v5.xyzw
      dcl_output_siv o0.xyzw, position
      dcl_output o1.xyzw
      dcl_output o2.xyzw
      dcl_output o3.xyzw
      dcl_output o4.xyz
      dcl_output o5.xyz
      dcl_temps 2
   0: mul r0.xyz, v0.yyyy, cb0[1].xyzx
   1: mad r0.xyz, cb0[0].xyzx, v0.xxxx, r0.xyzx
   2: mad r0.xyz, cb0[2].xyzx, v0.zzzz, r0.xyzx
   3: add r0.xyz, r0.xyzx, cb0[3].xyzx
   4: mul r1.xyzw, r0.yyyy, cb1[18].xyzw
   5: mad r1.xyzw, cb1[17].xyzw, r0.xxxx, r1.xyzw
   6: mad r1.xyzw, cb1[19].xyzw, r0.zzzz, r1.xyzw
   7: mov o4.xyz, r0.xyzx
   8: add o0.xyzw, r1.xyzw, cb1[20].xyzw
   9: mov o1.xyzw, v3.xyzw
  10: mov o2.xyzw, v4.xyzw
  11: mov o3.xyzw, v5.xyzw
  12: dp3 r0.x, v1.xyzx, cb0[4].xyzx
  13: dp3 r0.y, v1.xyzx, cb0[5].xyzx
  14: dp3 r0.z, v1.xyzx, cb0[6].xyzx
  15: dp3 r0.w, r0.xyzx, r0.xyzx
  16: max r0.w, r0.w, l(0.000000)
  17: rsq r0.w, r0.w
  18: mul o5.xyz, r0.wwww, r0.xyzx
  19: ret 
// Approximately 0 instruction slots used


-- Hardware tier variant: Tier 1
-- Fragment shader for "d3d11":
// Stats: 68 math, 4 temp registers, 5 textures
Set 2D Texture "_CameraDepthTexture" to slot 0
Set 2D Texture "_MainTex" to slot 1
Set 2D Texture "_Noise" to slot 2
Set 2D Texture "_Flow" to slot 3
Set 2D Texture "_Mask" to slot 4

Constant Buffer "UnityPerCamera" (144 bytes) on slot 0 {
  Vector4 _Time at 0
  Vector3 _WorldSpaceCameraPos at 64
  Vector4 _ProjectionParams at 80
  Vector4 _ScreenParams at 96
  Vector4 _ZBufferParams at 112
  Vector4 unity_OrthoParams at 128
}
Constant Buffer "UnityPerFrame" (368 bytes) on slot 1 {
  Matrix4x4 unity_MatrixV at 144
  Matrix4x4 unity_MatrixVP at 272
}
Constant Buffer "UnityPerMaterial" (240 bytes) on slot 2 {
  Float _Fresnelpower at 4
  Float _Fresnelscale at 8
  Float _Useonlycolor at 16
  Float _Use_fresnel at 20
  Vector4 _MainTex_ST at 48
  Vector4 _Noise_ST at 80
  Vector4 _Flow_ST at 112
  Vector4 _Mask_ST at 144
  Vector4 _SpeedMainTexUVNoiseZW at 160
  Vector4 _DistortionSpeedXYPowerZ at 176
  Float _Emission at 192
  Float _Usecenterglow at 228
  Float _Depthpower at 236
}

Shader Disassembly:
//
// Generated by Microsoft (R) D3D Shader Disassembler
//
//
// Input signature:
//
// Name                 Index   Mask Register SysValue  Format   Used
// -------------------- ----- ------ -------- -------- ------- ------
// SV_POSITION              0   xyzw        0      POS   float   xy  
// INTERP                   0   xyzw        1     NONE   float   xyzw
// INTERP                   1   xyzw        2     NONE   float   x   
// INTERP                   2   xyzw        3     NONE   float   xyz 
// INTERP                   3   xyz         4     NONE   float   xyz 
// INTERP                   4   xyz         5     NONE   float   xyz 
// SV_IsFrontFace           0   x           6    FFACE    uint   x   
//
//
// Output signature:
//
// Name                 Index   Mask Register SysValue  Format   Used
// -------------------- ----- ------ -------- -------- ------- ------
// SV_TARGET                0   xyzw        0   TARGET   float   xyzw
//
      ps_4_0
      dcl_constantbuffer CB0[9], immediateIndexed
      dcl_constantbuffer CB1[21], immediateIndexed
      dcl_constantbuffer CB2[15], immediateIndexed
      dcl_sampler s0, mode_default
      dcl_sampler s1, mode_default
      dcl_sampler s2, mode_default
      dcl_sampler s3, mode_default
      dcl_sampler s4, mode_default
      dcl_resource_texture2d (float,float,float,float) t0
      dcl_resource_texture2d (float,float,float,float) t1
      dcl_resource_texture2d (float,float,float,float) t2
      dcl_resource_texture2d (float,float,float,float) t3
      dcl_resource_texture2d (float,float,float,float) t4
      dcl_input_ps_siv linear noperspective v0.xy, position
      dcl_input_ps linear v1.xyzw
      dcl_input_ps linear v2.x
      dcl_input_ps linear v3.xyz
      dcl_input_ps linear v4.xyz
      dcl_input_ps linear v5.xyz
      dcl_input_ps_sgv constant v6.x, is_front_face
      dcl_output o0.xyzw
      dcl_temps 4
   0: dp3 r0.x, v5.xyzx, v5.xyzx
   1: sqrt r0.x, r0.x
   2: div r0.x, l(1.000000, 1.000000, 1.000000, 1.000000), r0.x
   3: mul r0.xyz, r0.xxxx, v5.xyzx
   4: dp3 r0.w, r0.xyzx, r0.xyzx
   5: rsq r0.w, r0.w
   6: mul r0.xyz, r0.wwww, r0.xyzx
   7: add r1.xyz, -v4.xyzx, cb0[4].xyzx
   8: dp3 r0.w, r1.xyzx, r1.xyzx
   9: rsq r0.w, r0.w
  10: mul r1.xyz, r0.wwww, r1.xyzx
  11: eq r0.w, cb0[8].w, l(0.000000)
  12: movc r2.x, r0.w, r1.x, cb1[9].z
  13: movc r2.y, r0.w, r1.y, cb1[10].z
  14: movc r2.z, r0.w, r1.z, cb1[11].z
  15: dp3_sat r0.x, r0.xyzx, r2.xyzx
  16: add r0.x, -r0.x, l(1.000000)
  17: log r0.x, r0.x
  18: mul r0.x, r0.x, cb2[0].y
  19: exp r0.x, r0.x
  20: mul_sat r0.x, r0.x, cb2[0].z
  21: and r0.x, r0.x, v6.x
  22: lt r0.y, cb0[5].x, l(0.000000)
  23: add r0.z, -v0.y, cb0[6].y
  24: movc r1.y, r0.y, r0.z, v0.y
  25: mov r1.x, v0.x
  26: div r1.xy, r1.xyxx, cb0[6].xyxx
  27: add r1.z, -r1.y, l(1.000000)
  28: sample r1.xyzw, r1.xzxx, t0.xyzw, s0
  29: mad r0.y, cb0[7].x, r1.x, cb0[7].y
  30: div r0.y, l(1.000000, 1.000000, 1.000000, 1.000000), r0.y
  31: mul r0.z, v4.y, cb1[18].w
  32: mad r0.z, cb1[17].w, v4.x, r0.z
  33: mad r0.z, cb1[19].w, v4.z, r0.z
  34: add r0.z, r0.z, cb1[20].w
  35: mad r0.y, r0.y, cb0[5].z, -r0.z
  36: div_sat r0.y, r0.y, cb2[14].w
  37: ge r0.z, l(0.000000), r0.y
  38: and r0.z, r0.z, l(0x3f800000)
  39: add r0.w, -r0.y, l(1.000000)
  40: mad r0.y, r0.z, r0.w, r0.y
  41: add r0.x, -r0.y, r0.x
  42: add r0.x, r0.x, l(1.000000)
  43: mul r0.y, v1.w, l(1.000100)
  44: ge r0.z, r0.y, -r0.y
  45: frc r0.y, |r0.y|
  46: movc r0.y, r0.z, r0.y, -r0.y
  47: mul r0.y, r0.y, l(0.999900)
  48: div r0.zw, cb2[10].zzzw, cb2[5].xxxy
  49: mad r0.yz, cb0[0].yyyy, r0.zzwz, r0.yyyy
  50: add r0.yz, r0.yyzy, v1.xxyx
  51: mad r0.yz, r0.yyzy, cb2[5].xxyx, cb2[5].zzwz
  52: sample r1.xyzw, r0.yzyy, t2.xyzw, s2
  53: div r0.yz, cb2[11].xxyx, cb2[7].xxyx
  54: mad r0.yz, r0.yyzy, cb0[0].yyyy, v1.xxyx
  55: mad r0.yz, r0.yyzy, cb2[7].xxyx, cb2[7].zzwz
  56: sample r2.xyzw, r0.yzyy, t3.xyzw, s3
  57: mad r0.yz, v1.xxyx, cb2[9].xxyx, cb2[9].zzwz
  58: sample r3.xyzw, r0.yzyy, t4.xyzw, s4
  59: mul r0.yz, r2.xxyx, r3.xxyx
  60: mul r0.yz, r0.yyzy, cb2[11].zzzz
  61: div r0.yz, r0.yyzy, cb2[3].xxyx
  62: div r2.xy, cb2[10].xyxx, cb2[3].xyxx
  63: mad r2.xy, r2.xyxx, cb0[0].yyyy, v1.xyxx
  64: add r0.yz, -r0.yyzy, r2.xxyx
  65: mad r0.yz, r0.yyzy, cb2[3].xxyx, cb2[3].zzwz
  66: sample r2.xyzw, r0.yzyy, t1.xyzw, s1
  67: mul r1.xyzw, r1.wxyz, r2.wxyz
  68: ne r0.yz, l(0.000000, 0.000000, 0.000000, 0.000000), cb2[1].xxyx
  69: movc r2.xyz, r0.yyyy, r1.xxxx, r1.yzwy
  70: max r0.xyw, r0.xxxx, r2.xyxz
  71: movc r0.xyz, r0.zzzz, r0.xywx, r1.yzwy
  72: add r0.w, -v1.z, l(1.000000)
  73: add_sat r1.xyz, -r0.wwww, r3.xyzx
  74: mul_sat r1.xyz, r1.xyzx, r3.xyzx
  75: mul r1.xyz, r0.xyzx, r1.xyzx
  76: ne r0.w, l(0.000000, 0.000000, 0.000000, 0.000000), cb2[14].y
  77: movc r0.xyz, r0.wwww, r1.xyzx, r0.xyzx
  78: mul r0.xyz, r0.xyzx, cb2[12].xxxx
  79: mul r0.xyz, r0.xyzx, v3.xyzx
  80: max r0.w, v2.x, l(1.000000)
  81: min r0.w, r0.w, l(1000000.000000)
  82: eq r1.x, v1.x, v2.x
  83: movc r0.w, r1.x, l(1.000000), r0.w
  84: mul o0.xyz, r0.xyzx, r0.wwww
  85: mov o0.w, l(1.000000)
  86: ret 
// Approximately 0 instruction slots used


//////////////////////////////////////////////////////
Keywords: DIRECTIONAL LIGHTPROBE_SH
-- Hardware tier variant: Tier 1
-- Vertex shader for "d3d11":
// Stats: 15 math, 2 temp registers
Uses vertex data channel "Vertex"
Uses vertex data channel "Normal"
Uses vertex data channel "TexCoord0"
Uses vertex data channel "TexCoord1"
Uses vertex data channel "Color"

Constant Buffer "UnityPerDraw" (176 bytes) on slot 0 {
  Matrix4x4 unity_ObjectToWorld at 0
  Matrix4x4 unity_WorldToObject at 64
}
Constant Buffer "UnityPerFrame" (368 bytes) on slot 1 {
  Matrix4x4 unity_MatrixVP at 272
}

Shader Disassembly:
//
// Generated by Microsoft (R) D3D Shader Disassembler
//
//
// Input signature:
//
// Name                 Index   Mask Register SysValue  Format   Used
// -------------------- ----- ------ -------- -------- ------- ------
// POSITION                 0   xyz         0     NONE   float   xyz 
// NORMAL                   0   xyz         1     NONE   float   xyz 
// TANGENT                  0   xyzw        2     NONE   float       
// TEXCOORD                 0   xyzw        3     NONE   float   xyzw
// TEXCOORD                 1   xyzw        4     NONE   float   xyzw
// COLOR                    0   xyzw        5     NONE   float   xyzw
//
//
// Output signature:
//
// Name                 Index   Mask Register SysValue  Format   Used
// -------------------- ----- ------ -------- -------- ------- ------
// SV_POSITION              0   xyzw        0      POS   float   xyzw
// INTERP                   0   xyzw        1     NONE   float   xyzw
// INTERP                   1   xyzw        2     NONE   float   xyzw
// INTERP                   2   xyzw        3     NONE   float   xyzw
// INTERP                   3   xyz         4     NONE   float   xyz 
// INTERP                   4   xyz         5     NONE   float   xyz 
//
      vs_4_0
      dcl_constantbuffer CB0[7], immediateIndexed
      dcl_constantbuffer CB1[21], immediateIndexed
      dcl_input v0.xyz
      dcl_input v1.xyz
      dcl_input v3.xyzw
      dcl_input v4.xyzw
      dcl_input v5.xyzw
      dcl_output_siv o0.xyzw, position
      dcl_output o1.xyzw
      dcl_output o2.xyzw
      dcl_output o3.xyzw
      dcl_output o4.xyz
      dcl_output o5.xyz
      dcl_temps 2
   0: mul r0.xyz, v0.yyyy, cb0[1].xyzx
   1: mad r0.xyz, cb0[0].xyzx, v0.xxxx, r0.xyzx
   2: mad r0.xyz, cb0[2].xyzx, v0.zzzz, r0.xyzx
   3: add r0.xyz, r0.xyzx, cb0[3].xyzx
   4: mul r1.xyzw, r0.yyyy, cb1[18].xyzw
   5: mad r1.xyzw, cb1[17].xyzw, r0.xxxx, r1.xyzw
   6: mad r1.xyzw, cb1[19].xyzw, r0.zzzz, r1.xyzw
   7: mov o4.xyz, r0.xyzx
   8: add o0.xyzw, r1.xyzw, cb1[20].xyzw
   9: mov o1.xyzw, v3.xyzw
  10: mov o2.xyzw, v4.xyzw
  11: mov o3.xyzw, v5.xyzw
  12: dp3 r0.x, v1.xyzx, cb0[4].xyzx
  13: dp3 r0.y, v1.xyzx, cb0[5].xyzx
  14: dp3 r0.z, v1.xyzx, cb0[6].xyzx
  15: dp3 r0.w, r0.xyzx, r0.xyzx
  16: max r0.w, r0.w, l(0.000000)
  17: rsq r0.w, r0.w
  18: mul o5.xyz, r0.wwww, r0.xyzx
  19: ret 
// Approximately 0 instruction slots used


-- Fragment shader for "d3d11":
// No shader variant for this keyword set. The closest match will be used instead.

//////////////////////////////////////////////////////
Keywords: DIRECTIONAL SHADOWS_SCREEN
-- Hardware tier variant: Tier 1
-- Vertex shader for "d3d11":
// Stats: 15 math, 2 temp registers
Uses vertex data channel "Vertex"
Uses vertex data channel "Normal"
Uses vertex data channel "TexCoord0"
Uses vertex data channel "TexCoord1"
Uses vertex data channel "Color"

Constant Buffer "UnityPerDraw" (176 bytes) on slot 0 {
  Matrix4x4 unity_ObjectToWorld at 0
  Matrix4x4 unity_WorldToObject at 64
}
Constant Buffer "UnityPerFrame" (368 bytes) on slot 1 {
  Matrix4x4 unity_MatrixVP at 272
}

Shader Disassembly:
//
// Generated by Microsoft (R) D3D Shader Disassembler
//
//
// Input signature:
//
// Name                 Index   Mask Register SysValue  Format   Used
// -------------------- ----- ------ -------- -------- ------- ------
// POSITION                 0   xyz         0     NONE   float   xyz 
// NORMAL                   0   xyz         1     NONE   float   xyz 
// TANGENT                  0   xyzw        2     NONE   float       
// TEXCOORD                 0   xyzw        3     NONE   float   xyzw
// TEXCOORD                 1   xyzw        4     NONE   float   xyzw
// COLOR                    0   xyzw        5     NONE   float   xyzw
//
//
// Output signature:
//
// Name                 Index   Mask Register SysValue  Format   Used
// -------------------- ----- ------ -------- -------- ------- ------
// SV_POSITION              0   xyzw        0      POS   float   xyzw
// INTERP                   0   xyzw        1     NONE   float   xyzw
// INTERP                   1   xyzw        2     NONE   float   xyzw
// INTERP                   2   xyzw        3     NONE   float   xyzw
// INTERP                   3   xyz         4     NONE   float   xyz 
// INTERP                   4   xyz         5     NONE   float   xyz 
//
      vs_4_0
      dcl_constantbuffer CB0[7], immediateIndexed
      dcl_constantbuffer CB1[21], immediateIndexed
      dcl_input v0.xyz
      dcl_input v1.xyz
      dcl_input v3.xyzw
      dcl_input v4.xyzw
      dcl_input v5.xyzw
      dcl_output_siv o0.xyzw, position
      dcl_output o1.xyzw
      dcl_output o2.xyzw
      dcl_output o3.xyzw
      dcl_output o4.xyz
      dcl_output o5.xyz
      dcl_temps 2
   0: mul r0.xyz, v0.yyyy, cb0[1].xyzx
   1: mad r0.xyz, cb0[0].xyzx, v0.xxxx, r0.xyzx
   2: mad r0.xyz, cb0[2].xyzx, v0.zzzz, r0.xyzx
   3: add r0.xyz, r0.xyzx, cb0[3].xyzx
   4: mul r1.xyzw, r0.yyyy, cb1[18].xyzw
   5: mad r1.xyzw, cb1[17].xyzw, r0.xxxx, r1.xyzw
   6: mad r1.xyzw, cb1[19].xyzw, r0.zzzz, r1.xyzw
   7: mov o4.xyz, r0.xyzx
   8: add o0.xyzw, r1.xyzw, cb1[20].xyzw
   9: mov o1.xyzw, v3.xyzw
  10: mov o2.xyzw, v4.xyzw
  11: mov o3.xyzw, v5.xyzw
  12: dp3 r0.x, v1.xyzx, cb0[4].xyzx
  13: dp3 r0.y, v1.xyzx, cb0[5].xyzx
  14: dp3 r0.z, v1.xyzx, cb0[6].xyzx
  15: dp3 r0.w, r0.xyzx, r0.xyzx
  16: max r0.w, r0.w, l(0.000000)
  17: rsq r0.w, r0.w
  18: mul o5.xyz, r0.wwww, r0.xyzx
  19: ret 
// Approximately 0 instruction slots used


-- Fragment shader for "d3d11":
// No shader variant for this keyword set. The closest match will be used instead.

//////////////////////////////////////////////////////
Keywords: DIRECTIONAL LIGHTPROBE_SH SHADOWS_SCREEN
-- Hardware tier variant: Tier 1
-- Vertex shader for "d3d11":
// Stats: 15 math, 2 temp registers
Uses vertex data channel "Vertex"
Uses vertex data channel "Normal"
Uses vertex data channel "TexCoord0"
Uses vertex data channel "TexCoord1"
Uses vertex data channel "Color"

Constant Buffer "UnityPerDraw" (176 bytes) on slot 0 {
  Matrix4x4 unity_ObjectToWorld at 0
  Matrix4x4 unity_WorldToObject at 64
}
Constant Buffer "UnityPerFrame" (368 bytes) on slot 1 {
  Matrix4x4 unity_MatrixVP at 272
}

Shader Disassembly:
//
// Generated by Microsoft (R) D3D Shader Disassembler
//
//
// Input signature:
//
// Name                 Index   Mask Register SysValue  Format   Used
// -------------------- ----- ------ -------- -------- ------- ------
// POSITION                 0   xyz         0     NONE   float   xyz 
// NORMAL                   0   xyz         1     NONE   float   xyz 
// TANGENT                  0   xyzw        2     NONE   float       
// TEXCOORD                 0   xyzw        3     NONE   float   xyzw
// TEXCOORD                 1   xyzw        4     NONE   float   xyzw
// COLOR                    0   xyzw        5     NONE   float   xyzw
//
//
// Output signature:
//
// Name                 Index   Mask Register SysValue  Format   Used
// -------------------- ----- ------ -------- -------- ------- ------
// SV_POSITION              0   xyzw        0      POS   float   xyzw
// INTERP                   0   xyzw        1     NONE   float   xyzw
// INTERP                   1   xyzw        2     NONE   float   xyzw
// INTERP                   2   xyzw        3     NONE   float   xyzw
// INTERP                   3   xyz         4     NONE   float   xyz 
// INTERP                   4   xyz         5     NONE   float   xyz 
//
      vs_4_0
      dcl_constantbuffer CB0[7], immediateIndexed
      dcl_constantbuffer CB1[21], immediateIndexed
      dcl_input v0.xyz
      dcl_input v1.xyz
      dcl_input v3.xyzw
      dcl_input v4.xyzw
      dcl_input v5.xyzw
      dcl_output_siv o0.xyzw, position
      dcl_output o1.xyzw
      dcl_output o2.xyzw
      dcl_output o3.xyzw
      dcl_output o4.xyz
      dcl_output o5.xyz
      dcl_temps 2
   0: mul r0.xyz, v0.yyyy, cb0[1].xyzx
   1: mad r0.xyz, cb0[0].xyzx, v0.xxxx, r0.xyzx
   2: mad r0.xyz, cb0[2].xyzx, v0.zzzz, r0.xyzx
   3: add r0.xyz, r0.xyzx, cb0[3].xyzx
   4: mul r1.xyzw, r0.yyyy, cb1[18].xyzw
   5: mad r1.xyzw, cb1[17].xyzw, r0.xxxx, r1.xyzw
   6: mad r1.xyzw, cb1[19].xyzw, r0.zzzz, r1.xyzw
   7: mov o4.xyz, r0.xyzx
   8: add o0.xyzw, r1.xyzw, cb1[20].xyzw
   9: mov o1.xyzw, v3.xyzw
  10: mov o2.xyzw, v4.xyzw
  11: mov o3.xyzw, v5.xyzw
  12: dp3 r0.x, v1.xyzx, cb0[4].xyzx
  13: dp3 r0.y, v1.xyzx, cb0[5].xyzx
  14: dp3 r0.z, v1.xyzx, cb0[6].xyzx
  15: dp3 r0.w, r0.xyzx, r0.xyzx
  16: max r0.w, r0.w, l(0.000000)
  17: rsq r0.w, r0.w
  18: mul o5.xyz, r0.wwww, r0.xyzx
  19: ret 
// Approximately 0 instruction slots used


-- Fragment shader for "d3d11":
// No shader variant for this keyword set. The closest match will be used instead.

//////////////////////////////////////////////////////
Keywords: DIRECTIONAL VERTEXLIGHT_ON
-- Hardware tier variant: Tier 1
-- Vertex shader for "d3d11":
// Stats: 15 math, 2 temp registers
Uses vertex data channel "Vertex"
Uses vertex data channel "Normal"
Uses vertex data channel "TexCoord0"
Uses vertex data channel "TexCoord1"
Uses vertex data channel "Color"

Constant Buffer "UnityPerDraw" (176 bytes) on slot 0 {
  Matrix4x4 unity_ObjectToWorld at 0
  Matrix4x4 unity_WorldToObject at 64
}
Constant Buffer "UnityPerFrame" (368 bytes) on slot 1 {
  Matrix4x4 unity_MatrixVP at 272
}

Shader Disassembly:
//
// Generated by Microsoft (R) D3D Shader Disassembler
//
//
// Input signature:
//
// Name                 Index   Mask Register SysValue  Format   Used
// -------------------- ----- ------ -------- -------- ------- ------
// POSITION                 0   xyz         0     NONE   float   xyz 
// NORMAL                   0   xyz         1     NONE   float   xyz 
// TANGENT                  0   xyzw        2     NONE   float       
// TEXCOORD                 0   xyzw        3     NONE   float   xyzw
// TEXCOORD                 1   xyzw        4     NONE   float   xyzw
// COLOR                    0   xyzw        5     NONE   float   xyzw
//
//
// Output signature:
//
// Name                 Index   Mask Register SysValue  Format   Used
// -------------------- ----- ------ -------- -------- ------- ------
// SV_POSITION              0   xyzw        0      POS   float   xyzw
// INTERP                   0   xyzw        1     NONE   float   xyzw
// INTERP                   1   xyzw        2     NONE   float   xyzw
// INTERP                   2   xyzw        3     NONE   float   xyzw
// INTERP                   3   xyz         4     NONE   float   xyz 
// INTERP                   4   xyz         5     NONE   float   xyz 
//
      vs_4_0
      dcl_constantbuffer CB0[7], immediateIndexed
      dcl_constantbuffer CB1[21], immediateIndexed
      dcl_input v0.xyz
      dcl_input v1.xyz
      dcl_input v3.xyzw
      dcl_input v4.xyzw
      dcl_input v5.xyzw
      dcl_output_siv o0.xyzw, position
      dcl_output o1.xyzw
      dcl_output o2.xyzw
      dcl_output o3.xyzw
      dcl_output o4.xyz
      dcl_output o5.xyz
      dcl_temps 2
   0: mul r0.xyz, v0.yyyy, cb0[1].xyzx
   1: mad r0.xyz, cb0[0].xyzx, v0.xxxx, r0.xyzx
   2: mad r0.xyz, cb0[2].xyzx, v0.zzzz, r0.xyzx
   3: add r0.xyz, r0.xyzx, cb0[3].xyzx
   4: mul r1.xyzw, r0.yyyy, cb1[18].xyzw
   5: mad r1.xyzw, cb1[17].xyzw, r0.xxxx, r1.xyzw
   6: mad r1.xyzw, cb1[19].xyzw, r0.zzzz, r1.xyzw
   7: mov o4.xyz, r0.xyzx
   8: add o0.xyzw, r1.xyzw, cb1[20].xyzw
   9: mov o1.xyzw, v3.xyzw
  10: mov o2.xyzw, v4.xyzw
  11: mov o3.xyzw, v5.xyzw
  12: dp3 r0.x, v1.xyzx, cb0[4].xyzx
  13: dp3 r0.y, v1.xyzx, cb0[5].xyzx
  14: dp3 r0.z, v1.xyzx, cb0[6].xyzx
  15: dp3 r0.w, r0.xyzx, r0.xyzx
  16: max r0.w, r0.w, l(0.000000)
  17: rsq r0.w, r0.w
  18: mul o5.xyz, r0.wwww, r0.xyzx
  19: ret 
// Approximately 0 instruction slots used


-- Fragment shader for "d3d11":
// No shader variant for this keyword set. The closest match will be used instead.

//////////////////////////////////////////////////////
Keywords: DIRECTIONAL LIGHTPROBE_SH VERTEXLIGHT_ON
-- Hardware tier variant: Tier 1
-- Vertex shader for "d3d11":
// Stats: 15 math, 2 temp registers
Uses vertex data channel "Vertex"
Uses vertex data channel "Normal"
Uses vertex data channel "TexCoord0"
Uses vertex data channel "TexCoord1"
Uses vertex data channel "Color"

Constant Buffer "UnityPerDraw" (176 bytes) on slot 0 {
  Matrix4x4 unity_ObjectToWorld at 0
  Matrix4x4 unity_WorldToObject at 64
}
Constant Buffer "UnityPerFrame" (368 bytes) on slot 1 {
  Matrix4x4 unity_MatrixVP at 272
}

Shader Disassembly:
//
// Generated by Microsoft (R) D3D Shader Disassembler
//
//
// Input signature:
//
// Name                 Index   Mask Register SysValue  Format   Used
// -------------------- ----- ------ -------- -------- ------- ------
// POSITION                 0   xyz         0     NONE   float   xyz 
// NORMAL                   0   xyz         1     NONE   float   xyz 
// TANGENT                  0   xyzw        2     NONE   float       
// TEXCOORD                 0   xyzw        3     NONE   float   xyzw
// TEXCOORD                 1   xyzw        4     NONE   float   xyzw
// COLOR                    0   xyzw        5     NONE   float   xyzw
//
//
// Output signature:
//
// Name                 Index   Mask Register SysValue  Format   Used
// -------------------- ----- ------ -------- -------- ------- ------
// SV_POSITION              0   xyzw        0      POS   float   xyzw
// INTERP                   0   xyzw        1     NONE   float   xyzw
// INTERP                   1   xyzw        2     NONE   float   xyzw
// INTERP                   2   xyzw        3     NONE   float   xyzw
// INTERP                   3   xyz         4     NONE   float   xyz 
// INTERP                   4   xyz         5     NONE   float   xyz 
//
      vs_4_0
      dcl_constantbuffer CB0[7], immediateIndexed
      dcl_constantbuffer CB1[21], immediateIndexed
      dcl_input v0.xyz
      dcl_input v1.xyz
      dcl_input v3.xyzw
      dcl_input v4.xyzw
      dcl_input v5.xyzw
      dcl_output_siv o0.xyzw, position
      dcl_output o1.xyzw
      dcl_output o2.xyzw
      dcl_output o3.xyzw
      dcl_output o4.xyz
      dcl_output o5.xyz
      dcl_temps 2
   0: mul r0.xyz, v0.yyyy, cb0[1].xyzx
   1: mad r0.xyz, cb0[0].xyzx, v0.xxxx, r0.xyzx
   2: mad r0.xyz, cb0[2].xyzx, v0.zzzz, r0.xyzx
   3: add r0.xyz, r0.xyzx, cb0[3].xyzx
   4: mul r1.xyzw, r0.yyyy, cb1[18].xyzw
   5: mad r1.xyzw, cb1[17].xyzw, r0.xxxx, r1.xyzw
   6: mad r1.xyzw, cb1[19].xyzw, r0.zzzz, r1.xyzw
   7: mov o4.xyz, r0.xyzx
   8: add o0.xyzw, r1.xyzw, cb1[20].xyzw
   9: mov o1.xyzw, v3.xyzw
  10: mov o2.xyzw, v4.xyzw
  11: mov o3.xyzw, v5.xyzw
  12: dp3 r0.x, v1.xyzx, cb0[4].xyzx
  13: dp3 r0.y, v1.xyzx, cb0[5].xyzx
  14: dp3 r0.z, v1.xyzx, cb0[6].xyzx
  15: dp3 r0.w, r0.xyzx, r0.xyzx
  16: max r0.w, r0.w, l(0.000000)
  17: rsq r0.w, r0.w
  18: mul o5.xyz, r0.wwww, r0.xyzx
  19: ret 
// Approximately 0 instruction slots used


-- Fragment shader for "d3d11":
// No shader variant for this keyword set. The closest match will be used instead.

//////////////////////////////////////////////////////
Keywords: DIRECTIONAL SHADOWS_SCREEN VERTEXLIGHT_ON
-- Hardware tier variant: Tier 1
-- Vertex shader for "d3d11":
// Stats: 15 math, 2 temp registers
Uses vertex data channel "Vertex"
Uses vertex data channel "Normal"
Uses vertex data channel "TexCoord0"
Uses vertex data channel "TexCoord1"
Uses vertex data channel "Color"

Constant Buffer "UnityPerDraw" (176 bytes) on slot 0 {
  Matrix4x4 unity_ObjectToWorld at 0
  Matrix4x4 unity_WorldToObject at 64
}
Constant Buffer "UnityPerFrame" (368 bytes) on slot 1 {
  Matrix4x4 unity_MatrixVP at 272
}

Shader Disassembly:
//
// Generated by Microsoft (R) D3D Shader Disassembler
//
//
// Input signature:
//
// Name                 Index   Mask Register SysValue  Format   Used
// -------------------- ----- ------ -------- -------- ------- ------
// POSITION                 0   xyz         0     NONE   float   xyz 
// NORMAL                   0   xyz         1     NONE   float   xyz 
// TANGENT                  0   xyzw        2     NONE   float       
// TEXCOORD                 0   xyzw        3     NONE   float   xyzw
// TEXCOORD                 1   xyzw        4     NONE   float   xyzw
// COLOR                    0   xyzw        5     NONE   float   xyzw
//
//
// Output signature:
//
// Name                 Index   Mask Register SysValue  Format   Used
// -------------------- ----- ------ -------- -------- ------- ------
// SV_POSITION              0   xyzw        0      POS   float   xyzw
// INTERP                   0   xyzw        1     NONE   float   xyzw
// INTERP                   1   xyzw        2     NONE   float   xyzw
// INTERP                   2   xyzw        3     NONE   float   xyzw
// INTERP                   3   xyz         4     NONE   float   xyz 
// INTERP                   4   xyz         5     NONE   float   xyz 
//
      vs_4_0
      dcl_constantbuffer CB0[7], immediateIndexed
      dcl_constantbuffer CB1[21], immediateIndexed
      dcl_input v0.xyz
      dcl_input v1.xyz
      dcl_input v3.xyzw
      dcl_input v4.xyzw
      dcl_input v5.xyzw
      dcl_output_siv o0.xyzw, position
      dcl_output o1.xyzw
      dcl_output o2.xyzw
      dcl_output o3.xyzw
      dcl_output o4.xyz
      dcl_output o5.xyz
      dcl_temps 2
   0: mul r0.xyz, v0.yyyy, cb0[1].xyzx
   1: mad r0.xyz, cb0[0].xyzx, v0.xxxx, r0.xyzx
   2: mad r0.xyz, cb0[2].xyzx, v0.zzzz, r0.xyzx
   3: add r0.xyz, r0.xyzx, cb0[3].xyzx
   4: mul r1.xyzw, r0.yyyy, cb1[18].xyzw
   5: mad r1.xyzw, cb1[17].xyzw, r0.xxxx, r1.xyzw
   6: mad r1.xyzw, cb1[19].xyzw, r0.zzzz, r1.xyzw
   7: mov o4.xyz, r0.xyzx
   8: add o0.xyzw, r1.xyzw, cb1[20].xyzw
   9: mov o1.xyzw, v3.xyzw
  10: mov o2.xyzw, v4.xyzw
  11: mov o3.xyzw, v5.xyzw
  12: dp3 r0.x, v1.xyzx, cb0[4].xyzx
  13: dp3 r0.y, v1.xyzx, cb0[5].xyzx
  14: dp3 r0.z, v1.xyzx, cb0[6].xyzx
  15: dp3 r0.w, r0.xyzx, r0.xyzx
  16: max r0.w, r0.w, l(0.000000)
  17: rsq r0.w, r0.w
  18: mul o5.xyz, r0.wwww, r0.xyzx
  19: ret 
// Approximately 0 instruction slots used


-- Fragment shader for "d3d11":
// No shader variant for this keyword set. The closest match will be used instead.

//////////////////////////////////////////////////////
Keywords: DIRECTIONAL LIGHTPROBE_SH SHADOWS_SCREEN VERTEXLIGHT_ON
-- Hardware tier variant: Tier 1
-- Vertex shader for "d3d11":
// Stats: 15 math, 2 temp registers
Uses vertex data channel "Vertex"
Uses vertex data channel "Normal"
Uses vertex data channel "TexCoord0"
Uses vertex data channel "TexCoord1"
Uses vertex data channel "Color"

Constant Buffer "UnityPerDraw" (176 bytes) on slot 0 {
  Matrix4x4 unity_ObjectToWorld at 0
  Matrix4x4 unity_WorldToObject at 64
}
Constant Buffer "UnityPerFrame" (368 bytes) on slot 1 {
  Matrix4x4 unity_MatrixVP at 272
}

Shader Disassembly:
//
// Generated by Microsoft (R) D3D Shader Disassembler
//
//
// Input signature:
//
// Name                 Index   Mask Register SysValue  Format   Used
// -------------------- ----- ------ -------- -------- ------- ------
// POSITION                 0   xyz         0     NONE   float   xyz 
// NORMAL                   0   xyz         1     NONE   float   xyz 
// TANGENT                  0   xyzw        2     NONE   float       
// TEXCOORD                 0   xyzw        3     NONE   float   xyzw
// TEXCOORD                 1   xyzw        4     NONE   float   xyzw
// COLOR                    0   xyzw        5     NONE   float   xyzw
//
//
// Output signature:
//
// Name                 Index   Mask Register SysValue  Format   Used
// -------------------- ----- ------ -------- -------- ------- ------
// SV_POSITION              0   xyzw        0      POS   float   xyzw
// INTERP                   0   xyzw        1     NONE   float   xyzw
// INTERP                   1   xyzw        2     NONE   float   xyzw
// INTERP                   2   xyzw        3     NONE   float   xyzw
// INTERP                   3   xyz         4     NONE   float   xyz 
// INTERP                   4   xyz         5     NONE   float   xyz 
//
      vs_4_0
      dcl_constantbuffer CB0[7], immediateIndexed
      dcl_constantbuffer CB1[21], immediateIndexed
      dcl_input v0.xyz
      dcl_input v1.xyz
      dcl_input v3.xyzw
      dcl_input v4.xyzw
      dcl_input v5.xyzw
      dcl_output_siv o0.xyzw, position
      dcl_output o1.xyzw
      dcl_output o2.xyzw
      dcl_output o3.xyzw
      dcl_output o4.xyz
      dcl_output o5.xyz
      dcl_temps 2
   0: mul r0.xyz, v0.yyyy, cb0[1].xyzx
   1: mad r0.xyz, cb0[0].xyzx, v0.xxxx, r0.xyzx
   2: mad r0.xyz, cb0[2].xyzx, v0.zzzz, r0.xyzx
   3: add r0.xyz, r0.xyzx, cb0[3].xyzx
   4: mul r1.xyzw, r0.yyyy, cb1[18].xyzw
   5: mad r1.xyzw, cb1[17].xyzw, r0.xxxx, r1.xyzw
   6: mad r1.xyzw, cb1[19].xyzw, r0.zzzz, r1.xyzw
   7: mov o4.xyz, r0.xyzx
   8: add o0.xyzw, r1.xyzw, cb1[20].xyzw
   9: mov o1.xyzw, v3.xyzw
  10: mov o2.xyzw, v4.xyzw
  11: mov o3.xyzw, v5.xyzw
  12: dp3 r0.x, v1.xyzx, cb0[4].xyzx
  13: dp3 r0.y, v1.xyzx, cb0[5].xyzx
  14: dp3 r0.z, v1.xyzx, cb0[6].xyzx
  15: dp3 r0.w, r0.xyzx, r0.xyzx
  16: max r0.w, r0.w, l(0.000000)
  17: rsq r0.w, r0.w
  18: mul o5.xyz, r0.wwww, r0.xyzx
  19: ret 
// Approximately 0 instruction slots used


-- Fragment shader for "d3d11":
// No shader variant for this keyword set. The closest match will be used instead.

//////////////////////////////////////////////////////
Keywords: DIRECTIONAL FOG_EXP2
-- Hardware tier variant: Tier 1
-- Vertex shader for "d3d11":
// Stats: 15 math, 2 temp registers
Uses vertex data channel "Vertex"
Uses vertex data channel "Normal"
Uses vertex data channel "TexCoord0"
Uses vertex data channel "TexCoord1"
Uses vertex data channel "Color"

Constant Buffer "UnityPerDraw" (176 bytes) on slot 0 {
  Matrix4x4 unity_ObjectToWorld at 0
  Matrix4x4 unity_WorldToObject at 64
}
Constant Buffer "UnityPerFrame" (368 bytes) on slot 1 {
  Matrix4x4 unity_MatrixVP at 272
}

Shader Disassembly:
//
// Generated by Microsoft (R) D3D Shader Disassembler
//
//
// Input signature:
//
// Name                 Index   Mask Register SysValue  Format   Used
// -------------------- ----- ------ -------- -------- ------- ------
// POSITION                 0   xyz         0     NONE   float   xyz 
// NORMAL                   0   xyz         1     NONE   float   xyz 
// TANGENT                  0   xyzw        2     NONE   float       
// TEXCOORD                 0   xyzw        3     NONE   float   xyzw
// TEXCOORD                 1   xyzw        4     NONE   float   xyzw
// COLOR                    0   xyzw        5     NONE   float   xyzw
//
//
// Output signature:
//
// Name                 Index   Mask Register SysValue  Format   Used
// -------------------- ----- ------ -------- -------- ------- ------
// SV_POSITION              0   xyzw        0      POS   float   xyzw
// INTERP                   0   xyzw        1     NONE   float   xyzw
// INTERP                   1   xyzw        2     NONE   float   xyzw
// INTERP                   2   xyzw        3     NONE   float   xyzw
// INTERP                   3   xyz         4     NONE   float   xyz 
// INTERP                   4   xyz         5     NONE   float   xyz 
//
      vs_4_0
      dcl_constantbuffer CB0[7], immediateIndexed
      dcl_constantbuffer CB1[21], immediateIndexed
      dcl_input v0.xyz
      dcl_input v1.xyz
      dcl_input v3.xyzw
      dcl_input v4.xyzw
      dcl_input v5.xyzw
      dcl_output_siv o0.xyzw, position
      dcl_output o1.xyzw
      dcl_output o2.xyzw
      dcl_output o3.xyzw
      dcl_output o4.xyz
      dcl_output o5.xyz
      dcl_temps 2
   0: mul r0.xyz, v0.yyyy, cb0[1].xyzx
   1: mad r0.xyz, cb0[0].xyzx, v0.xxxx, r0.xyzx
   2: mad r0.xyz, cb0[2].xyzx, v0.zzzz, r0.xyzx
   3: add r0.xyz, r0.xyzx, cb0[3].xyzx
   4: mul r1.xyzw, r0.yyyy, cb1[18].xyzw
   5: mad r1.xyzw, cb1[17].xyzw, r0.xxxx, r1.xyzw
   6: mad r1.xyzw, cb1[19].xyzw, r0.zzzz, r1.xyzw
   7: mov o4.xyz, r0.xyzx
   8: add o0.xyzw, r1.xyzw, cb1[20].xyzw
   9: mov o1.xyzw, v3.xyzw
  10: mov o2.xyzw, v4.xyzw
  11: mov o3.xyzw, v5.xyzw
  12: dp3 r0.x, v1.xyzx, cb0[4].xyzx
  13: dp3 r0.y, v1.xyzx, cb0[5].xyzx
  14: dp3 r0.z, v1.xyzx, cb0[6].xyzx
  15: dp3 r0.w, r0.xyzx, r0.xyzx
  16: max r0.w, r0.w, l(0.000000)
  17: rsq r0.w, r0.w
  18: mul o5.xyz, r0.wwww, r0.xyzx
  19: ret 
// Approximately 0 instruction slots used


-- Fragment shader for "d3d11":
// No shader variant for this keyword set. The closest match will be used instead.

//////////////////////////////////////////////////////
Keywords: DIRECTIONAL FOG_EXP2 LIGHTPROBE_SH
-- Hardware tier variant: Tier 1
-- Vertex shader for "d3d11":
// Stats: 15 math, 2 temp registers
Uses vertex data channel "Vertex"
Uses vertex data channel "Normal"
Uses vertex data channel "TexCoord0"
Uses vertex data channel "TexCoord1"
Uses vertex data channel "Color"

Constant Buffer "UnityPerDraw" (176 bytes) on slot 0 {
  Matrix4x4 unity_ObjectToWorld at 0
  Matrix4x4 unity_WorldToObject at 64
}
Constant Buffer "UnityPerFrame" (368 bytes) on slot 1 {
  Matrix4x4 unity_MatrixVP at 272
}

Shader Disassembly:
//
// Generated by Microsoft (R) D3D Shader Disassembler
//
//
// Input signature:
//
// Name                 Index   Mask Register SysValue  Format   Used
// -------------------- ----- ------ -------- -------- ------- ------
// POSITION                 0   xyz         0     NONE   float   xyz 
// NORMAL                   0   xyz         1     NONE   float   xyz 
// TANGENT                  0   xyzw        2     NONE   float       
// TEXCOORD                 0   xyzw        3     NONE   float   xyzw
// TEXCOORD                 1   xyzw        4     NONE   float   xyzw
// COLOR                    0   xyzw        5     NONE   float   xyzw
//
//
// Output signature:
//
// Name                 Index   Mask Register SysValue  Format   Used
// -------------------- ----- ------ -------- -------- ------- ------
// SV_POSITION              0   xyzw        0      POS   float   xyzw
// INTERP                   0   xyzw        1     NONE   float   xyzw
// INTERP                   1   xyzw        2     NONE   float   xyzw
// INTERP                   2   xyzw        3     NONE   float   xyzw
// INTERP                   3   xyz         4     NONE   float   xyz 
// INTERP                   4   xyz         5     NONE   float   xyz 
//
      vs_4_0
      dcl_constantbuffer CB0[7], immediateIndexed
      dcl_constantbuffer CB1[21], immediateIndexed
      dcl_input v0.xyz
      dcl_input v1.xyz
      dcl_input v3.xyzw
      dcl_input v4.xyzw
      dcl_input v5.xyzw
      dcl_output_siv o0.xyzw, position
      dcl_output o1.xyzw
      dcl_output o2.xyzw
      dcl_output o3.xyzw
      dcl_output o4.xyz
      dcl_output o5.xyz
      dcl_temps 2
   0: mul r0.xyz, v0.yyyy, cb0[1].xyzx
   1: mad r0.xyz, cb0[0].xyzx, v0.xxxx, r0.xyzx
   2: mad r0.xyz, cb0[2].xyzx, v0.zzzz, r0.xyzx
   3: add r0.xyz, r0.xyzx, cb0[3].xyzx
   4: mul r1.xyzw, r0.yyyy, cb1[18].xyzw
   5: mad r1.xyzw, cb1[17].xyzw, r0.xxxx, r1.xyzw
   6: mad r1.xyzw, cb1[19].xyzw, r0.zzzz, r1.xyzw
   7: mov o4.xyz, r0.xyzx
   8: add o0.xyzw, r1.xyzw, cb1[20].xyzw
   9: mov o1.xyzw, v3.xyzw
  10: mov o2.xyzw, v4.xyzw
  11: mov o3.xyzw, v5.xyzw
  12: dp3 r0.x, v1.xyzx, cb0[4].xyzx
  13: dp3 r0.y, v1.xyzx, cb0[5].xyzx
  14: dp3 r0.z, v1.xyzx, cb0[6].xyzx
  15: dp3 r0.w, r0.xyzx, r0.xyzx
  16: max r0.w, r0.w, l(0.000000)
  17: rsq r0.w, r0.w
  18: mul o5.xyz, r0.wwww, r0.xyzx
  19: ret 
// Approximately 0 instruction slots used


-- Fragment shader for "d3d11":
// No shader variant for this keyword set. The closest match will be used instead.

//////////////////////////////////////////////////////
Keywords: DIRECTIONAL FOG_EXP2 SHADOWS_SCREEN
-- Hardware tier variant: Tier 1
-- Vertex shader for "d3d11":
// Stats: 15 math, 2 temp registers
Uses vertex data channel "Vertex"
Uses vertex data channel "Normal"
Uses vertex data channel "TexCoord0"
Uses vertex data channel "TexCoord1"
Uses vertex data channel "Color"

Constant Buffer "UnityPerDraw" (176 bytes) on slot 0 {
  Matrix4x4 unity_ObjectToWorld at 0
  Matrix4x4 unity_WorldToObject at 64
}
Constant Buffer "UnityPerFrame" (368 bytes) on slot 1 {
  Matrix4x4 unity_MatrixVP at 272
}

Shader Disassembly:
//
// Generated by Microsoft (R) D3D Shader Disassembler
//
//
// Input signature:
//
// Name                 Index   Mask Register SysValue  Format   Used
// -------------------- ----- ------ -------- -------- ------- ------
// POSITION                 0   xyz         0     NONE   float   xyz 
// NORMAL                   0   xyz         1     NONE   float   xyz 
// TANGENT                  0   xyzw        2     NONE   float       
// TEXCOORD                 0   xyzw        3     NONE   float   xyzw
// TEXCOORD                 1   xyzw        4     NONE   float   xyzw
// COLOR                    0   xyzw        5     NONE   float   xyzw
//
//
// Output signature:
//
// Name                 Index   Mask Register SysValue  Format   Used
// -------------------- ----- ------ -------- -------- ------- ------
// SV_POSITION              0   xyzw        0      POS   float   xyzw
// INTERP                   0   xyzw        1     NONE   float   xyzw
// INTERP                   1   xyzw        2     NONE   float   xyzw
// INTERP                   2   xyzw        3     NONE   float   xyzw
// INTERP                   3   xyz         4     NONE   float   xyz 
// INTERP                   4   xyz         5     NONE   float   xyz 
//
      vs_4_0
      dcl_constantbuffer CB0[7], immediateIndexed
      dcl_constantbuffer CB1[21], immediateIndexed
      dcl_input v0.xyz
      dcl_input v1.xyz
      dcl_input v3.xyzw
      dcl_input v4.xyzw
      dcl_input v5.xyzw
      dcl_output_siv o0.xyzw, position
      dcl_output o1.xyzw
      dcl_output o2.xyzw
      dcl_output o3.xyzw
      dcl_output o4.xyz
      dcl_output o5.xyz
      dcl_temps 2
   0: mul r0.xyz, v0.yyyy, cb0[1].xyzx
   1: mad r0.xyz, cb0[0].xyzx, v0.xxxx, r0.xyzx
   2: mad r0.xyz, cb0[2].xyzx, v0.zzzz, r0.xyzx
   3: add r0.xyz, r0.xyzx, cb0[3].xyzx
   4: mul r1.xyzw, r0.yyyy, cb1[18].xyzw
   5: mad r1.xyzw, cb1[17].xyzw, r0.xxxx, r1.xyzw
   6: mad r1.xyzw, cb1[19].xyzw, r0.zzzz, r1.xyzw
   7: mov o4.xyz, r0.xyzx
   8: add o0.xyzw, r1.xyzw, cb1[20].xyzw
   9: mov o1.xyzw, v3.xyzw
  10: mov o2.xyzw, v4.xyzw
  11: mov o3.xyzw, v5.xyzw
  12: dp3 r0.x, v1.xyzx, cb0[4].xyzx
  13: dp3 r0.y, v1.xyzx, cb0[5].xyzx
  14: dp3 r0.z, v1.xyzx, cb0[6].xyzx
  15: dp3 r0.w, r0.xyzx, r0.xyzx
  16: max r0.w, r0.w, l(0.000000)
  17: rsq r0.w, r0.w
  18: mul o5.xyz, r0.wwww, r0.xyzx
  19: ret 
// Approximately 0 instruction slots used


-- Fragment shader for "d3d11":
// No shader variant for this keyword set. The closest match will be used instead.

//////////////////////////////////////////////////////
Keywords: DIRECTIONAL FOG_EXP2 LIGHTPROBE_SH SHADOWS_SCREEN
-- Hardware tier variant: Tier 1
-- Vertex shader for "d3d11":
// Stats: 15 math, 2 temp registers
Uses vertex data channel "Vertex"
Uses vertex data channel "Normal"
Uses vertex data channel "TexCoord0"
Uses vertex data channel "TexCoord1"
Uses vertex data channel "Color"

Constant Buffer "UnityPerDraw" (176 bytes) on slot 0 {
  Matrix4x4 unity_ObjectToWorld at 0
  Matrix4x4 unity_WorldToObject at 64
}
Constant Buffer "UnityPerFrame" (368 bytes) on slot 1 {
  Matrix4x4 unity_MatrixVP at 272
}

Shader Disassembly:
//
// Generated by Microsoft (R) D3D Shader Disassembler
//
//
// Input signature:
//
// Name                 Index   Mask Register SysValue  Format   Used
// -------------------- ----- ------ -------- -------- ------- ------
// POSITION                 0   xyz         0     NONE   float   xyz 
// NORMAL                   0   xyz         1     NONE   float   xyz 
// TANGENT                  0   xyzw        2     NONE   float       
// TEXCOORD                 0   xyzw        3     NONE   float   xyzw
// TEXCOORD                 1   xyzw        4     NONE   float   xyzw
// COLOR                    0   xyzw        5     NONE   float   xyzw
//
//
// Output signature:
//
// Name                 Index   Mask Register SysValue  Format   Used
// -------------------- ----- ------ -------- -------- ------- ------
// SV_POSITION              0   xyzw        0      POS   float   xyzw
// INTERP                   0   xyzw        1     NONE   float   xyzw
// INTERP                   1   xyzw        2     NONE   float   xyzw
// INTERP                   2   xyzw        3     NONE   float   xyzw
// INTERP                   3   xyz         4     NONE   float   xyz 
// INTERP                   4   xyz         5     NONE   float   xyz 
//
      vs_4_0
      dcl_constantbuffer CB0[7], immediateIndexed
      dcl_constantbuffer CB1[21], immediateIndexed
      dcl_input v0.xyz
      dcl_input v1.xyz
      dcl_input v3.xyzw
      dcl_input v4.xyzw
      dcl_input v5.xyzw
      dcl_output_siv o0.xyzw, position
      dcl_output o1.xyzw
      dcl_output o2.xyzw
      dcl_output o3.xyzw
      dcl_output o4.xyz
      dcl_output o5.xyz
      dcl_temps 2
   0: mul r0.xyz, v0.yyyy, cb0[1].xyzx
   1: mad r0.xyz, cb0[0].xyzx, v0.xxxx, r0.xyzx
   2: mad r0.xyz, cb0[2].xyzx, v0.zzzz, r0.xyzx
   3: add r0.xyz, r0.xyzx, cb0[3].xyzx
   4: mul r1.xyzw, r0.yyyy, cb1[18].xyzw
   5: mad r1.xyzw, cb1[17].xyzw, r0.xxxx, r1.xyzw
   6: mad r1.xyzw, cb1[19].xyzw, r0.zzzz, r1.xyzw
   7: mov o4.xyz, r0.xyzx
   8: add o0.xyzw, r1.xyzw, cb1[20].xyzw
   9: mov o1.xyzw, v3.xyzw
  10: mov o2.xyzw, v4.xyzw
  11: mov o3.xyzw, v5.xyzw
  12: dp3 r0.x, v1.xyzx, cb0[4].xyzx
  13: dp3 r0.y, v1.xyzx, cb0[5].xyzx
  14: dp3 r0.z, v1.xyzx, cb0[6].xyzx
  15: dp3 r0.w, r0.xyzx, r0.xyzx
  16: max r0.w, r0.w, l(0.000000)
  17: rsq r0.w, r0.w
  18: mul o5.xyz, r0.wwww, r0.xyzx
  19: ret 
// Approximately 0 instruction slots used


-- Fragment shader for "d3d11":
// No shader variant for this keyword set. The closest match will be used instead.

//////////////////////////////////////////////////////
Keywords: DIRECTIONAL FOG_EXP2 VERTEXLIGHT_ON
-- Hardware tier variant: Tier 1
-- Vertex shader for "d3d11":
// Stats: 15 math, 2 temp registers
Uses vertex data channel "Vertex"
Uses vertex data channel "Normal"
Uses vertex data channel "TexCoord0"
Uses vertex data channel "TexCoord1"
Uses vertex data channel "Color"

Constant Buffer "UnityPerDraw" (176 bytes) on slot 0 {
  Matrix4x4 unity_ObjectToWorld at 0
  Matrix4x4 unity_WorldToObject at 64
}
Constant Buffer "UnityPerFrame" (368 bytes) on slot 1 {
  Matrix4x4 unity_MatrixVP at 272
}

Shader Disassembly:
//
// Generated by Microsoft (R) D3D Shader Disassembler
//
//
// Input signature:
//
// Name                 Index   Mask Register SysValue  Format   Used
// -------------------- ----- ------ -------- -------- ------- ------
// POSITION                 0   xyz         0     NONE   float   xyz 
// NORMAL                   0   xyz         1     NONE   float   xyz 
// TANGENT                  0   xyzw        2     NONE   float       
// TEXCOORD                 0   xyzw        3     NONE   float   xyzw
// TEXCOORD                 1   xyzw        4     NONE   float   xyzw
// COLOR                    0   xyzw        5     NONE   float   xyzw
//
//
// Output signature:
//
// Name                 Index   Mask Register SysValue  Format   Used
// -------------------- ----- ------ -------- -------- ------- ------
// SV_POSITION              0   xyzw        0      POS   float   xyzw
// INTERP                   0   xyzw        1     NONE   float   xyzw
// INTERP                   1   xyzw        2     NONE   float   xyzw
// INTERP                   2   xyzw        3     NONE   float   xyzw
// INTERP                   3   xyz         4     NONE   float   xyz 
// INTERP                   4   xyz         5     NONE   float   xyz 
//
      vs_4_0
      dcl_constantbuffer CB0[7], immediateIndexed
      dcl_constantbuffer CB1[21], immediateIndexed
      dcl_input v0.xyz
      dcl_input v1.xyz
      dcl_input v3.xyzw
      dcl_input v4.xyzw
      dcl_input v5.xyzw
      dcl_output_siv o0.xyzw, position
      dcl_output o1.xyzw
      dcl_output o2.xyzw
      dcl_output o3.xyzw
      dcl_output o4.xyz
      dcl_output o5.xyz
      dcl_temps 2
   0: mul r0.xyz, v0.yyyy, cb0[1].xyzx
   1: mad r0.xyz, cb0[0].xyzx, v0.xxxx, r0.xyzx
   2: mad r0.xyz, cb0[2].xyzx, v0.zzzz, r0.xyzx
   3: add r0.xyz, r0.xyzx, cb0[3].xyzx
   4: mul r1.xyzw, r0.yyyy, cb1[18].xyzw
   5: mad r1.xyzw, cb1[17].xyzw, r0.xxxx, r1.xyzw
   6: mad r1.xyzw, cb1[19].xyzw, r0.zzzz, r1.xyzw
   7: mov o4.xyz, r0.xyzx
   8: add o0.xyzw, r1.xyzw, cb1[20].xyzw
   9: mov o1.xyzw, v3.xyzw
  10: mov o2.xyzw, v4.xyzw
  11: mov o3.xyzw, v5.xyzw
  12: dp3 r0.x, v1.xyzx, cb0[4].xyzx
  13: dp3 r0.y, v1.xyzx, cb0[5].xyzx
  14: dp3 r0.z, v1.xyzx, cb0[6].xyzx
  15: dp3 r0.w, r0.xyzx, r0.xyzx
  16: max r0.w, r0.w, l(0.000000)
  17: rsq r0.w, r0.w
  18: mul o5.xyz, r0.wwww, r0.xyzx
  19: ret 
// Approximately 0 instruction slots used


-- Fragment shader for "d3d11":
// No shader variant for this keyword set. The closest match will be used instead.

//////////////////////////////////////////////////////
Keywords: DIRECTIONAL FOG_EXP2 LIGHTPROBE_SH VERTEXLIGHT_ON
-- Hardware tier variant: Tier 1
-- Vertex shader for "d3d11":
// Stats: 15 math, 2 temp registers
Uses vertex data channel "Vertex"
Uses vertex data channel "Normal"
Uses vertex data channel "TexCoord0"
Uses vertex data channel "TexCoord1"
Uses vertex data channel "Color"

Constant Buffer "UnityPerDraw" (176 bytes) on slot 0 {
  Matrix4x4 unity_ObjectToWorld at 0
  Matrix4x4 unity_WorldToObject at 64
}
Constant Buffer "UnityPerFrame" (368 bytes) on slot 1 {
  Matrix4x4 unity_MatrixVP at 272
}

Shader Disassembly:
//
// Generated by Microsoft (R) D3D Shader Disassembler
//
//
// Input signature:
//
// Name                 Index   Mask Register SysValue  Format   Used
// -------------------- ----- ------ -------- -------- ------- ------
// POSITION                 0   xyz         0     NONE   float   xyz 
// NORMAL                   0   xyz         1     NONE   float   xyz 
// TANGENT                  0   xyzw        2     NONE   float       
// TEXCOORD                 0   xyzw        3     NONE   float   xyzw
// TEXCOORD                 1   xyzw        4     NONE   float   xyzw
// COLOR                    0   xyzw        5     NONE   float   xyzw
//
//
// Output signature:
//
// Name                 Index   Mask Register SysValue  Format   Used
// -------------------- ----- ------ -------- -------- ------- ------
// SV_POSITION              0   xyzw        0      POS   float   xyzw
// INTERP                   0   xyzw        1     NONE   float   xyzw
// INTERP                   1   xyzw        2     NONE   float   xyzw
// INTERP                   2   xyzw        3     NONE   float   xyzw
// INTERP                   3   xyz         4     NONE   float   xyz 
// INTERP                   4   xyz         5     NONE   float   xyz 
//
      vs_4_0
      dcl_constantbuffer CB0[7], immediateIndexed
      dcl_constantbuffer CB1[21], immediateIndexed
      dcl_input v0.xyz
      dcl_input v1.xyz
      dcl_input v3.xyzw
      dcl_input v4.xyzw
      dcl_input v5.xyzw
      dcl_output_siv o0.xyzw, position
      dcl_output o1.xyzw
      dcl_output o2.xyzw
      dcl_output o3.xyzw
      dcl_output o4.xyz
      dcl_output o5.xyz
      dcl_temps 2
   0: mul r0.xyz, v0.yyyy, cb0[1].xyzx
   1: mad r0.xyz, cb0[0].xyzx, v0.xxxx, r0.xyzx
   2: mad r0.xyz, cb0[2].xyzx, v0.zzzz, r0.xyzx
   3: add r0.xyz, r0.xyzx, cb0[3].xyzx
   4: mul r1.xyzw, r0.yyyy, cb1[18].xyzw
   5: mad r1.xyzw, cb1[17].xyzw, r0.xxxx, r1.xyzw
   6: mad r1.xyzw, cb1[19].xyzw, r0.zzzz, r1.xyzw
   7: mov o4.xyz, r0.xyzx
   8: add o0.xyzw, r1.xyzw, cb1[20].xyzw
   9: mov o1.xyzw, v3.xyzw
  10: mov o2.xyzw, v4.xyzw
  11: mov o3.xyzw, v5.xyzw
  12: dp3 r0.x, v1.xyzx, cb0[4].xyzx
  13: dp3 r0.y, v1.xyzx, cb0[5].xyzx
  14: dp3 r0.z, v1.xyzx, cb0[6].xyzx
  15: dp3 r0.w, r0.xyzx, r0.xyzx
  16: max r0.w, r0.w, l(0.000000)
  17: rsq r0.w, r0.w
  18: mul o5.xyz, r0.wwww, r0.xyzx
  19: ret 
// Approximately 0 instruction slots used


-- Fragment shader for "d3d11":
// No shader variant for this keyword set. The closest match will be used instead.

//////////////////////////////////////////////////////
Keywords: DIRECTIONAL FOG_EXP2 SHADOWS_SCREEN VERTEXLIGHT_ON
-- Hardware tier variant: Tier 1
-- Vertex shader for "d3d11":
// Stats: 15 math, 2 temp registers
Uses vertex data channel "Vertex"
Uses vertex data channel "Normal"
Uses vertex data channel "TexCoord0"
Uses vertex data channel "TexCoord1"
Uses vertex data channel "Color"

Constant Buffer "UnityPerDraw" (176 bytes) on slot 0 {
  Matrix4x4 unity_ObjectToWorld at 0
  Matrix4x4 unity_WorldToObject at 64
}
Constant Buffer "UnityPerFrame" (368 bytes) on slot 1 {
  Matrix4x4 unity_MatrixVP at 272
}

Shader Disassembly:
//
// Generated by Microsoft (R) D3D Shader Disassembler
//
//
// Input signature:
//
// Name                 Index   Mask Register SysValue  Format   Used
// -------------------- ----- ------ -------- -------- ------- ------
// POSITION                 0   xyz         0     NONE   float   xyz 
// NORMAL                   0   xyz         1     NONE   float   xyz 
// TANGENT                  0   xyzw        2     NONE   float       
// TEXCOORD                 0   xyzw        3     NONE   float   xyzw
// TEXCOORD                 1   xyzw        4     NONE   float   xyzw
// COLOR                    0   xyzw        5     NONE   float   xyzw
//
//
// Output signature:
//
// Name                 Index   Mask Register SysValue  Format   Used
// -------------------- ----- ------ -------- -------- ------- ------
// SV_POSITION              0   xyzw        0      POS   float   xyzw
// INTERP                   0   xyzw        1     NONE   float   xyzw
// INTERP                   1   xyzw        2     NONE   float   xyzw
// INTERP                   2   xyzw        3     NONE   float   xyzw
// INTERP                   3   xyz         4     NONE   float   xyz 
// INTERP                   4   xyz         5     NONE   float   xyz 
//
      vs_4_0
      dcl_constantbuffer CB0[7], immediateIndexed
      dcl_constantbuffer CB1[21], immediateIndexed
      dcl_input v0.xyz
      dcl_input v1.xyz
      dcl_input v3.xyzw
      dcl_input v4.xyzw
      dcl_input v5.xyzw
      dcl_output_siv o0.xyzw, position
      dcl_output o1.xyzw
      dcl_output o2.xyzw
      dcl_output o3.xyzw
      dcl_output o4.xyz
      dcl_output o5.xyz
      dcl_temps 2
   0: mul r0.xyz, v0.yyyy, cb0[1].xyzx
   1: mad r0.xyz, cb0[0].xyzx, v0.xxxx, r0.xyzx
   2: mad r0.xyz, cb0[2].xyzx, v0.zzzz, r0.xyzx
   3: add r0.xyz, r0.xyzx, cb0[3].xyzx
   4: mul r1.xyzw, r0.yyyy, cb1[18].xyzw
   5: mad r1.xyzw, cb1[17].xyzw, r0.xxxx, r1.xyzw
   6: mad r1.xyzw, cb1[19].xyzw, r0.zzzz, r1.xyzw
   7: mov o4.xyz, r0.xyzx
   8: add o0.xyzw, r1.xyzw, cb1[20].xyzw
   9: mov o1.xyzw, v3.xyzw
  10: mov o2.xyzw, v4.xyzw
  11: mov o3.xyzw, v5.xyzw
  12: dp3 r0.x, v1.xyzx, cb0[4].xyzx
  13: dp3 r0.y, v1.xyzx, cb0[5].xyzx
  14: dp3 r0.z, v1.xyzx, cb0[6].xyzx
  15: dp3 r0.w, r0.xyzx, r0.xyzx
  16: max r0.w, r0.w, l(0.000000)
  17: rsq r0.w, r0.w
  18: mul o5.xyz, r0.wwww, r0.xyzx
  19: ret 
// Approximately 0 instruction slots used


-- Fragment shader for "d3d11":
// No shader variant for this keyword set. The closest match will be used instead.

//////////////////////////////////////////////////////
Keywords: DIRECTIONAL FOG_EXP2 LIGHTPROBE_SH SHADOWS_SCREEN VERTEXLIGHT_ON
-- Hardware tier variant: Tier 1
-- Vertex shader for "d3d11":
// Stats: 15 math, 2 temp registers
Uses vertex data channel "Vertex"
Uses vertex data channel "Normal"
Uses vertex data channel "TexCoord0"
Uses vertex data channel "TexCoord1"
Uses vertex data channel "Color"

Constant Buffer "UnityPerDraw" (176 bytes) on slot 0 {
  Matrix4x4 unity_ObjectToWorld at 0
  Matrix4x4 unity_WorldToObject at 64
}
Constant Buffer "UnityPerFrame" (368 bytes) on slot 1 {
  Matrix4x4 unity_MatrixVP at 272
}

Shader Disassembly:
//
// Generated by Microsoft (R) D3D Shader Disassembler
//
//
// Input signature:
//
// Name                 Index   Mask Register SysValue  Format   Used
// -------------------- ----- ------ -------- -------- ------- ------
// POSITION                 0   xyz         0     NONE   float   xyz 
// NORMAL                   0   xyz         1     NONE   float   xyz 
// TANGENT                  0   xyzw        2     NONE   float       
// TEXCOORD                 0   xyzw        3     NONE   float   xyzw
// TEXCOORD                 1   xyzw        4     NONE   float   xyzw
// COLOR                    0   xyzw        5     NONE   float   xyzw
//
//
// Output signature:
//
// Name                 Index   Mask Register SysValue  Format   Used
// -------------------- ----- ------ -------- -------- ------- ------
// SV_POSITION              0   xyzw        0      POS   float   xyzw
// INTERP                   0   xyzw        1     NONE   float   xyzw
// INTERP                   1   xyzw        2     NONE   float   xyzw
// INTERP                   2   xyzw        3     NONE   float   xyzw
// INTERP                   3   xyz         4     NONE   float   xyz 
// INTERP                   4   xyz         5     NONE   float   xyz 
//
      vs_4_0
      dcl_constantbuffer CB0[7], immediateIndexed
      dcl_constantbuffer CB1[21], immediateIndexed
      dcl_input v0.xyz
      dcl_input v1.xyz
      dcl_input v3.xyzw
      dcl_input v4.xyzw
      dcl_input v5.xyzw
      dcl_output_siv o0.xyzw, position
      dcl_output o1.xyzw
      dcl_output o2.xyzw
      dcl_output o3.xyzw
      dcl_output o4.xyz
      dcl_output o5.xyz
      dcl_temps 2
   0: mul r0.xyz, v0.yyyy, cb0[1].xyzx
   1: mad r0.xyz, cb0[0].xyzx, v0.xxxx, r0.xyzx
   2: mad r0.xyz, cb0[2].xyzx, v0.zzzz, r0.xyzx
   3: add r0.xyz, r0.xyzx, cb0[3].xyzx
   4: mul r1.xyzw, r0.yyyy, cb1[18].xyzw
   5: mad r1.xyzw, cb1[17].xyzw, r0.xxxx, r1.xyzw
   6: mad r1.xyzw, cb1[19].xyzw, r0.zzzz, r1.xyzw
   7: mov o4.xyz, r0.xyzx
   8: add o0.xyzw, r1.xyzw, cb1[20].xyzw
   9: mov o1.xyzw, v3.xyzw
  10: mov o2.xyzw, v4.xyzw
  11: mov o3.xyzw, v5.xyzw
  12: dp3 r0.x, v1.xyzx, cb0[4].xyzx
  13: dp3 r0.y, v1.xyzx, cb0[5].xyzx
  14: dp3 r0.z, v1.xyzx, cb0[6].xyzx
  15: dp3 r0.w, r0.xyzx, r0.xyzx
  16: max r0.w, r0.w, l(0.000000)
  17: rsq r0.w, r0.w
  18: mul o5.xyz, r0.wwww, r0.xyzx
  19: ret 
// Approximately 0 instruction slots used


-- Fragment shader for "d3d11":
// No shader variant for this keyword set. The closest match will be used instead.

//////////////////////////////////////////////////////
Keywords: DIRECTIONAL LIGHTPROBE_SH _BUILTIN_SURFACE_TYPE_TRANSPARENT
-- Vertex shader for "d3d11":
// No shader variant for this keyword set. The closest match will be used instead.

-- Hardware tier variant: Tier 1
-- Fragment shader for "d3d11":
// Stats: 73 math, 4 temp registers, 5 textures
Set 2D Texture "_CameraDepthTexture" to slot 0
Set 2D Texture "_MainTex" to slot 1
Set 2D Texture "_Noise" to slot 2
Set 2D Texture "_Flow" to slot 3
Set 2D Texture "_Mask" to slot 4

Constant Buffer "UnityPerCamera" (144 bytes) on slot 0 {
  Vector4 _Time at 0
  Vector3 _WorldSpaceCameraPos at 64
  Vector4 _ProjectionParams at 80
  Vector4 _ScreenParams at 96
  Vector4 _ZBufferParams at 112
  Vector4 unity_OrthoParams at 128
}
Constant Buffer "UnityPerFrame" (368 bytes) on slot 1 {
  Matrix4x4 unity_MatrixV at 144
  Matrix4x4 unity_MatrixVP at 272
}
Constant Buffer "UnityPerMaterial" (240 bytes) on slot 2 {
  Float _Textureopacity at 0
  Float _Fresnelpower at 4
  Float _Fresnelscale at 8
  Float _Multiply_texture at 12
  Float _Useonlycolor at 16
  Float _Use_fresnel at 20
  Vector4 _MainTex_ST at 48
  Vector4 _Noise_ST at 80
  Vector4 _Flow_ST at 112
  Vector4 _Mask_ST at 144
  Vector4 _SpeedMainTexUVNoiseZW at 160
  Vector4 _DistortionSpeedXYPowerZ at 176
  Float _Emission at 192
  Float _Opacity at 224
  Float _Usecenterglow at 228
  Float _Usedepth at 232
  Float _Depthpower at 236
}

Shader Disassembly:
//
// Generated by Microsoft (R) D3D Shader Disassembler
//
//
// Input signature:
//
// Name                 Index   Mask Register SysValue  Format   Used
// -------------------- ----- ------ -------- -------- ------- ------
// SV_POSITION              0   xyzw        0      POS   float   xy  
// INTERP                   0   xyzw        1     NONE   float   xyzw
// INTERP                   1   xyzw        2     NONE   float   x   
// INTERP                   2   xyzw        3     NONE   float   xyzw
// INTERP                   3   xyz         4     NONE   float   xyz 
// INTERP                   4   xyz         5     NONE   float   xyz 
// SV_IsFrontFace           0   x           6    FFACE    uint   x   
//
//
// Output signature:
//
// Name                 Index   Mask Register SysValue  Format   Used
// -------------------- ----- ------ -------- -------- ------- ------
// SV_TARGET                0   xyzw        0   TARGET   float   xyzw
//
      ps_4_0
      dcl_constantbuffer CB0[9], immediateIndexed
      dcl_constantbuffer CB1[21], immediateIndexed
      dcl_constantbuffer CB2[15], immediateIndexed
      dcl_sampler s0, mode_default
      dcl_sampler s1, mode_default
      dcl_sampler s2, mode_default
      dcl_sampler s3, mode_default
      dcl_sampler s4, mode_default
      dcl_resource_texture2d (float,float,float,float) t0
      dcl_resource_texture2d (float,float,float,float) t1
      dcl_resource_texture2d (float,float,float,float) t2
      dcl_resource_texture2d (float,float,float,float) t3
      dcl_resource_texture2d (float,float,float,float) t4
      dcl_input_ps_siv linear noperspective v0.xy, position
      dcl_input_ps linear v1.xyzw
      dcl_input_ps linear v2.x
      dcl_input_ps linear v3.xyzw
      dcl_input_ps linear v4.xyz
      dcl_input_ps linear v5.xyz
      dcl_input_ps_sgv constant v6.x, is_front_face
      dcl_output o0.xyzw
      dcl_temps 4
   0: dp3 r0.x, v5.xyzx, v5.xyzx
   1: sqrt r0.x, r0.x
   2: div r0.x, l(1.000000, 1.000000, 1.000000, 1.000000), r0.x
   3: mul r0.xyz, r0.xxxx, v5.xyzx
   4: dp3 r0.w, r0.xyzx, r0.xyzx
   5: rsq r0.w, r0.w
   6: mul r0.xyz, r0.wwww, r0.xyzx
   7: add r1.xyz, -v4.xyzx, cb0[4].xyzx
   8: dp3 r0.w, r1.xyzx, r1.xyzx
   9: rsq r0.w, r0.w
  10: mul r1.xyz, r0.wwww, r1.xyzx
  11: eq r0.w, cb0[8].w, l(0.000000)
  12: movc r2.x, r0.w, r1.x, cb1[9].z
  13: movc r2.y, r0.w, r1.y, cb1[10].z
  14: movc r2.z, r0.w, r1.z, cb1[11].z
  15: dp3_sat r0.x, r0.xyzx, r2.xyzx
  16: add r0.x, -r0.x, l(1.000000)
  17: log r0.x, r0.x
  18: mul r0.x, r0.x, cb2[0].y
  19: exp r0.x, r0.x
  20: mul_sat r0.x, r0.x, cb2[0].z
  21: and r0.x, r0.x, v6.x
  22: lt r0.y, cb0[5].x, l(0.000000)
  23: add r0.z, -v0.y, cb0[6].y
  24: movc r1.y, r0.y, r0.z, v0.y
  25: mov r1.x, v0.x
  26: div r1.xy, r1.xyxx, cb0[6].xyxx
  27: add r1.z, -r1.y, l(1.000000)
  28: sample r1.xyzw, r1.xzxx, t0.xyzw, s0
  29: mad r0.y, cb0[7].x, r1.x, cb0[7].y
  30: div r0.y, l(1.000000, 1.000000, 1.000000, 1.000000), r0.y
  31: mul r0.z, v4.y, cb1[18].w
  32: mad r0.z, cb1[17].w, v4.x, r0.z
  33: mad r0.z, cb1[19].w, v4.z, r0.z
  34: add r0.z, r0.z, cb1[20].w
  35: mad r0.y, r0.y, cb0[5].z, -r0.z
  36: div_sat r0.y, r0.y, cb2[14].w
  37: ge r0.z, l(0.000000), r0.y
  38: and r0.z, r0.z, l(0x3f800000)
  39: add r0.w, -r0.y, l(1.000000)
  40: mad r0.y, r0.z, r0.w, r0.y
  41: add r0.x, -r0.y, r0.x
  42: add r0.x, r0.x, l(1.000000)
  43: mul r0.z, v1.w, l(1.000100)
  44: ge r0.w, r0.z, -r0.z
  45: frc r0.z, |r0.z|
  46: movc r0.z, r0.w, r0.z, -r0.z
  47: mul r0.z, r0.z, l(0.999900)
  48: div r1.xy, cb2[10].zwzz, cb2[5].xyxx
  49: mad r0.zw, cb0[0].yyyy, r1.xxxy, r0.zzzz
  50: add r0.zw, r0.zzzw, v1.xxxy
  51: mad r0.zw, r0.zzzw, cb2[5].xxxy, cb2[5].zzzw
  52: sample r1.xyzw, r0.zwzz, t2.xyzw, s2
  53: div r0.zw, cb2[11].xxxy, cb2[7].xxxy
  54: mad r0.zw, r0.zzzw, cb0[0].yyyy, v1.xxxy
  55: mad r0.zw, r0.zzzw, cb2[7].xxxy, cb2[7].zzzw
  56: sample r2.xyzw, r0.zwzz, t3.xyzw, s3
  57: mad r0.zw, v1.xxxy, cb2[9].xxxy, cb2[9].zzzw
  58: sample r3.xyzw, r0.zwzz, t4.xyzw, s4
  59: mul r0.zw, r2.xxxy, r3.xxxy
  60: mul r0.zw, r0.zzzw, cb2[11].zzzz
  61: div r0.zw, r0.zzzw, cb2[3].xxxy
  62: div r2.xy, cb2[10].xyxx, cb2[3].xyxx
  63: mad r2.xy, r2.xyxx, cb0[0].yyyy, v1.xyxx
  64: add r0.zw, -r0.zzzw, r2.xxxy
  65: mad r0.zw, r0.zzzw, cb2[3].xxxy, cb2[3].zzzw
  66: sample r2.xyzw, r0.zwzz, t1.xyzw, s1
  67: mul r1.xyzw, r1.wxyz, r2.wxyz
  68: ne r0.zw, l(0.000000, 0.000000, 0.000000, 0.000000), cb2[1].xxxy
  69: movc r2.xyz, r0.zzzz, r1.xxxx, r1.yzwy
  70: max r2.xyz, r0.xxxx, r2.xyzx
  71: mad_sat r0.x, r1.x, cb2[0].x, r0.x
  72: movc r1.yzw, r0.wwww, r2.xxyz, r1.yyzw
  73: add r0.z, -v1.z, l(1.000000)
  74: add_sat r2.xyz, -r0.zzzz, r3.xyzx
  75: mul_sat r2.xyz, r2.xyzx, r3.xyzx
  76: mul r2.xyz, r1.yzwy, r2.xyzx
  77: ne r3.xy, l(0.000000, 0.000000, 0.000000, 0.000000), cb2[14].yzyy
  78: movc r1.yzw, r3.xxxx, r2.xxyz, r1.yyzw
  79: mul r1.yzw, r1.yyzw, cb2[12].xxxx
  80: mul r1.yzw, r1.yyzw, v3.xxyz
  81: max r0.z, v2.x, l(1.000000)
  82: min r0.z, r0.z, l(1000000.000000)
  83: eq r2.x, v1.x, v2.x
  84: movc r0.z, r2.x, l(1.000000), r0.z
  85: mul o0.xyz, r1.yzwy, r0.zzzz
  86: mul r0.z, r0.x, r1.x
  87: ne r1.y, l(0.000000, 0.000000, 0.000000, 0.000000), cb2[0].w
  88: movc r0.x, r1.y, r0.z, r0.x
  89: movc r0.x, r0.w, r0.x, r1.x
  90: mul r0.y, r0.y, r0.x
  91: movc r0.x, r3.y, r0.y, r0.x
  92: mul r0.x, r0.x, v3.w
  93: mul_sat o0.w, r0.x, cb2[14].x
  94: ret 
// Approximately 0 instruction slots used


//////////////////////////////////////////////////////
Keywords: DIRECTIONAL LIGHTPROBE_SH
-- Vertex shader for "d3d11":
// No shader variant for this keyword set. The closest match will be used instead.

-- Hardware tier variant: Tier 1
-- Fragment shader for "d3d11":
// Stats: 68 math, 4 temp registers, 5 textures
Set 2D Texture "_CameraDepthTexture" to slot 0
Set 2D Texture "_MainTex" to slot 1
Set 2D Texture "_Noise" to slot 2
Set 2D Texture "_Flow" to slot 3
Set 2D Texture "_Mask" to slot 4

Constant Buffer "UnityPerCamera" (144 bytes) on slot 0 {
  Vector4 _Time at 0
  Vector3 _WorldSpaceCameraPos at 64
  Vector4 _ProjectionParams at 80
  Vector4 _ScreenParams at 96
  Vector4 _ZBufferParams at 112
  Vector4 unity_OrthoParams at 128
}
Constant Buffer "UnityPerFrame" (368 bytes) on slot 1 {
  Matrix4x4 unity_MatrixV at 144
  Matrix4x4 unity_MatrixVP at 272
}
Constant Buffer "UnityPerMaterial" (240 bytes) on slot 2 {
  Float _Fresnelpower at 4
  Float _Fresnelscale at 8
  Float _Useonlycolor at 16
  Float _Use_fresnel at 20
  Vector4 _MainTex_ST at 48
  Vector4 _Noise_ST at 80
  Vector4 _Flow_ST at 112
  Vector4 _Mask_ST at 144
  Vector4 _SpeedMainTexUVNoiseZW at 160
  Vector4 _DistortionSpeedXYPowerZ at 176
  Float _Emission at 192
  Float _Usecenterglow at 228
  Float _Depthpower at 236
}

Shader Disassembly:
//
// Generated by Microsoft (R) D3D Shader Disassembler
//
//
// Input signature:
//
// Name                 Index   Mask Register SysValue  Format   Used
// -------------------- ----- ------ -------- -------- ------- ------
// SV_POSITION              0   xyzw        0      POS   float   xy  
// INTERP                   0   xyzw        1     NONE   float   xyzw
// INTERP                   1   xyzw        2     NONE   float   x   
// INTERP                   2   xyzw        3     NONE   float   xyz 
// INTERP                   3   xyz         4     NONE   float   xyz 
// INTERP                   4   xyz         5     NONE   float   xyz 
// SV_IsFrontFace           0   x           6    FFACE    uint   x   
//
//
// Output signature:
//
// Name                 Index   Mask Register SysValue  Format   Used
// -------------------- ----- ------ -------- -------- ------- ------
// SV_TARGET                0   xyzw        0   TARGET   float   xyzw
//
      ps_4_0
      dcl_constantbuffer CB0[9], immediateIndexed
      dcl_constantbuffer CB1[21], immediateIndexed
      dcl_constantbuffer CB2[15], immediateIndexed
      dcl_sampler s0, mode_default
      dcl_sampler s1, mode_default
      dcl_sampler s2, mode_default
      dcl_sampler s3, mode_default
      dcl_sampler s4, mode_default
      dcl_resource_texture2d (float,float,float,float) t0
      dcl_resource_texture2d (float,float,float,float) t1
      dcl_resource_texture2d (float,float,float,float) t2
      dcl_resource_texture2d (float,float,float,float) t3
      dcl_resource_texture2d (float,float,float,float) t4
      dcl_input_ps_siv linear noperspective v0.xy, position
      dcl_input_ps linear v1.xyzw
      dcl_input_ps linear v2.x
      dcl_input_ps linear v3.xyz
      dcl_input_ps linear v4.xyz
      dcl_input_ps linear v5.xyz
      dcl_input_ps_sgv constant v6.x, is_front_face
      dcl_output o0.xyzw
      dcl_temps 4
   0: dp3 r0.x, v5.xyzx, v5.xyzx
   1: sqrt r0.x, r0.x
   2: div r0.x, l(1.000000, 1.000000, 1.000000, 1.000000), r0.x
   3: mul r0.xyz, r0.xxxx, v5.xyzx
   4: dp3 r0.w, r0.xyzx, r0.xyzx
   5: rsq r0.w, r0.w
   6: mul r0.xyz, r0.wwww, r0.xyzx
   7: add r1.xyz, -v4.xyzx, cb0[4].xyzx
   8: dp3 r0.w, r1.xyzx, r1.xyzx
   9: rsq r0.w, r0.w
  10: mul r1.xyz, r0.wwww, r1.xyzx
  11: eq r0.w, cb0[8].w, l(0.000000)
  12: movc r2.x, r0.w, r1.x, cb1[9].z
  13: movc r2.y, r0.w, r1.y, cb1[10].z
  14: movc r2.z, r0.w, r1.z, cb1[11].z
  15: dp3_sat r0.x, r0.xyzx, r2.xyzx
  16: add r0.x, -r0.x, l(1.000000)
  17: log r0.x, r0.x
  18: mul r0.x, r0.x, cb2[0].y
  19: exp r0.x, r0.x
  20: mul_sat r0.x, r0.x, cb2[0].z
  21: and r0.x, r0.x, v6.x
  22: lt r0.y, cb0[5].x, l(0.000000)
  23: add r0.z, -v0.y, cb0[6].y
  24: movc r1.y, r0.y, r0.z, v0.y
  25: mov r1.x, v0.x
  26: div r1.xy, r1.xyxx, cb0[6].xyxx
  27: add r1.z, -r1.y, l(1.000000)
  28: sample r1.xyzw, r1.xzxx, t0.xyzw, s0
  29: mad r0.y, cb0[7].x, r1.x, cb0[7].y
  30: div r0.y, l(1.000000, 1.000000, 1.000000, 1.000000), r0.y
  31: mul r0.z, v4.y, cb1[18].w
  32: mad r0.z, cb1[17].w, v4.x, r0.z
  33: mad r0.z, cb1[19].w, v4.z, r0.z
  34: add r0.z, r0.z, cb1[20].w
  35: mad r0.y, r0.y, cb0[5].z, -r0.z
  36: div_sat r0.y, r0.y, cb2[14].w
  37: ge r0.z, l(0.000000), r0.y
  38: and r0.z, r0.z, l(0x3f800000)
  39: add r0.w, -r0.y, l(1.000000)
  40: mad r0.y, r0.z, r0.w, r0.y
  41: add r0.x, -r0.y, r0.x
  42: add r0.x, r0.x, l(1.000000)
  43: mul r0.y, v1.w, l(1.000100)
  44: ge r0.z, r0.y, -r0.y
  45: frc r0.y, |r0.y|
  46: movc r0.y, r0.z, r0.y, -r0.y
  47: mul r0.y, r0.y, l(0.999900)
  48: div r0.zw, cb2[10].zzzw, cb2[5].xxxy
  49: mad r0.yz, cb0[0].yyyy, r0.zzwz, r0.yyyy
  50: add r0.yz, r0.yyzy, v1.xxyx
  51: mad r0.yz, r0.yyzy, cb2[5].xxyx, cb2[5].zzwz
  52: sample r1.xyzw, r0.yzyy, t2.xyzw, s2
  53: div r0.yz, cb2[11].xxyx, cb2[7].xxyx
  54: mad r0.yz, r0.yyzy, cb0[0].yyyy, v1.xxyx
  55: mad r0.yz, r0.yyzy, cb2[7].xxyx, cb2[7].zzwz
  56: sample r2.xyzw, r0.yzyy, t3.xyzw, s3
  57: mad r0.yz, v1.xxyx, cb2[9].xxyx, cb2[9].zzwz
  58: sample r3.xyzw, r0.yzyy, t4.xyzw, s4
  59: mul r0.yz, r2.xxyx, r3.xxyx
  60: mul r0.yz, r0.yyzy, cb2[11].zzzz
  61: div r0.yz, r0.yyzy, cb2[3].xxyx
  62: div r2.xy, cb2[10].xyxx, cb2[3].xyxx
  63: mad r2.xy, r2.xyxx, cb0[0].yyyy, v1.xyxx
  64: add r0.yz, -r0.yyzy, r2.xxyx
  65: mad r0.yz, r0.yyzy, cb2[3].xxyx, cb2[3].zzwz
  66: sample r2.xyzw, r0.yzyy, t1.xyzw, s1
  67: mul r1.xyzw, r1.wxyz, r2.wxyz
  68: ne r0.yz, l(0.000000, 0.000000, 0.000000, 0.000000), cb2[1].xxyx
  69: movc r2.xyz, r0.yyyy, r1.xxxx, r1.yzwy
  70: max r0.xyw, r0.xxxx, r2.xyxz
  71: movc r0.xyz, r0.zzzz, r0.xywx, r1.yzwy
  72: add r0.w, -v1.z, l(1.000000)
  73: add_sat r1.xyz, -r0.wwww, r3.xyzx
  74: mul_sat r1.xyz, r1.xyzx, r3.xyzx
  75: mul r1.xyz, r0.xyzx, r1.xyzx
  76: ne r0.w, l(0.000000, 0.000000, 0.000000, 0.000000), cb2[14].y
  77: movc r0.xyz, r0.wwww, r1.xyzx, r0.xyzx
  78: mul r0.xyz, r0.xyzx, cb2[12].xxxx
  79: mul r0.xyz, r0.xyzx, v3.xyzx
  80: max r0.w, v2.x, l(1.000000)
  81: min r0.w, r0.w, l(1000000.000000)
  82: eq r1.x, v1.x, v2.x
  83: movc r0.w, r1.x, l(1.000000), r0.w
  84: mul o0.xyz, r0.xyzx, r0.wwww
  85: mov o0.w, l(1.000000)
  86: ret 
// Approximately 0 instruction slots used


//////////////////////////////////////////////////////
Keywords: DIRECTIONAL SHADOWS_SCREEN _BUILTIN_SURFACE_TYPE_TRANSPARENT
-- Vertex shader for "d3d11":
// No shader variant for this keyword set. The closest match will be used instead.

-- Hardware tier variant: Tier 1
-- Fragment shader for "d3d11":
// Stats: 73 math, 4 temp registers, 5 textures
Set 2D Texture "_CameraDepthTexture" to slot 0
Set 2D Texture "_MainTex" to slot 1
Set 2D Texture "_Noise" to slot 2
Set 2D Texture "_Flow" to slot 3
Set 2D Texture "_Mask" to slot 4

Constant Buffer "UnityPerCamera" (144 bytes) on slot 0 {
  Vector4 _Time at 0
  Vector3 _WorldSpaceCameraPos at 64
  Vector4 _ProjectionParams at 80
  Vector4 _ScreenParams at 96
  Vector4 _ZBufferParams at 112
  Vector4 unity_OrthoParams at 128
}
Constant Buffer "UnityPerFrame" (368 bytes) on slot 1 {
  Matrix4x4 unity_MatrixV at 144
  Matrix4x4 unity_MatrixVP at 272
}
Constant Buffer "UnityPerMaterial" (240 bytes) on slot 2 {
  Float _Textureopacity at 0
  Float _Fresnelpower at 4
  Float _Fresnelscale at 8
  Float _Multiply_texture at 12
  Float _Useonlycolor at 16
  Float _Use_fresnel at 20
  Vector4 _MainTex_ST at 48
  Vector4 _Noise_ST at 80
  Vector4 _Flow_ST at 112
  Vector4 _Mask_ST at 144
  Vector4 _SpeedMainTexUVNoiseZW at 160
  Vector4 _DistortionSpeedXYPowerZ at 176
  Float _Emission at 192
  Float _Opacity at 224
  Float _Usecenterglow at 228
  Float _Usedepth at 232
  Float _Depthpower at 236
}

Shader Disassembly:
//
// Generated by Microsoft (R) D3D Shader Disassembler
//
//
// Input signature:
//
// Name                 Index   Mask Register SysValue  Format   Used
// -------------------- ----- ------ -------- -------- ------- ------
// SV_POSITION              0   xyzw        0      POS   float   xy  
// INTERP                   0   xyzw        1     NONE   float   xyzw
// INTERP                   1   xyzw        2     NONE   float   x   
// INTERP                   2   xyzw        3     NONE   float   xyzw
// INTERP                   3   xyz         4     NONE   float   xyz 
// INTERP                   4   xyz         5     NONE   float   xyz 
// SV_IsFrontFace           0   x           6    FFACE    uint   x   
//
//
// Output signature:
//
// Name                 Index   Mask Register SysValue  Format   Used
// -------------------- ----- ------ -------- -------- ------- ------
// SV_TARGET                0   xyzw        0   TARGET   float   xyzw
//
      ps_4_0
      dcl_constantbuffer CB0[9], immediateIndexed
      dcl_constantbuffer CB1[21], immediateIndexed
      dcl_constantbuffer CB2[15], immediateIndexed
      dcl_sampler s0, mode_default
      dcl_sampler s1, mode_default
      dcl_sampler s2, mode_default
      dcl_sampler s3, mode_default
      dcl_sampler s4, mode_default
      dcl_resource_texture2d (float,float,float,float) t0
      dcl_resource_texture2d (float,float,float,float) t1
      dcl_resource_texture2d (float,float,float,float) t2
      dcl_resource_texture2d (float,float,float,float) t3
      dcl_resource_texture2d (float,float,float,float) t4
      dcl_input_ps_siv linear noperspective v0.xy, position
      dcl_input_ps linear v1.xyzw
      dcl_input_ps linear v2.x
      dcl_input_ps linear v3.xyzw
      dcl_input_ps linear v4.xyz
      dcl_input_ps linear v5.xyz
      dcl_input_ps_sgv constant v6.x, is_front_face
      dcl_output o0.xyzw
      dcl_temps 4
   0: dp3 r0.x, v5.xyzx, v5.xyzx
   1: sqrt r0.x, r0.x
   2: div r0.x, l(1.000000, 1.000000, 1.000000, 1.000000), r0.x
   3: mul r0.xyz, r0.xxxx, v5.xyzx
   4: dp3 r0.w, r0.xyzx, r0.xyzx
   5: rsq r0.w, r0.w
   6: mul r0.xyz, r0.wwww, r0.xyzx
   7: add r1.xyz, -v4.xyzx, cb0[4].xyzx
   8: dp3 r0.w, r1.xyzx, r1.xyzx
   9: rsq r0.w, r0.w
  10: mul r1.xyz, r0.wwww, r1.xyzx
  11: eq r0.w, cb0[8].w, l(0.000000)
  12: movc r2.x, r0.w, r1.x, cb1[9].z
  13: movc r2.y, r0.w, r1.y, cb1[10].z
  14: movc r2.z, r0.w, r1.z, cb1[11].z
  15: dp3_sat r0.x, r0.xyzx, r2.xyzx
  16: add r0.x, -r0.x, l(1.000000)
  17: log r0.x, r0.x
  18: mul r0.x, r0.x, cb2[0].y
  19: exp r0.x, r0.x
  20: mul_sat r0.x, r0.x, cb2[0].z
  21: and r0.x, r0.x, v6.x
  22: lt r0.y, cb0[5].x, l(0.000000)
  23: add r0.z, -v0.y, cb0[6].y
  24: movc r1.y, r0.y, r0.z, v0.y
  25: mov r1.x, v0.x
  26: div r1.xy, r1.xyxx, cb0[6].xyxx
  27: add r1.z, -r1.y, l(1.000000)
  28: sample r1.xyzw, r1.xzxx, t0.xyzw, s0
  29: mad r0.y, cb0[7].x, r1.x, cb0[7].y
  30: div r0.y, l(1.000000, 1.000000, 1.000000, 1.000000), r0.y
  31: mul r0.z, v4.y, cb1[18].w
  32: mad r0.z, cb1[17].w, v4.x, r0.z
  33: mad r0.z, cb1[19].w, v4.z, r0.z
  34: add r0.z, r0.z, cb1[20].w
  35: mad r0.y, r0.y, cb0[5].z, -r0.z
  36: div_sat r0.y, r0.y, cb2[14].w
  37: ge r0.z, l(0.000000), r0.y
  38: and r0.z, r0.z, l(0x3f800000)
  39: add r0.w, -r0.y, l(1.000000)
  40: mad r0.y, r0.z, r0.w, r0.y
  41: add r0.x, -r0.y, r0.x
  42: add r0.x, r0.x, l(1.000000)
  43: mul r0.z, v1.w, l(1.000100)
  44: ge r0.w, r0.z, -r0.z
  45: frc r0.z, |r0.z|
  46: movc r0.z, r0.w, r0.z, -r0.z
  47: mul r0.z, r0.z, l(0.999900)
  48: div r1.xy, cb2[10].zwzz, cb2[5].xyxx
  49: mad r0.zw, cb0[0].yyyy, r1.xxxy, r0.zzzz
  50: add r0.zw, r0.zzzw, v1.xxxy
  51: mad r0.zw, r0.zzzw, cb2[5].xxxy, cb2[5].zzzw
  52: sample r1.xyzw, r0.zwzz, t2.xyzw, s2
  53: div r0.zw, cb2[11].xxxy, cb2[7].xxxy
  54: mad r0.zw, r0.zzzw, cb0[0].yyyy, v1.xxxy
  55: mad r0.zw, r0.zzzw, cb2[7].xxxy, cb2[7].zzzw
  56: sample r2.xyzw, r0.zwzz, t3.xyzw, s3
  57: mad r0.zw, v1.xxxy, cb2[9].xxxy, cb2[9].zzzw
  58: sample r3.xyzw, r0.zwzz, t4.xyzw, s4
  59: mul r0.zw, r2.xxxy, r3.xxxy
  60: mul r0.zw, r0.zzzw, cb2[11].zzzz
  61: div r0.zw, r0.zzzw, cb2[3].xxxy
  62: div r2.xy, cb2[10].xyxx, cb2[3].xyxx
  63: mad r2.xy, r2.xyxx, cb0[0].yyyy, v1.xyxx
  64: add r0.zw, -r0.zzzw, r2.xxxy
  65: mad r0.zw, r0.zzzw, cb2[3].xxxy, cb2[3].zzzw
  66: sample r2.xyzw, r0.zwzz, t1.xyzw, s1
  67: mul r1.xyzw, r1.wxyz, r2.wxyz
  68: ne r0.zw, l(0.000000, 0.000000, 0.000000, 0.000000), cb2[1].xxxy
  69: movc r2.xyz, r0.zzzz, r1.xxxx, r1.yzwy
  70: max r2.xyz, r0.xxxx, r2.xyzx
  71: mad_sat r0.x, r1.x, cb2[0].x, r0.x
  72: movc r1.yzw, r0.wwww, r2.xxyz, r1.yyzw
  73: add r0.z, -v1.z, l(1.000000)
  74: add_sat r2.xyz, -r0.zzzz, r3.xyzx
  75: mul_sat r2.xyz, r2.xyzx, r3.xyzx
  76: mul r2.xyz, r1.yzwy, r2.xyzx
  77: ne r3.xy, l(0.000000, 0.000000, 0.000000, 0.000000), cb2[14].yzyy
  78: movc r1.yzw, r3.xxxx, r2.xxyz, r1.yyzw
  79: mul r1.yzw, r1.yyzw, cb2[12].xxxx
  80: mul r1.yzw, r1.yyzw, v3.xxyz
  81: max r0.z, v2.x, l(1.000000)
  82: min r0.z, r0.z, l(1000000.000000)
  83: eq r2.x, v1.x, v2.x
  84: movc r0.z, r2.x, l(1.000000), r0.z
  85: mul o0.xyz, r1.yzwy, r0.zzzz
  86: mul r0.z, r0.x, r1.x
  87: ne r1.y, l(0.000000, 0.000000, 0.000000, 0.000000), cb2[0].w
  88: movc r0.x, r1.y, r0.z, r0.x
  89: movc r0.x, r0.w, r0.x, r1.x
  90: mul r0.y, r0.y, r0.x
  91: movc r0.x, r3.y, r0.y, r0.x
  92: mul r0.x, r0.x, v3.w
  93: mul_sat o0.w, r0.x, cb2[14].x
  94: ret 
// Approximately 0 instruction slots used


//////////////////////////////////////////////////////
Keywords: DIRECTIONAL SHADOWS_SCREEN
-- Vertex shader for "d3d11":
// No shader variant for this keyword set. The closest match will be used instead.

-- Hardware tier variant: Tier 1
-- Fragment shader for "d3d11":
// Stats: 68 math, 4 temp registers, 5 textures
Set 2D Texture "_CameraDepthTexture" to slot 0
Set 2D Texture "_MainTex" to slot 1
Set 2D Texture "_Noise" to slot 2
Set 2D Texture "_Flow" to slot 3
Set 2D Texture "_Mask" to slot 4

Constant Buffer "UnityPerCamera" (144 bytes) on slot 0 {
  Vector4 _Time at 0
  Vector3 _WorldSpaceCameraPos at 64
  Vector4 _ProjectionParams at 80
  Vector4 _ScreenParams at 96
  Vector4 _ZBufferParams at 112
  Vector4 unity_OrthoParams at 128
}
Constant Buffer "UnityPerFrame" (368 bytes) on slot 1 {
  Matrix4x4 unity_MatrixV at 144
  Matrix4x4 unity_MatrixVP at 272
}
Constant Buffer "UnityPerMaterial" (240 bytes) on slot 2 {
  Float _Fresnelpower at 4
  Float _Fresnelscale at 8
  Float _Useonlycolor at 16
  Float _Use_fresnel at 20
  Vector4 _MainTex_ST at 48
  Vector4 _Noise_ST at 80
  Vector4 _Flow_ST at 112
  Vector4 _Mask_ST at 144
  Vector4 _SpeedMainTexUVNoiseZW at 160
  Vector4 _DistortionSpeedXYPowerZ at 176
  Float _Emission at 192
  Float _Usecenterglow at 228
  Float _Depthpower at 236
}

Shader Disassembly:
//
// Generated by Microsoft (R) D3D Shader Disassembler
//
//
// Input signature:
//
// Name                 Index   Mask Register SysValue  Format   Used
// -------------------- ----- ------ -------- -------- ------- ------
// SV_POSITION              0   xyzw        0      POS   float   xy  
// INTERP                   0   xyzw        1     NONE   float   xyzw
// INTERP                   1   xyzw        2     NONE   float   x   
// INTERP                   2   xyzw        3     NONE   float   xyz 
// INTERP                   3   xyz         4     NONE   float   xyz 
// INTERP                   4   xyz         5     NONE   float   xyz 
// SV_IsFrontFace           0   x           6    FFACE    uint   x   
//
//
// Output signature:
//
// Name                 Index   Mask Register SysValue  Format   Used
// -------------------- ----- ------ -------- -------- ------- ------
// SV_TARGET                0   xyzw        0   TARGET   float   xyzw
//
      ps_4_0
      dcl_constantbuffer CB0[9], immediateIndexed
      dcl_constantbuffer CB1[21], immediateIndexed
      dcl_constantbuffer CB2[15], immediateIndexed
      dcl_sampler s0, mode_default
      dcl_sampler s1, mode_default
      dcl_sampler s2, mode_default
      dcl_sampler s3, mode_default
      dcl_sampler s4, mode_default
      dcl_resource_texture2d (float,float,float,float) t0
      dcl_resource_texture2d (float,float,float,float) t1
      dcl_resource_texture2d (float,float,float,float) t2
      dcl_resource_texture2d (float,float,float,float) t3
      dcl_resource_texture2d (float,float,float,float) t4
      dcl_input_ps_siv linear noperspective v0.xy, position
      dcl_input_ps linear v1.xyzw
      dcl_input_ps linear v2.x
      dcl_input_ps linear v3.xyz
      dcl_input_ps linear v4.xyz
      dcl_input_ps linear v5.xyz
      dcl_input_ps_sgv constant v6.x, is_front_face
      dcl_output o0.xyzw
      dcl_temps 4
   0: dp3 r0.x, v5.xyzx, v5.xyzx
   1: sqrt r0.x, r0.x
   2: div r0.x, l(1.000000, 1.000000, 1.000000, 1.000000), r0.x
   3: mul r0.xyz, r0.xxxx, v5.xyzx
   4: dp3 r0.w, r0.xyzx, r0.xyzx
   5: rsq r0.w, r0.w
   6: mul r0.xyz, r0.wwww, r0.xyzx
   7: add r1.xyz, -v4.xyzx, cb0[4].xyzx
   8: dp3 r0.w, r1.xyzx, r1.xyzx
   9: rsq r0.w, r0.w
  10: mul r1.xyz, r0.wwww, r1.xyzx
  11: eq r0.w, cb0[8].w, l(0.000000)
  12: movc r2.x, r0.w, r1.x, cb1[9].z
  13: movc r2.y, r0.w, r1.y, cb1[10].z
  14: movc r2.z, r0.w, r1.z, cb1[11].z
  15: dp3_sat r0.x, r0.xyzx, r2.xyzx
  16: add r0.x, -r0.x, l(1.000000)
  17: log r0.x, r0.x
  18: mul r0.x, r0.x, cb2[0].y
  19: exp r0.x, r0.x
  20: mul_sat r0.x, r0.x, cb2[0].z
  21: and r0.x, r0.x, v6.x
  22: lt r0.y, cb0[5].x, l(0.000000)
  23: add r0.z, -v0.y, cb0[6].y
  24: movc r1.y, r0.y, r0.z, v0.y
  25: mov r1.x, v0.x
  26: div r1.xy, r1.xyxx, cb0[6].xyxx
  27: add r1.z, -r1.y, l(1.000000)
  28: sample r1.xyzw, r1.xzxx, t0.xyzw, s0
  29: mad r0.y, cb0[7].x, r1.x, cb0[7].y
  30: div r0.y, l(1.000000, 1.000000, 1.000000, 1.000000), r0.y
  31: mul r0.z, v4.y, cb1[18].w
  32: mad r0.z, cb1[17].w, v4.x, r0.z
  33: mad r0.z, cb1[19].w, v4.z, r0.z
  34: add r0.z, r0.z, cb1[20].w
  35: mad r0.y, r0.y, cb0[5].z, -r0.z
  36: div_sat r0.y, r0.y, cb2[14].w
  37: ge r0.z, l(0.000000), r0.y
  38: and r0.z, r0.z, l(0x3f800000)
  39: add r0.w, -r0.y, l(1.000000)
  40: mad r0.y, r0.z, r0.w, r0.y
  41: add r0.x, -r0.y, r0.x
  42: add r0.x, r0.x, l(1.000000)
  43: mul r0.y, v1.w, l(1.000100)
  44: ge r0.z, r0.y, -r0.y
  45: frc r0.y, |r0.y|
  46: movc r0.y, r0.z, r0.y, -r0.y
  47: mul r0.y, r0.y, l(0.999900)
  48: div r0.zw, cb2[10].zzzw, cb2[5].xxxy
  49: mad r0.yz, cb0[0].yyyy, r0.zzwz, r0.yyyy
  50: add r0.yz, r0.yyzy, v1.xxyx
  51: mad r0.yz, r0.yyzy, cb2[5].xxyx, cb2[5].zzwz
  52: sample r1.xyzw, r0.yzyy, t2.xyzw, s2
  53: div r0.yz, cb2[11].xxyx, cb2[7].xxyx
  54: mad r0.yz, r0.yyzy, cb0[0].yyyy, v1.xxyx
  55: mad r0.yz, r0.yyzy, cb2[7].xxyx, cb2[7].zzwz
  56: sample r2.xyzw, r0.yzyy, t3.xyzw, s3
  57: mad r0.yz, v1.xxyx, cb2[9].xxyx, cb2[9].zzwz
  58: sample r3.xyzw, r0.yzyy, t4.xyzw, s4
  59: mul r0.yz, r2.xxyx, r3.xxyx
  60: mul r0.yz, r0.yyzy, cb2[11].zzzz
  61: div r0.yz, r0.yyzy, cb2[3].xxyx
  62: div r2.xy, cb2[10].xyxx, cb2[3].xyxx
  63: mad r2.xy, r2.xyxx, cb0[0].yyyy, v1.xyxx
  64: add r0.yz, -r0.yyzy, r2.xxyx
  65: mad r0.yz, r0.yyzy, cb2[3].xxyx, cb2[3].zzwz
  66: sample r2.xyzw, r0.yzyy, t1.xyzw, s1
  67: mul r1.xyzw, r1.wxyz, r2.wxyz
  68: ne r0.yz, l(0.000000, 0.000000, 0.000000, 0.000000), cb2[1].xxyx
  69: movc r2.xyz, r0.yyyy, r1.xxxx, r1.yzwy
  70: max r0.xyw, r0.xxxx, r2.xyxz
  71: movc r0.xyz, r0.zzzz, r0.xywx, r1.yzwy
  72: add r0.w, -v1.z, l(1.000000)
  73: add_sat r1.xyz, -r0.wwww, r3.xyzx
  74: mul_sat r1.xyz, r1.xyzx, r3.xyzx
  75: mul r1.xyz, r0.xyzx, r1.xyzx
  76: ne r0.w, l(0.000000, 0.000000, 0.000000, 0.000000), cb2[14].y
  77: movc r0.xyz, r0.wwww, r1.xyzx, r0.xyzx
  78: mul r0.xyz, r0.xyzx, cb2[12].xxxx
  79: mul r0.xyz, r0.xyzx, v3.xyzx
  80: max r0.w, v2.x, l(1.000000)
  81: min r0.w, r0.w, l(1000000.000000)
  82: eq r1.x, v1.x, v2.x
  83: movc r0.w, r1.x, l(1.000000), r0.w
  84: mul o0.xyz, r0.xyzx, r0.wwww
  85: mov o0.w, l(1.000000)
  86: ret 
// Approximately 0 instruction slots used


//////////////////////////////////////////////////////
Keywords: DIRECTIONAL LIGHTPROBE_SH SHADOWS_SCREEN _BUILTIN_SURFACE_TYPE_TRANSPARENT
-- Vertex shader for "d3d11":
// No shader variant for this keyword set. The closest match will be used instead.

-- Hardware tier variant: Tier 1
-- Fragment shader for "d3d11":
// Stats: 73 math, 4 temp registers, 5 textures
Set 2D Texture "_CameraDepthTexture" to slot 0
Set 2D Texture "_MainTex" to slot 1
Set 2D Texture "_Noise" to slot 2
Set 2D Texture "_Flow" to slot 3
Set 2D Texture "_Mask" to slot 4

Constant Buffer "UnityPerCamera" (144 bytes) on slot 0 {
  Vector4 _Time at 0
  Vector3 _WorldSpaceCameraPos at 64
  Vector4 _ProjectionParams at 80
  Vector4 _ScreenParams at 96
  Vector4 _ZBufferParams at 112
  Vector4 unity_OrthoParams at 128
}
Constant Buffer "UnityPerFrame" (368 bytes) on slot 1 {
  Matrix4x4 unity_MatrixV at 144
  Matrix4x4 unity_MatrixVP at 272
}
Constant Buffer "UnityPerMaterial" (240 bytes) on slot 2 {
  Float _Textureopacity at 0
  Float _Fresnelpower at 4
  Float _Fresnelscale at 8
  Float _Multiply_texture at 12
  Float _Useonlycolor at 16
  Float _Use_fresnel at 20
  Vector4 _MainTex_ST at 48
  Vector4 _Noise_ST at 80
  Vector4 _Flow_ST at 112
  Vector4 _Mask_ST at 144
  Vector4 _SpeedMainTexUVNoiseZW at 160
  Vector4 _DistortionSpeedXYPowerZ at 176
  Float _Emission at 192
  Float _Opacity at 224
  Float _Usecenterglow at 228
  Float _Usedepth at 232
  Float _Depthpower at 236
}

Shader Disassembly:
//
// Generated by Microsoft (R) D3D Shader Disassembler
//
//
// Input signature:
//
// Name                 Index   Mask Register SysValue  Format   Used
// -------------------- ----- ------ -------- -------- ------- ------
// SV_POSITION              0   xyzw        0      POS   float   xy  
// INTERP                   0   xyzw        1     NONE   float   xyzw
// INTERP                   1   xyzw        2     NONE   float   x   
// INTERP                   2   xyzw        3     NONE   float   xyzw
// INTERP                   3   xyz         4     NONE   float   xyz 
// INTERP                   4   xyz         5     NONE   float   xyz 
// SV_IsFrontFace           0   x           6    FFACE    uint   x   
//
//
// Output signature:
//
// Name                 Index   Mask Register SysValue  Format   Used
// -------------------- ----- ------ -------- -------- ------- ------
// SV_TARGET                0   xyzw        0   TARGET   float   xyzw
//
      ps_4_0
      dcl_constantbuffer CB0[9], immediateIndexed
      dcl_constantbuffer CB1[21], immediateIndexed
      dcl_constantbuffer CB2[15], immediateIndexed
      dcl_sampler s0, mode_default
      dcl_sampler s1, mode_default
      dcl_sampler s2, mode_default
      dcl_sampler s3, mode_default
      dcl_sampler s4, mode_default
      dcl_resource_texture2d (float,float,float,float) t0
      dcl_resource_texture2d (float,float,float,float) t1
      dcl_resource_texture2d (float,float,float,float) t2
      dcl_resource_texture2d (float,float,float,float) t3
      dcl_resource_texture2d (float,float,float,float) t4
      dcl_input_ps_siv linear noperspective v0.xy, position
      dcl_input_ps linear v1.xyzw
      dcl_input_ps linear v2.x
      dcl_input_ps linear v3.xyzw
      dcl_input_ps linear v4.xyz
      dcl_input_ps linear v5.xyz
      dcl_input_ps_sgv constant v6.x, is_front_face
      dcl_output o0.xyzw
      dcl_temps 4
   0: dp3 r0.x, v5.xyzx, v5.xyzx
   1: sqrt r0.x, r0.x
   2: div r0.x, l(1.000000, 1.000000, 1.000000, 1.000000), r0.x
   3: mul r0.xyz, r0.xxxx, v5.xyzx
   4: dp3 r0.w, r0.xyzx, r0.xyzx
   5: rsq r0.w, r0.w
   6: mul r0.xyz, r0.wwww, r0.xyzx
   7: add r1.xyz, -v4.xyzx, cb0[4].xyzx
   8: dp3 r0.w, r1.xyzx, r1.xyzx
   9: rsq r0.w, r0.w
  10: mul r1.xyz, r0.wwww, r1.xyzx
  11: eq r0.w, cb0[8].w, l(0.000000)
  12: movc r2.x, r0.w, r1.x, cb1[9].z
  13: movc r2.y, r0.w, r1.y, cb1[10].z
  14: movc r2.z, r0.w, r1.z, cb1[11].z
  15: dp3_sat r0.x, r0.xyzx, r2.xyzx
  16: add r0.x, -r0.x, l(1.000000)
  17: log r0.x, r0.x
  18: mul r0.x, r0.x, cb2[0].y
  19: exp r0.x, r0.x
  20: mul_sat r0.x, r0.x, cb2[0].z
  21: and r0.x, r0.x, v6.x
  22: lt r0.y, cb0[5].x, l(0.000000)
  23: add r0.z, -v0.y, cb0[6].y
  24: movc r1.y, r0.y, r0.z, v0.y
  25: mov r1.x, v0.x
  26: div r1.xy, r1.xyxx, cb0[6].xyxx
  27: add r1.z, -r1.y, l(1.000000)
  28: sample r1.xyzw, r1.xzxx, t0.xyzw, s0
  29: mad r0.y, cb0[7].x, r1.x, cb0[7].y
  30: div r0.y, l(1.000000, 1.000000, 1.000000, 1.000000), r0.y
  31: mul r0.z, v4.y, cb1[18].w
  32: mad r0.z, cb1[17].w, v4.x, r0.z
  33: mad r0.z, cb1[19].w, v4.z, r0.z
  34: add r0.z, r0.z, cb1[20].w
  35: mad r0.y, r0.y, cb0[5].z, -r0.z
  36: div_sat r0.y, r0.y, cb2[14].w
  37: ge r0.z, l(0.000000), r0.y
  38: and r0.z, r0.z, l(0x3f800000)
  39: add r0.w, -r0.y, l(1.000000)
  40: mad r0.y, r0.z, r0.w, r0.y
  41: add r0.x, -r0.y, r0.x
  42: add r0.x, r0.x, l(1.000000)
  43: mul r0.z, v1.w, l(1.000100)
  44: ge r0.w, r0.z, -r0.z
  45: frc r0.z, |r0.z|
  46: movc r0.z, r0.w, r0.z, -r0.z
  47: mul r0.z, r0.z, l(0.999900)
  48: div r1.xy, cb2[10].zwzz, cb2[5].xyxx
  49: mad r0.zw, cb0[0].yyyy, r1.xxxy, r0.zzzz
  50: add r0.zw, r0.zzzw, v1.xxxy
  51: mad r0.zw, r0.zzzw, cb2[5].xxxy, cb2[5].zzzw
  52: sample r1.xyzw, r0.zwzz, t2.xyzw, s2
  53: div r0.zw, cb2[11].xxxy, cb2[7].xxxy
  54: mad r0.zw, r0.zzzw, cb0[0].yyyy, v1.xxxy
  55: mad r0.zw, r0.zzzw, cb2[7].xxxy, cb2[7].zzzw
  56: sample r2.xyzw, r0.zwzz, t3.xyzw, s3
  57: mad r0.zw, v1.xxxy, cb2[9].xxxy, cb2[9].zzzw
  58: sample r3.xyzw, r0.zwzz, t4.xyzw, s4
  59: mul r0.zw, r2.xxxy, r3.xxxy
  60: mul r0.zw, r0.zzzw, cb2[11].zzzz
  61: div r0.zw, r0.zzzw, cb2[3].xxxy
  62: div r2.xy, cb2[10].xyxx, cb2[3].xyxx
  63: mad r2.xy, r2.xyxx, cb0[0].yyyy, v1.xyxx
  64: add r0.zw, -r0.zzzw, r2.xxxy
  65: mad r0.zw, r0.zzzw, cb2[3].xxxy, cb2[3].zzzw
  66: sample r2.xyzw, r0.zwzz, t1.xyzw, s1
  67: mul r1.xyzw, r1.wxyz, r2.wxyz
  68: ne r0.zw, l(0.000000, 0.000000, 0.000000, 0.000000), cb2[1].xxxy
  69: movc r2.xyz, r0.zzzz, r1.xxxx, r1.yzwy
  70: max r2.xyz, r0.xxxx, r2.xyzx
  71: mad_sat r0.x, r1.x, cb2[0].x, r0.x
  72: movc r1.yzw, r0.wwww, r2.xxyz, r1.yyzw
  73: add r0.z, -v1.z, l(1.000000)
  74: add_sat r2.xyz, -r0.zzzz, r3.xyzx
  75: mul_sat r2.xyz, r2.xyzx, r3.xyzx
  76: mul r2.xyz, r1.yzwy, r2.xyzx
  77: ne r3.xy, l(0.000000, 0.000000, 0.000000, 0.000000), cb2[14].yzyy
  78: movc r1.yzw, r3.xxxx, r2.xxyz, r1.yyzw
  79: mul r1.yzw, r1.yyzw, cb2[12].xxxx
  80: mul r1.yzw, r1.yyzw, v3.xxyz
  81: max r0.z, v2.x, l(1.000000)
  82: min r0.z, r0.z, l(1000000.000000)
  83: eq r2.x, v1.x, v2.x
  84: movc r0.z, r2.x, l(1.000000), r0.z
  85: mul o0.xyz, r1.yzwy, r0.zzzz
  86: mul r0.z, r0.x, r1.x
  87: ne r1.y, l(0.000000, 0.000000, 0.000000, 0.000000), cb2[0].w
  88: movc r0.x, r1.y, r0.z, r0.x
  89: movc r0.x, r0.w, r0.x, r1.x
  90: mul r0.y, r0.y, r0.x
  91: movc r0.x, r3.y, r0.y, r0.x
  92: mul r0.x, r0.x, v3.w
  93: mul_sat o0.w, r0.x, cb2[14].x
  94: ret 
// Approximately 0 instruction slots used


//////////////////////////////////////////////////////
Keywords: DIRECTIONAL LIGHTPROBE_SH SHADOWS_SCREEN
-- Vertex shader for "d3d11":
// No shader variant for this keyword set. The closest match will be used instead.

-- Hardware tier variant: Tier 1
-- Fragment shader for "d3d11":
// Stats: 68 math, 4 temp registers, 5 textures
Set 2D Texture "_CameraDepthTexture" to slot 0
Set 2D Texture "_MainTex" to slot 1
Set 2D Texture "_Noise" to slot 2
Set 2D Texture "_Flow" to slot 3
Set 2D Texture "_Mask" to slot 4

Constant Buffer "UnityPerCamera" (144 bytes) on slot 0 {
  Vector4 _Time at 0
  Vector3 _WorldSpaceCameraPos at 64
  Vector4 _ProjectionParams at 80
  Vector4 _ScreenParams at 96
  Vector4 _ZBufferParams at 112
  Vector4 unity_OrthoParams at 128
}
Constant Buffer "UnityPerFrame" (368 bytes) on slot 1 {
  Matrix4x4 unity_MatrixV at 144
  Matrix4x4 unity_MatrixVP at 272
}
Constant Buffer "UnityPerMaterial" (240 bytes) on slot 2 {
  Float _Fresnelpower at 4
  Float _Fresnelscale at 8
  Float _Useonlycolor at 16
  Float _Use_fresnel at 20
  Vector4 _MainTex_ST at 48
  Vector4 _Noise_ST at 80
  Vector4 _Flow_ST at 112
  Vector4 _Mask_ST at 144
  Vector4 _SpeedMainTexUVNoiseZW at 160
  Vector4 _DistortionSpeedXYPowerZ at 176
  Float _Emission at 192
  Float _Usecenterglow at 228
  Float _Depthpower at 236
}

Shader Disassembly:
//
// Generated by Microsoft (R) D3D Shader Disassembler
//
//
// Input signature:
//
// Name                 Index   Mask Register SysValue  Format   Used
// -------------------- ----- ------ -------- -------- ------- ------
// SV_POSITION              0   xyzw        0      POS   float   xy  
// INTERP                   0   xyzw        1     NONE   float   xyzw
// INTERP                   1   xyzw        2     NONE   float   x   
// INTERP                   2   xyzw        3     NONE   float   xyz 
// INTERP                   3   xyz         4     NONE   float   xyz 
// INTERP                   4   xyz         5     NONE   float   xyz 
// SV_IsFrontFace           0   x           6    FFACE    uint   x   
//
//
// Output signature:
//
// Name                 Index   Mask Register SysValue  Format   Used
// -------------------- ----- ------ -------- -------- ------- ------
// SV_TARGET                0   xyzw        0   TARGET   float   xyzw
//
      ps_4_0
      dcl_constantbuffer CB0[9], immediateIndexed
      dcl_constantbuffer CB1[21], immediateIndexed
      dcl_constantbuffer CB2[15], immediateIndexed
      dcl_sampler s0, mode_default
      dcl_sampler s1, mode_default
      dcl_sampler s2, mode_default
      dcl_sampler s3, mode_default
      dcl_sampler s4, mode_default
      dcl_resource_texture2d (float,float,float,float) t0
      dcl_resource_texture2d (float,float,float,float) t1
      dcl_resource_texture2d (float,float,float,float) t2
      dcl_resource_texture2d (float,float,float,float) t3
      dcl_resource_texture2d (float,float,float,float) t4
      dcl_input_ps_siv linear noperspective v0.xy, position
      dcl_input_ps linear v1.xyzw
      dcl_input_ps linear v2.x
      dcl_input_ps linear v3.xyz
      dcl_input_ps linear v4.xyz
      dcl_input_ps linear v5.xyz
      dcl_input_ps_sgv constant v6.x, is_front_face
      dcl_output o0.xyzw
      dcl_temps 4
   0: dp3 r0.x, v5.xyzx, v5.xyzx
   1: sqrt r0.x, r0.x
   2: div r0.x, l(1.000000, 1.000000, 1.000000, 1.000000), r0.x
   3: mul r0.xyz, r0.xxxx, v5.xyzx
   4: dp3 r0.w, r0.xyzx, r0.xyzx
   5: rsq r0.w, r0.w
   6: mul r0.xyz, r0.wwww, r0.xyzx
   7: add r1.xyz, -v4.xyzx, cb0[4].xyzx
   8: dp3 r0.w, r1.xyzx, r1.xyzx
   9: rsq r0.w, r0.w
  10: mul r1.xyz, r0.wwww, r1.xyzx
  11: eq r0.w, cb0[8].w, l(0.000000)
  12: movc r2.x, r0.w, r1.x, cb1[9].z
  13: movc r2.y, r0.w, r1.y, cb1[10].z
  14: movc r2.z, r0.w, r1.z, cb1[11].z
  15: dp3_sat r0.x, r0.xyzx, r2.xyzx
  16: add r0.x, -r0.x, l(1.000000)
  17: log r0.x, r0.x
  18: mul r0.x, r0.x, cb2[0].y
  19: exp r0.x, r0.x
  20: mul_sat r0.x, r0.x, cb2[0].z
  21: and r0.x, r0.x, v6.x
  22: lt r0.y, cb0[5].x, l(0.000000)
  23: add r0.z, -v0.y, cb0[6].y
  24: movc r1.y, r0.y, r0.z, v0.y
  25: mov r1.x, v0.x
  26: div r1.xy, r1.xyxx, cb0[6].xyxx
  27: add r1.z, -r1.y, l(1.000000)
  28: sample r1.xyzw, r1.xzxx, t0.xyzw, s0
  29: mad r0.y, cb0[7].x, r1.x, cb0[7].y
  30: div r0.y, l(1.000000, 1.000000, 1.000000, 1.000000), r0.y
  31: mul r0.z, v4.y, cb1[18].w
  32: mad r0.z, cb1[17].w, v4.x, r0.z
  33: mad r0.z, cb1[19].w, v4.z, r0.z
  34: add r0.z, r0.z, cb1[20].w
  35: mad r0.y, r0.y, cb0[5].z, -r0.z
  36: div_sat r0.y, r0.y, cb2[14].w
  37: ge r0.z, l(0.000000), r0.y
  38: and r0.z, r0.z, l(0x3f800000)
  39: add r0.w, -r0.y, l(1.000000)
  40: mad r0.y, r0.z, r0.w, r0.y
  41: add r0.x, -r0.y, r0.x
  42: add r0.x, r0.x, l(1.000000)
  43: mul r0.y, v1.w, l(1.000100)
  44: ge r0.z, r0.y, -r0.y
  45: frc r0.y, |r0.y|
  46: movc r0.y, r0.z, r0.y, -r0.y
  47: mul r0.y, r0.y, l(0.999900)
  48: div r0.zw, cb2[10].zzzw, cb2[5].xxxy
  49: mad r0.yz, cb0[0].yyyy, r0.zzwz, r0.yyyy
  50: add r0.yz, r0.yyzy, v1.xxyx
  51: mad r0.yz, r0.yyzy, cb2[5].xxyx, cb2[5].zzwz
  52: sample r1.xyzw, r0.yzyy, t2.xyzw, s2
  53: div r0.yz, cb2[11].xxyx, cb2[7].xxyx
  54: mad r0.yz, r0.yyzy, cb0[0].yyyy, v1.xxyx
  55: mad r0.yz, r0.yyzy, cb2[7].xxyx, cb2[7].zzwz
  56: sample r2.xyzw, r0.yzyy, t3.xyzw, s3
  57: mad r0.yz, v1.xxyx, cb2[9].xxyx, cb2[9].zzwz
  58: sample r3.xyzw, r0.yzyy, t4.xyzw, s4
  59: mul r0.yz, r2.xxyx, r3.xxyx
  60: mul r0.yz, r0.yyzy, cb2[11].zzzz
  61: div r0.yz, r0.yyzy, cb2[3].xxyx
  62: div r2.xy, cb2[10].xyxx, cb2[3].xyxx
  63: mad r2.xy, r2.xyxx, cb0[0].yyyy, v1.xyxx
  64: add r0.yz, -r0.yyzy, r2.xxyx
  65: mad r0.yz, r0.yyzy, cb2[3].xxyx, cb2[3].zzwz
  66: sample r2.xyzw, r0.yzyy, t1.xyzw, s1
  67: mul r1.xyzw, r1.wxyz, r2.wxyz
  68: ne r0.yz, l(0.000000, 0.000000, 0.000000, 0.000000), cb2[1].xxyx
  69: movc r2.xyz, r0.yyyy, r1.xxxx, r1.yzwy
  70: max r0.xyw, r0.xxxx, r2.xyxz
  71: movc r0.xyz, r0.zzzz, r0.xywx, r1.yzwy
  72: add r0.w, -v1.z, l(1.000000)
  73: add_sat r1.xyz, -r0.wwww, r3.xyzx
  74: mul_sat r1.xyz, r1.xyzx, r3.xyzx
  75: mul r1.xyz, r0.xyzx, r1.xyzx
  76: ne r0.w, l(0.000000, 0.000000, 0.000000, 0.000000), cb2[14].y
  77: movc r0.xyz, r0.wwww, r1.xyzx, r0.xyzx
  78: mul r0.xyz, r0.xyzx, cb2[12].xxxx
  79: mul r0.xyz, r0.xyzx, v3.xyzx
  80: max r0.w, v2.x, l(1.000000)
  81: min r0.w, r0.w, l(1000000.000000)
  82: eq r1.x, v1.x, v2.x
  83: movc r0.w, r1.x, l(1.000000), r0.w
  84: mul o0.xyz, r0.xyzx, r0.wwww
  85: mov o0.w, l(1.000000)
  86: ret 
// Approximately 0 instruction slots used


//////////////////////////////////////////////////////
Keywords: DIRECTIONAL _BUILTIN_SURFACE_TYPE_TRANSPARENT
-- Vertex shader for "d3d11":
// No shader variant for this keyword set. The closest match will be used instead.

-- Hardware tier variant: Tier 1
-- Fragment shader for "d3d11":
// Stats: 73 math, 4 temp registers, 5 textures
Set 2D Texture "_CameraDepthTexture" to slot 0
Set 2D Texture "_MainTex" to slot 1
Set 2D Texture "_Noise" to slot 2
Set 2D Texture "_Flow" to slot 3
Set 2D Texture "_Mask" to slot 4

Constant Buffer "UnityPerCamera" (144 bytes) on slot 0 {
  Vector4 _Time at 0
  Vector3 _WorldSpaceCameraPos at 64
  Vector4 _ProjectionParams at 80
  Vector4 _ScreenParams at 96
  Vector4 _ZBufferParams at 112
  Vector4 unity_OrthoParams at 128
}
Constant Buffer "UnityPerFrame" (368 bytes) on slot 1 {
  Matrix4x4 unity_MatrixV at 144
  Matrix4x4 unity_MatrixVP at 272
}
Constant Buffer "UnityPerMaterial" (240 bytes) on slot 2 {
  Float _Textureopacity at 0
  Float _Fresnelpower at 4
  Float _Fresnelscale at 8
  Float _Multiply_texture at 12
  Float _Useonlycolor at 16
  Float _Use_fresnel at 20
  Vector4 _MainTex_ST at 48
  Vector4 _Noise_ST at 80
  Vector4 _Flow_ST at 112
  Vector4 _Mask_ST at 144
  Vector4 _SpeedMainTexUVNoiseZW at 160
  Vector4 _DistortionSpeedXYPowerZ at 176
  Float _Emission at 192
  Float _Opacity at 224
  Float _Usecenterglow at 228
  Float _Usedepth at 232
  Float _Depthpower at 236
}

Shader Disassembly:
//
// Generated by Microsoft (R) D3D Shader Disassembler
//
//
// Input signature:
//
// Name                 Index   Mask Register SysValue  Format   Used
// -------------------- ----- ------ -------- -------- ------- ------
// SV_POSITION              0   xyzw        0      POS   float   xy  
// INTERP                   0   xyzw        1     NONE   float   xyzw
// INTERP                   1   xyzw        2     NONE   float   x   
// INTERP                   2   xyzw        3     NONE   float   xyzw
// INTERP                   3   xyz         4     NONE   float   xyz 
// INTERP                   4   xyz         5     NONE   float   xyz 
// SV_IsFrontFace           0   x           6    FFACE    uint   x   
//
//
// Output signature:
//
// Name                 Index   Mask Register SysValue  Format   Used
// -------------------- ----- ------ -------- -------- ------- ------
// SV_TARGET                0   xyzw        0   TARGET   float   xyzw
//
      ps_4_0
      dcl_constantbuffer CB0[9], immediateIndexed
      dcl_constantbuffer CB1[21], immediateIndexed
      dcl_constantbuffer CB2[15], immediateIndexed
      dcl_sampler s0, mode_default
      dcl_sampler s1, mode_default
      dcl_sampler s2, mode_default
      dcl_sampler s3, mode_default
      dcl_sampler s4, mode_default
      dcl_resource_texture2d (float,float,float,float) t0
      dcl_resource_texture2d (float,float,float,float) t1
      dcl_resource_texture2d (float,float,float,float) t2
      dcl_resource_texture2d (float,float,float,float) t3
      dcl_resource_texture2d (float,float,float,float) t4
      dcl_input_ps_siv linear noperspective v0.xy, position
      dcl_input_ps linear v1.xyzw
      dcl_input_ps linear v2.x
      dcl_input_ps linear v3.xyzw
      dcl_input_ps linear v4.xyz
      dcl_input_ps linear v5.xyz
      dcl_input_ps_sgv constant v6.x, is_front_face
      dcl_output o0.xyzw
      dcl_temps 4
   0: dp3 r0.x, v5.xyzx, v5.xyzx
   1: sqrt r0.x, r0.x
   2: div r0.x, l(1.000000, 1.000000, 1.000000, 1.000000), r0.x
   3: mul r0.xyz, r0.xxxx, v5.xyzx
   4: dp3 r0.w, r0.xyzx, r0.xyzx
   5: rsq r0.w, r0.w
   6: mul r0.xyz, r0.wwww, r0.xyzx
   7: add r1.xyz, -v4.xyzx, cb0[4].xyzx
   8: dp3 r0.w, r1.xyzx, r1.xyzx
   9: rsq r0.w, r0.w
  10: mul r1.xyz, r0.wwww, r1.xyzx
  11: eq r0.w, cb0[8].w, l(0.000000)
  12: movc r2.x, r0.w, r1.x, cb1[9].z
  13: movc r2.y, r0.w, r1.y, cb1[10].z
  14: movc r2.z, r0.w, r1.z, cb1[11].z
  15: dp3_sat r0.x, r0.xyzx, r2.xyzx
  16: add r0.x, -r0.x, l(1.000000)
  17: log r0.x, r0.x
  18: mul r0.x, r0.x, cb2[0].y
  19: exp r0.x, r0.x
  20: mul_sat r0.x, r0.x, cb2[0].z
  21: and r0.x, r0.x, v6.x
  22: lt r0.y, cb0[5].x, l(0.000000)
  23: add r0.z, -v0.y, cb0[6].y
  24: movc r1.y, r0.y, r0.z, v0.y
  25: mov r1.x, v0.x
  26: div r1.xy, r1.xyxx, cb0[6].xyxx
  27: add r1.z, -r1.y, l(1.000000)
  28: sample r1.xyzw, r1.xzxx, t0.xyzw, s0
  29: mad r0.y, cb0[7].x, r1.x, cb0[7].y
  30: div r0.y, l(1.000000, 1.000000, 1.000000, 1.000000), r0.y
  31: mul r0.z, v4.y, cb1[18].w
  32: mad r0.z, cb1[17].w, v4.x, r0.z
  33: mad r0.z, cb1[19].w, v4.z, r0.z
  34: add r0.z, r0.z, cb1[20].w
  35: mad r0.y, r0.y, cb0[5].z, -r0.z
  36: div_sat r0.y, r0.y, cb2[14].w
  37: ge r0.z, l(0.000000), r0.y
  38: and r0.z, r0.z, l(0x3f800000)
  39: add r0.w, -r0.y, l(1.000000)
  40: mad r0.y, r0.z, r0.w, r0.y
  41: add r0.x, -r0.y, r0.x
  42: add r0.x, r0.x, l(1.000000)
  43: mul r0.z, v1.w, l(1.000100)
  44: ge r0.w, r0.z, -r0.z
  45: frc r0.z, |r0.z|
  46: movc r0.z, r0.w, r0.z, -r0.z
  47: mul r0.z, r0.z, l(0.999900)
  48: div r1.xy, cb2[10].zwzz, cb2[5].xyxx
  49: mad r0.zw, cb0[0].yyyy, r1.xxxy, r0.zzzz
  50: add r0.zw, r0.zzzw, v1.xxxy
  51: mad r0.zw, r0.zzzw, cb2[5].xxxy, cb2[5].zzzw
  52: sample r1.xyzw, r0.zwzz, t2.xyzw, s2
  53: div r0.zw, cb2[11].xxxy, cb2[7].xxxy
  54: mad r0.zw, r0.zzzw, cb0[0].yyyy, v1.xxxy
  55: mad r0.zw, r0.zzzw, cb2[7].xxxy, cb2[7].zzzw
  56: sample r2.xyzw, r0.zwzz, t3.xyzw, s3
  57: mad r0.zw, v1.xxxy, cb2[9].xxxy, cb2[9].zzzw
  58: sample r3.xyzw, r0.zwzz, t4.xyzw, s4
  59: mul r0.zw, r2.xxxy, r3.xxxy
  60: mul r0.zw, r0.zzzw, cb2[11].zzzz
  61: div r0.zw, r0.zzzw, cb2[3].xxxy
  62: div r2.xy, cb2[10].xyxx, cb2[3].xyxx
  63: mad r2.xy, r2.xyxx, cb0[0].yyyy, v1.xyxx
  64: add r0.zw, -r0.zzzw, r2.xxxy
  65: mad r0.zw, r0.zzzw, cb2[3].xxxy, cb2[3].zzzw
  66: sample r2.xyzw, r0.zwzz, t1.xyzw, s1
  67: mul r1.xyzw, r1.wxyz, r2.wxyz
  68: ne r0.zw, l(0.000000, 0.000000, 0.000000, 0.000000), cb2[1].xxxy
  69: movc r2.xyz, r0.zzzz, r1.xxxx, r1.yzwy
  70: max r2.xyz, r0.xxxx, r2.xyzx
  71: mad_sat r0.x, r1.x, cb2[0].x, r0.x
  72: movc r1.yzw, r0.wwww, r2.xxyz, r1.yyzw
  73: add r0.z, -v1.z, l(1.000000)
  74: add_sat r2.xyz, -r0.zzzz, r3.xyzx
  75: mul_sat r2.xyz, r2.xyzx, r3.xyzx
  76: mul r2.xyz, r1.yzwy, r2.xyzx
  77: ne r3.xy, l(0.000000, 0.000000, 0.000000, 0.000000), cb2[14].yzyy
  78: movc r1.yzw, r3.xxxx, r2.xxyz, r1.yyzw
  79: mul r1.yzw, r1.yyzw, cb2[12].xxxx
  80: mul r1.yzw, r1.yyzw, v3.xxyz
  81: max r0.z, v2.x, l(1.000000)
  82: min r0.z, r0.z, l(1000000.000000)
  83: eq r2.x, v1.x, v2.x
  84: movc r0.z, r2.x, l(1.000000), r0.z
  85: mul o0.xyz, r1.yzwy, r0.zzzz
  86: mul r0.z, r0.x, r1.x
  87: ne r1.y, l(0.000000, 0.000000, 0.000000, 0.000000), cb2[0].w
  88: movc r0.x, r1.y, r0.z, r0.x
  89: movc r0.x, r0.w, r0.x, r1.x
  90: mul r0.y, r0.y, r0.x
  91: movc r0.x, r3.y, r0.y, r0.x
  92: mul r0.x, r0.x, v3.w
  93: mul_sat o0.w, r0.x, cb2[14].x
  94: ret 
// Approximately 0 instruction slots used


//////////////////////////////////////////////////////
Keywords: DIRECTIONAL FOG_EXP2
-- Vertex shader for "d3d11":
// No shader variant for this keyword set. The closest match will be used instead.

-- Hardware tier variant: Tier 1
-- Fragment shader for "d3d11":
// Stats: 68 math, 4 temp registers, 5 textures
Set 2D Texture "_CameraDepthTexture" to slot 0
Set 2D Texture "_MainTex" to slot 1
Set 2D Texture "_Noise" to slot 2
Set 2D Texture "_Flow" to slot 3
Set 2D Texture "_Mask" to slot 4

Constant Buffer "UnityPerCamera" (144 bytes) on slot 0 {
  Vector4 _Time at 0
  Vector3 _WorldSpaceCameraPos at 64
  Vector4 _ProjectionParams at 80
  Vector4 _ScreenParams at 96
  Vector4 _ZBufferParams at 112
  Vector4 unity_OrthoParams at 128
}
Constant Buffer "UnityPerFrame" (368 bytes) on slot 1 {
  Matrix4x4 unity_MatrixV at 144
  Matrix4x4 unity_MatrixVP at 272
}
Constant Buffer "UnityPerMaterial" (240 bytes) on slot 2 {
  Float _Fresnelpower at 4
  Float _Fresnelscale at 8
  Float _Useonlycolor at 16
  Float _Use_fresnel at 20
  Vector4 _MainTex_ST at 48
  Vector4 _Noise_ST at 80
  Vector4 _Flow_ST at 112
  Vector4 _Mask_ST at 144
  Vector4 _SpeedMainTexUVNoiseZW at 160
  Vector4 _DistortionSpeedXYPowerZ at 176
  Float _Emission at 192
  Float _Usecenterglow at 228
  Float _Depthpower at 236
}

Shader Disassembly:
//
// Generated by Microsoft (R) D3D Shader Disassembler
//
//
// Input signature:
//
// Name                 Index   Mask Register SysValue  Format   Used
// -------------------- ----- ------ -------- -------- ------- ------
// SV_POSITION              0   xyzw        0      POS   float   xy  
// INTERP                   0   xyzw        1     NONE   float   xyzw
// INTERP                   1   xyzw        2     NONE   float   x   
// INTERP                   2   xyzw        3     NONE   float   xyz 
// INTERP                   3   xyz         4     NONE   float   xyz 
// INTERP                   4   xyz         5     NONE   float   xyz 
// SV_IsFrontFace           0   x           6    FFACE    uint   x   
//
//
// Output signature:
//
// Name                 Index   Mask Register SysValue  Format   Used
// -------------------- ----- ------ -------- -------- ------- ------
// SV_TARGET                0   xyzw        0   TARGET   float   xyzw
//
      ps_4_0
      dcl_constantbuffer CB0[9], immediateIndexed
      dcl_constantbuffer CB1[21], immediateIndexed
      dcl_constantbuffer CB2[15], immediateIndexed
      dcl_sampler s0, mode_default
      dcl_sampler s1, mode_default
      dcl_sampler s2, mode_default
      dcl_sampler s3, mode_default
      dcl_sampler s4, mode_default
      dcl_resource_texture2d (float,float,float,float) t0
      dcl_resource_texture2d (float,float,float,float) t1
      dcl_resource_texture2d (float,float,float,float) t2
      dcl_resource_texture2d (float,float,float,float) t3
      dcl_resource_texture2d (float,float,float,float) t4
      dcl_input_ps_siv linear noperspective v0.xy, position
      dcl_input_ps linear v1.xyzw
      dcl_input_ps linear v2.x
      dcl_input_ps linear v3.xyz
      dcl_input_ps linear v4.xyz
      dcl_input_ps linear v5.xyz
      dcl_input_ps_sgv constant v6.x, is_front_face
      dcl_output o0.xyzw
      dcl_temps 4
   0: dp3 r0.x, v5.xyzx, v5.xyzx
   1: sqrt r0.x, r0.x
   2: div r0.x, l(1.000000, 1.000000, 1.000000, 1.000000), r0.x
   3: mul r0.xyz, r0.xxxx, v5.xyzx
   4: dp3 r0.w, r0.xyzx, r0.xyzx
   5: rsq r0.w, r0.w
   6: mul r0.xyz, r0.wwww, r0.xyzx
   7: add r1.xyz, -v4.xyzx, cb0[4].xyzx
   8: dp3 r0.w, r1.xyzx, r1.xyzx
   9: rsq r0.w, r0.w
  10: mul r1.xyz, r0.wwww, r1.xyzx
  11: eq r0.w, cb0[8].w, l(0.000000)
  12: movc r2.x, r0.w, r1.x, cb1[9].z
  13: movc r2.y, r0.w, r1.y, cb1[10].z
  14: movc r2.z, r0.w, r1.z, cb1[11].z
  15: dp3_sat r0.x, r0.xyzx, r2.xyzx
  16: add r0.x, -r0.x, l(1.000000)
  17: log r0.x, r0.x
  18: mul r0.x, r0.x, cb2[0].y
  19: exp r0.x, r0.x
  20: mul_sat r0.x, r0.x, cb2[0].z
  21: and r0.x, r0.x, v6.x
  22: lt r0.y, cb0[5].x, l(0.000000)
  23: add r0.z, -v0.y, cb0[6].y
  24: movc r1.y, r0.y, r0.z, v0.y
  25: mov r1.x, v0.x
  26: div r1.xy, r1.xyxx, cb0[6].xyxx
  27: add r1.z, -r1.y, l(1.000000)
  28: sample r1.xyzw, r1.xzxx, t0.xyzw, s0
  29: mad r0.y, cb0[7].x, r1.x, cb0[7].y
  30: div r0.y, l(1.000000, 1.000000, 1.000000, 1.000000), r0.y
  31: mul r0.z, v4.y, cb1[18].w
  32: mad r0.z, cb1[17].w, v4.x, r0.z
  33: mad r0.z, cb1[19].w, v4.z, r0.z
  34: add r0.z, r0.z, cb1[20].w
  35: mad r0.y, r0.y, cb0[5].z, -r0.z
  36: div_sat r0.y, r0.y, cb2[14].w
  37: ge r0.z, l(0.000000), r0.y
  38: and r0.z, r0.z, l(0x3f800000)
  39: add r0.w, -r0.y, l(1.000000)
  40: mad r0.y, r0.z, r0.w, r0.y
  41: add r0.x, -r0.y, r0.x
  42: add r0.x, r0.x, l(1.000000)
  43: mul r0.y, v1.w, l(1.000100)
  44: ge r0.z, r0.y, -r0.y
  45: frc r0.y, |r0.y|
  46: movc r0.y, r0.z, r0.y, -r0.y
  47: mul r0.y, r0.y, l(0.999900)
  48: div r0.zw, cb2[10].zzzw, cb2[5].xxxy
  49: mad r0.yz, cb0[0].yyyy, r0.zzwz, r0.yyyy
  50: add r0.yz, r0.yyzy, v1.xxyx
  51: mad r0.yz, r0.yyzy, cb2[5].xxyx, cb2[5].zzwz
  52: sample r1.xyzw, r0.yzyy, t2.xyzw, s2
  53: div r0.yz, cb2[11].xxyx, cb2[7].xxyx
  54: mad r0.yz, r0.yyzy, cb0[0].yyyy, v1.xxyx
  55: mad r0.yz, r0.yyzy, cb2[7].xxyx, cb2[7].zzwz
  56: sample r2.xyzw, r0.yzyy, t3.xyzw, s3
  57: mad r0.yz, v1.xxyx, cb2[9].xxyx, cb2[9].zzwz
  58: sample r3.xyzw, r0.yzyy, t4.xyzw, s4
  59: mul r0.yz, r2.xxyx, r3.xxyx
  60: mul r0.yz, r0.yyzy, cb2[11].zzzz
  61: div r0.yz, r0.yyzy, cb2[3].xxyx
  62: div r2.xy, cb2[10].xyxx, cb2[3].xyxx
  63: mad r2.xy, r2.xyxx, cb0[0].yyyy, v1.xyxx
  64: add r0.yz, -r0.yyzy, r2.xxyx
  65: mad r0.yz, r0.yyzy, cb2[3].xxyx, cb2[3].zzwz
  66: sample r2.xyzw, r0.yzyy, t1.xyzw, s1
  67: mul r1.xyzw, r1.wxyz, r2.wxyz
  68: ne r0.yz, l(0.000000, 0.000000, 0.000000, 0.000000), cb2[1].xxyx
  69: movc r2.xyz, r0.yyyy, r1.xxxx, r1.yzwy
  70: max r0.xyw, r0.xxxx, r2.xyxz
  71: movc r0.xyz, r0.zzzz, r0.xywx, r1.yzwy
  72: add r0.w, -v1.z, l(1.000000)
  73: add_sat r1.xyz, -r0.wwww, r3.xyzx
  74: mul_sat r1.xyz, r1.xyzx, r3.xyzx
  75: mul r1.xyz, r0.xyzx, r1.xyzx
  76: ne r0.w, l(0.000000, 0.000000, 0.000000, 0.000000), cb2[14].y
  77: movc r0.xyz, r0.wwww, r1.xyzx, r0.xyzx
  78: mul r0.xyz, r0.xyzx, cb2[12].xxxx
  79: mul r0.xyz, r0.xyzx, v3.xyzx
  80: max r0.w, v2.x, l(1.000000)
  81: min r0.w, r0.w, l(1000000.000000)
  82: eq r1.x, v1.x, v2.x
  83: movc r0.w, r1.x, l(1.000000), r0.w
  84: mul o0.xyz, r0.xyzx, r0.wwww
  85: mov o0.w, l(1.000000)
  86: ret 
// Approximately 0 instruction slots used


//////////////////////////////////////////////////////
Keywords: DIRECTIONAL FOG_EXP2 LIGHTPROBE_SH _BUILTIN_SURFACE_TYPE_TRANSPARENT
-- Vertex shader for "d3d11":
// No shader variant for this keyword set. The closest match will be used instead.

-- Hardware tier variant: Tier 1
-- Fragment shader for "d3d11":
// Stats: 73 math, 4 temp registers, 5 textures
Set 2D Texture "_CameraDepthTexture" to slot 0
Set 2D Texture "_MainTex" to slot 1
Set 2D Texture "_Noise" to slot 2
Set 2D Texture "_Flow" to slot 3
Set 2D Texture "_Mask" to slot 4

Constant Buffer "UnityPerCamera" (144 bytes) on slot 0 {
  Vector4 _Time at 0
  Vector3 _WorldSpaceCameraPos at 64
  Vector4 _ProjectionParams at 80
  Vector4 _ScreenParams at 96
  Vector4 _ZBufferParams at 112
  Vector4 unity_OrthoParams at 128
}
Constant Buffer "UnityPerFrame" (368 bytes) on slot 1 {
  Matrix4x4 unity_MatrixV at 144
  Matrix4x4 unity_MatrixVP at 272
}
Constant Buffer "UnityPerMaterial" (240 bytes) on slot 2 {
  Float _Textureopacity at 0
  Float _Fresnelpower at 4
  Float _Fresnelscale at 8
  Float _Multiply_texture at 12
  Float _Useonlycolor at 16
  Float _Use_fresnel at 20
  Vector4 _MainTex_ST at 48
  Vector4 _Noise_ST at 80
  Vector4 _Flow_ST at 112
  Vector4 _Mask_ST at 144
  Vector4 _SpeedMainTexUVNoiseZW at 160
  Vector4 _DistortionSpeedXYPowerZ at 176
  Float _Emission at 192
  Float _Opacity at 224
  Float _Usecenterglow at 228
  Float _Usedepth at 232
  Float _Depthpower at 236
}

Shader Disassembly:
//
// Generated by Microsoft (R) D3D Shader Disassembler
//
//
// Input signature:
//
// Name                 Index   Mask Register SysValue  Format   Used
// -------------------- ----- ------ -------- -------- ------- ------
// SV_POSITION              0   xyzw        0      POS   float   xy  
// INTERP                   0   xyzw        1     NONE   float   xyzw
// INTERP                   1   xyzw        2     NONE   float   x   
// INTERP                   2   xyzw        3     NONE   float   xyzw
// INTERP                   3   xyz         4     NONE   float   xyz 
// INTERP                   4   xyz         5     NONE   float   xyz 
// SV_IsFrontFace           0   x           6    FFACE    uint   x   
//
//
// Output signature:
//
// Name                 Index   Mask Register SysValue  Format   Used
// -------------------- ----- ------ -------- -------- ------- ------
// SV_TARGET                0   xyzw        0   TARGET   float   xyzw
//
      ps_4_0
      dcl_constantbuffer CB0[9], immediateIndexed
      dcl_constantbuffer CB1[21], immediateIndexed
      dcl_constantbuffer CB2[15], immediateIndexed
      dcl_sampler s0, mode_default
      dcl_sampler s1, mode_default
      dcl_sampler s2, mode_default
      dcl_sampler s3, mode_default
      dcl_sampler s4, mode_default
      dcl_resource_texture2d (float,float,float,float) t0
      dcl_resource_texture2d (float,float,float,float) t1
      dcl_resource_texture2d (float,float,float,float) t2
      dcl_resource_texture2d (float,float,float,float) t3
      dcl_resource_texture2d (float,float,float,float) t4
      dcl_input_ps_siv linear noperspective v0.xy, position
      dcl_input_ps linear v1.xyzw
      dcl_input_ps linear v2.x
      dcl_input_ps linear v3.xyzw
      dcl_input_ps linear v4.xyz
      dcl_input_ps linear v5.xyz
      dcl_input_ps_sgv constant v6.x, is_front_face
      dcl_output o0.xyzw
      dcl_temps 4
   0: dp3 r0.x, v5.xyzx, v5.xyzx
   1: sqrt r0.x, r0.x
   2: div r0.x, l(1.000000, 1.000000, 1.000000, 1.000000), r0.x
   3: mul r0.xyz, r0.xxxx, v5.xyzx
   4: dp3 r0.w, r0.xyzx, r0.xyzx
   5: rsq r0.w, r0.w
   6: mul r0.xyz, r0.wwww, r0.xyzx
   7: add r1.xyz, -v4.xyzx, cb0[4].xyzx
   8: dp3 r0.w, r1.xyzx, r1.xyzx
   9: rsq r0.w, r0.w
  10: mul r1.xyz, r0.wwww, r1.xyzx
  11: eq r0.w, cb0[8].w, l(0.000000)
  12: movc r2.x, r0.w, r1.x, cb1[9].z
  13: movc r2.y, r0.w, r1.y, cb1[10].z
  14: movc r2.z, r0.w, r1.z, cb1[11].z
  15: dp3_sat r0.x, r0.xyzx, r2.xyzx
  16: add r0.x, -r0.x, l(1.000000)
  17: log r0.x, r0.x
  18: mul r0.x, r0.x, cb2[0].y
  19: exp r0.x, r0.x
  20: mul_sat r0.x, r0.x, cb2[0].z
  21: and r0.x, r0.x, v6.x
  22: lt r0.y, cb0[5].x, l(0.000000)
  23: add r0.z, -v0.y, cb0[6].y
  24: movc r1.y, r0.y, r0.z, v0.y
  25: mov r1.x, v0.x
  26: div r1.xy, r1.xyxx, cb0[6].xyxx
  27: add r1.z, -r1.y, l(1.000000)
  28: sample r1.xyzw, r1.xzxx, t0.xyzw, s0
  29: mad r0.y, cb0[7].x, r1.x, cb0[7].y
  30: div r0.y, l(1.000000, 1.000000, 1.000000, 1.000000), r0.y
  31: mul r0.z, v4.y, cb1[18].w
  32: mad r0.z, cb1[17].w, v4.x, r0.z
  33: mad r0.z, cb1[19].w, v4.z, r0.z
  34: add r0.z, r0.z, cb1[20].w
  35: mad r0.y, r0.y, cb0[5].z, -r0.z
  36: div_sat r0.y, r0.y, cb2[14].w
  37: ge r0.z, l(0.000000), r0.y
  38: and r0.z, r0.z, l(0x3f800000)
  39: add r0.w, -r0.y, l(1.000000)
  40: mad r0.y, r0.z, r0.w, r0.y
  41: add r0.x, -r0.y, r0.x
  42: add r0.x, r0.x, l(1.000000)
  43: mul r0.z, v1.w, l(1.000100)
  44: ge r0.w, r0.z, -r0.z
  45: frc r0.z, |r0.z|
  46: movc r0.z, r0.w, r0.z, -r0.z
  47: mul r0.z, r0.z, l(0.999900)
  48: div r1.xy, cb2[10].zwzz, cb2[5].xyxx
  49: mad r0.zw, cb0[0].yyyy, r1.xxxy, r0.zzzz
  50: add r0.zw, r0.zzzw, v1.xxxy
  51: mad r0.zw, r0.zzzw, cb2[5].xxxy, cb2[5].zzzw
  52: sample r1.xyzw, r0.zwzz, t2.xyzw, s2
  53: div r0.zw, cb2[11].xxxy, cb2[7].xxxy
  54: mad r0.zw, r0.zzzw, cb0[0].yyyy, v1.xxxy
  55: mad r0.zw, r0.zzzw, cb2[7].xxxy, cb2[7].zzzw
  56: sample r2.xyzw, r0.zwzz, t3.xyzw, s3
  57: mad r0.zw, v1.xxxy, cb2[9].xxxy, cb2[9].zzzw
  58: sample r3.xyzw, r0.zwzz, t4.xyzw, s4
  59: mul r0.zw, r2.xxxy, r3.xxxy
  60: mul r0.zw, r0.zzzw, cb2[11].zzzz
  61: div r0.zw, r0.zzzw, cb2[3].xxxy
  62: div r2.xy, cb2[10].xyxx, cb2[3].xyxx
  63: mad r2.xy, r2.xyxx, cb0[0].yyyy, v1.xyxx
  64: add r0.zw, -r0.zzzw, r2.xxxy
  65: mad r0.zw, r0.zzzw, cb2[3].xxxy, cb2[3].zzzw
  66: sample r2.xyzw, r0.zwzz, t1.xyzw, s1
  67: mul r1.xyzw, r1.wxyz, r2.wxyz
  68: ne r0.zw, l(0.000000, 0.000000, 0.000000, 0.000000), cb2[1].xxxy
  69: movc r2.xyz, r0.zzzz, r1.xxxx, r1.yzwy
  70: max r2.xyz, r0.xxxx, r2.xyzx
  71: mad_sat r0.x, r1.x, cb2[0].x, r0.x
  72: movc r1.yzw, r0.wwww, r2.xxyz, r1.yyzw
  73: add r0.z, -v1.z, l(1.000000)
  74: add_sat r2.xyz, -r0.zzzz, r3.xyzx
  75: mul_sat r2.xyz, r2.xyzx, r3.xyzx
  76: mul r2.xyz, r1.yzwy, r2.xyzx
  77: ne r3.xy, l(0.000000, 0.000000, 0.000000, 0.000000), cb2[14].yzyy
  78: movc r1.yzw, r3.xxxx, r2.xxyz, r1.yyzw
  79: mul r1.yzw, r1.yyzw, cb2[12].xxxx
  80: mul r1.yzw, r1.yyzw, v3.xxyz
  81: max r0.z, v2.x, l(1.000000)
  82: min r0.z, r0.z, l(1000000.000000)
  83: eq r2.x, v1.x, v2.x
  84: movc r0.z, r2.x, l(1.000000), r0.z
  85: mul o0.xyz, r1.yzwy, r0.zzzz
  86: mul r0.z, r0.x, r1.x
  87: ne r1.y, l(0.000000, 0.000000, 0.000000, 0.000000), cb2[0].w
  88: movc r0.x, r1.y, r0.z, r0.x
  89: movc r0.x, r0.w, r0.x, r1.x
  90: mul r0.y, r0.y, r0.x
  91: movc r0.x, r3.y, r0.y, r0.x
  92: mul r0.x, r0.x, v3.w
  93: mul_sat o0.w, r0.x, cb2[14].x
  94: ret 
// Approximately 0 instruction slots used


//////////////////////////////////////////////////////
Keywords: DIRECTIONAL FOG_EXP2 LIGHTPROBE_SH
-- Vertex shader for "d3d11":
// No shader variant for this keyword set. The closest match will be used instead.

-- Hardware tier variant: Tier 1
-- Fragment shader for "d3d11":
// Stats: 68 math, 4 temp registers, 5 textures
Set 2D Texture "_CameraDepthTexture" to slot 0
Set 2D Texture "_MainTex" to slot 1
Set 2D Texture "_Noise" to slot 2
Set 2D Texture "_Flow" to slot 3
Set 2D Texture "_Mask" to slot 4

Constant Buffer "UnityPerCamera" (144 bytes) on slot 0 {
  Vector4 _Time at 0
  Vector3 _WorldSpaceCameraPos at 64
  Vector4 _ProjectionParams at 80
  Vector4 _ScreenParams at 96
  Vector4 _ZBufferParams at 112
  Vector4 unity_OrthoParams at 128
}
Constant Buffer "UnityPerFrame" (368 bytes) on slot 1 {
  Matrix4x4 unity_MatrixV at 144
  Matrix4x4 unity_MatrixVP at 272
}
Constant Buffer "UnityPerMaterial" (240 bytes) on slot 2 {
  Float _Fresnelpower at 4
  Float _Fresnelscale at 8
  Float _Useonlycolor at 16
  Float _Use_fresnel at 20
  Vector4 _MainTex_ST at 48
  Vector4 _Noise_ST at 80
  Vector4 _Flow_ST at 112
  Vector4 _Mask_ST at 144
  Vector4 _SpeedMainTexUVNoiseZW at 160
  Vector4 _DistortionSpeedXYPowerZ at 176
  Float _Emission at 192
  Float _Usecenterglow at 228
  Float _Depthpower at 236
}

Shader Disassembly:
//
// Generated by Microsoft (R) D3D Shader Disassembler
//
//
// Input signature:
//
// Name                 Index   Mask Register SysValue  Format   Used
// -------------------- ----- ------ -------- -------- ------- ------
// SV_POSITION              0   xyzw        0      POS   float   xy  
// INTERP                   0   xyzw        1     NONE   float   xyzw
// INTERP                   1   xyzw        2     NONE   float   x   
// INTERP                   2   xyzw        3     NONE   float   xyz 
// INTERP                   3   xyz         4     NONE   float   xyz 
// INTERP                   4   xyz         5     NONE   float   xyz 
// SV_IsFrontFace           0   x           6    FFACE    uint   x   
//
//
// Output signature:
//
// Name                 Index   Mask Register SysValue  Format   Used
// -------------------- ----- ------ -------- -------- ------- ------
// SV_TARGET                0   xyzw        0   TARGET   float   xyzw
//
      ps_4_0
      dcl_constantbuffer CB0[9], immediateIndexed
      dcl_constantbuffer CB1[21], immediateIndexed
      dcl_constantbuffer CB2[15], immediateIndexed
      dcl_sampler s0, mode_default
      dcl_sampler s1, mode_default
      dcl_sampler s2, mode_default
      dcl_sampler s3, mode_default
      dcl_sampler s4, mode_default
      dcl_resource_texture2d (float,float,float,float) t0
      dcl_resource_texture2d (float,float,float,float) t1
      dcl_resource_texture2d (float,float,float,float) t2
      dcl_resource_texture2d (float,float,float,float) t3
      dcl_resource_texture2d (float,float,float,float) t4
      dcl_input_ps_siv linear noperspective v0.xy, position
      dcl_input_ps linear v1.xyzw
      dcl_input_ps linear v2.x
      dcl_input_ps linear v3.xyz
      dcl_input_ps linear v4.xyz
      dcl_input_ps linear v5.xyz
      dcl_input_ps_sgv constant v6.x, is_front_face
      dcl_output o0.xyzw
      dcl_temps 4
   0: dp3 r0.x, v5.xyzx, v5.xyzx
   1: sqrt r0.x, r0.x
   2: div r0.x, l(1.000000, 1.000000, 1.000000, 1.000000), r0.x
   3: mul r0.xyz, r0.xxxx, v5.xyzx
   4: dp3 r0.w, r0.xyzx, r0.xyzx
   5: rsq r0.w, r0.w
   6: mul r0.xyz, r0.wwww, r0.xyzx
   7: add r1.xyz, -v4.xyzx, cb0[4].xyzx
   8: dp3 r0.w, r1.xyzx, r1.xyzx
   9: rsq r0.w, r0.w
  10: mul r1.xyz, r0.wwww, r1.xyzx
  11: eq r0.w, cb0[8].w, l(0.000000)
  12: movc r2.x, r0.w, r1.x, cb1[9].z
  13: movc r2.y, r0.w, r1.y, cb1[10].z
  14: movc r2.z, r0.w, r1.z, cb1[11].z
  15: dp3_sat r0.x, r0.xyzx, r2.xyzx
  16: add r0.x, -r0.x, l(1.000000)
  17: log r0.x, r0.x
  18: mul r0.x, r0.x, cb2[0].y
  19: exp r0.x, r0.x
  20: mul_sat r0.x, r0.x, cb2[0].z
  21: and r0.x, r0.x, v6.x
  22: lt r0.y, cb0[5].x, l(0.000000)
  23: add r0.z, -v0.y, cb0[6].y
  24: movc r1.y, r0.y, r0.z, v0.y
  25: mov r1.x, v0.x
  26: div r1.xy, r1.xyxx, cb0[6].xyxx
  27: add r1.z, -r1.y, l(1.000000)
  28: sample r1.xyzw, r1.xzxx, t0.xyzw, s0
  29: mad r0.y, cb0[7].x, r1.x, cb0[7].y
  30: div r0.y, l(1.000000, 1.000000, 1.000000, 1.000000), r0.y
  31: mul r0.z, v4.y, cb1[18].w
  32: mad r0.z, cb1[17].w, v4.x, r0.z
  33: mad r0.z, cb1[19].w, v4.z, r0.z
  34: add r0.z, r0.z, cb1[20].w
  35: mad r0.y, r0.y, cb0[5].z, -r0.z
  36: div_sat r0.y, r0.y, cb2[14].w
  37: ge r0.z, l(0.000000), r0.y
  38: and r0.z, r0.z, l(0x3f800000)
  39: add r0.w, -r0.y, l(1.000000)
  40: mad r0.y, r0.z, r0.w, r0.y
  41: add r0.x, -r0.y, r0.x
  42: add r0.x, r0.x, l(1.000000)
  43: mul r0.y, v1.w, l(1.000100)
  44: ge r0.z, r0.y, -r0.y
  45: frc r0.y, |r0.y|
  46: movc r0.y, r0.z, r0.y, -r0.y
  47: mul r0.y, r0.y, l(0.999900)
  48: div r0.zw, cb2[10].zzzw, cb2[5].xxxy
  49: mad r0.yz, cb0[0].yyyy, r0.zzwz, r0.yyyy
  50: add r0.yz, r0.yyzy, v1.xxyx
  51: mad r0.yz, r0.yyzy, cb2[5].xxyx, cb2[5].zzwz
  52: sample r1.xyzw, r0.yzyy, t2.xyzw, s2
  53: div r0.yz, cb2[11].xxyx, cb2[7].xxyx
  54: mad r0.yz, r0.yyzy, cb0[0].yyyy, v1.xxyx
  55: mad r0.yz, r0.yyzy, cb2[7].xxyx, cb2[7].zzwz
  56: sample r2.xyzw, r0.yzyy, t3.xyzw, s3
  57: mad r0.yz, v1.xxyx, cb2[9].xxyx, cb2[9].zzwz
  58: sample r3.xyzw, r0.yzyy, t4.xyzw, s4
  59: mul r0.yz, r2.xxyx, r3.xxyx
  60: mul r0.yz, r0.yyzy, cb2[11].zzzz
  61: div r0.yz, r0.yyzy, cb2[3].xxyx
  62: div r2.xy, cb2[10].xyxx, cb2[3].xyxx
  63: mad r2.xy, r2.xyxx, cb0[0].yyyy, v1.xyxx
  64: add r0.yz, -r0.yyzy, r2.xxyx
  65: mad r0.yz, r0.yyzy, cb2[3].xxyx, cb2[3].zzwz
  66: sample r2.xyzw, r0.yzyy, t1.xyzw, s1
  67: mul r1.xyzw, r1.wxyz, r2.wxyz
  68: ne r0.yz, l(0.000000, 0.000000, 0.000000, 0.000000), cb2[1].xxyx
  69: movc r2.xyz, r0.yyyy, r1.xxxx, r1.yzwy
  70: max r0.xyw, r0.xxxx, r2.xyxz
  71: movc r0.xyz, r0.zzzz, r0.xywx, r1.yzwy
  72: add r0.w, -v1.z, l(1.000000)
  73: add_sat r1.xyz, -r0.wwww, r3.xyzx
  74: mul_sat r1.xyz, r1.xyzx, r3.xyzx
  75: mul r1.xyz, r0.xyzx, r1.xyzx
  76: ne r0.w, l(0.000000, 0.000000, 0.000000, 0.000000), cb2[14].y
  77: movc r0.xyz, r0.wwww, r1.xyzx, r0.xyzx
  78: mul r0.xyz, r0.xyzx, cb2[12].xxxx
  79: mul r0.xyz, r0.xyzx, v3.xyzx
  80: max r0.w, v2.x, l(1.000000)
  81: min r0.w, r0.w, l(1000000.000000)
  82: eq r1.x, v1.x, v2.x
  83: movc r0.w, r1.x, l(1.000000), r0.w
  84: mul o0.xyz, r0.xyzx, r0.wwww
  85: mov o0.w, l(1.000000)
  86: ret 
// Approximately 0 instruction slots used


//////////////////////////////////////////////////////
Keywords: DIRECTIONAL FOG_EXP2 SHADOWS_SCREEN _BUILTIN_SURFACE_TYPE_TRANSPARENT
-- Vertex shader for "d3d11":
// No shader variant for this keyword set. The closest match will be used instead.

-- Hardware tier variant: Tier 1
-- Fragment shader for "d3d11":
// Stats: 73 math, 4 temp registers, 5 textures
Set 2D Texture "_CameraDepthTexture" to slot 0
Set 2D Texture "_MainTex" to slot 1
Set 2D Texture "_Noise" to slot 2
Set 2D Texture "_Flow" to slot 3
Set 2D Texture "_Mask" to slot 4

Constant Buffer "UnityPerCamera" (144 bytes) on slot 0 {
  Vector4 _Time at 0
  Vector3 _WorldSpaceCameraPos at 64
  Vector4 _ProjectionParams at 80
  Vector4 _ScreenParams at 96
  Vector4 _ZBufferParams at 112
  Vector4 unity_OrthoParams at 128
}
Constant Buffer "UnityPerFrame" (368 bytes) on slot 1 {
  Matrix4x4 unity_MatrixV at 144
  Matrix4x4 unity_MatrixVP at 272
}
Constant Buffer "UnityPerMaterial" (240 bytes) on slot 2 {
  Float _Textureopacity at 0
  Float _Fresnelpower at 4
  Float _Fresnelscale at 8
  Float _Multiply_texture at 12
  Float _Useonlycolor at 16
  Float _Use_fresnel at 20
  Vector4 _MainTex_ST at 48
  Vector4 _Noise_ST at 80
  Vector4 _Flow_ST at 112
  Vector4 _Mask_ST at 144
  Vector4 _SpeedMainTexUVNoiseZW at 160
  Vector4 _DistortionSpeedXYPowerZ at 176
  Float _Emission at 192
  Float _Opacity at 224
  Float _Usecenterglow at 228
  Float _Usedepth at 232
  Float _Depthpower at 236
}

Shader Disassembly:
//
// Generated by Microsoft (R) D3D Shader Disassembler
//
//
// Input signature:
//
// Name                 Index   Mask Register SysValue  Format   Used
// -------------------- ----- ------ -------- -------- ------- ------
// SV_POSITION              0   xyzw        0      POS   float   xy  
// INTERP                   0   xyzw        1     NONE   float   xyzw
// INTERP                   1   xyzw        2     NONE   float   x   
// INTERP                   2   xyzw        3     NONE   float   xyzw
// INTERP                   3   xyz         4     NONE   float   xyz 
// INTERP                   4   xyz         5     NONE   float   xyz 
// SV_IsFrontFace           0   x           6    FFACE    uint   x   
//
//
// Output signature:
//
// Name                 Index   Mask Register SysValue  Format   Used
// -------------------- ----- ------ -------- -------- ------- ------
// SV_TARGET                0   xyzw        0   TARGET   float   xyzw
//
      ps_4_0
      dcl_constantbuffer CB0[9], immediateIndexed
      dcl_constantbuffer CB1[21], immediateIndexed
      dcl_constantbuffer CB2[15], immediateIndexed
      dcl_sampler s0, mode_default
      dcl_sampler s1, mode_default
      dcl_sampler s2, mode_default
      dcl_sampler s3, mode_default
      dcl_sampler s4, mode_default
      dcl_resource_texture2d (float,float,float,float) t0
      dcl_resource_texture2d (float,float,float,float) t1
      dcl_resource_texture2d (float,float,float,float) t2
      dcl_resource_texture2d (float,float,float,float) t3
      dcl_resource_texture2d (float,float,float,float) t4
      dcl_input_ps_siv linear noperspective v0.xy, position
      dcl_input_ps linear v1.xyzw
      dcl_input_ps linear v2.x
      dcl_input_ps linear v3.xyzw
      dcl_input_ps linear v4.xyz
      dcl_input_ps linear v5.xyz
      dcl_input_ps_sgv constant v6.x, is_front_face
      dcl_output o0.xyzw
      dcl_temps 4
   0: dp3 r0.x, v5.xyzx, v5.xyzx
   1: sqrt r0.x, r0.x
   2: div r0.x, l(1.000000, 1.000000, 1.000000, 1.000000), r0.x
   3: mul r0.xyz, r0.xxxx, v5.xyzx
   4: dp3 r0.w, r0.xyzx, r0.xyzx
   5: rsq r0.w, r0.w
   6: mul r0.xyz, r0.wwww, r0.xyzx
   7: add r1.xyz, -v4.xyzx, cb0[4].xyzx
   8: dp3 r0.w, r1.xyzx, r1.xyzx
   9: rsq r0.w, r0.w
  10: mul r1.xyz, r0.wwww, r1.xyzx
  11: eq r0.w, cb0[8].w, l(0.000000)
  12: movc r2.x, r0.w, r1.x, cb1[9].z
  13: movc r2.y, r0.w, r1.y, cb1[10].z
  14: movc r2.z, r0.w, r1.z, cb1[11].z
  15: dp3_sat r0.x, r0.xyzx, r2.xyzx
  16: add r0.x, -r0.x, l(1.000000)
  17: log r0.x, r0.x
  18: mul r0.x, r0.x, cb2[0].y
  19: exp r0.x, r0.x
  20: mul_sat r0.x, r0.x, cb2[0].z
  21: and r0.x, r0.x, v6.x
  22: lt r0.y, cb0[5].x, l(0.000000)
  23: add r0.z, -v0.y, cb0[6].y
  24: movc r1.y, r0.y, r0.z, v0.y
  25: mov r1.x, v0.x
  26: div r1.xy, r1.xyxx, cb0[6].xyxx
  27: add r1.z, -r1.y, l(1.000000)
  28: sample r1.xyzw, r1.xzxx, t0.xyzw, s0
  29: mad r0.y, cb0[7].x, r1.x, cb0[7].y
  30: div r0.y, l(1.000000, 1.000000, 1.000000, 1.000000), r0.y
  31: mul r0.z, v4.y, cb1[18].w
  32: mad r0.z, cb1[17].w, v4.x, r0.z
  33: mad r0.z, cb1[19].w, v4.z, r0.z
  34: add r0.z, r0.z, cb1[20].w
  35: mad r0.y, r0.y, cb0[5].z, -r0.z
  36: div_sat r0.y, r0.y, cb2[14].w
  37: ge r0.z, l(0.000000), r0.y
  38: and r0.z, r0.z, l(0x3f800000)
  39: add r0.w, -r0.y, l(1.000000)
  40: mad r0.y, r0.z, r0.w, r0.y
  41: add r0.x, -r0.y, r0.x
  42: add r0.x, r0.x, l(1.000000)
  43: mul r0.z, v1.w, l(1.000100)
  44: ge r0.w, r0.z, -r0.z
  45: frc r0.z, |r0.z|
  46: movc r0.z, r0.w, r0.z, -r0.z
  47: mul r0.z, r0.z, l(0.999900)
  48: div r1.xy, cb2[10].zwzz, cb2[5].xyxx
  49: mad r0.zw, cb0[0].yyyy, r1.xxxy, r0.zzzz
  50: add r0.zw, r0.zzzw, v1.xxxy
  51: mad r0.zw, r0.zzzw, cb2[5].xxxy, cb2[5].zzzw
  52: sample r1.xyzw, r0.zwzz, t2.xyzw, s2
  53: div r0.zw, cb2[11].xxxy, cb2[7].xxxy
  54: mad r0.zw, r0.zzzw, cb0[0].yyyy, v1.xxxy
  55: mad r0.zw, r0.zzzw, cb2[7].xxxy, cb2[7].zzzw
  56: sample r2.xyzw, r0.zwzz, t3.xyzw, s3
  57: mad r0.zw, v1.xxxy, cb2[9].xxxy, cb2[9].zzzw
  58: sample r3.xyzw, r0.zwzz, t4.xyzw, s4
  59: mul r0.zw, r2.xxxy, r3.xxxy
  60: mul r0.zw, r0.zzzw, cb2[11].zzzz
  61: div r0.zw, r0.zzzw, cb2[3].xxxy
  62: div r2.xy, cb2[10].xyxx, cb2[3].xyxx
  63: mad r2.xy, r2.xyxx, cb0[0].yyyy, v1.xyxx
  64: add r0.zw, -r0.zzzw, r2.xxxy
  65: mad r0.zw, r0.zzzw, cb2[3].xxxy, cb2[3].zzzw
  66: sample r2.xyzw, r0.zwzz, t1.xyzw, s1
  67: mul r1.xyzw, r1.wxyz, r2.wxyz
  68: ne r0.zw, l(0.000000, 0.000000, 0.000000, 0.000000), cb2[1].xxxy
  69: movc r2.xyz, r0.zzzz, r1.xxxx, r1.yzwy
  70: max r2.xyz, r0.xxxx, r2.xyzx
  71: mad_sat r0.x, r1.x, cb2[0].x, r0.x
  72: movc r1.yzw, r0.wwww, r2.xxyz, r1.yyzw
  73: add r0.z, -v1.z, l(1.000000)
  74: add_sat r2.xyz, -r0.zzzz, r3.xyzx
  75: mul_sat r2.xyz, r2.xyzx, r3.xyzx
  76: mul r2.xyz, r1.yzwy, r2.xyzx
  77: ne r3.xy, l(0.000000, 0.000000, 0.000000, 0.000000), cb2[14].yzyy
  78: movc r1.yzw, r3.xxxx, r2.xxyz, r1.yyzw
  79: mul r1.yzw, r1.yyzw, cb2[12].xxxx
  80: mul r1.yzw, r1.yyzw, v3.xxyz
  81: max r0.z, v2.x, l(1.000000)
  82: min r0.z, r0.z, l(1000000.000000)
  83: eq r2.x, v1.x, v2.x
  84: movc r0.z, r2.x, l(1.000000), r0.z
  85: mul o0.xyz, r1.yzwy, r0.zzzz
  86: mul r0.z, r0.x, r1.x
  87: ne r1.y, l(0.000000, 0.000000, 0.000000, 0.000000), cb2[0].w
  88: movc r0.x, r1.y, r0.z, r0.x
  89: movc r0.x, r0.w, r0.x, r1.x
  90: mul r0.y, r0.y, r0.x
  91: movc r0.x, r3.y, r0.y, r0.x
  92: mul r0.x, r0.x, v3.w
  93: mul_sat o0.w, r0.x, cb2[14].x
  94: ret 
// Approximately 0 instruction slots used


//////////////////////////////////////////////////////
Keywords: DIRECTIONAL FOG_EXP2 SHADOWS_SCREEN
-- Vertex shader for "d3d11":
// No shader variant for this keyword set. The closest match will be used instead.

-- Hardware tier variant: Tier 1
-- Fragment shader for "d3d11":
// Stats: 68 math, 4 temp registers, 5 textures
Set 2D Texture "_CameraDepthTexture" to slot 0
Set 2D Texture "_MainTex" to slot 1
Set 2D Texture "_Noise" to slot 2
Set 2D Texture "_Flow" to slot 3
Set 2D Texture "_Mask" to slot 4

Constant Buffer "UnityPerCamera" (144 bytes) on slot 0 {
  Vector4 _Time at 0
  Vector3 _WorldSpaceCameraPos at 64
  Vector4 _ProjectionParams at 80
  Vector4 _ScreenParams at 96
  Vector4 _ZBufferParams at 112
  Vector4 unity_OrthoParams at 128
}
Constant Buffer "UnityPerFrame" (368 bytes) on slot 1 {
  Matrix4x4 unity_MatrixV at 144
  Matrix4x4 unity_MatrixVP at 272
}
Constant Buffer "UnityPerMaterial" (240 bytes) on slot 2 {
  Float _Fresnelpower at 4
  Float _Fresnelscale at 8
  Float _Useonlycolor at 16
  Float _Use_fresnel at 20
  Vector4 _MainTex_ST at 48
  Vector4 _Noise_ST at 80
  Vector4 _Flow_ST at 112
  Vector4 _Mask_ST at 144
  Vector4 _SpeedMainTexUVNoiseZW at 160
  Vector4 _DistortionSpeedXYPowerZ at 176
  Float _Emission at 192
  Float _Usecenterglow at 228
  Float _Depthpower at 236
}

Shader Disassembly:
//
// Generated by Microsoft (R) D3D Shader Disassembler
//
//
// Input signature:
//
// Name                 Index   Mask Register SysValue  Format   Used
// -------------------- ----- ------ -------- -------- ------- ------
// SV_POSITION              0   xyzw        0      POS   float   xy  
// INTERP                   0   xyzw        1     NONE   float   xyzw
// INTERP                   1   xyzw        2     NONE   float   x   
// INTERP                   2   xyzw        3     NONE   float   xyz 
// INTERP                   3   xyz         4     NONE   float   xyz 
// INTERP                   4   xyz         5     NONE   float   xyz 
// SV_IsFrontFace           0   x           6    FFACE    uint   x   
//
//
// Output signature:
//
// Name                 Index   Mask Register SysValue  Format   Used
// -------------------- ----- ------ -------- -------- ------- ------
// SV_TARGET                0   xyzw        0   TARGET   float   xyzw
//
      ps_4_0
      dcl_constantbuffer CB0[9], immediateIndexed
      dcl_constantbuffer CB1[21], immediateIndexed
      dcl_constantbuffer CB2[15], immediateIndexed
      dcl_sampler s0, mode_default
      dcl_sampler s1, mode_default
      dcl_sampler s2, mode_default
      dcl_sampler s3, mode_default
      dcl_sampler s4, mode_default
      dcl_resource_texture2d (float,float,float,float) t0
      dcl_resource_texture2d (float,float,float,float) t1
      dcl_resource_texture2d (float,float,float,float) t2
      dcl_resource_texture2d (float,float,float,float) t3
      dcl_resource_texture2d (float,float,float,float) t4
      dcl_input_ps_siv linear noperspective v0.xy, position
      dcl_input_ps linear v1.xyzw
      dcl_input_ps linear v2.x
      dcl_input_ps linear v3.xyz
      dcl_input_ps linear v4.xyz
      dcl_input_ps linear v5.xyz
      dcl_input_ps_sgv constant v6.x, is_front_face
      dcl_output o0.xyzw
      dcl_temps 4
   0: dp3 r0.x, v5.xyzx, v5.xyzx
   1: sqrt r0.x, r0.x
   2: div r0.x, l(1.000000, 1.000000, 1.000000, 1.000000), r0.x
   3: mul r0.xyz, r0.xxxx, v5.xyzx
   4: dp3 r0.w, r0.xyzx, r0.xyzx
   5: rsq r0.w, r0.w
   6: mul r0.xyz, r0.wwww, r0.xyzx
   7: add r1.xyz, -v4.xyzx, cb0[4].xyzx
   8: dp3 r0.w, r1.xyzx, r1.xyzx
   9: rsq r0.w, r0.w
  10: mul r1.xyz, r0.wwww, r1.xyzx
  11: eq r0.w, cb0[8].w, l(0.000000)
  12: movc r2.x, r0.w, r1.x, cb1[9].z
  13: movc r2.y, r0.w, r1.y, cb1[10].z
  14: movc r2.z, r0.w, r1.z, cb1[11].z
  15: dp3_sat r0.x, r0.xyzx, r2.xyzx
  16: add r0.x, -r0.x, l(1.000000)
  17: log r0.x, r0.x
  18: mul r0.x, r0.x, cb2[0].y
  19: exp r0.x, r0.x
  20: mul_sat r0.x, r0.x, cb2[0].z
  21: and r0.x, r0.x, v6.x
  22: lt r0.y, cb0[5].x, l(0.000000)
  23: add r0.z, -v0.y, cb0[6].y
  24: movc r1.y, r0.y, r0.z, v0.y
  25: mov r1.x, v0.x
  26: div r1.xy, r1.xyxx, cb0[6].xyxx
  27: add r1.z, -r1.y, l(1.000000)
  28: sample r1.xyzw, r1.xzxx, t0.xyzw, s0
  29: mad r0.y, cb0[7].x, r1.x, cb0[7].y
  30: div r0.y, l(1.000000, 1.000000, 1.000000, 1.000000), r0.y
  31: mul r0.z, v4.y, cb1[18].w
  32: mad r0.z, cb1[17].w, v4.x, r0.z
  33: mad r0.z, cb1[19].w, v4.z, r0.z
  34: add r0.z, r0.z, cb1[20].w
  35: mad r0.y, r0.y, cb0[5].z, -r0.z
  36: div_sat r0.y, r0.y, cb2[14].w
  37: ge r0.z, l(0.000000), r0.y
  38: and r0.z, r0.z, l(0x3f800000)
  39: add r0.w, -r0.y, l(1.000000)
  40: mad r0.y, r0.z, r0.w, r0.y
  41: add r0.x, -r0.y, r0.x
  42: add r0.x, r0.x, l(1.000000)
  43: mul r0.y, v1.w, l(1.000100)
  44: ge r0.z, r0.y, -r0.y
  45: frc r0.y, |r0.y|
  46: movc r0.y, r0.z, r0.y, -r0.y
  47: mul r0.y, r0.y, l(0.999900)
  48: div r0.zw, cb2[10].zzzw, cb2[5].xxxy
  49: mad r0.yz, cb0[0].yyyy, r0.zzwz, r0.yyyy
  50: add r0.yz, r0.yyzy, v1.xxyx
  51: mad r0.yz, r0.yyzy, cb2[5].xxyx, cb2[5].zzwz
  52: sample r1.xyzw, r0.yzyy, t2.xyzw, s2
  53: div r0.yz, cb2[11].xxyx, cb2[7].xxyx
  54: mad r0.yz, r0.yyzy, cb0[0].yyyy, v1.xxyx
  55: mad r0.yz, r0.yyzy, cb2[7].xxyx, cb2[7].zzwz
  56: sample r2.xyzw, r0.yzyy, t3.xyzw, s3
  57: mad r0.yz, v1.xxyx, cb2[9].xxyx, cb2[9].zzwz
  58: sample r3.xyzw, r0.yzyy, t4.xyzw, s4
  59: mul r0.yz, r2.xxyx, r3.xxyx
  60: mul r0.yz, r0.yyzy, cb2[11].zzzz
  61: div r0.yz, r0.yyzy, cb2[3].xxyx
  62: div r2.xy, cb2[10].xyxx, cb2[3].xyxx
  63: mad r2.xy, r2.xyxx, cb0[0].yyyy, v1.xyxx
  64: add r0.yz, -r0.yyzy, r2.xxyx
  65: mad r0.yz, r0.yyzy, cb2[3].xxyx, cb2[3].zzwz
  66: sample r2.xyzw, r0.yzyy, t1.xyzw, s1
  67: mul r1.xyzw, r1.wxyz, r2.wxyz
  68: ne r0.yz, l(0.000000, 0.000000, 0.000000, 0.000000), cb2[1].xxyx
  69: movc r2.xyz, r0.yyyy, r1.xxxx, r1.yzwy
  70: max r0.xyw, r0.xxxx, r2.xyxz
  71: movc r0.xyz, r0.zzzz, r0.xywx, r1.yzwy
  72: add r0.w, -v1.z, l(1.000000)
  73: add_sat r1.xyz, -r0.wwww, r3.xyzx
  74: mul_sat r1.xyz, r1.xyzx, r3.xyzx
  75: mul r1.xyz, r0.xyzx, r1.xyzx
  76: ne r0.w, l(0.000000, 0.000000, 0.000000, 0.000000), cb2[14].y
  77: movc r0.xyz, r0.wwww, r1.xyzx, r0.xyzx
  78: mul r0.xyz, r0.xyzx, cb2[12].xxxx
  79: mul r0.xyz, r0.xyzx, v3.xyzx
  80: max r0.w, v2.x, l(1.000000)
  81: min r0.w, r0.w, l(1000000.000000)
  82: eq r1.x, v1.x, v2.x
  83: movc r0.w, r1.x, l(1.000000), r0.w
  84: mul o0.xyz, r0.xyzx, r0.wwww
  85: mov o0.w, l(1.000000)
  86: ret 
// Approximately 0 instruction slots used


//////////////////////////////////////////////////////
Keywords: DIRECTIONAL FOG_EXP2 LIGHTPROBE_SH SHADOWS_SCREEN _BUILTIN_SURFACE_TYPE_TRANSPARENT
-- Vertex shader for "d3d11":
// No shader variant for this keyword set. The closest match will be used instead.

-- Hardware tier variant: Tier 1
-- Fragment shader for "d3d11":
// Stats: 73 math, 4 temp registers, 5 textures
Set 2D Texture "_CameraDepthTexture" to slot 0
Set 2D Texture "_MainTex" to slot 1
Set 2D Texture "_Noise" to slot 2
Set 2D Texture "_Flow" to slot 3
Set 2D Texture "_Mask" to slot 4

Constant Buffer "UnityPerCamera" (144 bytes) on slot 0 {
  Vector4 _Time at 0
  Vector3 _WorldSpaceCameraPos at 64
  Vector4 _ProjectionParams at 80
  Vector4 _ScreenParams at 96
  Vector4 _ZBufferParams at 112
  Vector4 unity_OrthoParams at 128
}
Constant Buffer "UnityPerFrame" (368 bytes) on slot 1 {
  Matrix4x4 unity_MatrixV at 144
  Matrix4x4 unity_MatrixVP at 272
}
Constant Buffer "UnityPerMaterial" (240 bytes) on slot 2 {
  Float _Textureopacity at 0
  Float _Fresnelpower at 4
  Float _Fresnelscale at 8
  Float _Multiply_texture at 12
  Float _Useonlycolor at 16
  Float _Use_fresnel at 20
  Vector4 _MainTex_ST at 48
  Vector4 _Noise_ST at 80
  Vector4 _Flow_ST at 112
  Vector4 _Mask_ST at 144
  Vector4 _SpeedMainTexUVNoiseZW at 160
  Vector4 _DistortionSpeedXYPowerZ at 176
  Float _Emission at 192
  Float _Opacity at 224
  Float _Usecenterglow at 228
  Float _Usedepth at 232
  Float _Depthpower at 236
}

Shader Disassembly:
//
// Generated by Microsoft (R) D3D Shader Disassembler
//
//
// Input signature:
//
// Name                 Index   Mask Register SysValue  Format   Used
// -------------------- ----- ------ -------- -------- ------- ------
// SV_POSITION              0   xyzw        0      POS   float   xy  
// INTERP                   0   xyzw        1     NONE   float   xyzw
// INTERP                   1   xyzw        2     NONE   float   x   
// INTERP                   2   xyzw        3     NONE   float   xyzw
// INTERP                   3   xyz         4     NONE   float   xyz 
// INTERP                   4   xyz         5     NONE   float   xyz 
// SV_IsFrontFace           0   x           6    FFACE    uint   x   
//
//
// Output signature:
//
// Name                 Index   Mask Register SysValue  Format   Used
// -------------------- ----- ------ -------- -------- ------- ------
// SV_TARGET                0   xyzw        0   TARGET   float   xyzw
//
      ps_4_0
      dcl_constantbuffer CB0[9], immediateIndexed
      dcl_constantbuffer CB1[21], immediateIndexed
      dcl_constantbuffer CB2[15], immediateIndexed
      dcl_sampler s0, mode_default
      dcl_sampler s1, mode_default
      dcl_sampler s2, mode_default
      dcl_sampler s3, mode_default
      dcl_sampler s4, mode_default
      dcl_resource_texture2d (float,float,float,float) t0
      dcl_resource_texture2d (float,float,float,float) t1
      dcl_resource_texture2d (float,float,float,float) t2
      dcl_resource_texture2d (float,float,float,float) t3
      dcl_resource_texture2d (float,float,float,float) t4
      dcl_input_ps_siv linear noperspective v0.xy, position
      dcl_input_ps linear v1.xyzw
      dcl_input_ps linear v2.x
      dcl_input_ps linear v3.xyzw
      dcl_input_ps linear v4.xyz
      dcl_input_ps linear v5.xyz
      dcl_input_ps_sgv constant v6.x, is_front_face
      dcl_output o0.xyzw
      dcl_temps 4
   0: dp3 r0.x, v5.xyzx, v5.xyzx
   1: sqrt r0.x, r0.x
   2: div r0.x, l(1.000000, 1.000000, 1.000000, 1.000000), r0.x
   3: mul r0.xyz, r0.xxxx, v5.xyzx
   4: dp3 r0.w, r0.xyzx, r0.xyzx
   5: rsq r0.w, r0.w
   6: mul r0.xyz, r0.wwww, r0.xyzx
   7: add r1.xyz, -v4.xyzx, cb0[4].xyzx
   8: dp3 r0.w, r1.xyzx, r1.xyzx
   9: rsq r0.w, r0.w
  10: mul r1.xyz, r0.wwww, r1.xyzx
  11: eq r0.w, cb0[8].w, l(0.000000)
  12: movc r2.x, r0.w, r1.x, cb1[9].z
  13: movc r2.y, r0.w, r1.y, cb1[10].z
  14: movc r2.z, r0.w, r1.z, cb1[11].z
  15: dp3_sat r0.x, r0.xyzx, r2.xyzx
  16: add r0.x, -r0.x, l(1.000000)
  17: log r0.x, r0.x
  18: mul r0.x, r0.x, cb2[0].y
  19: exp r0.x, r0.x
  20: mul_sat r0.x, r0.x, cb2[0].z
  21: and r0.x, r0.x, v6.x
  22: lt r0.y, cb0[5].x, l(0.000000)
  23: add r0.z, -v0.y, cb0[6].y
  24: movc r1.y, r0.y, r0.z, v0.y
  25: mov r1.x, v0.x
  26: div r1.xy, r1.xyxx, cb0[6].xyxx
  27: add r1.z, -r1.y, l(1.000000)
  28: sample r1.xyzw, r1.xzxx, t0.xyzw, s0
  29: mad r0.y, cb0[7].x, r1.x, cb0[7].y
  30: div r0.y, l(1.000000, 1.000000, 1.000000, 1.000000), r0.y
  31: mul r0.z, v4.y, cb1[18].w
  32: mad r0.z, cb1[17].w, v4.x, r0.z
  33: mad r0.z, cb1[19].w, v4.z, r0.z
  34: add r0.z, r0.z, cb1[20].w
  35: mad r0.y, r0.y, cb0[5].z, -r0.z
  36: div_sat r0.y, r0.y, cb2[14].w
  37: ge r0.z, l(0.000000), r0.y
  38: and r0.z, r0.z, l(0x3f800000)
  39: add r0.w, -r0.y, l(1.000000)
  40: mad r0.y, r0.z, r0.w, r0.y
  41: add r0.x, -r0.y, r0.x
  42: add r0.x, r0.x, l(1.000000)
  43: mul r0.z, v1.w, l(1.000100)
  44: ge r0.w, r0.z, -r0.z
  45: frc r0.z, |r0.z|
  46: movc r0.z, r0.w, r0.z, -r0.z
  47: mul r0.z, r0.z, l(0.999900)
  48: div r1.xy, cb2[10].zwzz, cb2[5].xyxx
  49: mad r0.zw, cb0[0].yyyy, r1.xxxy, r0.zzzz
  50: add r0.zw, r0.zzzw, v1.xxxy
  51: mad r0.zw, r0.zzzw, cb2[5].xxxy, cb2[5].zzzw
  52: sample r1.xyzw, r0.zwzz, t2.xyzw, s2
  53: div r0.zw, cb2[11].xxxy, cb2[7].xxxy
  54: mad r0.zw, r0.zzzw, cb0[0].yyyy, v1.xxxy
  55: mad r0.zw, r0.zzzw, cb2[7].xxxy, cb2[7].zzzw
  56: sample r2.xyzw, r0.zwzz, t3.xyzw, s3
  57: mad r0.zw, v1.xxxy, cb2[9].xxxy, cb2[9].zzzw
  58: sample r3.xyzw, r0.zwzz, t4.xyzw, s4
  59: mul r0.zw, r2.xxxy, r3.xxxy
  60: mul r0.zw, r0.zzzw, cb2[11].zzzz
  61: div r0.zw, r0.zzzw, cb2[3].xxxy
  62: div r2.xy, cb2[10].xyxx, cb2[3].xyxx
  63: mad r2.xy, r2.xyxx, cb0[0].yyyy, v1.xyxx
  64: add r0.zw, -r0.zzzw, r2.xxxy
  65: mad r0.zw, r0.zzzw, cb2[3].xxxy, cb2[3].zzzw
  66: sample r2.xyzw, r0.zwzz, t1.xyzw, s1
  67: mul r1.xyzw, r1.wxyz, r2.wxyz
  68: ne r0.zw, l(0.000000, 0.000000, 0.000000, 0.000000), cb2[1].xxxy
  69: movc r2.xyz, r0.zzzz, r1.xxxx, r1.yzwy
  70: max r2.xyz, r0.xxxx, r2.xyzx
  71: mad_sat r0.x, r1.x, cb2[0].x, r0.x
  72: movc r1.yzw, r0.wwww, r2.xxyz, r1.yyzw
  73: add r0.z, -v1.z, l(1.000000)
  74: add_sat r2.xyz, -r0.zzzz, r3.xyzx
  75: mul_sat r2.xyz, r2.xyzx, r3.xyzx
  76: mul r2.xyz, r1.yzwy, r2.xyzx
  77: ne r3.xy, l(0.000000, 0.000000, 0.000000, 0.000000), cb2[14].yzyy
  78: movc r1.yzw, r3.xxxx, r2.xxyz, r1.yyzw
  79: mul r1.yzw, r1.yyzw, cb2[12].xxxx
  80: mul r1.yzw, r1.yyzw, v3.xxyz
  81: max r0.z, v2.x, l(1.000000)
  82: min r0.z, r0.z, l(1000000.000000)
  83: eq r2.x, v1.x, v2.x
  84: movc r0.z, r2.x, l(1.000000), r0.z
  85: mul o0.xyz, r1.yzwy, r0.zzzz
  86: mul r0.z, r0.x, r1.x
  87: ne r1.y, l(0.000000, 0.000000, 0.000000, 0.000000), cb2[0].w
  88: movc r0.x, r1.y, r0.z, r0.x
  89: movc r0.x, r0.w, r0.x, r1.x
  90: mul r0.y, r0.y, r0.x
  91: movc r0.x, r3.y, r0.y, r0.x
  92: mul r0.x, r0.x, v3.w
  93: mul_sat o0.w, r0.x, cb2[14].x
  94: ret 
// Approximately 0 instruction slots used


//////////////////////////////////////////////////////
Keywords: DIRECTIONAL FOG_EXP2 LIGHTPROBE_SH SHADOWS_SCREEN
-- Vertex shader for "d3d11":
// No shader variant for this keyword set. The closest match will be used instead.

-- Hardware tier variant: Tier 1
-- Fragment shader for "d3d11":
// Stats: 68 math, 4 temp registers, 5 textures
Set 2D Texture "_CameraDepthTexture" to slot 0
Set 2D Texture "_MainTex" to slot 1
Set 2D Texture "_Noise" to slot 2
Set 2D Texture "_Flow" to slot 3
Set 2D Texture "_Mask" to slot 4

Constant Buffer "UnityPerCamera" (144 bytes) on slot 0 {
  Vector4 _Time at 0
  Vector3 _WorldSpaceCameraPos at 64
  Vector4 _ProjectionParams at 80
  Vector4 _ScreenParams at 96
  Vector4 _ZBufferParams at 112
  Vector4 unity_OrthoParams at 128
}
Constant Buffer "UnityPerFrame" (368 bytes) on slot 1 {
  Matrix4x4 unity_MatrixV at 144
  Matrix4x4 unity_MatrixVP at 272
}
Constant Buffer "UnityPerMaterial" (240 bytes) on slot 2 {
  Float _Fresnelpower at 4
  Float _Fresnelscale at 8
  Float _Useonlycolor at 16
  Float _Use_fresnel at 20
  Vector4 _MainTex_ST at 48
  Vector4 _Noise_ST at 80
  Vector4 _Flow_ST at 112
  Vector4 _Mask_ST at 144
  Vector4 _SpeedMainTexUVNoiseZW at 160
  Vector4 _DistortionSpeedXYPowerZ at 176
  Float _Emission at 192
  Float _Usecenterglow at 228
  Float _Depthpower at 236
}

Shader Disassembly:
//
// Generated by Microsoft (R) D3D Shader Disassembler
//
//
// Input signature:
//
// Name                 Index   Mask Register SysValue  Format   Used
// -------------------- ----- ------ -------- -------- ------- ------
// SV_POSITION              0   xyzw        0      POS   float   xy  
// INTERP                   0   xyzw        1     NONE   float   xyzw
// INTERP                   1   xyzw        2     NONE   float   x   
// INTERP                   2   xyzw        3     NONE   float   xyz 
// INTERP                   3   xyz         4     NONE   float   xyz 
// INTERP                   4   xyz         5     NONE   float   xyz 
// SV_IsFrontFace           0   x           6    FFACE    uint   x   
//
//
// Output signature:
//
// Name                 Index   Mask Register SysValue  Format   Used
// -------------------- ----- ------ -------- -------- ------- ------
// SV_TARGET                0   xyzw        0   TARGET   float   xyzw
//
      ps_4_0
      dcl_constantbuffer CB0[9], immediateIndexed
      dcl_constantbuffer CB1[21], immediateIndexed
      dcl_constantbuffer CB2[15], immediateIndexed
      dcl_sampler s0, mode_default
      dcl_sampler s1, mode_default
      dcl_sampler s2, mode_default
      dcl_sampler s3, mode_default
      dcl_sampler s4, mode_default
      dcl_resource_texture2d (float,float,float,float) t0
      dcl_resource_texture2d (float,float,float,float) t1
      dcl_resource_texture2d (float,float,float,float) t2
      dcl_resource_texture2d (float,float,float,float) t3
      dcl_resource_texture2d (float,float,float,float) t4
      dcl_input_ps_siv linear noperspective v0.xy, position
      dcl_input_ps linear v1.xyzw
      dcl_input_ps linear v2.x
      dcl_input_ps linear v3.xyz
      dcl_input_ps linear v4.xyz
      dcl_input_ps linear v5.xyz
      dcl_input_ps_sgv constant v6.x, is_front_face
      dcl_output o0.xyzw
      dcl_temps 4
   0: dp3 r0.x, v5.xyzx, v5.xyzx
   1: sqrt r0.x, r0.x
   2: div r0.x, l(1.000000, 1.000000, 1.000000, 1.000000), r0.x
   3: mul r0.xyz, r0.xxxx, v5.xyzx
   4: dp3 r0.w, r0.xyzx, r0.xyzx
   5: rsq r0.w, r0.w
   6: mul r0.xyz, r0.wwww, r0.xyzx
   7: add r1.xyz, -v4.xyzx, cb0[4].xyzx
   8: dp3 r0.w, r1.xyzx, r1.xyzx
   9: rsq r0.w, r0.w
  10: mul r1.xyz, r0.wwww, r1.xyzx
  11: eq r0.w, cb0[8].w, l(0.000000)
  12: movc r2.x, r0.w, r1.x, cb1[9].z
  13: movc r2.y, r0.w, r1.y, cb1[10].z
  14: movc r2.z, r0.w, r1.z, cb1[11].z
  15: dp3_sat r0.x, r0.xyzx, r2.xyzx
  16: add r0.x, -r0.x, l(1.000000)
  17: log r0.x, r0.x
  18: mul r0.x, r0.x, cb2[0].y
  19: exp r0.x, r0.x
  20: mul_sat r0.x, r0.x, cb2[0].z
  21: and r0.x, r0.x, v6.x
  22: lt r0.y, cb0[5].x, l(0.000000)
  23: add r0.z, -v0.y, cb0[6].y
  24: movc r1.y, r0.y, r0.z, v0.y
  25: mov r1.x, v0.x
  26: div r1.xy, r1.xyxx, cb0[6].xyxx
  27: add r1.z, -r1.y, l(1.000000)
  28: sample r1.xyzw, r1.xzxx, t0.xyzw, s0
  29: mad r0.y, cb0[7].x, r1.x, cb0[7].y
  30: div r0.y, l(1.000000, 1.000000, 1.000000, 1.000000), r0.y
  31: mul r0.z, v4.y, cb1[18].w
  32: mad r0.z, cb1[17].w, v4.x, r0.z
  33: mad r0.z, cb1[19].w, v4.z, r0.z
  34: add r0.z, r0.z, cb1[20].w
  35: mad r0.y, r0.y, cb0[5].z, -r0.z
  36: div_sat r0.y, r0.y, cb2[14].w
  37: ge r0.z, l(0.000000), r0.y
  38: and r0.z, r0.z, l(0x3f800000)
  39: add r0.w, -r0.y, l(1.000000)
  40: mad r0.y, r0.z, r0.w, r0.y
  41: add r0.x, -r0.y, r0.x
  42: add r0.x, r0.x, l(1.000000)
  43: mul r0.y, v1.w, l(1.000100)
  44: ge r0.z, r0.y, -r0.y
  45: frc r0.y, |r0.y|
  46: movc r0.y, r0.z, r0.y, -r0.y
  47: mul r0.y, r0.y, l(0.999900)
  48: div r0.zw, cb2[10].zzzw, cb2[5].xxxy
  49: mad r0.yz, cb0[0].yyyy, r0.zzwz, r0.yyyy
  50: add r0.yz, r0.yyzy, v1.xxyx
  51: mad r0.yz, r0.yyzy, cb2[5].xxyx, cb2[5].zzwz
  52: sample r1.xyzw, r0.yzyy, t2.xyzw, s2
  53: div r0.yz, cb2[11].xxyx, cb2[7].xxyx
  54: mad r0.yz, r0.yyzy, cb0[0].yyyy, v1.xxyx
  55: mad r0.yz, r0.yyzy, cb2[7].xxyx, cb2[7].zzwz
  56: sample r2.xyzw, r0.yzyy, t3.xyzw, s3
  57: mad r0.yz, v1.xxyx, cb2[9].xxyx, cb2[9].zzwz
  58: sample r3.xyzw, r0.yzyy, t4.xyzw, s4
  59: mul r0.yz, r2.xxyx, r3.xxyx
  60: mul r0.yz, r0.yyzy, cb2[11].zzzz
  61: div r0.yz, r0.yyzy, cb2[3].xxyx
  62: div r2.xy, cb2[10].xyxx, cb2[3].xyxx
  63: mad r2.xy, r2.xyxx, cb0[0].yyyy, v1.xyxx
  64: add r0.yz, -r0.yyzy, r2.xxyx
  65: mad r0.yz, r0.yyzy, cb2[3].xxyx, cb2[3].zzwz
  66: sample r2.xyzw, r0.yzyy, t1.xyzw, s1
  67: mul r1.xyzw, r1.wxyz, r2.wxyz
  68: ne r0.yz, l(0.000000, 0.000000, 0.000000, 0.000000), cb2[1].xxyx
  69: movc r2.xyz, r0.yyyy, r1.xxxx, r1.yzwy
  70: max r0.xyw, r0.xxxx, r2.xyxz
  71: movc r0.xyz, r0.zzzz, r0.xywx, r1.yzwy
  72: add r0.w, -v1.z, l(1.000000)
  73: add_sat r1.xyz, -r0.wwww, r3.xyzx
  74: mul_sat r1.xyz, r1.xyzx, r3.xyzx
  75: mul r1.xyz, r0.xyzx, r1.xyzx
  76: ne r0.w, l(0.000000, 0.000000, 0.000000, 0.000000), cb2[14].y
  77: movc r0.xyz, r0.wwww, r1.xyzx, r0.xyzx
  78: mul r0.xyz, r0.xyzx, cb2[12].xxxx
  79: mul r0.xyz, r0.xyzx, v3.xyzx
  80: max r0.w, v2.x, l(1.000000)
  81: min r0.w, r0.w, l(1000000.000000)
  82: eq r1.x, v1.x, v2.x
  83: movc r0.w, r1.x, l(1.000000), r0.w
  84: mul o0.xyz, r0.xyzx, r0.wwww
  85: mov o0.w, l(1.000000)
  86: ret 
// Approximately 0 instruction slots used


//////////////////////////////////////////////////////
Keywords: DIRECTIONAL FOG_EXP2 _BUILTIN_SURFACE_TYPE_TRANSPARENT
-- Vertex shader for "d3d11":
// No shader variant for this keyword set. The closest match will be used instead.

-- Hardware tier variant: Tier 1
-- Fragment shader for "d3d11":
// Stats: 73 math, 4 temp registers, 5 textures
Set 2D Texture "_CameraDepthTexture" to slot 0
Set 2D Texture "_MainTex" to slot 1
Set 2D Texture "_Noise" to slot 2
Set 2D Texture "_Flow" to slot 3
Set 2D Texture "_Mask" to slot 4

Constant Buffer "UnityPerCamera" (144 bytes) on slot 0 {
  Vector4 _Time at 0
  Vector3 _WorldSpaceCameraPos at 64
  Vector4 _ProjectionParams at 80
  Vector4 _ScreenParams at 96
  Vector4 _ZBufferParams at 112
  Vector4 unity_OrthoParams at 128
}
Constant Buffer "UnityPerFrame" (368 bytes) on slot 1 {
  Matrix4x4 unity_MatrixV at 144
  Matrix4x4 unity_MatrixVP at 272
}
Constant Buffer "UnityPerMaterial" (240 bytes) on slot 2 {
  Float _Textureopacity at 0
  Float _Fresnelpower at 4
  Float _Fresnelscale at 8
  Float _Multiply_texture at 12
  Float _Useonlycolor at 16
  Float _Use_fresnel at 20
  Vector4 _MainTex_ST at 48
  Vector4 _Noise_ST at 80
  Vector4 _Flow_ST at 112
  Vector4 _Mask_ST at 144
  Vector4 _SpeedMainTexUVNoiseZW at 160
  Vector4 _DistortionSpeedXYPowerZ at 176
  Float _Emission at 192
  Float _Opacity at 224
  Float _Usecenterglow at 228
  Float _Usedepth at 232
  Float _Depthpower at 236
}

Shader Disassembly:
//
// Generated by Microsoft (R) D3D Shader Disassembler
//
//
// Input signature:
//
// Name                 Index   Mask Register SysValue  Format   Used
// -------------------- ----- ------ -------- -------- ------- ------
// SV_POSITION              0   xyzw        0      POS   float   xy  
// INTERP                   0   xyzw        1     NONE   float   xyzw
// INTERP                   1   xyzw        2     NONE   float   x   
// INTERP                   2   xyzw        3     NONE   float   xyzw
// INTERP                   3   xyz         4     NONE   float   xyz 
// INTERP                   4   xyz         5     NONE   float   xyz 
// SV_IsFrontFace           0   x           6    FFACE    uint   x   
//
//
// Output signature:
//
// Name                 Index   Mask Register SysValue  Format   Used
// -------------------- ----- ------ -------- -------- ------- ------
// SV_TARGET                0   xyzw        0   TARGET   float   xyzw
//
      ps_4_0
      dcl_constantbuffer CB0[9], immediateIndexed
      dcl_constantbuffer CB1[21], immediateIndexed
      dcl_constantbuffer CB2[15], immediateIndexed
      dcl_sampler s0, mode_default
      dcl_sampler s1, mode_default
      dcl_sampler s2, mode_default
      dcl_sampler s3, mode_default
      dcl_sampler s4, mode_default
      dcl_resource_texture2d (float,float,float,float) t0
      dcl_resource_texture2d (float,float,float,float) t1
      dcl_resource_texture2d (float,float,float,float) t2
      dcl_resource_texture2d (float,float,float,float) t3
      dcl_resource_texture2d (float,float,float,float) t4
      dcl_input_ps_siv linear noperspective v0.xy, position
      dcl_input_ps linear v1.xyzw
      dcl_input_ps linear v2.x
      dcl_input_ps linear v3.xyzw
      dcl_input_ps linear v4.xyz
      dcl_input_ps linear v5.xyz
      dcl_input_ps_sgv constant v6.x, is_front_face
      dcl_output o0.xyzw
      dcl_temps 4
   0: dp3 r0.x, v5.xyzx, v5.xyzx
   1: sqrt r0.x, r0.x
   2: div r0.x, l(1.000000, 1.000000, 1.000000, 1.000000), r0.x
   3: mul r0.xyz, r0.xxxx, v5.xyzx
   4: dp3 r0.w, r0.xyzx, r0.xyzx
   5: rsq r0.w, r0.w
   6: mul r0.xyz, r0.wwww, r0.xyzx
   7: add r1.xyz, -v4.xyzx, cb0[4].xyzx
   8: dp3 r0.w, r1.xyzx, r1.xyzx
   9: rsq r0.w, r0.w
  10: mul r1.xyz, r0.wwww, r1.xyzx
  11: eq r0.w, cb0[8].w, l(0.000000)
  12: movc r2.x, r0.w, r1.x, cb1[9].z
  13: movc r2.y, r0.w, r1.y, cb1[10].z
  14: movc r2.z, r0.w, r1.z, cb1[11].z
  15: dp3_sat r0.x, r0.xyzx, r2.xyzx
  16: add r0.x, -r0.x, l(1.000000)
  17: log r0.x, r0.x
  18: mul r0.x, r0.x, cb2[0].y
  19: exp r0.x, r0.x
  20: mul_sat r0.x, r0.x, cb2[0].z
  21: and r0.x, r0.x, v6.x
  22: lt r0.y, cb0[5].x, l(0.000000)
  23: add r0.z, -v0.y, cb0[6].y
  24: movc r1.y, r0.y, r0.z, v0.y
  25: mov r1.x, v0.x
  26: div r1.xy, r1.xyxx, cb0[6].xyxx
  27: add r1.z, -r1.y, l(1.000000)
  28: sample r1.xyzw, r1.xzxx, t0.xyzw, s0
  29: mad r0.y, cb0[7].x, r1.x, cb0[7].y
  30: div r0.y, l(1.000000, 1.000000, 1.000000, 1.000000), r0.y
  31: mul r0.z, v4.y, cb1[18].w
  32: mad r0.z, cb1[17].w, v4.x, r0.z
  33: mad r0.z, cb1[19].w, v4.z, r0.z
  34: add r0.z, r0.z, cb1[20].w
  35: mad r0.y, r0.y, cb0[5].z, -r0.z
  36: div_sat r0.y, r0.y, cb2[14].w
  37: ge r0.z, l(0.000000), r0.y
  38: and r0.z, r0.z, l(0x3f800000)
  39: add r0.w, -r0.y, l(1.000000)
  40: mad r0.y, r0.z, r0.w, r0.y
  41: add r0.x, -r0.y, r0.x
  42: add r0.x, r0.x, l(1.000000)
  43: mul r0.z, v1.w, l(1.000100)
  44: ge r0.w, r0.z, -r0.z
  45: frc r0.z, |r0.z|
  46: movc r0.z, r0.w, r0.z, -r0.z
  47: mul r0.z, r0.z, l(0.999900)
  48: div r1.xy, cb2[10].zwzz, cb2[5].xyxx
  49: mad r0.zw, cb0[0].yyyy, r1.xxxy, r0.zzzz
  50: add r0.zw, r0.zzzw, v1.xxxy
  51: mad r0.zw, r0.zzzw, cb2[5].xxxy, cb2[5].zzzw
  52: sample r1.xyzw, r0.zwzz, t2.xyzw, s2
  53: div r0.zw, cb2[11].xxxy, cb2[7].xxxy
  54: mad r0.zw, r0.zzzw, cb0[0].yyyy, v1.xxxy
  55: mad r0.zw, r0.zzzw, cb2[7].xxxy, cb2[7].zzzw
  56: sample r2.xyzw, r0.zwzz, t3.xyzw, s3
  57: mad r0.zw, v1.xxxy, cb2[9].xxxy, cb2[9].zzzw
  58: sample r3.xyzw, r0.zwzz, t4.xyzw, s4
  59: mul r0.zw, r2.xxxy, r3.xxxy
  60: mul r0.zw, r0.zzzw, cb2[11].zzzz
  61: div r0.zw, r0.zzzw, cb2[3].xxxy
  62: div r2.xy, cb2[10].xyxx, cb2[3].xyxx
  63: mad r2.xy, r2.xyxx, cb0[0].yyyy, v1.xyxx
  64: add r0.zw, -r0.zzzw, r2.xxxy
  65: mad r0.zw, r0.zzzw, cb2[3].xxxy, cb2[3].zzzw
  66: sample r2.xyzw, r0.zwzz, t1.xyzw, s1
  67: mul r1.xyzw, r1.wxyz, r2.wxyz
  68: ne r0.zw, l(0.000000, 0.000000, 0.000000, 0.000000), cb2[1].xxxy
  69: movc r2.xyz, r0.zzzz, r1.xxxx, r1.yzwy
  70: max r2.xyz, r0.xxxx, r2.xyzx
  71: mad_sat r0.x, r1.x, cb2[0].x, r0.x
  72: movc r1.yzw, r0.wwww, r2.xxyz, r1.yyzw
  73: add r0.z, -v1.z, l(1.000000)
  74: add_sat r2.xyz, -r0.zzzz, r3.xyzx
  75: mul_sat r2.xyz, r2.xyzx, r3.xyzx
  76: mul r2.xyz, r1.yzwy, r2.xyzx
  77: ne r3.xy, l(0.000000, 0.000000, 0.000000, 0.000000), cb2[14].yzyy
  78: movc r1.yzw, r3.xxxx, r2.xxyz, r1.yyzw
  79: mul r1.yzw, r1.yyzw, cb2[12].xxxx
  80: mul r1.yzw, r1.yyzw, v3.xxyz
  81: max r0.z, v2.x, l(1.000000)
  82: min r0.z, r0.z, l(1000000.000000)
  83: eq r2.x, v1.x, v2.x
  84: movc r0.z, r2.x, l(1.000000), r0.z
  85: mul o0.xyz, r1.yzwy, r0.zzzz
  86: mul r0.z, r0.x, r1.x
  87: ne r1.y, l(0.000000, 0.000000, 0.000000, 0.000000), cb2[0].w
  88: movc r0.x, r1.y, r0.z, r0.x
  89: movc r0.x, r0.w, r0.x, r1.x
  90: mul r0.y, r0.y, r0.x
  91: movc r0.x, r3.y, r0.y, r0.x
  92: mul r0.x, r0.x, v3.w
  93: mul_sat o0.w, r0.x, cb2[14].x
  94: ret 
// Approximately 0 instruction slots used


 }


 // Stats for Vertex shader:
 //        d3d11: 15 math
 Pass {
  Name "DepthOnly"
  Tags { "LIGHTMODE"="DepthOnly" "QUEUE"="Transparent" "RenderType"="Transparent" "ShaderGraphShader"="true" "ShaderGraphTargetId"="BuiltInUnlitSubTarget" "BuiltInMaterialType"="Unlit" }
  Cull [_BUILTIN_CullMode]
  Blend [_BUILTIN_SrcBlend] [_BUILTIN_DstBlend]
  ColorMask 0
  //////////////////////////////////
  //                              //
  //      Compiled programs       //
  //                              //
  //////////////////////////////////
//////////////////////////////////////////////////////
Keywords: <none>
-- Hardware tier variant: Tier 1
-- Vertex shader for "d3d11":
// Stats: 15 math, 2 temp registers
Uses vertex data channel "Vertex"
Uses vertex data channel "Normal"
Uses vertex data channel "TexCoord0"
Uses vertex data channel "Color"

Constant Buffer "UnityPerDraw" (176 bytes) on slot 0 {
  Matrix4x4 unity_ObjectToWorld at 0
  Matrix4x4 unity_WorldToObject at 64
}
Constant Buffer "UnityPerFrame" (368 bytes) on slot 1 {
  Matrix4x4 unity_MatrixVP at 272
}

Shader Disassembly:
//
// Generated by Microsoft (R) D3D Shader Disassembler
//
//
// Input signature:
//
// Name                 Index   Mask Register SysValue  Format   Used
// -------------------- ----- ------ -------- -------- ------- ------
// POSITION                 0   xyz         0     NONE   float   xyz 
// NORMAL                   0   xyz         1     NONE   float   xyz 
// TANGENT                  0   xyzw        2     NONE   float       
// TEXCOORD                 0   xyzw        3     NONE   float   xyzw
// COLOR                    0   xyzw        4     NONE   float   xyzw
//
//
// Output signature:
//
// Name                 Index   Mask Register SysValue  Format   Used
// -------------------- ----- ------ -------- -------- ------- ------
// SV_POSITION              0   xyzw        0      POS   float   xyzw
// INTERP                   0   xyzw        1     NONE   float   xyzw
// INTERP                   1   xyzw        2     NONE   float   xyzw
// INTERP                   2   xyz         3     NONE   float   xyz 
// INTERP                   3   xyz         4     NONE   float   xyz 
//
      vs_4_0
      dcl_constantbuffer CB0[7], immediateIndexed
      dcl_constantbuffer CB1[21], immediateIndexed
      dcl_input v0.xyz
      dcl_input v1.xyz
      dcl_input v3.xyzw
      dcl_input v4.xyzw
      dcl_output_siv o0.xyzw, position
      dcl_output o1.xyzw
      dcl_output o2.xyzw
      dcl_output o3.xyz
      dcl_output o4.xyz
      dcl_temps 2
   0: mul r0.xyz, v0.yyyy, cb0[1].xyzx
   1: mad r0.xyz, cb0[0].xyzx, v0.xxxx, r0.xyzx
   2: mad r0.xyz, cb0[2].xyzx, v0.zzzz, r0.xyzx
   3: add r0.xyz, r0.xyzx, cb0[3].xyzx
   4: mul r1.xyzw, r0.yyyy, cb1[18].xyzw
   5: mad r1.xyzw, cb1[17].xyzw, r0.xxxx, r1.xyzw
   6: mad r1.xyzw, cb1[19].xyzw, r0.zzzz, r1.xyzw
   7: mov o3.xyz, r0.xyzx
   8: add o0.xyzw, r1.xyzw, cb1[20].xyzw
   9: mov o1.xyzw, v3.xyzw
  10: mov o2.xyzw, v4.xyzw
  11: dp3 r0.x, v1.xyzx, cb0[4].xyzx
  12: dp3 r0.y, v1.xyzx, cb0[5].xyzx
  13: dp3 r0.z, v1.xyzx, cb0[6].xyzx
  14: dp3 r0.w, r0.xyzx, r0.xyzx
  15: max r0.w, r0.w, l(0.000000)
  16: rsq r0.w, r0.w
  17: mul o4.xyz, r0.wwww, r0.xyzx
  18: ret 
// Approximately 0 instruction slots used


-- Hardware tier variant: Tier 1
-- Fragment shader for "d3d11":
Shader Disassembly:
//
// Generated by Microsoft (R) D3D Shader Disassembler
//
//
// Input signature:
//
// Name                 Index   Mask Register SysValue  Format   Used
// -------------------- ----- ------ -------- -------- ------- ------
// SV_POSITION              0   xyzw        0      POS   float       
// INTERP                   0   xyzw        1     NONE   float       
// INTERP                   1   xyzw        2     NONE   float       
// INTERP                   2   xyz         3     NONE   float       
// INTERP                   3   xyz         4     NONE   float       
// SV_IsFrontFace           0   x           5    FFACE    uint       
//
//
// Output signature:
//
// Name                 Index   Mask Register SysValue  Format   Used
// -------------------- ----- ------ -------- -------- ------- ------
// SV_TARGET                0   xyzw        0   TARGET   float   xyzw
//
      ps_4_0
      dcl_output o0.xyzw
   0: mov o0.xyzw, l(0,0,0,0)
   1: ret 
// Approximately 0 instruction slots used


 }


 // Stats for Vertex shader:
 //        d3d11: 33 avg math (31..35)
 Pass {
  Name "ShadowCaster"
  Tags { "LIGHTMODE"="SHADOWCASTER" "QUEUE"="Transparent" "SHADOWSUPPORT"="true" "RenderType"="Transparent" "ShaderGraphShader"="true" "ShaderGraphTargetId"="BuiltInUnlitSubTarget" "BuiltInMaterialType"="Unlit" }
  Cull [_BUILTIN_CullMode]
  Blend [_BUILTIN_SrcBlend] [_BUILTIN_DstBlend]
  ColorMask 0
  //////////////////////////////////
  //                              //
  //      Compiled programs       //
  //                              //
  //////////////////////////////////
//////////////////////////////////////////////////////
Keywords: SHADOWS_DEPTH
-- Hardware tier variant: Tier 1
-- Vertex shader for "d3d11":
// Stats: 35 math, 3 temp registers
Uses vertex data channel "Vertex"
Uses vertex data channel "Normal"
Uses vertex data channel "TexCoord0"
Uses vertex data channel "Color"

Constant Buffer "UnityLighting" (768 bytes) on slot 0 {
  Vector4 _WorldSpaceLightPos0 at 0
}
Constant Buffer "UnityShadows" (416 bytes) on slot 1 {
  Vector4 unity_LightShadowBias at 80
}
Constant Buffer "UnityPerDraw" (176 bytes) on slot 2 {
  Matrix4x4 unity_ObjectToWorld at 0
  Matrix4x4 unity_WorldToObject at 64
}
Constant Buffer "UnityPerFrame" (368 bytes) on slot 3 {
  Matrix4x4 unity_MatrixVP at 272
}

Shader Disassembly:
//
// Generated by Microsoft (R) D3D Shader Disassembler
//
//
// Input signature:
//
// Name                 Index   Mask Register SysValue  Format   Used
// -------------------- ----- ------ -------- -------- ------- ------
// POSITION                 0   xyz         0     NONE   float   xyz 
// NORMAL                   0   xyz         1     NONE   float   xyz 
// TANGENT                  0   xyzw        2     NONE   float       
// TEXCOORD                 0   xyzw        3     NONE   float   xyzw
// COLOR                    0   xyzw        4     NONE   float   xyzw
//
//
// Output signature:
//
// Name                 Index   Mask Register SysValue  Format   Used
// -------------------- ----- ------ -------- -------- ------- ------
// SV_POSITION              0   xyzw        0      POS   float   xyzw
// INTERP                   0   xyzw        1     NONE   float   xyzw
// INTERP                   1   xyzw        2     NONE   float   xyzw
// INTERP                   2   xyz         3     NONE   float   xyz 
// INTERP                   3   xyz         4     NONE   float   xyz 
//
      vs_4_0
      dcl_constantbuffer CB0[1], immediateIndexed
      dcl_constantbuffer CB1[6], immediateIndexed
      dcl_constantbuffer CB2[7], immediateIndexed
      dcl_constantbuffer CB3[21], immediateIndexed
      dcl_input v0.xyz
      dcl_input v1.xyz
      dcl_input v3.xyzw
      dcl_input v4.xyzw
      dcl_output_siv o0.xyzw, position
      dcl_output o1.xyzw
      dcl_output o2.xyzw
      dcl_output o3.xyz
      dcl_output o4.xyz
      dcl_temps 3
   0: dp3 r0.x, v1.xyzx, cb2[4].xyzx
   1: dp3 r0.y, v1.xyzx, cb2[5].xyzx
   2: dp3 r0.z, v1.xyzx, cb2[6].xyzx
   3: dp3 r0.w, r0.xyzx, r0.xyzx
   4: rsq r0.w, r0.w
   5: mul r0.xyz, r0.wwww, r0.xyzx
   6: mul r1.xyzw, v0.yyyy, cb2[1].xyzw
   7: mad r1.xyzw, cb2[0].xyzw, v0.xxxx, r1.xyzw
   8: mad r1.xyzw, cb2[2].xyzw, v0.zzzz, r1.xyzw
   9: add r1.xyzw, r1.xyzw, cb2[3].xyzw
  10: mad r2.xyz, -r1.xyzx, cb0[0].wwww, cb0[0].xyzx
  11: dp3 r0.w, r2.xyzx, r2.xyzx
  12: rsq r0.w, r0.w
  13: mul r2.xyz, r0.wwww, r2.xyzx
  14: dp3 r0.w, r0.xyzx, r2.xyzx
  15: mad r0.w, -r0.w, r0.w, l(1.000000)
  16: sqrt r0.w, r0.w
  17: mul r0.w, r0.w, cb1[5].z
  18: mad r0.xyz, -r0.xyzx, r0.wwww, r1.xyzx
  19: ne r0.w, cb1[5].z, l(0.000000)
  20: movc r0.xyz, r0.wwww, r0.xyzx, r1.xyzx
  21: mul r2.xyzw, r0.yyyy, cb3[18].xyzw
  22: mad r2.xyzw, cb3[17].xyzw, r0.xxxx, r2.xyzw
  23: mad r0.xyzw, cb3[19].xyzw, r0.zzzz, r2.xyzw
  24: mad r0.xyzw, cb3[20].xyzw, r1.wwww, r0.xyzw
  25: div r1.x, cb1[5].x, r0.w
  26: min r1.x, r1.x, l(0.000000)
  27: max r1.x, r1.x, l(-1.000000)
  28: add r0.z, r0.z, r1.x
  29: min r1.x, r0.w, r0.z
  30: mov o0.xyw, r0.xyxw
  31: add r0.x, -r0.z, r1.x
  32: mad o0.z, cb1[5].y, r0.x, r0.z
  33: mov o1.xyzw, v3.xyzw
  34: mov o2.xyzw, v4.xyzw
  35: mul r0.xyz, v0.yyyy, cb2[1].xyzx
  36: mad r0.xyz, cb2[0].xyzx, v0.xxxx, r0.xyzx
  37: mad r0.xyz, cb2[2].xyzx, v0.zzzz, r0.xyzx
  38: add o3.xyz, r0.xyzx, cb2[3].xyzx
  39: mov o4.xyz, l(0,0,0,0)
  40: ret 
// Approximately 0 instruction slots used


-- Hardware tier variant: Tier 1
-- Fragment shader for "d3d11":
Shader Disassembly:
//
// Generated by Microsoft (R) D3D Shader Disassembler
//
//
// Input signature:
//
// Name                 Index   Mask Register SysValue  Format   Used
// -------------------- ----- ------ -------- -------- ------- ------
// SV_POSITION              0   xyzw        0      POS   float       
// INTERP                   0   xyzw        1     NONE   float       
// INTERP                   1   xyzw        2     NONE   float       
// INTERP                   2   xyz         3     NONE   float       
// INTERP                   3   xyz         4     NONE   float       
// SV_IsFrontFace           0   x           5    FFACE    uint       
//
//
// Output signature:
//
// Name                 Index   Mask Register SysValue  Format   Used
// -------------------- ----- ------ -------- -------- ------- ------
// SV_TARGET                0   xyzw        0   TARGET   float   xyzw
//
      ps_4_0
      dcl_output o0.xyzw
   0: mov o0.xyzw, l(0,0,0,0)
   1: ret 
// Approximately 0 instruction slots used


//////////////////////////////////////////////////////
Keywords: SHADOWS_CUBE
-- Hardware tier variant: Tier 1
-- Vertex shader for "d3d11":
// Stats: 31 math, 3 temp registers
Uses vertex data channel "Vertex"
Uses vertex data channel "Normal"
Uses vertex data channel "TexCoord0"
Uses vertex data channel "Color"

Constant Buffer "UnityLighting" (768 bytes) on slot 0 {
  Vector4 _WorldSpaceLightPos0 at 0
}
Constant Buffer "UnityShadows" (416 bytes) on slot 1 {
  Vector4 unity_LightShadowBias at 80
}
Constant Buffer "UnityPerDraw" (176 bytes) on slot 2 {
  Matrix4x4 unity_ObjectToWorld at 0
  Matrix4x4 unity_WorldToObject at 64
}
Constant Buffer "UnityPerFrame" (368 bytes) on slot 3 {
  Matrix4x4 unity_MatrixVP at 272
}

Shader Disassembly:
//
// Generated by Microsoft (R) D3D Shader Disassembler
//
//
// Input signature:
//
// Name                 Index   Mask Register SysValue  Format   Used
// -------------------- ----- ------ -------- -------- ------- ------
// POSITION                 0   xyz         0     NONE   float   xyz 
// NORMAL                   0   xyz         1     NONE   float   xyz 
// TANGENT                  0   xyzw        2     NONE   float       
// TEXCOORD                 0   xyzw        3     NONE   float   xyzw
// COLOR                    0   xyzw        4     NONE   float   xyzw
//
//
// Output signature:
//
// Name                 Index   Mask Register SysValue  Format   Used
// -------------------- ----- ------ -------- -------- ------- ------
// SV_POSITION              0   xyzw        0      POS   float   xyzw
// INTERP                   0   xyzw        1     NONE   float   xyzw
// INTERP                   1   xyzw        2     NONE   float   xyzw
// INTERP                   2   xyz         3     NONE   float   xyz 
// INTERP                   3   xyz         4     NONE   float   xyz 
//
      vs_4_0
      dcl_constantbuffer CB0[1], immediateIndexed
      dcl_constantbuffer CB1[6], immediateIndexed
      dcl_constantbuffer CB2[7], immediateIndexed
      dcl_constantbuffer CB3[21], immediateIndexed
      dcl_input v0.xyz
      dcl_input v1.xyz
      dcl_input v3.xyzw
      dcl_input v4.xyzw
      dcl_output_siv o0.xyzw, position
      dcl_output o1.xyzw
      dcl_output o2.xyzw
      dcl_output o3.xyz
      dcl_output o4.xyz
      dcl_temps 3
   0: dp3 r0.x, v1.xyzx, cb2[4].xyzx
   1: dp3 r0.y, v1.xyzx, cb2[5].xyzx
   2: dp3 r0.z, v1.xyzx, cb2[6].xyzx
   3: dp3 r0.w, r0.xyzx, r0.xyzx
   4: rsq r0.w, r0.w
   5: mul r0.xyz, r0.wwww, r0.xyzx
   6: mul r1.xyzw, v0.yyyy, cb2[1].xyzw
   7: mad r1.xyzw, cb2[0].xyzw, v0.xxxx, r1.xyzw
   8: mad r1.xyzw, cb2[2].xyzw, v0.zzzz, r1.xyzw
   9: add r1.xyzw, r1.xyzw, cb2[3].xyzw
  10: mad r2.xyz, -r1.xyzx, cb0[0].wwww, cb0[0].xyzx
  11: dp3 r0.w, r2.xyzx, r2.xyzx
  12: rsq r0.w, r0.w
  13: mul r2.xyz, r0.wwww, r2.xyzx
  14: dp3 r0.w, r0.xyzx, r2.xyzx
  15: mad r0.w, -r0.w, r0.w, l(1.000000)
  16: sqrt r0.w, r0.w
  17: mul r0.w, r0.w, cb1[5].z
  18: mad r0.xyz, -r0.xyzx, r0.wwww, r1.xyzx
  19: ne r0.w, cb1[5].z, l(0.000000)
  20: movc r0.xyz, r0.wwww, r0.xyzx, r1.xyzx
  21: mul r2.xyzw, r0.yyyy, cb3[18].xyzw
  22: mad r2.xyzw, cb3[17].xyzw, r0.xxxx, r2.xyzw
  23: mad r0.xyzw, cb3[19].xyzw, r0.zzzz, r2.xyzw
  24: mad r0.xyzw, cb3[20].xyzw, r1.wwww, r0.xyzw
  25: min r1.x, r0.w, r0.z
  26: add r1.x, -r0.z, r1.x
  27: mad o0.z, cb1[5].y, r1.x, r0.z
  28: mov o0.xyw, r0.xyxw
  29: mov o1.xyzw, v3.xyzw
  30: mov o2.xyzw, v4.xyzw
  31: mul r0.xyz, v0.yyyy, cb2[1].xyzx
  32: mad r0.xyz, cb2[0].xyzx, v0.xxxx, r0.xyzx
  33: mad r0.xyz, cb2[2].xyzx, v0.zzzz, r0.xyzx
  34: add o3.xyz, r0.xyzx, cb2[3].xyzx
  35: mov o4.xyz, l(0,0,0,0)
  36: ret 
// Approximately 0 instruction slots used


-- Fragment shader for "d3d11":
// No shader variant for this keyword set. The closest match will be used instead.

//////////////////////////////////////////////////////
Keywords: SHADOWS_DEPTH _CASTING_PUNCTUAL_LIGHT_SHADOW
-- Hardware tier variant: Tier 1
-- Vertex shader for "d3d11":
// Stats: 35 math, 3 temp registers
Uses vertex data channel "Vertex"
Uses vertex data channel "Normal"
Uses vertex data channel "TexCoord0"
Uses vertex data channel "Color"

Constant Buffer "UnityLighting" (768 bytes) on slot 0 {
  Vector4 _WorldSpaceLightPos0 at 0
}
Constant Buffer "UnityShadows" (416 bytes) on slot 1 {
  Vector4 unity_LightShadowBias at 80
}
Constant Buffer "UnityPerDraw" (176 bytes) on slot 2 {
  Matrix4x4 unity_ObjectToWorld at 0
  Matrix4x4 unity_WorldToObject at 64
}
Constant Buffer "UnityPerFrame" (368 bytes) on slot 3 {
  Matrix4x4 unity_MatrixVP at 272
}

Shader Disassembly:
//
// Generated by Microsoft (R) D3D Shader Disassembler
//
//
// Input signature:
//
// Name                 Index   Mask Register SysValue  Format   Used
// -------------------- ----- ------ -------- -------- ------- ------
// POSITION                 0   xyz         0     NONE   float   xyz 
// NORMAL                   0   xyz         1     NONE   float   xyz 
// TANGENT                  0   xyzw        2     NONE   float       
// TEXCOORD                 0   xyzw        3     NONE   float   xyzw
// COLOR                    0   xyzw        4     NONE   float   xyzw
//
//
// Output signature:
//
// Name                 Index   Mask Register SysValue  Format   Used
// -------------------- ----- ------ -------- -------- ------- ------
// SV_POSITION              0   xyzw        0      POS   float   xyzw
// INTERP                   0   xyzw        1     NONE   float   xyzw
// INTERP                   1   xyzw        2     NONE   float   xyzw
// INTERP                   2   xyz         3     NONE   float   xyz 
// INTERP                   3   xyz         4     NONE   float   xyz 
//
      vs_4_0
      dcl_constantbuffer CB0[1], immediateIndexed
      dcl_constantbuffer CB1[6], immediateIndexed
      dcl_constantbuffer CB2[7], immediateIndexed
      dcl_constantbuffer CB3[21], immediateIndexed
      dcl_input v0.xyz
      dcl_input v1.xyz
      dcl_input v3.xyzw
      dcl_input v4.xyzw
      dcl_output_siv o0.xyzw, position
      dcl_output o1.xyzw
      dcl_output o2.xyzw
      dcl_output o3.xyz
      dcl_output o4.xyz
      dcl_temps 3
   0: dp3 r0.x, v1.xyzx, cb2[4].xyzx
   1: dp3 r0.y, v1.xyzx, cb2[5].xyzx
   2: dp3 r0.z, v1.xyzx, cb2[6].xyzx
   3: dp3 r0.w, r0.xyzx, r0.xyzx
   4: rsq r0.w, r0.w
   5: mul r0.xyz, r0.wwww, r0.xyzx
   6: mul r1.xyzw, v0.yyyy, cb2[1].xyzw
   7: mad r1.xyzw, cb2[0].xyzw, v0.xxxx, r1.xyzw
   8: mad r1.xyzw, cb2[2].xyzw, v0.zzzz, r1.xyzw
   9: add r1.xyzw, r1.xyzw, cb2[3].xyzw
  10: mad r2.xyz, -r1.xyzx, cb0[0].wwww, cb0[0].xyzx
  11: dp3 r0.w, r2.xyzx, r2.xyzx
  12: rsq r0.w, r0.w
  13: mul r2.xyz, r0.wwww, r2.xyzx
  14: dp3 r0.w, r0.xyzx, r2.xyzx
  15: mad r0.w, -r0.w, r0.w, l(1.000000)
  16: sqrt r0.w, r0.w
  17: mul r0.w, r0.w, cb1[5].z
  18: mad r0.xyz, -r0.xyzx, r0.wwww, r1.xyzx
  19: ne r0.w, cb1[5].z, l(0.000000)
  20: movc r0.xyz, r0.wwww, r0.xyzx, r1.xyzx
  21: mul r2.xyzw, r0.yyyy, cb3[18].xyzw
  22: mad r2.xyzw, cb3[17].xyzw, r0.xxxx, r2.xyzw
  23: mad r0.xyzw, cb3[19].xyzw, r0.zzzz, r2.xyzw
  24: mad r0.xyzw, cb3[20].xyzw, r1.wwww, r0.xyzw
  25: div r1.x, cb1[5].x, r0.w
  26: min r1.x, r1.x, l(0.000000)
  27: max r1.x, r1.x, l(-1.000000)
  28: add r0.z, r0.z, r1.x
  29: min r1.x, r0.w, r0.z
  30: mov o0.xyw, r0.xyxw
  31: add r0.x, -r0.z, r1.x
  32: mad o0.z, cb1[5].y, r0.x, r0.z
  33: mov o1.xyzw, v3.xyzw
  34: mov o2.xyzw, v4.xyzw
  35: mul r0.xyz, v0.yyyy, cb2[1].xyzx
  36: mad r0.xyz, cb2[0].xyzx, v0.xxxx, r0.xyzx
  37: mad r0.xyz, cb2[2].xyzx, v0.zzzz, r0.xyzx
  38: add o3.xyz, r0.xyzx, cb2[3].xyzx
  39: mov o4.xyz, l(0,0,0,0)
  40: ret 
// Approximately 0 instruction slots used


-- Fragment shader for "d3d11":
// No shader variant for this keyword set. The closest match will be used instead.

//////////////////////////////////////////////////////
Keywords: SHADOWS_CUBE _CASTING_PUNCTUAL_LIGHT_SHADOW
-- Hardware tier variant: Tier 1
-- Vertex shader for "d3d11":
// Stats: 31 math, 3 temp registers
Uses vertex data channel "Vertex"
Uses vertex data channel "Normal"
Uses vertex data channel "TexCoord0"
Uses vertex data channel "Color"

Constant Buffer "UnityLighting" (768 bytes) on slot 0 {
  Vector4 _WorldSpaceLightPos0 at 0
}
Constant Buffer "UnityShadows" (416 bytes) on slot 1 {
  Vector4 unity_LightShadowBias at 80
}
Constant Buffer "UnityPerDraw" (176 bytes) on slot 2 {
  Matrix4x4 unity_ObjectToWorld at 0
  Matrix4x4 unity_WorldToObject at 64
}
Constant Buffer "UnityPerFrame" (368 bytes) on slot 3 {
  Matrix4x4 unity_MatrixVP at 272
}

Shader Disassembly:
//
// Generated by Microsoft (R) D3D Shader Disassembler
//
//
// Input signature:
//
// Name                 Index   Mask Register SysValue  Format   Used
// -------------------- ----- ------ -------- -------- ------- ------
// POSITION                 0   xyz         0     NONE   float   xyz 
// NORMAL                   0   xyz         1     NONE   float   xyz 
// TANGENT                  0   xyzw        2     NONE   float       
// TEXCOORD                 0   xyzw        3     NONE   float   xyzw
// COLOR                    0   xyzw        4     NONE   float   xyzw
//
//
// Output signature:
//
// Name                 Index   Mask Register SysValue  Format   Used
// -------------------- ----- ------ -------- -------- ------- ------
// SV_POSITION              0   xyzw        0      POS   float   xyzw
// INTERP                   0   xyzw        1     NONE   float   xyzw
// INTERP                   1   xyzw        2     NONE   float   xyzw
// INTERP                   2   xyz         3     NONE   float   xyz 
// INTERP                   3   xyz         4     NONE   float   xyz 
//
      vs_4_0
      dcl_constantbuffer CB0[1], immediateIndexed
      dcl_constantbuffer CB1[6], immediateIndexed
      dcl_constantbuffer CB2[7], immediateIndexed
      dcl_constantbuffer CB3[21], immediateIndexed
      dcl_input v0.xyz
      dcl_input v1.xyz
      dcl_input v3.xyzw
      dcl_input v4.xyzw
      dcl_output_siv o0.xyzw, position
      dcl_output o1.xyzw
      dcl_output o2.xyzw
      dcl_output o3.xyz
      dcl_output o4.xyz
      dcl_temps 3
   0: dp3 r0.x, v1.xyzx, cb2[4].xyzx
   1: dp3 r0.y, v1.xyzx, cb2[5].xyzx
   2: dp3 r0.z, v1.xyzx, cb2[6].xyzx
   3: dp3 r0.w, r0.xyzx, r0.xyzx
   4: rsq r0.w, r0.w
   5: mul r0.xyz, r0.wwww, r0.xyzx
   6: mul r1.xyzw, v0.yyyy, cb2[1].xyzw
   7: mad r1.xyzw, cb2[0].xyzw, v0.xxxx, r1.xyzw
   8: mad r1.xyzw, cb2[2].xyzw, v0.zzzz, r1.xyzw
   9: add r1.xyzw, r1.xyzw, cb2[3].xyzw
  10: mad r2.xyz, -r1.xyzx, cb0[0].wwww, cb0[0].xyzx
  11: dp3 r0.w, r2.xyzx, r2.xyzx
  12: rsq r0.w, r0.w
  13: mul r2.xyz, r0.wwww, r2.xyzx
  14: dp3 r0.w, r0.xyzx, r2.xyzx
  15: mad r0.w, -r0.w, r0.w, l(1.000000)
  16: sqrt r0.w, r0.w
  17: mul r0.w, r0.w, cb1[5].z
  18: mad r0.xyz, -r0.xyzx, r0.wwww, r1.xyzx
  19: ne r0.w, cb1[5].z, l(0.000000)
  20: movc r0.xyz, r0.wwww, r0.xyzx, r1.xyzx
  21: mul r2.xyzw, r0.yyyy, cb3[18].xyzw
  22: mad r2.xyzw, cb3[17].xyzw, r0.xxxx, r2.xyzw
  23: mad r0.xyzw, cb3[19].xyzw, r0.zzzz, r2.xyzw
  24: mad r0.xyzw, cb3[20].xyzw, r1.wwww, r0.xyzw
  25: min r1.x, r0.w, r0.z
  26: add r1.x, -r0.z, r1.x
  27: mad o0.z, cb1[5].y, r1.x, r0.z
  28: mov o0.xyw, r0.xyxw
  29: mov o1.xyzw, v3.xyzw
  30: mov o2.xyzw, v4.xyzw
  31: mul r0.xyz, v0.yyyy, cb2[1].xyzx
  32: mad r0.xyz, cb2[0].xyzx, v0.xxxx, r0.xyzx
  33: mad r0.xyz, cb2[2].xyzx, v0.zzzz, r0.xyzx
  34: add o3.xyz, r0.xyzx, cb2[3].xyzx
  35: mov o4.xyz, l(0,0,0,0)
  36: ret 
// Approximately 0 instruction slots used


-- Fragment shader for "d3d11":
// No shader variant for this keyword set. The closest match will be used instead.

//////////////////////////////////////////////////////
Keywords: SHADOWS_CUBE _BUILTIN_SURFACE_TYPE_TRANSPARENT
-- Vertex shader for "d3d11":
// No shader variant for this keyword set. The closest match will be used instead.

-- Hardware tier variant: Tier 1
-- Fragment shader for "d3d11":
Shader Disassembly:
//
// Generated by Microsoft (R) D3D Shader Disassembler
//
//
// Input signature:
//
// Name                 Index   Mask Register SysValue  Format   Used
// -------------------- ----- ------ -------- -------- ------- ------
// SV_POSITION              0   xyzw        0      POS   float       
// INTERP                   0   xyzw        1     NONE   float       
// INTERP                   1   xyzw        2     NONE   float       
// INTERP                   2   xyz         3     NONE   float       
// INTERP                   3   xyz         4     NONE   float       
// SV_IsFrontFace           0   x           5    FFACE    uint       
//
//
// Output signature:
//
// Name                 Index   Mask Register SysValue  Format   Used
// -------------------- ----- ------ -------- -------- ------- ------
// SV_TARGET                0   xyzw        0   TARGET   float   xyzw
//
      ps_4_0
      dcl_output o0.xyzw
   0: mov o0.xyzw, l(0,0,0,0)
   1: ret 
// Approximately 0 instruction slots used


//////////////////////////////////////////////////////
Keywords: SHADOWS_CUBE
-- Vertex shader for "d3d11":
// No shader variant for this keyword set. The closest match will be used instead.

-- Hardware tier variant: Tier 1
-- Fragment shader for "d3d11":
Shader Disassembly:
//
// Generated by Microsoft (R) D3D Shader Disassembler
//
//
// Input signature:
//
// Name                 Index   Mask Register SysValue  Format   Used
// -------------------- ----- ------ -------- -------- ------- ------
// SV_POSITION              0   xyzw        0      POS   float       
// INTERP                   0   xyzw        1     NONE   float       
// INTERP                   1   xyzw        2     NONE   float       
// INTERP                   2   xyz         3     NONE   float       
// INTERP                   3   xyz         4     NONE   float       
// SV_IsFrontFace           0   x           5    FFACE    uint       
//
//
// Output signature:
//
// Name                 Index   Mask Register SysValue  Format   Used
// -------------------- ----- ------ -------- -------- ------- ------
// SV_TARGET                0   xyzw        0   TARGET   float   xyzw
//
      ps_4_0
      dcl_output o0.xyzw
   0: mov o0.xyzw, l(0,0,0,0)
   1: ret 
// Approximately 0 instruction slots used


//////////////////////////////////////////////////////
Keywords: SHADOWS_DEPTH _BUILTIN_SURFACE_TYPE_TRANSPARENT
-- Vertex shader for "d3d11":
// No shader variant for this keyword set. The closest match will be used instead.

-- Hardware tier variant: Tier 1
-- Fragment shader for "d3d11":
Shader Disassembly:
//
// Generated by Microsoft (R) D3D Shader Disassembler
//
//
// Input signature:
//
// Name                 Index   Mask Register SysValue  Format   Used
// -------------------- ----- ------ -------- -------- ------- ------
// SV_POSITION              0   xyzw        0      POS   float       
// INTERP                   0   xyzw        1     NONE   float       
// INTERP                   1   xyzw        2     NONE   float       
// INTERP                   2   xyz         3     NONE   float       
// INTERP                   3   xyz         4     NONE   float       
// SV_IsFrontFace           0   x           5    FFACE    uint       
//
//
// Output signature:
//
// Name                 Index   Mask Register SysValue  Format   Used
// -------------------- ----- ------ -------- -------- ------- ------
// SV_TARGET                0   xyzw        0   TARGET   float   xyzw
//
      ps_4_0
      dcl_output o0.xyzw
   0: mov o0.xyzw, l(0,0,0,0)
   1: ret 
// Approximately 0 instruction slots used


//////////////////////////////////////////////////////
Keywords: SHADOWS_DEPTH _CASTING_PUNCTUAL_LIGHT_SHADOW
-- Vertex shader for "d3d11":
// No shader variant for this keyword set. The closest match will be used instead.

-- Hardware tier variant: Tier 1
-- Fragment shader for "d3d11":
Shader Disassembly:
//
// Generated by Microsoft (R) D3D Shader Disassembler
//
//
// Input signature:
//
// Name                 Index   Mask Register SysValue  Format   Used
// -------------------- ----- ------ -------- -------- ------- ------
// SV_POSITION              0   xyzw        0      POS   float       
// INTERP                   0   xyzw        1     NONE   float       
// INTERP                   1   xyzw        2     NONE   float       
// INTERP                   2   xyz         3     NONE   float       
// INTERP                   3   xyz         4     NONE   float       
// SV_IsFrontFace           0   x           5    FFACE    uint       
//
//
// Output signature:
//
// Name                 Index   Mask Register SysValue  Format   Used
// -------------------- ----- ------ -------- -------- ------- ------
// SV_TARGET                0   xyzw        0   TARGET   float   xyzw
//
      ps_4_0
      dcl_output o0.xyzw
   0: mov o0.xyzw, l(0,0,0,0)
   1: ret 
// Approximately 0 instruction slots used


//////////////////////////////////////////////////////
Keywords: SHADOWS_CUBE _BUILTIN_SURFACE_TYPE_TRANSPARENT _CASTING_PUNCTUAL_LIGHT_SHADOW
-- Vertex shader for "d3d11":
// No shader variant for this keyword set. The closest match will be used instead.

-- Hardware tier variant: Tier 1
-- Fragment shader for "d3d11":
Shader Disassembly:
//
// Generated by Microsoft (R) D3D Shader Disassembler
//
//
// Input signature:
//
// Name                 Index   Mask Register SysValue  Format   Used
// -------------------- ----- ------ -------- -------- ------- ------
// SV_POSITION              0   xyzw        0      POS   float       
// INTERP                   0   xyzw        1     NONE   float       
// INTERP                   1   xyzw        2     NONE   float       
// INTERP                   2   xyz         3     NONE   float       
// INTERP                   3   xyz         4     NONE   float       
// SV_IsFrontFace           0   x           5    FFACE    uint       
//
//
// Output signature:
//
// Name                 Index   Mask Register SysValue  Format   Used
// -------------------- ----- ------ -------- -------- ------- ------
// SV_TARGET                0   xyzw        0   TARGET   float   xyzw
//
      ps_4_0
      dcl_output o0.xyzw
   0: mov o0.xyzw, l(0,0,0,0)
   1: ret 
// Approximately 0 instruction slots used


//////////////////////////////////////////////////////
Keywords: SHADOWS_CUBE _CASTING_PUNCTUAL_LIGHT_SHADOW
-- Vertex shader for "d3d11":
// No shader variant for this keyword set. The closest match will be used instead.

-- Hardware tier variant: Tier 1
-- Fragment shader for "d3d11":
Shader Disassembly:
//
// Generated by Microsoft (R) D3D Shader Disassembler
//
//
// Input signature:
//
// Name                 Index   Mask Register SysValue  Format   Used
// -------------------- ----- ------ -------- -------- ------- ------
// SV_POSITION              0   xyzw        0      POS   float       
// INTERP                   0   xyzw        1     NONE   float       
// INTERP                   1   xyzw        2     NONE   float       
// INTERP                   2   xyz         3     NONE   float       
// INTERP                   3   xyz         4     NONE   float       
// SV_IsFrontFace           0   x           5    FFACE    uint       
//
//
// Output signature:
//
// Name                 Index   Mask Register SysValue  Format   Used
// -------------------- ----- ------ -------- -------- ------- ------
// SV_TARGET                0   xyzw        0   TARGET   float   xyzw
//
      ps_4_0
      dcl_output o0.xyzw
   0: mov o0.xyzw, l(0,0,0,0)
   1: ret 
// Approximately 0 instruction slots used


//////////////////////////////////////////////////////
Keywords: SHADOWS_DEPTH _BUILTIN_SURFACE_TYPE_TRANSPARENT _CASTING_PUNCTUAL_LIGHT_SHADOW
-- Vertex shader for "d3d11":
// No shader variant for this keyword set. The closest match will be used instead.

-- Hardware tier variant: Tier 1
-- Fragment shader for "d3d11":
Shader Disassembly:
//
// Generated by Microsoft (R) D3D Shader Disassembler
//
//
// Input signature:
//
// Name                 Index   Mask Register SysValue  Format   Used
// -------------------- ----- ------ -------- -------- ------- ------
// SV_POSITION              0   xyzw        0      POS   float       
// INTERP                   0   xyzw        1     NONE   float       
// INTERP                   1   xyzw        2     NONE   float       
// INTERP                   2   xyz         3     NONE   float       
// INTERP                   3   xyz         4     NONE   float       
// SV_IsFrontFace           0   x           5    FFACE    uint       
//
//
// Output signature:
//
// Name                 Index   Mask Register SysValue  Format   Used
// -------------------- ----- ------ -------- -------- ------- ------
// SV_TARGET                0   xyzw        0   TARGET   float   xyzw
//
      ps_4_0
      dcl_output o0.xyzw
   0: mov o0.xyzw, l(0,0,0,0)
   1: ret 
// Approximately 0 instruction slots used


 }
}
CustomEditor "UnityEditor.ShaderGraph.GenericShaderGraphMaterialGUI"
Fallback "Hidden/Shader Graph/FallbackError"
}
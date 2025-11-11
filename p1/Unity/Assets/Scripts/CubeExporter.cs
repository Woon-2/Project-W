using UnityEngine;
using System.IO;

public class CubeExporter : ILevelExportable
{
    void ExtractMaterials(BinaryWriter binaryWriter, Material[] materials)
    {
        ExtractUtil.WriteHeadTag(binaryWriter, "Materials");

        ExtractUtil.WriteInteger(binaryWriter, "MaterialCnt", materials.Length);

        for (int i = 0; i < materials.Length; i++)
        {
            ExtractUtil.WriteHeadTag(binaryWriter, "Material");

            // 상수들 추출
            // cAlbedo
            if (materials[i].HasProperty("_Color"))
            {
                Color albedo = materials[i].GetColor("_Color");
                ExtractUtil.WriteColor(binaryWriter, "cAlbedo", albedo);
            }
            // cEmmisive
            if (materials[i].HasProperty("_EmissionColor"))
            {
                Color emission = Color.black; // Default value for emission color when emission is not enabled

                if (materials[i].IsKeywordEnabled("_EMISSION"))
                {
                    emission = materials[i].GetColor("_EmissionColor");
                }

                ExtractUtil.WriteColor(binaryWriter, "cEmmisive", emission);
            }
            // cSmoothness
            if (materials[i].HasProperty("_Smoothness"))
            {
                ExtractUtil.WriteFloat(binaryWriter, "cSmoothness", materials[i].GetFloat("_Smoothness"));
            }
            else if (materials[i].HasProperty("_Glossiness"))
            {
                ExtractUtil.WriteFloat(binaryWriter, "cSmoothness", materials[i].GetFloat("_Glossiness"));
            }
            // cMetallic
            if (materials[i].HasProperty("_Metallic"))
            {
                ExtractUtil.WriteFloat(binaryWriter, "cMetallic", materials[i].GetFloat("_Metallic"));
            }
            // cAOStrength
            if (materials[i].HasProperty("_OcclusionStrength"))
            {
                ExtractUtil.WriteFloat(binaryWriter, "cAOStrength", materials[i].GetFloat("_OcclusionStrength"));
            }

            // 텍스처들 추출
            // AlbedoMap
            if (materials[i].HasProperty("_MainTex"))
            {
                Texture mainAlbedoMap = materials[i].GetTexture("_MainTex");
                if (mainAlbedoMap != null)
                {
                    ExtractUtil.WriteText(binaryWriter, "AlbedoMap", mainAlbedoMap.name);
                }
            }
            // NormalMap
            if (materials[i].HasProperty("_BumpMap"))
            {
                Texture bumpMap = materials[i].GetTexture("_BumpMap");
                if (bumpMap != null)
                {
                    ExtractUtil.WriteText(binaryWriter, "NormalMap", bumpMap.name);
                }
            }
            // MetallicSmoothnessMap
            if (materials[i].HasProperty("_MetallicGlossMap"))
            {
                Texture metallicGlossMap = materials[i].GetTexture("_MetallicGlossMap");
                if (metallicGlossMap != null)
                {
                    ExtractUtil.WriteText(binaryWriter, "MetallicSmoothnessMap", metallicGlossMap.name);
                }
            }
            // EmmisiveMap
            if (materials[i].HasProperty("_EmissionMap"))
            {
                Texture emmisionMap = materials[i].GetTexture("_EmissionMap");
                if (emmisionMap != null)
                {
                    ExtractUtil.WriteText(binaryWriter, "EmmisiveMap", emmisionMap.name);
                }
            }
            // AOMap
            if (materials[i].HasProperty("_OcclusionMap"))
            {
                Texture aoMap = materials[i].GetTexture("_OcclusionMap");
                if (aoMap != null)
                {
                    ExtractUtil.WriteText(binaryWriter, "AOMap", aoMap.name);
                }
            }


            ExtractUtil.WriteTailTag(binaryWriter, "Material");
        }

        ExtractUtil.WriteTailTag(binaryWriter, "Materials");
    }

    public override void ExportLevelNodeData(BinaryWriter binaryWriter, Transform root)
    {
        ExportLevelNodeHeader(binaryWriter, root);

        ExtractUtil.WriteText(binaryWriter, "Mesh", "CubeMesh");

        MeshRenderer meshRenderer = gameObject.GetComponent<MeshRenderer>();
        Material[] materials = meshRenderer.sharedMaterials;
        if (materials.Length > 0) ExtractMaterials(binaryWriter, materials);

        ExportChildren(binaryWriter, root);

        ExportLevelNodeFooter(binaryWriter, root);
    }
}

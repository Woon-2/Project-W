using System.Collections;
using System.Collections.Generic;
using UnityEngine;
using System.IO;
using UnityEditor;
using System.Text;

public class TextureNamePrint : MonoBehaviour {
    private List<string> textureNames = new List<string>();

    void DispatchMaterial(Material material) {
        if (material.HasProperty("_MainTex"))
        {
            Texture mainAlbedoMap = material.GetTexture("_MainTex");
            if(mainAlbedoMap != null && !textureNames.Contains(mainAlbedoMap.name)) {
                textureNames.Add(mainAlbedoMap.name);
                Debug.Log("Found Texture: " + mainAlbedoMap.name);
            }
        }
        if (material.HasProperty("_SpecGlossMap"))
        {
            Texture specularcMap = material.GetTexture("_SpecGlossMap");
            if (specularcMap != null && !textureNames.Contains(specularcMap.name))
            {
                textureNames.Add(specularcMap.name);
                Debug.Log("Found Texture: " + specularcMap.name);
            }
        }
        if (material.HasProperty("_MetallicGlossMap"))
        {
            Texture metallicMap = material.GetTexture("_MetallicGlossMap");
            if (metallicMap != null && !textureNames.Contains(metallicMap.name))
            {
                textureNames.Add(metallicMap.name);
                Debug.Log("Found Texture: " + metallicMap.name);
            }
        }
        if (material.HasProperty("_BumpMap"))
        {
            Texture bumpMap = material.GetTexture("_BumpMap");
            if (bumpMap != null && !textureNames.Contains(bumpMap.name))
            {
                textureNames.Add(bumpMap.name);
                Debug.Log("Found Texture: " + bumpMap.name);
            }
        }
        if (material.HasProperty("_EmissionMap"))
        {
            Texture emissionMap = material.GetTexture("_EmissionMap");
            if (emissionMap != null && !textureNames.Contains(emissionMap.name))
            {
                textureNames.Add(emissionMap.name);
                Debug.Log("Found Texture: " + emissionMap.name);
            }
        }
        if (material.HasProperty("_DetailAlbedoMap"))
        {
            Texture detailAlbedoMap = material.GetTexture("_DetailAlbedoMap");
            if (detailAlbedoMap != null && !textureNames.Contains(detailAlbedoMap.name))
            {
                textureNames.Add(detailAlbedoMap.name);
                Debug.Log("Found Texture: " + detailAlbedoMap.name);
            }
        }
        if (material.HasProperty("_DetailNormalMap"))
        {
            Texture detailNormalMap = material.GetTexture("_DetailNormalMap");
            if (detailNormalMap != null && !textureNames.Contains(detailNormalMap.name))
            {
                textureNames.Add(detailNormalMap.name);
                Debug.Log("Found Texture: " + detailNormalMap.name);
            }
        }
    }

    void DispatchMaterials(Material[] materials) {
        foreach (Material material in materials) {
            DispatchMaterial(material);
        }
    }

    void WriteTextureHierarchy(Transform current) {
        Renderer renderer = current.GetComponent<Renderer>();
        if (renderer != null) {
            DispatchMaterials(renderer.materials);
        }
        foreach (Transform child in current) {
            WriteTextureHierarchy(child);
        }
    }

    void Start() {
        WriteTextureHierarchy(transform);
    }
}
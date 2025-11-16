using UnityEngine;
using UnityEditor;
using System;
using System.Collections;
using System.Collections.Generic;
using System.IO;
using System.Text;

public class SkyboxDDSExtractorWindow : EditorWindow
{
    private GameObject targetObject;
    private Dictionary<Texture, string> textureMappings = new Dictionary<Texture, string>();
    private Vector2 scrollPos;

    private BinaryWriter binaryWriter = null;

    // --- Cubemap 정보 구조 ---
    [Serializable]
    public class CubemapInfo
    {
        public string cubemapName;
        public List<Texture> faces = new List<Texture>();
        public string ddsPath;
        public TextureWrapMode wrapU;
        public TextureWrapMode wrapV;
        public TextureWrapMode wrapW;
        public string filterMode;
        public int anisoLevel;
    }

    private Dictionary<string, CubemapInfo> cubemapMappings = new Dictionary<string, CubemapInfo>();

    [MenuItem("Tools/Skybox Extractor(DDS)")]
    public static void OpenWindow()
    {
        GetWindow<SkyboxDDSExtractorWindow>("Skybox Extractor(DDS)");
    }

    private void OnGUI()
    {
        EditorGUILayout.LabelField("🎨 Skybox DDS Extractor Tool", EditorStyles.boldLabel);
        EditorGUILayout.Space();

        targetObject = (GameObject)EditorGUILayout.ObjectField("Target Object", targetObject, typeof(GameObject), true);
        EditorGUILayout.Space();

        if (GUILayout.Button("🔍 Scan Skybox") && targetObject != null)
        {
            ScanTextures(targetObject);
        }

        EditorGUILayout.Space();
        EditorGUILayout.LabelField("🧾 Cubemap List", EditorStyles.boldLabel);
        scrollPos = EditorGUILayout.BeginScrollView(scrollPos, GUILayout.Height(250));

        foreach (var kv in cubemapMappings)
        {
            var cube = kv.Value;
            EditorGUILayout.BeginHorizontal();
            EditorGUILayout.LabelField(cube.cubemapName, GUILayout.Width(200));
            cube.ddsPath = EditorGUILayout.TextField(cube.ddsPath);
            EditorGUILayout.EndHorizontal();
        }

        EditorGUILayout.EndScrollView();

        if (GUILayout.Button("💾 Export as Binary (.bin)"))
        {
            ExportBinary();
        }
    }

    private void ScanTextures(GameObject obj)
    {
        cubemapMappings.Clear();

        SkyboxSelector selector = obj.GetComponent<SkyboxSelector>();
        if (selector == null)
        {
            Debug.LogWarning($"{obj.name} doesn't have SkyboxSelector Component.");
            return;
        }

        string[] facesProp = { "_FrontTex", "_BackTex", "_LeftTex", "_RightTex", "_UpTex", "_DownTex" };

        for (int i = 0; i < selector.skyboxMaterials.Count; ++i)
        {
            Material mat = selector.skyboxMaterials[i];
            List<Texture> faceTextures = new List<Texture>();

            foreach (string faceProp in facesProp)
            {
                if (mat.HasProperty(faceProp))
                {
                    Texture tex = mat.GetTexture(faceProp);
                    if (tex != null)
                        faceTextures.Add(tex);
                }
            }

            if (faceTextures.Count == 6)
            {
                string cubeName = mat.name + "_Cubemap";

                if (cubemapMappings.ContainsKey(cubeName) == false)
                {
                    Texture sample = faceTextures[0];

                    cubemapMappings[cubeName] = new CubemapInfo()
                    {
                        cubemapName = cubeName,
                        faces = faceTextures,
                        ddsPath = "Assets/Textures/" + cubeName + ".dds",
                        wrapU = sample.wrapModeU,
                        wrapV = sample.wrapModeV,
                        wrapW = sample.wrapModeW,
                        filterMode = sample.filterMode.ToString(),
                        anisoLevel = sample.anisoLevel
                    };
                }
            }
        }

        Debug.Log($"[SkyboxExtractor] Found {cubemapMappings.Count} cubemaps.");
    }

    // ---------------- Export --------------------

    void ExtractTextureMapping()
    {
        ExtractUtil.WriteHeadTag(binaryWriter, "TextureMapping");

        foreach (var kvp in cubemapMappings)
        {
            CubemapInfo cube = kvp.Value;

            ExtractUtil.WriteHeadTag(binaryWriter, "Cube");

            ExtractUtil.WriteText(binaryWriter, "Name", cube.cubemapName);
            ExtractUtil.WriteText(binaryWriter, "WrapModeU", cube.wrapU.ToString());
            ExtractUtil.WriteText(binaryWriter, "WrapModeV", cube.wrapV.ToString());
            ExtractUtil.WriteText(binaryWriter, "WrapModeW", cube.wrapW.ToString());
            ExtractUtil.WriteText(binaryWriter, "FilterMode", cube.filterMode);
            ExtractUtil.WriteInteger(binaryWriter, "AnisoLevel", cube.anisoLevel);
            ExtractUtil.WriteText(binaryWriter, "DDSPath", cube.ddsPath);

            ExtractUtil.WriteTailTag(binaryWriter, "Cube");
        }

        ExtractUtil.WriteTailTag(binaryWriter, "TextureMapping");
    }

    void ExtractMaterials()
    {
        SkyboxSelector selector = targetObject.GetComponent<SkyboxSelector>();
        if (selector == null)
            return;

        ExtractUtil.WriteInteger(binaryWriter, "MaterialCnt", selector.skyboxMaterials.Count);

        for (int i = 0; i < selector.skyboxMaterials.Count; ++i)
        {
            Material mat = selector.skyboxMaterials[i];
            string cubeName = mat.name + "_Cubemap";

            ExtractUtil.WriteHeadTag(binaryWriter, "Material");
            ExtractUtil.WriteText(binaryWriter, "Cubemap", cubeName);
            ExtractUtil.WriteTailTag(binaryWriter, "Material");
        }
    }

    void ExportBinary()
    {
        string path = EditorUtility.SaveFilePanel("Export Binary", "ExportedAssets", "skybox.bin", "bin");
        binaryWriter = new BinaryWriter(File.Open(path, FileMode.Create));

        ExtractTextureMapping();
        ExtractMaterials();

        binaryWriter.Flush();
        binaryWriter.Close();
        Debug.Log("Skybox Binary Write Completed");
    }
}
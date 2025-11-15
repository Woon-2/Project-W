using UnityEngine;
using UnityEditor;
using System;
using System.Collections;
using System.Collections.Generic;
using System.IO;
using System.Text;

public class SkyboxExtractorWindow : EditorWindow
{
    private GameObject targetObject;
    private Dictionary<Texture, string> textureMappings = new Dictionary<Texture, string>();
    private Vector2 scrollPos;

    private BinaryWriter binaryWriter = null;

    [MenuItem("Tools/Skybox Extractor")]
    public static void OpenWindow()
    {
        GetWindow<SkyboxExtractorWindow>("Skybox Extractor");
    }

    private void OnGUI()
    {
        EditorGUILayout.LabelField("🎨 Skybox Extractor Tool", EditorStyles.boldLabel);
        EditorGUILayout.Space();

        targetObject = (GameObject)EditorGUILayout.ObjectField("Target Object", targetObject, typeof(GameObject), true);
        EditorGUILayout.Space();

        if (GUILayout.Button("🔍 Scan Textures") && targetObject != null)
        {
            ScanTextures(targetObject);
        }

        EditorGUILayout.Space();
        EditorGUILayout.LabelField("🧾 Texture List", EditorStyles.boldLabel);
        scrollPos = EditorGUILayout.BeginScrollView(scrollPos, GUILayout.Height(300));

        var keys = new List<Texture>(textureMappings.Keys);
        foreach (var tex in keys)
        {
            string texName = tex.name;
            EditorGUILayout.BeginHorizontal();
            EditorGUILayout.LabelField(texName, GUILayout.Width(200));
            textureMappings[tex] = EditorGUILayout.TextField(textureMappings[tex]);
            EditorGUILayout.EndHorizontal();
        }

        EditorGUILayout.EndScrollView();

        if (GUILayout.Button("💾 Export as a binary(.bin)"))
        {
            ExportBinary();
        }
    }

    private void ScanTextures(GameObject obj)
    {
        textureMappings.Clear();

        SkyboxSelector skyboxSelector = obj.GetComponent<SkyboxSelector>();
        if (skyboxSelector == null)
        {
            Debug.LogWarning(obj.name + " doesn't have SkyboxSelector Component, "
                + "skybox texture scanning failed.");
            return;
        }

        for (int i = 0; i < skyboxSelector.skyboxMaterials.Count; ++i)
        {
            Material skyMat = skyboxSelector.skyboxMaterials[i];

            string[] faces = { "_FrontTex", "_BackTex", "_LeftTex", "_RightTex", "_UpTex", "_DownTex" };
            string[] faceNames = { "+Z", "-Z", "+X", "-X", "+Y", "-Y" };

            for (int j = 0; j < faces.Length; j++)
            {
                if (skyMat.HasProperty(faces[j]))
                {
                    Texture tex = skyMat.GetTexture(faces[j]);
                    if (tex != null && !textureMappings.ContainsKey(tex))
                    {
                        textureMappings[tex] = AssetDatabase.GetAssetPath(tex);
                    }
                }
            }
        }

        Debug.Log($"[TextureMapper] Found {textureMappings.Count} textures in {obj.name}");
    }

    [System.Serializable]
    public class TextureMappingData
    {
        public List<TextureMappingItem> items = new List<TextureMappingItem>();

        public TextureMappingData(Dictionary<string, string> dict)
        {
            foreach (var kv in dict)
                items.Add(new TextureMappingItem(kv.Key, kv.Value));
        }
    }

    [System.Serializable]
    public class TextureMappingItem
    {
        public string textureName;
        public string texturePath;
        public TextureMappingItem(string name, string path)
        {
            textureName = name;
            texturePath = path;
        }
    }

    void ExtractTextureMapping()
    {
        ExtractUtil.WriteHeadTag(binaryWriter, "TextureMapping");

        foreach (var kvp in textureMappings)
        {
            ExtractUtil.WriteHeadTag(binaryWriter, "Item");
            Texture tex = kvp.Key;
            ExtractUtil.WriteText(binaryWriter, "TextureName", tex.name);
            ExtractUtil.WriteText(binaryWriter, "WrapModeU", tex.wrapModeU.ToString());
            ExtractUtil.WriteText(binaryWriter, "WrapModeV", tex.wrapModeV.ToString());
            ExtractUtil.WriteText(binaryWriter, "WrapModeW", tex.wrapModeW.ToString());
            if (tex.anisoLevel > 1)
            {
                ExtractUtil.WriteText(binaryWriter, "FilterMode", "Anisotropic");
            }
            else
            {
                ExtractUtil.WriteText(binaryWriter, "FilterMode", tex.filterMode.ToString());
            }
            ExtractUtil.WriteInteger(binaryWriter, "AnisoLevel", tex.anisoLevel);
            ExtractUtil.WriteText(binaryWriter, "Path", kvp.Value);
            ExtractUtil.WriteTailTag(binaryWriter, "Item");
        }

        ExtractUtil.WriteTailTag(binaryWriter, "TextureMapping");
    }

    void ExtractMaterials()
    {
        SkyboxSelector skyboxSelector = targetObject.GetComponent<SkyboxSelector>();
        if (skyboxSelector == null)
        {
            Debug.LogWarning(targetObject.name + " doesn't have SkyboxSelector Component, "
                + "the level will be exported without skybox info.");
            return;
        }

        ExtractUtil.WriteInteger(binaryWriter, "MaterialCnt", skyboxSelector.skyboxMaterials.Count);
        for (int i = 0; i < skyboxSelector.skyboxMaterials.Count; ++i)
        {
            ExtractUtil.WriteHeadTag(binaryWriter, "Material");
            Material skyMat = skyboxSelector.skyboxMaterials[i];

            string[] faces = { "_FrontTex", "_BackTex", "_LeftTex", "_RightTex", "_UpTex", "_DownTex" };
            string[] faceNames = { "+Z", "-Z", "+X", "-X", "+Y", "-Y" };

            for (int j = 0; j < faces.Length; j++)
            {
                if (skyMat.HasProperty(faces[j]))
                {
                    Texture tex = skyMat.GetTexture(faces[j]);
                    string texName = tex != null ? tex.name : "None";
                    ExtractUtil.WriteText(binaryWriter, faceNames[i], texName);
                }
            }

            ExtractUtil.WriteTailTag(binaryWriter, "Material");
        }
    }

    void ExportBinary()
    {
        string path = EditorUtility.SaveFilePanel("Export Binary", "ExportedAssets", "output.bin", "bin");
        binaryWriter = new BinaryWriter(File.Open(path, FileMode.Create));

        // 텍스처 매핑 정보를 가장 먼저 출력한다.
        // 나중에 임포트할 때, 중복되는 텍스처들을 다시 로드하지 않기 위해 필요하다.
        // 또한 텍스처를 추출할 때 그냥 이름 그대로 추출해도 되게 만든다.
        // 텍스처 이름(Key)이 이미 맵에 있다면 해당 경로(Value)의 텍스처는 로드하지 않는다.
        // (경로가 Key인 것보단 이름이 Key인 것이 SSO에서 유리할 것이다.)
        // (대신, 서로 다른 리소스간 중복된 텍스처 이름이 없어야 할 것.)
        ExtractTextureMapping();
        ExtractMaterials();

        binaryWriter.Flush();
        binaryWriter.Close();
        Debug.Log("Skybox Binary Write Completed");
    }
}

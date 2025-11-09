using UnityEngine;
using UnityEditor;
using System;
using System.Collections;
using System.Collections.Generic;
using System.IO;
using System.Text;

public class ModelExtractorWindow : EditorWindow
{
    private GameObject targetObject;
    private Dictionary<string, string> textureMappings = new Dictionary<string, string>();
    private Vector2 scrollPos;
    private string targetName = "";

    private BinaryWriter geometryWriter = null;

    [MenuItem("Tools/Model Extractor")]
    public static void OpenWindow()
    {
        GetWindow<ModelExtractorWindow>("Model Extractor");
    }

    private void OnGUI()
    {
        EditorGUILayout.LabelField("🎨 Model Extractor Tool", EditorStyles.boldLabel);
        EditorGUILayout.Space();

        targetObject = (GameObject)EditorGUILayout.ObjectField("Target Object", targetObject, typeof(GameObject), true);
        EditorGUILayout.Space();

        targetName = (string)EditorGUILayout.TextField("Object Name: ", targetName);
        EditorGUILayout.Space();

        if (GUILayout.Button("🔍 Scan Textures") && targetObject != null)
        {
            ScanTextures(targetObject);
        }

        if (textureMappings.Count > 0)
        {
            EditorGUILayout.Space();
            EditorGUILayout.LabelField("🧾 Texture List", EditorStyles.boldLabel);
            scrollPos = EditorGUILayout.BeginScrollView(scrollPos, GUILayout.Height(300));

            var keys = new List<string>(textureMappings.Keys);
            foreach (var texName in keys)
            {
                EditorGUILayout.BeginHorizontal();
                EditorGUILayout.LabelField(texName, GUILayout.Width(200));
                textureMappings[texName] = EditorGUILayout.TextField(textureMappings[texName]);
                EditorGUILayout.EndHorizontal();
            }

            EditorGUILayout.EndScrollView();

            if (GUILayout.Button("💾 Export as a binary(.bin)"))
            {
                ExportBinary();
            }
        }
    }

    private void ScanTextures(GameObject obj)
    {
        textureMappings.Clear();

        var renderers = obj.GetComponentsInChildren<Renderer>(true);
        foreach (var renderer in renderers)
        {
            foreach (var mat in renderer.sharedMaterials)
            {
                if (mat == null) continue;
                var shader = mat.shader;
                int count = ShaderUtil.GetPropertyCount(shader);

                for (int i = 0; i < count; i++)
                {
                    if (ShaderUtil.GetPropertyType(shader, i) == ShaderUtil.ShaderPropertyType.TexEnv)
                    {
                        string propName = ShaderUtil.GetPropertyName(shader, i);
                        Texture tex = mat.GetTexture(propName);
                        if (tex != null && !textureMappings.ContainsKey(tex.name))
                        {
                            textureMappings[tex.name] = AssetDatabase.GetAssetPath(tex);
                        }
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

    string ReplaceWhiteSpace(string str)
    {
        return string.Copy(str).Replace(" ", "_");
    }

    string MakeHeadTag(string tagSource)
    {
        return string.Format("<{0}:>", tagSource);
    }

    string MakeTailTag(string tagSource)
    {
        return string.Format("</{0}>", tagSource);
    }

    // WriteXX...: BinaryWriter로 특정한 자료형을 출력하기 위한 유틸리티 함수
    // ExtractXX...: WriteXX... 함수들을 이용하여 유니티의 특정한 자료구조를 출력하는 함수

    // @param Transform 노드 개수 추적의 대상이 될 트리(서브트리)의 루트노드
    // @param nodeCnt 노드 개수를 누적시킬 변수
    // Transform 트리(서브트리)의 노드 개수를 nodeCnt에 누적한다.
    void AccNodeCnt(Transform xform, ref int nodeCnt)
    {
        nodeCnt++;
        if (xform.childCount > 0)
        {
            for (int k = 0; k < xform.childCount; k++)
            {
                AccNodeCnt(xform.GetChild(k), ref nodeCnt);
            }
        }
    }

    void WriteString(BinaryWriter binaryWriter, string strToWrite)
    {
        binaryWriter.Write(strToWrite);
    }

    void WriteText(BinaryWriter binaryWriter, string tagSource, string text)
    {
        WriteHeadTag(binaryWriter, tagSource);
        WriteString(binaryWriter, text);
        WriteTailTag(binaryWriter, tagSource);
    }

    void WriteHeadTag(BinaryWriter binaryWriter, string tagSource)
    {
        WriteString(binaryWriter, MakeHeadTag(tagSource));
    }

    void WriteTailTag(BinaryWriter binaryWriter, string tagSource)
    {
        WriteString(binaryWriter, MakeTailTag(tagSource));
    }

    void WriteInteger(BinaryWriter binaryWriter, int value)
    {
        binaryWriter.Write(value);
    }

    void WriteInteger(BinaryWriter binaryWriter, string tagSource, int value)
    {
        WriteHeadTag(binaryWriter, tagSource);
        WriteInteger(binaryWriter, value);
        WriteTailTag(binaryWriter, tagSource);
    }

    void WriteFloat(BinaryWriter binaryWriter, float value)
    {
        binaryWriter.Write(value);
    }

    void WriteFloat(BinaryWriter binaryWriter, string tagSource, float value)
    {
        WriteHeadTag(binaryWriter, tagSource);
        WriteFloat(binaryWriter, value);
        WriteTailTag(binaryWriter, tagSource);
    }

    void WriteIntegers(BinaryWriter binaryWriter, string tagSource, int[] integers)
    {
        WriteHeadTag(binaryWriter, tagSource);
        WriteInteger(binaryWriter, "Cnt", integers.Length);
        if (integers.Length > 0)
        {
            foreach (int i in integers)
            {
                WriteInteger(binaryWriter, i);
            }
        }
        WriteTailTag(binaryWriter, tagSource);
    }

    void WriteU16(BinaryWriter binaryWriter, ushort value)
    {
        binaryWriter.Write(value);
    }

    void WriteU16(BinaryWriter binaryWriter, string tagSource, ushort value)
    {
        WriteHeadTag(binaryWriter, tagSource);
        WriteU16(binaryWriter, value);
        WriteTailTag(binaryWriter, tagSource);
    }

    void WriteU16s(BinaryWriter binaryWriter, string tagSource, ushort[] u16s)
    {
        WriteHeadTag(binaryWriter, tagSource);
        WriteInteger(binaryWriter, "Cnt", u16s.Length);
        if (u16s.Length > 0)
        {
            foreach (ushort i in u16s)
            {
                WriteU16(binaryWriter, i);
            }
        }
        WriteTailTag(binaryWriter, tagSource);
    }

    void WriteIntegerAsU16s(BinaryWriter binaryWriter, string tagSource, int[] integers)
    {
        WriteHeadTag(binaryWriter, tagSource);
        WriteInteger(binaryWriter, "Cnt", integers.Length);
        if (integers.Length > 0)
        {
            foreach (int i in integers)
            {
                WriteU16(binaryWriter, (ushort)i);
            }
        }
        WriteTailTag(binaryWriter, tagSource);
    }

    void WriteMatrix(BinaryWriter binaryWriter, string tagSource, Matrix4x4 matrix)
    {
        WriteHeadTag(binaryWriter, tagSource);
        binaryWriter.Write(matrix.m00);
        binaryWriter.Write(matrix.m10);
        binaryWriter.Write(matrix.m20);
        binaryWriter.Write(matrix.m30);
        binaryWriter.Write(matrix.m01);
        binaryWriter.Write(matrix.m11);
        binaryWriter.Write(matrix.m21);
        binaryWriter.Write(matrix.m31);
        binaryWriter.Write(matrix.m02);
        binaryWriter.Write(matrix.m12);
        binaryWriter.Write(matrix.m22);
        binaryWriter.Write(matrix.m32);
        binaryWriter.Write(matrix.m03);
        binaryWriter.Write(matrix.m13);
        binaryWriter.Write(matrix.m23);
        binaryWriter.Write(matrix.m33);
        WriteTailTag(binaryWriter, tagSource);
    }

    void WriteLocalMatrix(BinaryWriter binaryWriter, string tagSource, Transform xform)
    {
        Matrix4x4 matrix = Matrix4x4.identity;
        matrix.SetTRS(xform.localPosition, xform.localRotation, xform.localScale);
        WriteMatrix(binaryWriter, tagSource, matrix);
    }

    void WriteDressMatrix(BinaryWriter binaryWriter, string tagSource, Transform dressXform, Transform xform)
    {
        WriteMatrix(binaryWriter, tagSource, dressXform.worldToLocalMatrix * xform.localToWorldMatrix);
    }

    void WriteVector(BinaryWriter binaryWriter, Vector2 vec)
    {
        binaryWriter.Write(vec.x);
        binaryWriter.Write(vec.y);
    }

    void WriteVector(BinaryWriter binaryWriter, string tagSource, Vector2 vec)
    {
        WriteHeadTag(binaryWriter, tagSource);
        WriteVector(binaryWriter, vec);
        WriteTailTag(binaryWriter, tagSource);
    }

    void WriteVectors(BinaryWriter binaryWriter, string tagSource, Vector2[] vectors)
    {
        WriteHeadTag(binaryWriter, tagSource);
        WriteInteger(binaryWriter, "Cnt", vectors.Length);
        if (vectors.Length > 0)
        {
            foreach (Vector2 v in vectors)
            {
                WriteVector(binaryWriter, v);
            }
        }
        WriteTailTag(binaryWriter, tagSource);
    }

    void WriteVector(BinaryWriter binaryWriter, Vector3 vec)
    {
        binaryWriter.Write(vec.x);
        binaryWriter.Write(vec.y);
        binaryWriter.Write(vec.z);
    }

    void WriteVector(BinaryWriter binaryWriter, string tagSource, Vector3 vec)
    {
        WriteHeadTag(binaryWriter, tagSource);
        WriteVector(binaryWriter, vec);
        WriteTailTag(binaryWriter, tagSource);
    }

    void WriteVectors(BinaryWriter binaryWriter, string tagSource, Vector3[] vectors)
    {
        WriteHeadTag(binaryWriter, tagSource);
        WriteInteger(binaryWriter, "Cnt", vectors.Length);
        if (vectors.Length > 0)
        {
            foreach (Vector3 v in vectors)
            {
                WriteVector(binaryWriter, v);
            }
        }
        WriteTailTag(binaryWriter, tagSource);
    }

    void WriteColor(BinaryWriter binaryWriter, Color c)
    {
        binaryWriter.Write(c.r);
        binaryWriter.Write(c.g);
        binaryWriter.Write(c.b);
        binaryWriter.Write(c.a);
    }

    void WriteColor(BinaryWriter binaryWriter, string tagSource, Color c)
    {
        WriteHeadTag(binaryWriter, tagSource);
        WriteColor(binaryWriter, c);
        WriteTailTag(binaryWriter, tagSource);
    }

    void WriteColors(BinaryWriter binaryWriter, string tagSource, Color[] colors)
    {
        WriteHeadTag(binaryWriter, tagSource);
        WriteInteger(binaryWriter, "Cnt", colors.Length);
        if (colors.Length > 0)
        {
            foreach (Color c in colors)
            {
                WriteColor(binaryWriter, c);
            }
        }
        WriteTailTag(binaryWriter, tagSource);
    }

    void ExtractTextureMapping()
    {
        WriteHeadTag(geometryWriter, "TextureMapping");

        foreach (var kvp in textureMappings)
        {
            WriteHeadTag(geometryWriter, "Item");
            WriteString(geometryWriter, kvp.Key);
            WriteString(geometryWriter, kvp.Value);
            WriteTailTag(geometryWriter, "Item");
        }

        WriteTailTag(geometryWriter, "TextureMapping");
    }

    void ExtractMesh(Mesh mesh)
    {
        WriteHeadTag(geometryWriter, "Mesh");

        // 버텍스 버퍼들 추출
        WriteHeadTag(geometryWriter, "VertexBuffers");
        if ((mesh.vertices != null) && (mesh.vertices.Length > 0)) WriteVectors(geometryWriter, "Positions", mesh.vertices);
        if ((mesh.colors != null) && (mesh.colors.Length > 0)) WriteColors(geometryWriter, "Colors", mesh.colors);
        if ((mesh.normals != null) && (mesh.normals.Length > 0)) WriteVectors(geometryWriter, "Normals", mesh.normals);
        if ((mesh.uv != null) && (mesh.uv.Length > 0)) WriteVectors(geometryWriter, "TextureCoords0", mesh.uv);
        if ((mesh.uv2 != null) && (mesh.uv2.Length > 0)) WriteVectors(geometryWriter, "TextureCoords1", mesh.uv2);
        WriteTailTag(geometryWriter, "VertexBuffers");

        // 서브메시(인덱스 버퍼) 추출
        // 최고 인덱스가 65536 미만일 경우 16비트 부호 없는 정수형으로 Write,
        // 그렇지 않을 경우 32비트 부호 없는 정수형으로 Write한다.
        WriteHeadTag(geometryWriter, "Submeshes");

        WriteInteger(geometryWriter, "SubmeshCnt", mesh.subMeshCount);
        int maxIdx = 0;
        for (int i = 0; i < mesh.subMeshCount; i++)
        {
            int[] subIndices = mesh.GetTriangles(i);
            foreach (int subIdx in subIndices)
            {
                maxIdx = Math.Max(subIdx, maxIdx);
            }
        }
        WriteInteger(geometryWriter, "MaxIndex", maxIdx);

        if (maxIdx < 65536)
        {
            for (int i = 0; i < mesh.subMeshCount; i++)
            {
                int[] subIndices = mesh.GetTriangles(i);
                WriteIntegerAsU16s(geometryWriter, "Submesh", subIndices);
            }
        }
        else
        {
            for (int i = 0; i < mesh.subMeshCount; i++)
            {
                int[] subIndices = mesh.GetTriangles(i);
                WriteIntegers(geometryWriter, "Submesh", subIndices);
            }
        }

        WriteTailTag(geometryWriter, "Submeshes");

        WriteTailTag(geometryWriter, "Mesh");
    }

    void ExtractMaterials(Material[] materials)
    {
        WriteHeadTag(geometryWriter, "Materials");

        WriteInteger(geometryWriter, "MaterialCnt", materials.Length);

        for (int i = 0; i < materials.Length; i++)
        {
            WriteHeadTag(geometryWriter, "Material");

            // 상수들 추출
            // cAlbedo
            if (materials[i].HasProperty("_Color"))
            {
                Color albedo = materials[i].GetColor("_Color");
                WriteColor(geometryWriter, "cAlbedo", albedo);
            }
            // cEmmisive
            if (materials[i].HasProperty("_EmissionColor"))
            {
                Color emission = Color.black; // Default value for emission color when emission is not enabled

                if (materials[i].IsKeywordEnabled("_EMISSION"))
                {
                    emission = materials[i].GetColor("_EmissionColor");
                }

                WriteColor(geometryWriter, "cEmmisive", emission);
            }
            // cSmoothness
            if (materials[i].HasProperty("_Smoothness"))
            {
                WriteFloat(geometryWriter, "cSmoothness", materials[i].GetFloat("_Smoothness"));
            }
            // cMetallic
            if (materials[i].HasProperty("_Metallic"))
            {
                WriteFloat(geometryWriter, "cMetallic", materials[i].GetFloat("_Metallic"));
            }

            // 텍스처들 추출
            // AlbedoMap
            if (materials[i].HasProperty("_MainTex"))
            {
                Texture mainAlbedoMap = materials[i].GetTexture("_MainTex");
                if (mainAlbedoMap != null)
                {
                    WriteText(geometryWriter, "AlbedoMap", mainAlbedoMap.name);
                }
            }
            // NormalMap
            if (materials[i].HasProperty("_BumpMap"))
            {
                Texture bumpMap = materials[i].GetTexture("_BumpMap");
                if (bumpMap != null)
                {
                    WriteText(geometryWriter, "NormalMap", bumpMap.name);
                }
            }
            // MetallicSmoothnessMap
            if (materials[i].HasProperty("_MetallicGlossMap"))
            {
                Texture metallicGlossMap = materials[i].GetTexture("_MetallicGlossMap");
                if (metallicGlossMap != null)
                {
                    WriteText(geometryWriter, "MetallicSmoothnessMap", metallicGlossMap.name);
                }
            }
            // EmmisiveMap
            if (materials[i].HasProperty("_EmissionMap"))
            {
                Texture emmisionMap = materials[i].GetTexture("_EmissionMap");
                if (emmisionMap != null)
                {
                    WriteText(geometryWriter, "EmmisiveMap", emmisionMap.name);
                }
            }

            WriteTailTag(geometryWriter, "Material");
        }

        WriteTailTag(geometryWriter, "Materials");
    }

    void ExtractTransform(Transform root, Transform xform)
    {
        WriteHeadTag(geometryWriter, "Node");
        WriteString(geometryWriter, ReplaceWhiteSpace(xform.gameObject.name));

        // 변환 추출
        WriteLocalMatrix(geometryWriter, "LocalMatrix", xform);
        WriteDressMatrix(geometryWriter, "DressMatrix", root, xform);

        // 메시 추출
        MeshRenderer meshRenderer = xform.gameObject.GetComponent<MeshRenderer>();
        MeshFilter meshFilter = xform.gameObject.GetComponent<MeshFilter>();
        SkinnedMeshRenderer skinnedMeshRenderer = xform.gameObject.GetComponent<SkinnedMeshRenderer>();

        if (meshRenderer && meshFilter)
        {
            if (meshRenderer.enabled)
            {
                ExtractMesh(meshFilter.sharedMesh);

                Material[] materials = meshRenderer.sharedMaterials;
                if (materials.Length > 0) ExtractMaterials(materials);
            }
        }
        else if (skinnedMeshRenderer && skinnedMeshRenderer.enabled == true)
        {
            ExtractMesh(skinnedMeshRenderer.sharedMesh);

            Material[] materials = skinnedMeshRenderer.sharedMaterials;
            if (materials.Length > 0) ExtractMaterials(materials);
        }

        WriteInteger(geometryWriter, "ChildCnt", xform.childCount);
        WriteHeadTag(geometryWriter, "Children");

        if (xform.childCount > 0)
        {
            for (int k = 0; k < xform.childCount; k++)
            {
                ExtractTransform(root, xform.GetChild(k));
            }
        }

        WriteTailTag(geometryWriter, "Children");

        WriteTailTag(geometryWriter, "Node");
    }

    void ExtractGeometry()
    {
        WriteHeadTag(geometryWriter, "Geometry");
        int nodeCnt = 0;
        AccNodeCnt(targetObject.transform, ref nodeCnt);
        WriteInteger(geometryWriter, "NodeCnt", nodeCnt);

        ExtractTransform(targetObject.transform, targetObject.transform);

        WriteTailTag(geometryWriter, "Geometry");
    }

    void ExportBinary()
    {
        string path = EditorUtility.SaveFilePanel("Export Binary", "ExportedAssets", "output.bin", "bin");
        geometryWriter = new BinaryWriter(File.Open(path, FileMode.Create));

        WriteText(geometryWriter, "ModelName", targetName);

        // 텍스처 매핑 정보를 가장 먼저 출력한다.
        // 나중에 임포트할 때, 중복되는 텍스처들을 다시 로드하지 않기 위해 필요하다.
        // 또한 텍스처를 추출할 때 그냥 이름 그대로 추출해도 되게 만든다.
        // 텍스처 이름(Key)이 이미 맵에 있다면 해당 경로(Value)의 텍스처는 로드하지 않는다.
        // (경로가 Key인 것보단 이름이 Key인 것이 SSO에서 유리할 것이다.)
        // (대신, 서로 다른 리소스간 중복된 텍스처 이름이 없어야 할 것.)
        ExtractTextureMapping();
        ExtractGeometry();

        geometryWriter.Flush();
        geometryWriter.Close();
        Debug.Log("Model Binary Write Completed");
    }
}

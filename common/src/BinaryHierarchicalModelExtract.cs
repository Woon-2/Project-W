//#define _WITH_SKINNED_BONES_ANIMATION

using System.Collections;
using System.Collections.Generic;
using UnityEngine;
using System.IO;
using UnityEditor;
using System.Text;

public class BinaryHierarchicalModelExtract : MonoBehaviour
{
    public Transform geometry = null;
    public Transform skeleton = null;
    public GameObject go = null;

    private List<string> m_pTextureNamesListForCounting = new List<string>();
    private List<string> m_pTextureNamesListForWriting = new List<string>();

    private List<string> m_pTexturePath = new List<string>();

    // path : path, resourceTypeIndex, ArrayIndex
    private Dictionary<string, (string, int, int, int)> m_pTextureIndexInfo = new Dictionary<string, (string, int, int, int)>();
    private Dictionary<(int, int, int, int), (string, string)> m_mapRefMap = new Dictionary<(int, int, int, int), (string, string)>();

    private BinaryWriter geometryWriter = null;
    private BinaryWriter skeletonWriter = null;
    private BinaryReader binaryReader = null;

    private List<Transform> bones = null;
    private List<Matrix4x4> bindposes = null;

    public AnimationClip[] animClips;
    public string[] clipExtractionNames;
    public float[] clipPresampleFPSs;
    public float[] keyFramePosThresholds;
    public float[] keyFrameRotThresholds; // degrees
    public float[] keyFrameScaleThresholds;

    void WriteDictionary(BinaryWriter binaryWriter)
    {
        binaryWriter.Write("<Dictionary:>");

        foreach (var kvp in m_mapRefMap)
        {
            binaryWriter.Write("<Item:>");
            binaryWriter.Write(kvp.Key.Item1);    // MapRef
            binaryWriter.Write(kvp.Key.Item2);
            binaryWriter.Write(kvp.Key.Item3);
            binaryWriter.Write(kvp.Key.Item4);
            WriteString(binaryWriter, kvp.Value.Item2);   // newTexPath
        }

        binaryWriter.Write("</Dictionary>");
    }

    string ReadString()
    {
        int length = binaryReader.ReadInt32(); // Read string length
        byte[] stringBytes = binaryReader.ReadBytes(length);
        return System.Text.Encoding.UTF8.GetString(stringBytes); // Decode string
    }

    void ReadRemapFile(string path)
    {
        binaryReader = new BinaryReader(File.Open(path, FileMode.Open));

        string convertMapTag = ReadString();
        if (convertMapTag != "<ConvertMap:>")
        {
            throw new InvalidDataException("Invalid file format: missing <ConvertMap:> tag.");
        }

        int i = 0;

        // Read Items in ConvertMap
        while (true)
        {
            string itemTag = ReadString();
            if (itemTag == "</ConvertMap>")
            {
                break; // End of ConvertMap section
            }
            if (itemTag != "<Item:>")
            {
                throw new InvalidDataException("Invalid file format: missing <Item:> tag.");
            }

            string inputPath = ReadString();
            string newTexPath = ReadString();
            int resourceTypeIndex = binaryReader.ReadInt32();
            int arrayIndex = binaryReader.ReadInt32();
            int colorSpace = binaryReader.ReadInt32();

            m_pTextureIndexInfo.Add(inputPath, (newTexPath, resourceTypeIndex, arrayIndex, colorSpace));
            m_mapRefMap.Add((resourceTypeIndex, i++, arrayIndex, colorSpace), (inputPath, newTexPath));
        }

        binaryReader.Close();
    }

    void DispatchMaterials(Material[] materials)
    {
        for (int i = 0; i < materials.Length; i++)
        {
            if (materials[i].HasProperty("_MainTex"))
            {
                Texture mainAlbedoMap = materials[i].GetTexture("_MainTex");
                if (mainAlbedoMap != null && !m_pTexturePath.Contains(mainAlbedoMap.name))
                    m_pTexturePath.Add(mainAlbedoMap.name);
            }
            if (materials[i].HasProperty("_SpecGlossMap"))
            {
                Texture specularcMap = materials[i].GetTexture("_SpecGlossMap");
                if (specularcMap != null && !m_pTexturePath.Contains(specularcMap.name))
                    m_pTexturePath.Add(specularcMap.name);
            }
            if (materials[i].HasProperty("_MetallicGlossMap"))
            {
                Texture metallicMap = materials[i].GetTexture("_MetallicGlossMap");
                if (metallicMap != null && !m_pTexturePath.Contains(metallicMap.name))
                    m_pTexturePath.Add(metallicMap.name);
            }
            if (materials[i].HasProperty("_BumpMap"))
            {
                Texture bumpMap = materials[i].GetTexture("_BumpMap");
                if (bumpMap != null && !m_pTexturePath.Contains(bumpMap.name))
                    m_pTexturePath.Add(bumpMap.name);
            }
            if (materials[i].HasProperty("_EmissionMap"))
            {
                Texture emissionMap = materials[i].GetTexture("_EmissionMap");
                if (emissionMap != null && !m_pTexturePath.Contains(emissionMap.name))
                    m_pTexturePath.Add(emissionMap.name);
            }
            if (materials[i].HasProperty("_OcclusionMap"))
            {
                Texture occlusionMap = materials[i].GetTexture("_OcclusionMap");
                if (occlusionMap != null && !m_pTexturePath.Contains(occlusionMap.name))
                    m_pTexturePath.Add(occlusionMap.name);
            }
        }
    }

    void StoreMaterialInfo(Transform current)
    {
        SkinnedMeshRenderer skinnedMeshRenderer = current.gameObject.GetComponent<SkinnedMeshRenderer>();

        if (skinnedMeshRenderer)
        {
            Material[] materials = skinnedMeshRenderer.materials;
            if (materials.Length > 0)
            {
                DispatchMaterials(materials);
            }
            return;
        }

        MeshRenderer meshRenderer = current.gameObject.GetComponent<MeshRenderer>();
        MeshFilter meshFilter = current.gameObject.GetComponent<MeshFilter>();

        if (meshRenderer && meshFilter)
        {
            Material[] materials = meshRenderer.materials;
            if (materials.Length > 0)
            {
                DispatchMaterials(materials);
            }
        }
    }

    void ArrangeTextureList(Transform child)
    {
        StoreMaterialInfo(child);

        if (child.childCount > 0)
        {
            for (int k = 0; k < child.childCount; k++)
            {
                ArrangeTextureList(child.GetChild(k));
            }
        }
    }

    void CalculateResourceIndices()
    {
        var updatedMapRefMap = new Dictionary<(int, int, int, int), (string, string)>();

        foreach (var kvp in m_mapRefMap)
        {
            var oldKey = kvp.Key;       // 현재 키 (int, int, int, int)
            var value = kvp.Value;      // 현재 값 (string, string)

            string inputPath = value.Item1;
            string newPath = value.Item2;

            // inputPath가 m_pTexturePath에 있는지 확인하고 인덱스 찾기
            int newIndex = m_pTexturePath.IndexOf(inputPath);
            if (newIndex == -1)
            {
                Debug.LogWarning($"Value '{inputPath}' not found in m_pTexturePath.");
                continue; // 존재하지 않는 경우 스킵
            }

            // 새로운 키 생성: 두 번째 값(oldKey.Item2)을 newIndex로 교체
            var newKey = (oldKey.Item1, newIndex, oldKey.Item3, oldKey.Item4);

            // 새 딕셔너리에 추가
            updatedMapRefMap[newKey] = value;
        }

        // 기존 딕셔너리를 갱신된 딕셔너리로 교체
        m_mapRefMap = updatedMapRefMap;
    }

    void accNodeCntRecursive(Transform child, ref int nodeCnt)
    {
        nodeCnt++;
        if (child.childCount > 0)
        {
            for (int k = 0; k < child.childCount; k++)
            {
                accNodeCntRecursive(child.GetChild(k), ref nodeCnt);
            }
        }
    }

    bool FindTextureByName(List<string> pTextureNamesList, Texture texture)
    {
        if (texture)
        {
            string strTextureName = string.Copy(texture.name).Replace(" ", "_");
            for (int i = 0; i < pTextureNamesList.Count; i++)
            {
                if (pTextureNamesList.Contains(strTextureName)) return (true);
            }
            pTextureNamesList.Add(strTextureName);
            return (false);
        }
        else
        {
            return (true);
        }
    }

    void WriteObjectName(BinaryWriter binaryWriter, Object obj)
    {
        binaryWriter.Write((obj) ? string.Copy(obj.name).Replace(" ", "_") : "null");
    }

    void WriteObjectName(BinaryWriter binaryWriter, int i, Object obj)
    {
        binaryWriter.Write(i);
        binaryWriter.Write((obj) ? string.Copy(obj.name).Replace(" ", "_") : "null");
    }

    void WriteObjectName(BinaryWriter binaryWriter, string strHeader, Object obj)
    {
        binaryWriter.Write(strHeader);
        binaryWriter.Write((obj) ? string.Copy(obj.name).Replace(" ", "_") : "null");
    }

    void WriteObjectName(BinaryWriter binaryWriter, string strHeader, int i, Object obj)
    {
        binaryWriter.Write(strHeader);
        binaryWriter.Write(i);
        binaryWriter.Write((obj) ? string.Copy(obj.name).Replace(" ", "_") : "null");
    }

    void WriteObjectName(BinaryWriter binaryWriter, string strHeader, int i, int j, Object obj)
    {
        binaryWriter.Write(strHeader);
        binaryWriter.Write(i);
        binaryWriter.Write(j);
        binaryWriter.Write((obj) ? string.Copy(obj.name).Replace(" ", "_") : "null");
    }

    void WriteObjectName(BinaryWriter binaryWriter, string strHeader, int i, Object obj, float f, int j, int k)
    {
        binaryWriter.Write(strHeader);
        binaryWriter.Write(i);
        binaryWriter.Write((obj) ? string.Copy(obj.name).Replace(" ", "_") : "null");
        binaryWriter.Write(f);
        binaryWriter.Write(j);
        binaryWriter.Write(k);
    }

    void WriteString(BinaryWriter binaryWriter, string strToWrite)
    {
        binaryWriter.Write(strToWrite);
    }

    void WriteString(BinaryWriter binaryWriter, string strHeader, string strToWrite)
    {
        binaryWriter.Write(strHeader);
        binaryWriter.Write(strToWrite);
    }

    void WriteString(BinaryWriter binaryWriter, string strToWrite, int i)
    {
        binaryWriter.Write(strToWrite);
        binaryWriter.Write(i);
    }

    void WriteString(BinaryWriter binaryWriter, string strToWrite, int i, float f)
    {
        binaryWriter.Write(strToWrite);
        binaryWriter.Write(i);
        binaryWriter.Write(f);
    }

    void WriteTextureName(BinaryWriter binaryWriter, string strHeader, string strFooter, Texture texture)
    {
        binaryWriter.Write(strHeader);
        if (texture)
        {
            binaryWriter.Write(string.Copy(texture.name).Replace(" ", "_"));
        }
        else
        {
            binaryWriter.Write("null");
        }
        binaryWriter.Write(strFooter);
    }

    void WriteInteger(BinaryWriter binaryWriter, int i)
    {
        binaryWriter.Write(i);
    }

    void WriteInteger(BinaryWriter binaryWriter, string strHeader, int i)
    {
        binaryWriter.Write(strHeader);
        binaryWriter.Write(i);
    }

    void WriteFloat(BinaryWriter binaryWriter, float f)
    {
        binaryWriter.Write(f);
    }


    void WriteFloat(BinaryWriter binaryWriter, string strHeader, float f)
    {
        binaryWriter.Write(strHeader);
        binaryWriter.Write(f);
    }

    void WriteVector(BinaryWriter binaryWriter, Vector2 v)
    {
        binaryWriter.Write(v.x);
        binaryWriter.Write(v.y);
    }

    void WriteVector(BinaryWriter binaryWriter, string strHeader, Vector2 v)
    {
        binaryWriter.Write(strHeader);
        WriteVector(binaryWriter, v);
    }

    void WriteVector(BinaryWriter binaryWriter, Vector3 v)
    {
        binaryWriter.Write(v.x);
        binaryWriter.Write(v.y);
        binaryWriter.Write(v.z);
    }

    void WriteVector(BinaryWriter binaryWriter, string strHeader, Vector3 v)
    {
        binaryWriter.Write(strHeader);
        WriteVector(binaryWriter, v);
    }

    void WriteVector(BinaryWriter binaryWriter, Vector4 v)
    {
        binaryWriter.Write(v.x);
        binaryWriter.Write(v.y);
        binaryWriter.Write(v.z);
        binaryWriter.Write(v.w);
    }

    void WriteVector(BinaryWriter binaryWriter, string strHeader, Vector4 v)
    {
        binaryWriter.Write(strHeader);
        WriteVector(binaryWriter, v);
    }

    void WriteVector(BinaryWriter binaryWriter, Quaternion q)
    {
        binaryWriter.Write(q.x);
        binaryWriter.Write(q.y);
        binaryWriter.Write(q.z);
        binaryWriter.Write(q.w);
    }

    void WriteVector(BinaryWriter binaryWriter, string strHeader, Quaternion q)
    {
        binaryWriter.Write(strHeader);
        WriteVector(binaryWriter, q);
    }

    void WriteColor(BinaryWriter binaryWriter, Color c)
    {
        binaryWriter.Write(c.r);
        binaryWriter.Write(c.g);
        binaryWriter.Write(c.b);
        binaryWriter.Write(c.a);
    }

    void WriteColor(BinaryWriter binaryWriter, string strHeader, Color c)
    {
        binaryWriter.Write(strHeader);
        WriteColor(binaryWriter, c);
    }

    void WriteTextureCoord(BinaryWriter binaryWriter, Vector2 uv)
    {
        binaryWriter.Write(uv.x);
        binaryWriter.Write(1.0f - uv.y);
    }

    void WriteVectors(BinaryWriter binaryWriter, string strHeader, Vector2[] vectors)
    {
        binaryWriter.Write(strHeader);
        binaryWriter.Write(vectors.Length);
        if (vectors.Length > 0) foreach (Vector2 v in vectors) WriteVector(binaryWriter, v);
    }

    void WriteVectors(BinaryWriter binaryWriter, string strHeader, Vector3[] vectors)
    {
        binaryWriter.Write(strHeader);
        binaryWriter.Write(vectors.Length);
        if (vectors.Length > 0) foreach (Vector3 v in vectors) WriteVector(binaryWriter, v);
    }

    void WriteVectors(BinaryWriter binaryWriter, string strHeader, Vector4[] vectors)
    {
        binaryWriter.Write(strHeader);
        binaryWriter.Write(vectors.Length);
        if (vectors.Length > 0) foreach (Vector4 v in vectors) WriteVector(binaryWriter, v);
    }

    void WriteColors(BinaryWriter binaryWriter, string strHeader, Color[] colors)
    {
        binaryWriter.Write(strHeader);
        binaryWriter.Write(colors.Length);
        if (colors.Length > 0) foreach (Color c in colors) WriteColor(binaryWriter, c);
    }

    void WriteTextureCoords(BinaryWriter binaryWriter, string strHeader, Vector2[] uvs)
    {
        binaryWriter.Write(strHeader);
        binaryWriter.Write(uvs.Length);
        if (uvs.Length > 0) foreach (Vector2 uv in uvs) WriteTextureCoord(binaryWriter, uv);
    }

    void WriteIntegers(BinaryWriter binaryWriter, int[] pIntegers)
    {
        binaryWriter.Write(pIntegers.Length);
        foreach (int i in pIntegers) binaryWriter.Write(i);
    }

    void WriteIntegers(BinaryWriter binaryWriter, string strHeader, int[] pIntegers)
    {
        binaryWriter.Write(strHeader);
        binaryWriter.Write(pIntegers.Length);
        if (pIntegers.Length > 0) foreach (int i in pIntegers) binaryWriter.Write(i);
    }

    void WriteUInteger16s(BinaryWriter binaryWriter, string strHeader, int n, int[] pIntegers)
    {
        binaryWriter.Write(strHeader);
        binaryWriter.Write(n);
        binaryWriter.Write(pIntegers.Length);
        if (pIntegers.Length > 0) foreach (int i in pIntegers) binaryWriter.Write((ushort)i);
    }

    void WriteIntegers(BinaryWriter binaryWriter, string strHeader, int n, int[] pIntegers)
    {
        binaryWriter.Write(strHeader);
        binaryWriter.Write(n);
        binaryWriter.Write(pIntegers.Length);
        if (pIntegers.Length > 0) foreach (int i in pIntegers) binaryWriter.Write(i);
    }

    void WriteBoundingBox(BinaryWriter binaryWriter, string strHeader, Bounds bounds)
    {
        binaryWriter.Write(strHeader);
        WriteVector(binaryWriter, bounds.center);
        WriteVector(binaryWriter, bounds.extents);
    }

    void WriteMatrix(BinaryWriter binaryWriter, Matrix4x4 matrix)
    {
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
    }

    void WriteMatrix(BinaryWriter binaryWriter, string strHeader, Matrix4x4 matrix)
    {
        binaryWriter.Write(strHeader);
        WriteMatrix(binaryWriter, matrix);
    }

    void WriteMatrix(BinaryWriter binaryWriter, Vector3 position, Quaternion rotation, Vector3 scale)
    {
        Matrix4x4 matrix = Matrix4x4.identity;
        matrix.SetTRS(position, rotation, scale);
        WriteMatrix(binaryWriter, matrix);
    }

    void WriteTransform(BinaryWriter binaryWriter, string strHeader, Transform current)
    {
        binaryWriter.Write(strHeader);
        WriteVector(binaryWriter, current.localPosition);
        WriteVector(binaryWriter, current.localEulerAngles);
        WriteVector(binaryWriter, current.localScale);
        WriteVector(binaryWriter, current.localRotation);
    }

    void WriteLocalMatrix(BinaryWriter binaryWriter, string strHeader, Transform current)
    {
        binaryWriter.Write(strHeader);
        Matrix4x4 matrix = Matrix4x4.identity;
        matrix.SetTRS(current.localPosition, current.localRotation, current.localScale);
        WriteMatrix(binaryWriter, matrix);
    }

    void WriteWorldMatrix(BinaryWriter binaryWriter, string strHeader, Transform current)
    {
        binaryWriter.Write(strHeader);
        Matrix4x4 matrix = Matrix4x4.identity;
        matrix.SetTRS(current.position, current.rotation, current.lossyScale);
        WriteMatrix(binaryWriter, matrix);
    }

    void WriteMatrixes(BinaryWriter binaryWriter, string strHeader, Matrix4x4[] matrixes)
    {
        WriteString(binaryWriter, strHeader, matrixes.Length);
        if (matrixes.Length > 0)
        {
            foreach (Matrix4x4 matrix in matrixes) WriteMatrix(binaryWriter, matrix);
        }
    }

    void WriteBoneWeights(BinaryWriter binaryWriter, string strHeader, BoneWeight[] boneWeights, int[] boneIndices)
    {
        WriteString(binaryWriter, strHeader, boneWeights.Length);
        if (boneWeights.Length > 0)
        {
            foreach (BoneWeight bw in boneWeights)
            {
                binaryWriter.Write(bw.weight0);
                binaryWriter.Write(bw.weight1);
                binaryWriter.Write(bw.weight2);
                binaryWriter.Write(bw.weight3);
            }
        }
    }

    void WriteBoneIndices(BinaryWriter binaryWriter, string strHeader, BoneWeight[] boneWeights, int[] boneIndices)
    {
        WriteString(binaryWriter, strHeader, boneWeights.Length);
        if (boneWeights.Length > 0)
        {
            foreach (BoneWeight bw in boneWeights)
            {
                binaryWriter.Write(boneIndices[bw.boneIndex0]);
                binaryWriter.Write(boneIndices[bw.boneIndex1]);
                binaryWriter.Write(boneIndices[bw.boneIndex2]);
                binaryWriter.Write(boneIndices[bw.boneIndex3]);
            }
        }
    }

    int GetTexturesCount(Material[] materials)
    {
        int nTextures = 0;
        for (int i = 0; i < materials.Length; i++)
        {
            if (materials[i].HasProperty("_MainTex"))
            {
                if (!FindTextureByName(m_pTextureNamesListForCounting, materials[i].GetTexture("_MainTex"))) nTextures++;
            }
            if (materials[i].HasProperty("_MetallicGlossMap"))
            {
                if (!FindTextureByName(m_pTextureNamesListForCounting, materials[i].GetTexture("_MetallicGlossMap"))) nTextures++;
            }
            if (materials[i].HasProperty("_BumpMap"))
            {
                if (!FindTextureByName(m_pTextureNamesListForCounting, materials[i].GetTexture("_BumpMap"))) nTextures++;
            }
            if (materials[i].HasProperty("_EmissionMap"))
            {
                if (!FindTextureByName(m_pTextureNamesListForCounting, materials[i].GetTexture("_EmissionMap"))) nTextures++;
            }
            if (materials[i].HasProperty("_OcclusionMap"))
            {
                if (!FindTextureByName(m_pTextureNamesListForCounting, materials[i].GetTexture("_OcclusionMap"))) nTextures++;
            }
        }
        return (nTextures);
    }

    int GetTexturesCount(Transform current)
    {
        int nTextures = 0;
        SkinnedMeshRenderer meshRenderer = current.gameObject.GetComponent<SkinnedMeshRenderer>();
        if (meshRenderer) nTextures = GetTexturesCount(meshRenderer.materials);

        for (int k = 0; k < current.childCount; k++) nTextures += GetTexturesCount(current.GetChild(k));

        return (nTextures);
    }

    void WriteMeshInfo(Mesh mesh, int[] boneIndices = null)
    {
        WriteObjectName(geometryWriter, "<Mesh:>", mesh.vertexCount, mesh);

        WriteBoundingBox(geometryWriter, "<Bounds:>", mesh.bounds);

        if ((mesh.vertices != null) && (mesh.vertices.Length > 0)) WriteVectors(geometryWriter, "<Positions:>", mesh.vertices);
        if ((mesh.colors != null) && (mesh.colors.Length > 0)) WriteColors(geometryWriter, "<Colors:>", mesh.colors);
        if ((mesh.uv != null) && (mesh.uv.Length > 0)) WriteTextureCoords(geometryWriter, "<TextureCoords0:>", mesh.uv);
        if ((mesh.uv2 != null) && (mesh.uv2.Length > 0)) WriteTextureCoords(geometryWriter, "<TextureCoords1:>", mesh.uv2);
        if ((mesh.normals != null) && (mesh.normals.Length > 0)) WriteVectors(geometryWriter, "<Normals:>", mesh.normals);
        if ((mesh.boneWeights != null) && (mesh.boneWeights.Length > 0))
        {
            WriteBoneWeights(geometryWriter, "<BoneWeights:>", mesh.boneWeights, boneIndices);
            WriteBoneIndices(geometryWriter, "<BoneIndices:>", mesh.boneWeights, boneIndices);
        }

        if ((mesh.normals != null && mesh.normals.Length > 0) && (mesh.tangents != null && mesh.tangents.Length > 0))
        {
            Vector3[] tangents = new Vector3[mesh.tangents.Length];
            Vector3[] biTangents = new Vector3[mesh.tangents.Length];
            for (int i = 0; i < mesh.tangents.Length; i++)
            {
                tangents[i] = new Vector3(mesh.tangents[i].x, mesh.tangents[i].y, mesh.tangents[i].z);
                biTangents[i] = Vector3.Normalize(Vector3.Cross(mesh.normals[i], tangents[i])) * mesh.tangents[i].w;
            }

            WriteVectors(geometryWriter, "<Tangents:>", tangents);
            WriteVectors(geometryWriter, "<BiTangents:>", biTangents);
        }

        WriteInteger(geometryWriter, "<Submeshes:>", mesh.subMeshCount);
        int indexCount = 0;
        for (int i = 0; i < mesh.subMeshCount; i++)
        {
            int[] subindicies = mesh.GetTriangles(i);
            indexCount += subindicies.Length;
        }
        geometryWriter.Write(indexCount);

        if (mesh.subMeshCount > 0)
        {
            if (indexCount < 65536)
            {
                for (int i = 0; i < mesh.subMeshCount; i++)
                {
                    int[] subindicies = mesh.GetTriangles(i);
                    WriteUInteger16s(geometryWriter, "<Submesh:>", i, subindicies);
                }
            }
            else
            {
                for (int i = 0; i < mesh.subMeshCount; i++)
                {
                    int[] subindicies = mesh.GetTriangles(i);
                    WriteIntegers(geometryWriter, "<Submesh:>", i, subindicies);
                }
            }
        }

        WriteString(geometryWriter, "</Mesh>");
    }

    void WriteMapRef(BinaryWriter binaryWriter, string header, string path)
    {
        WriteString(geometryWriter, header);
        WriteMapRef(geometryWriter, path);
    }

    void WriteMapRef(BinaryWriter binaryWriter, string path)
    {
        if (m_pTextureIndexInfo.TryGetValue(path, out var value))
        {
            // Item2 : resourceTypeIndex -> type
            binaryWriter.Write(value.Item2);
            // m_pTexturePath[] path to index -> resourceIndex
            int index = m_pTexturePath.IndexOf(path);
            if (index < 0)
            {
                Debug.LogWarning($"Path not found in m_pTexturePath: {path}");
            }
            binaryWriter.Write(index);
            // Item3 : arrayIndex -> arrayIdx
            binaryWriter.Write(value.Item3);
            // Item4 : colorSpace -> colorSpace
            binaryWriter.Write(value.Item4);
        }
        else
        {
            Debug.LogWarning($"Path not found in texture index info: {path}");
            Debug.Log($"Current texture index infos: {string.Join(", ", m_pTextureIndexInfo.Keys)}");
            binaryWriter.Write(0); // Default value for missing data
        }
    }

    void WriteMaterials(Material[] materials)
    {
        WriteInteger(geometryWriter, "<Materials:>", materials.Length);
        for (int i = 0; i < materials.Length; i++)
        {
            WriteInteger(geometryWriter, "<Material:>", i);

            // temporarily write default values
            WriteFloat(geometryWriter, "<AmbientOcclusion:>", 1.0f);

            if (materials[i].HasProperty("_Color"))
            {
                Color albedo = materials[i].GetColor("_Color");
                WriteColor(geometryWriter, "<AlbedoColor:>", albedo);
            }
            if (materials[i].HasProperty("_EmissionColor"))
            {
                Color emission = Color.black; // Default value for emission color when emission is not enabled

                if (materials[i].IsKeywordEnabled("_EMISSION"))
                {
                    emission = materials[i].GetColor("_EmissionColor");
                }

                WriteColor(geometryWriter, "<EmissiveColor:>", emission);
            }
            if (materials[i].HasProperty("_Smoothness"))
            {
                WriteFloat(geometryWriter, "<Smoothness:>", materials[i].GetFloat("_Smoothness"));
            }
            if (materials[i].HasProperty("_Metallic"))
            {
                WriteFloat(geometryWriter, "<Metallic:>", materials[i].GetFloat("_Metallic"));
            }
            if (materials[i].HasProperty("_MainTex"))
            {
                Texture mainAlbedoMap = materials[i].GetTexture("_MainTex");
                if (mainAlbedoMap != null)
                    WriteMapRef(geometryWriter, "<AlbedoMap:>", mainAlbedoMap.name);
            }
            if (materials[i].HasProperty("_BumpMap"))
            {
                Texture bumpMap = materials[i].GetTexture("_BumpMap");
                if (bumpMap != null)
                    WriteMapRef(geometryWriter, "<NormalMap:>", bumpMap.name);
            }
            if (materials[i].HasProperty("_MetallicGlossMap"))
            {
                Texture metallicMap = materials[i].GetTexture("_MetallicGlossMap");
                if (metallicMap != null)
                    WriteMapRef(geometryWriter, "<MetallicSmoothnessMap:>", metallicMap.name);
            }
            if (materials[i].HasProperty("_EmissionMap"))
            {
                Texture emissionMap = materials[i].GetTexture("_EmissionMap");
                if (emissionMap != null)
                    WriteMapRef(geometryWriter, "<EmissionMap:>", emissionMap.name);
            }
            if (materials[i].HasProperty("_OcclusionMap"))
            {
                Texture occlusionMap = materials[i].GetTexture("_OcclusionMap");
                if (occlusionMap != null)
                    WriteMapRef(geometryWriter, "<OcclusionMap:>", occlusionMap.name);
            }
            WriteString(geometryWriter, "</Material>");
        }
        WriteString(geometryWriter, "</Materials>");
    }

    void WriteFrameInfo(Transform current)
    {
        WriteLocalMatrix(geometryWriter, "<Xform:>", current);

        MeshRenderer meshRenderer = current.gameObject.GetComponent<MeshRenderer>();
        MeshFilter meshFilter = current.gameObject.GetComponent<MeshFilter>();

        if (meshRenderer && meshFilter)
        {
            if (meshRenderer.enabled == false) return;

            WriteMeshInfo(meshFilter.sharedMesh);

            Material[] materials = meshRenderer.materials;
            if (materials.Length > 0) WriteMaterials(materials);
            return;
        }

        SkinnedMeshRenderer skinnedMeshRenderer = current.gameObject.GetComponent<SkinnedMeshRenderer>();

        if (skinnedMeshRenderer && skinnedMeshRenderer.enabled == true)
        {
            int[] boneIndices = new int[skinnedMeshRenderer.bones.Length];
            for (int i = 0; i < skinnedMeshRenderer.bones.Length; i++)
            {
                bool found = false;
                for (int j = 0; j < bones.Count; j++)
                {
                    if (skinnedMeshRenderer.bones[i].gameObject.name == bones[j].gameObject.name)
                    {
                        found = true;
                        boneIndices[i] = j;
                        break;
                    }
                }

                if (!found)
                {
                    Debug.LogError("Bone not found: " + skinnedMeshRenderer.bones[i].gameObject.name);
                }
            }

            WriteMeshInfo(skinnedMeshRenderer.sharedMesh, boneIndices);

            Material[] materials = skinnedMeshRenderer.materials;
            if (materials.Length > 0) WriteMaterials(materials);
        }
    }

    void WriteFrameHierarchyInfo(Transform current)
    {
        WriteObjectName(geometryWriter, "<Node:>", current.gameObject);
        WriteFrameInfo(current);

        WriteInteger(geometryWriter, "<Children:>", current.childCount);

        if (current.childCount > 0)
        {
            for (int k = 0; k < current.childCount; k++)
            {
                WriteFrameHierarchyInfo(current.GetChild(k));
            }
        }
        WriteString(geometryWriter, "</Node>");
    }

    void WriteBoneHierarchyInfo(Transform current, ref int n)
    {
        WriteObjectName(skeletonWriter, "<Bone:>", current.gameObject);
        WriteInteger(skeletonWriter, "<BoneIndex:>", n);
        WriteLocalMatrix(skeletonWriter, "<Xform:>", current);
        WriteMatrix(skeletonWriter, "<BindPose:>", bindposes[n]);
        ++n;

        WriteInteger(skeletonWriter, "<Children:>", current.childCount);

        for (int k = 0; k < current.childCount; k++)
        {
            WriteBoneHierarchyInfo(current.GetChild(k), ref n);
        }
        WriteString(skeletonWriter, "</Bone>");
    }

    void processBoneHierarchy(Transform root, Transform current)
    {
        bones.Add(current);
        bindposes.Add(current.worldToLocalMatrix * root.localToWorldMatrix);
        // bindposes.Add(Matrix4x4.identity);  // Initialize bindpose with identity matrix

        for (int k = 0; k < current.childCount; k++)
        {
            processBoneHierarchy(root, current.GetChild(k));
        }
    }

    void updateBindPoses(Transform current)
    {
        SkinnedMeshRenderer skinnedMeshRenderer = current.gameObject.GetComponent<SkinnedMeshRenderer>();

        if (skinnedMeshRenderer && skinnedMeshRenderer.enabled == true)
        {
            int[] boneIndices = new int[skinnedMeshRenderer.bones.Length];
            for (int i = 0; i < skinnedMeshRenderer.bones.Length; i++)
            {
                for (int j = 0; j < bones.Count; j++)
                {
                    if (skinnedMeshRenderer.bones[i].gameObject.name == bones[j].gameObject.name)
                    {
                        boneIndices[i] = j;
                        break;
                    }
                }
            }

            Mesh mesh = skinnedMeshRenderer.sharedMesh;
            Debug.Log(current.gameObject.name);

            if (mesh != null && mesh.bindposes.Length > 0)
            {
                for (int i = 0; i < mesh.bindposes.Length; i++)
                {
                    if (mesh.bindposes[i] != Matrix4x4.identity)
                    {
                        bindposes[boneIndices[i]] = mesh.bindposes[i];
                    }
                }
            }
        }

        for (int i = 0; i < current.childCount; ++i)
        {
            updateBindPoses(current.GetChild(i));
        }
    }

    void processBones()
    {
        processBoneHierarchy(skeleton, skeleton);
        // updateBindPoses(geometry);
    }

    void ExtractGeometry()
    {
        WriteString(geometryWriter, "<Geometry:>");

        int nodeCnt = 0;
        accNodeCntRecursive(geometry, ref nodeCnt);
        WriteInteger(geometryWriter, "<NodeCnt:>", nodeCnt);

        WriteFrameHierarchyInfo(geometry);

        WriteString(geometryWriter, "</Geometry>");
    }

    void ExtractSkeleton()
    {
        WriteString(skeletonWriter, "<Skeleton:>");

        int nodeCnt = 0;
        accNodeCntRecursive(skeleton, ref nodeCnt);
        WriteInteger(skeletonWriter, "<BoneCnt:>", nodeCnt);

        int n = 0;
        WriteBoneHierarchyInfo(skeleton, ref n);

        WriteString(skeletonWriter, "</Skeleton>");
    }

    void ExtractKeyFrameClip(AnimationClip clip, BinaryWriter binaryWriter)
    {
        if (clip == null) return;

        int clipIdx = System.Array.IndexOf(animClips, clip);

        WriteString(binaryWriter, "<AnimationClip:>", clipExtractionNames[clipIdx]);

        WriteInteger(binaryWriter, "<BoneCnt:>", bones.Count);

        int fps = (int)clip.frameRate;
        int keyFrameCnt = Mathf.CeilToInt(clip.length * fps) + 1;
        WriteFloat(binaryWriter, "<Duration:>", (keyFrameCnt - 1) / (float)fps);
        WriteInteger(binaryWriter, "<KeyFrames:>", keyFrameCnt);

        Vector3[] lastPositions = new Vector3[bones.Count];
        Quaternion[] lastRotations = new Quaternion[bones.Count];
        Vector3[] lastScales = new Vector3[bones.Count];

        for (int i = 0; i < keyFrameCnt; ++i)
        {
            float time = i / (float)fps;
            clip.SampleAnimation(go, time);

            WriteString(binaryWriter, "<KeyFrame:>");
            if (i == keyFrameCnt - 1)
            {
                WriteFloat(binaryWriter, float.MaxValue);
            }
            else
            {
                WriteFloat(binaryWriter, time);
            }
            for (int j = 0; j < bones.Count; ++j)
            {
                Transform bone = bones[j];

                string name = bone.name;
                Vector3 pos = bone.localPosition;
                Quaternion rot = bone.localRotation;
                Vector3 scale = bone.localScale;

                bool changed = Vector3.Distance(pos, lastPositions[j]) > keyFramePosThresholds[clipIdx] ||
                               Quaternion.Angle(rot, lastRotations[j]) > keyFrameRotThresholds[clipIdx] ||
                               Vector3.Distance(scale, lastScales[j]) > keyFrameScaleThresholds[clipIdx];

                if (!(changed || i == 0 || i == keyFrameCnt - 1))
                {
                    continue;
                }

                WriteInteger(binaryWriter, "<BoneIdx:>", j);
                WriteVector(binaryWriter, pos);
                WriteVector(binaryWriter, rot);
                WriteVector(binaryWriter, scale);

                lastPositions[j] = pos;
                lastRotations[j] = rot;
                lastScales[j] = scale;
            }
            WriteString(binaryWriter, "</KeyFrame>");
        }

        WriteString(binaryWriter, "</AnimationClip>");
    }

    void ExtractKeyFrameAnimations(BinaryWriter binaryWriter)
    {
        if (go == null)
        {
            Debug.LogError("root game object is not assigned.");
            return;
        }

        if (skeleton == null)
        {
            Debug.LogError("Skeleton is not assigned.");
            return;
        }

        if (bones == null || bones.Count == 0)
        {
            Debug.LogError("No bones found in the skeleton.");
            return;
        }

        if (animClips == null || animClips.Length == 0)
        {
            Debug.LogWarning("No animation clips found.");
            return;
        }

        WriteString(binaryWriter, "<KeyFrameAnimationClips:>", animClips.Length);

        foreach (var clip in animClips)
        {
            ExtractKeyFrameClip(clip, binaryWriter);
        }

        WriteString(binaryWriter, "</KeyFrameAnimationClips>");
    }

    void ExtractPresampledClip(AnimationClip clip, BinaryWriter binaryWriter)
    {
        if (clip == null) return;

        int animIdx = System.Array.IndexOf(animClips, clip);

        WriteString(binaryWriter, "<AnimationClip:>", clipExtractionNames[animIdx]);

        WriteInteger(binaryWriter, "<BoneCnt:>", bones.Count);

        int fps = (int)clipPresampleFPSs[animIdx];
        int sampleCnt = Mathf.CeilToInt(clip.length * fps);
        WriteFloat(binaryWriter, "<Duration:>", (sampleCnt) / (float)fps);
        WriteInteger(binaryWriter, "<Samples:>", sampleCnt);

        for (int i = 0; i < sampleCnt; ++i)
        {
            clip.SampleAnimation(go, i / (float)fps);

            WriteString(binaryWriter, "<Sample:>");
            for (int j = 0; j < bones.Count; ++j)
            {
                Transform bone = bones[j];
                Matrix4x4 boneWorld = skeleton.worldToLocalMatrix * bone.localToWorldMatrix;
                WriteMatrix(binaryWriter, "<Xform:>", boneWorld);
            }
            WriteString(binaryWriter, "</Sample>");
        }

        WriteString(binaryWriter, "</AnimationClip>");
    }

    void ExtractPresampledAnimation(BinaryWriter binaryWriter)
    {
        if (go == null)
        {
            Debug.LogError("root game object is not assigned.");
            return;
        }

        if (skeleton == null)
        {
            Debug.LogError("Skeleton is not assigned.");
            return;
        }

        if (bones == null || bones.Count == 0)
        {
            Debug.LogError("No bones found in the skeleton.");
            return;
        }

        if (animClips == null || animClips.Length == 0)
        {
            Debug.LogWarning("No animation clips found.");
            return;
        }

        WriteString(binaryWriter, "<PresampledAnimationClips:>", animClips.Length);

        foreach (var clip in animClips)
        {
            ExtractPresampledClip(clip, binaryWriter);
        }

        WriteString(binaryWriter, "</PresampledAnimationClips>");
    }

    void Start()
    {
        if (geometry == null)
        {
            Debug.LogError("Geometry is not assigned.");
            return;
        }

        geometryWriter = new BinaryWriter(File.Open(string.Copy(transform.parent.gameObject.name).Replace(" ", "_") + ".bin", FileMode.Create));
        skeletonWriter = new BinaryWriter(File.Open(string.Copy(transform.parent.gameObject.name).Replace(" ", "_") + ".anim", FileMode.Create));
        bones = new List<Transform>();
        bindposes = new List<Matrix4x4>();

        ReadRemapFile("Remap.bin");
        ArrangeTextureList(geometry);
        CalculateResourceIndices();

        WriteDictionary(geometryWriter);


        if (skeleton != null) processBones();
        ExtractGeometry();
        if (skeleton != null) ExtractSkeleton();
        if (skeleton != null) ExtractKeyFrameAnimations(skeletonWriter);
        if (skeleton != null) ExtractPresampledAnimation(skeletonWriter);

        geometryWriter.Flush();
        geometryWriter.Close();
        skeletonWriter.Flush();
        skeletonWriter.Close();

        print("Model Binary Write Completed");
    }
}
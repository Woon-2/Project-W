//#define _WITH_SKINNED_BONES_ANIMATION

using System.Collections;
using System.Collections.Generic;
using UnityEngine;
using System.IO;
using UnityEditor;
using System.Text;

public class BinaryHierarchicalModelExtract : MonoBehaviour
{
    private List<string> m_pTextureNamesListForCounting = new List<string>();
    private List<string> m_pTextureNamesListForWriting = new List<string>();

    private List<string> m_pTexturePath = new List<string>();

    // path : path, resourceIndex, ArrayIndex
    private Dictionary<string, (string, uint, uint)> m_pTextureIndexInfo = new Dictionary<string, (string, uint, uint)>();

    private BinaryWriter binaryWriter = null;

    void WriteConvertMap()
    {

    }

    void WriteTextureInfo(Transform current)
    {
        SkinnedMeshRenderer meshRenderer = current.gameObject.GetComponent<SkinnedMeshRenderer>();

        if (meshRenderer)
        {
            Material[] materials = meshRenderer.materials;
            if (materials.Length > 0) {
                for (int i = 0; i < materials.Length; i++)
                {
                    if (materials[i].HasProperty("_MainTex"))
                    {
                        Texture mainAlbedoMap = materials[i].GetTexture("_MainTex");
                        WriteTextureName("<Item>", "</Item>", mainAlbedoMap);
                        if(mainAlbedoMap != null)
                            m_pTexturePath.Add(mainAlbedoMap.name);
                    }
                    if (materials[i].HasProperty("_SpecGlossMap"))
                    {
                        Texture specularcMap = materials[i].GetTexture("_SpecGlossMap");
                        WriteTextureName("<Item>", "</Item>", specularcMap);
                        if(specularcMap != null)
                            m_pTexturePath.Add(specularcMap.name);
                    }
                    if (materials[i].HasProperty("_MetallicGlossMap"))
                    {
                        Texture metallicMap = materials[i].GetTexture("_MetallicGlossMap");
                        WriteTextureName("<Item>", "</Item>", metallicMap);
                        if(metallicMap != null)
                            m_pTexturePath.Add(metallicMap.name);
                    }
                    if (materials[i].HasProperty("_BumpMap"))
                    {
                        Texture bumpMap = materials[i].GetTexture("_BumpMap");
                        WriteTextureName("<Item>", "</Item>", bumpMap);
                        if(bumpMap != null)
                            m_pTexturePath.Add(bumpMap.name);
                    }
                    if (materials[i].HasProperty("_EmissionMap"))
                    {
                        Texture emissionMap = materials[i].GetTexture("_EmissionMap");
                        WriteTextureName("<Item>", "</Item>", emissionMap);
                        if (emissionMap != null)
                            m_pTexturePath.Add(emissionMap.name);
                    }
                    if (materials[i].HasProperty("_DetailAlbedoMap"))
                    {
                        Texture detailAlbedoMap = materials[i].GetTexture("_DetailAlbedoMap");
                        WriteTextureName("<Item>", "</Item>", detailAlbedoMap);
                        if(detailAlbedoMap != null)
                            m_pTexturePath.Add(detailAlbedoMap.name);
                    }
                    if (materials[i].HasProperty("_DetailNormalMap"))
                    {
                        Texture detailNormalMap = materials[i].GetTexture("_DetailNormalMap");
                        WriteTextureName("<Item>", "</Item>", detailNormalMap);
                        if(detailNormalMap != null)
                            m_pTexturePath.Add(detailNormalMap.name);
                    }
                }
            }
        }
    }

    void StoreTexturePath(Texture texture)
{
    if (texture)
    {
        string texturePath = AssetDatabase.GetAssetPath(texture); // Unity 에디터에서 텍스처 경로를 가져옴
        if (!string.IsNullOrEmpty(texturePath) && !m_pTextureNamesListForWriting.Contains(texturePath))
        {
            m_pTextureNamesListForWriting.Add(texturePath);
        }
    }
}

    void WriteTextureMap(Transform child)
    {
        WriteTextureInfo(child);

        if (child.childCount > 0)
        {
            for (int k = 0; k < child.childCount; k++)
            {
                WriteTextureMap(child.GetChild(k));
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
                if (pTextureNamesList.Contains(strTextureName)) return(true);
            }
            pTextureNamesList.Add(strTextureName);
            return(false);
        }
        else
        {
            return(true);
        }
    }

    void WriteObjectName(Object obj)
    {
        binaryWriter.Write((obj) ? string.Copy(obj.name).Replace(" ", "_") : "null");
    }

    void WriteObjectName(int i, Object obj)
    {
        binaryWriter.Write(i);
        binaryWriter.Write((obj) ? string.Copy(obj.name).Replace(" ", "_") : "null");
    }

    void WriteObjectName(string strHeader, Object obj)
    {
        binaryWriter.Write(strHeader);
        binaryWriter.Write((obj) ? string.Copy(obj.name).Replace(" ", "_") : "null");
    }

    void WriteObjectName(string strHeader, int i, Object obj)
    {
        binaryWriter.Write(strHeader);
        binaryWriter.Write(i);
        binaryWriter.Write((obj) ? string.Copy(obj.name).Replace(" ", "_") : "null");
    }

    void WriteObjectName(string strHeader, int i, int j, Object obj)
    {
        binaryWriter.Write(strHeader);
        binaryWriter.Write(i);
        binaryWriter.Write(j);
        binaryWriter.Write((obj) ? string.Copy(obj.name).Replace(" ", "_") : "null");
    }

    void WriteObjectName(string strHeader, int i, Object obj, float f, int j, int k)
    {
        binaryWriter.Write(strHeader);
        binaryWriter.Write(i);
        binaryWriter.Write((obj) ? string.Copy(obj.name).Replace(" ", "_") : "null");
        binaryWriter.Write(f);
        binaryWriter.Write(j);
        binaryWriter.Write(k);
    }

    void WriteString(string strToWrite)
    {
        binaryWriter.Write(strToWrite);
    }

    void WriteString(string strHeader, string strToWrite)
    {
        binaryWriter.Write(strHeader);
        binaryWriter.Write(strToWrite);
    }

    void WriteString(string strToWrite, int i)
    {
        binaryWriter.Write(strToWrite);
        binaryWriter.Write(i);
    }

    void WriteString(string strToWrite, int i, float f)
    {
        binaryWriter.Write(strToWrite);
        binaryWriter.Write(i);
        binaryWriter.Write(f);
    }

    void WriteTextureName(string strHeader, string strFooter, Texture texture)
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

    void WriteInteger(int i)
    {
        binaryWriter.Write(i);
    }

    void WriteInteger(string strHeader, int i)
    {
        binaryWriter.Write(strHeader);
        binaryWriter.Write(i);
    }

    void WriteFloat(string strHeader, float f)
    {
        binaryWriter.Write(strHeader);
        binaryWriter.Write(f);
    }

    void WriteVector(Vector2 v)
    {
        binaryWriter.Write(v.x);
        binaryWriter.Write(v.y);
    }

    void WriteVector(string strHeader, Vector2 v)
    {
        binaryWriter.Write(strHeader);
        WriteVector(v);
    }

    void WriteVector(Vector3 v)
    {
        binaryWriter.Write(v.x);
        binaryWriter.Write(v.y);
        binaryWriter.Write(v.z);
    }

    void WriteVector(string strHeader, Vector3 v)
    {
        binaryWriter.Write(strHeader);
        WriteVector(v);
    }

    void WriteVector(Vector4 v)
    {
        binaryWriter.Write(v.x);
        binaryWriter.Write(v.y);
        binaryWriter.Write(v.z);
        binaryWriter.Write(v.w);
    }

    void WriteVector(string strHeader, Vector4 v)
    {
        binaryWriter.Write(strHeader);
        WriteVector(v);
    }

    void WriteVector(Quaternion q)
    {
        binaryWriter.Write(q.x);
        binaryWriter.Write(q.y);
        binaryWriter.Write(q.z);
        binaryWriter.Write(q.w);
    }

    void WriteVector(string strHeader, Quaternion q)
    {
        binaryWriter.Write(strHeader);
        WriteVector(q);
    }

    void WriteColor(Color c)
    {
        binaryWriter.Write(c.r);
        binaryWriter.Write(c.g);
        binaryWriter.Write(c.b);
        binaryWriter.Write(c.a);
    }

    void WriteColor(string strHeader, Color c)
    {
        binaryWriter.Write(strHeader);
        WriteColor(c);
    }

    void WriteTextureCoord(Vector2 uv)
    {
        binaryWriter.Write(uv.x);
        binaryWriter.Write(1.0f - uv.y);
    }

    void WriteVectors(string strHeader, Vector2[] vectors)
    {
        binaryWriter.Write(strHeader);
        binaryWriter.Write(vectors.Length);
        if (vectors.Length > 0) foreach (Vector2 v in vectors) WriteVector(v);
    }

    void WriteVectors(string strHeader, Vector3[] vectors)
    {
        binaryWriter.Write(strHeader);
        binaryWriter.Write(vectors.Length);
        if (vectors.Length > 0) foreach (Vector3 v in vectors) WriteVector(v);
    }

    void WriteVectors(string strHeader, Vector4[] vectors)
    {
        binaryWriter.Write(strHeader);
        binaryWriter.Write(vectors.Length);
        if (vectors.Length > 0) foreach (Vector4 v in vectors) WriteVector(v); 
    }

    void WriteColors(string strHeader, Color[] colors)
    {
        binaryWriter.Write(strHeader);
        binaryWriter.Write(colors.Length);
        if (colors.Length > 0) foreach (Color c in colors) WriteColor(c);
    }

    void WriteTextureCoords(string strHeader, Vector2[] uvs)
    {
        binaryWriter.Write(strHeader);
        binaryWriter.Write(uvs.Length);
        if (uvs.Length > 0) foreach (Vector2 uv in uvs) WriteTextureCoord(uv);
    }

    void WriteIntegers(int[] pIntegers)
    {
        binaryWriter.Write(pIntegers.Length);
        foreach (int i in pIntegers) binaryWriter.Write(i);
    }

    void WriteIntegers(string strHeader, int[] pIntegers)
    {
        binaryWriter.Write(strHeader);
        binaryWriter.Write(pIntegers.Length);
        if (pIntegers.Length > 0) foreach (int i in pIntegers) binaryWriter.Write(i);
    }

    void WriteIntegers(string strHeader, int n, int[] pIntegers)
    {
        binaryWriter.Write(strHeader);
        binaryWriter.Write(n);
        binaryWriter.Write(pIntegers.Length);
        if (pIntegers.Length > 0) foreach (int i in pIntegers) binaryWriter.Write(i);
    }

    void WriteBoundingBox(string strHeader, Bounds bounds)
    {
        binaryWriter.Write(strHeader);
        WriteVector(bounds.center);
        WriteVector(bounds.extents);
    }

    void WriteMatrix(Matrix4x4 matrix)
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

    void WriteMatrix(Vector3 position, Quaternion rotation, Vector3 scale)
    {
        Matrix4x4 matrix = Matrix4x4.identity;
        matrix.SetTRS(position, rotation, scale);
        WriteMatrix(matrix);
    }

    void WriteTransform(string strHeader, Transform current)
    {
        binaryWriter.Write(strHeader);
        WriteVector(current.localPosition);
        WriteVector(current.localEulerAngles);
        WriteVector(current.localScale);
        WriteVector(current.localRotation);
    }

    void WriteLocalMatrix(string strHeader, Transform current)
    {
        binaryWriter.Write(strHeader);
        Matrix4x4 matrix = Matrix4x4.identity;
        matrix.SetTRS(current.localPosition, current.localRotation, current.localScale);
        WriteMatrix(matrix);
    }

    void WriteWorldMatrix(string strHeader, Transform current)
    {
        binaryWriter.Write(strHeader);
        Matrix4x4 matrix = Matrix4x4.identity;
        matrix.SetTRS(current.position, current.rotation, current.lossyScale);
        WriteMatrix(matrix);
    }

        void WriteMatrixes(string strHeader, Matrix4x4[] matrixes)
    {
        WriteString(strHeader, matrixes.Length);
        if (matrixes.Length > 0)
        {
            foreach (Matrix4x4 matrix in matrixes) WriteMatrix(matrix);
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
            if (materials[i].HasProperty("_SpecGlossMap"))
            {
                if (!FindTextureByName(m_pTextureNamesListForCounting, materials[i].GetTexture("_SpecGlossMap"))) nTextures++;
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
            if (materials[i].HasProperty("_DetailAlbedoMap"))
            {
                if (!FindTextureByName(m_pTextureNamesListForCounting, materials[i].GetTexture("_DetailAlbedoMap"))) nTextures++;
            }
            if (materials[i].HasProperty("_DetailNormalMap"))
            {
                if (!FindTextureByName(m_pTextureNamesListForCounting, materials[i].GetTexture("_DetailNormalMap"))) nTextures++;
            }
        }
        return(nTextures);
    }

    int GetTexturesCount(Transform current)
    {
        int nTextures = 0;
        SkinnedMeshRenderer meshRenderer = current.gameObject.GetComponent<SkinnedMeshRenderer>();
        if (meshRenderer) nTextures = GetTexturesCount(meshRenderer.materials);

        for (int k = 0; k < current.childCount; k++) nTextures += GetTexturesCount(current.GetChild(k));

        return(nTextures);
    }

    void WriteMeshInfo(Mesh mesh)
    {
        WriteObjectName("<Mesh>:", mesh.vertexCount, mesh);

        WriteBoundingBox("<Bounds>:", mesh.bounds);

        if ((mesh.vertices != null) && (mesh.vertices.Length > 0)) WriteVectors("<Positions>:", mesh.vertices);
        if ((mesh.colors != null) && (mesh.colors.Length > 0)) WriteColors("<Colors>:", mesh.colors);
        if ((mesh.uv != null) && (mesh.uv.Length > 0)) WriteTextureCoords("<TextureCoords0>:", mesh.uv);
        if ((mesh.uv2 != null) && (mesh.uv2.Length > 0)) WriteTextureCoords("<TextureCoords1>:", mesh.uv2);
        if ((mesh.normals != null) && (mesh.normals.Length > 0)) WriteVectors("<Normals>:", mesh.normals);

        if ((mesh.normals.Length > 0) && (mesh.tangents.Length > 0))
        {
            Vector3[] tangents = new Vector3[mesh.tangents.Length];
            Vector3[] biTangents = new Vector3[mesh.tangents.Length];
            for (int i = 0; i < mesh.tangents.Length; i++)
            {
                tangents[i] = new Vector3(mesh.tangents[i].x, mesh.tangents[i].y, mesh.tangents[i].z);
                biTangents[i] = Vector3.Normalize(Vector3.Cross(mesh.normals[i], tangents[i])) * mesh.tangents[i].w;
            }

            WriteVectors("<Tangents>:", tangents);
            WriteVectors("<BiTangents>:", biTangents);
        }

        WriteInteger("<SubMeshes>:", mesh.subMeshCount);
        if (mesh.subMeshCount > 0)
        {
            for (int i = 0; i < mesh.subMeshCount; i++)
            {
                int[] subindicies = mesh.GetTriangles(i);
                WriteIntegers("<SubMesh>:", i, subindicies);
            }
        }

        WriteString("</Mesh>");
    }

    void WriteMapRef(string path) 
    {
        if( m_pTextureIndexInfo.TryGetValue(path, out var value) )
        {
            // Item2 : resourceTypeIndex -> type
            binaryWriter.Write(value.Item2);
            // m_pTexturePath[] path to index -> resourceIndex
            int index = m_pTexturePath.IndexOf(path);
            if (index >= 0)
            {
                binaryWriter.Write((uint)index);
            }
            // Item3 : arrayIndex -> arrayIdx
            binaryWriter.Write(value.Item3);
            // padding
            binaryWriter.Write(0);
        }
        else
        {
            Debug.LogWarning($"Path not found in texture index info: {path}");
            binaryWriter.Write(0); // Default value for missing data
        }
    }

    void WriteMaterials(Material[] materials)
    {
        WriteInteger("<Materials>:", materials.Length);
        for (int i = 0; i < materials.Length; i++)
        {
            WriteInteger("<Material>:", i);

            if (materials[i].HasProperty("_Color"))
            {
                Color albedo = materials[i].GetColor("_Color");
                WriteColor("<AlbedoColor>:", albedo);
            }
            if (materials[i].HasProperty("_EmissionColor"))
            {
                Color emission = materials[i].GetColor("_EmissionColor");
                WriteColor("<EmissiveColor>:", emission);
            }
            if (materials[i].HasProperty("_SpecColor"))
            {
                Color specular = materials[i].GetColor("_SpecColor");
                WriteColor("<SpecularColor>:", specular);
            }
            if (materials[i].HasProperty("_Glossiness"))
            {
                WriteFloat("<Glossiness>:", materials[i].GetFloat("_Glossiness"));
            }
            if (materials[i].HasProperty("_Smoothness"))
            {
                WriteFloat("<Smoothness>:", materials[i].GetFloat("_Smoothness"));
            }
            if (materials[i].HasProperty("_Metallic"))
            {
                WriteFloat("<Metallic>:", materials[i].GetFloat("_Metallic"));
            }
            if (materials[i].HasProperty("_SpecularHighlights"))
            {
                WriteFloat("<SpecularHighlight>:", materials[i].GetFloat("_SpecularHighlights"));
            }
            if (materials[i].HasProperty("_GlossyReflections"))
            {
                WriteFloat("<GlossyReflection>:", materials[i].GetFloat("_GlossyReflections"));
            }

            if (materials[i].HasProperty("_MainTex"))
            {
                Texture mainAlbedoMap = materials[i].GetTexture("_MainTex");
                WriteTextureName("<AlbedoMap>:", "</AlbedoMap>", mainAlbedoMap);
                if (mainAlbedoMap != null) 
                    WriteMapRef(mainAlbedoMap.name);
            }
            if (materials[i].HasProperty("_SpecGlossMap"))
            {
                Texture specularcMap = materials[i].GetTexture("_SpecGlossMap");
                WriteTextureName("<SpecularMap>:", "</SpecularMap>", specularcMap);
                if (specularcMap != null)
                    WriteMapRef(specularcMap.name);
            }
            if (materials[i].HasProperty("_MetallicGlossMap"))
            {
                Texture metallicMap = materials[i].GetTexture("_MetallicGlossMap");
                WriteTextureName("<MetallicMap>:", "</MetallicMap>", metallicMap);
                if (metallicMap != null)
                    WriteMapRef(metallicMap.name);
            }
            if (materials[i].HasProperty("_BumpMap"))
            {
                Texture bumpMap = materials[i].GetTexture("_BumpMap");
                WriteTextureName("<NormalMap>:", "</NormalMap>", bumpMap);
                if (bumpMap != null)
                    WriteMapRef(bumpMap.name);
            }
            if (materials[i].HasProperty("_EmissionMap"))
            {
                Texture emissionMap = materials[i].GetTexture("_EmissionMap");
                WriteTextureName("<EmissionMap>:", "</EmissionMap>", emissionMap);
                if(emissionMap != null) 
                    WriteMapRef(emissionMap.name);
            }
            if (materials[i].HasProperty("_DetailAlbedoMap"))
            {
                Texture detailAlbedoMap = materials[i].GetTexture("_DetailAlbedoMap");
                WriteTextureName("<DetailAlbedoMap>:", "</DetailAlbedoMap>", detailAlbedoMap);
                if (detailAlbedoMap != null)
                    WriteMapRef(detailAlbedoMap.name);
            }
            if (materials[i].HasProperty("_DetailNormalMap"))
            {
                Texture detailNormalMap = materials[i].GetTexture("_DetailNormalMap");
                WriteTextureName("<DetailNormalMap>:", "</DetailNormalMap>", detailNormalMap);
                if (detailNormalMap != null)
                    WriteMapRef(detailNormalMap.name);
            }
        }
        WriteString("</Materials>");
    }

    void WriteFrameInfo(Transform current)
    {
        WriteObjectName("<Node>:", current.gameObject);

        WriteLocalMatrix("<Xform>:", current);
        WriteString("</Xform>");

        MeshRenderer meshRenderer = current.gameObject.GetComponent<MeshRenderer>();

        if (meshRenderer)
        {
            WriteMeshInfo(meshRenderer.sharedMesh);

            Material[] materials = meshRenderer.materials;
            if (materials.Length > 0) WriteMaterials(materials);

            return;
        }

        SkinnedMeshRenderer skinnedMeshRenderer = current.gameObject.GetComponent<SkinnedMeshRenderer>();

        if (skinnedMeshRenderer)
        {
            WriteMeshInfo(skinnedMeshRenderer.sharedMesh);

            Material[] materials = skinnedMeshRenderer.materials;
            if (materials.Length > 0) WriteMaterials(materials);
        }
    }

    void WriteFrameHierarchyInfo(Transform child)
    {
        WriteFrameInfo(child);

        WriteInteger("<Children>:", child.childCount);

        if (child.childCount > 0)
        {
            for (int k = 0; k < child.childCount; k++)
            {
                WriteFrameHierarchyInfo(child.GetChild(k));
            }
        }

        WriteString("</Node>"); 
    }

    void Start()
    {
        binaryWriter = new BinaryWriter(File.Open(string.Copy(gameObject.name).Replace(" ", "_") + ".bin", FileMode.Create));

        WriteConvertMap();
        WriteString("<TexturePath>");
        WriteTextureMap(transform);
        WriteString("</TexturePath>");

        WriteFrameHierarchyInfo(transform);

        binaryWriter.Flush();
        binaryWriter.Close();

        print("Model Binary Write Completed");
    }
}

// (path, ResourceTypeIndex, ArrayIndex)

// (type, resourceIdx, arrayIdx, padding)

// ResourceTypeIndex -> type
// ArrayIndex -> arrayIdx

// path의 TexturePath에서의 인덱스를 찾아냄: i -> resourceIdx
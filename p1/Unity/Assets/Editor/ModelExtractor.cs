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
    private GameObject targetSkeleton;
    private Dictionary<Texture, string> textureMappings = new Dictionary<Texture, string>();
    private Vector2 scrollPos;
    private string targetObjectName = "";
    private string targetSkeletonName = "";

    private BinaryWriter geometryWriter = null;
    private BinaryWriter skeletonWriter = null;

    private List<Transform> bones = null;
    private List<Matrix4x4> bindposes = null;

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

        targetObjectName = (string)EditorGUILayout.TextField("Object Name: ", targetObjectName);
        EditorGUILayout.Space();

        targetSkeleton = (GameObject)EditorGUILayout.ObjectField("Target Skeleton(optional)", targetSkeleton, typeof(GameObject), true);
        EditorGUILayout.Space();

        targetSkeletonName = (string)EditorGUILayout.TextField("Skeleton Name(optional): ", targetSkeletonName);
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
                        if (tex != null && !textureMappings.ContainsKey(tex))
                        {
                            textureMappings[tex] = AssetDatabase.GetAssetPath(tex);
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

    void ExtractTextureMapping()
    {
        ExtractUtil.WriteHeadTag(geometryWriter, "TextureMapping");

        foreach (var kvp in textureMappings)
        {
            ExtractUtil.WriteHeadTag(geometryWriter, "Item");
            Texture tex = kvp.Key;
            ExtractUtil.WriteText(geometryWriter, "TextureName", tex.name);
            ExtractUtil.WriteText(geometryWriter, "WrapModeU", tex.wrapModeU.ToString());
            ExtractUtil.WriteText(geometryWriter, "WrapModeV", tex.wrapModeV.ToString());
            ExtractUtil.WriteText(geometryWriter, "WrapModeW", tex.wrapModeW.ToString());
            if (tex.anisoLevel > 1)
            {
                ExtractUtil.WriteText(geometryWriter, "FilterMode", "Anisotropic");
            }
            else
            {
                ExtractUtil.WriteText(geometryWriter, "FilterMode", tex.filterMode.ToString());
            }
            ExtractUtil.WriteInteger(geometryWriter, "AnisoLevel", tex.anisoLevel);
            ExtractUtil.WriteText(geometryWriter, "Path", kvp.Value);
            ExtractUtil.WriteTailTag(geometryWriter, "Item");
        }

        ExtractUtil.WriteTailTag(geometryWriter, "TextureMapping");
    }

    void ExtractMesh(Mesh mesh)
    {
        ExtractUtil.WriteHeadTag(geometryWriter, "Mesh");

        // 버텍스 버퍼들 추출
        ExtractUtil.WriteHeadTag(geometryWriter, "VertexBuffers");
        if ((mesh.vertices != null) && (mesh.vertices.Length > 0)) ExtractUtil.WriteVectors(geometryWriter, "Positions", mesh.vertices);
        if ((mesh.colors != null) && (mesh.colors.Length > 0)) ExtractUtil.WriteColors(geometryWriter, "Colors", mesh.colors);
        if ((mesh.normals != null) && (mesh.normals.Length > 0)) ExtractUtil.WriteVectors(geometryWriter, "Normals", mesh.normals);
        if ( (mesh.normals != null) && (mesh.normals.Length > 0)
            && (mesh.tangents != null) && (mesh.tangents.Length > 0)
        )
        {
            Vector3[] tangents = new Vector3[mesh.tangents.Length];
            Vector3[] bitangents = new Vector3[mesh.tangents.Length];
            for (int i = 0; i < mesh.tangents.Length; i++)
            {
                tangents[i] = new Vector3(mesh.tangents[i].x, mesh.tangents[i].y, mesh.tangents[i].z);
                bitangents[i] = Vector3.Normalize(Vector3.Cross(mesh.normals[i], tangents[i])) * mesh.tangents[i].w;
            }
            ExtractUtil.WriteVectors(geometryWriter, "Tangents", tangents);
            ExtractUtil.WriteVectors(geometryWriter, "Bitangents", bitangents);
        }
        if ((mesh.uv != null) && (mesh.uv.Length > 0)) ExtractUtil.WriteVectors(geometryWriter, "TextureCoords0", mesh.uv);
        if ((mesh.uv2 != null) && (mesh.uv2.Length > 0)) ExtractUtil.WriteVectors(geometryWriter, "TextureCoords1", mesh.uv2);
        ExtractUtil.WriteTailTag(geometryWriter, "VertexBuffers");

        // 서브메시(인덱스 버퍼) 추출
        // 최고 인덱스가 65536 미만일 경우 16비트 부호 없는 정수형으로 ExtractUtil.Write,
        // 그렇지 않을 경우 32비트 부호 없는 정수형으로 ExtractUtil.Write한다.
        ExtractUtil.WriteHeadTag(geometryWriter, "Submeshes");

        ExtractUtil.WriteInteger(geometryWriter, "SubmeshCnt", mesh.subMeshCount);
        int maxIdx = 0;
        for (int i = 0; i < mesh.subMeshCount; i++)
        {
            int[] subIndices = mesh.GetTriangles(i);
            foreach (int subIdx in subIndices)
            {
                maxIdx = Math.Max(subIdx, maxIdx);
            }
        }
        ExtractUtil.WriteInteger(geometryWriter, "MaxIndex", maxIdx);

        if (maxIdx < 65536)
        {
            for (int i = 0; i < mesh.subMeshCount; i++)
            {
                int[] subIndices = mesh.GetTriangles(i);
                ExtractUtil.WriteIntegerAsU16s(geometryWriter, "Submesh", subIndices);
            }
        }
        else
        {
            for (int i = 0; i < mesh.subMeshCount; i++)
            {
                int[] subIndices = mesh.GetTriangles(i);
                ExtractUtil.WriteIntegers(geometryWriter, "Submesh", subIndices);
            }
        }

        ExtractUtil.WriteTailTag(geometryWriter, "Submeshes");

        ExtractUtil.WriteTailTag(geometryWriter, "Mesh");
    }

    void ExtractMaterials(Material[] materials)
    {
        ExtractUtil.WriteHeadTag(geometryWriter, "Materials");

        ExtractUtil.WriteInteger(geometryWriter, "MaterialCnt", materials.Length);

        for (int i = 0; i < materials.Length; i++)
        {
            ExtractUtil.WriteHeadTag(geometryWriter, "Material");

            // 상수들 추출
            // cAlbedo
            if (materials[i].HasProperty("_Color"))
            {
                Color albedo = materials[i].GetColor("_Color");
                ExtractUtil.WriteColor(geometryWriter, "cAlbedo", albedo);
            }
            // cEmmisive
            if (materials[i].HasProperty("_EmissionColor"))
            {
                Color emission = Color.black; // Default value for emission color when emission is not enabled

                if (materials[i].IsKeywordEnabled("_EMISSION"))
                {
                    emission = materials[i].GetColor("_EmissionColor");
                }

                ExtractUtil.WriteColor(geometryWriter, "cEmmisive", emission);
            }
            // cSmoothness
            if (materials[i].HasProperty("_Smoothness"))
            {
                ExtractUtil.WriteFloat(geometryWriter, "cSmoothness", materials[i].GetFloat("_Smoothness"));
            }
            else if (materials[i].HasProperty("_Glossiness"))
            {
                ExtractUtil.WriteFloat(geometryWriter, "cSmoothness", materials[i].GetFloat("_Glossiness"));
            }
            // cMetallic
            if (materials[i].HasProperty("_Metallic"))
            {
                ExtractUtil.WriteFloat(geometryWriter, "cMetallic", materials[i].GetFloat("_Metallic"));
            }
            // cAOStrength
            if (materials[i].HasProperty("_OcclusionStrength"))
            {
                ExtractUtil.WriteFloat(geometryWriter, "cAOStrength", materials[i].GetFloat("_OcclusionStrength"));
            }

            // 텍스처들 추출
            // AlbedoMap
            if (materials[i].HasProperty("_MainTex"))
            {
                Texture mainAlbedoMap = materials[i].GetTexture("_MainTex");
                if (mainAlbedoMap != null)
                {
                    ExtractUtil.WriteText(geometryWriter, "AlbedoMap", mainAlbedoMap.name);
                }
            }
            // NormalMap
            if (materials[i].HasProperty("_BumpMap"))
            {
                Texture bumpMap = materials[i].GetTexture("_BumpMap");
                if (bumpMap != null)
                {
                    ExtractUtil.WriteText(geometryWriter, "NormalMap", bumpMap.name);
                }
            }
            // MetallicSmoothnessMap
            if (materials[i].HasProperty("_MetallicGlossMap"))
            {
                Texture metallicGlossMap = materials[i].GetTexture("_MetallicGlossMap");
                if (metallicGlossMap != null)
                {
                    ExtractUtil.WriteText(geometryWriter, "MetallicSmoothnessMap", metallicGlossMap.name);
                }
            }
            // EmmisiveMap
            if (materials[i].HasProperty("_EmissionMap"))
            {
                Texture emmisionMap = materials[i].GetTexture("_EmissionMap");
                if (emmisionMap != null)
                {
                    ExtractUtil.WriteText(geometryWriter, "EmmisiveMap", emmisionMap.name);
                }
            }
            // AOMap
            if (materials[i].HasProperty("_OcclusionMap"))
            {
                Texture aoMap = materials[i].GetTexture("_OcclusionMap");
                if (aoMap != null)
                {
                    ExtractUtil.WriteText(geometryWriter, "AOMap", aoMap.name);
                }
            }


            ExtractUtil.WriteTailTag(geometryWriter, "Material");
        }

        ExtractUtil.WriteTailTag(geometryWriter, "Materials");
    }

    void ExtractTransform(Transform root, Transform xform)
    {
        ExtractUtil.WriteHeadTag(geometryWriter, "Node");
        ExtractUtil.WriteString(geometryWriter, ExtractUtil.ReplaceWhiteSpace(xform.gameObject.name));

        // 변환 추출
        ExtractUtil.WriteLocalMatrix(geometryWriter, "LocalMatrix", xform);
        ExtractUtil.WriteDressMatrix(geometryWriter, "DressMatrix", root, xform);

        // 메시 추출
        MeshRenderer meshRenderer = xform.gameObject.GetComponent<MeshRenderer>();
        MeshFilter meshFilter = xform.gameObject.GetComponent<MeshFilter>();
        SkinnedMeshRenderer skinnedMeshRenderer = xform.gameObject.GetComponent<SkinnedMeshRenderer>();

        Mesh mesh = null;
        if (meshRenderer && meshFilter && meshRenderer.enabled)
        {
            mesh = meshFilter.sharedMesh;
        }
        else if (skinnedMeshRenderer && skinnedMeshRenderer.enabled)
        {
            mesh = skinnedMeshRenderer.sharedMesh;
        }

        if (mesh != null)
        {
            ExtractMesh(mesh);
        }

        // --- MaterialSetSelector 지원 ---
        MaterialSetSelector selector = xform.GetComponent<MaterialSetSelector>();
        if (selector != null && selector.materialSets != null && selector.materialSets.Count > 0)
        {
            ExtractUtil.WriteHeadTag(geometryWriter, "MaterialSets");
            ExtractUtil.WriteInteger(geometryWriter, "MaterialSetCnt", selector.materialSets.Count);

            foreach (var set in selector.materialSets)
            {
                ExtractUtil.WriteHeadTag(geometryWriter, "MaterialSet");
                ExtractUtil.WriteText(geometryWriter, "Name", set.name);

                if (set.materials != null && set.materials.Length > 0)
                    ExtractMaterials(set.materials);

                ExtractUtil.WriteTailTag(geometryWriter, "MaterialSet");
            }

            ExtractUtil.WriteTailTag(geometryWriter, "MaterialSets");
        }
        else
        {
            // MaterialSetSelector가 없다면 기본으로 메시에 적용된 재질들 추출
            Material[] materials = null;
            if (meshRenderer && meshRenderer.enabled)
            {
                materials = meshRenderer.sharedMaterials;
            }
            else if (skinnedMeshRenderer && skinnedMeshRenderer.enabled)
            {
                materials = skinnedMeshRenderer.sharedMaterials;
            }

            if (materials != null && materials.Length > 0)
            {
                ExtractMaterials(materials);
            }
        }

        // 자식 노드들 추출
        ExtractUtil.WriteInteger(geometryWriter, "ChildCnt", xform.childCount);
        ExtractUtil.WriteHeadTag(geometryWriter, "Children");

        if (xform.childCount > 0)
        {
            for (int k = 0; k < xform.childCount; k++)
            {
                ExtractTransform(root, xform.GetChild(k));
            }
        }

        ExtractUtil.WriteTailTag(geometryWriter, "Children");
        ExtractUtil.WriteTailTag(geometryWriter, "Node");
    }

    void ExtractGeometry()
    {
        ExtractUtil.WriteHeadTag(geometryWriter, "Geometry");
        int nodeCnt = 0;
        ExtractUtil.AccNodeCnt(targetObject.transform, ref nodeCnt);
        ExtractUtil.WriteInteger(geometryWriter, "NodeCnt", nodeCnt);

        ExtractTransform(targetObject.transform, targetObject.transform);

        ExtractUtil.WriteTailTag(geometryWriter, "Geometry");
    }

    // 바운딩 볼륨들 추출
    // 후에 바운딩 볼륨의 계층구조 추출이 필요할 것이다.
    void ExtractBoundingVolumes()
    {
        // BoundingVolume 컴포넌트 찾기
        BoundingVolume bvComponent = targetObject.GetComponent<BoundingVolume>();
        if (bvComponent == null || bvComponent.boundingVolumes == null || bvComponent.boundingVolumes.Count == 0)
        {
            ExtractUtil.WriteHeadTag(geometryWriter, "BoundingVolumes");
            ExtractUtil.WriteInteger(geometryWriter, "Count", 0);
            ExtractUtil.WriteTailTag(geometryWriter, "BoundingVolumes");
            return;
        }

        ExtractUtil.WriteHeadTag(geometryWriter, "BoundingVolumes");
        ExtractUtil.WriteInteger(geometryWriter, "Count", bvComponent.boundingVolumes.Count);

        foreach (var bv in bvComponent.boundingVolumes)
        {
            if (bv == null || bv.BVBox == null)
                continue;

            ExtractUtil.WriteHeadTag(geometryWriter, "BoundingVolume");

            // 이름
            ExtractUtil.WriteText(geometryWriter, "Name", bv.BVName);

            BoxCollider box = bv.BVBox;
            Transform t = box.transform;

            // Transform 정보 저장 (로컬 기준)
            ExtractUtil.WriteLocalMatrix(geometryWriter, "LocalMatrix", t);
            ExtractUtil.WriteLocalTRS(geometryWriter, "LocalTRS", t);

            // BoxCollider 고유 값들 저장
            ExtractUtil.WriteVector(geometryWriter, "Center", box.center);
            ExtractUtil.WriteVector(geometryWriter, "Size", box.size);

            ExtractUtil.WriteTailTag(geometryWriter, "BoundingVolume");
        }

        ExtractUtil.WriteTailTag(geometryWriter, "BoundingVolumes");
    }

    void ProcessBoneHierarchy(Transform root, Transform bone)
    {
        bones.Add(bone);
        bindposes.Add(bone.worldToLocalMatrix * root.localToWorldMatrix);

        for (int i = 0; i < bone.childCount; ++i)
        {
            ProcessBoneHierarchy(root, bone.GetChild(i));
        }
    }

    void ProcessBones()
    {
        bones = new List<Transform>();
        ProcessBoneHierarchy(targetSkeleton.transform, targetSkeleton.transform);
    }

    void ExtractBoneHierarchy(Transform bone, ref int boneIdx)
    {
        ExtractUtil.WriteHeadTag(skeletonWriter, "Bone");
        ExtractUtil.WriteText(skeletonWriter, "Name", bone.gameObject.name);
        ExtractUtil.WriteLocalMatrix(skeletonWriter, "ToParent", bone);
        ExtractUtil.WriteMatrix(skeletonWriter, "ToLocal", bindposes[boneIdx]);

        ++boneIdx;

        ExtractUtil.WriteHeadTag(skeletonWriter, "Children");
        ExtractUtil.WriteInteger(skeletonWriter, "ChildCnt", bone.childCount);

        for (int i = 0; i < bone.childCount; ++i)
        {
            ExtractBoneHierarchy(bone.GetChild(i), ref boneIdx);
        }

        ExtractUtil.WriteTailTag(skeletonWriter, "Children");

        ExtractUtil.WriteTailTag(skeletonWriter, "Bone");
    }

    void ExtractSkeleton()
    {
        ProcessBones();
        ExtractUtil.WriteHeadTag(skeletonWriter, "Skeleton");
        ExtractUtil.WriteText(skeletonWriter, "Name", targetSkeletonName);

        ExtractUtil.WriteInteger(skeletonWriter, "Count", bones.Count);
        int n = 0;
        ExtractBoneHierarchy(targetSkeleton.transform, ref n);

        ExtractUtil.WriteTailTag(skeletonWriter, "Skeleton");
    }

    void ExportBinary()
    {
        string path = EditorUtility.SaveFilePanel("Export Binary", "ExportedAssets", "output.bin", "bin");
        geometryWriter = new BinaryWriter(File.Open(path, FileMode.Create));
        skeletonWriter = geometryWriter;

        ExtractUtil.WriteText(geometryWriter, "ModelName", targetObjectName);

        // 텍스처 매핑 정보를 가장 먼저 출력한다.
        // 나중에 임포트할 때, 중복되는 텍스처들을 다시 로드하지 않기 위해 필요하다.
        // 또한 텍스처를 추출할 때 그냥 이름 그대로 추출해도 되게 만든다.
        // 텍스처 이름(Key)이 이미 맵에 있다면 해당 경로(Value)의 텍스처는 로드하지 않는다.
        // (경로가 Key인 것보단 이름이 Key인 것이 SSO에서 유리할 것이다.)
        // (대신, 서로 다른 리소스간 중복된 텍스처 이름이 없어야 할 것.)
        ExtractTextureMapping();
        ExtractGeometry();
        ExtractBoundingVolumes();
        if (targetSkeleton != null) ExtractSkeleton();

        geometryWriter.Flush();
        geometryWriter.Close();
        Debug.Log("Model Binary Write Completed");
    }
}

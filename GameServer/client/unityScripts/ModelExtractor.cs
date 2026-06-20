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
    private string skeletonEnumeration = "";

    private BinaryWriter geometryWriter = null;
    private BinaryWriter skeletonWriter = null;
    private BinaryWriter itemDataWriter = null;

    private List<Transform> bones = null;
    private List<Matrix4x4> bindposes = null;
    private int[] boneIdxMap = null;

    // LOD 트리/식생 프리팹 대응:
    // 최상위 LOD(LOD0)에 속한 렌더러는 항상 추출하고(lod0Renderers),
    // 하위 LOD(LOD1+)에 속한 렌더러는 메시/재질을 건너뛴다(excludedLODRenderers).
    // 노드(Transform) 자체는 그대로 남겨 NodeCnt/ChildCnt 정합성을 유지한다.
    private HashSet<Renderer> lod0Renderers = new HashSet<Renderer>();
    private HashSet<Renderer> excludedLODRenderers = new HashSet<Renderer>();

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

        skeletonEnumeration = (string)EditorGUILayout.TextField("Skeleton Enumeration(optional): ", skeletonEnumeration);
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

    // 텍스처를 식별하는 키를 만든다.
    // Texture.name만으로는 텍스처를 구분할 수 없는 경우가 있다 (예: InfinityPBR 계열 에셋팩은
    // bat/gargoyle/goblin/Hobgoblin/bomber/snake 등 서로 다른 크리처의 albedo 텍스처가
    // 전부 "InfinityPBR_StandardColorID"라는 동일한 이름을 갖는다).
    // texHashMap(AssetManager 전역 텍스처 캐시)이 이 이름을 키로 사용하므로,
    // 이름이 겹치면 먼저 로드된 크리처의 텍스처를 나중에 로드되는 다른 크리처가 그대로 재사용해버린다.
    //
    // 전체 AssetDatabase 경로를 키로 쓰면 바이너리 포맷의 문자열 길이 제한에 걸린다
    // (길이 prefix가 1바이트라 최대 255바이트, C++ 측 읽기 버퍼도 그에 맞춰 256바이트로 고정되어 있음).
    // 대신 textureMappings에 기록된 출력 경로(Path)의 파일명(확장자 제외)을 키로 쓴다.
    // 이 프로젝트에서는 크리처마다 출력 파일명이 이미 고유하게 관리되고 있어
    // (BatColorSet, SnakeColorSet, GargoyleColorSet ...) 충돌 없이 짧은 키를 얻을 수 있다.
    string GetTextureKey(Texture tex)
    {
        if (tex == null) return null;

        string path = (textureMappings != null && textureMappings.TryGetValue(tex, out var mapped))
            ? mapped
            : AssetDatabase.GetAssetPath(tex);

        string key = string.IsNullOrEmpty(path) ? tex.name : Path.GetFileNameWithoutExtension(path);
        if (string.IsNullOrEmpty(key)) key = tex.name;

        if (System.Text.Encoding.UTF8.GetByteCount(key) > 255)
        {
            Debug.LogError($"[ModelExtractor] 텍스처 키가 바이너리 포맷 한계(255바이트)를 초과합니다: {key}");
        }

        return key;
    }

    // 잘 알려진 albedo 프로퍼티 이름 후보.
    static readonly string[] kAlbedoPropCandidates =
        { "_MainTex", "_BaseMap", "_BaseColorMap", "_AlbedoMap", "_TextureSample0" };

    // 머티리얼의 albedo(베이스 컬러) 텍스처를 견고하게 찾는다.
    // 1) 알려진 프로퍼티 이름, 2) 셰이더 텍스처 프로퍼티 전체 스캔(노멀/마스크/이미시브 등은 제외,
    // base/albedo/diffuse/main/color 류 이름 우선, 그래도 없으면 첫 번째 일반 텍스처).
    static Texture FindAlbedoTexture(Material mat)
    {
        if (mat == null || mat.shader == null) return null;

        foreach (var name in kAlbedoPropCandidates)
            if (mat.HasProperty(name) && mat.GetTexture(name) != null)
                return mat.GetTexture(name);

        var shader = mat.shader;
        int count = ShaderUtil.GetPropertyCount(shader);

        bool IsExcluded(string lname) =>
            lname.Contains("norm") || lname.Contains("bump") || lname.Contains("mask") ||
            lname.Contains("metal") || lname.Contains("smooth") || lname.Contains("spec") ||
            lname.Contains("gloss") || lname.Contains("occl") || lname.Contains("_ao") ||
            lname.Contains("emiss") || lname.Contains("height") || lname.Contains("detail") ||
            lname.Contains("lightmap");

        // pass 1: albedo로 보이는 이름 우선
        for (int i = 0; i < count; i++)
        {
            if (ShaderUtil.GetPropertyType(shader, i) != ShaderUtil.ShaderPropertyType.TexEnv) continue;
            string pn = ShaderUtil.GetPropertyName(shader, i);
            Texture tex = mat.GetTexture(pn);
            if (tex == null) continue;
            string l = pn.ToLowerInvariant();
            if (IsExcluded(l)) continue;
            if (l.Contains("base") || l.Contains("albedo") || l.Contains("diff") ||
                l.Contains("main") || l.Contains("color") || l.Contains("col"))
                return tex;
        }
        // pass 2: 제외 슬롯이 아닌 첫 번째 텍스처
        for (int i = 0; i < count; i++)
        {
            if (ShaderUtil.GetPropertyType(shader, i) != ShaderUtil.ShaderPropertyType.TexEnv) continue;
            string pn = ShaderUtil.GetPropertyName(shader, i);
            Texture tex = mat.GetTexture(pn);
            if (tex == null) continue;
            if (IsExcluded(pn.ToLowerInvariant())) continue;
            return tex;
        }
        return null;
    }

    void ExtractRagdollConfig(GoblinRagdollConfig ragdoll)
    {
        var w = itemDataWriter;
        ExtractUtil.WriteHeadTag(w, "RagdollConfig");

        // 래그돌 박스 치수는 unscaled로 추출한다. 런타임 Ragdoll::build에서 모델 고유 scale을
        // halfExtents·inertia에만 곱하고, center(capsuleOffset)는 boneWorldMat(objectWorldMat에
        // scale 포함)으로 변환되므로 unscaled로 둔다.
        ExtractUtil.WriteInteger(w, "BodyCnt", ragdoll.bodies.Count);
        foreach (var body in ragdoll.bodies)
        {
            ExtractUtil.WriteHeadTag(w, "Body");
            ExtractUtil.WriteText(w, "BodyName", body.name);
            ExtractUtil.WriteText(w, "BoneName", body.bone.name);
            ExtractUtil.WriteVector(w, "HalfExtents", body.halfExtents);
            ExtractUtil.WriteVector(w, "Center", body.center);
            ExtractUtil.WriteVector(w, "RotEuler", body.rotationEuler);
            ExtractUtil.WriteFloat(w, "Mass",           body.mass);
            ExtractUtil.WriteFloat(w, "LinearDamping",  body.linearDamping);
            ExtractUtil.WriteFloat(w, "AngularDamping", body.angularDamping);
            ExtractUtil.WriteFloat(w, "Friction",       body.friction);
            ExtractUtil.WriteFloat(w, "Restitution",    body.restitution);
            ExtractUtil.WriteFloat(w, "NoiseImpulse",   body.noiseImpulse);
            ExtractUtil.WriteTailTag(w, "Body");
        }

        ExtractUtil.WriteInteger(w, "JointCnt", ragdoll.joints.Count);
        foreach (var joint in ragdoll.joints)
        {
            ExtractUtil.WriteHeadTag(w, "Joint");
            ExtractUtil.WriteText(w, "ParentBody", joint.parentBodyName);
            ExtractUtil.WriteText(w, "ChildBody", joint.childBodyName);
            ExtractUtil.WriteText(w, "ParentBone", joint.parentBoneName);
            ExtractUtil.WriteText(w, "ChildBone", joint.childBoneName);
            ExtractUtil.WriteText(w, "JointType", joint.jointType.ToString());

            switch (joint.jointType)
            {
                case RagdollJointType.Hinge:
                    ExtractUtil.WriteVector(w, "AxisLocalA", joint.hingeAxisLocal);
                    ExtractUtil.WriteFloat(w, "MinAngle", Mathf.Deg2Rad * joint.minAngleDeg);
                    ExtractUtil.WriteFloat(w, "MaxAngle", Mathf.Deg2Rad * joint.maxAngleDeg);
                    break;
                case RagdollJointType.ConeTwist:
                    ExtractUtil.WriteFloat(w, "ConeHalfAngle", Mathf.Deg2Rad * joint.coneHalfAngleDeg);
                    ExtractUtil.WriteFloat(w, "TwistLimit", Mathf.Deg2Rad * joint.twistLimitDeg);
                    break;
            }

            ExtractUtil.WriteTailTag(w, "Joint");
        }

        ExtractUtil.WriteTailTag(w, "RagdollConfig");
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
            ExtractUtil.WriteText(geometryWriter, "TextureName", GetTextureKey(tex));
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

    // 정점별 dress 스킨 행렬(mesh-local→dress)을 만든다.
    //   dressSkin[b] = root.worldToLocal * bones[b].localToWorld * mesh.bindposes[b]
    //   vertex i    = Σ w * dressSkin[boneIndex]   (boneWeights 의 본 인덱스는 smr.bones/bindposes
    //                 평행 배열 기준; boneIdxMap 으로 스켈레톤 순서로 재매핑되기 '전'의 인덱스다.)
    // 본 팔레트가 rest 포즈에서 만드는 변형과 정확히 동일하므로, 씬 포즈가 FBX bind 와 달라도 정확.
    Matrix4x4[] BuildSkinBakeMatrices(Transform root, Transform xform,
        SkinnedMeshRenderer smr, Mesh mesh)
    {
        int vcnt = mesh != null ? mesh.vertexCount : 0;
        Matrix4x4 rootW2L = root.worldToLocalMatrix;
        Matrix4x4 nodeFallback = rootW2L * xform.localToWorldMatrix;

        Transform[] smrBones = smr != null ? smr.bones : null;
        Matrix4x4[] bindposes = mesh != null ? mesh.bindposes : null;
        BoneWeight[] weights = mesh != null ? mesh.boneWeights : null;

        // 폴백: bind/weight 정보가 부족하면 과거 동작(SMR 노드 변환)을 모든 정점에 동일 적용.
        if (vcnt == 0 || smrBones == null || smrBones.Length == 0
            || bindposes == null || bindposes.Length != smrBones.Length
            || weights == null || weights.Length != vcnt)
        {
            if (vcnt == 0) return null;
            Matrix4x4[] all = new Matrix4x4[vcnt];
            for (int i = 0; i < vcnt; i++) all[i] = nodeFallback;
            return all;
        }

        Matrix4x4[] dressSkin = new Matrix4x4[smrBones.Length];
        for (int b = 0; b < smrBones.Length; b++)
        {
            Matrix4x4 boneL2W = smrBones[b] != null
                ? smrBones[b].localToWorldMatrix : Matrix4x4.identity;
            dressSkin[b] = rootW2L * boneL2W * bindposes[b];
        }

        Matrix4x4[] result = new Matrix4x4[vcnt];
        for (int i = 0; i < vcnt; i++)
        {
            BoneWeight w = weights[i];
            float wsum = w.weight0 + w.weight1 + w.weight2 + w.weight3;
            if (wsum <= 1e-6f) { result[i] = nodeFallback; continue; }

            Matrix4x4 m = new Matrix4x4();   // 구조체 기본값 = 전부 0
            AddScaledMatrix(ref m, dressSkin[w.boneIndex0], w.weight0);
            AddScaledMatrix(ref m, dressSkin[w.boneIndex1], w.weight1);
            AddScaledMatrix(ref m, dressSkin[w.boneIndex2], w.weight2);
            AddScaledMatrix(ref m, dressSkin[w.boneIndex3], w.weight3);
            result[i] = m;
        }
        return result;
    }

    static void AddScaledMatrix(ref Matrix4x4 acc, Matrix4x4 m, float s)
    {
        if (s == 0f) return;
        for (int c = 0; c < 4; c++)
            for (int r = 0; r < 4; r++)
                acc[r, c] += m[r, c] * s;
    }

    // skinBakeMats != null(스킨드 메시)이면 정점별 dress 스킨 행렬로 정점/방향을 미리 굽는다.
    // (position은 affine point, normal·tangent는 동일 행렬의 MultiplyVector 후 정규화 — 런타임
    //  스킨드 셰이더가 normal 에도 anim 행렬을 그대로 곱하는 것과 일치시킨다.)
    void ExtractMesh(Mesh mesh, Matrix4x4[] skinBakeMats = null)
    {
        ExtractUtil.WriteHeadTag(geometryWriter, "Mesh");

        mesh.RecalculateTangents();

        // 정점/방향 데이터 준비 (스킨드면 dress 공간으로 베이크)
        Vector3[] positions = mesh.vertices;
        Vector3[] normalsArr = mesh.normals;
        Vector4[] tangents4 = mesh.tangents;

        bool baked = skinBakeMats != null && positions != null
                  && skinBakeMats.Length == positions.Length;
        if (baked)
        {
            Vector3[] bakedPos = new Vector3[positions.Length];
            for (int i = 0; i < positions.Length; i++)
                bakedPos[i] = skinBakeMats[i].MultiplyPoint3x4(positions[i]);
            positions = bakedPos;

            if (normalsArr != null && normalsArr.Length == skinBakeMats.Length)
            {
                Vector3[] bakedN = new Vector3[normalsArr.Length];
                for (int i = 0; i < normalsArr.Length; i++)
                    bakedN[i] = skinBakeMats[i].MultiplyVector(normalsArr[i]).normalized;
                normalsArr = bakedN;
            }
            if (tangents4 != null && tangents4.Length == skinBakeMats.Length)
            {
                Vector4[] bakedT = new Vector4[tangents4.Length];
                for (int i = 0; i < tangents4.Length; i++)
                {
                    Vector3 t = skinBakeMats[i].MultiplyVector(
                        new Vector3(tangents4[i].x, tangents4[i].y, tangents4[i].z)).normalized;
                    bakedT[i] = new Vector4(t.x, t.y, t.z, tangents4[i].w);   // w(부호) 보존
                }
                tangents4 = bakedT;
            }
        }

        // =========================
        // AABB 추출 (베이크된 정점 기준으로 재계산)
        // =========================
        Bounds bounds;
        if (baked && positions != null && positions.Length > 0)
        {
            Vector3 mn = positions[0], mx = positions[0];
            for (int i = 1; i < positions.Length; i++)
            {
                mn = Vector3.Min(mn, positions[i]);
                mx = Vector3.Max(mx, positions[i]);
            }
            bounds = new Bounds((mn + mx) * 0.5f, mx - mn);
        }
        else
        {
            bounds = mesh.bounds;
        }

        ExtractUtil.WriteHeadTag(geometryWriter, "Bounds");
        ExtractUtil.WriteVector(geometryWriter, "Center", bounds.center);
        ExtractUtil.WriteVector(geometryWriter, "Extents", bounds.extents);
        ExtractUtil.WriteVector(geometryWriter, "Min", bounds.min);
        ExtractUtil.WriteVector(geometryWriter, "Max", bounds.max);
        ExtractUtil.WriteTailTag(geometryWriter, "Bounds");

        // 버텍스 버퍼들 추출
        ExtractUtil.WriteHeadTag(geometryWriter, "VertexBuffers");
        if ((positions != null) && (positions.Length > 0)) ExtractUtil.WriteVectors(geometryWriter, "Positions", positions);
        if ((mesh.colors != null) && (mesh.colors.Length > 0)) ExtractUtil.WriteColors(geometryWriter, "Colors", mesh.colors);
        if ((normalsArr != null) && (normalsArr.Length > 0)) ExtractUtil.WriteVectors(geometryWriter, "Normals", normalsArr);
        if ((normalsArr != null) && (normalsArr.Length > 0)
            && (tangents4 != null) && (tangents4.Length > 0)
        )
        {
            Vector3[] tangents = new Vector3[tangents4.Length];
            Vector3[] bitangents = new Vector3[tangents4.Length];
            for (int i = 0; i < tangents4.Length; i++)
            {
                tangents[i] = new Vector3(tangents4[i].x, tangents4[i].y, tangents4[i].z);
                bitangents[i] = Vector3.Normalize(Vector3.Cross(normalsArr[i], tangents[i])) * tangents4[i].w;
            }
            ExtractUtil.WriteVectors(geometryWriter, "Tangents", tangents);
            ExtractUtil.WriteVectors(geometryWriter, "Bitangents", bitangents);
        }
        if ((mesh.uv != null) && (mesh.uv.Length > 0)) ExtractUtil.WriteVectors(geometryWriter, "TextureCoords0", mesh.uv);
        if ((mesh.uv2 != null) && (mesh.uv2.Length > 0)) ExtractUtil.WriteVectors(geometryWriter, "TextureCoords1", mesh.uv2);
        if ((mesh.boneWeights != null) && (mesh.boneWeights.Length > 0))
        {
            ExtractUtil.WriteBoneIndices(geometryWriter, "BoneIndices", mesh.boneWeights, boneIdxMap);
            ExtractUtil.WriteBoneWeights(geometryWriter, "BoneWeights", mesh.boneWeights);
        }
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
            // cAlbedo: _Color / _BaseColor 우선, 없으면 불투명 흰색으로 기본값을 쓴다.
            // (색상 상수가 비면 constantAlbedo가 (0,0,0,0)이 되어 검게 보이거나,
            //  폴리지 알파테스트에서 알파 0으로 전부 클리핑되어 사라질 수 있다 — 트리 버그 원인)
            {
                Color albedo = Color.white;
                if (materials[i].HasProperty("_Color"))          albedo = materials[i].GetColor("_Color");
                else if (materials[i].HasProperty("_BaseColor")) albedo = materials[i].GetColor("_BaseColor");
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
            // AlbedoMap: 잘 알려진 프로퍼티 이름을 먼저 시도하고, 없으면 셰이더의
            // 텍스처 프로퍼티 전체를 훑어 albedo로 보이는 슬롯을 찾는다. 커스텀/Shader Graph
            // 기반 트리처럼 albedo 슬롯 이름이 _MainTex/_BaseMap이 아닌 경우, 이 폴백이 없으면
            // 머티리얼이 albedo 맵 없이 임포트되어 폴리지 알파테스트로 전부 클리핑(=안 보임)된다.
            Texture albedoTex = FindAlbedoTexture(materials[i]);
            if (albedoTex != null)
            {
                ExtractUtil.WriteText(geometryWriter, "AlbedoMap", GetTextureKey(albedoTex));
            }
            // NormalMap
            if (materials[i].HasProperty("_BumpMap"))
            {
                Texture bumpMap = materials[i].GetTexture("_BumpMap");
                if (bumpMap != null)
                {
                    ExtractUtil.WriteText(geometryWriter, "NormalMap", GetTextureKey(bumpMap));
                }
            }
            // MetallicSmoothnessMap
            if (materials[i].HasProperty("_MetallicGlossMap"))
            {
                Texture metallicGlossMap = materials[i].GetTexture("_MetallicGlossMap");
                if (metallicGlossMap != null)
                {
                    ExtractUtil.WriteText(geometryWriter, "MetallicSmoothnessMap", GetTextureKey(metallicGlossMap));
                }
            }
            // EmmisiveMap
            if (materials[i].HasProperty("_EmissionMap"))
            {
                Texture emmisionMap = materials[i].GetTexture("_EmissionMap");
                if (emmisionMap != null)
                {
                    ExtractUtil.WriteText(geometryWriter, "EmmisiveMap", GetTextureKey(emmisionMap));
                }
            }
            // AOMap
            if (materials[i].HasProperty("_OcclusionMap"))
            {
                Texture aoMap = materials[i].GetTexture("_OcclusionMap");
                if (aoMap != null)
                {
                    ExtractUtil.WriteText(geometryWriter, "AOMap", GetTextureKey(aoMap));
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

        // 메시/렌더러 판별을 변환 행렬 기록보다 먼저 한다.
        // 스킨드 메시는 노드 변환 처리 방식이 달라(아래 참조) 행렬을 쓰기 전에 알아야 한다.
        MeshRenderer meshRenderer = xform.gameObject.GetComponent<MeshRenderer>();
        MeshFilter meshFilter = xform.gameObject.GetComponent<MeshFilter>();
        SkinnedMeshRenderer skinnedMeshRenderer = xform.gameObject.GetComponent<SkinnedMeshRenderer>();

        // LOD 분류 반영: 하위 LOD 렌더러는 추출 대상에서 제외한다.
        bool meshRendererUsable = IsRendererUsable(meshRenderer);
        bool skinnedMeshRendererUsable = IsRendererUsable(skinnedMeshRenderer);

        Mesh mesh = null;
        bool isSkinnedMesh = false;
        if (meshRenderer && meshFilter && meshRendererUsable)
        {
            mesh = meshFilter.sharedMesh;
        }
        else if (skinnedMeshRenderer && skinnedMeshRendererUsable)
        {
            mesh = skinnedMeshRenderer.sharedMesh;
            isSkinnedMesh = true;
            boneIdxMap = new int[skinnedMeshRenderer.bones.Length];
            for (int i = 0; i < skinnedMeshRenderer.bones.Length; i++)
            {
                bool found = false;
                for (int j = 0; j < bones.Count; j++)
                {
                    if (skinnedMeshRenderer.bones[i].gameObject.name == bones[j].gameObject.name)
                    {
                        found = true;
                        boneIdxMap[i] = j;
                        break;
                    }
                }

                if (!found)
                {
                    Debug.LogError("Bone not found: " + skinnedMeshRenderer.bones[i].gameObject.name);
                }
            }
        }

        // 변환 추출
        // 스킨드 메시는 본 팔레트(toDress/toLocal, root 기준 dress 공간)가 정점 변형을 책임진다.
        // 그런데 런타임 스킨드 셰이더는 position·anim·meshXform 순서로 곱하므로(meshXform=노드
        // DressMatrix), 노드 자체에 회전/스케일이 있으면 그 변환이 본 변형 '뒤'에 적용되어
        // 정점(mesh-local) 공간과 본 팔레트(dress) 공간이 어긋난다 — snake가 90° 서거나 birdy
        // 말단이 늘어나는 원인. 따라서 정점을 dress(root) 공간으로 미리 구워 옮기고, 노드 행렬은
        // 항등으로 기록한다. 이러면 런타임이 position(dress)·anim·I 로 본 팔레트를 올바른 공간에서
        // 적용한다. 정적 메시는 기존대로 노드 변환을 meshXform으로 유지한다(정점은 raw mesh-local).
        //
        // [중요] 정점을 dress 공간으로 옮기는 변환은 SMR 노드 변환이 아니라 '실제 bind pose'에서
        // 유도해야 한다. Unity 는 스킨드 렌더링에서 SMR 노드 자체 변환을 무시하고 mesh.bindposes 로
        // mesh-local→bone 결합을 정의한다. 본 b 의 dress 스킨 행렬은
        //   dressSkin[b] = root.worldToLocal * bones[b].localToWorld * mesh.bindposes[b]
        // 이고, 정점 i 의 베이크 변환은 가중 합 Σ w * dressSkin[b] (= 본 팔레트가 rest 포즈에서 만드는
        // 변형과 정확히 동일) 이다. 씬의 rest 포즈가 FBX bind 와 다르면 본마다 dressSkin 이 달라지므로
        // 단일 행렬(특정 본 하나)로 구우면 메시가 그 본의 포즈 오차만큼 강체로 기울어진다 — 정점별 LBS 로
        // 구워야 정확하다. 노드 행렬은 항등으로 기록한다. (씬==bind 인 birdy 는 모든 dressSkin 이
        // 동일해 단일 행렬과 같은 결과; snake 는 정점별로 올바르게 정렬.)
        Matrix4x4[] skinBakeMats = null;
        if (isSkinnedMesh)
        {
            skinBakeMats = BuildSkinBakeMatrices(root, xform, skinnedMeshRenderer, mesh);
            ExtractUtil.WriteMatrix(geometryWriter, "LocalMatrix", Matrix4x4.identity);
            ExtractUtil.WriteMatrix(geometryWriter, "DressMatrix", Matrix4x4.identity);
        }
        else
        {
            ExtractUtil.WriteLocalMatrix(geometryWriter, "LocalMatrix", xform);
            ExtractUtil.WriteDressMatrix(geometryWriter, "DressMatrix", root, xform);
        }

        if (mesh != null)
        {
            ExtractMesh(mesh, skinBakeMats);
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
            if (meshRenderer && meshRendererUsable)
            {
                materials = meshRenderer.sharedMaterials;
            }
            else if (skinnedMeshRenderer && skinnedMeshRendererUsable)
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

    // LODGroup이 있는 프리팹(나무/식생 등)에서 LOD0 렌더러와 하위 LOD 렌더러를 분류한다.
    // LOD0은 항상 추출하고, LOD1+ 는 메시/재질을 건너뛰기 위함이다.
    void CollectLODRenderers()
    {
        lod0Renderers.Clear();
        excludedLODRenderers.Clear();

        var lodGroups = targetObject.GetComponentsInChildren<LODGroup>(true);
        foreach (var lg in lodGroups)
        {
            var lods = lg.GetLODs();
            if (lods == null || lods.Length == 0) continue;

            // LOD0 렌더러 (강제 포함)
            foreach (var r in lods[0].renderers)
            {
                if (r != null) lod0Renderers.Add(r);
            }

            // LOD1+ 렌더러 (제외). 단, LOD0에도 참조된 렌더러는 제외하지 않는다.
            for (int i = 1; i < lods.Length; i++)
            {
                foreach (var r in lods[i].renderers)
                {
                    if (r != null && !lod0Renderers.Contains(r))
                        excludedLODRenderers.Add(r);
                }
            }
        }
    }

    // 렌더러를 추출 대상으로 삼을지 판단한다.
    // - 하위 LOD(excludedLODRenderers)면 추출하지 않는다.
    // - LOD0(lod0Renderers)이면 씬뷰 상태로 enabled가 꺼져 있어도 강제 추출한다.
    // - 그 외(LOD 없는 일반 오브젝트)는 기존 동작대로 enabled를 따른다.
    bool IsRendererUsable(Renderer r)
    {
        if (r == null) return false;
        if (excludedLODRenderers.Contains(r)) return false;
        return r.enabled || lod0Renderers.Contains(r);
    }

    void ExtractGeometry()
    {
        ExtractUtil.WriteHeadTag(geometryWriter, "Geometry");
        int nodeCnt = 0;
        ExtractUtil.AccNodeCnt(targetObject.transform, ref nodeCnt);
        ExtractUtil.WriteInteger(geometryWriter, "NodeCnt", nodeCnt);

        // Model's own scale (Unity root localScale). The geometry/BV/bones are extracted
        // unscaled; the runtime applies this via Object's body scale (setModel), so mesh and
        // BV stay consistent through a single scale path.
        ExtractUtil.WriteVector(geometryWriter, "ModelScale", targetObject.transform.localScale);

        ExtractTransform(targetObject.transform, targetObject.transform);

        ExtractUtil.WriteTailTag(geometryWriter, "Geometry");
    }

    // 바운딩 볼륨들 추출
    void ExtractBoundingVolumes()
    {
        var mbv = targetObject.GetComponent<MultiBoundingVolume>();

        ExtractUtil.WriteHeadTag(geometryWriter, "BoundingVolumes");

        if (mbv == null || mbv.lods == null || mbv.lods.Count == 0)
        {
            ExtractUtil.WriteInteger(geometryWriter, "LODCount", 0);
            ExtractUtil.WriteTailTag(geometryWriter, "BoundingVolumes");
            return;
        }

        // LOD 개수
        ExtractUtil.WriteInteger(geometryWriter, "LODCount", mbv.lods.Count);

        for (int i = 0; i < mbv.lods.Count; i++)
        {
            var lod = mbv.lods[i];

            ExtractUtil.WriteHeadTag(geometryWriter, "LOD");
            ExtractUtil.WriteInteger(geometryWriter, "Index", i);
            ExtractUtil.WriteInteger(geometryWriter, "BoxCount", lod.boxes.Count);

            foreach (var box in lod.boxes)
            {
                ExtractUtil.WriteHeadTag(geometryWriter, "Box");

                // 이름
                ExtractUtil.WriteText(geometryWriter, "Name", box.name ?? "");

                // 본 이름 (중요)
                string boneName = box.bone != null ? box.bone.name : "";
                ExtractUtil.WriteText(geometryWriter, "Bone", boneName);

                // BV는 unscaled로 추출(모델 고유 scale은 런타임 body scale로 적용).
                ExtractUtil.WriteVector(geometryWriter, "Center", box.localCenter);
                ExtractUtil.WriteVector(geometryWriter, "Size", box.size);
                ExtractUtil.WriteVector(geometryWriter, "Rotation", box.rotationEuler);

                // static 여부 (본에 붙은 박스는 본의, 그 외는 대상 오브젝트의 static 플래그를 따른다)
                bool isStatic = box.bone != null ? box.bone.gameObject.isStatic : targetObject.isStatic;
                ExtractUtil.WriteInteger(geometryWriter, "IsStatic", isStatic ? 1 : 0);

                ExtractUtil.WriteTailTag(geometryWriter, "Box");
            }

            ExtractUtil.WriteTailTag(geometryWriter, "LOD");
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
        bindposes = new List<Matrix4x4>();

        ProcessBoneHierarchy(targetObject.transform, targetSkeleton.transform);
    }

    void ExtractBoneHierarchy(Transform bone, ref int boneIdx)
    {
        ExtractUtil.WriteHeadTag(skeletonWriter, "Bone");
        ExtractUtil.WriteText(skeletonWriter, "Name", bone.gameObject.name);
        ExtractUtil.WriteDressMatrix(skeletonWriter, "Dress", targetObject.transform, bone);
        ExtractUtil.WriteMatrix(skeletonWriter, "ToLocal", bindposes[boneIdx]);
        BoneSocket socket = bone.gameObject.GetComponent<BoneSocket>();
        if (socket != null)
        {
            ExtractUtil.WriteText(skeletonWriter, "SocketType", socket.socketType.ToString());
        }
        else
        {
            ExtractUtil.WriteText(skeletonWriter, "SocketType", "None");
        }

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
        ExtractUtil.WriteHeadTag(skeletonWriter, "Skeleton");
        ExtractUtil.WriteText(skeletonWriter, "Name", targetSkeletonName);
        ExtractUtil.WriteText(skeletonWriter, "SkeletonEnumeration", skeletonEnumeration);

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
        itemDataWriter = geometryWriter;

        ExtractUtil.WriteText(geometryWriter, "ModelName", targetObjectName);

        // 텍스처 매핑 정보를 가장 먼저 출력한다.
        // 나중에 임포트할 때, 중복되는 텍스처들을 다시 로드하지 않기 위해 필요하다.
        // 또한 텍스처를 추출할 때 그냥 이름 그대로 추출해도 되게 만든다.
        // 텍스처 이름(Key)이 이미 맵에 있다면 해당 경로(Value)의 텍스처는 로드하지 않는다.
        // (경로가 Key인 것보단 이름이 Key인 것이 SSO에서 유리할 것이다.)
        // (대신, 서로 다른 리소스간 중복된 텍스처 이름이 없어야 할 것.)
        if (targetSkeleton != null) ProcessBones();
        CollectLODRenderers();
        ExtractTextureMapping();
        ExtractGeometry();
        ExtractBoundingVolumes();
        ExtractUtil.WriteInteger(skeletonWriter, "HasSkeleton", Convert.ToInt32(targetSkeleton != null));
        if (targetSkeleton != null) ExtractSkeleton();
        Weapon weapon = targetObject.GetComponent<Weapon>();
        ExtractUtil.WriteInteger(itemDataWriter, "HasWeaponInfo", Convert.ToInt32(weapon != null));
        if (weapon != null)
        {
            ItemData itemData = weapon.itemData;
            itemData.WriteBinaryInfo(itemDataWriter);
        }

        GoblinRagdollConfig ragdoll = targetObject.GetComponent<GoblinRagdollConfig>();
        ExtractUtil.WriteInteger(itemDataWriter, "HasRagdollConfig", Convert.ToInt32(ragdoll != null));
        if (ragdoll != null)
            ExtractRagdollConfig(ragdoll);

        geometryWriter.Flush();
        geometryWriter.Close();
        Debug.Log("Model Binary Write Completed");
    }
}

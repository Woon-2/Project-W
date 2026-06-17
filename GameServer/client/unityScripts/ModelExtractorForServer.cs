using UnityEngine;
using UnityEditor;
using System.Collections.Generic;
using System.IO;

public class ModelExtractorForServerWindow : EditorWindow
{
    private GameObject targetObject;
    private GameObject targetSkeleton;
    private string targetName = "";
    private string targetSkeletonName = "";
    private string skeletonEnumeration = "";
    private Dictionary<string, float> damageCoeffs = new Dictionary<string, float>();
    private Vector2 scrollPos;

    private BinaryWriter geometryWriter = null;

    [MenuItem("Tools/Model Extractor(For Server)")]
    public static void OpenWindow()
    {
        GetWindow<ModelExtractorForServerWindow>("Model Extractor(For Server)");
    }

    private void OnGUI()
    {
        EditorGUILayout.LabelField("Model Extractor Tool (Server)", EditorStyles.boldLabel);
        EditorGUILayout.Space();

        targetObject = (GameObject)EditorGUILayout.ObjectField("Target Object", targetObject, typeof(GameObject), true);
        targetName = EditorGUILayout.TextField("Object Name", targetName);
        EditorGUILayout.Space();

        targetSkeleton = (GameObject)EditorGUILayout.ObjectField("Target Skeleton (optional)", targetSkeleton, typeof(GameObject), true);
        targetSkeletonName = EditorGUILayout.TextField("Skeleton Name", targetSkeletonName);
        skeletonEnumeration = EditorGUILayout.TextField("Skeleton Enumeration", skeletonEnumeration);
        EditorGUILayout.Space();

        if (targetObject != null)
        {
            var mbv = targetObject.GetComponent<MultiBoundingVolume>();
            if (mbv != null && mbv.lods != null && mbv.lods.Count > 0)
            {
                EditorGUILayout.LabelField("Damage Coefficients", EditorStyles.boldLabel);
                scrollPos = EditorGUILayout.BeginScrollView(scrollPos, GUILayout.MaxHeight(200f));
                for (int lod = 0; lod < mbv.lods.Count; lod++)
                {
                    EditorGUILayout.LabelField("LOD " + lod, EditorStyles.miniBoldLabel);
                    for (int b = 0; b < mbv.lods[lod].boxes.Count; b++)
                    {
                        var box = mbv.lods[lod].boxes[b];
                        string key = lod + "_" + b + "_" + box.name;
                        if (!damageCoeffs.ContainsKey(key)) damageCoeffs[key] = 1.0f;
                        damageCoeffs[key] = EditorGUILayout.FloatField("  " + box.name + " DamageCoeff", damageCoeffs[key]);
                    }
                }
                EditorGUILayout.EndScrollView();
                EditorGUILayout.Space();
            }
        }

        if (GUILayout.Button("Export as binary (.bin)"))
        {
            ExportBinary();
        }
    }

    int CountBones(Transform bone)
    {
        int count = 1;
        for (int i = 0; i < bone.childCount; ++i)
            count += CountBones(bone.GetChild(i));
        return count;
    }

    void ExtractBoneHierarchy(Transform bone, ref int boneIdx)
    {
        ExtractUtil.WriteHeadTag(geometryWriter, "Bone");
        ExtractUtil.WriteText(geometryWriter, "Name", bone.gameObject.name);
        ExtractUtil.WriteDressMatrix(geometryWriter, "Dress", targetObject.transform, bone);
        ExtractUtil.WriteMatrix(geometryWriter, "ToLocal", Matrix4x4.identity);
        ExtractUtil.WriteText(geometryWriter, "SocketType", "None");
        ++boneIdx;

        ExtractUtil.WriteHeadTag(geometryWriter, "Children");
        ExtractUtil.WriteInteger(geometryWriter, "ChildCnt", bone.childCount);
        for (int i = 0; i < bone.childCount; ++i)
            ExtractBoneHierarchy(bone.GetChild(i), ref boneIdx);
        ExtractUtil.WriteTailTag(geometryWriter, "Children");

        ExtractUtil.WriteTailTag(geometryWriter, "Bone");
    }

    void ExtractSkeleton()
    {
        ExtractUtil.WriteHeadTag(geometryWriter, "Skeleton");
        ExtractUtil.WriteText(geometryWriter, "Name", targetSkeletonName);
        ExtractUtil.WriteText(geometryWriter, "SkeletonEnumeration", skeletonEnumeration);
        int boneCnt = CountBones(targetSkeleton.transform);
        ExtractUtil.WriteInteger(geometryWriter, "Count", boneCnt);
        int n = 0;
        ExtractBoneHierarchy(targetSkeleton.transform, ref n);
        ExtractUtil.WriteTailTag(geometryWriter, "Skeleton");
    }

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

        ExtractUtil.WriteInteger(geometryWriter, "LODCount", mbv.lods.Count);

        for (int i = 0; i < mbv.lods.Count; i++)
        {
            var lod = mbv.lods[i];

            ExtractUtil.WriteHeadTag(geometryWriter, "LOD");
            ExtractUtil.WriteInteger(geometryWriter, "Index", i);
            ExtractUtil.WriteInteger(geometryWriter, "BoxCount", lod.boxes.Count);

            for (int b = 0; b < lod.boxes.Count; b++)
            {
                var box = lod.boxes[b];
                string key = i + "_" + b + "_" + box.name;
                float dc = damageCoeffs.ContainsKey(key) ? damageCoeffs[key] : 1.0f;

                ExtractUtil.WriteHeadTag(geometryWriter, "Box");
                ExtractUtil.WriteText(geometryWriter, "Name", box.name ?? "");
                string boneName = box.bone != null ? box.bone.name : "";
                ExtractUtil.WriteText(geometryWriter, "Bone", boneName);
                // 루트 스케일 반영 (ExtractUtil.GetScaledBoxCenterSize 참고)
                ExtractUtil.GetScaledBoxCenterSize(
                    box, targetObject.transform.localScale, out var center, out var size);
                ExtractUtil.WriteVector(geometryWriter, "Center", center);
                ExtractUtil.WriteVector(geometryWriter, "Size", size);
                ExtractUtil.WriteVector(geometryWriter, "Rotation", box.rotationEuler);
                // static 여부 (본에 붙은 박스는 본의, 그 외는 대상 오브젝트의 static 플래그를 따른다)
                bool isStatic = box.bone != null ? box.bone.gameObject.isStatic : targetObject.isStatic;
                ExtractUtil.WriteInteger(geometryWriter, "IsStatic", isStatic ? 1 : 0);
                ExtractUtil.WriteFloat(geometryWriter, "DamageCoeff", dc);
                ExtractUtil.WriteTailTag(geometryWriter, "Box");
            }

            ExtractUtil.WriteTailTag(geometryWriter, "LOD");
        }

        ExtractUtil.WriteTailTag(geometryWriter, "BoundingVolumes");
    }

    void ExportBinary()
    {
        string path = EditorUtility.SaveFilePanel("Export Binary", "ExportedAssets", "output.bin", "bin");
        if (string.IsNullOrEmpty(path)) return;

        geometryWriter = new BinaryWriter(File.Open(path, FileMode.Create));

        ExtractUtil.WriteText(geometryWriter, "ModelName", targetName);
        ExtractBoundingVolumes();

        bool hasSkeleton = targetSkeleton != null;
        ExtractUtil.WriteInteger(geometryWriter, "HasSkeleton", hasSkeleton ? 1 : 0);
        if (hasSkeleton)
            ExtractSkeleton();

        geometryWriter.Flush();
        geometryWriter.Close();
        Debug.Log("Model Binary Write Completed");
    }
}

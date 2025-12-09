using UnityEngine;
using UnityEditor;
using System;
using System.Collections;
using System.Collections.Generic;
using System.IO;
using System.Text;

public class ModelExtractorForServerWindow : EditorWindow
{
    private GameObject targetObject;
    private Vector2 scrollPos;
    private string targetName = "";

    private BinaryWriter geometryWriter = null;

    [MenuItem("Tools/Model Extractor(For Server)")]
    public static void OpenWindow()
    {
        GetWindow<ModelExtractorForServerWindow>("Model Extractor(For Server)");
    }

    private void OnGUI()
    {
        EditorGUILayout.LabelField("🎨 Model Extractor Tool", EditorStyles.boldLabel);
        EditorGUILayout.Space();

        targetObject = (GameObject)EditorGUILayout.ObjectField("Target Object", targetObject, typeof(GameObject), true);
        EditorGUILayout.Space();

        targetName = (string)EditorGUILayout.TextField("Object Name: ", targetName);
        EditorGUILayout.Space();

        if (GUILayout.Button("💾 Export as a binary(.bin)"))
        {
            ExportBinary();
        }
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
        ExtractUtil.WriteInteger(geometryWriter, "Count", bvComponent.boundingVolumes.Count * 2);

        // 바운딩 박스
        foreach (var bv in bvComponent.boundingVolumes)
        {
            if (bv == null || bv.BVBox == null)
                continue;

            ExtractUtil.WriteHeadTag(geometryWriter, "BoundingVolume");
            ExtractUtil.WriteText(geometryWriter, "Type", "Box");

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
        // 바운딩 사각형 (박스를 통해 추출)
        foreach (var bv in bvComponent.boundingVolumes)
        {
            if (bv == null || bv.BVBox == null)
                continue;

            ExtractUtil.WriteHeadTag(geometryWriter, "BoundingVolume");
            ExtractUtil.WriteText(geometryWriter, "Type", "Rect");

            // 이름
            ExtractUtil.WriteText(geometryWriter, "Name", bv.BVName + "_2D");

            BoxCollider box = bv.BVBox;

            Vector2 center = new Vector2(box.center.x, box.center.z);
            Vector2 size = new Vector2(box.size.x, box.size.z);

            // BoxCollider 고유 값들 저장
            ExtractUtil.WriteVector(geometryWriter, "Center", center);
            ExtractUtil.WriteVector(geometryWriter, "Size", size);

            ExtractUtil.WriteTailTag(geometryWriter, "BoundingVolume");
        }

        ExtractUtil.WriteTailTag(geometryWriter, "BoundingVolumes");
    }

    void ExportBinary()
    {
        string path = EditorUtility.SaveFilePanel("Export Binary", "ExportedAssets", "output.bin", "bin");
        geometryWriter = new BinaryWriter(File.Open(path, FileMode.Create));

        ExtractUtil.WriteText(geometryWriter, "ModelName", targetName);

        ExtractBoundingVolumes();

        geometryWriter.Flush();
        geometryWriter.Close();
        Debug.Log("Model Binary Write Completed");
    }
}

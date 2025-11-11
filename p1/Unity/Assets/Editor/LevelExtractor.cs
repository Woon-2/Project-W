using UnityEngine;
using UnityEditor;
using System;
using System.Collections;
using System.Collections.Generic;
using System.IO;
using System.Text;

public class LevelExtractorWindow : EditorWindow
{
    private GameObject levelRoot;

    private BinaryWriter binaryWriter = null;

    [MenuItem("Tools/Level Extractor")]
    public static void OpenWindow()
    {
        GetWindow<LevelExtractorWindow>("Level Extractor");
    }

    private void OnGUI()
    {
        EditorGUILayout.LabelField("🎨 Level Extractor Tool", EditorStyles.boldLabel);
        EditorGUILayout.Space();

        levelRoot = (GameObject)EditorGUILayout.ObjectField("Level Root", levelRoot, typeof(GameObject), true);
        EditorGUILayout.Space();

        if (GUILayout.Button("💾 Export as a binary(.bin)"))
        {
            ExportBinary();
        }
    }

    void ExtractNodes()
    {
        ExtractUtil.WriteHeadTag(binaryWriter, "Level");
        int nodeCnt = 0;
        ExtractUtil.AccNodeCnt(levelRoot.transform, ref nodeCnt);
        ExtractUtil.WriteInteger(binaryWriter, "NodeCnt", nodeCnt);

        levelRoot.GetComponent<ILevelExportable>().ExportLevelNodeData(binaryWriter, levelRoot.transform);

        ExtractUtil.WriteTailTag(binaryWriter, "Level");
    }

    void ExportBinary()
    {
        string path = EditorUtility.SaveFilePanel("Export Binary", "ExportedAssets", "output.bin", "bin");
        binaryWriter = new BinaryWriter(File.Open(path, FileMode.Create));

        ExtractNodes();

        binaryWriter.Flush();
        binaryWriter.Close();
        Debug.Log("Level Binary Write Completed");
    }
}
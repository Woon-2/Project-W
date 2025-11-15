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
        ExtractSkybox();

        binaryWriter.Flush();
        binaryWriter.Close();
        Debug.Log("Level Binary Write Completed");
    }

    void ExtractSkybox()
    {
        SkyboxSelector skyboxSelector = levelRoot.GetComponent<SkyboxSelector>();
        if (skyboxSelector == null)
        {
            Debug.LogWarning(levelRoot.name + " doesn't have SkyboxSelector Component, "
                + "the level will be exported without skybox info.");
            return;
        }

        ExtractUtil.WriteHeadTag(binaryWriter, "Skyboxes");

        ExtractUtil.WriteInteger(binaryWriter, "SkyboxCnt", skyboxSelector.skyboxMaterials.Count);
        for (int i = 0; i < skyboxSelector.skyboxMaterials.Count; ++i)
        {
            ExtractUtil.WriteHeadTag(binaryWriter, "Skybox");

            int materialCnt = 1;
            ExtractUtil.WriteInteger(binaryWriter, "MaterialCnt", materialCnt);
            for (int j = 0; j < materialCnt; ++j)
            {
                ExtractUtil.WriteHeadTag(binaryWriter, "Material");
                ExtractUtil.WriteText(binaryWriter, "Cubemap", skyboxSelector.skyboxMaterials[i].name + "_Cubemap");
                ExtractUtil.WriteTailTag(binaryWriter, "Material");
            }

            ExtractUtil.WriteTailTag(binaryWriter, "Skybox");
        }

        ExtractUtil.WriteTailTag(binaryWriter, "Skyboxes");

        ExtractUtil.WriteHeadTag(binaryWriter, "SelectedSkybox");
        ExtractUtil.WriteText(binaryWriter, "Name", skyboxSelector.skyboxMaterials[skyboxSelector.currentMaterialIdx].name + "_Cubemap");
        ExtractUtil.WriteInteger(binaryWriter, "Index", skyboxSelector.currentMaterialIdx);
        ExtractUtil.WriteTailTag(binaryWriter, "SelectedSkybox");
    }
}
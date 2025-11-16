#if UNITY_EDITOR
using UnityEditor;
using UnityEngine;

[CustomEditor(typeof(SkyboxSelector))]
public class SkyboxSelectorEditor : Editor
{
    public override void OnInspectorGUI()
    {
        var selector = (SkyboxSelector)target;

        DrawDefaultInspector();

        if (selector.skyboxMaterials == null || selector.skyboxMaterials.Count == 0)
            return;

        EditorGUILayout.Space();
        EditorGUILayout.LabelField("Preview", EditorStyles.boldLabel);

        // 드롭다운으로 재질 선택
        string[] options = new string[selector.skyboxMaterials.Count];
        for (int i = 0; i < selector.skyboxMaterials.Count; i++)
            options[i] = selector.skyboxMaterials[i].name;

        int newIndex = EditorGUILayout.Popup("Active Material", selector.currentMaterialIdx, options);
        if (newIndex != selector.currentMaterialIdx)
        {
            Undo.RecordObject(selector, "Change Material");
            selector.currentMaterialIdx = newIndex;
            selector.ApplyCurrentSkybox();
        }

        if (GUILayout.Button("Apply Current Material"))
        {
            selector.ApplyCurrentSkybox();
        }
    }
}
#endif
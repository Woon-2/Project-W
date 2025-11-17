#if UNITY_EDITOR
using UnityEditor;
using UnityEngine;

[CustomEditor(typeof(MaterialSetSelector))]
public class MaterialSetSelectorEditor : Editor
{
    public override void OnInspectorGUI()
    {
        var selector = (MaterialSetSelector)target;

        DrawDefaultInspector();

        if (selector.materialSets == null || selector.materialSets.Count == 0)
            return;

        EditorGUILayout.Space();
        EditorGUILayout.LabelField("Preview", EditorStyles.boldLabel);

        // 드롭다운으로 세트 선택
        string[] options = new string[selector.materialSets.Count];
        for (int i = 0; i < selector.materialSets.Count; i++)
            options[i] = selector.materialSets[i].name;

        int newIndex = EditorGUILayout.Popup("Active Set", selector.currentSetIndex, options);
        if (newIndex != selector.currentSetIndex)
        {
            Undo.RecordObject(selector, "Change Material Set");
            selector.currentSetIndex = newIndex;
            selector.ApplyCurrentSet();
        }

        if (GUILayout.Button("Apply Current Set"))
        {
            selector.ApplyCurrentSet();
        }
    }
}
#endif
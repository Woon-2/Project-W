using UnityEngine;
using UnityEditor;
using System.Collections.Generic;

#if UNITY_EDITOR
[CustomEditor(typeof(MultiBoundingVolume))]
public class MultiBoundingVolumeEditor : Editor
{
    private MultiBoundingVolume mbv;

    private bool editMode = false;
    private int selectedLOD = 0;
    private int selectedBox = -1;

    private void OnEnable()
    {
        mbv = (MultiBoundingVolume)target;
        mbv.SyncLOD();
    }

    public override void OnInspectorGUI()
    {
        EditorGUI.BeginChangeCheck();

        mbv.lodCount = EditorGUILayout.IntSlider("LOD Count", mbv.lodCount, 1, 3);

        if (EditorGUI.EndChangeCheck())
        {
            Undo.RecordObject(mbv, "Change LOD Count");
            mbv.SyncLOD();
        }

        EditorGUILayout.Space();
        editMode = EditorGUILayout.Toggle("Edit Mode", editMode);

        for (int i = 0; i < mbv.lods.Count; i++)
        {
            var lod = mbv.lods[i];

            EditorGUILayout.Space();
            EditorGUILayout.LabelField($"LOD {i}", EditorStyles.boldLabel);

            int removeIndex = -1;

            for (int j = 0; j < lod.boxes.Count; j++)
            {
                var box = lod.boxes[j];

                EditorGUILayout.BeginVertical("box");

                EditorGUILayout.BeginHorizontal();
                if (GUILayout.Toggle(selectedLOD == i && selectedBox == j, "Select", "Button"))
                {
                    selectedLOD = i;
                    selectedBox = j;
                }
                EditorGUILayout.LabelField(box.name == string.Empty ? "(Unnamed)" : box.name);
                EditorGUILayout.EndHorizontal();

                box.name = EditorGUILayout.TextField("Name", box.name);
                box.bone = (Transform)EditorGUILayout.ObjectField("Bone", box.bone, typeof(Transform), true);
                box.localCenter = EditorGUILayout.Vector3Field("Local Center", box.localCenter);
                box.size = EditorGUILayout.Vector3Field("Size", box.size);
                box.rotationEuler = EditorGUILayout.Vector3Field("Rotation", box.rotationEuler);

                if (GUILayout.Button("Remove Box"))
                    removeIndex = j;

                EditorGUILayout.EndVertical();
            }

            if (removeIndex >= 0)
            {
                Undo.RecordObject(mbv, "Remove Box");
                lod.boxes.RemoveAt(removeIndex);
                if (selectedLOD == i && selectedBox == removeIndex)
                    selectedBox = -1;
            }

            if (GUILayout.Button("Add Box"))
            {
                Undo.RecordObject(mbv, "Add Box");
                lod.boxes.Add(new BoundingBoxData());
            }
        }

        if (GUI.changed)
        {
            EditorUtility.SetDirty(mbv);
        }
    }

    private void OnSceneGUI()
    {
        mbv = (MultiBoundingVolume)target;

        for (int i = 0; i < mbv.lods.Count; i++)
        {
            var lod = mbv.lods[i];

            for (int j = 0; j < lod.boxes.Count; j++)
            {
                var box = lod.boxes[j];
                if (box.bone == null) continue;

                bool isSelected = (i == selectedLOD && j == selectedBox);

                if (editMode && !isSelected)
                    continue;

                Handles.color = isSelected ? Color.yellow : Color.green;

                Matrix4x4 matrix = Matrix4x4.TRS(
                    box.bone.position,
                    box.bone.rotation * Quaternion.Euler(box.rotationEuler),
                    Vector3.one);

                using (new Handles.DrawingScope(matrix))
                {
                    EditorGUI.BeginChangeCheck();

                    Vector3 newCenter = Handles.PositionHandle(box.localCenter, Quaternion.identity);
                    Quaternion newRot = Handles.RotationHandle(Quaternion.Euler(box.rotationEuler), box.localCenter);
                    Vector3 newSize = Handles.ScaleHandle(box.size, box.localCenter, Quaternion.identity, 1.0f);

                    if (EditorGUI.EndChangeCheck())
                    {
                        Undo.RecordObject(mbv, "Edit Bone Box");
                        box.localCenter = newCenter;
                        box.rotationEuler = newRot.eulerAngles;
                        box.size = newSize;
                    }

                    Handles.DrawWireCube(box.localCenter, box.size);
                }
            }
        }
    }
}
#endif

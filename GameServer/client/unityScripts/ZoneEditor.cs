using UnityEngine;
#if UNITY_EDITOR
using UnityEditor;

// Inspector + scene-handle editor for ZoneMarker. Mirrors the MultiBoundingVolume
// editor: each volume can be edited in the list and, when selected, manipulated
// with position/rotation/scale handles drawn in the marker's local frame.
[CustomEditor(typeof(ZoneMarker))]
public class ZoneMarkerEditor : Editor
{
    private ZoneMarker zm;
    private bool editMode = false;
    private int selectedVolume = -1;

    private void OnEnable()
    {
        zm = (ZoneMarker)target;
    }

    public override void OnInspectorGUI()
    {
        zm.zoneTag  = EditorGUILayout.TextField("Zone Tag", zm.zoneTag);
        zm.factions = (ZoneMarker.FactionFlags)EditorGUILayout.EnumFlagsField("Factions", zm.factions);

        EditorGUILayout.Space();
        editMode = EditorGUILayout.Toggle("Edit Mode (scene handles)", editMode);

        EditorGUILayout.Space();
        EditorGUILayout.LabelField("Volumes", EditorStyles.boldLabel);

        int removeIndex = -1;
        for (int i = 0; i < zm.volumes.Count; i++)
        {
            var v = zm.volumes[i];

            EditorGUILayout.BeginVertical("box");

            EditorGUILayout.BeginHorizontal();
            if (GUILayout.Toggle(selectedVolume == i, "Select", "Button"))
                selectedVolume = i;
            EditorGUILayout.LabelField(v.name == string.Empty ? $"Volume {i} ({v.shape})" : v.name);
            EditorGUILayout.EndHorizontal();

            v.name  = EditorGUILayout.TextField("Name", v.name);
            v.shape = (ZoneMarker.Shape)EditorGUILayout.EnumPopup("Shape", v.shape);
            v.localCenter = EditorGUILayout.Vector3Field("Local Center", v.localCenter);
            if (v.shape == ZoneMarker.Shape.Box)
            {
                v.size          = EditorGUILayout.Vector3Field("Size", v.size);
                v.rotationEuler = EditorGUILayout.Vector3Field("Rotation", v.rotationEuler);
            }
            else
            {
                v.radius = EditorGUILayout.FloatField("Radius", v.radius);
            }

            if (GUILayout.Button("Remove Volume"))
                removeIndex = i;

            EditorGUILayout.EndVertical();
        }

        if (removeIndex >= 0)
        {
            Undo.RecordObject(zm, "Remove Zone Volume");
            zm.volumes.RemoveAt(removeIndex);
            if (selectedVolume == removeIndex) selectedVolume = -1;
        }

        if (GUILayout.Button("Add Volume"))
        {
            Undo.RecordObject(zm, "Add Zone Volume");
            zm.volumes.Add(new ZoneMarker.ZoneVolume());
        }

        if (GUI.changed)
            EditorUtility.SetDirty(zm);
    }

    private void OnSceneGUI()
    {
        zm = (ZoneMarker)target;
        var t = zm.transform;

        for (int i = 0; i < zm.volumes.Count; i++)
        {
            var v = zm.volumes[i];
            bool isSelected = (i == selectedVolume);
            if (editMode && !isSelected) continue;

            Handles.color = isSelected ? Color.yellow : Color.green;

            // Draw in the marker's world frame (position + rotation + scale).
            Matrix4x4 matrix = Matrix4x4.TRS(t.position, t.rotation, t.lossyScale);
            using (new Handles.DrawingScope(matrix))
            {
                if (v.shape == ZoneMarker.Shape.Box)
                {
                    EditorGUI.BeginChangeCheck();
                    Vector3 newCenter = Handles.PositionHandle(v.localCenter, Quaternion.Euler(v.rotationEuler));
                    Quaternion newRot = Handles.RotationHandle(Quaternion.Euler(v.rotationEuler), v.localCenter);
                    Vector3 newSize   = Handles.ScaleHandle(v.size, v.localCenter, Quaternion.Euler(v.rotationEuler), 1.0f);
                    if (EditorGUI.EndChangeCheck())
                    {
                        Undo.RecordObject(zm, "Edit Zone Box");
                        v.localCenter   = newCenter;
                        v.rotationEuler = newRot.eulerAngles;
                        v.size          = newSize;
                    }

                    using (new Handles.DrawingScope(matrix * Matrix4x4.TRS(v.localCenter, Quaternion.Euler(v.rotationEuler), Vector3.one)))
                        Handles.DrawWireCube(Vector3.zero, v.size);
                }
                else
                {
                    EditorGUI.BeginChangeCheck();
                    Vector3 newCenter = Handles.PositionHandle(v.localCenter, Quaternion.identity);
                    float   newRadius = Handles.RadiusHandle(Quaternion.identity, v.localCenter, v.radius);
                    if (EditorGUI.EndChangeCheck())
                    {
                        Undo.RecordObject(zm, "Edit Zone Sphere");
                        v.localCenter = newCenter;
                        v.radius      = newRadius;
                    }
                }
            }
        }
    }
}
#endif

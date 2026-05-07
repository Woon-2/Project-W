using UnityEngine;
using UnityEditor;
using System;
using System.IO;

public class RagdollEditorWindow : EditorWindow
{
    private GoblinRagdollConfig config;
    private Vector2 scrollPos;

    private bool[] bodyFoldouts  = new bool[0];
    private bool[] jointFoldouts = new bool[0];
    private bool showBodies = true;
    private bool showJoints = true;

    private int selectedBodyIdx  = -1;   // scene highlight: body index, -1 = none
    private int selectedJointIdx = -1;   // scene highlight: joint index, -1 = none

    [MenuItem("Tools/Ragdoll Editor")]
    public static void OpenWindow()
    {
        GetWindow<RagdollEditorWindow>("Ragdoll Editor");
    }

    private void OnEnable()  => SceneView.duringSceneGui += OnSceneGUI;
    private void OnDisable() => SceneView.duringSceneGui -= OnSceneGUI;

    // -------------------------------------------------------------------------
    // Main GUI
    // -------------------------------------------------------------------------
    private void OnGUI()
    {
        EditorGUILayout.LabelField("Ragdoll Editor", EditorStyles.boldLabel);
        EditorGUILayout.Space();

        config = (GoblinRagdollConfig)EditorGUILayout.ObjectField(
            "Config Component", config, typeof(GoblinRagdollConfig), true);

        if (config == null)
        {
            EditorGUILayout.HelpBox(
                "Attach a GoblinRagdollConfig component to a scene object and assign it here.",
                MessageType.Info);
            return;
        }

        EditorGUILayout.Space();
        EditorGUI.BeginChangeCheck();
        config.skeletonRoot = (GameObject)EditorGUILayout.ObjectField(
            "Skeleton Root", config.skeletonRoot, typeof(GameObject), true);
        config.mbvSource = (MultiBoundingVolume)EditorGUILayout.ObjectField(
            "MBV Source", config.mbvSource, typeof(MultiBoundingVolume), true);
        if (EditorGUI.EndChangeCheck())
            EditorUtility.SetDirty(config);

        EditorGUILayout.BeginHorizontal();
        using (new EditorGUI.DisabledScope(config.skeletonRoot == null))
        {
            if (GUILayout.Button("Add All Bones"))
                AddAllBonesAsBodies();
        }
        using (new EditorGUI.DisabledScope(config.mbvSource == null))
        {
            if (GUILayout.Button("Import from MBV"))
                ImportFromMBV();
        }
        if (GUILayout.Button("Clear Bodies") && EditorUtility.DisplayDialog(
                "Clear Bodies", "Remove all bodies?", "Yes", "Cancel"))
        {
            Undo.RecordObject(config, "Clear Bodies");
            config.bodies.Clear();
            EditorUtility.SetDirty(config);
        }
        EditorGUILayout.EndHorizontal();

        EditorGUILayout.Space();

        scrollPos = EditorGUILayout.BeginScrollView(scrollPos);
        DrawBodiesSection();
        EditorGUILayout.Space();
        DrawJointsSection();
        EditorGUILayout.EndScrollView();

        EditorGUILayout.Space();
        using (new EditorGUI.DisabledScope(config.bodies.Count == 0 && config.joints.Count == 0))
        {
            if (GUILayout.Button("Export Binary (.bin)", GUILayout.Height(28)))
                ExportBinary();
        }
    }

    // -------------------------------------------------------------------------
    // Bodies  (maps to BoneBoxDef)
    // -------------------------------------------------------------------------
    private void AddAllBonesAsBodies()
    {
        Undo.RecordObject(config, "Add All Bones");
        foreach (Transform bone in config.skeletonRoot.GetComponentsInChildren<Transform>(true))
        {
            if (!config.bodies.Exists(b => b.bone == bone))
                config.bodies.Add(new RagdollBody { name = bone.name, bone = bone });
        }
        EditorUtility.SetDirty(config);
    }

    private void DrawBodiesSection()
    {
        showBodies = EditorGUILayout.Foldout(
            showBodies, $"Bodies  ({config.bodies.Count})", true, EditorStyles.foldoutHeader);
        if (!showBodies) return;

        EnsureArray(ref bodyFoldouts, config.bodies.Count);

        int  removeIdx = -1;
        bool changed   = false;

        for (int i = 0; i < config.bodies.Count; i++)
        {
            var body = config.bodies[i];
            EditorGUILayout.BeginVertical("box");

            EditorGUILayout.BeginHorizontal();
            bool bodyWasOpen = bodyFoldouts[i];
            bodyFoldouts[i] = EditorGUILayout.Foldout(bodyFoldouts[i], $"{i}: {body.name}", true);
            if (bodyFoldouts[i] != bodyWasOpen)
            {
                selectedBodyIdx  = bodyFoldouts[i] ? i : (selectedBodyIdx == i ? -1 : selectedBodyIdx);
                selectedJointIdx = bodyFoldouts[i] ? -1 : selectedJointIdx;
                SceneView.RepaintAll();
            }
            if (GUILayout.Button("Remove", GUILayout.Width(60)))
                removeIdx = i;
            EditorGUILayout.EndHorizontal();

            if (bodyFoldouts[i])
            {
                EditorGUI.indentLevel++;
                EditorGUI.BeginChangeCheck();

                body.name = EditorGUILayout.TextField("Bone Name", body.name);
                body.bone = (Transform)EditorGUILayout.ObjectField(
                    "Bone (scene)", body.bone, typeof(Transform), true);

                EditorGUILayout.LabelField("Box (bone-local)", EditorStyles.miniLabel);
                body.center        = EditorGUILayout.Vector3Field("Center", body.center);
                body.halfExtents   = Vector3.Max(Vector3.one * 0.001f,
                    EditorGUILayout.Vector3Field("Half Extents", body.halfExtents));
                body.rotationEuler = EditorGUILayout.Vector3Field("Rotation", body.rotationEuler);

                EditorGUILayout.LabelField("Physics", EditorStyles.miniLabel);
                body.mass = Mathf.Max(0.001f, EditorGUILayout.FloatField("Mass", body.mass));

                if (EditorGUI.EndChangeCheck()) changed = true;
                EditorGUI.indentLevel--;
            }

            EditorGUILayout.EndVertical();
        }

        if (removeIdx >= 0)
        {
            Undo.RecordObject(config, "Remove Body");
            config.bodies.RemoveAt(removeIdx);
            changed = true;
        }

        if (GUILayout.Button("+ Add Body"))
        {
            Undo.RecordObject(config, "Add Body");
            config.bodies.Add(new RagdollBody());
            changed = true;
        }

        if (changed) EditorUtility.SetDirty(config);
    }

    // -------------------------------------------------------------------------
    // Joints  (maps to JointDef)
    // -------------------------------------------------------------------------
    private void DrawJointsSection()
    {
        showJoints = EditorGUILayout.Foldout(
            showJoints, $"Joints  ({config.joints.Count})", true, EditorStyles.foldoutHeader);
        if (!showJoints) return;

        EnsureArray(ref jointFoldouts, config.joints.Count);

        string[] names = config.bodies.Count > 0
            ? config.bodies.ConvertAll(b => b.name).ToArray()
            : new[] { "(no bodies)" };

        int  removeIdx = -1;
        bool changed   = false;

        for (int i = 0; i < config.joints.Count; i++)
        {
            var joint = config.joints[i];
            EditorGUILayout.BeginVertical("box");

            EditorGUILayout.BeginHorizontal();
            bool jointWasOpen = jointFoldouts[i];
            jointFoldouts[i] = EditorGUILayout.Foldout(
                jointFoldouts[i],
                $"{i}: [{joint.jointType}]  {joint.parentBoneName} → {joint.childBoneName}", true);
            if (jointFoldouts[i] != jointWasOpen)
            {
                selectedJointIdx = jointFoldouts[i] ? i : (selectedJointIdx == i ? -1 : selectedJointIdx);
                selectedBodyIdx  = jointFoldouts[i] ? -1 : selectedBodyIdx;
                SceneView.RepaintAll();
            }
            if (GUILayout.Button("Remove", GUILayout.Width(60)))
                removeIdx = i;
            EditorGUILayout.EndHorizontal();

            if (jointFoldouts[i])
            {
                EditorGUI.indentLevel++;
                EditorGUI.BeginChangeCheck();

                // Parent / child dropdowns
                int pi = Mathf.Max(0, Array.IndexOf(names, joint.parentBoneName));
                int ci = Mathf.Max(0, Array.IndexOf(names, joint.childBoneName));
                pi = EditorGUILayout.Popup("Parent Body", pi, names);
                ci = EditorGUILayout.Popup("Child Body",  ci, names);
                if (config.bodies.Count > 0)
                {
                    joint.parentBoneName = names[pi];
                    joint.childBoneName  = names[ci];
                }

                joint.jointType = (RagdollJointType)EditorGUILayout.EnumPopup("Joint Type", joint.jointType);
                EditorGUILayout.Space(2);

                switch (joint.jointType)
                {
                    case RagdollJointType.BallSocket:
                        EditorGUILayout.HelpBox(
                            "BallSocket: 3 translational DOFs constrained. No angle limits.",
                            MessageType.None);
                        break;

                    case RagdollJointType.Hinge:
                        EditorGUILayout.LabelField("Hinge", EditorStyles.miniLabel);
                        joint.hingeAxisLocal = EditorGUILayout.Vector3Field(
                            "Axis (parent-bone local)", joint.hingeAxisLocal);
                        // Preset axis buttons
                        EditorGUILayout.BeginHorizontal();
                        EditorGUILayout.PrefixLabel(" ");
                        if (GUILayout.Button("+X")) joint.hingeAxisLocal = Vector3.right;
                        if (GUILayout.Button("+Y")) joint.hingeAxisLocal = Vector3.up;
                        if (GUILayout.Button("+Z")) joint.hingeAxisLocal = Vector3.forward;
                        if (GUILayout.Button("-X")) joint.hingeAxisLocal = Vector3.left;
                        if (GUILayout.Button("-Y")) joint.hingeAxisLocal = Vector3.down;
                        if (GUILayout.Button("-Z")) joint.hingeAxisLocal = Vector3.back;
                        EditorGUILayout.EndHorizontal();

                        EditorGUILayout.Space(2);
                        EditorGUILayout.LabelField("Angle Limits (deg)", EditorStyles.miniLabel);
                        EditorGUILayout.BeginHorizontal();
                        EditorGUILayout.LabelField("Min", GUILayout.Width(28));
                        joint.minAngleDeg = EditorGUILayout.FloatField(joint.minAngleDeg, GUILayout.Width(52));
                        EditorGUILayout.LabelField("Max", GUILayout.Width(28));
                        joint.maxAngleDeg = EditorGUILayout.FloatField(joint.maxAngleDeg, GUILayout.Width(52));
                        EditorGUILayout.EndHorizontal();
                        EditorGUILayout.MinMaxSlider(
                            ref joint.minAngleDeg, ref joint.maxAngleDeg, -180f, 180f);
                        EditorGUILayout.LabelField(
                            $"  [{joint.minAngleDeg:F1}°  →  {joint.maxAngleDeg:F1}°]",
                            EditorStyles.miniLabel);
                        break;

                    case RagdollJointType.ConeTwist:
                        EditorGUILayout.LabelField("ConeTwist", EditorStyles.miniLabel);
                        joint.coneHalfAngleDeg = EditorGUILayout.Slider(
                            "Cone Half-Angle", joint.coneHalfAngleDeg, 0f, 153f);  // clamped to pi*0.85 internally
                        joint.twistLimitDeg = EditorGUILayout.Slider(
                            "Twist Limit (±)", joint.twistLimitDeg, 0f, 180f);
                        EditorGUILayout.LabelField(
                            $"  cone ±{joint.coneHalfAngleDeg:F1}°   twist ±{joint.twistLimitDeg:F1}°",
                            EditorStyles.miniLabel);
                        break;
                }

                if (EditorGUI.EndChangeCheck()) changed = true;
                EditorGUI.indentLevel--;
            }

            EditorGUILayout.EndVertical();
        }

        if (removeIdx >= 0)
        {
            Undo.RecordObject(config, "Remove Joint");
            config.joints.RemoveAt(removeIdx);
            changed = true;
        }

        if (GUILayout.Button("+ Add Joint"))
        {
            Undo.RecordObject(config, "Add Joint");
            config.joints.Add(new RagdollJoint());
            changed = true;
        }

        if (changed) EditorUtility.SetDirty(config);
    }

    // -------------------------------------------------------------------------
    // MBV Import  (highest LOD = most detailed = best for physics)
    // -------------------------------------------------------------------------
    private void ImportFromMBV()
    {
        var mbv = config.mbvSource;
        if (mbv == null || mbv.lods.Count == 0) return;

        var lod = mbv.lods[mbv.lods.Count - 1];

        Undo.RecordObject(config, "Import from MBV");
        foreach (var box in lod.boxes)
        {
            if (config.bodies.Exists(b => b.name == box.name)) continue;
            config.bodies.Add(new RagdollBody
            {
                name          = box.name,
                bone          = box.bone,
                center        = box.localCenter,
                halfExtents   = box.size * 0.5f,
                rotationEuler = box.rotationEuler,
                mass          = 1.0f,
            });
        }
        EditorUtility.SetDirty(config);
    }

    // -------------------------------------------------------------------------
    // Scene gizmos
    // -------------------------------------------------------------------------
    private void OnSceneGUI(SceneView sv)
    {
        if (config == null) return;

        // Determine which body names the selected joint references.
        string jointParentName = "";
        string jointChildName  = "";
        if (selectedJointIdx >= 0 && selectedJointIdx < config.joints.Count)
        {
            var sj = config.joints[selectedJointIdx];
            jointParentName = sj.parentBoneName;
            jointChildName  = sj.childBoneName;
        }

        bool anySelection = selectedBodyIdx >= 0 || selectedJointIdx >= 0;

        // Bodies: draw OBB wireframes with highlight
        for (int i = 0; i < config.bodies.Count; i++)
        {
            var body = config.bodies[i];
            if (body.bone == null) continue;

            Color c;
            if (i == selectedBodyIdx)
                c = Color.yellow;                                       // body selected
            else if (body.name == jointParentName)
                c = new Color(1f, 0.55f, 0.1f, 1f);                   // joint parent: orange
            else if (body.name == jointChildName)
                c = new Color(0.2f, 0.9f, 1f, 1f);                    // joint child:  cyan
            else
                c = new Color(0.3f, 1f, 0.3f, anySelection ? 0.2f : 0.85f); // dim when something else is selected

            Handles.color = c;
            Matrix4x4 mat = Matrix4x4.TRS(
                body.bone.position,
                body.bone.rotation * Quaternion.Euler(body.rotationEuler),
                Vector3.one);
            using (new Handles.DrawingScope(mat))
                Handles.DrawWireCube(body.center, body.halfExtents * 2f);
        }

        // Joints: anchor point + axis/limit visualization
        foreach (var joint in config.joints)
        {
            var parentBody = config.bodies.Find(b => b.name == joint.parentBoneName);
            var childBody  = config.bodies.Find(b => b.name == joint.childBoneName);
            if (parentBody?.bone == null || childBody?.bone == null) continue;

            // Approximate anchor: child bone origin (C++ computes this from T-pose)
            Vector3 anchor = childBody.bone.position;

            switch (joint.jointType)
            {
                case RagdollJointType.BallSocket:
                    Handles.color = Color.cyan;
                    Handles.SphereHandleCap(0, anchor, Quaternion.identity, 0.03f, EventType.Repaint);
                    break;

                case RagdollJointType.Hinge:
                {
                    Vector3 worldAxis = parentBody.bone.TransformDirection(joint.hingeAxisLocal).normalized;
                    Handles.color = new Color(1f, 0.8f, 0.2f, 0.9f);
                    Handles.DrawLine(anchor - worldAxis * 0.1f, anchor + worldAxis * 0.1f);

                    // Draw arc showing angle range
                    Vector3 perp = Vector3.Cross(worldAxis, Vector3.up);
                    if (perp.sqrMagnitude < 0.01f) perp = Vector3.Cross(worldAxis, Vector3.right);
                    perp.Normalize();
                    float range = joint.maxAngleDeg - joint.minAngleDeg;
                    if (Mathf.Abs(range) > 0.5f)
                    {
                        Handles.color = new Color(1f, 0.8f, 0.2f, 0.3f);
                        Vector3 fromDir = Quaternion.AngleAxis(joint.minAngleDeg, worldAxis) * perp;
                        Handles.DrawSolidArc(anchor, worldAxis, fromDir, range, 0.08f);
                    }
                    break;
                }

                case RagdollJointType.ConeTwist:
                {
                    // Draw cone opening and twist disc
                    Vector3 fwd = (anchor - parentBody.bone.position).normalized;
                    if (fwd.sqrMagnitude < 0.001f) fwd = parentBody.bone.forward;

                    Handles.color = new Color(0.5f, 0.7f, 1f, 0.3f);
                    if (joint.coneHalfAngleDeg > 0.5f)
                    {
                        Vector3 perp = Vector3.Cross(fwd, Vector3.up).normalized;
                        if (perp.sqrMagnitude < 0.01f) perp = Vector3.Cross(fwd, Vector3.right).normalized;
                        Handles.DrawSolidArc(anchor, perp, fwd, joint.coneHalfAngleDeg,  0.07f);
                        Handles.DrawSolidArc(anchor, perp, fwd, -joint.coneHalfAngleDeg, 0.07f);
                    }
                    if (joint.twistLimitDeg > 0.5f)
                    {
                        Handles.color = new Color(1f, 0.55f, 0.2f, 0.3f);
                        Vector3 up = Vector3.Cross(fwd, parentBody.bone.right).normalized;
                        Handles.DrawSolidArc(anchor, fwd, up,  joint.twistLimitDeg, 0.05f);
                        Handles.DrawSolidArc(anchor, fwd, up, -joint.twistLimitDeg, 0.05f);
                    }
                    break;
                }
            }
        }

        Handles.color  = Color.white;
        Handles.matrix = Matrix4x4.identity;
    }

    // -------------------------------------------------------------------------
    // Binary export  —  mirrors BoneCapsuleDef / JointDef in ragdollDef.hpp
    // -------------------------------------------------------------------------
    private void ExportBinary()
    {
        string path = EditorUtility.SaveFilePanel(
            "Export Ragdoll Config", "ExportedAssets", "ragdoll.bin", "bin");
        if (string.IsNullOrEmpty(path)) return;

        using (var w = new BinaryWriter(File.Open(path, FileMode.Create)))
        {
            ExtractUtil.WriteHeadTag(w, "RagdollConfig");

            // --- BoneBoxDefs ---
            ExtractUtil.WriteInteger(w, "BodyCnt", config.bodies.Count);
            foreach (var body in config.bodies)
            {
                ExtractUtil.WriteHeadTag(w, "Body");
                ExtractUtil.WriteText(w,   "BoneName",    body.name);
                ExtractUtil.WriteVector(w, "HalfExtents", body.halfExtents);
                ExtractUtil.WriteVector(w, "Center",      body.center);
                ExtractUtil.WriteVector(w, "RotEuler",    body.rotationEuler);
                ExtractUtil.WriteFloat(w,  "Mass",        body.mass);
                ExtractUtil.WriteTailTag(w, "Body");
            }

            // --- JointDefs ---
            ExtractUtil.WriteInteger(w, "JointCnt", config.joints.Count);
            foreach (var joint in config.joints)
            {
                ExtractUtil.WriteHeadTag(w, "Joint");
                ExtractUtil.WriteText(w,    "ParentBone", joint.parentBoneName);
                ExtractUtil.WriteText(w,    "ChildBone",  joint.childBoneName);
                ExtractUtil.WriteText(w,    "JointType",  joint.jointType.ToString());

                switch (joint.jointType)
                {
                    case RagdollJointType.Hinge:
                        ExtractUtil.WriteVector(w, "AxisLocalA", joint.hingeAxisLocal);
                        ExtractUtil.WriteFloat(w,  "MinAngle",   Mathf.Deg2Rad * joint.minAngleDeg);
                        ExtractUtil.WriteFloat(w,  "MaxAngle",   Mathf.Deg2Rad * joint.maxAngleDeg);
                        break;

                    case RagdollJointType.ConeTwist:
                        ExtractUtil.WriteFloat(w, "ConeHalfAngle", Mathf.Deg2Rad * joint.coneHalfAngleDeg);
                        ExtractUtil.WriteFloat(w, "TwistLimit",    Mathf.Deg2Rad * joint.twistLimitDeg);
                        break;

                    // BallSocket: no extra params
                }

                ExtractUtil.WriteTailTag(w, "Joint");
            }

            ExtractUtil.WriteTailTag(w, "RagdollConfig");
        }

        Debug.Log($"[RagdollEditor] Exported {config.bodies.Count} bodies, {config.joints.Count} joints → {path}");
    }

    // -------------------------------------------------------------------------
    // Utility
    // -------------------------------------------------------------------------
    private static void EnsureArray(ref bool[] arr, int count)
    {
        if (arr == null || arr.Length != count)
            Array.Resize(ref arr, count);
    }
}

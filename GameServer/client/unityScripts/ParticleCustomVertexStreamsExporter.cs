// Unity Editor script: Tools > Export Particle Custom Vertex Streams
// Dumps ParticleSystemRenderer Custom Vertex Streams and ParticleSystem Custom Data
// module settings to JSON. Useful when porting particle Shader Graph effects.

#if UNITY_EDITOR

using System;
using System.Collections.Generic;
using System.IO;
using UnityEditor;
using UnityEngine;

public class ParticleCustomVertexStreamsExporter : EditorWindow {

    private GameObject targetRoot;
    private bool       includeChildren = true;
    private bool       includeInactive = true;
    private bool       exportLoadedSceneWhenNoRoot = false;
    private string     outputPath = "";

    [MenuItem("Tools/Export Particle Custom Vertex Streams")]
    public static void ShowWindow() {
        GetWindow<ParticleCustomVertexStreamsExporter>("Export Particle Streams");
    }

    private void OnGUI() {
        GUILayout.Label("Particle Custom Vertex Streams Exporter", EditorStyles.boldLabel);
        EditorGUILayout.Space();

        targetRoot = (GameObject)EditorGUILayout.ObjectField(
            "Target Root", targetRoot, typeof(GameObject), true);

        includeChildren = EditorGUILayout.Toggle("Include Children", includeChildren);
        includeInactive = EditorGUILayout.Toggle("Include Inactive", includeInactive);
        exportLoadedSceneWhenNoRoot = EditorGUILayout.Toggle(
            "Scene If No Root", exportLoadedSceneWhenNoRoot);

        EditorGUILayout.Space();

        if (GUILayout.Button("Use Selected GameObject")) {
            targetRoot = Selection.activeGameObject;
            if (targetRoot == null)
                Debug.LogWarning("ParticleCustomVertexStreamsExporter: no active GameObject selected.");
        }

        EditorGUILayout.Space();

        if (GUILayout.Button("Choose Output File")) {
            string defaultName = targetRoot != null
                ? SanitizeFileName(targetRoot.name) + "_ParticleStreams"
                : "ParticleStreams";

            outputPath = EditorUtility.SaveFilePanel(
                "Save particle custom vertex streams JSON",
                Application.dataPath,
                defaultName,
                "json");
        }

        if (!string.IsNullOrEmpty(outputPath))
            EditorGUILayout.LabelField("Output:", outputPath);

        EditorGUILayout.Space();

        bool canExport = !string.IsNullOrEmpty(outputPath)
                      && (targetRoot != null || exportLoadedSceneWhenNoRoot);

        GUI.enabled = canExport;
        if (GUILayout.Button("Export JSON")) {
            Export(targetRoot, outputPath, includeChildren, includeInactive, exportLoadedSceneWhenNoRoot);
        }
        GUI.enabled = true;

        if (!canExport) {
            EditorGUILayout.HelpBox(
                "Choose a Target Root, or enable Scene If No Root, then choose an output file.",
                MessageType.Info);
        }
    }

    public static void Export(GameObject root, string path, bool includeChildren = true,
                              bool includeInactive = true, bool exportLoadedSceneWhenNoRoot = false) {
        List<ParticleSystemRenderer> renderers = FindParticleRenderers(
            root, includeChildren, includeInactive, exportLoadedSceneWhenNoRoot);

        var dump = new ExportDump {
            unityVersion = Application.unityVersion,
            exportedAtLocal = DateTime.Now.ToString("yyyy-MM-dd HH:mm:ss"),
            rootPath = root != null ? GetHierarchyPath(root.transform) : "",
            includeChildren = includeChildren,
            includeInactive = includeInactive,
            exportLoadedSceneWhenNoRoot = exportLoadedSceneWhenNoRoot,
            rendererCount = renderers.Count,
            renderers = new List<RendererDump>()
        };

        foreach (ParticleSystemRenderer renderer in renderers) {
            if (renderer == null)
                continue;

            dump.renderers.Add(DumpRenderer(renderer));
        }

        string json = JsonUtility.ToJson(dump, true);
        File.WriteAllText(path, json);

        Debug.Log($"ParticleCustomVertexStreamsExporter: exported {dump.renderers.Count} renderer(s) -> {path}");
    }

    private static List<ParticleSystemRenderer> FindParticleRenderers(
        GameObject root, bool includeChildren, bool includeInactive, bool exportLoadedSceneWhenNoRoot) {

        var ret = new List<ParticleSystemRenderer>();

        if (root != null) {
            if (includeChildren) {
                ret.AddRange(root.GetComponentsInChildren<ParticleSystemRenderer>(includeInactive));
            } else {
                ParticleSystemRenderer renderer = root.GetComponent<ParticleSystemRenderer>();
                if (renderer != null)
                    ret.Add(renderer);
            }

            return ret;
        }

        if (!exportLoadedSceneWhenNoRoot)
            return ret;

        ParticleSystemRenderer[] allRenderers = Resources.FindObjectsOfTypeAll<ParticleSystemRenderer>();
        foreach (ParticleSystemRenderer renderer in allRenderers) {
            if (renderer == null)
                continue;

            GameObject go = renderer.gameObject;
            if (go == null)
                continue;

            if (EditorUtility.IsPersistent(go))
                continue;

            if (!go.scene.IsValid() || !go.scene.isLoaded)
                continue;

            if (!includeInactive && !go.activeInHierarchy)
                continue;

            ret.Add(renderer);
        }

        ret.Sort((a, b) => string.Compare(
            GetHierarchyPath(a.transform), GetHierarchyPath(b.transform), StringComparison.Ordinal));

        return ret;
    }

    private static RendererDump DumpRenderer(ParticleSystemRenderer renderer) {
        var activeStreams = new List<ParticleSystemVertexStream>();
        renderer.GetActiveVertexStreams(activeStreams);

        ParticleSystem particleSystem = renderer.GetComponent<ParticleSystem>();

        var dump = new RendererDump {
            gameObjectName = renderer.gameObject.name,
            hierarchyPath = GetHierarchyPath(renderer.transform),
            activeInHierarchy = renderer.gameObject.activeInHierarchy,
            rendererEnabled = renderer.enabled,
            renderMode = renderer.renderMode.ToString(),
            alignment = renderer.alignment.ToString(),
            sortMode = renderer.sortMode.ToString(),
            normalDirection = renderer.normalDirection,
            minParticleSize = renderer.minParticleSize,
            maxParticleSize = renderer.maxParticleSize,
            lengthScale = renderer.lengthScale,
            velocityScale = renderer.velocityScale,
            cameraVelocityScale = renderer.cameraVelocityScale,
            pivot = Vec3Dump.From(renderer.pivot),
            meshName = renderer.mesh != null ? renderer.mesh.name : "",
            materialNames = DumpMaterials(renderer.sharedMaterials),
            activeVertexStreams = new List<VertexStreamDump>(),
            particleSystem = particleSystem != null ? DumpParticleSystem(particleSystem) : null
        };

        for (int i = 0; i < activeStreams.Count; ++i) {
            ParticleSystemVertexStream stream = activeStreams[i];
            dump.activeVertexStreams.Add(new VertexStreamDump {
                index = i,
                name = stream.ToString(),
                enumValue = (int)stream,
                semanticHint = GuessSemantic(stream)
            });
        }

        return dump;
    }

    private static ParticleSystemDump DumpParticleSystem(ParticleSystem particleSystem) {
        ParticleSystem.MainModule main = particleSystem.main;
        ParticleSystem.CustomDataModule customData = particleSystem.customData;

        return new ParticleSystemDump {
            particleSystemName = particleSystem.name,
            duration = main.duration,
            looping = main.loop,
            startLifetime = MinMaxCurveDump.From(main.startLifetime),
            startSpeed = MinMaxCurveDump.From(main.startSpeed),
            startSize = MinMaxCurveDump.From(main.startSize),
            startColor = MinMaxGradientDump.From(main.startColor),
            customData = DumpCustomData(customData)
        };
    }

    private static CustomDataModuleDump DumpCustomData(ParticleSystem.CustomDataModule customData) {
        var ret = new CustomDataModuleDump {
            enabled = customData.enabled,
            custom1 = DumpCustomDataStream(customData, ParticleSystemCustomData.Custom1),
            custom2 = DumpCustomDataStream(customData, ParticleSystemCustomData.Custom2)
        };

        return ret;
    }

    private static CustomDataStreamDump DumpCustomDataStream(
        ParticleSystem.CustomDataModule customData, ParticleSystemCustomData stream) {

        ParticleSystemCustomDataMode mode = customData.GetMode(stream);
        var ret = new CustomDataStreamDump {
            stream = stream.ToString(),
            mode = mode.ToString(),
            vectorComponentCount = customData.GetVectorComponentCount(stream),
            vectorComponents = new List<MinMaxCurveDump>()
        };

        if (mode == ParticleSystemCustomDataMode.Vector) {
            for (int component = 0; component < ret.vectorComponentCount; ++component) {
                ret.vectorComponents.Add(MinMaxCurveDump.From(customData.GetVector(stream, component)));
            }
        } else if (mode == ParticleSystemCustomDataMode.Color) {
            ret.color = MinMaxGradientDump.From(customData.GetColor(stream));
        }

        return ret;
    }

    private static List<string> DumpMaterials(Material[] materials) {
        var ret = new List<string>();
        if (materials == null)
            return ret;

        foreach (Material material in materials)
            ret.Add(material != null ? material.name : "");

        return ret;
    }

    private static string GuessSemantic(ParticleSystemVertexStream stream) {
        string name = stream.ToString();

        if (name == "Position") return "POSITION.xyz";
        if (name == "Normal") return "NORMAL.xyz";
        if (name == "Tangent") return "TANGENT.xyzw";
        if (name == "Color") return "COLOR.xyzw";
        if (name == "UV") return "TEXCOORD0.xy";

        if (name.StartsWith("UV", StringComparison.Ordinal))
            return "TEXCOORD stream; verify packing in Unity shader input signature";

        if (name.StartsWith("Custom1", StringComparison.Ordinal))
            return "Custom1; usually packed into TEXCOORD stream";

        if (name.StartsWith("Custom2", StringComparison.Ordinal))
            return "Custom2; usually packed into TEXCOORD stream";

        if (name.Contains("Lifetime") || name.Contains("Random") || name.Contains("Anim") ||
            name.Contains("Velocity") || name.Contains("Size") || name.Contains("Rotation")) {
            return "Particle stream; Unity packs according to Custom Vertex Streams order";
        }

        return "";
    }

    private static string GetHierarchyPath(Transform transform) {
        if (transform == null)
            return "";

        string path = transform.name;
        Transform current = transform.parent;
        while (current != null) {
            path = current.name + "/" + path;
            current = current.parent;
        }

        return path;
    }

    private static string SanitizeFileName(string name) {
        foreach (char c in Path.GetInvalidFileNameChars())
            name = name.Replace(c, '_');

        return name;
    }

    [Serializable]
    private class ExportDump {
        public string unityVersion;
        public string exportedAtLocal;
        public string rootPath;
        public bool includeChildren;
        public bool includeInactive;
        public bool exportLoadedSceneWhenNoRoot;
        public int rendererCount;
        public List<RendererDump> renderers;
    }

    [Serializable]
    private class RendererDump {
        public string gameObjectName;
        public string hierarchyPath;
        public bool activeInHierarchy;
        public bool rendererEnabled;
        public string renderMode;
        public string alignment;
        public string sortMode;
        public float normalDirection;
        public float minParticleSize;
        public float maxParticleSize;
        public float lengthScale;
        public float velocityScale;
        public float cameraVelocityScale;
        public Vec3Dump pivot;
        public string meshName;
        public List<string> materialNames;
        public List<VertexStreamDump> activeVertexStreams;
        public ParticleSystemDump particleSystem;
    }

    [Serializable]
    private class VertexStreamDump {
        public int index;
        public string name;
        public int enumValue;
        public string semanticHint;
    }

    [Serializable]
    private class ParticleSystemDump {
        public string particleSystemName;
        public float duration;
        public bool looping;
        public MinMaxCurveDump startLifetime;
        public MinMaxCurveDump startSpeed;
        public MinMaxCurveDump startSize;
        public MinMaxGradientDump startColor;
        public CustomDataModuleDump customData;
    }

    [Serializable]
    private class CustomDataModuleDump {
        public bool enabled;
        public CustomDataStreamDump custom1;
        public CustomDataStreamDump custom2;
    }

    [Serializable]
    private class CustomDataStreamDump {
        public string stream;
        public string mode;
        public int vectorComponentCount;
        public List<MinMaxCurveDump> vectorComponents;
        public MinMaxGradientDump color;
    }

    [Serializable]
    private class MinMaxCurveDump {
        public string mode;
        public float curveMultiplier;
        public float constant;
        public float constantMin;
        public float constantMax;
        public CurveDump curve;
        public CurveDump curveMin;
        public CurveDump curveMax;

        public static MinMaxCurveDump From(ParticleSystem.MinMaxCurve value) {
            return new MinMaxCurveDump {
                mode = value.mode.ToString(),
                curveMultiplier = value.curveMultiplier,
                constant = value.constant,
                constantMin = value.constantMin,
                constantMax = value.constantMax,
                curve = CurveDump.From(value.curve),
                curveMin = CurveDump.From(value.curveMin),
                curveMax = CurveDump.From(value.curveMax)
            };
        }
    }

    [Serializable]
    private class CurveDump {
        public List<KeyframeDump> keys = new List<KeyframeDump>();

        public static CurveDump From(AnimationCurve curve) {
            var ret = new CurveDump();
            if (curve == null)
                return ret;

            foreach (Keyframe key in curve.keys)
                ret.keys.Add(KeyframeDump.From(key));

            return ret;
        }
    }

    [Serializable]
    private class KeyframeDump {
        public float time;
        public float value;
        public float inTangent;
        public float outTangent;

        public static KeyframeDump From(Keyframe key) {
            return new KeyframeDump {
                time = key.time,
                value = key.value,
                inTangent = key.inTangent,
                outTangent = key.outTangent
            };
        }
    }

    [Serializable]
    private class MinMaxGradientDump {
        public string mode;
        public ColorDump color;
        public ColorDump colorMin;
        public ColorDump colorMax;
        public GradientDump gradient;
        public GradientDump gradientMin;
        public GradientDump gradientMax;

        public static MinMaxGradientDump From(ParticleSystem.MinMaxGradient value) {
            return new MinMaxGradientDump {
                mode = value.mode.ToString(),
                color = ColorDump.From(value.color),
                colorMin = ColorDump.From(value.colorMin),
                colorMax = ColorDump.From(value.colorMax),
                gradient = GradientDump.From(value.gradient),
                gradientMin = GradientDump.From(value.gradientMin),
                gradientMax = GradientDump.From(value.gradientMax)
            };
        }
    }

    [Serializable]
    private class GradientDump {
        public List<GradientColorKeyDump> colorKeys = new List<GradientColorKeyDump>();
        public List<GradientAlphaKeyDump> alphaKeys = new List<GradientAlphaKeyDump>();

        public static GradientDump From(Gradient gradient) {
            var ret = new GradientDump();
            if (gradient == null)
                return ret;

            foreach (GradientColorKey key in gradient.colorKeys)
                ret.colorKeys.Add(GradientColorKeyDump.From(key));

            foreach (GradientAlphaKey key in gradient.alphaKeys)
                ret.alphaKeys.Add(GradientAlphaKeyDump.From(key));

            return ret;
        }
    }

    [Serializable]
    private class GradientColorKeyDump {
        public ColorDump color;
        public float time;

        public static GradientColorKeyDump From(GradientColorKey key) {
            return new GradientColorKeyDump {
                color = ColorDump.From(key.color),
                time = key.time
            };
        }
    }

    [Serializable]
    private class GradientAlphaKeyDump {
        public float alpha;
        public float time;

        public static GradientAlphaKeyDump From(GradientAlphaKey key) {
            return new GradientAlphaKeyDump {
                alpha = key.alpha,
                time = key.time
            };
        }
    }

    [Serializable]
    private class ColorDump {
        public float r;
        public float g;
        public float b;
        public float a;

        public static ColorDump From(Color value) {
            return new ColorDump {
                r = value.r,
                g = value.g,
                b = value.b,
                a = value.a
            };
        }
    }

    [Serializable]
    private class Vec3Dump {
        public float x;
        public float y;
        public float z;

        public static Vec3Dump From(Vector3 value) {
            return new Vec3Dump {
                x = value.x,
                y = value.y,
                z = value.z
            };
        }
    }
}

#endif // UNITY_EDITOR

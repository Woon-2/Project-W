#if UNITY_EDITOR
using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using UnityEditor;
using UnityEngine;

public static class ParticleSystemPropertyExporter
{
    private const string MenuPath = "Tools/Effects/Export Selected Particle Systems To JSON";

    [MenuItem(MenuPath)]
    private static void ExportSelected()
    {
        GameObject root = Selection.activeGameObject;

        if (root == null)
        {
            EditorUtility.DisplayDialog("Particle Exporter", "파티클 시스템을 포함한 오브젝트를 선택하세요.", "OK");
            return;
        }

        ParticleSystem[] particleSystems = root.GetComponentsInChildren<ParticleSystem>(true);

        if (particleSystems.Length == 0)
        {
            EditorUtility.DisplayDialog("Particle Exporter", "선택한 오브젝트 아래에 ParticleSystem이 없습니다.", "OK");
            return;
        }

        var export = new ParticleSystemRootExport
        {
            unityVersion = Application.unityVersion,
            exportedAtLocal = DateTime.Now.ToString("yyyy-MM-dd HH:mm:ss"),
            rootName = root.name,
            rootScene = root.scene.IsValid() ? root.scene.name : string.Empty,
            systems = new List<ParticleSystemExport>()
        };

        foreach (ParticleSystem ps in particleSystems)
        {
            export.systems.Add(CaptureParticleSystem(root.transform, ps));
        }

        string defaultName = SanitizeFileName(root.name) + "_ParticleSystems.json";
        string savePath = EditorUtility.SaveFilePanel(
            "Export Particle Systems",
            Application.dataPath,
            defaultName,
            "json"
        );

        if (string.IsNullOrEmpty(savePath))
            return;

        string json = JsonUtility.ToJson(export, true);
        File.WriteAllText(savePath, json);

        AssetDatabase.Refresh();
        EditorUtility.RevealInFinder(savePath);

        EditorUtility.DisplayDialog(
            "Particle Exporter",
            particleSystems.Length + "개의 ParticleSystem 속성을 추출했습니다.\n\n" + savePath,
            "OK"
        );
    }

    [MenuItem(MenuPath, true)]
    private static bool ValidateExportSelected()
    {
        return Selection.activeGameObject != null;
    }

    private static ParticleSystemExport CaptureParticleSystem(Transform root, ParticleSystem ps)
    {
        ParticleSystemRenderer renderer = ps.GetComponent<ParticleSystemRenderer>();

        return new ParticleSystemExport
        {
            name = ps.name,
            relativePath = GetRelativePath(root, ps.transform),
            activeSelf = ps.gameObject.activeSelf,
            activeInHierarchy = ps.gameObject.activeInHierarchy,
            transform = CaptureTransform(ps.transform),
            summary = CaptureSummary(ps, renderer),
            particleSystemSerializedProperties = DumpSerializedObject(ps),
            rendererSerializedProperties = renderer != null
                ? DumpSerializedObject(renderer)
                : new List<SerializedPropertyExport>()
        };
    }

    private static ParticleSystemSummary CaptureSummary(ParticleSystem ps, ParticleSystemRenderer renderer)
    {
        ParticleSystem.MainModule main = ps.main;
        ParticleSystem.EmissionModule emission = ps.emission;
        ParticleSystem.ShapeModule shape = ps.shape;
        ParticleSystem.RotationOverLifetimeModule rotationOverLifetime = ps.rotationOverLifetime;
        ParticleSystem.ColorOverLifetimeModule colorOverLifetime = ps.colorOverLifetime;
        ParticleSystem.SizeOverLifetimeModule sizeOverLifetime = ps.sizeOverLifetime;
        ParticleSystem.CustomDataModule customDataModule = ps.customData;

        var summary = new ParticleSystemSummary
        {
            main = new MainModuleExport
            {
                duration = main.duration,
                loop = main.loop,
                prewarm = main.prewarm,
                startDelay = Curve(main.startDelay),
                startLifetime = Curve(main.startLifetime),
                startSpeed = Curve(main.startSpeed),
                startSize3D = main.startSize3D,
                startSize = Curve(main.startSize),
                startSizeX = Curve(main.startSizeX),
                startSizeY = Curve(main.startSizeY),
                startSizeZ = Curve(main.startSizeZ),
                startRotation3D = main.startRotation3D,
                startRotation = Curve(main.startRotation),
                startRotationX = Curve(main.startRotationX),
                startRotationY = Curve(main.startRotationY),
                startRotationZ = Curve(main.startRotationZ),
                flipRotation = main.flipRotation,
                startColor = Gradient(main.startColor),
                gravityModifier = Curve(main.gravityModifier),
                simulationSpace = main.simulationSpace.ToString(),
                customSimulationSpace = main.customSimulationSpace != null ? main.customSimulationSpace.name : string.Empty,
                simulationSpeed = main.simulationSpeed,
                scalingMode = main.scalingMode.ToString(),
                playOnAwake = main.playOnAwake,
                maxParticles = main.maxParticles
            },

            emission = new EmissionModuleExport
            {
                enabled = emission.enabled,
                rateOverTime = Curve(emission.rateOverTime),
                rateOverDistance = Curve(emission.rateOverDistance),
                bursts = CaptureBursts(emission)
            },

            shape = new ShapeModuleExport
            {
                enabled = shape.enabled,
                shapeType = shape.shapeType.ToString(),
                angle = shape.angle,
                radius = shape.radius,
                radiusThickness = shape.radiusThickness,
                arc = shape.arc,
                position = shape.position,
                rotation = shape.rotation,
                scale = shape.scale,
                alignToDirection = shape.alignToDirection,
                randomDirectionAmount = shape.randomDirectionAmount,
                sphericalDirectionAmount = shape.sphericalDirectionAmount,
                randomPositionAmount = shape.randomPositionAmount,
                mesh = ObjectRef(shape.mesh),
                meshRenderer = ObjectRef(shape.meshRenderer),
                skinnedMeshRenderer = ObjectRef(shape.skinnedMeshRenderer),
                sprite = ObjectRef(shape.sprite),
                spriteRenderer = ObjectRef(shape.spriteRenderer),
                texture = ObjectRef(shape.texture)
            },

            rotationOverLifetime = new RotationOverLifetimeModuleExport
            {
                enabled = rotationOverLifetime.enabled,
                separateAxes = rotationOverLifetime.separateAxes,
                x = Curve(rotationOverLifetime.x),
                y = Curve(rotationOverLifetime.y),
                z = Curve(rotationOverLifetime.z)
            },

            colorOverLifetime = new ColorOverLifetimeModuleExport
            {
                enabled = colorOverLifetime.enabled,
                color = Gradient(colorOverLifetime.color)
            },

            sizeOverLifetime = new SizeOverLifetimeModuleExport
            {
                enabled = sizeOverLifetime.enabled,
                size = Curve(sizeOverLifetime.size)
            },

            customData = CaptureCustomData(customDataModule)
        };

        if (renderer != null)
        {
            summary.renderer = new RendererModuleExport
            {
                renderMode = renderer.renderMode.ToString(),
                alignment = renderer.alignment.ToString(),
                sortMode = renderer.sortMode.ToString(),
                sortingLayerName = renderer.sortingLayerName,
                sortingLayerID = renderer.sortingLayerID,
                sortingOrder = renderer.sortingOrder,
                minParticleSize = renderer.minParticleSize,
                maxParticleSize = renderer.maxParticleSize,
                normalDirection = renderer.normalDirection,
                shadowCastingMode = renderer.shadowCastingMode.ToString(),
                receiveShadows = renderer.receiveShadows,
                allowRoll = renderer.allowRoll,
                pivot = renderer.pivot,
                flip = renderer.flip,
                sharedMaterial = ObjectRef(renderer.sharedMaterial),
                trailMaterial = ObjectRef(renderer.trailMaterial)
            };
        }

        return summary;
    }

    private static CustomDataModuleExport CaptureCustomData(ParticleSystem.CustomDataModule customData)
    {
        return new CustomDataModuleExport
        {
            enabled = customData.enabled,
            custom1 = CaptureCustomDataStream(customData, ParticleSystemCustomData.Custom1),
            custom2 = CaptureCustomDataStream(customData, ParticleSystemCustomData.Custom2)
        };
    }

    private static CustomDataStreamExport CaptureCustomDataStream(
        ParticleSystem.CustomDataModule customData, ParticleSystemCustomData stream)
    {
        ParticleSystemCustomDataMode mode = customData.GetMode(stream);
        var result = new CustomDataStreamExport
        {
            stream = stream.ToString(),
            mode = mode.ToString(),
            vectorComponentCount = customData.GetVectorComponentCount(stream),
            vectorComponents = new List<MinMaxCurveExport>()
        };

        if (mode == ParticleSystemCustomDataMode.Vector)
        {
            for (int i = 0; i < result.vectorComponentCount; ++i)
                result.vectorComponents.Add(Curve(customData.GetVector(stream, i)));
        }
        else if (mode == ParticleSystemCustomDataMode.Color)
        {
            result.color = Gradient(customData.GetColor(stream));
        }

        return result;
    }

    private static List<BurstExport> CaptureBursts(ParticleSystem.EmissionModule emission)
    {
        var result = new List<BurstExport>();
        int count = emission.burstCount;

        if (count <= 0)
            return result;

        var bursts = new ParticleSystem.Burst[count];
        emission.GetBursts(bursts);

        foreach (ParticleSystem.Burst burst in bursts)
        {
            result.Add(new BurstExport
            {
                time = burst.time,
                count = Curve(burst.count),
                cycleCount = burst.cycleCount,
                repeatInterval = burst.repeatInterval,
                probability = burst.probability
            });
        }

        return result;
    }

    private static List<SerializedPropertyExport> DumpSerializedObject(UnityEngine.Object target)
    {
        var result = new List<SerializedPropertyExport>();

        if (target == null)
            return result;

        var serializedObject = new SerializedObject(target);
        SerializedProperty iterator = serializedObject.GetIterator();

        bool enterChildren = true;
        while (iterator.NextVisible(enterChildren))
        {
            SerializedProperty prop = iterator.Copy();

            var exported = new SerializedPropertyExport
            {
                path = prop.propertyPath,
                displayName = prop.displayName,
                type = prop.propertyType.ToString(),
                depth = prop.depth,
                isArray = prop.isArray,
                value = GetSerializedPropertyValue(prop)
            };

            if (prop.propertyType == SerializedPropertyType.ObjectReference)
            {
                exported.objectReference = ObjectRef(prop.objectReferenceValue);
                exported.objectReferenceInstanceID = prop.objectReferenceInstanceIDValue;
            }
            else if (prop.propertyType == SerializedPropertyType.AnimationCurve)
            {
                exported.animationCurve = AnimationCurveExport.From(prop.animationCurveValue);
            }

            result.Add(exported);
            enterChildren = true;
        }

        return result;
    }

    private static string GetSerializedPropertyValue(SerializedProperty prop)
    {
        switch (prop.propertyType)
        {
            case SerializedPropertyType.Integer:
                return prop.intValue.ToString(CultureInfo.InvariantCulture);

            case SerializedPropertyType.Boolean:
                return prop.boolValue.ToString();

            case SerializedPropertyType.Float:
                return prop.floatValue.ToString("R", CultureInfo.InvariantCulture);

            case SerializedPropertyType.String:
                return prop.stringValue;

            case SerializedPropertyType.Color:
                return ColorToString(prop.colorValue);

            case SerializedPropertyType.ObjectReference:
                return prop.objectReferenceValue != null
                    ? prop.objectReferenceValue.name + " (" + prop.objectReferenceValue.GetType().Name + ")"
                    : "null";

            case SerializedPropertyType.LayerMask:
                return prop.intValue.ToString(CultureInfo.InvariantCulture);

            case SerializedPropertyType.Enum:
                if (prop.enumDisplayNames != null &&
                    prop.enumValueIndex >= 0 &&
                    prop.enumValueIndex < prop.enumDisplayNames.Length)
                {
                    return prop.enumDisplayNames[prop.enumValueIndex] + " (" + prop.enumValueIndex + ")";
                }

                return prop.enumValueIndex.ToString(CultureInfo.InvariantCulture);

            case SerializedPropertyType.Vector2:
                return Vector2ToString(prop.vector2Value);

            case SerializedPropertyType.Vector3:
                return Vector3ToString(prop.vector3Value);

            case SerializedPropertyType.Vector4:
                return Vector4ToString(prop.vector4Value);

            case SerializedPropertyType.Rect:
                return RectToString(prop.rectValue);

            case SerializedPropertyType.ArraySize:
                return prop.intValue.ToString(CultureInfo.InvariantCulture);

            case SerializedPropertyType.Character:
                return prop.intValue.ToString(CultureInfo.InvariantCulture);

            case SerializedPropertyType.AnimationCurve:
                return CurveToString(prop.animationCurveValue);

            case SerializedPropertyType.Bounds:
                return BoundsToString(prop.boundsValue);

            case SerializedPropertyType.Quaternion:
                return QuaternionToString(prop.quaternionValue);

            default:
                if (prop.isArray)
                    return "Array(size=" + prop.arraySize + ")";

                return string.Empty;
        }
    }

    private static TransformExport CaptureTransform(Transform transform)
    {
        return new TransformExport
        {
            localPosition = transform.localPosition,
            localRotation = transform.localRotation,
            localEulerAngles = transform.localEulerAngles,
            localScale = transform.localScale
        };
    }

    private static MinMaxCurveExport Curve(ParticleSystem.MinMaxCurve curve)
    {
        return new MinMaxCurveExport
        {
            mode = curve.mode.ToString(),
            constant = curve.constant,
            constantMin = curve.constantMin,
            constantMax = curve.constantMax,
            curveMultiplier = curve.curveMultiplier,
            curve = AnimationCurveExport.From(curve.curve),
            curveMin = AnimationCurveExport.From(curve.curveMin),
            curveMax = AnimationCurveExport.From(curve.curveMax)
        };
    }

    private static MinMaxGradientExport Gradient(ParticleSystem.MinMaxGradient gradient)
    {
        return new MinMaxGradientExport
        {
            mode = gradient.mode.ToString(),
            color = ColorExport.From(gradient.color),
            colorMin = ColorExport.From(gradient.colorMin),
            colorMax = ColorExport.From(gradient.colorMax),
            gradient = GradientExport.From(gradient.gradient),
            gradientMin = GradientExport.From(gradient.gradientMin),
            gradientMax = GradientExport.From(gradient.gradientMax)
        };
    }

    private static ObjectReferenceExport ObjectRef(UnityEngine.Object obj)
    {
        if (obj == null)
        {
            return new ObjectReferenceExport
            {
                exists = false
            };
        }

        string assetPath = AssetDatabase.GetAssetPath(obj);

        return new ObjectReferenceExport
        {
            exists = true,
            name = obj.name,
            type = obj.GetType().Name,
            assetPath = assetPath,
            assetGuid = string.IsNullOrEmpty(assetPath) ? string.Empty : AssetDatabase.AssetPathToGUID(assetPath)
        };
    }

    private static string GetRelativePath(Transform root, Transform target)
    {
        if (root == target)
            return root.name;

        var parts = new List<string>();
        Transform current = target;

        while (current != null)
        {
            parts.Add(current.name);

            if (current == root)
                break;

            current = current.parent;
        }

        parts.Reverse();
        return string.Join("/", parts);
    }

    private static string SanitizeFileName(string fileName)
    {
        foreach (char invalid in Path.GetInvalidFileNameChars())
        {
            fileName = fileName.Replace(invalid, '_');
        }

        return fileName;
    }

    private static string Float(float value)
    {
        return value.ToString("R", CultureInfo.InvariantCulture);
    }

    private static string ColorToString(Color c)
    {
        return "RGBA(" + Float(c.r) + ", " + Float(c.g) + ", " + Float(c.b) + ", " + Float(c.a) + ") #" +
               ColorUtility.ToHtmlStringRGBA(c);
    }

    private static string Vector2ToString(Vector2 v)
    {
        return "(" + Float(v.x) + ", " + Float(v.y) + ")";
    }

    private static string Vector3ToString(Vector3 v)
    {
        return "(" + Float(v.x) + ", " + Float(v.y) + ", " + Float(v.z) + ")";
    }

    private static string Vector4ToString(Vector4 v)
    {
        return "(" + Float(v.x) + ", " + Float(v.y) + ", " + Float(v.z) + ", " + Float(v.w) + ")";
    }

    private static string QuaternionToString(Quaternion q)
    {
        return "(" + Float(q.x) + ", " + Float(q.y) + ", " + Float(q.z) + ", " + Float(q.w) + ")";
    }

    private static string RectToString(Rect r)
    {
        return "x=" + Float(r.x) + ", y=" + Float(r.y) + ", width=" + Float(r.width) + ", height=" + Float(r.height);
    }

    private static string BoundsToString(Bounds b)
    {
        return "center=" + Vector3ToString(b.center) + ", size=" + Vector3ToString(b.size);
    }

    private static string CurveToString(AnimationCurve curve)
    {
        if (curve == null)
            return "null";

        return "keys=" + curve.length +
               ", preWrap=" + curve.preWrapMode +
               ", postWrap=" + curve.postWrapMode;
    }
}

[Serializable]
public class ParticleSystemRootExport
{
    public string unityVersion;
    public string exportedAtLocal;
    public string rootName;
    public string rootScene;
    public List<ParticleSystemExport> systems;
}

[Serializable]
public class ParticleSystemExport
{
    public string name;
    public string relativePath;
    public bool activeSelf;
    public bool activeInHierarchy;
    public TransformExport transform;
    public ParticleSystemSummary summary;
    public List<SerializedPropertyExport> particleSystemSerializedProperties;
    public List<SerializedPropertyExport> rendererSerializedProperties;
}

[Serializable]
public class TransformExport
{
    public Vector3 localPosition;
    public Quaternion localRotation;
    public Vector3 localEulerAngles;
    public Vector3 localScale;
}

[Serializable]
public class ParticleSystemSummary
{
    public MainModuleExport main;
    public EmissionModuleExport emission;
    public ShapeModuleExport shape;
    public RotationOverLifetimeModuleExport rotationOverLifetime;
    public ColorOverLifetimeModuleExport colorOverLifetime;
    public SizeOverLifetimeModuleExport sizeOverLifetime;
    public RendererModuleExport renderer;
    public CustomDataModuleExport customData;
}

[Serializable]
public class CustomDataModuleExport
{
    public bool enabled;
    public CustomDataStreamExport custom1;
    public CustomDataStreamExport custom2;
}

[Serializable]
public class CustomDataStreamExport
{
    public string stream;
    public string mode;
    public int vectorComponentCount;
    public List<MinMaxCurveExport> vectorComponents;
    public MinMaxGradientExport color;
}

[Serializable]
public class MainModuleExport
{
    public float duration;
    public bool loop;
    public bool prewarm;
    public MinMaxCurveExport startDelay;
    public MinMaxCurveExport startLifetime;
    public MinMaxCurveExport startSpeed;
    public bool startSize3D;
    public MinMaxCurveExport startSize;
    public MinMaxCurveExport startSizeX;
    public MinMaxCurveExport startSizeY;
    public MinMaxCurveExport startSizeZ;
    public bool startRotation3D;
    public MinMaxCurveExport startRotation;
    public MinMaxCurveExport startRotationX;
    public MinMaxCurveExport startRotationY;
    public MinMaxCurveExport startRotationZ;
    public float flipRotation;
    public MinMaxGradientExport startColor;
    public MinMaxCurveExport gravityModifier;
    public string simulationSpace;
    public string customSimulationSpace;
    public float simulationSpeed;
    public string scalingMode;
    public bool playOnAwake;
    public int maxParticles;
}

[Serializable]
public class EmissionModuleExport
{
    public bool enabled;
    public MinMaxCurveExport rateOverTime;
    public MinMaxCurveExport rateOverDistance;
    public List<BurstExport> bursts;
}

[Serializable]
public class BurstExport
{
    public float time;
    public MinMaxCurveExport count;
    public int cycleCount;
    public float repeatInterval;
    public float probability;
}

[Serializable]
public class ShapeModuleExport
{
    public bool enabled;
    public string shapeType;
    public float angle;
    public float radius;
    public float radiusThickness;
    public float arc;
    public Vector3 position;
    public Vector3 rotation;
    public Vector3 scale;
    public bool alignToDirection;
    public float randomDirectionAmount;
    public float sphericalDirectionAmount;
    public float randomPositionAmount;
    public ObjectReferenceExport mesh;
    public ObjectReferenceExport meshRenderer;
    public ObjectReferenceExport skinnedMeshRenderer;
    public ObjectReferenceExport sprite;
    public ObjectReferenceExport spriteRenderer;
    public ObjectReferenceExport texture;
}

[Serializable]
public class RotationOverLifetimeModuleExport
{
    public bool enabled;
    public bool separateAxes;
    public MinMaxCurveExport x;
    public MinMaxCurveExport y;
    public MinMaxCurveExport z;
}

[Serializable]
public class RendererModuleExport
{
    public string renderMode;
    public string alignment;
    public string sortMode;
    public string sortingLayerName;
    public int sortingLayerID;
    public int sortingOrder;
    public float minParticleSize;
    public float maxParticleSize;
    public float normalDirection;
    public string shadowCastingMode;
    public bool receiveShadows;
    public bool allowRoll;
    public Vector3 pivot;
    public Vector3 flip;
    public ObjectReferenceExport sharedMaterial;
    public ObjectReferenceExport trailMaterial;
}

[Serializable]
public class SerializedPropertyExport
{
    public string path;
    public string displayName;
    public string type;
    public int depth;
    public bool isArray;
    public string value;
    public AnimationCurveExport animationCurve;
    public int objectReferenceInstanceID;
    public ObjectReferenceExport objectReference;
}

[Serializable]
public class MinMaxCurveExport
{
    public string mode;
    public float constant;
    public float constantMin;
    public float constantMax;
    public float curveMultiplier;
    public AnimationCurveExport curve;
    public AnimationCurveExport curveMin;
    public AnimationCurveExport curveMax;
}

[Serializable]
public class MinMaxGradientExport
{
    public string mode;
    public ColorExport color;
    public ColorExport colorMin;
    public ColorExport colorMax;
    public GradientExport gradient;
    public GradientExport gradientMin;
    public GradientExport gradientMax;
}

[Serializable]
public class AnimationCurveExport
{
    public string preWrapMode;
    public string postWrapMode;
    public List<KeyframeExport> keys;

    public static AnimationCurveExport From(AnimationCurve curve)
    {
        if (curve == null)
            return null;

        var export = new AnimationCurveExport
        {
            preWrapMode = curve.preWrapMode.ToString(),
            postWrapMode = curve.postWrapMode.ToString(),
            keys = new List<KeyframeExport>()
        };

        foreach (Keyframe key in curve.keys)
        {
            export.keys.Add(new KeyframeExport
            {
                time = key.time,
                value = key.value,
                inTangent = key.inTangent,
                outTangent = key.outTangent,
                inWeight = key.inWeight,
                outWeight = key.outWeight,
                weightedMode = key.weightedMode.ToString()
            });
        }

        return export;
    }
}

[Serializable]
public class KeyframeExport
{
    public float time;
    public float value;
    public float inTangent;
    public float outTangent;
    public float inWeight;
    public float outWeight;
    public string weightedMode;
}

[Serializable]
public class GradientExport
{
    public string mode;
    public List<GradientColorKeyExport> colorKeys;
    public List<GradientAlphaKeyExport> alphaKeys;

    public static GradientExport From(Gradient gradient)
    {
        if (gradient == null)
            return null;

        var export = new GradientExport
        {
            mode = gradient.mode.ToString(),
            colorKeys = new List<GradientColorKeyExport>(),
            alphaKeys = new List<GradientAlphaKeyExport>()
        };

        foreach (GradientColorKey key in gradient.colorKeys)
        {
            export.colorKeys.Add(new GradientColorKeyExport
            {
                time = key.time,
                color = ColorExport.From(key.color)
            });
        }

        foreach (GradientAlphaKey key in gradient.alphaKeys)
        {
            export.alphaKeys.Add(new GradientAlphaKeyExport
            {
                time = key.time,
                alpha = key.alpha
            });
        }

        return export;
    }
}

[Serializable]
public class GradientColorKeyExport
{
    public float time;
    public ColorExport color;
}

[Serializable]
public class GradientAlphaKeyExport
{
    public float time;
    public float alpha;
}

[Serializable]
public class ColorExport
{
    public float r;
    public float g;
    public float b;
    public float a;
    public string htmlRGBA;

    public static ColorExport From(Color color)
    {
        return new ColorExport
        {
            r = color.r,
            g = color.g,
            b = color.b,
            a = color.a,
            htmlRGBA = "#" + ColorUtility.ToHtmlStringRGBA(color)
        };
    }
}

[Serializable]
public class ColorOverLifetimeModuleExport
{
    public bool enabled;
    public MinMaxGradientExport color;
}

[Serializable]
public class SizeOverLifetimeModuleExport
{
    public bool enabled;
    public MinMaxCurveExport size;
}

[Serializable]
public class ObjectReferenceExport
{
    public bool exists;
    public string name;
    public string type;
    public string assetPath;
    public string assetGuid;
}
#endif

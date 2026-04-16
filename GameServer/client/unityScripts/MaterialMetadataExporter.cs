// Unity Editor script: Tools > Export Material Metadata
// Dumps a material's shader properties and serialized saved properties to JSON.
// Useful when porting Shader Graph materials to the DX12 renderer.

#if UNITY_EDITOR

using System;
using System.Collections.Generic;
using System.IO;
using System.Reflection;
using UnityEditor;
using UnityEngine;

public class MaterialMetadataExporter : EditorWindow {

    private Material targetMaterial;
    private Renderer targetRenderer;
    private int      materialIndex = 0;
    private string   outputPath    = "";

    [MenuItem("Tools/Export Material Metadata")]
    public static void ShowWindow() {
        GetWindow<MaterialMetadataExporter>("Export Material Metadata");
    }

    private void OnGUI() {
        GUILayout.Label("Material Metadata Exporter", EditorStyles.boldLabel);
        EditorGUILayout.Space();

        targetMaterial = (Material)EditorGUILayout.ObjectField(
            "Material", targetMaterial, typeof(Material), false);

        targetRenderer = (Renderer)EditorGUILayout.ObjectField(
            "Renderer Fallback", targetRenderer, typeof(Renderer), true);

        materialIndex = EditorGUILayout.IntField("Renderer Material Index", materialIndex);

        EditorGUILayout.Space();

        if (GUILayout.Button("Use Selected Renderer")) {
            if (Selection.activeGameObject != null) {
                targetRenderer = Selection.activeGameObject.GetComponent<Renderer>();
                if (targetRenderer == null)
                    Debug.LogWarning("MaterialMetadataExporter: selected GameObject has no Renderer.");
            }
        }

        Material resolved = ResolveMaterial();
        using (new EditorGUI.DisabledScope(true)) {
            EditorGUILayout.ObjectField("Resolved Material", resolved, typeof(Material), false);
        }

        EditorGUILayout.Space();

        if (GUILayout.Button("Choose Output File")) {
            string defaultName = resolved != null ? SanitizeFileName(resolved.name) : "MaterialMetadata";
            outputPath = EditorUtility.SaveFilePanel(
                "Save material metadata JSON", Application.dataPath, defaultName, "json");
        }

        if (!string.IsNullOrEmpty(outputPath))
            EditorGUILayout.LabelField("Output:", outputPath);

        EditorGUILayout.Space();

        GUI.enabled = resolved != null && !string.IsNullOrEmpty(outputPath);
        if (GUILayout.Button("Export JSON")) {
            Export(resolved, outputPath);
        }
        GUI.enabled = true;
    }

    private Material ResolveMaterial() {
        if (targetMaterial != null)
            return targetMaterial;

        if (targetRenderer == null)
            return null;

        Material[] materials = targetRenderer.sharedMaterials;
        if (materials == null || materials.Length == 0)
            return null;

        int idx = Mathf.Clamp(materialIndex, 0, materials.Length - 1);
        return materials[idx];
    }

    public static void Export(Material material, string outputPath) {
        if (material == null) {
            Debug.LogError("MaterialMetadataExporter: material is null.");
            return;
        }

        MaterialMetadata metadata = BuildMetadata(material);
        string json = JsonUtility.ToJson(metadata, true);
        File.WriteAllText(outputPath, json);
        AssetDatabase.Refresh();

        Debug.Log($"MaterialMetadataExporter: exported '{material.name}' -> {outputPath}");
    }

    private static MaterialMetadata BuildMetadata(Material material) {
        Shader shader = material.shader;

        MaterialMetadata ret = new MaterialMetadata {
            materialName              = material.name,
            materialAssetPath         = AssetDatabase.GetAssetPath(material),
            shaderName                = shader != null ? shader.name : "",
            shaderAssetPath           = shader != null ? AssetDatabase.GetAssetPath(shader) : "",
            renderQueue               = material.renderQueue,
            rawRenderQueue            = material.rawRenderQueue,
            enableInstancing          = material.enableInstancing,
            doubleSidedGI             = material.doubleSidedGI,
            globalIlluminationFlags   = material.globalIlluminationFlags.ToString(),
            shaderKeywords            = new List<string>(material.shaderKeywords),
            tags                      = CollectTags(material),
            shaderProperties          = CollectShaderProperties(material),
            savedProperties           = CollectSavedProperties(material),
            swordSlashMappedValues    = CollectSwordSlashMappedValues(material)
        };

        return ret;
    }

    private static List<TagValue> CollectTags(Material material) {
        string[] tagNames = {
            "Queue",
            "RenderType",
            "RenderPipeline",
            "UniversalMaterialType",
            "ShaderGraphShader",
            "ShaderGraphTargetId",
            "BuiltInMaterialType",
            "DisableBatching",
            "IgnoreProjector",
            "PreviewType"
        };

        var ret = new List<TagValue>();
        foreach (string tagName in tagNames) {
            string value = material.GetTag(tagName, false, "");
            if (!string.IsNullOrEmpty(value)) {
                ret.Add(new TagValue { name = tagName, value = value });
            }
        }
        return ret;
    }

    private static List<ShaderPropertyDump> CollectShaderProperties(Material material) {
        var ret = new List<ShaderPropertyDump>();
        Shader shader = material.shader;
        if (shader == null)
            return ret;

        int count = ShaderUtil.GetPropertyCount(shader);
        for (int i = 0; i < count; ++i) {
            string name = ShaderUtil.GetPropertyName(shader, i);
            ShaderUtil.ShaderPropertyType type = ShaderUtil.GetPropertyType(shader, i);

            ShaderPropertyDump item = new ShaderPropertyDump {
                index       = i,
                name        = name,
                displayName = GetPropertyDescriptionSafe(shader, i),
                type        = type.ToString(),
                attributes  = GetPropertyAttributesSafe(shader, i),
                flags       = GetPropertyFlagsSafe(shader, i)
            };

            if (material.HasProperty(name)) {
                FillPropertyValue(material, name, type, item);
            } else {
                item.value = "<material has no property>";
            }

            ret.Add(item);
        }

        return ret;
    }

    private static void FillPropertyValue(
        Material material,
        string propertyName,
        ShaderUtil.ShaderPropertyType type,
        ShaderPropertyDump item
    ) {
        if (type == ShaderUtil.ShaderPropertyType.TexEnv) {
            Texture tex = material.GetTexture(propertyName);
            item.texture = new TextureDump {
                textureName    = tex != null ? tex.name : "",
                assetPath      = tex != null ? AssetDatabase.GetAssetPath(tex) : "",
                scale          = Vec2Dump.From(material.GetTextureScale(propertyName)),
                offset         = Vec2Dump.From(material.GetTextureOffset(propertyName)),
                wrapModeU      = tex != null ? tex.wrapModeU.ToString() : "",
                wrapModeV      = tex != null ? tex.wrapModeV.ToString() : "",
                wrapModeW      = tex != null ? tex.wrapModeW.ToString() : "",
                filterMode     = tex != null ? tex.filterMode.ToString() : "",
                anisoLevel     = tex != null ? tex.anisoLevel : 0,
                width          = tex != null ? tex.width : 0,
                height         = tex != null ? tex.height : 0
            };
            item.value = tex != null ? tex.name : "<null>";
        } else if (type == ShaderUtil.ShaderPropertyType.Color) {
            item.color = ColorDump.From(material.GetColor(propertyName));
            item.value = item.color.ToCompactString();
        } else if (type == ShaderUtil.ShaderPropertyType.Vector) {
            item.vector = Vec4Dump.From(material.GetVector(propertyName));
            item.value = item.vector.ToCompactString();
        } else {
            item.floatValue = material.GetFloat(propertyName);
            item.value = item.floatValue.ToString("R");
        }
    }

    private static SavedPropertiesDump CollectSavedProperties(Material material) {
        var ret = new SavedPropertiesDump();
        var serialized = new SerializedObject(material);
        SerializedProperty saved = serialized.FindProperty("m_SavedProperties");

        if (saved == null)
            return ret;

        CollectSavedTexEnvs(saved.FindPropertyRelative("m_TexEnvs"), ret.texEnvs);
        CollectSavedFloats(saved.FindPropertyRelative("m_Floats"), ret.floats);
        CollectSavedColors(saved.FindPropertyRelative("m_Colors"), ret.colors);
        CollectSavedInts(saved.FindPropertyRelative("m_Ints"), ret.ints);

        return ret;
    }

    private static void CollectSavedTexEnvs(SerializedProperty array, List<SavedTexEnvDump> output) {
        if (array == null || !array.isArray)
            return;

        for (int i = 0; i < array.arraySize; ++i) {
            SerializedProperty elem = array.GetArrayElementAtIndex(i);
            SerializedProperty nameProp = elem.FindPropertyRelative("first");
            SerializedProperty texProp = elem.FindPropertyRelative("second.m_Texture");
            SerializedProperty scaleProp = elem.FindPropertyRelative("second.m_Scale");
            SerializedProperty offsetProp = elem.FindPropertyRelative("second.m_Offset");

            Texture tex = texProp != null ? texProp.objectReferenceValue as Texture : null;
            output.Add(new SavedTexEnvDump {
                name        = nameProp != null ? nameProp.stringValue : "",
                textureName = tex != null ? tex.name : "",
                assetPath   = tex != null ? AssetDatabase.GetAssetPath(tex) : "",
                scale       = scaleProp != null ? Vec2Dump.From(scaleProp.vector2Value) : new Vec2Dump(),
                offset      = offsetProp != null ? Vec2Dump.From(offsetProp.vector2Value) : new Vec2Dump()
            });
        }
    }

    private static void CollectSavedFloats(SerializedProperty array, List<SavedFloatDump> output) {
        if (array == null || !array.isArray)
            return;

        for (int i = 0; i < array.arraySize; ++i) {
            SerializedProperty elem = array.GetArrayElementAtIndex(i);
            SerializedProperty nameProp = elem.FindPropertyRelative("first");
            SerializedProperty valueProp = elem.FindPropertyRelative("second");

            output.Add(new SavedFloatDump {
                name  = nameProp != null ? nameProp.stringValue : "",
                value = valueProp != null ? valueProp.floatValue : 0.0f
            });
        }
    }

    private static void CollectSavedColors(SerializedProperty array, List<SavedColorDump> output) {
        if (array == null || !array.isArray)
            return;

        for (int i = 0; i < array.arraySize; ++i) {
            SerializedProperty elem = array.GetArrayElementAtIndex(i);
            SerializedProperty nameProp = elem.FindPropertyRelative("first");
            SerializedProperty valueProp = elem.FindPropertyRelative("second");

            output.Add(new SavedColorDump {
                name  = nameProp != null ? nameProp.stringValue : "",
                value = valueProp != null ? ColorDump.From(valueProp.colorValue) : new ColorDump()
            });
        }
    }

    private static void CollectSavedInts(SerializedProperty array, List<SavedIntDump> output) {
        if (array == null || !array.isArray)
            return;

        for (int i = 0; i < array.arraySize; ++i) {
            SerializedProperty elem = array.GetArrayElementAtIndex(i);
            SerializedProperty nameProp = elem.FindPropertyRelative("first");
            SerializedProperty valueProp = elem.FindPropertyRelative("second");

            output.Add(new SavedIntDump {
                name  = nameProp != null ? nameProp.stringValue : "",
                value = valueProp != null ? valueProp.intValue : 0
            });
        }
    }

    private static SwordSlashValues CollectSwordSlashMappedValues(Material material) {
        return new SwordSlashValues {
            opacity             = GetFloatIfExists(material, "_Opacity", 1.0f),
            emission            = GetFloatIfExists(material, "_Emission", 1.0f),
            desaturation        = GetFloatIfExists(material, "_Desaturation", 0.0f),
            useSmoothDissolve   = GetFloatIfExists(material, "_Usesmoothdissolve", 0.0f),
            useDepth            = GetFloatIfExists(material, "_Usedepth", 0.0f),
            flowPower           = GetFloatIfExists(material, "_Flowpower", 0.0f),
            remap               = Vec4Dump.From(GetVectorIfExists(material, "_Remap", Vector4.zero)),
            addColor            = ColorDump.From(GetColorIfExists(material, "_AddColor", Color.clear)),
            speedMainTexUVNoise = Vec4Dump.From(GetVectorIfExists(material, "_SpeedMainTexUVNoiseZW", Vector4.zero)),
            speedFlow           = Vec4Dump.From(GetVectorIfExists(material, "_SpeedFlow", Vector4.zero)),
            mainTexture         = GetTextureIfExists(material, "_MainTexture"),
            emissionTex         = GetTextureIfExists(material, "_EmissionTex"),
            dissolveTex         = GetTextureIfExists(material, "_Dissolve"),
            flowTex             = GetTextureIfExists(material, "_Flow")
        };
    }

    private static float GetFloatIfExists(Material material, string name, float fallback) {
        return material.HasProperty(name) ? material.GetFloat(name) : fallback;
    }

    private static Vector4 GetVectorIfExists(Material material, string name, Vector4 fallback) {
        return material.HasProperty(name) ? material.GetVector(name) : fallback;
    }

    private static Color GetColorIfExists(Material material, string name, Color fallback) {
        return material.HasProperty(name) ? material.GetColor(name) : fallback;
    }

    private static TextureDump GetTextureIfExists(Material material, string name) {
        var item = new ShaderPropertyDump();
        if (material.HasProperty(name)) {
            FillPropertyValue(material, name, ShaderUtil.ShaderPropertyType.TexEnv, item);
            return item.texture;
        }

        return new TextureDump();
    }

    private static string SanitizeFileName(string name) {
        foreach (char c in Path.GetInvalidFileNameChars()) {
            name = name.Replace(c, '_');
        }
        return name;
    }

    private static List<string> GetPropertyAttributesSafe(Shader shader, int propertyIndex) {
        MethodInfo method = typeof(ShaderUtil).GetMethod(
            "GetPropertyAttributes",
            BindingFlags.Public | BindingFlags.NonPublic | BindingFlags.Static,
            null,
            new Type[] { typeof(Shader), typeof(int) },
            null);

        if (method == null)
            return new List<string>();

        try {
            object value = method.Invoke(null, new object[] { shader, propertyIndex });
            if (value is string[] array)
                return new List<string>(array);

            if (value is IEnumerable<string> enumerable)
                return new List<string>(enumerable);
        } catch {
            // Older Unity versions expose different ShaderUtil internals.
        }

        return new List<string>();
    }

    private static string GetPropertyDescriptionSafe(Shader shader, int propertyIndex) {
        MethodInfo method = typeof(ShaderUtil).GetMethod(
            "GetPropertyDescription",
            BindingFlags.Public | BindingFlags.NonPublic | BindingFlags.Static,
            null,
            new Type[] { typeof(Shader), typeof(int) },
            null);

        if (method == null)
            return "";

        try {
            object value = method.Invoke(null, new object[] { shader, propertyIndex });
            return value != null ? value.ToString() : "";
        } catch {
            return "";
        }
    }

    private static string GetPropertyFlagsSafe(Shader shader, int propertyIndex) {
        MethodInfo method = typeof(ShaderUtil).GetMethod(
            "GetPropertyFlags",
            BindingFlags.Public | BindingFlags.NonPublic | BindingFlags.Static,
            null,
            new Type[] { typeof(Shader), typeof(int) },
            null);

        if (method == null)
            return "";

        object value = method.Invoke(null, new object[] { shader, propertyIndex });
        return value != null ? value.ToString() : "";
    }

    [Serializable]
    private class MaterialMetadata {
        public string materialName;
        public string materialAssetPath;
        public string shaderName;
        public string shaderAssetPath;
        public int renderQueue;
        public int rawRenderQueue;
        public bool enableInstancing;
        public bool doubleSidedGI;
        public string globalIlluminationFlags;
        public List<string> shaderKeywords = new List<string>();
        public List<TagValue> tags = new List<TagValue>();
        public List<ShaderPropertyDump> shaderProperties = new List<ShaderPropertyDump>();
        public SavedPropertiesDump savedProperties = new SavedPropertiesDump();
        public SwordSlashValues swordSlashMappedValues = new SwordSlashValues();
    }

    [Serializable]
    private class TagValue {
        public string name;
        public string value;
    }

    [Serializable]
    private class ShaderPropertyDump {
        public int index;
        public string name;
        public string displayName;
        public string type;
        public string flags;
        public List<string> attributes = new List<string>();
        public string value;
        public float floatValue;
        public Vec4Dump vector;
        public ColorDump color;
        public TextureDump texture;
    }

    [Serializable]
    private class SavedPropertiesDump {
        public List<SavedTexEnvDump> texEnvs = new List<SavedTexEnvDump>();
        public List<SavedFloatDump> floats = new List<SavedFloatDump>();
        public List<SavedColorDump> colors = new List<SavedColorDump>();
        public List<SavedIntDump> ints = new List<SavedIntDump>();
    }

    [Serializable]
    private class SavedTexEnvDump {
        public string name;
        public string textureName;
        public string assetPath;
        public Vec2Dump scale;
        public Vec2Dump offset;
    }

    [Serializable]
    private class SavedFloatDump {
        public string name;
        public float value;
    }

    [Serializable]
    private class SavedColorDump {
        public string name;
        public ColorDump value;
    }

    [Serializable]
    private class SavedIntDump {
        public string name;
        public int value;
    }

    [Serializable]
    private class SwordSlashValues {
        public float opacity;
        public float emission;
        public float desaturation;
        public float useSmoothDissolve;
        public float useDepth;
        public float flowPower;
        public Vec4Dump remap;
        public ColorDump addColor;
        public Vec4Dump speedMainTexUVNoise;
        public Vec4Dump speedFlow;
        public TextureDump mainTexture;
        public TextureDump emissionTex;
        public TextureDump dissolveTex;
        public TextureDump flowTex;
    }

    [Serializable]
    private class TextureDump {
        public string textureName;
        public string assetPath;
        public Vec2Dump scale;
        public Vec2Dump offset;
        public string wrapModeU;
        public string wrapModeV;
        public string wrapModeW;
        public string filterMode;
        public int anisoLevel;
        public int width;
        public int height;
    }

    [Serializable]
    private class Vec2Dump {
        public float x;
        public float y;

        public static Vec2Dump From(Vector2 value) {
            return new Vec2Dump { x = value.x, y = value.y };
        }
    }

    [Serializable]
    private class Vec4Dump {
        public float x;
        public float y;
        public float z;
        public float w;

        public static Vec4Dump From(Vector4 value) {
            return new Vec4Dump { x = value.x, y = value.y, z = value.z, w = value.w };
        }

        public string ToCompactString() {
            return $"({x:R}, {y:R}, {z:R}, {w:R})";
        }
    }

    [Serializable]
    private class ColorDump {
        public float r;
        public float g;
        public float b;
        public float a;

        public static ColorDump From(Color value) {
            return new ColorDump { r = value.r, g = value.g, b = value.b, a = value.a };
        }

        public string ToCompactString() {
            return $"({r:R}, {g:R}, {b:R}, {a:R})";
        }
    }
}

#endif // UNITY_EDITOR

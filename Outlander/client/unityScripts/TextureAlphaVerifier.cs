// Unity Editor script: Tools > Verify Texture Alpha
// Reports alpha coverage for a Texture2D asset, comparing the source image file
// with Unity's imported texture when possible.

#if UNITY_EDITOR

using System;
using System.Collections.Generic;
using System.IO;
using System.Text;
using UnityEditor;
using UnityEngine;

public class TextureAlphaVerifier : EditorWindow {

    private Texture2D texture;
    private bool inspectSourceFile = true;
    private bool inspectImportedTexture = true;
    private bool temporarilyMakeReadable = true;
    private Vector2 scroll;
    private string exportPngPath = "";
    private string lastReport = "";

    [MenuItem("Tools/Verify Texture Alpha")]
    public static void ShowWindow() {
        GetWindow<TextureAlphaVerifier>("Verify Texture Alpha");
    }

    private void OnGUI() {
        GUILayout.Label("Texture Alpha Verifier", EditorStyles.boldLabel);
        EditorGUILayout.Space();

        texture = (Texture2D)EditorGUILayout.ObjectField(
            "Texture", texture, typeof(Texture2D), false);

        inspectSourceFile = EditorGUILayout.Toggle("Inspect Source File", inspectSourceFile);
        inspectImportedTexture = EditorGUILayout.Toggle("Inspect Imported Texture", inspectImportedTexture);
        temporarilyMakeReadable = EditorGUILayout.Toggle("Temporarily Make Readable", temporarilyMakeReadable);

        EditorGUILayout.Space();

        using (new EditorGUILayout.HorizontalScope()) {
            if (GUILayout.Button("Use Selected Texture")) {
                texture = Selection.activeObject as Texture2D;
                if (texture == null)
                    Debug.LogWarning("TextureAlphaVerifier: selected asset is not a Texture2D.");
            }

            if (GUILayout.Button("Find Smoke12.png")) {
                texture = FindTextureByFileName("Smoke12.png");
                if (texture == null)
                    Debug.LogWarning("TextureAlphaVerifier: Smoke12.png was not found in this Unity project.");
            }
        }

        GUI.enabled = texture != null;
        if (GUILayout.Button("Verify Alpha")) {
            lastReport = BuildReport(texture, inspectSourceFile, inspectImportedTexture, temporarilyMakeReadable);
            Debug.Log(lastReport);
        }
        GUI.enabled = true;

        EditorGUILayout.Space();
        GUILayout.Label("Export Imported Texture", EditorStyles.boldLabel);
        using (new EditorGUILayout.HorizontalScope()) {
            exportPngPath = EditorGUILayout.TextField("PNG Path", exportPngPath);
            if (GUILayout.Button("Choose", GUILayout.Width(80))) {
                string defaultDir = DefaultEngineTextureDirectory();
                string defaultName = texture != null ? texture.name + "_UnityImported.png" : "Smoke12_UnityImported.png";
                exportPngPath = EditorUtility.SaveFilePanel(
                    "Export imported RGBA PNG", defaultDir, defaultName, "png");
            }
        }

        GUI.enabled = texture != null;
        if (GUILayout.Button("Export Imported RGBA PNG")) {
            if (string.IsNullOrEmpty(exportPngPath)) {
                string defaultDir = DefaultEngineTextureDirectory();
                string defaultName = texture.name + "_UnityImported.png";
                exportPngPath = EditorUtility.SaveFilePanel(
                    "Export imported RGBA PNG", defaultDir, defaultName, "png");
            }

            if (!string.IsNullOrEmpty(exportPngPath)) {
                ExportImportedTextureToPng(texture, exportPngPath, temporarilyMakeReadable);
            }
        }
        GUI.enabled = true;

        EditorGUILayout.Space();
        GUILayout.Label("Report", EditorStyles.boldLabel);
        scroll = EditorGUILayout.BeginScrollView(scroll);
        EditorGUILayout.TextArea(lastReport, GUILayout.ExpandHeight(true));
        EditorGUILayout.EndScrollView();
    }

    private static Texture2D FindTextureByFileName(string fileName) {
        string wanted = fileName.Replace('\\', '/');
        foreach (string guid in AssetDatabase.FindAssets(Path.GetFileNameWithoutExtension(fileName) + " t:Texture2D")) {
            string assetPath = AssetDatabase.GUIDToAssetPath(guid).Replace('\\', '/');
            if (assetPath.EndsWith("/" + wanted, StringComparison.OrdinalIgnoreCase) ||
                string.Equals(assetPath, wanted, StringComparison.OrdinalIgnoreCase)) {
                return AssetDatabase.LoadAssetAtPath<Texture2D>(assetPath);
            }
        }

        return null;
    }

    private static string DefaultEngineTextureDirectory() {
        string projectRoot = Path.GetFullPath(Path.Combine(Application.dataPath, ".."));
        for (DirectoryInfo dir = new DirectoryInfo(projectRoot); dir != null; dir = dir.Parent) {
            string candidate = Path.Combine(dir.FullName, "resources", "Textures");
            if (Directory.Exists(candidate))
                return candidate;
        }

        return projectRoot;
    }

    private static string BuildReport(
        Texture2D tex,
        bool inspectSource,
        bool inspectImported,
        bool makeReadable
    ) {
        var sb = new StringBuilder();
        string assetPath = AssetDatabase.GetAssetPath(tex);
        var importer = AssetImporter.GetAtPath(assetPath) as TextureImporter;

        sb.AppendLine("TextureAlphaVerifier");
        sb.AppendLine($"Asset: {assetPath}");
        sb.AppendLine($"Texture: {tex.name} ({tex.width}x{tex.height}) format={tex.format}");

        if (importer != null) {
            sb.AppendLine("Importer:");
            sb.AppendLine($"  textureType={importer.textureType}");
            sb.AppendLine($"  alphaSource={importer.alphaSource}");
            sb.AppendLine($"  alphaIsTransparency={importer.alphaIsTransparency}");
            sb.AppendLine($"  isReadable={importer.isReadable}");
            sb.AppendLine($"  sRGBTexture={importer.sRGBTexture}");
            sb.AppendLine($"  textureCompression={importer.textureCompression}");
            sb.AppendLine($"  maxTextureSize={importer.maxTextureSize}");
        } else {
            sb.AppendLine("Importer: unavailable");
        }

        if (inspectSource) {
            sb.AppendLine();
            sb.AppendLine("Source file alpha:");
            AppendSourceFileAlphaReport(sb, assetPath);
        }

        if (inspectImported) {
            sb.AppendLine();
            sb.AppendLine("Imported texture alpha:");
            AppendImportedTextureAlphaReport(sb, tex, assetPath, importer, makeReadable);
        }

        return sb.ToString();
    }

    private static void AppendSourceFileAlphaReport(StringBuilder sb, string assetPath) {
        string projectRoot = Path.GetFullPath(Path.Combine(Application.dataPath, ".."));
        string fullPath = Path.GetFullPath(Path.Combine(projectRoot, assetPath));
        if (!File.Exists(fullPath)) {
            sb.AppendLine($"  Source file not found: {fullPath}");
            return;
        }

        var rawTex = new Texture2D(2, 2, TextureFormat.RGBA32, false, true);
        try {
            byte[] bytes = File.ReadAllBytes(fullPath);
            if (!ImageConversion.LoadImage(rawTex, bytes, false)) {
                sb.AppendLine("  Unity could not decode the source file.");
                return;
            }

            AppendAlphaStats(sb, rawTex.width, rawTex.height, rawTex.GetPixels32());
        } finally {
            UnityEngine.Object.DestroyImmediate(rawTex);
        }
    }

    private static void AppendImportedTextureAlphaReport(
        StringBuilder sb,
        Texture2D tex,
        string assetPath,
        TextureImporter importer,
        bool makeReadable
    ) {
        bool restoredReadable = false;
        bool originalReadable = false;

        try {
            if (!tex.isReadable) {
                if (importer == null || !makeReadable) {
                    sb.AppendLine("  Imported texture is not readable. Enable the temporary readable option.");
                    return;
                }

                originalReadable = importer.isReadable;
                importer.isReadable = true;
                importer.SaveAndReimport();
                restoredReadable = true;
                tex = AssetDatabase.LoadAssetAtPath<Texture2D>(assetPath);
            }

            AppendAlphaStats(sb, tex.width, tex.height, tex.GetPixels32());
        } finally {
            if (restoredReadable && importer != null) {
                importer.isReadable = originalReadable;
                importer.SaveAndReimport();
            }
        }
    }

    private static void ExportImportedTextureToPng(Texture2D tex, string outputPath, bool makeReadable) {
        string assetPath = AssetDatabase.GetAssetPath(tex);
        var importer = AssetImporter.GetAtPath(assetPath) as TextureImporter;
        bool restoredReadable = false;
        bool originalReadable = false;

        try {
            if (!tex.isReadable) {
                if (importer == null || !makeReadable) {
                    Debug.LogError("TextureAlphaVerifier: imported texture is not readable.");
                    return;
                }

                originalReadable = importer.isReadable;
                importer.isReadable = true;
                importer.SaveAndReimport();
                restoredReadable = true;
                tex = AssetDatabase.LoadAssetAtPath<Texture2D>(assetPath);
            }

            Color32[] pixels = tex.GetPixels32();
            var rgba = new Texture2D(tex.width, tex.height, TextureFormat.RGBA32, false, false);
            try {
                rgba.SetPixels32(pixels);
                rgba.Apply(false, false);

                string dir = Path.GetDirectoryName(outputPath);
                if (!string.IsNullOrEmpty(dir))
                    Directory.CreateDirectory(dir);

                File.WriteAllBytes(outputPath, rgba.EncodeToPNG());
                Debug.Log($"TextureAlphaVerifier: exported imported texture RGBA PNG -> {outputPath}");
            } finally {
                UnityEngine.Object.DestroyImmediate(rgba);
            }
        } finally {
            if (restoredReadable && importer != null) {
                importer.isReadable = originalReadable;
                importer.SaveAndReimport();
            }
        }
    }

    private static void AppendAlphaStats(StringBuilder sb, int width, int height, Color32[] pixels) {
        int total = pixels.Length;
        int minAlpha = 255;
        int maxAlpha = 0;
        int zeroAlpha = 0;
        int fullAlpha = 0;
        int partialAlpha = 0;
        var uniqueAlpha = new HashSet<byte>();

        for (int i = 0; i < pixels.Length; ++i) {
            byte a = pixels[i].a;
            minAlpha = Mathf.Min(minAlpha, a);
            maxAlpha = Mathf.Max(maxAlpha, a);
            uniqueAlpha.Add(a);

            if (a == 0)
                ++zeroAlpha;
            else if (a == 255)
                ++fullAlpha;
            else
                ++partialAlpha;
        }

        sb.AppendLine($"  size={width}x{height}, pixels={total}");
        sb.AppendLine($"  alphaMin={minAlpha}, alphaMax={maxAlpha}, uniqueAlphaCount={uniqueAlpha.Count}");
        sb.AppendLine($"  alphaZero={zeroAlpha}, alphaPartial={partialAlpha}, alphaFull={fullAlpha}");

        if (minAlpha == 255 && maxAlpha == 255)
            sb.AppendLine("  Result: fully opaque alpha. This stage has no transparency.");
        else
            sb.AppendLine("  Result: alpha contains transparency.");
    }
}

#endif // UNITY_EDITOR

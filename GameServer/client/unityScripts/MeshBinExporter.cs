// Unity Editor script: Tools > Export Mesh Bin
// Exports a single MeshFilter, ParticleSystemRenderer mesh, or direct Mesh asset
// to the .meshbin format
// used by the DX12 engine's loadMeshBin().
//
// Usage:
//   1. Select a GameObject with a MeshFilter or ParticleSystemRenderer.
//   2. Open Tools > Export Mesh Bin.
//   3. Fill in the texture path and click Export.
//
// .meshbin v1 format (legacy, no vertex color):
//   [u8 x 8]  magic = "MESHBIN\0"
//   [u8]      version = 1
//   [u32]     vertexCount
//   [u32]     indexCount
//   [struct { float3 pos, float2 uv }] x vertexCount   (stride = 20B)
//   [u16] x indexCount
//   [u8]      texturePathLen
//   [char x texturePathLen]   (path relative to ../resources/Textures/, '/' separator)
//
// .meshbin v2 format (adds optional vertex color):
//   [u8 x 8]  magic = "MESHBIN\0"
//   [u8]      version = 2
//   [u8]      flags  (bit 0 = hasColor)
//   [u32]     vertexCount
//   [u32]     indexCount
//   [struct { float3 pos, float2 uv }] x vertexCount   (stride = 20B)
//   [struct { float4 color }] x vertexCount             (only if flags & 1, stride = 16B)
//   [u16] x indexCount
//   [u8]      texturePathLen
//   [char x texturePathLen]   (path relative to ../resources/Textures/, '/' separator)

#if UNITY_EDITOR

using System;
using System.IO;
using UnityEditor;
using UnityEngine;

public class MeshBinExporter : EditorWindow {

    private MeshFilter             targetMeshFilter;
    private ParticleSystemRenderer targetParticleRenderer;
    private Mesh                   targetMesh;
    private int                    subMeshIndex  = 0;
    private string                 texturePath   = "Smoke12.dds";
    private string                 outputPath    = "";
    private bool                   flipZ         = false;  // enable if mesh appears mirrored in engine
    private bool                   exportColor   = true;   // export vertex colors as v2 (COLOR0 channel)

    [MenuItem("Tools/Export Mesh Bin")]
    public static void ShowWindow() {
        GetWindow<MeshBinExporter>("Export Mesh Bin");
    }

    private void OnGUI() {
        GUILayout.Label("Mesh Bin Exporter", EditorStyles.boldLabel);
        EditorGUILayout.Space();

        targetMeshFilter = (MeshFilter)EditorGUILayout.ObjectField(
            "Mesh Filter", targetMeshFilter, typeof(MeshFilter), true);

        targetParticleRenderer = (ParticleSystemRenderer)EditorGUILayout.ObjectField(
            "Particle Renderer", targetParticleRenderer, typeof(ParticleSystemRenderer), true);

        targetMesh = (Mesh)EditorGUILayout.ObjectField(
            "Mesh Asset", targetMesh, typeof(Mesh), false);

        subMeshIndex = EditorGUILayout.IntField("Sub Mesh Index", subMeshIndex);

        texturePath = EditorGUILayout.TextField(
            "Texture Path (relative to Textures/)", texturePath);

        flipZ        = EditorGUILayout.Toggle("Flip Z (if mirrored in engine)", flipZ);
        exportColor  = EditorGUILayout.Toggle("Export Vertex Colors (v2)", exportColor);

        EditorGUILayout.Space();

        if (GUILayout.Button("Use Selected GameObject")) {
            if (Selection.activeGameObject != null) {
                targetMeshFilter = Selection.activeGameObject.GetComponent<MeshFilter>();
                targetParticleRenderer = Selection.activeGameObject.GetComponent<ParticleSystemRenderer>();
                targetMesh = null;

                if (targetMeshFilter == null && targetParticleRenderer == null)
                    Debug.LogWarning("MeshBinExporter: selected GameObject has no MeshFilter or ParticleSystemRenderer.");
            }
        }

        Mesh resolvedMesh = ResolveMesh();
        using (new EditorGUI.DisabledScope(true)) {
            EditorGUILayout.ObjectField("Resolved Mesh", resolvedMesh, typeof(Mesh), false);
        }

        EditorGUILayout.Space();

        if (GUILayout.Button("Choose Output File")) {
            string defaultName = resolvedMesh != null ? SanitizeFileName(resolvedMesh.name) : "SwordSlash";
            outputPath = EditorUtility.SaveFilePanel(
                "Save .meshbin", Application.dataPath, defaultName, "meshbin");
        }

        if (!string.IsNullOrEmpty(outputPath))
            EditorGUILayout.LabelField("Output:", outputPath);

        EditorGUILayout.Space();

        GUI.enabled = resolvedMesh != null && !string.IsNullOrEmpty(outputPath);
        if (GUILayout.Button("Export")) {
            Export(resolvedMesh, texturePath, outputPath, flipZ, exportColor, subMeshIndex);
        }
        GUI.enabled = true;
    }

    public static void Export(MeshFilter mf, string texPath, string outputPath, bool flipZ,
                              bool exportColor = false) {
        Mesh mesh = mf.sharedMesh;
        if (mesh == null) {
            Debug.LogError("MeshBinExporter: MeshFilter has no mesh.");
            return;
        }

        Export(mesh, texPath, outputPath, flipZ, exportColor, 0);
    }

    public static void Export(Mesh mesh, string texPath, string outputPath, bool flipZ,
                              bool exportColor = false, int subMeshIndex = 0) {
        if (mesh == null) {
            Debug.LogError("MeshBinExporter: mesh is null.");
            return;
        }

        // Validate index limit
        if (mesh.vertexCount > 65535) {
            Debug.LogError($"MeshBinExporter: vertexCount {mesh.vertexCount} exceeds u16 limit (65535). Export aborted.");
            return;
        }

        if (subMeshIndex < 0 || subMeshIndex >= mesh.subMeshCount) {
            Debug.LogError($"MeshBinExporter: subMeshIndex {subMeshIndex} is out of range. subMeshCount={mesh.subMeshCount}.");
            return;
        }

        Vector3[] positions = mesh.vertices;
        Vector2[] uvs       = mesh.uv;
        Color[]   colors    = mesh.colors;
        int[]     triangles = mesh.GetTriangles(subMeshIndex);

        // UV fallback
        if (uvs == null || uvs.Length != positions.Length) {
            uvs = new Vector2[positions.Length];
            Debug.LogWarning("MeshBinExporter: mesh has no UVs or UV count mismatch. Using (0,0) fallback.");
        }

        // Color fallback: if mesh has no colors, use white (1,1,1,1)
        bool hasColor = exportColor && colors != null && colors.Length == positions.Length;
        if (exportColor && !hasColor) {
            Debug.LogWarning("MeshBinExporter: exportColor=true but mesh has no vertex colors. " +
                             "Exporting v2 with white (1,1,1,1) fallback.");
            colors  = new Color[positions.Length];
            for (int i = 0; i < colors.Length; ++i) colors[i] = Color.white;
            hasColor = true;
        }

        int vertexCount = positions.Length;
        int indexCount  = triangles.Length;

        Vector2 uvMin = new Vector2(float.PositiveInfinity, float.PositiveInfinity);
        Vector2 uvMax = new Vector2(float.NegativeInfinity, float.NegativeInfinity);
        for (int i = 0; i < uvs.Length; ++i) {
            uvMin = Vector2.Min(uvMin, uvs[i]);
            uvMax = Vector2.Max(uvMax, uvs[i]);
        }

        // Normalize texture path
        texPath = texPath.Replace('\\', '/');
        if (texPath.Length > 255) {
            Debug.LogError("MeshBinExporter: texturePath too long (>255 chars). Export aborted.");
            return;
        }

        using var writer = new BinaryWriter(File.Open(outputPath, FileMode.Create));

        // magic "MESHBIN\0" (8 bytes)
        writer.Write(System.Text.Encoding.ASCII.GetBytes("MESHBIN"));
        writer.Write((byte)0);

        // version + flags
        bool useV2 = exportColor;
        if (useV2) {
            writer.Write((byte)2);                          // version = 2
            writer.Write((byte)(hasColor ? 1 : 0));        // flags: bit0 = hasColor
        } else {
            writer.Write((byte)1);                          // version = 1
        }

        // counts
        writer.Write((uint)vertexCount);
        writer.Write((uint)indexCount);

        // interleaved pos(float3) + uv(float2)
        // UV y-flip: Unity (0,0)=bottom-left -> DX12 (0,0)=top-left
        for (int i = 0; i < vertexCount; ++i) {
            Vector3 p = positions[i];
            if (flipZ) p.z = -p.z;
            writer.Write(p.x);
            writer.Write(p.y);
            writer.Write(p.z);
            writer.Write(uvs[i].x);
            writer.Write(1.0f - uvs[i].y);  // flip UV.y
        }

        // color channel (v2 only, flat array after pos+uv block)
        if (useV2 && hasColor) {
            for (int i = 0; i < vertexCount; ++i) {
                writer.Write(colors[i].r);
                writer.Write(colors[i].g);
                writer.Write(colors[i].b);
                writer.Write(colors[i].a);
            }
        }

        // indices — if flipZ, reverse winding for each triangle
        for (int i = 0; i < triangles.Length; i += 3) {
            if (flipZ) {
                writer.Write((ushort)triangles[i]);
                writer.Write((ushort)triangles[i + 2]);
                writer.Write((ushort)triangles[i + 1]);
            } else {
                writer.Write((ushort)triangles[i]);
                writer.Write((ushort)triangles[i + 1]);
                writer.Write((ushort)triangles[i + 2]);
            }
        }

        // texture path
        byte[] texPathBytes = System.Text.Encoding.UTF8.GetBytes(texPath);
        writer.Write((byte)texPathBytes.Length);
        writer.Write(texPathBytes);

        Debug.Log($"MeshBinExporter: exported '{mesh.name}' -> {outputPath} "
                + $"(v{(useV2 ? 2 : 1)}, subMesh={subMeshIndex}, "
                + $"{vertexCount} verts, {indexCount} indices, "
                + $"color={hasColor}, tex={texPath}, flipZ={flipZ}, "
                + $"bounds={mesh.bounds}, uvMin={uvMin}, uvMax={uvMax})");
    }

    private Mesh ResolveMesh() {
        if (targetMesh != null)
            return targetMesh;

        if (targetParticleRenderer != null && targetParticleRenderer.mesh != null)
            return targetParticleRenderer.mesh;

        if (targetMeshFilter != null)
            return targetMeshFilter.sharedMesh;

        return null;
    }

    private static string SanitizeFileName(string name) {
        foreach (char c in Path.GetInvalidFileNameChars())
            name = name.Replace(c, '_');

        return name;
    }
}

#endif // UNITY_EDITOR

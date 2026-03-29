// Unity Editor script: Tools > Export Mesh Bin
// Exports a single MeshFilter + its first sub-mesh to the .meshbin v1 format
// used by the DX12 engine's loadMeshBin().
//
// Usage:
//   1. Select a GameObject with a MeshFilter (and optionally a MeshRenderer).
//   2. Open Tools > Export Mesh Bin.
//   3. Fill in the texture path and click Export.
//
// .meshbin v1 format:
//   [u8 x 8]  magic = "MESHBIN\0"
//   [u8]      version = 1
//   [u32]     vertexCount
//   [u32]     indexCount
//   [struct { float3 pos, float2 uv }] x vertexCount   (stride = 20B)
//   [u16] x indexCount
//   [u8]      texturePathLen
//   [char x texturePathLen]   (path relative to ../resources/Textures/, '/' separator)

#if UNITY_EDITOR

using System;
using System.IO;
using UnityEditor;
using UnityEngine;

public class MeshBinExporter : EditorWindow {

    private MeshFilter  targetMeshFilter;
    private string      texturePath    = "SwordSlash.dds";
    private string      outputPath     = "";
    private bool        flipZ          = false;  // enable if mesh appears mirrored in engine

    [MenuItem("Tools/Export Mesh Bin")]
    public static void ShowWindow() {
        GetWindow<MeshBinExporter>("Export Mesh Bin");
    }

    private void OnGUI() {
        GUILayout.Label("Mesh Bin Exporter (v1)", EditorStyles.boldLabel);
        EditorGUILayout.Space();

        targetMeshFilter = (MeshFilter)EditorGUILayout.ObjectField(
            "Mesh Filter", targetMeshFilter, typeof(MeshFilter), true);

        texturePath = EditorGUILayout.TextField(
            "Texture Path (relative to Textures/)", texturePath);

        flipZ = EditorGUILayout.Toggle("Flip Z (if mirrored in engine)", flipZ);

        EditorGUILayout.Space();

        if (GUILayout.Button("Choose Output File")) {
            outputPath = EditorUtility.SaveFilePanel(
                "Save .meshbin", Application.dataPath, "SwordSlash", "meshbin");
        }

        if (!string.IsNullOrEmpty(outputPath))
            EditorGUILayout.LabelField("Output:", outputPath);

        EditorGUILayout.Space();

        GUI.enabled = targetMeshFilter != null && !string.IsNullOrEmpty(outputPath);
        if (GUILayout.Button("Export")) {
            Export(targetMeshFilter, texturePath, outputPath, flipZ);
        }
        GUI.enabled = true;
    }

    public static void Export(MeshFilter mf, string texPath, string outputPath, bool flipZ) {
        Mesh mesh = mf.sharedMesh;
        if (mesh == null) {
            Debug.LogError("MeshBinExporter: MeshFilter has no mesh.");
            return;
        }

        // Validate index limit
        if (mesh.vertexCount > 65535) {
            Debug.LogError($"MeshBinExporter: vertexCount {mesh.vertexCount} exceeds u16 limit (65535). Export aborted.");
            return;
        }

        Vector3[] positions = mesh.vertices;
        Vector2[] uvs       = mesh.uv;
        int[]     triangles = mesh.GetTriangles(0);  // first sub-mesh only

        // UV arrays can be empty for meshes without UVs
        if (uvs == null || uvs.Length != positions.Length) {
            uvs = new Vector2[positions.Length];  // zero-filled fallback
            Debug.LogWarning("MeshBinExporter: mesh has no UVs or UV count mismatch. Using (0,0) fallback.");
        }

        int vertexCount = positions.Length;
        int indexCount  = triangles.Length;

        // Normalize texture path: ensure forward slashes
        texPath = texPath.Replace('\\', '/');
        if (texPath.Length > 255) {
            Debug.LogError("MeshBinExporter: texturePath too long (>255 chars). Export aborted.");
            return;
        }

        using var writer = new BinaryWriter(File.Open(outputPath, FileMode.Create));

        // magic "MESHBIN\0" (8 bytes)
        writer.Write(System.Text.Encoding.ASCII.GetBytes("MESHBIN"));
        writer.Write((byte)0);

        // version
        writer.Write((byte)1);

        // counts
        writer.Write((uint)vertexCount);
        writer.Write((uint)indexCount);

        // interleaved vertices: position(float3) + uv(float2)
        // UV y-flip: Unity (0,0)=bottom-left -> DX12 (0,0)=top-left
        for (int i = 0; i < vertexCount; ++i) {
            Vector3 p = positions[i];
            if (flipZ) p.z = -p.z;

            writer.Write(p.x);
            writer.Write(p.y);
            writer.Write(p.z);

            writer.Write(uvs[i].x);
            writer.Write(1.0f - uvs[i].y);  // always flip UV.y
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
                + $"({vertexCount} verts, {indexCount} indices, tex={texPath}, flipZ={flipZ})");
    }
}

#endif // UNITY_EDITOR

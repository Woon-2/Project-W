using UnityEngine;
using UnityEditor;
using System.IO;
using System.Text;
using System.Collections;
using System.Collections.Generic;

public class TerrainExtractor : EditorWindow
{
    class TextureMapping
    {
        public string tag;          // Height, Splat, Diffuse...
        public string sourcePath;   // Unity에서 추출된 경로
        public string mappedPath;   // 유저가 입력하는 엔진 경로
    }

    private Terrain terrain;
    private string exportPath = "TerrainExport";

    private List<TextureMapping> mappings = new List<TextureMapping>();
    private bool exportDone = false;

    private string engineBasePath = "Assets/Terrain/";
    private string metaFileName = "terrain_meta.bin";
    private string manifestFileName = "terrain_manifest.bin";

    private Vector2 scrollPos;

    [MenuItem("Tools/Terrain Extractor")]
    public static void ShowWindow()
    {
        GetWindow<TerrainExtractor>("Terrain Extractor");
    }

    private void OnGUI()
    {
        terrain = (Terrain)EditorGUILayout.ObjectField("Terrain", terrain, typeof(Terrain), true);

        EditorGUILayout.Space();
        EditorGUILayout.LabelField("Engine Path Settings", EditorStyles.boldLabel);

        engineBasePath = EditorGUILayout.TextField("Engine Base Path", engineBasePath);
        metaFileName = EditorGUILayout.TextField("Meta File Name", metaFileName);
        manifestFileName = EditorGUILayout.TextField("Manifest File Name", manifestFileName);

        exportPath = EditorGUILayout.TextField("Export Path", exportPath);

        if (GUILayout.Button("Export"))
        {
            if (terrain == null)
            {
                Debug.LogError("Terrain not assigned!");
                return;
            }

            ExportTerrain();
        }

        if (exportDone)
        {
            EditorGUILayout.Space();
            EditorGUILayout.LabelField("Path Remapping", EditorStyles.boldLabel);

            scrollPos = EditorGUILayout.BeginScrollView(scrollPos, GUILayout.Height(300));

            foreach (var m in mappings)
            {
                EditorGUILayout.BeginVertical("box");

                EditorGUILayout.LabelField($"[{m.tag}]");

                EditorGUILayout.LabelField("Source", m.sourcePath);
                m.mappedPath = EditorGUILayout.TextField("Mapped", m.mappedPath);

                EditorGUILayout.EndVertical();
            }

            EditorGUILayout.EndScrollView();

            if (GUILayout.Button("Generate Manifest"))
            {
                ExportManifestWithMapping();
            }

            if (GUILayout.Button("Apply Base Path"))
            {
                foreach (var m in mappings)
                {
                    string fileName = Path.GetFileName(m.mappedPath);
                    m.mappedPath = engineBasePath + fileName;
                }
            }
        }
    }

    void ExportTerrain()
    {
        var data = terrain.terrainData;

        Directory.CreateDirectory(exportPath);
        mappings.Clear();

        ExportHeightmap(data);
        ExportSplatmap(data);
        ExportLayers(data);
        ExportMetadata(data);

        exportDone = true;

        Debug.Log("Export Complete. Now configure paths.");
    }

    // ------------------------
    // Heightmap (.raw)
    // ------------------------
    void ExportHeightmap(TerrainData data)
    {
        int res = data.heightmapResolution;
        float[,] heights = data.GetHeights(0, 0, res, res);

        string fileName = "height.raw";
        string path = Path.Combine(exportPath, fileName);

        mappings.Add(new TextureMapping
        {
            tag = "HeightMap",
            sourcePath = path,
            mappedPath = engineBasePath + fileName
        });

        using (BinaryWriter writer = new BinaryWriter(File.Open(path, FileMode.Create)))
        {
            for (int y = 0; y < res; y++)
            {
                for (int x = 0; x < res; x++)
                {
                    ushort value = (ushort)(heights[y, x] * 65535);
                    writer.Write(value);
                }
            }
        }
    }

    // ------------------------
    // Splatmap (RGBA PNG)
    // ------------------------
    void ExportSplatmap(TerrainData data)
    {
        int width = data.alphamapWidth;
        int height = data.alphamapHeight;
        int layers = data.alphamapLayers;

        float[,,] alphamaps = data.GetAlphamaps(0, 0, width, height);

        int textureCount = Mathf.CeilToInt(layers / 4.0f);

        for (int t = 0; t < textureCount; t++)
        {
            Texture2D tex = new Texture2D(width, height, TextureFormat.RGBA32, false);

            for (int y = 0; y < height; y++)
            {
                for (int x = 0; x < width; x++)
                {
                    float r = 0, g = 0, b = 0, a = 0;

                    int baseLayer = t * 4;

                    if (baseLayer + 0 < layers) r = alphamaps[y, x, baseLayer + 0];
                    if (baseLayer + 1 < layers) g = alphamaps[y, x, baseLayer + 1];
                    if (baseLayer + 2 < layers) b = alphamaps[y, x, baseLayer + 2];
                    if (baseLayer + 3 < layers) a = alphamaps[y, x, baseLayer + 3];

                    tex.SetPixel(x, y, new Color(r, g, b, a));
                }
            }

            tex.Apply();

            byte[] png = tex.EncodeToPNG();

            string fileName = $"splat_{t}.png";

            mappings.Add(new TextureMapping
            {
                tag = "Splat",
                sourcePath = Path.Combine(exportPath, fileName),
                mappedPath = engineBasePath + $"splat_{t}.dds"
            });

            File.WriteAllBytes(Path.Combine(exportPath, fileName), png);
        }
    }

    // ------------------------
    // Layer Textures
    // ------------------------
    void ExportLayers(TerrainData data)
    {
        var layers = data.terrainLayers;

        for (int i = 0; i < layers.Length; i++)
        {
            var layer = layers[i];

            if (layer.diffuseTexture != null)
            {
                string fileName = $"layer_{i}_diffuse.png";

                mappings.Add(new TextureMapping
                {
                    tag = "Diffuse",
                    sourcePath = Path.Combine(exportPath, fileName),
                    mappedPath = engineBasePath + $"layer_{i}_diffuse.dds"
                });

                SaveTexture(layer.diffuseTexture, fileName, false);
            }

            if (layer.normalMapTexture != null)
            {
                string fileName = $"layer_{i}_normal.png";

                mappings.Add(new TextureMapping
                {
                    tag = "Normal",
                    sourcePath = Path.Combine(exportPath, fileName),
                    mappedPath = engineBasePath + $"layer_{i}_normal.dds"
                });

                SaveTexture(layer.normalMapTexture, fileName, true);
            }
        }
    }

    Texture2D GetReadableTexture(Texture2D source, bool isNormal)
    {
        RenderTextureReadWrite rw = isNormal
            ? RenderTextureReadWrite.Linear
            : RenderTextureReadWrite.sRGB;

        RenderTexture rt = RenderTexture.GetTemporary(
            source.width,
            source.height,
            0,
            RenderTextureFormat.ARGB32,
            rw
        );

        Graphics.Blit(source, rt);

        RenderTexture prev = RenderTexture.active;
        RenderTexture.active = rt;

        Texture2D tex = new Texture2D(
            source.width,
            source.height,
            TextureFormat.RGBA32,
            false,
            isNormal // linear 여부
        );

        tex.ReadPixels(new Rect(0, 0, rt.width, rt.height), 0, 0);
        tex.Apply();

        RenderTexture.active = prev;
        RenderTexture.ReleaseTemporary(rt);

        return tex;
    }

    void SaveTexture(Texture2D source, string filename, bool isNormal = false)
    {
        Texture2D readableTex = GetReadableTexture(source, isNormal);

        byte[] png = readableTex.EncodeToPNG();

        string fullPath = Path.Combine(exportPath, filename);
        File.WriteAllBytes(fullPath, png);

        DestroyImmediate(readableTex);
    }

    // ------------------------
    // Metadata (.bin)
    // ------------------------
    void ExportMetadata(TerrainData data)
    {
        string path = Path.Combine(exportPath, "terrain_meta.bin");

        mappings.Add(new TextureMapping
        {
            tag = "MetaData",
            sourcePath = path,
            mappedPath = engineBasePath + metaFileName
        });

        using (BinaryWriter writer = new BinaryWriter(File.Open(path, FileMode.Create)))
        {
            writer.Write(data.heightmapResolution);
            writer.Write(data.alphamapResolution);

            Vector3 size = data.size;
            writer.Write(size.x);
            writer.Write(size.y);
            writer.Write(size.z);

            writer.Write(data.terrainLayers.Length);

            // Layer info
            foreach (var layer in data.terrainLayers)
            {
                Vector2 tileSize = layer.tileSize;
                Vector2 tileOffset = layer.tileOffset;

                writer.Write(tileSize.x);
                writer.Write(tileSize.y);
                writer.Write(tileOffset.x);
                writer.Write(tileOffset.y);
                writer.Write(layer.metallic);
                writer.Write(1.0f - layer.smoothness);  // roughness = 1 - smoothness
            }
        }
    }

    //void ExportManifest(TerrainData data)
    //{
    //    string path = Path.Combine(exportPath, manifestFileName);

    //    using (BinaryWriter writer = new BinaryWriter(File.Open(path, FileMode.Create)))
    //    {
    //        ExtractUtil.WriteHeadTag(writer, "Terrain");

    //        // ------------------------
    //        // Terrain Name
    //        // ------------------------
    //        ExtractUtil.WriteText(writer, "TerrainName", terrain.name);

    //        // ------------------------
    //        // Height / Meta
    //        // ------------------------
    //        ExtractUtil.WriteText(writer, "HeightMap", engineBasePath + "height.raw");
    //        ExtractUtil.WriteText(writer, "MetaData", engineBasePath + metaFileName);

    //        // ------------------------
    //        // Splat maps
    //        // ------------------------
    //        int splatCount = Mathf.CeilToInt(data.alphamapLayers / 4.0f);
    //        ExtractUtil.WriteInteger(writer, "SplatCount", splatCount);

    //        for (int i = 0; i < splatCount; i++)
    //        {
    //            string pathStr = engineBasePath + $"splat_{i}.dds";
    //            ExtractUtil.WriteText(writer, "SplatPath", pathStr);
    //        }

    //        // ------------------------
    //        // Layers
    //        // ------------------------
    //        var layers = data.terrainLayers;
    //        ExtractUtil.WriteInteger(writer, "LayerCount", layers.Length);

    //        for (int i = 0; i < layers.Length; i++)
    //        {
    //            // diffuse
    //            string diffuse = engineBasePath + $"layer_{i}_diffuse.dds";
    //            ExtractUtil.WriteText(writer, "DiffusePath", diffuse);

    //            // normal
    //            string normal = engineBasePath + $"layer_{i}_normal.dds";
    //            ExtractUtil.WriteText(writer, "NormalPath", normal);
    //        }

    //        ExtractUtil.WriteTailTag(writer, "Terrain");
    //    }
    //}

    void ExportManifestWithMapping()
    {
        string path = Path.Combine(exportPath, manifestFileName);

        using (BinaryWriter writer = new BinaryWriter(File.Open(path, FileMode.Create)))
        {
            ExtractUtil.WriteHeadTag(writer, "Terrain");

            ExtractUtil.WriteText(writer, "TerrainName", terrain.name);

            foreach (var m in mappings)
            {
                switch (m.tag)
                {
                    case "HeightMap":
                        ExtractUtil.WriteText(writer, "HeightMap", m.mappedPath);
                        break;

                    case "MetaData":
                        ExtractUtil.WriteText(writer, "MetaData", m.mappedPath);
                        break;

                    case "Splat":
                        ExtractUtil.WriteText(writer, "SplatPath", m.mappedPath);
                        break;

                    case "Diffuse":
                        ExtractUtil.WriteText(writer, "DiffusePath", m.mappedPath);
                        break;

                    case "Normal":
                        ExtractUtil.WriteText(writer, "NormalPath", m.mappedPath);
                        break;
                }
            }

            ExtractUtil.WriteTailTag(writer, "Terrain");
        }

        Debug.Log("Manifest Generated with remapped paths!");
    }
}
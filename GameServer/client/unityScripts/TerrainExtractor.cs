using UnityEngine;
using UnityEditor;
using System.IO;
using System.Text;
using System.Collections.Generic;

// Batch terrain chunk extractor.
//
// Workflow: place every chunk Terrain as a child of a single parent GameObject
// (e.g. "TerrainRoot"), assign that parent here, and press "Export All Chunks".
// Because the extractor sees all chunks at once, it can:
//   - derive each chunk's grid coord (col,row) from transform.position,
//   - compute 4-neighbor adjacency among existing chunks,
//   - export the shared layer palette exactly once (and validate all chunks share it),
//   - write a single global index file (chunks_index.bin) in one pass.
//
// Output (all in exportPath, flat folder):
//   chunks_index.bin
//   layer_{i}_diffuse.png / layer_{i}_normal.png        (shared palette, once)
//   chunk_{col}_{row}_height.raw                         (per chunk)
//   chunk_{col}_{row}_splat0.png                         (per chunk)
// PNG -> DDS conversion remains an external step; the index records .dds paths.
public class TerrainExtractor : EditorWindow
{
    private Transform terrainRoot;
    private string exportPath      = "TerrainExport";
    private string engineBasePath  = "Assets/Terrain/";
    private string indexFileName   = "chunks_index.bin";

    private const int kMaxLayers = 4;   // unified palette: <= 4 layers, single splat texture

    [MenuItem("Tools/Terrain Extractor")]
    public static void ShowWindow()
    {
        GetWindow<TerrainExtractor>("Terrain Extractor");
    }

    private void OnGUI()
    {
        terrainRoot = (Transform)EditorGUILayout.ObjectField(
            "Terrain Root", terrainRoot, typeof(Transform), true);

        EditorGUILayout.Space();
        EditorGUILayout.LabelField("Engine Path Settings", EditorStyles.boldLabel);
        engineBasePath = EditorGUILayout.TextField("Engine Base Path", engineBasePath);
        indexFileName  = EditorGUILayout.TextField("Index File Name", indexFileName);
        exportPath     = EditorGUILayout.TextField("Export Path", exportPath);

        EditorGUILayout.Space();
        if (GUILayout.Button("Export All Chunks"))
        {
            if (terrainRoot == null)
            {
                Debug.LogError("[TerrainExtractor] Terrain Root not assigned!");
                return;
            }
            ExportAllChunks();
        }
    }

    // ------------------------------------------------------------------
    // Per-chunk record gathered before writing the index
    // ------------------------------------------------------------------
    class ChunkRecord
    {
        public Terrain terrain;
        public int   col, row;
        public float sizeX, sizeY, sizeZ;
        public int   resolution;
        public int   alphamapResolution;
        public List<Vector2Int> neighbors = new List<Vector2Int>();
        public string heightPath;   // engine path (.raw)
        public string splatPath;    // engine path (.dds)
    }

    // ==================================================================
    // Main entry
    // ==================================================================
    void ExportAllChunks()
    {
        Directory.CreateDirectory(exportPath);

        Terrain[] terrains = terrainRoot.GetComponentsInChildren<Terrain>();
        if (terrains.Length == 0)
        {
            Debug.LogError("[TerrainExtractor] No Terrain components found under the root.");
            return;
        }

        TerrainData first = terrains[0].terrainData;
        float chunkSizeX = first.size.x;
        float chunkSizeZ = first.size.z;

        // 1) Compute grid coords, validate uniform cell size, detect coord collisions.
        var records  = new List<ChunkRecord>();
        var coordSet = new HashSet<Vector2Int>();
        foreach (var t in terrains)
        {
            TerrainData d = t.terrainData;
            if (Mathf.Abs(d.size.x - chunkSizeX) > 1e-3f ||
                Mathf.Abs(d.size.z - chunkSizeZ) > 1e-3f)
            {
                Debug.LogError($"[TerrainExtractor] Chunk size mismatch on '{t.name}': " +
                    $"expected ({chunkSizeX},{chunkSizeZ}), got ({d.size.x},{d.size.z}). Aborting.");
                return;
            }

            int col = Mathf.RoundToInt(t.transform.position.x / chunkSizeX);
            int row = Mathf.RoundToInt(t.transform.position.z / chunkSizeZ);
            var coord = new Vector2Int(col, row);
            if (coordSet.Contains(coord))
            {
                Debug.LogError($"[TerrainExtractor] Duplicate chunk coord ({col},{row}) at '{t.name}'. " +
                    "Two terrains map to the same grid cell. Aborting.");
                return;
            }
            coordSet.Add(coord);

            records.Add(new ChunkRecord
            {
                terrain            = t,
                col                = col,
                row                = row,
                sizeX              = d.size.x,
                sizeY              = d.size.y,
                sizeZ              = d.size.z,
                resolution         = d.heightmapResolution,
                alphamapResolution = d.alphamapResolution
            });
        }

        // 2) 4-neighbor adjacency among existing chunks only (sparse-safe).
        foreach (var r in records)
        {
            TryAddNeighbor(coordSet, r, r.col + 1, r.row);
            TryAddNeighbor(coordSet, r, r.col - 1, r.row);
            TryAddNeighbor(coordSet, r, r.col,     r.row + 1);
            TryAddNeighbor(coordSet, r, r.col,     r.row - 1);
        }

        // 3) Shared palette: enforce that every chunk uses the same layer palette, export once.
        TerrainLayer[] paletteLayers = first.terrainLayers;
        if (paletteLayers.Length > kMaxLayers)
        {
            Debug.LogError($"[TerrainExtractor] Palette has {paletteLayers.Length} layers; " +
                $"engine supports at most {kMaxLayers} (single splat). Aborting.");
            return;
        }
        string paletteSig = PaletteSignature(paletteLayers);
        foreach (var t in terrains)
        {
            if (PaletteSignature(t.terrainData.terrainLayers) != paletteSig)
            {
                Debug.LogError($"[TerrainExtractor] Chunk '{t.name}' uses a different layer palette. " +
                    "All chunks must share one unified palette. Aborting.");
                return;
            }
        }
        ExportPalette(paletteLayers);

        // 4) Per-chunk heightmap + splat.
        foreach (var r in records)
        {
            r.heightPath = engineBasePath + $"chunk_{r.col}_{r.row}_height.raw";
            r.splatPath  = engineBasePath + $"chunk_{r.col}_{r.row}_splat0.dds";
            ExportHeightmap(r);
            ExportSplatmap(r);
        }

        // 5) Single global index.
        WriteIndex(records, paletteLayers);

        Debug.Log($"[TerrainExtractor] Export complete: {records.Count} chunks, " +
            $"{Mathf.Min(paletteLayers.Length, kMaxLayers)} shared layers. " +
            "Convert PNGs to DDS, then load via ChunkManager.");
    }

    void TryAddNeighbor(HashSet<Vector2Int> set, ChunkRecord r, int c, int rr)
    {
        var coord = new Vector2Int(c, rr);
        if (set.Contains(coord)) r.neighbors.Add(coord);
    }

    // Stable identity for a layer palette: per-layer asset paths + tiling/material scalars.
    string PaletteSignature(TerrainLayer[] layers)
    {
        var sb = new StringBuilder();
        foreach (var ly in layers)
        {
            string dp = ly != null && ly.diffuseTexture   != null ? AssetDatabase.GetAssetPath(ly.diffuseTexture)   : "null";
            string np = ly != null && ly.normalMapTexture != null ? AssetDatabase.GetAssetPath(ly.normalMapTexture) : "null";
            sb.Append(dp).Append('|').Append(np).Append('|');
            if (ly != null)
            {
                sb.Append(ly.tileSize.x).Append(',').Append(ly.tileSize.y).Append('|')
                  .Append(ly.tileOffset.x).Append(',').Append(ly.tileOffset.y).Append('|')
                  .Append(ly.metallic).Append('|').Append(ly.smoothness);
            }
            sb.Append(";;");
        }
        return sb.ToString();
    }

    // ------------------------------------------------------------------
    // Shared palette textures (exported once)
    // ------------------------------------------------------------------
    void ExportPalette(TerrainLayer[] layers)
    {
        int count = Mathf.Min(layers.Length, kMaxLayers);
        for (int i = 0; i < count; i++)
        {
            var layer = layers[i];
            if (layer != null && layer.diffuseTexture != null)
                SaveTexture(layer.diffuseTexture, $"layer_{i}_diffuse.png", false);
            if (layer != null && layer.normalMapTexture != null)
                SaveTexture(layer.normalMapTexture, $"layer_{i}_normal.png", true);
        }
    }

    // ------------------------------------------------------------------
    // Per-chunk heightmap (.raw, ushort, y-outer / x-inner)
    // ------------------------------------------------------------------
    void ExportHeightmap(ChunkRecord r)
    {
        TerrainData data = r.terrain.terrainData;
        int res = data.heightmapResolution;
        float[,] heights = data.GetHeights(0, 0, res, res);

        string path = Path.Combine(exportPath, $"chunk_{r.col}_{r.row}_height.raw");
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

    // ------------------------------------------------------------------
    // Per-chunk splat map (RGBA PNG, first 4 layers only)
    // ------------------------------------------------------------------
    void ExportSplatmap(ChunkRecord r)
    {
        TerrainData data = r.terrain.terrainData;
        int width  = data.alphamapWidth;
        int height = data.alphamapHeight;
        int layers = data.alphamapLayers;

        float[,,] alphamaps = data.GetAlphamaps(0, 0, width, height);

        Texture2D tex = new Texture2D(width, height, TextureFormat.RGBA32, false);
        for (int y = 0; y < height; y++)
        {
            for (int x = 0; x < width; x++)
            {
                float cr = 0, cg = 0, cb = 0, ca = 0;
                if (0 < layers) cr = alphamaps[y, x, 0];
                if (1 < layers) cg = alphamaps[y, x, 1];
                if (2 < layers) cb = alphamaps[y, x, 2];
                if (3 < layers) ca = alphamaps[y, x, 3];
                tex.SetPixel(x, y, new Color(cr, cg, cb, ca));
            }
        }
        tex.Apply();

        byte[] png = tex.EncodeToPNG();
        File.WriteAllBytes(Path.Combine(exportPath, $"chunk_{r.col}_{r.row}_splat0.png"), png);
        DestroyImmediate(tex);
    }

    // ------------------------------------------------------------------
    // Global index file
    // ------------------------------------------------------------------
    void WriteIndex(List<ChunkRecord> records, TerrainLayer[] layers)
    {
        string path = Path.Combine(exportPath, indexFileName);
        int L = Mathf.Min(layers.Length, kMaxLayers);

        using (BinaryWriter w = new BinaryWriter(File.Open(path, FileMode.Create)))
        {
            ExtractUtil.WriteHeadTag(w, "ChunkIndex");
            ExtractUtil.WriteInteger(w, "Version", 1);

            // ---- shared palette ----
            ExtractUtil.WriteInteger(w, "LayerCount", L);
            for (int i = 0; i < L; i++)
            {
                var ly = layers[i];
                ExtractUtil.WriteText (w, "DiffusePath", engineBasePath + $"layer_{i}_diffuse.dds");
                ExtractUtil.WriteText (w, "NormalPath",  engineBasePath + $"layer_{i}_normal.dds");
                ExtractUtil.WriteFloat(w, "TileSizeX",   ly != null ? ly.tileSize.x   : 1f);
                ExtractUtil.WriteFloat(w, "TileSizeY",   ly != null ? ly.tileSize.y   : 1f);
                ExtractUtil.WriteFloat(w, "TileOffsetX", ly != null ? ly.tileOffset.x : 0f);
                ExtractUtil.WriteFloat(w, "TileOffsetY", ly != null ? ly.tileOffset.y : 0f);
                ExtractUtil.WriteFloat(w, "Metallic",    ly != null ? ly.metallic     : 0f);
                ExtractUtil.WriteFloat(w, "Roughness",   ly != null ? 1.0f - ly.smoothness : 0.85f);
            }

            // ---- chunk records ----
            ExtractUtil.WriteInteger(w, "ChunkCount", records.Count);
            foreach (var r in records)
            {
                ExtractUtil.WriteHeadTag(w, "Chunk");
                ExtractUtil.WriteInteger(w, "Col", r.col);
                ExtractUtil.WriteInteger(w, "Row", r.row);
                ExtractUtil.WriteFloat  (w, "SizeX", r.sizeX);
                ExtractUtil.WriteFloat  (w, "SizeY", r.sizeY);
                ExtractUtil.WriteFloat  (w, "SizeZ", r.sizeZ);
                ExtractUtil.WriteInteger(w, "Resolution", r.resolution);
                ExtractUtil.WriteInteger(w, "AlphamapResolution", r.alphamapResolution);
                ExtractUtil.WriteInteger(w, "NeighborCount", r.neighbors.Count);
                foreach (var n in r.neighbors)
                {
                    ExtractUtil.WriteInteger(w, "NCol", n.x);
                    ExtractUtil.WriteInteger(w, "NRow", n.y);
                }
                ExtractUtil.WriteText(w, "HeightPath", r.heightPath);
                ExtractUtil.WriteText(w, "SplatPath",  r.splatPath);
                ExtractUtil.WriteTailTag(w, "Chunk");
            }

            // ---- stronghold records (gameplay; consumed by the server) ----
            var marks = Object.FindObjectsByType<StrongholdMarker>(FindObjectsSortMode.InstanceID);
            ExtractUtil.WriteInteger(w, "StrongholdCount", marks.Length);
            for (int s = 0; s < marks.Length; s++)
            {
                var m = marks[s];
                var t = m.transform;
                ExtractUtil.WriteHeadTag(w, "Stronghold");
                ExtractUtil.WriteInteger(w, "Id", s);                       // id = export index (stable per export)
                ExtractUtil.WriteFloat(w, "CenterX", t.position.x);
                ExtractUtil.WriteFloat(w, "CenterY", t.position.y);
                ExtractUtil.WriteFloat(w, "CenterZ", t.position.z);
                ExtractUtil.WriteFloat(w, "OrientX", t.rotation.x);
                ExtractUtil.WriteFloat(w, "OrientY", t.rotation.y);
                ExtractUtil.WriteFloat(w, "OrientZ", t.rotation.z);
                ExtractUtil.WriteFloat(w, "OrientW", t.rotation.w);
                ExtractUtil.WriteFloat(w, "ScaleX", t.lossyScale.x);
                ExtractUtil.WriteFloat(w, "ScaleY", t.lossyScale.y);
                ExtractUtil.WriteFloat(w, "ScaleZ", t.lossyScale.z);
                ExtractUtil.WriteFloat(w, "ActivityRadius", m.activityRadius);
                ExtractUtil.WriteFloat(w, "SpawnRadius", m.spawnRadius);
                ExtractUtil.WriteInteger(w, "MaxHp", m.maxHp);
                ExtractUtil.WriteFloat(w, "RespawnDelaySec", m.respawnDelaySec);
                ExtractUtil.WriteInteger(w, "PopulationCount", m.populations.Count);
                foreach (var pop in m.populations)
                {
                    ExtractUtil.WriteInteger(w, "MonsterType", (int)pop.type);
                    ExtractUtil.WriteInteger(w, "TargetCount", pop.targetCount);
                    ExtractUtil.WriteInteger(w, "MaxPerWave", pop.maxPerWave);
                    ExtractUtil.WriteFloat(w, "RespawnIntervalSec", pop.respawnIntervalSec);
                }
                ExtractUtil.WriteTailTag(w, "Stronghold");
            }

            // ---- zone records (trigger volumes; consumed by server + client) ----
            var zoneMarks = Object.FindObjectsByType<ZoneMarker>(FindObjectsSortMode.InstanceID);
            ExtractUtil.WriteInteger(w, "ZoneCount", zoneMarks.Length);
            for (int z = 0; z < zoneMarks.Length; z++)
            {
                var zmk = zoneMarks[z];
                var zt  = zmk.transform;
                ExtractUtil.WriteHeadTag(w, "Zone");
                ExtractUtil.WriteInteger(w, "Id", z);                  // id = export index (stable per export)
                ExtractUtil.WriteText(w, "Tag", zmk.zoneTag);
                ExtractUtil.WriteInteger(w, "FactionMask", (int)zmk.factions);
                ExtractUtil.WriteInteger(w, "VolumeCount", zmk.volumes.Count);

                Vector3 absScale = new Vector3(
                    Mathf.Abs(zt.lossyScale.x),
                    Mathf.Abs(zt.lossyScale.y),
                    Mathf.Abs(zt.lossyScale.z));
                float maxScale = Mathf.Max(absScale.x, Mathf.Max(absScale.y, absScale.z));

                foreach (var v in zmk.volumes)
                {
                    // Bake each volume to world space (the engine has no transform hierarchy).
                    Vector3    wc = zt.TransformPoint(v.localCenter);
                    Quaternion wq = zt.rotation * Quaternion.Euler(v.rotationEuler);
                    Vector3    half = Vector3.Scale(v.size, absScale) * 0.5f;

                    ExtractUtil.WriteInteger(w, "Shape", (int)v.shape);
                    ExtractUtil.WriteFloat(w, "CenterX", wc.x);
                    ExtractUtil.WriteFloat(w, "CenterY", wc.y);
                    ExtractUtil.WriteFloat(w, "CenterZ", wc.z);
                    ExtractUtil.WriteFloat(w, "OrientX", wq.x);
                    ExtractUtil.WriteFloat(w, "OrientY", wq.y);
                    ExtractUtil.WriteFloat(w, "OrientZ", wq.z);
                    ExtractUtil.WriteFloat(w, "OrientW", wq.w);
                    ExtractUtil.WriteFloat(w, "HalfX", half.x);
                    ExtractUtil.WriteFloat(w, "HalfY", half.y);
                    ExtractUtil.WriteFloat(w, "HalfZ", half.z);
                    ExtractUtil.WriteFloat(w, "Radius", v.radius * maxScale);
                }
                ExtractUtil.WriteTailTag(w, "Zone");
            }

            // ---- generic markers (type + name + transform; server + client) ----
            var markerMarks = Object.FindObjectsByType<LevelMarker>(FindObjectsSortMode.InstanceID);
            ExtractUtil.WriteInteger(w, "MarkerCount", markerMarks.Length);
            for (int i = 0; i < markerMarks.Length; i++)
            {
                var mk = markerMarks[i];
                var mt = mk.transform;
                ExtractUtil.WriteHeadTag(w, "Marker");
                ExtractUtil.WriteText(w, "Type", mk.markerType);
                ExtractUtil.WriteText(w, "Name",
                    string.IsNullOrEmpty(mk.markerName) ? mk.gameObject.name : mk.markerName);
                ExtractUtil.WriteFloat(w, "PosX", mt.position.x);
                ExtractUtil.WriteFloat(w, "PosY", mt.position.y);
                ExtractUtil.WriteFloat(w, "PosZ", mt.position.z);
                ExtractUtil.WriteFloat(w, "OrientX", mt.rotation.x);
                ExtractUtil.WriteFloat(w, "OrientY", mt.rotation.y);
                ExtractUtil.WriteFloat(w, "OrientZ", mt.rotation.z);
                ExtractUtil.WriteFloat(w, "OrientW", mt.rotation.w);
                ExtractUtil.WriteFloat(w, "ScaleX", mt.lossyScale.x);
                ExtractUtil.WriteFloat(w, "ScaleY", mt.lossyScale.y);
                ExtractUtil.WriteFloat(w, "ScaleZ", mt.lossyScale.z);
                ExtractUtil.WriteTailTag(w, "Marker");
            }

            ExtractUtil.WriteTailTag(w, "ChunkIndex");
        }
    }

    // ------------------------------------------------------------------
    // Texture readback helpers (unchanged from single-terrain extractor)
    // ------------------------------------------------------------------
    Texture2D GetReadableTexture(Texture2D source, bool isNormal)
    {
        RenderTextureReadWrite rw = isNormal
            ? RenderTextureReadWrite.Linear
            : RenderTextureReadWrite.sRGB;

        RenderTexture rt = RenderTexture.GetTemporary(
            source.width, source.height, 0,
            RenderTextureFormat.ARGB32, rw);

        Graphics.Blit(source, rt);

        RenderTexture prev = RenderTexture.active;
        RenderTexture.active = rt;

        Texture2D tex = new Texture2D(
            source.width, source.height,
            TextureFormat.RGBA32, false, isNormal);

        tex.ReadPixels(new Rect(0, 0, rt.width, rt.height), 0, 0);
        tex.Apply();

        RenderTexture.active = prev;
        RenderTexture.ReleaseTemporary(rt);
        return tex;
    }

    void SaveTexture(Texture2D source, string filename, bool isNormal)
    {
        Texture2D readableTex = GetReadableTexture(source, isNormal);
        byte[] png = readableTex.EncodeToPNG();
        File.WriteAllBytes(Path.Combine(exportPath, filename), png);
        DestroyImmediate(readableTex);
    }
}

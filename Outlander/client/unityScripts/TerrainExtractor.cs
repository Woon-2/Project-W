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
    private const int kIndexVersion = 3; // v2: ScatterPrototypes + per-chunk Scatter. v3: per-instance
                                         //     orientation is a full quaternion (Rot) instead of Yaw,
                                         //     baking Detail "Align To Ground" terrain-normal tilt.

    // ---- scatter (Paint Tree / Paint Detail) settings ----
    private bool  includeTrees   = true;
    private bool  includeDetails = true;
    private float detailDensityScale = 1.0f;        // extra 0..1 multiplier on terrain.detailObjectDensity
    private int   maxDetailInstancesPerChunk = 20000; // PER detail layer, PER chunk safety cap
    private List<ScatterProtoEntry> protoEntries = new List<ScatterProtoEntry>();
    private Vector2 protoScroll;
    private bool loggedDetailSample = false;   // one-shot position-range sanity log

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
        EditorGUILayout.LabelField("Scatter (Paint Tree / Paint Detail)", EditorStyles.boldLabel);
        includeTrees   = EditorGUILayout.Toggle("Include Trees", includeTrees);
        includeDetails = EditorGUILayout.Toggle("Include Details", includeDetails);
        detailDensityScale = EditorGUILayout.Slider("Detail Density Scale", detailDensityScale, 0f, 1f);
        maxDetailInstancesPerChunk = EditorGUILayout.IntField("Max / Detail Type / Chunk", maxDetailInstancesPerChunk);

        EditorGUILayout.Space();
        if (GUILayout.Button("Scan Prototypes"))
        {
            if (terrainRoot == null)
                Debug.LogError("[TerrainExtractor] Terrain Root not assigned!");
            else
                ScanPrototypes();
        }

        // Editable prototype name list. The Name is the MAPPING KEY: it must match the
        // name used when the model was extracted with ModelExtractor (targetName).
        if (protoEntries.Count > 0)
        {
            EditorGUILayout.LabelField($"Prototypes ({protoEntries.Count}) — Name = ModelExtractor targetName",
                EditorStyles.miniBoldLabel);
            protoScroll = EditorGUILayout.BeginScrollView(protoScroll, GUILayout.MaxHeight(240f));
            for (int i = 0; i < protoEntries.Count; i++)
            {
                var pe = protoEntries[i];
                EditorGUILayout.BeginHorizontal();
                EditorGUILayout.LabelField(KindLabel(pe.kind), GUILayout.Width(70f));
                pe.name = EditorGUILayout.TextField(pe.name);
                EditorGUILayout.LabelField(pe.sourceName, GUILayout.Width(140f));
                EditorGUILayout.EndHorizontal();
            }
            EditorGUILayout.EndScrollView();
        }

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

    static string KindLabel(int kind)
    {
        switch (kind)
        {
            case 0:  return "Tree";
            case 1:  return "MeshDtl";
            case 2:  return "Billboard";
            default: return "?";
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
        public List<ScatterInstanceRec> scatter = new List<ScatterInstanceRec>();
    }

    // ------------------------------------------------------------------
    // Scatter prototype (global dedup) + per-instance record
    // ------------------------------------------------------------------
    class ScatterProtoEntry
    {
        public string key;          // dedup key: kind + ":" + assetPath
        public int    kind;         // 0=TreeMesh, 1=MeshDetail, 2=BillboardGrass
        public string name;         // user-editable mapping key (= ModelExtractor targetName)
        public string sourceName;   // original prefab/texture name (display only)
        public Texture2D texture;   // billboard only (kind==2)
        public float  bbWidth = 1f, bbHeight = 1f;
        public Color  tint = Color.white;
        public string texturePath;  // engine path (.dds), filled at export (kind==2)
    }

    struct ScatterInstanceRec
    {
        public int        protoIdx;
        public Vector3    posLocal;   // chunk-local (world - chunk worldOffset)
        public Quaternion rot;        // full orientation (yaw + Detail ground-align tilt)
        public float      scaleW, scaleH;
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

        // 4.5) Scatter: ensure prototypes scanned, export billboard textures, gather instances.
        if (includeTrees || includeDetails)
        {
            if (protoEntries.Count == 0) ScanPrototypes();
            ExportScatterTextures();
            if (includeDetails) LogDetailSettings();

            var keyToIdx = new Dictionary<string, int>();
            for (int i = 0; i < protoEntries.Count; i++) keyToIdx[protoEntries[i].key] = i;

            loggedDetailSample = false;
            int totalInstances = 0;
            var perProto = new int[protoEntries.Count];
            foreach (var r in records)
            {
                GatherScatter(r, chunkSizeX, chunkSizeZ, keyToIdx);
                totalInstances += r.scatter.Count;
                foreach (var inst in r.scatter)
                    if (inst.protoIdx >= 0 && inst.protoIdx < perProto.Length) perProto[inst.protoIdx]++;
            }
            Debug.Log($"[TerrainExtractor] Scatter: {protoEntries.Count} prototypes, " +
                $"{totalInstances} instances across {records.Count} chunks.");
            // Per-prototype breakdown so a runaway detail layer is easy to spot.
            for (int i = 0; i < protoEntries.Count; i++)
            {
                var pe = protoEntries[i];
                Debug.Log($"  [{KindLabel(pe.kind)}] {pe.name} ({pe.sourceName}): {perProto[i]} instances");
            }
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

    // ------------------------------------------------------------------
    // Scatter: prototype scan + per-chunk instance gather
    // ------------------------------------------------------------------

    // Stable dedup key for a tree prefab / detail mesh / detail texture.
    static string ProtoKey(int kind, Object asset)
    {
        string p = AssetDatabase.GetAssetPath(asset);
        if (string.IsNullOrEmpty(p)) p = asset.name;
        return kind + ":" + p;
    }

    // Scan all terrains under the root for tree/detail prototypes, dedup globally,
    // and populate the editable name list. Preserves user-edited names across rescans.
    void ScanPrototypes()
    {
        var prevNames = new Dictionary<string, string>();
        foreach (var pe in protoEntries) prevNames[pe.key] = pe.name;

        protoEntries.Clear();
        var seen = new HashSet<string>();

        foreach (var t in terrainRoot.GetComponentsInChildren<Terrain>())
        {
            var d = t.terrainData;

            if (includeTrees)
            {
                foreach (var tp in d.treePrototypes)
                {
                    if (tp.prefab == null) continue;
                    string key = ProtoKey(0, tp.prefab);
                    if (!seen.Add(key)) continue;
                    protoEntries.Add(new ScatterProtoEntry {
                        key = key, kind = 0,
                        name = prevNames.TryGetValue(key, out var nm) ? nm : tp.prefab.name,
                        sourceName = tp.prefab.name
                    });
                }
            }

            if (includeDetails)
            {
                foreach (var dp in d.detailPrototypes)
                {
                    if (dp.usePrototypeMesh)
                    {
                        if (dp.prototype == null) continue;
                        string key = ProtoKey(1, dp.prototype);
                        if (!seen.Add(key)) continue;
                        protoEntries.Add(new ScatterProtoEntry {
                            key = key, kind = 1,
                            name = prevNames.TryGetValue(key, out var nm) ? nm : dp.prototype.name,
                            sourceName = dp.prototype.name
                        });
                    }
                    else
                    {
                        if (dp.prototypeTexture == null) continue;
                        string key = ProtoKey(2, dp.prototypeTexture);
                        if (!seen.Add(key)) continue;
                        protoEntries.Add(new ScatterProtoEntry {
                            key = key, kind = 2,
                            name = prevNames.TryGetValue(key, out var nm) ? nm : dp.prototypeTexture.name,
                            sourceName = dp.prototypeTexture.name,
                            texture  = dp.prototypeTexture,
                            // Per-instance scaleXZ/scaleY (from ComputeDetailInstanceTransforms) already
                            // carry the world size, so the billboard quad is unit-sized (1x1).
                            bbWidth  = 1f,
                            bbHeight = 1f,
                            tint     = dp.healthyColor
                        });
                    }
                }
            }
        }

        Debug.Log($"[TerrainExtractor] Scanned {protoEntries.Count} scatter prototypes. " +
            "Edit each Name to match its ModelExtractor targetName.");
    }

    // Gather scattered tree + detail instances for one chunk, chunk-local.
    void GatherScatter(ChunkRecord r, float chunkSizeX, float chunkSizeZ,
        Dictionary<string, int> keyToIdx)
    {
        var d    = r.terrain.terrainData;
        var tpos = r.terrain.transform.position;
        var worldOffset = new Vector3(r.col * chunkSizeX, 0f, r.row * chunkSizeZ);

        // --- trees (discrete instances) ---
        if (includeTrees)
        {
            foreach (var ti in d.treeInstances)
            {
                if (ti.prototypeIndex < 0 || ti.prototypeIndex >= d.treePrototypes.Length) continue;
                var prefab = d.treePrototypes[ti.prototypeIndex].prefab;
                if (prefab == null) continue;
                if (!keyToIdx.TryGetValue(ProtoKey(0, prefab), out int gIdx)) continue;

                Vector3 worldPos = tpos + Vector3.Scale(ti.position, d.size);
                // Trees only carry a Y rotation (TreeInstance.rotation is radians) and never
                // align to the ground, so the orientation is a pure yaw quaternion.
                r.scatter.Add(new ScatterInstanceRec {
                    protoIdx = gIdx,
                    posLocal = worldPos - worldOffset,
                    rot      = Quaternion.AngleAxis(ti.rotation * Mathf.Rad2Deg, Vector3.up),
                    scaleW   = ti.widthScale,
                    scaleH   = ti.heightScale
                });
            }
        }

        // --- details ---
        // Use Unity's own ComputeDetailInstanceTransforms so the result matches exactly
        // what Unity renders (handles Coverage vs InstanceCount mode, detail resolution,
        // per-patch layout and density). This avoids the coverage-value-as-count bug.
        if (includeDetails)
        {
            // Detail map is divided into (detailResolution / detailResolutionPerPatch)^2 patches.
            int patchCount = Mathf.Max(1, d.detailResolution / Mathf.Max(1, d.detailResolutionPerPatch));
            // Match the terrain's rendered density (0..1), optionally thinned further by the slider.
            float density = Mathf.Clamp01(r.terrain.detailObjectDensity * detailDensityScale);

            for (int layer = 0; layer < d.detailPrototypes.Length; ++layer)
            {
                var dp = d.detailPrototypes[layer];
                int kind; Object asset;
                if (dp.usePrototypeMesh) { if (dp.prototype == null) continue; kind = 1; asset = dp.prototype; }
                else                      { if (dp.prototypeTexture == null) continue; kind = 2; asset = dp.prototypeTexture; }
                if (!keyToIdx.TryGetValue(ProtoKey(kind, asset), out int gIdx)) continue;

                // "Align To Ground (%)": blend the detail's up vector toward the terrain surface
                // normal by this fraction. ComputeDetailInstanceTransforms only returns yaw, so we
                // bake the ground-normal tilt here (Unity applies it at render time, not in the
                // transform). Mesh details tilt; billboard grass (kind==2) stays upright.
                float alignToGround = 0f;
#if UNITY_2022_2_OR_NEWER
                alignToGround = Mathf.Clamp01(dp.alignToGround);
#endif

                int producedThisLayer = 0;
                for (int py = 0; py < patchCount && producedThisLayer < maxDetailInstancesPerChunk; ++py)
                {
                    for (int px = 0; px < patchCount && producedThisLayer < maxDetailInstancesPerChunk; ++px)
                    {
                        Bounds patchBounds;
                        // var avoids naming the (nested) DetailInstanceTransform type.
                        var xforms = d.ComputeDetailInstanceTransforms(px, py, layer, density, out patchBounds);
                        if (xforms == null) continue;
                        foreach (var it in xforms)
                        {
                            if (producedThisLayer >= maxDetailInstancesPerChunk) break;
                            // posX/posY/posZ are terrain-local; offset by the terrain world origin.
                            Vector3 worldPos = tpos + new Vector3(it.posX, it.posY, it.posZ);

                            if (!loggedDetailSample)
                            {
                                loggedDetailSample = true;
                                Debug.Log($"[TerrainExtractor] Detail sample (sanity): local=({it.posX:F2},{it.posY:F2},{it.posZ:F2}) " +
                                    $"terrainSize=({d.size.x:F0},{d.size.y:F0},{d.size.z:F0}) " +
                                    $"rotY={it.rotationY:F2} scaleXZ={it.scaleXZ:F2} scaleY={it.scaleY:F2}  " +
                                    "(posX/posZ should range 0..terrainSize)");
                            }

                            Quaternion yawRot = Quaternion.AngleAxis(it.rotationY * Mathf.Rad2Deg, Vector3.up);
                            Quaternion rot = yawRot;
                            if (kind == 1 && alignToGround > 0f)
                            {
                                float u  = Mathf.Clamp01(it.posX / Mathf.Max(1e-4f, d.size.x));
                                float vv = Mathf.Clamp01(it.posZ / Mathf.Max(1e-4f, d.size.z));
                                Vector3 n         = d.GetInterpolatedNormal(u, vv);
                                Vector3 alignedUp = Vector3.Slerp(Vector3.up, n, alignToGround);
                                rot = Quaternion.FromToRotation(Vector3.up, alignedUp) * yawRot;
                            }

                            r.scatter.Add(new ScatterInstanceRec {
                                protoIdx = gIdx,
                                posLocal = worldPos - worldOffset,
                                rot      = rot,
                                scaleW   = it.scaleXZ,
                                scaleH   = it.scaleY
                            });
                            producedThisLayer++;
                        }
                    }
                }
            }
        }
    }

    // Diagnostic: log the first terrain's detail settings (cheap). Detail instance counts
    // come from ComputeDetailInstanceTransforms, so we no longer need a per-cell density scan.
    void LogDetailSettings()
    {
        var terrains = terrainRoot.GetComponentsInChildren<Terrain>();
        if (terrains.Length == 0) return;
        var d0 = terrains[0].terrainData;
        string modeStr = "InstanceCount(assumed/legacy)";
#if UNITY_2022_2_OR_NEWER
        modeStr = d0.detailScatterMode.ToString();
#endif
        Debug.Log($"[TerrainExtractor] Detail settings (terrain '{terrains[0].name}'): " +
            $"scatterMode={modeStr}, detailResolution={d0.detailResolution}, " +
            $"detailResolutionPerPatch={d0.detailResolutionPerPatch}, " +
            $"detailObjectDensity={terrains[0].detailObjectDensity}, prototypes={d0.detailPrototypes.Length}");
    }

    // Export billboard textures (once per kind==2 prototype) and fill their texturePath.
    void ExportScatterTextures()
    {
        for (int i = 0; i < protoEntries.Count; i++)
        {
            var pe = protoEntries[i];
            if (pe.kind != 2 || pe.texture == null) continue;
            string baseName = $"scatter_{i}_{SanitizeFileName(pe.name)}";
            SaveTexture(pe.texture, baseName + ".png", false);
            pe.texturePath = engineBasePath + baseName + ".dds";
        }
    }

    static string SanitizeFileName(string s)
    {
        foreach (var c in Path.GetInvalidFileNameChars()) s = s.Replace(c, '_');
        return s.Replace(' ', '_');
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
            ExtractUtil.WriteInteger(w, "Version", kIndexVersion);

            // ---- shared palette ----
            ExtractUtil.WriteInteger(w, "LayerCount", L);
            for (int i = 0; i < L; i++)
            {
                var ly = layers[i];
                ExtractUtil.WriteText  (w, "DiffusePath", engineBasePath + $"layer_{i}_diffuse.dds");
                ExtractUtil.WriteText  (w, "NormalPath",  engineBasePath + $"layer_{i}_normal.dds");
                ExtractUtil.WriteVector(w, "TileSize",    ly != null ? ly.tileSize   : Vector2.one);
                ExtractUtil.WriteVector(w, "TileOffset",  ly != null ? ly.tileOffset : Vector2.zero);
                ExtractUtil.WriteFloat (w, "Metallic",    ly != null ? ly.metallic     : 0f);
                ExtractUtil.WriteFloat (w, "Roughness",   ly != null ? 1.0f - ly.smoothness : 0.85f);
            }

            // ---- scatter prototypes (global dedup table; index version >= 2) ----
            ExtractUtil.WriteInteger(w, "PrototypeCount", protoEntries.Count);
            foreach (var pe in protoEntries)
            {
                ExtractUtil.WriteHeadTag(w, "ScatterPrototype");
                ExtractUtil.WriteText   (w, "Name", pe.name);
                ExtractUtil.WriteInteger(w, "Kind", pe.kind);
                ExtractUtil.WriteText   (w, "TexturePath", pe.kind == 2 ? (pe.texturePath ?? "") : "");
                ExtractUtil.WriteVector (w, "BillboardSize", new Vector2(pe.bbWidth, pe.bbHeight));
                ExtractUtil.WriteColor  (w, "Tint", pe.tint);
                ExtractUtil.WriteTailTag(w, "ScatterPrototype");
            }

            // ---- chunk records ----
            ExtractUtil.WriteInteger(w, "ChunkCount", records.Count);
            foreach (var r in records)
            {
                ExtractUtil.WriteHeadTag(w, "Chunk");
                ExtractUtil.WriteInteger(w, "Col", r.col);
                ExtractUtil.WriteInteger(w, "Row", r.row);
                ExtractUtil.WriteVector (w, "Size", new Vector3(r.sizeX, r.sizeY, r.sizeZ));
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

                // ---- per-chunk scatter instances (index version >= 2) ----
                ExtractUtil.WriteInteger(w, "InstanceCount", r.scatter.Count);
                foreach (var inst in r.scatter)
                {
                    ExtractUtil.WriteInteger   (w, "ProtoIdx", inst.protoIdx);
                    ExtractUtil.WriteVector    (w, "PosLocal", inst.posLocal);
                    ExtractUtil.WriteQuaternion(w, "Rot", inst.rot);
                    ExtractUtil.WriteVector    (w, "Scale", new Vector2(inst.scaleW, inst.scaleH));
                }

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
                ExtractUtil.WriteVector(w, "Center", t.position);
                ExtractUtil.WriteQuaternion(w, "Orient", t.rotation);
                ExtractUtil.WriteVector(w, "Scale", t.lossyScale);
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
                    ExtractUtil.WriteVector(w, "Center", wc);
                    ExtractUtil.WriteQuaternion(w, "Orient", wq);
                    ExtractUtil.WriteVector(w, "Half", half);
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
                ExtractUtil.WriteVector(w, "Pos", mt.position);
                ExtractUtil.WriteQuaternion(w, "Orient", mt.rotation);
                ExtractUtil.WriteVector(w, "Scale", mt.lossyScale);
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

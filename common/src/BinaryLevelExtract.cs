using System.Collections;
using System.Collections.Generic;
using System.Text;
using UnityEngine;
using UnityEditor;
using System.IO;

public class BinaryLevelExtract : MonoBehaviour
{
    public GameObject terrainGroup;  // Assign the parent GameObject containing all terrain objects in the inspector.
    public GameObject objectGroup;  // Assign the parent GameObject containing all objects in the inspector.

    private List<string> m_pTexturePath = new List<string>();
    private Dictionary<string, (string, int, int, int)> m_pTextureIndexInfo = new Dictionary<string, (string, int, int, int)>();
    private Dictionary<(int, int, int, int), (string, string)> m_mapRefMap = new Dictionary<(int, int, int, int), (string, string)>();

    private void Start()
    {
        if (terrainGroup == null)
        {
            Debug.LogError("TerrainGroup is not assigned!");
            return;
        }

        ReadRemapFile("Remap.bin");
        ArrangeTextureList();
        CalculateResourceIndices();

        using (BinaryWriter writer = new BinaryWriter(
            File.Open(string.Copy(gameObject.name).Replace(" ", "_") + ".bin", FileMode.Create)
        ))
        {
            WriteDictionary(writer);
            ExtractAllTerrainData(writer);
            ExtractAllObjectsData(writer);
        }
    }
    /*
    private void ExtractAllTerrainData(BinaryWriter writer)
    {
        Terrain[] terrains = terrainGroup.GetComponentsInChildren<Terrain>();

        writer.Write("<Chunks:>");
        writer.Write(terrains.Length);


        foreach (var terrain in terrains)
        {
            ExtractTerrainData(terrain, writer);
        }

        writer.Write("</Chunks>");
    }
    */

    private void ExtractAllTerrainData(BinaryWriter writer)
    {
        Terrain[] terrains = terrainGroup.GetComponentsInChildren<Terrain>();

        int maxX = 0, maxY = 0;

        foreach (var terrain in terrains)
        {
            string[] parts = terrain.name.Split('_');
            if (parts.Length == 3)
            {
                if (int.TryParse(parts[1], out int x) && int.TryParse(parts[2], out int y))
                {
                    maxX = Mathf.Max(maxX, x);
                    maxY = Mathf.Max(maxY, y);
                }
            }
        }

        Terrain[,] terrainArray = new Terrain[maxX + 1, maxY + 1];

        foreach (var terrain in terrains)
        {
            string[] parts = terrain.name.Split('_');
            if (parts.Length == 3)
            {
                if (int.TryParse(parts[1], out int x) && int.TryParse(parts[2], out int y))
                {
                    terrainArray[x, y] = terrain;
                }
            }
        }
   

        writer.Write("<Chunks:>");
        writer.Write(terrains.Length);

        for (int x = 0; x <= maxX; x++)
        {
            for (int y = 0; y <= maxY; y++)
            {
                ExtractTerrainData(terrainArray, x, y, maxX, maxY, writer);
            }
        }

        writer.Write("</Chunks>");
    }

    private void ExtractTerrainData(Terrain[,] terrainArray, int x, int y, int maxX, int maxY, BinaryWriter writer)
    {
        if (terrainArray == null || terrainArray[x, y] == null) return;

        var terrain = terrainArray[x, y];

        TerrainData terrainData = terrain.terrainData;
        if (terrainData == null)
        {
            Debug.LogError($"TerrainData is missing for {terrain.name}");
            return;
        }

        // Extract and save heightmap data
        float[,] heights = terrainData.GetHeights(0, 0, terrainData.heightmapResolution, terrainData.heightmapResolution);

        if (x > 0 && terrainArray[x - 1, y] != null) {
            TerrainData leftTerrainData = terrainArray[x - 1, y].terrainData;
            if (leftTerrainData != null) {
                for (int i = 0; i < leftTerrainData.heightmapResolution; ++i)
                {
                    float[,] leftHeights = leftTerrainData.GetHeights(0, 0, leftTerrainData.heightmapResolution, leftTerrainData.heightmapResolution);
                    heights[i, 0] = Mathf.Max(heights[i, 0], leftHeights[i, leftTerrainData.heightmapResolution - 1]);
                }
            }
        }

        if (x < maxX && terrainArray[x + 1, y] != null)
        {
            TerrainData rightTerrainData = terrainArray[x + 1, y].terrainData;
            if (rightTerrainData != null)
            {
                for (int i = 0; i < rightTerrainData.heightmapResolution; ++i)
                {
                    float[,] rightHeights = rightTerrainData.GetHeights(0, 0, rightTerrainData.heightmapResolution, rightTerrainData.heightmapResolution);
                    heights[i, terrainData.heightmapResolution - 1] = Mathf.Max(heights[i, terrainData.heightmapResolution - 1], rightHeights[i, 0]);
                }
            }
        }

        if (y > 0 && terrainArray[x, y - 1] != null)
        {
            TerrainData downTerrainData = terrainArray[x, y - 1].terrainData;
            if (downTerrainData != null)
            {
                for (int i = 0; i < downTerrainData.heightmapResolution; ++i)
                {
                    float[,] downHeights = downTerrainData.GetHeights(0, 0, downTerrainData.heightmapResolution, downTerrainData.heightmapResolution);
                    heights[0, i] = Mathf.Max(heights[0, i], downHeights[downTerrainData.heightmapResolution - 1, i]);
                }
            }
        }

        if (y < maxY && terrainArray[x, y + 1] != null)
        {
            TerrainData upTerrainData = terrainArray[x, y + 1].terrainData;
            if (upTerrainData != null)
            {
                for (int i = 0; i < upTerrainData.heightmapResolution; ++i)
                {
                    float[,] upHeights = upTerrainData.GetHeights(0, 0, upTerrainData.heightmapResolution, upTerrainData.heightmapResolution);
                    
                    heights[terrainData.heightmapResolution - 1, i] = Mathf.Max(heights[terrainData.heightmapResolution - 1, i], upHeights[0, i]);
                }
            }
        }

        SaveHeightMapAsImage(terrain, heights, terrainData.heightmapResolution, terrain.name);

        writer.Write("<Chunk:>");
        writer.Write(terrain.name);

        // Extract and list albedo and normal map textures with tiling information
        TerrainLayer[] terrainLayers = terrainData.terrainLayers;

        for (int i = 0; i < terrainLayers.Length; i++)
        {
            TerrainLayer layer = terrainLayers[i];
            if (layer != null)
            {
                writer.Write($"<Layer:>");
                writer.Write(i);

                writer.Write("<AlbedoMap:>");
                WriteMapRef(layer.diffuseTexture.name, writer);

                writer.Write("<NormalMap:>");
                WriteMapRef(layer.normalMapTexture.name, writer);

                writer.Write("<Smoothness:>");
                writer.Write(layer.smoothness);

                writer.Write("<Metallic:>");
                writer.Write(layer.metallic);

                Vector2 tileSize = layer.tileSize;
                writer.Write("<TileSize:>");
                writer.Write(tileSize.x);
                writer.Write(tileSize.y);

                Vector2 tileOffset = layer.tileOffset;
                writer.Write("<TileOffset:>");
                writer.Write(tileOffset.x);
                writer.Write(tileOffset.y);
            }
        }
    }

    private void WriteMapRef(string path, BinaryWriter writer)
    {
        if (m_pTextureIndexInfo.TryGetValue(path, out var value))
        {
            // Item2 : resourceTypeIndex -> type
            writer.Write(value.Item2);
            // m_pTexturePath[] path to index -> resourceIndex
            int index = m_pTexturePath.IndexOf(path);
            if (index < 0)
            {
                Debug.LogWarning($"Path not found in m_pTexturePath: {path}");
            }
            writer.Write(index);
            // Item3 : arrayIndex -> arrayIdx
            writer.Write(value.Item3);
            // Item4 : colorSpace -> colorSpace
            writer.Write(value.Item4);
        }
        else
        {
            Debug.LogWarning($"Path not found in texture index info: {path}");
        }
    }

    private void SaveHeightMapAsImage(Terrain terrain, float[,] heights, int resolution, string terrainName)
    {
        Texture2D heightMapTexture = new Texture2D(resolution, resolution, TextureFormat.RGBA32, false, false);

        for (int y = 0; y < resolution; y++)
        {
            for (int x = 0; x < resolution; x++)
            {
                float normalizedHeight = heights[y, x];
                normalizedHeight = heights[y, x] * 30.0f; // Temporary scaling
                uint height = (uint)(normalizedHeight * 4294967296.0f);

                Color32 color = new Color32(
                    (byte)((height >> 24) & 0xFF),
                    (byte)((height >> 16) & 0xFF),
                    (byte)((height >> 8) & 0xFF),
                    (byte)(height & 0xFF)
                );
                heightMapTexture.SetPixel(x, y, color);
            }
        }

        // heightMapTexture.LoadRawTextureData(rawData);
        heightMapTexture.Apply();

        byte[] pngBytes = heightMapTexture.EncodeToPNG();
        File.WriteAllBytes(terrainName + "_HeightMap.png", pngBytes);

        Debug.Log("Height map for " + terrainName + " saved to " + terrainName + "_HeightMap.png");
    }

    private void ExtractAllObjectsData(BinaryWriter writer)
    {
        if (objectGroup == null)
        {
            Debug.LogError("ObjectGroup is not assigned!");
            return;
        }

        ExtractObjectsInHiearchy(objectGroup.transform, writer);
    }

    private void ExtractObjectsInHiearchy(Transform xform, BinaryWriter writer)
    {
        writer.Write("<Node:>");
        ExtractObjectData(xform, writer);

        writer.Write("<Children:>");
        writer.Write(xform.childCount);

        foreach (Transform child in xform)
        {
            ExtractObjectsInHiearchy(child, writer);
        }
        writer.Write("</Node>");
    }

    private void ExtractObjectData(Transform xform, BinaryWriter writer)
    {
        if (xform == null)
        {
            Debug.LogError("Transform is null!");
            return;
        }

        // {objectName}, {transform matrix}, {prefabName}, {isInstance}

        WriteObjectName(xform.gameObject, writer);

        Matrix4x4 matrix = Matrix4x4.identity;
        matrix.SetTRS(xform.position, xform.rotation, xform.localScale);
        WriteMatrix(matrix, writer);

        WriteObjectName(xform.parent.gameObject, writer);

        WriteIsInstance(xform.gameObject, writer);
    }

    private void WriteMatrix(Matrix4x4 matrix, BinaryWriter writer)
    {
        writer.Write(matrix.m00);
        writer.Write(matrix.m10);
        writer.Write(matrix.m20);
        writer.Write(matrix.m30);
        writer.Write(matrix.m01);
        writer.Write(matrix.m11);
        writer.Write(matrix.m21);
        writer.Write(matrix.m31);
        writer.Write(matrix.m02);
        writer.Write(matrix.m12);
        writer.Write(matrix.m22);
        writer.Write(matrix.m32);
        writer.Write(matrix.m03);
        writer.Write(matrix.m13);
        writer.Write(matrix.m23);
        writer.Write(matrix.m33);
    }

    private void WriteObjectName(Object obj, BinaryWriter writer)
    {
        writer.Write((obj) ? string.Copy(obj.name).Replace(" ", "_") : "null");
    }

    private void WriteIsInstance(Object obj, BinaryWriter writer)
    {
        writer.Write(obj.name.StartsWith("GO_"));
    }

    void ReadRemapFile(string path)
    {
        BinaryReader binaryReader = new BinaryReader(File.Open(path, FileMode.Open));

        string convertMapTag = ReadString(binaryReader);
        if (convertMapTag != "<ConvertMap:>")
        {
            throw new InvalidDataException("Invalid file format: missing <ConvertMap:> tag.");
        }

        int i = 0;

        // Read Items in ConvertMap
        while (true)
        {
            string itemTag = ReadString(binaryReader);
            if (itemTag == "</ConvertMap>")
            {
                break; // End of ConvertMap section
            }
            if (itemTag != "<Item:>")
            {
                throw new InvalidDataException("Invalid file format: missing <Item:> tag.");
            }

            string inputPath = ReadString(binaryReader);
            string newTexPath = ReadString(binaryReader);
            int resourceTypeIndex = binaryReader.ReadInt32();
            int arrayIndex = binaryReader.ReadInt32();
            int colorSpace = binaryReader.ReadInt32();

            m_pTextureIndexInfo.Add(inputPath, (newTexPath, resourceTypeIndex, arrayIndex, colorSpace));
            m_mapRefMap.Add((resourceTypeIndex, i++, arrayIndex, colorSpace), (inputPath, newTexPath));
        }

        binaryReader.Close();
    }

    string ReadString(BinaryReader binaryReader)
    {
        int length = binaryReader.ReadInt32(); // Read string length
        byte[] stringBytes = binaryReader.ReadBytes(length);
        return System.Text.Encoding.UTF8.GetString(stringBytes); // Decode string
    }

    void ArrangeTextureList()
    {
        Terrain[] terrains = terrainGroup.GetComponentsInChildren<Terrain>();

        foreach (var terrain in terrains)
        {
            TerrainData terrainData = terrain.terrainData;
            if (terrainData == null)
            {
                Debug.LogError($"TerrainData is missing for {terrain.name}");
                continue;
            }

            TerrainLayer[] terrainLayers = terrainData.terrainLayers;

            for (int i = 0; i < terrainLayers.Length; i++)
            {
                TerrainLayer layer = terrainLayers[i];
                if (layer != null)
                {
                    Texture2D albedo = layer.diffuseTexture;
                    Texture2D normalMap = layer.normalMapTexture;

                    if (albedo != null && !m_pTexturePath.Contains(albedo.name))
                    {
                        m_pTexturePath.Add(albedo.name);
                    }
                    if (normalMap != null && !m_pTexturePath.Contains(normalMap.name))
                    {
                        m_pTexturePath.Add(normalMap.name);
                    }
                }
            }
        }
    }

    void CalculateResourceIndices()
    {
        var updatedMapRefMap = new Dictionary<(int, int, int, int), (string, string)>();

        foreach (var kvp in m_mapRefMap)
        {
            var oldKey = kvp.Key;       // 현재 키 (int, int, int, int)
            var value = kvp.Value;      // 현재 값 (string, string)

            string inputPath = value.Item1;
            string newPath = value.Item2;

            // inputPath가 m_pTexturePath에 있는지 확인하고 인덱스 찾기
            int newIndex = m_pTexturePath.IndexOf(inputPath);
            if (newIndex == -1)
            {
                Debug.LogWarning($"Value '{inputPath}' not found in m_pTexturePath.");
                continue; // 존재하지 않는 경우 스킵
            }

            // 새로운 키 생성: 두 번째 값(oldKey.Item2)을 newIndex로 교체
            var newKey = (oldKey.Item1, newIndex, oldKey.Item3, oldKey.Item4);

            // 새 딕셔너리에 추가
            updatedMapRefMap[newKey] = value;
        }

        // 기존 딕셔너리를 갱신된 딕셔너리로 교체
        m_mapRefMap = updatedMapRefMap;
    }

    void WriteDictionary(BinaryWriter writer)
    {
        writer.Write("<Dictionary:>");

        foreach (var kvp in m_mapRefMap)
        {
            writer.Write("<Item:>");
            writer.Write(kvp.Key.Item1);    // MapRef
            writer.Write(kvp.Key.Item2);
            writer.Write(kvp.Key.Item3);
            writer.Write(kvp.Key.Item4);
            writer.Write(kvp.Value.Item2);   // newTexPath
        }

        writer.Write("</Dictionary>");
    }
}
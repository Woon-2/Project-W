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

        using ( BinaryWriter writer = new BinaryWriter(
            File.Open(string.Copy(gameObject.name).Replace(" ", "_") + ".bin", FileMode.Create)
        ) )
        {
            WriteDictionary(writer);
            ExtractAllTerrainData(writer);
            ExtractAllObjectsData(writer);
        }
    }

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

    private void ExtractTerrainData(Terrain terrain, BinaryWriter writer)
    {
        if (terrain == null) return;

        TerrainData terrainData = terrain.terrainData;
        if (terrainData == null)
        {
            Debug.LogError($"TerrainData is missing for {terrain.name}");
            return;
        }

        // Extract and save heightmap data
        float[,] heights = terrainData.GetHeights(0, 0, terrainData.heightmapResolution, terrainData.heightmapResolution);
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
        if( m_pTextureIndexInfo.TryGetValue(path, out var value) ) {
            // Item2 : resourceTypeIndex -> type
            writer.Write(value.Item2);
            // m_pTexturePath[] path to index -> resourceIndex
            int index = m_pTexturePath.IndexOf(path);
            if (index < 0) {
                Debug.LogWarning($"Path not found in m_pTexturePath: {path}");
            }
            writer.Write(index);
            // Item3 : arrayIndex -> arrayIdx
            writer.Write(value.Item3);
            // Item4 : colorSpace -> colorSpace
            writer.Write(value.Item4);
        }
        else {
            Debug.LogWarning($"Path not found in texture index info: {path}");
        }
    }

    private void SaveHeightMapAsImage(Terrain terrain, float[,] heights, int resolution, string terrainName)
    {
        Texture2D heightMapTexture = new Texture2D(resolution, resolution, TextureFormat.R8, false);

        for (int y = 0; y < resolution; y++)
        {
            for (int x = 0; x < resolution; x++)
            {
                float normalizedHeight = heights[y, x];
                // temporary
                normalizedHeight *= 20.0f;

                byte grayValue = (byte)(normalizedHeight * 255);  // Convert to 0-255 range
                Color heightColor = new Color32(grayValue, grayValue, grayValue, 255);
                heightMapTexture.SetPixel(x, y, heightColor);
            }
        }
        heightMapTexture.Apply();

        byte[] jpgBytes = heightMapTexture.EncodeToJPG();
        File.WriteAllBytes($"{terrainName}_HeightMap.jpg", jpgBytes);

        Debug.Log($"Height map for {terrainName} saved to {terrainName}_HeightMap.jpg");
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

    void ReadRemapFile(string path) {
        BinaryReader binaryReader = new BinaryReader(File.Open(path, FileMode.Open));

        string convertMapTag = ReadString(binaryReader);
        if (convertMapTag != "<ConvertMap:>") {
            throw new InvalidDataException("Invalid file format: missing <ConvertMap:> tag.");
        }

        int i = 0;

        // Read Items in ConvertMap
        while (true) {
            string itemTag = ReadString(binaryReader);
            if (itemTag == "</ConvertMap>") {
                break; // End of ConvertMap section
            }
            if (itemTag != "<Item:>") {
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

    string ReadString(BinaryReader binaryReader) {
        int length = binaryReader.ReadInt32(); // Read string length
        byte[] stringBytes = binaryReader.ReadBytes(length);
        return System.Text.Encoding.UTF8.GetString(stringBytes); // Decode string
    }

    void ArrangeTextureList() {
        Terrain[] terrains = terrainGroup.GetComponentsInChildren<Terrain>();

        foreach (var terrain in terrains) {
            TerrainData terrainData = terrain.terrainData;
            if (terrainData == null) {
                Debug.LogError($"TerrainData is missing for {terrain.name}");
                continue;
            }

            TerrainLayer[] terrainLayers = terrainData.terrainLayers;

            for (int i = 0; i < terrainLayers.Length; i++) {
                TerrainLayer layer = terrainLayers[i];
                if (layer != null) {
                    Texture2D albedo = layer.diffuseTexture;
                    Texture2D normalMap = layer.normalMapTexture;

                    if (albedo != null && !m_pTexturePath.Contains(albedo.name)) {
                        m_pTexturePath.Add(albedo.name);
                    }
                    if (normalMap != null && !m_pTexturePath.Contains(normalMap.name)) {
                        m_pTexturePath.Add(normalMap.name);
                    }
                }
            }
        }
    }

    void CalculateResourceIndices() {
        var updatedMapRefMap = new Dictionary<(int, int, int, int), (string, string)>();

        foreach (var kvp in m_mapRefMap) {
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

    void WriteDictionary(BinaryWriter writer) {
        writer.Write("<Dictionary:>");

        foreach (var kvp in m_mapRefMap) {
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
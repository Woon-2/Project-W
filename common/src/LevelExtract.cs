using UnityEngine;
using System.IO;

public class TerrainDataExtractor : MonoBehaviour
{
    public GameObject terrainGroup;  // Assign the parent GameObject containing all terrain objects in the inspector.

    private void Start()
    {
        if (terrainGroup == null)
        {
            Debug.LogError("TerrainGroup is not assigned!");
            return;
        }

        ExtractAllTerrainData();
    }

    private void ExtractAllTerrainData()
    {
        Terrain[] terrains = terrainGroup.GetComponentsInChildren<Terrain>();

        foreach (var terrain in terrains)
        {
            ExtractTerrainData(terrain);
        }
    }

    private void ExtractTerrainData(Terrain terrain)
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

        // Extract and list albedo and normal map textures with tiling information
        TerrainLayer[] terrainLayers = terrainData.terrainLayers;
        Debug.Log($"{terrain.name} uses the following textures:");

        for (int i = 0; i < terrainLayers.Length; i++)
        {
            TerrainLayer layer = terrainLayers[i];
            if (layer != null)
            {
                Texture2D albedo = layer.diffuseTexture;
                Texture2D normalMap = layer.normalMapTexture;
                Vector2 tileSize = layer.tileSize;
                Vector2 tileOffset = layer.tileOffset;

                Debug.Log($"Layer {i}: Albedo: {(albedo != null ? albedo.name : "None")}, Normal Map: {(normalMap != null ? normalMap.name : "None")}, Tile Size: {tileSize}, Tile Offset: {tileOffset}");
            }
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
}
using UnityEngine;
using System.IO;

public class TextureNamePrintTerrain : MonoBehaviour
{
    private void Start()
    {
        ExtractAllTerrainData();
    }

    private void ExtractAllTerrainData()
    {
        Terrain[] terrains = GetComponentsInChildren<Terrain>();

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
}
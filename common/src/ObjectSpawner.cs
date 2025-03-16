using UnityEngine;

public class ObjectSpawner : MonoBehaviour
{
    public GameObject[] objectsToSpawn;
    public Terrain[] terrains;
    public int objectCount = 10;
    public string baseName = "Object"; // 기본 이름

    private int objectIndex = 0; // 이름 식별자

    void Awake()
    {
        SpawnObjects();
    }

    void SpawnObjects()
    {
        for (int i = 0; i < objectCount; i++)
        {
            Vector3 randomPosition = GetRandomPositionOnTerrains();
            randomPosition.y += 5.0f;   // temporary y offset
            Quaternion randomRotation = GetRandomRotation();

            GameObject obj = Instantiate(objectsToSpawn[Random.Range(0, objectsToSpawn.Length)], randomPosition, randomRotation);
            obj.name = $"{baseName}_({objectIndex++})";
            obj.transform.SetParent(transform);
        }
    }

    Vector3 GetRandomPositionOnTerrains()
    {
        if (terrains.Length == 0) return Vector3.zero;

        Terrain terrain = terrains[Random.Range(0, terrains.Length)];
        float terrainWidth = terrain.terrainData.size.x;
        float terrainLength = terrain.terrainData.size.z;
        float terrainPosX = terrain.transform.position.x;
        float terrainPosZ = terrain.transform.position.z;

        float randomX = Random.Range(terrainPosX, terrainPosX + terrainWidth);
        float randomZ = Random.Range(terrainPosZ, terrainPosZ + terrainLength);

        float y = terrain.SampleHeight(new Vector3(randomX, 0, randomZ)) + terrain.transform.position.y;

        return new Vector3(randomX, y, randomZ);
    }


    Quaternion GetRandomRotation()
    {
        float randomYRotation = Random.Range(0f, 360f);
        return Quaternion.Euler(0, randomYRotation, 0);
    }
}
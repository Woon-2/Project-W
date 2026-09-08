using System.Collections.Generic;
using UnityEngine;

// Authoring component for a monster-spawner stronghold.
//
// Place this on a GameObject positioned where the stronghold structure should
// stand. TerrainExtractor collects every StrongholdMarker in the scene and
// writes them into chunks_index.bin (the "Stronghold" section). The server
// reads that data and drives monster spawning + structure HP/destruction; the
// client receives stronghold entities over the network (it does not read the
// stronghold section itself).
//
// The structure's ground height (Y) is resolved at runtime on the server via
// the terrain height field, so the authored Y is only a reference.
public class StrongholdMarker : MonoBehaviour
{
    // Ordinals MUST match the engine's ObjectType enum (ServerEngine/protocol.hpp):
    //   Player=0, Goblin=1, Ground=2, Stronghold=3, Snake=4, Mushroom=5, Hobgoblin=6,
    //   Bomber=7, Birdy=8, Slime=9, Treant=10, Grandbaum=11, Isys=12.
    // Only the normal stronghold-spawnable monsters are listed here. Named variants
    // (Hobgoblin/Grandbaum/Isys) are mid-boss encounters driven by arena zone markers,
    // not stronghold populations, so they are intentionally excluded.
    public enum MonsterType
    {
        Goblin   = 1,
        Snake    = 4,
        Mushroom = 5,
        Bomber   = 7,
        Birdy    = 8,
        Slime    = 9,
        Treant   = 10,
    }

    [System.Serializable]
    public struct Population
    {
        public MonsterType type;
        public int   targetCount;          // desired alive count (= pool size for this type)
        public int   maxPerWave;           // max revivals per respawn interval
        public float respawnIntervalSec;   // wait between revival waves
    }

    [Tooltip("Radius monsters may roam from the stronghold center.")]
    public float activityRadius = 40f;
    [Tooltip("Radius within which monsters spawn around the stronghold center.")]
    public float spawnRadius    = 15f;
    [Tooltip("Stronghold structure hit points.")]
    public int   maxHp          = 1000;
    [Tooltip("Seconds before a destroyed stronghold rebuilds.")]
    public float respawnDelaySec = 30f;
    public List<Population> populations = new List<Population>();
}

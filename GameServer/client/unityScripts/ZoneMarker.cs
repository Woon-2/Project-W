using System.Collections.Generic;
using UnityEngine;

// Authoring component for a trigger Zone.
//
// A Zone is the union of one or more primitive volumes (boxes and spheres),
// authored in this GameObject's local space. TerrainExtractor collects every
// ZoneMarker in the scene and writes them into chunks_index.bin (the "Zone"
// section). Both the server (authoritative gameplay zones) and the client
// (local cosmetic zones) read this section; behavior is bound in C++ by `tag`.
//
// factions selects which factions trigger this zone. The serialized mask matches
// the engine factionBit(Faction) layout: Players = 1<<1, Monsters = 1<<2.
public class ZoneMarker : MonoBehaviour
{
    [System.Flags]
    public enum FactionFlags
    {
        Players  = 1 << 1,
        Monsters = 1 << 2,
    }

    public enum Shape { Box = 0, Sphere = 1 }

    [System.Serializable]
    public class ZoneVolume
    {
        public string  name = "";
        public Shape   shape = Shape.Box;
        public Vector3 localCenter = Vector3.zero;   // relative to the marker transform
        public Vector3 size = Vector3.one;           // box full extents (local units)
        public Vector3 rotationEuler = Vector3.zero; // box orientation relative to transform
        public float   radius = 1f;                  // sphere radius (local units)
    }

    // Named zoneTag (not Component.tag) to avoid the Unity built-in tag property.
    [Tooltip("Handler binding key (e.g. \"boss_arena_1\"). Matched in C++ ZoneSystem.")]
    public string zoneTag = "";

    [Tooltip("Which factions trigger this zone.")]
    public FactionFlags factions = FactionFlags.Players;

    public List<ZoneVolume> volumes = new List<ZoneVolume>();
}

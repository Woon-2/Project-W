using UnityEngine;

// Generic placement marker. Put this on a GameObject and set a type (and
// optionally a name); TerrainExtractor bakes its type/name/world-transform into
// chunks_index.bin (the "Marker" section). Both the server and the client read
// the section. Use it for lightweight placements that do not need the Zone or
// Stronghold systems -- e.g. boss spawn points, barrier slabs, FX anchors.
public class LevelMarker : MonoBehaviour
{
    [Tooltip("Free-form category read by gameplay code, e.g. \"BossSpawn\", \"Wall\".")]
    public string markerType = "";

    [Tooltip("Instance name. If left empty, the GameObject name is used at export.")]
    public string markerName = "";
}

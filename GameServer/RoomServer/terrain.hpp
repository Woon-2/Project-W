#ifndef room_server_terrain_hpp
#define room_server_terrain_hpp

#include <filesystem>
#include <vector>

// CPU-side height field data used for physics collision queries.
// Built from height.raw during loadTerrainHeightFieldFromFiles().
struct TerrainHeightField {
    int   resolution = 0;
    float sizeX      = 0.f;
    float sizeY      = 0.f;
    float sizeZ      = 0.f;
    std::vector<float> heights; // normalized [0,1], row-major: heights[y*N + x]

    bool empty() const { return heights.empty(); }

    // Bilinear interpolation. localX in [0,sizeX], localZ in [0,sizeZ].
    // Returns world-space Y (already scaled by sizeY).
    float getHeightAt(float localX, float localZ) const;

    // Central-difference surface normal at (localX, localZ).
    // Always returns a vector with positive Y (upward-facing).
    mu::Vec3 getNormalAt(float localX, float localZ) const;
};

// Loads only the CPU-side height field from the terrain binary files.
// Reads terrain_manifest.bin, terrain_meta.bin, and height.raw.
// No GPU resources are allocated.
TerrainHeightField loadTerrainHeightFieldFromFiles(const std::filesystem::path& terrainDir);

#endif // room_server_terrain_hpp

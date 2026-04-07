#include "rspch.hpp"
#include "terrain.hpp"
#include "binaryImport.hpp"

// ---------------------------------------------------------------------------
// Manifest parsing
// ---------------------------------------------------------------------------

struct TerrainManifest {
    std::string heightMapPath;
    std::string metaPath;
};

static TerrainManifest parseManifest(const std::filesystem::path& manifestPath) {
    TerrainManifest result;

    auto ifs = std::ifstream(manifestPath, std::ios::binary);
    DISPLAY_ERROR_STR(ifs.good(),
        "[Terrain] parseManifest: cannot open " + manifestPath.string(), true);

    readHeadTag(ifs, "Terrain");

    // Skip TerrainName
    readHeadTag(ifs, "TerrainName");
    readString(ifs);
    readTailTag(ifs, "TerrainName");

    while (ifs) {
        auto rawTag = readString(ifs);
        if (isTailTag(rawTag, "Terrain")) break;
        auto tagName = untagHead(rawTag);
        auto value   = readString(ifs);
        readString(ifs); // consume tail tag

        if (tagName == "HeightMap") {
            result.heightMapPath = value;
        } else if (tagName == "MetaData") {
            result.metaPath = value;
        }
        // Other tags (SplatPath, DiffusePath, NormalPath) are skipped
    }

    return result;
}

// ---------------------------------------------------------------------------
// Metadata parsing
// ---------------------------------------------------------------------------

struct TerrainMeta {
    int   heightmapResolution;
    float sizeX, sizeY, sizeZ;
};

static TerrainMeta parseMeta(const std::filesystem::path& metaPath) {
    TerrainMeta m{};
    auto ifs = std::ifstream(metaPath, std::ios::binary);
    DISPLAY_ERROR_STR(ifs.good(),
        "[Terrain] parseMeta: cannot open " + metaPath.string(), true);

    m.heightmapResolution = readInteger(ifs); // heightmapResolution
    readInteger(ifs);                         // alphamapResolution (unused)
    m.sizeX = readFloat(ifs);
    m.sizeY = readFloat(ifs);
    m.sizeZ = readFloat(ifs);
    // Remaining layer data is not needed for CPU physics

    return m;
}

// ---------------------------------------------------------------------------
// Load function
// ---------------------------------------------------------------------------

TerrainHeightField loadTerrainHeightFieldFromFiles(const std::filesystem::path& terrainDir) {
    const auto manifestPath = terrainDir / "terrain_manifest.bin";
    const auto manifest = parseManifest(manifestPath);

    // Resolve a path string: if not absolute or not found as-is, look in terrainDir.
    auto resolvePath = [&](const std::string& pathStr) -> std::filesystem::path {
        auto p = std::filesystem::path(pathStr);
        if (!p.is_absolute() && !std::filesystem::exists(p)) {
            auto fallback = terrainDir / p.filename();
            if (std::filesystem::exists(fallback))
                return fallback;
        }
        return p;
    };

    const auto metaPath = resolvePath(manifest.metaPath);
    const auto meta = parseMeta(metaPath);

    const int N = meta.heightmapResolution;

    const auto heightRawPath = resolvePath(manifest.heightMapPath);
    auto hifs = std::ifstream(heightRawPath, std::ios::binary);
    DISPLAY_ERROR_STR(hifs.good(),
        "[Terrain] loadTerrainHeightFieldFromFiles: cannot open " + heightRawPath.string(), true);

    // Unity writes uint16 heights in y-outer, x-inner order.
    auto rawHeights = std::vector<uint16>(static_cast<size_t>(N) * N);
    hifs.read(reinterpret_cast<char*>(rawHeights.data()),
              static_cast<std::streamsize>(N) * N * sizeof(uint16));

    TerrainHeightField hf;
    hf.resolution = N;
    hf.sizeX = meta.sizeX;
    hf.sizeY = meta.sizeY;
    hf.sizeZ = meta.sizeZ;
    hf.heights.resize(static_cast<size_t>(N) * N);

    for (int y = 0; y < N; ++y)
        for (int x = 0; x < N; ++x)
            hf.heights[y * N + x] = rawHeights[y * N + x] / 65535.f;

    return hf;
}

// ---------------------------------------------------------------------------
// TerrainHeightField methods
// ---------------------------------------------------------------------------

float TerrainHeightField::getHeightAt(float localX, float localZ) const
{
    if (empty()) return 0.f;

    const int N   = resolution;
    const float fx = localX / sizeX * static_cast<float>(N - 1);
    const float fz = localZ / sizeZ * static_cast<float>(N - 1);

    const int ix = std::clamp(static_cast<int>(fx), 0, N - 2);
    const int iz = std::clamp(static_cast<int>(fz), 0, N - 2);
    const float tx = fx - static_cast<float>(ix);
    const float tz = fz - static_cast<float>(iz);

    const float h00 = heights[ iz      * N + ix    ];
    const float h10 = heights[ iz      * N + ix + 1];
    const float h01 = heights[(iz + 1) * N + ix    ];
    const float h11 = heights[(iz + 1) * N + ix + 1];

    const float h0 = h00 + (h10 - h00) * tx;
    const float h1 = h01 + (h11 - h01) * tx;
    return (h0 + (h1 - h0) * tz) * sizeY;
}

mu::Vec3 TerrainHeightField::getNormalAt(float localX, float localZ) const
{
    if (empty()) return mu::Vec3(0.f, 1.f, 0.f);

    const int N  = resolution;
    const float dx = sizeX / static_cast<float>(N - 1);
    const float dz = sizeZ / static_cast<float>(N - 1);

    const int ix = std::clamp(static_cast<int>(std::round(localX / dx)), 0, N - 1);
    const int iz = std::clamp(static_cast<int>(std::round(localZ / dz)), 0, N - 1);

    auto h = [&](int x, int y) -> float {
        return heights[std::clamp(y, 0, N - 1) * N + std::clamp(x, 0, N - 1)];
    };

    const float gx = (dx > 0.f) ? (h(ix - 1, iz) - h(ix + 1, iz)) * sizeY / (2.f * dx) : 0.f;
    const float gz = (dz > 0.f) ? (h(ix, iz - 1) - h(ix, iz + 1)) * sizeY / (2.f * dz) : 0.f;

    return mu::normalize(mu::Vec3(gx, 1.f, gz));
}

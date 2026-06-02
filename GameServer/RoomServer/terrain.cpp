#include "rspch.hpp"
#include "terrain.hpp"
#include "binaryImport.hpp"
#include <algorithm>
#include <cmath>

// ---------------------------------------------------------------------------
// Path resolution (mirror of the client resolveTerrainPath helper)
// ---------------------------------------------------------------------------

static std::filesystem::path resolveTerrainPath(const std::filesystem::path& terrainDir,
                                                 const std::string& pathStr) {
    auto p = std::filesystem::path(pathStr);
    if (!p.is_absolute() && !std::filesystem::exists(p)) {
        auto fallback = terrainDir / p.filename();
        if (std::filesystem::exists(fallback))
            return fallback;
    }
    return p;
}

// ---------------------------------------------------------------------------
// Chunk index parsing (mirror of client terrain.cpp parseChunkIndex).
// The shared palette descriptor is read but discarded; the server has no
// rendering and only needs the per-chunk height data.
// ---------------------------------------------------------------------------

ChunkIndex parseChunkIndex(const std::filesystem::path& terrainDir) {
    ChunkIndex result;

    const auto indexPath = terrainDir / "chunks_index.bin";
    auto ifs = std::ifstream(indexPath, std::ios::binary);
    if (!ifs.good()) {
        gSharedLog << "[Terrain] parseChunkIndex: cannot open " << indexPath
                   << " (terrain disabled)\n";
        return result; // empty -> terrain disabled (safe)
    }

    readHeadTag(ifs, "ChunkIndex");
    result.version = readInteger(ifs, "Version");

    // ---- shared palette (read and discard; render-only) ----
    const int L = readInteger(ifs, "LayerCount");
    for (int i = 0; i < L; ++i) {
        readText(ifs, "DiffusePath");
        readText(ifs, "NormalPath");
        readFloat(ifs, "TileSizeX");
        readFloat(ifs, "TileSizeY");
        readFloat(ifs, "TileOffsetX");
        readFloat(ifs, "TileOffsetY");
        readFloat(ifs, "Metallic");
        readFloat(ifs, "Roughness");
    }

    // ---- chunk records ----
    const int C = readInteger(ifs, "ChunkCount");
    result.chunks.resize(C);
    for (int c = 0; c < C; ++c) {
        auto& e = result.chunks[c];
        readHeadTag(ifs, "Chunk");
        e.col = readInteger(ifs, "Col");
        e.row = readInteger(ifs, "Row");
        e.sizeX = readFloat(ifs, "SizeX");
        e.sizeY = readFloat(ifs, "SizeY");
        e.sizeZ = readFloat(ifs, "SizeZ");
        e.resolution         = readInteger(ifs, "Resolution");
        e.alphamapResolution = readInteger(ifs, "AlphamapResolution");
        const int K = readInteger(ifs, "NeighborCount");
        e.neighbors.reserve(K);
        for (int k = 0; k < K; ++k) {
            const int nc = readInteger(ifs, "NCol");
            const int nr = readInteger(ifs, "NRow");
            e.neighbors.emplace_back(nc, nr);
        }
        e.heightPath = readText(ifs, "HeightPath");
        e.splatPath  = readText(ifs, "SplatPath");
        readTailTag(ifs, "Chunk");
    }

    // ---- stronghold records (gameplay) ----
    const int S = readInteger(ifs, "StrongholdCount");
    result.strongholds.resize(S);
    for (int s = 0; s < S; ++s) {
        auto& sh = result.strongholds[s];
        readHeadTag(ifs, "Stronghold");
        sh.id = readInteger(ifs, "Id");
        const float cx = readFloat(ifs, "CenterX");
        const float cy = readFloat(ifs, "CenterY");
        const float cz = readFloat(ifs, "CenterZ");
        sh.center = mu::Vec3(cx, cy, cz);
        const float ox = readFloat(ifs, "OrientX");
        const float oy = readFloat(ifs, "OrientY");
        const float oz = readFloat(ifs, "OrientZ");
        const float ow = readFloat(ifs, "OrientW");
        sh.orient = mu::NQuat(ox, oy, oz, ow);
        const float sx = readFloat(ifs, "ScaleX");
        const float sy = readFloat(ifs, "ScaleY");
        const float sz = readFloat(ifs, "ScaleZ");
        sh.scale = mu::Vec3(sx, sy, sz);
        sh.activityRadius = readFloat(ifs, "ActivityRadius");
        sh.spawnRadius    = readFloat(ifs, "SpawnRadius");
        sh.maxHp          = readInteger(ifs, "MaxHp");
        sh.respawnDelay   = Seconds{ readFloat(ifs, "RespawnDelaySec") };
        const int P = readInteger(ifs, "PopulationCount");
        sh.populations.resize(P);
        for (int p = 0; p < P; ++p) {
            auto& pop = sh.populations[p];
            pop.type            = static_cast<ObjectType>(readInteger(ifs, "MonsterType"));
            pop.targetCount     = readInteger(ifs, "TargetCount");
            pop.maxPerWave      = readInteger(ifs, "MaxPerWave");
            pop.respawnInterval = Seconds{ readFloat(ifs, "RespawnIntervalSec") };
        }
        readTailTag(ifs, "Stronghold");
    }

    readTailTag(ifs, "ChunkIndex");

    gSharedLog << "[Terrain] Chunk index parsed: " << C << " chunks, "
               << S << " strongholds\n";
    return result;
}

// ---------------------------------------------------------------------------
// Per-chunk height field loading (CPU only).
// Unity writes uint16 heights in y-outer, x-inner order.
// ---------------------------------------------------------------------------

TerrainHeightField loadChunkHeightField(const ChunkIndexEntry& entry,
                                        const std::filesystem::path& terrainDir) {
    const int N = entry.resolution;

    const auto heightRawPath = resolveTerrainPath(terrainDir, entry.heightPath);
    auto hifs = std::ifstream(heightRawPath, std::ios::binary);
    DISPLAY_ERROR_STR(hifs.good(),
        "[Terrain] loadChunkHeightField: cannot open " + heightRawPath.string(), true);

    auto rawHeights = std::vector<uint16>(static_cast<size_t>(N) * N);
    hifs.read(reinterpret_cast<char*>(rawHeights.data()),
              static_cast<std::streamsize>(N) * N * sizeof(uint16));

    TerrainHeightField hf;
    hf.resolution = N;
    hf.sizeX = entry.sizeX;
    hf.sizeY = entry.sizeY;
    hf.sizeZ = entry.sizeZ;
    hf.heights.resize(static_cast<size_t>(N) * N);

    for (int y = 0; y < N; ++y)
        for (int x = 0; x < N; ++x)
            hf.heights[y * N + x] = rawHeights[y * N + x] / 65535.f;

    return hf;
}

// ---------------------------------------------------------------------------
// TerrainChunkManager (server: load-all, shared, no streaming)
// ---------------------------------------------------------------------------

void TerrainChunkManager::init(const std::filesystem::path& terrainDir) {
    index_ = parseChunkIndex(terrainDir);
    if (index_.chunks.empty()) {
        gSharedLog << "[Terrain] No chunks loaded (terrain disabled).\n";
        dumpLog();
        return;
    }

    chunkSizeX_ = index_.chunks[0].sizeX;
    chunkSizeZ_ = index_.chunks[0].sizeZ;

    for (const auto& e : index_.chunks) {
        if (e.sizeX != chunkSizeX_ || e.sizeZ != chunkSizeZ_) {
            gSharedLog << "[Terrain] WARNING: chunk (" << e.col << "," << e.row
                       << ") size mismatch (" << e.sizeX << "," << e.sizeZ
                       << ") vs (" << chunkSizeX_ << "," << chunkSizeZ_
                       << ") -- world routing may be incorrect.\n";
        }
        TerrainHeightField hf = loadChunkHeightField(e, terrainDir);
        heightFields_.emplace(packCoord(e.col, e.row), std::move(hf));
    }

    gSharedLog << "[Terrain] Loaded " << heightFields_.size()
               << " chunk height fields (shared, read-only).\n";
    dumpLog();
}

std::optional<std::pair<int, int>>
TerrainChunkManager::chunkCoordAtWorld(float x, float z) const {
    if (chunkSizeX_ <= 0.f || chunkSizeZ_ <= 0.f) return std::nullopt;
    const int col = static_cast<int>(std::floor(x / chunkSizeX_));
    const int row = static_cast<int>(std::floor(z / chunkSizeZ_));
    if (heightFields_.find(packCoord(col, row)) == heightFields_.end())
        return std::nullopt;
    return std::make_pair(col, row);
}

float MU_CALLCONV TerrainChunkManager::heightAtWorld(float x, float z) const {
    auto coord = chunkCoordAtWorld(x, z);
    if (!coord) return 0.f;
    const TerrainHeightField* hf = findHf(coord->first, coord->second);
    if (!hf) return 0.f;
    const mu::Vec3 off = worldOffset(coord->first, coord->second);
    return off.y() + hf->getHeightAt(x - off.x(), z - off.z());
}

mu::Vec3 MU_CALLCONV TerrainChunkManager::normalAtWorld(float x, float z) const {
    auto coord = chunkCoordAtWorld(x, z);
    if (!coord) return mu::Vec3(0.f, 1.f, 0.f);
    const TerrainHeightField* hf = findHf(coord->first, coord->second);
    if (!hf) return mu::Vec3(0.f, 1.f, 0.f);
    const mu::Vec3 off = worldOffset(coord->first, coord->second);
    return hf->getNormalAt(x - off.x(), z - off.z());
}

// ---------------------------------------------------------------------------
// TerrainHeightField methods (unchanged)
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

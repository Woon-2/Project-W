#include "rspch.hpp"
#include "terrain.hpp"
#include "binaryImport.hpp"
#include "Model.hpp"          // loadModelFromFile (prop BVHs)
#include "physicsWorld.hpp"   // PhysicsWorld::registerScatter
#include <algorithm>
#include <cmath>

// Where server-side prop BV exports live: "<dir>/<name>Server.bin" (BV-only).
static const std::filesystem::path kPropModelDir = "../resources/models/props/";

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
        readVec2(ifs, "TileSize");
        readVec2(ifs, "TileOffset");
        readFloat(ifs, "Metallic");
        readFloat(ifs, "Roughness");
    }

    // ---- scatter prototypes (store name for prop BVH lookup; rest discarded) ----
    if (result.version >= 2) {
        const int PN = readInteger(ifs, "PrototypeCount");
        result.scatterPrototypeNames.resize(PN);
        for (int p = 0; p < PN; ++p) {
            readHeadTag(ifs, "ScatterPrototype");
            result.scatterPrototypeNames[p] = readText(ifs, "Name");
            readInteger(ifs, "Kind");        // discard (collidability = has *Server.bin BVH)
            readText(ifs, "TexturePath");    // discard (render-only)
            readVec2(ifs, "BillboardSize");  // discard
            readVec4(ifs, "Tint");           // discard
            readTailTag(ifs, "ScatterPrototype");
        }
    }

    // ---- chunk records ----
    const int C = readInteger(ifs, "ChunkCount");
    result.chunks.resize(C);
    for (int c = 0; c < C; ++c) {
        auto& e = result.chunks[c];
        readHeadTag(ifs, "Chunk");
        e.col = readInteger(ifs, "Col");
        e.row = readInteger(ifs, "Row");
        const auto chunkSize = readVec3(ifs, "Size");
        e.sizeX = chunkSize.x;
        e.sizeY = chunkSize.y;
        e.sizeZ = chunkSize.z;
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

        // ---- per-chunk scatter instances (stored for monster-vs-prop collision) ----
        if (result.version >= 2) {
            const int instCount = readInteger(ifs, "InstanceCount");
            e.scatter.resize(instCount);
            for (int i = 0; i < instCount; ++i) {
                auto& si = e.scatter[i];
                si.protoIdx = static_cast<uint16_t>(readInteger(ifs, "ProtoIdx"));
                const auto pl = readVec3(ifs, "PosLocal");
                si.posLocal = mu::Vec3(pl.x, pl.y, pl.z);
                if (result.version >= 3) {
                    const auto q = readVec4(ifs, "Rot");          // v3+: orientation quaternion
                    si.rot = mu::NQuat(q.x, q.y, q.z, q.w);
                } else {
                    const float yaw = readFloat(ifs, "Yaw");      // v2 (legacy): yaw float
                    si.rot = mu::NQuat(0.f, std::sin(yaw * 0.5f), 0.f, std::cos(yaw * 0.5f));
                }
                const auto sc = readVec2(ifs, "Scale");
                si.scaleW = sc.x;
                si.scaleH = sc.y;
            }
        }

        readTailTag(ifs, "Chunk");
    }

    // ---- stronghold records (gameplay) ----
    const int S = readInteger(ifs, "StrongholdCount");
    result.strongholds.resize(S);
    for (int s = 0; s < S; ++s) {
        auto& sh = result.strongholds[s];
        readHeadTag(ifs, "Stronghold");
        sh.id = readInteger(ifs, "Id");
        const auto c = readVec3(ifs, "Center");
        sh.center = mu::Vec3(c.x, c.y, c.z);
        const auto o = readVec4(ifs, "Orient");
        sh.orient = mu::NQuat(o.x, o.y, o.z, o.w);
        const auto sc = readVec3(ifs, "Scale");
        sh.scale = mu::Vec3(sc.x, sc.y, sc.z);
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

    // ---- zone records (trigger volumes) ----
    const int Z = readInteger(ifs, "ZoneCount");
    result.zones.resize(Z);
    for (int z = 0; z < Z; ++z) {
        auto& zone = result.zones[z];
        readHeadTag(ifs, "Zone");
        zone.id          = readInteger(ifs, "Id");
        zone.tag         = readText(ifs, "Tag");
        zone.factionMask = static_cast<std::uint32_t>(readInteger(ifs, "FactionMask"));
        const int V = readInteger(ifs, "VolumeCount");
        zone.volumes.resize(V);
        for (int v = 0; v < V; ++v) {
            auto& vol = zone.volumes[v];
            vol.shape = static_cast<ZoneShape>(readInteger(ifs, "Shape"));
            const auto c = readVec3(ifs, "Center");
            vol.center = mu::Vec3(c.x, c.y, c.z);
            const auto o = readVec4(ifs, "Orient");
            vol.orient = mu::NQuat(o.x, o.y, o.z, o.w);
            const auto h = readVec3(ifs, "Half");
            vol.halfExtents = mu::Vec3(h.x, h.y, h.z);
            vol.radius = readFloat(ifs, "Radius");
        }
        readTailTag(ifs, "Zone");
    }

    // ---- generic markers (type + name + transform) ----
    const int M = readInteger(ifs, "MarkerCount");
    result.markers.resize(M);
    for (int m = 0; m < M; ++m) {
        auto& mk = result.markers[m];
        readHeadTag(ifs, "Marker");
        mk.type = readText(ifs, "Type");
        mk.name = readText(ifs, "Name");
        const auto p = readVec3(ifs, "Pos");
        mk.pos = mu::Vec3(p.x, p.y, p.z);
        const auto o = readVec4(ifs, "Orient");
        mk.orient = mu::NQuat(o.x, o.y, o.z, o.w);
        const auto s = readVec3(ifs, "Scale");
        mk.scale = mu::Vec3(s.x, s.y, s.z);
        readTailTag(ifs, "Marker");
    }

    readTailTag(ifs, "ChunkIndex");

    gSharedLog << "[Terrain] Chunk index parsed: " << C << " chunks, "
               << S << " strongholds, " << Z << " zones, " << M << " markers\n";
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

    loadPropBVHs();

    gSharedLog << "[Terrain] Loaded " << heightFields_.size()
               << " chunk height fields (shared, read-only).\n";
    dumpLog();
}

void TerrainChunkManager::loadPropBVHs() {
    for (const std::string& name : index_.scatterPrototypeNames) {
        if (name.empty() || propBVHs_.count(name)) continue;
        const auto path = kPropModelDir / (name + "Server.bin");
        if (!std::filesystem::exists(path)) continue;   // billboards / no server BV -> non-collidable
        Model m = loadModelFromFile(path);
        if (!m.bvh.empty())
            propBVHs_.emplace(name, std::move(m.bvh));
    }
    gSharedLog << "[Terrain] Loaded " << propBVHs_.size() << " collidable prop BVHs.\n";
}

TerrainChunkManager::ScatterWorldXform
TerrainChunkManager::resolveInstanceXform(int col, int row,
                                          const TerrainHeightField* hf,
                                          const ScatterInstance& inst) const {
    const mu::Vec3 offset = worldOffset(col, row);
    // Ground-snap exactly as the client resolveInstanceXform so prop geometry
    // is identical on both peers (multi-chain position sync invariant).
    mu::Vec3 pos = offset + inst.posLocal;
    if (hf && !hf->empty())
        pos = mu::Vec3(pos.x(), offset.y() + hf->getHeightAt(inst.posLocal.x(), inst.posLocal.z()), pos.z());
    const mu::Vec3 scale(inst.scaleW, inst.scaleH, inst.scaleW);   // collidable props are meshes
    return ScatterWorldXform{ pos, inst.rot, scale };
}

void TerrainChunkManager::registerScatterColliders(PhysicsWorld& physicsWorld) const {
    if (propBVHs_.empty()) return;
    for (const auto& e : index_.chunks) {
        if (e.scatter.empty()) continue;
        const TerrainHeightField* hf = findHf(e.col, e.row);

        std::vector<ScatterCollider::InstanceInput> insts;
        for (const auto& si : e.scatter) {
            if (si.protoIdx >= index_.scatterPrototypeNames.size()) continue;
            auto it = propBVHs_.find(index_.scatterPrototypeNames[si.protoIdx]);
            if (it == propBVHs_.end() || it->second.empty()) continue;   // non-collidable (billboard)

            const ScatterWorldXform x = resolveInstanceXform(e.col, e.row, hf, si);
            insts.push_back(ScatterCollider::InstanceInput{ &it->second, x.pos, x.rot, x.scale });
        }
        if (insts.empty()) continue;

        auto collider = std::make_unique<ScatterCollider>(insts);
        if (!collider->empty())
            physicsWorld.registerScatter(std::move(collider));
    }
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

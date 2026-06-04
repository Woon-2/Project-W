#include "pch.hpp"
#include "terrain.hpp"
#include "binaryImport.hpp"
#include "errorHandling.hpp"

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

// Reads a BindlessIndex-invalid sentinel (idxRange = -1) into tex.idxSrv/idxUav.
static void markTextureInvalid(Texture& tex) {
    tex.idxSrv.idxRange    = -1;
    tex.idxSrv.idxResource = -1;
    tex.idxSrv.idxInArray  = -1;
    tex.idxSrv.idxSampler  = -1;
    tex.idxUav.idxRange    = -1;
}

// Loads a DDS texture, creates its SRV in the pool, and sets up the sampler index.
// Returns an invalid Texture (idxSrv.idxRange == -1) on failure.
static Texture loadAndRegisterTexture(
    const std::filesystem::path& path,
    const std::string& cacheKey,
    ID3D12Device* device,
    ID3D12GraphicsCommandList* cmdList,
    std::unordered_map<std::string, Texture>& texHashMap,
    DescriptorPool& texPool,
    Fence& fence
) {
    if (texHashMap.contains(cacheKey)) {
        return cloneTextureIdxOnly(texHashMap.at(cacheKey));
    }

    Texture::Type type{};
    auto tex = loadTexture(device, cmdList, path, fence, type);
    if (!tex.res) {
        markTextureInvalid(tex);
        return tex;
    }

    createSRV(device, tex, texPool);
    tex.idxSrv.idxSampler = etoi(Samplers::TrilinearWrap);

    texHashMap.try_emplace(cacheKey, tex);
    return cloneTextureIdxOnly(texHashMap.at(cacheKey));
}

// ---------------------------------------------------------------------------
// Mesh building
// ---------------------------------------------------------------------------

// Clamps x to [0, N-1] range for safe neighbor height access.
static int clampIdx(int x, int N) {
    return x < 0 ? 0 : (x >= N ? N - 1 : x);
}

// Resolves a stored engine path: if it does not exist as-is, fall back to
// terrainDir/filename. Shared by the index/palette/chunk loaders.
static std::filesystem::path resolveTerrainPath(
    const std::filesystem::path& terrainDir, const std::string& pathStr
) {
    auto p = std::filesystem::path(pathStr);
    if (!p.is_absolute() && !std::filesystem::exists(p)) {
        auto fallback = terrainDir / p.filename();
        if (std::filesystem::exists(fallback)) return fallback;
    }
    return p;
}

// Generates CPU-side vertex attributes, 32-bit indices, and normalized heights for
// an N x N height grid. PURE CPU (no D3D) so it can run on a worker thread.
// Shared by buildTerrainMesh and buildChunkCpu.
static void genChunkGeometryCpu(
    int N, float sizeX, float sizeY, float sizeZ,
    const std::vector<u16t>& rawHeights,
    std::vector<XMFLOAT3>& positions,
    std::vector<XMFLOAT3>& normals,
    std::vector<XMFLOAT3>& tangents,
    std::vector<XMFLOAT3>& bitangents,
    std::vector<XMFLOAT2>& uvs,
    std::vector<u32t>& indices,
    std::vector<float>& outNormalizedHeights
) {
    // Helper: normalized height at (x, y), clamped. (Unity writes y outer, x inner.)
    auto height = [&](int x, int y) -> float {
        return rawHeights[clampIdx(y, N) * N + clampIdx(x, N)] / 65535.f;
    };

    // CPU-side normalized heights for physics collision.
    outNormalizedHeights.resize(static_cast<size_t>(N) * N);
    for (int y = 0; y < N; ++y)
        for (int x = 0; x < N; ++x)
            outNormalizedHeights[y * N + x] = height(x, y);

    // --- Build vertices ---
    const float dx = (N > 1) ? sizeX / (N - 1) : 0.f;
    const float dz = (N > 1) ? sizeZ / (N - 1) : 0.f;

    positions.assign(static_cast<size_t>(N) * N, XMFLOAT3{});
    normals.assign(static_cast<size_t>(N) * N, XMFLOAT3{});
    tangents.assign(static_cast<size_t>(N) * N, XMFLOAT3{});
    bitangents.assign(static_cast<size_t>(N) * N, XMFLOAT3{});
    uvs.assign(static_cast<size_t>(N) * N, XMFLOAT2{});

    for (int y = 0; y < N; ++y) {
        for (int x = 0; x < N; ++x) {
            const int   idx = y * N + x;
            const float fx  = static_cast<float>(x) / (N - 1);
            const float fz  = static_cast<float>(y) / (N - 1);
            const float fy  = height(x, y);

            positions[idx] = XMFLOAT3(fx * sizeX, fy * sizeY, fz * sizeZ);
            uvs[idx]       = XMFLOAT2(fx, fz);

            // Central-difference normal in world space.
            const float gx = (dx > 0.f) ? (height(x - 1, y) - height(x + 1, y)) * sizeY / (2.f * dx) : 0.f;
            const float gz = (dz > 0.f) ? (height(x, y - 1) - height(x, y + 1)) * sizeY / (2.f * dz) : 0.f;

            auto n = DirectX::XMVector3Normalize(DirectX::XMVectorSet(gx, 1.f, gz, 0.f));
            DirectX::XMStoreFloat3(&normals[idx], n);

            // Tangent: world-space dP/dU direction (U increases with x), Gram-Schmidt vs normal.
            const float rawTy = (dx > 0.f) ? (height(x + 1, y) - height(x - 1, y)) * sizeY / (2.f * dx) : 0.f;
            auto rawT = DirectX::XMVector3Normalize(DirectX::XMVectorSet(1.f, rawTy, 0.f, 0.f));

            auto t = DirectX::XMVector3Normalize(
                DirectX::XMVectorSubtract(rawT,
                    DirectX::XMVectorScale(n, DirectX::XMVectorGetX(DirectX::XMVector3Dot(rawT, n))))
            );
            auto b = DirectX::XMVector3Cross(n, t);  // orthonormal bitangent

            DirectX::XMStoreFloat3(&tangents[idx],   t);
            DirectX::XMStoreFloat3(&bitangents[idx], b);
        }
    }

    // --- Build 32-bit indices ---
    const int cellsX = N - 1;
    const int cellsZ = N - 1;
    indices.assign(static_cast<size_t>(cellsX) * cellsZ * 6u, 0u);
    size_t idxWrite = 0;
    for (int y = 0; y < cellsZ; ++y) {
        for (int x = 0; x < cellsX; ++x) {
            const u32t i0 = static_cast<u32t>(y     * N + x);
            const u32t i1 = static_cast<u32t>(y     * N + (x + 1));
            const u32t i2 = static_cast<u32t>((y + 1) * N + x);
            const u32t i3 = static_cast<u32t>((y + 1) * N + (x + 1));
            // Triangle 0: i0, i2, i1  (CW)
            indices[idxWrite++] = i0;
            indices[idxWrite++] = i2;
            indices[idxWrite++] = i1;
            // Triangle 1: i1, i2, i3  (CW)
            indices[idxWrite++] = i1;
            indices[idxWrite++] = i2;
            indices[idxWrite++] = i3;
        }
    }
}

// Uploads CPU geometry arrays to GPU vertex/index buffers and assembles a Mesh.
// MAIN THREAD ONLY (records copies into cmdList, associates upload buffers w/ fence).
static Mesh assembleChunkMeshGpu(
    const std::vector<XMFLOAT3>& positions,
    const std::vector<XMFLOAT3>& normals,
    const std::vector<XMFLOAT3>& tangents,
    const std::vector<XMFLOAT3>& bitangents,
    const std::vector<XMFLOAT2>& uvs,
    const std::vector<u32t>& indices,
    ID3D12Device* device,
    ID3D12GraphicsCommandList* cmdList,
    Fence& fence
) {

    // --- Upload to GPU ---
    const size_t posByteSize  = positions.size()  * sizeof(XMFLOAT3);
    const size_t normByteSize = normals.size()    * sizeof(XMFLOAT3);
    const size_t tanByteSize  = tangents.size()   * sizeof(XMFLOAT3);
    const size_t bitByteSize  = bitangents.size() * sizeof(XMFLOAT3);
    const size_t uvByteSize   = uvs.size()        * sizeof(XMFLOAT2);
    const size_t idxByteSize  = indices.size()    * sizeof(u32t);

    auto vbPos  = createBufferResource(device, nullptr, posByteSize,  BufferCreationType::VertexBuffer);
    auto vbPosU = createBufferResource(device, positions.data(), posByteSize,  BufferCreationType::UploadBuffer);
    setD3DName(vbPos.Get(),  "TerrainMesh_VB_Position");
    setD3DName(vbPosU.Get(), "TerrainMesh_VB_Position_Upload");
    copyResource(cmdList, vbPosU.Get(), vbPos.Get(),
        D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);

    auto vbNrm  = createBufferResource(device, nullptr, normByteSize, BufferCreationType::VertexBuffer);
    auto vbNrmU = createBufferResource(device, normals.data(), normByteSize,   BufferCreationType::UploadBuffer);
    setD3DName(vbNrm.Get(),  "TerrainMesh_VB_Normal");
    setD3DName(vbNrmU.Get(), "TerrainMesh_VB_Normal_Upload");
    copyResource(cmdList, vbNrmU.Get(), vbNrm.Get(),
        D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);

    auto vbTan  = createBufferResource(device, nullptr, tanByteSize, BufferCreationType::VertexBuffer);
    auto vbTanU = createBufferResource(device, tangents.data(), tanByteSize, BufferCreationType::UploadBuffer);
    setD3DName(vbTan.Get(),  "TerrainMesh_VB_Tangent");
    setD3DName(vbTanU.Get(), "TerrainMesh_VB_Tangent_Upload");
    copyResource(cmdList, vbTanU.Get(), vbTan.Get(),
        D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);

    auto vbBit  = createBufferResource(device, nullptr, bitByteSize, BufferCreationType::VertexBuffer);
    auto vbBitU = createBufferResource(device, bitangents.data(), bitByteSize, BufferCreationType::UploadBuffer);
    setD3DName(vbBit.Get(),  "TerrainMesh_VB_Bitangent");
    setD3DName(vbBitU.Get(), "TerrainMesh_VB_Bitangent_Upload");
    copyResource(cmdList, vbBitU.Get(), vbBit.Get(),
        D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);

    auto vbUV   = createBufferResource(device, nullptr, uvByteSize,   BufferCreationType::VertexBuffer);
    auto vbUVU  = createBufferResource(device, uvs.data(), uvByteSize, BufferCreationType::UploadBuffer);
    setD3DName(vbUV.Get(),  "TerrainMesh_VB_UV");
    setD3DName(vbUVU.Get(), "TerrainMesh_VB_UV_Upload");
    copyResource(cmdList, vbUVU.Get(), vbUV.Get(),
        D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);

    auto ib  = createBufferResource(device, nullptr, idxByteSize,  BufferCreationType::IndexBuffer);
    auto ibU = createBufferResource(device, indices.data(), idxByteSize,  BufferCreationType::UploadBuffer);
    setD3DName(ib.Get(),  "TerrainMesh_IB");
    setD3DName(ibU.Get(), "TerrainMesh_IB_Upload");
    copyResource(cmdList, ibU.Get(), ib.Get(),
        D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_INDEX_BUFFER);

    // --- Assemble Mesh ---
    Mesh mesh;
    mesh.name = "TerrainMesh";

    mesh.vbViews.emplace_back(
        vbPos->GetGPUVirtualAddress(),
        static_cast<UINT>(posByteSize),
        static_cast<UINT>(sizeof(XMFLOAT3))
    );
    mesh.vbViews.emplace_back(
        vbNrm->GetGPUVirtualAddress(),
        static_cast<UINT>(normByteSize),
        static_cast<UINT>(sizeof(XMFLOAT3))
    );
    mesh.vbViews.emplace_back(
        vbTan->GetGPUVirtualAddress(),
        static_cast<UINT>(tanByteSize),
        static_cast<UINT>(sizeof(XMFLOAT3))
    );
    mesh.vbViews.emplace_back(
        vbBit->GetGPUVirtualAddress(),
        static_cast<UINT>(bitByteSize),
        static_cast<UINT>(sizeof(XMFLOAT3))
    );
    mesh.vbViews.emplace_back(
        vbUV->GetGPUVirtualAddress(),
        static_cast<UINT>(uvByteSize),
        static_cast<UINT>(sizeof(XMFLOAT2))
    );

    mesh.vbs.push_back(std::move(vbPos));
    mesh.vbIdxMap.try_emplace("TerrainMesh_VB_Position",  0u);
    mesh.vbs.push_back(std::move(vbNrm));
    mesh.vbIdxMap.try_emplace("TerrainMesh_VB_Normal",    1u);
    mesh.vbs.push_back(std::move(vbTan));
    mesh.vbIdxMap.try_emplace("TerrainMesh_VB_Tangent",   2u);
    mesh.vbs.push_back(std::move(vbBit));
    mesh.vbIdxMap.try_emplace("TerrainMesh_VB_Bitangent", 3u);
    mesh.vbs.push_back(std::move(vbUV));
    mesh.vbIdxMap.try_emplace("TerrainMesh_VB_UV",        4u);

    mesh.subMeshes.emplace_back(
        "TerrainMesh_SubMesh",
        D3D12_INDEX_BUFFER_VIEW{
            .BufferLocation = ib->GetGPUVirtualAddress(),
            .SizeInBytes    = static_cast<UINT>(idxByteSize),
            .Format         = DXGI_FORMAT_R32_UINT
        }
    );

    mesh.ibs.push_back(std::move(ib));

    // Keep upload buffers alive until the fence signals GPU completion.
    fence.associatedResources_.push_back(std::move(vbPosU));
    fence.associatedResources_.push_back(std::move(vbNrmU));
    fence.associatedResources_.push_back(std::move(vbTanU));
    fence.associatedResources_.push_back(std::move(vbBitU));
    fence.associatedResources_.push_back(std::move(vbUVU));
    fence.associatedResources_.push_back(std::move(ibU));

    gSharedLog << "[Terrain] TerrainMesh built: " << positions.size()
               << " vertices, " << indices.size() / 3 << " triangles\n";

    return mesh;
}

// ---------------------------------------------------------------------------
// Chunk streaming API
// ---------------------------------------------------------------------------

ChunkIndex parseChunkIndex(const std::filesystem::path& terrainDir) {
    ChunkIndex result;

    const auto indexPath = terrainDir / "chunks_index.bin";
    auto ifs = std::ifstream(indexPath, std::ios::binary);
    DISPLAY_ERROR_STR(ifs.good(),
        "[Terrain] parseChunkIndex: cannot open " + indexPath.string(), true);

    readHeadTag(ifs, "ChunkIndex");
    result.version = readInteger(ifs, "Version");

    // ---- shared palette ----
    const int L = readInteger(ifs, "LayerCount");
    result.palette.layerCount = L;
    result.palette.layers.resize(L);
    result.palette.diffusePaths.resize(L);
    result.palette.normalPaths.resize(L);
    for (int i = 0; i < L; ++i) {
        auto& ly = result.palette.layers[i];
        result.palette.diffusePaths[i] = readText(ifs, "DiffusePath");
        result.palette.normalPaths[i]  = readText(ifs, "NormalPath");
        ly.tileSizeX   = readFloat(ifs, "TileSizeX");
        ly.tileSizeY   = readFloat(ifs, "TileSizeY");
        ly.tileOffsetX = readFloat(ifs, "TileOffsetX");
        ly.tileOffsetY = readFloat(ifs, "TileOffsetY");
        ly.metallic    = readFloat(ifs, "Metallic");
        ly.roughness   = readFloat(ifs, "Roughness");
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

    // ---- stronghold records (server-only gameplay data) ----
    // The client does not use strongholds (they are synced from the server as
    // entities), but must consume the tags here to keep the binary stream aligned.
    const int S = readInteger(ifs, "StrongholdCount");
    for (int s = 0; s < S; ++s) {
        readHeadTag(ifs, "Stronghold");
        readInteger(ifs, "Id");
        readFloat(ifs, "CenterX"); readFloat(ifs, "CenterY"); readFloat(ifs, "CenterZ");
        readFloat(ifs, "OrientX"); readFloat(ifs, "OrientY"); readFloat(ifs, "OrientZ"); readFloat(ifs, "OrientW");
        readFloat(ifs, "ScaleX"); readFloat(ifs, "ScaleY"); readFloat(ifs, "ScaleZ");
        readFloat(ifs, "ActivityRadius"); readFloat(ifs, "SpawnRadius");
        readInteger(ifs, "MaxHp");
        readFloat(ifs, "RespawnDelaySec");
        const int P = readInteger(ifs, "PopulationCount");
        for (int p = 0; p < P; ++p) {
            readInteger(ifs, "MonsterType");
            readInteger(ifs, "TargetCount");
            readInteger(ifs, "MaxPerWave");
            readFloat(ifs, "RespawnIntervalSec");
        }
        readTailTag(ifs, "Stronghold");
    }

    // ---- zone records (trigger volumes) ----
    // Unlike strongholds, the client parses these for real: it runs local
    // cosmetic zones (e.g. BGM/camera) against the predicted player position.
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
            const float cx = readFloat(ifs, "CenterX");
            const float cy = readFloat(ifs, "CenterY");
            const float cz = readFloat(ifs, "CenterZ");
            vol.center = mu::Vec3(cx, cy, cz);
            const float ox = readFloat(ifs, "OrientX");
            const float oy = readFloat(ifs, "OrientY");
            const float oz = readFloat(ifs, "OrientZ");
            const float ow = readFloat(ifs, "OrientW");
            vol.orient = mu::NQuat(ox, oy, oz, ow);
            const float hx = readFloat(ifs, "HalfX");
            const float hy = readFloat(ifs, "HalfY");
            const float hz = readFloat(ifs, "HalfZ");
            vol.halfExtents = mu::Vec3(hx, hy, hz);
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
        const float px = readFloat(ifs, "PosX");
        const float py = readFloat(ifs, "PosY");
        const float pz = readFloat(ifs, "PosZ");
        mk.pos = mu::Vec3(px, py, pz);
        const float ox = readFloat(ifs, "OrientX");
        const float oy = readFloat(ifs, "OrientY");
        const float oz = readFloat(ifs, "OrientZ");
        const float ow = readFloat(ifs, "OrientW");
        mk.orient = mu::NQuat(ox, oy, oz, ow);
        const float sx = readFloat(ifs, "ScaleX");
        const float sy = readFloat(ifs, "ScaleY");
        const float sz = readFloat(ifs, "ScaleZ");
        mk.scale = mu::Vec3(sx, sy, sz);
        readTailTag(ifs, "Marker");
    }

    readTailTag(ifs, "ChunkIndex");

    gSharedLog << "[Terrain] Chunk index parsed: " << C << " chunks, "
               << L << " shared layers, " << S << " strongholds, " << Z << " zones, "
               << M << " markers\n";
    return result;
}

TerrainLayerPalette loadLayerPalette(
    const ChunkIndex& index,
    const std::filesystem::path& terrainDir,
    ID3D12Device* device,
    ID3D12GraphicsCommandList* cmdList,
    std::unordered_map<std::string, Texture>& texHashMap,
    DescriptorPool& texPool,
    Fence& fenceToAssociate
) {
    TerrainLayerPalette pal = index.palette;   // copies scalars + load-time paths
    for (int i = 0; i < pal.layerCount; ++i) {
        auto& ly = pal.layers[i];

        if (i < static_cast<int>(pal.diffusePaths.size()) && !pal.diffusePaths[i].empty()) {
            ly.diffuse = loadAndRegisterTexture(
                resolveTerrainPath(terrainDir, pal.diffusePaths[i]),
                "Terrain_Palette_Layer" + std::to_string(i) + "_Diffuse",
                device, cmdList, texHashMap, texPool, fenceToAssociate);
        } else {
            markTextureInvalid(ly.diffuse);
        }

        if (i < static_cast<int>(pal.normalPaths.size()) && !pal.normalPaths[i].empty()) {
            ly.normalMap = loadAndRegisterTexture(
                resolveTerrainPath(terrainDir, pal.normalPaths[i]),
                "Terrain_Palette_Layer" + std::to_string(i) + "_Normal",
                device, cmdList, texHashMap, texPool, fenceToAssociate);
        } else {
            markTextureInvalid(ly.normalMap);
        }
    }
    pal.loaded = true;
    gSharedLog << "[Terrain] Layer palette loaded: " << pal.layerCount << " layers\n";
    return pal;
}

void buildChunkCpu(
    const ChunkIndexEntry& entry,
    const std::filesystem::path& terrainDir,
    ChunkCpuBuild& out
) {
    out.col = entry.col;
    out.row = entry.row;
    out.sizeX = entry.sizeX;
    out.sizeY = entry.sizeY;
    out.sizeZ = entry.sizeZ;
    out.resolution = entry.resolution;
    out.alphamapResolution = entry.alphamapResolution;
    out.splatPath = entry.splatPath;

    const int N = entry.resolution;

    // No DISPLAY_ERROR_STR here: this runs on a worker thread. Signal failure via flag.
    auto heightPath = resolveTerrainPath(terrainDir, entry.heightPath);
    auto hifs = std::ifstream(heightPath, std::ios::binary);
    if (!hifs.good() || N <= 0) {
        out.failed = true;
        out.done.store(true, std::memory_order_release);
        return;
    }

    auto rawHeights = std::vector<u16t>(static_cast<size_t>(N) * N);
    hifs.read(reinterpret_cast<char*>(rawHeights.data()),
              static_cast<std::streamsize>(N) * N * sizeof(u16t));

    genChunkGeometryCpu(N, entry.sizeX, entry.sizeY, entry.sizeZ, rawHeights,
        out.positions, out.normals, out.tangents, out.bitangents, out.uvs,
        out.indices, out.normalizedHeights);

    out.done.store(true, std::memory_order_release);
}

TerrainData finalizeChunkGpu(
    const ChunkCpuBuild& cpu,
    const TerrainLayerPalette& palette,
    const std::filesystem::path& terrainDir,
    ID3D12Device* device,
    ID3D12GraphicsCommandList* cmdList,
    std::unordered_map<std::string, Texture>& texHashMap,
    DescriptorPool& texPool,
    Fence& fenceToAssociate
) {
    TerrainData td;
    td.heightmapResolution = cpu.resolution;
    td.alphamapResolution  = cpu.alphamapResolution;
    td.sizeX = cpu.sizeX;
    td.sizeY = cpu.sizeY;
    td.sizeZ = cpu.sizeZ;
    td.layerCount = palette.layerCount;
    td.chunkCol = cpu.col;
    td.chunkRow = cpu.row;

    // Shared palette layer handles (cheap BindlessIndex copies; one GPU texture set).
    td.layers = palette.layers;

    // GPU mesh from CPU arrays.
    td.mesh = assembleChunkMeshGpu(cpu.positions, cpu.normals, cpu.tangents,
        cpu.bitangents, cpu.uvs, cpu.indices, device, cmdList, fenceToAssociate);

    // Per-chunk splat map.
    const std::string splatKey =
        "Terrain_Splat_" + std::to_string(cpu.col) + "_" + std::to_string(cpu.row);
    td.splatMap = loadAndRegisterTexture(
        resolveTerrainPath(terrainDir, cpu.splatPath), splatKey,
        device, cmdList, texHashMap, texPool, fenceToAssociate);

    // CPU height field for physics.
    td.heightField.resolution = cpu.resolution;
    td.heightField.sizeX = cpu.sizeX;
    td.heightField.sizeY = cpu.sizeY;
    td.heightField.sizeZ = cpu.sizeZ;
    td.heightField.heights = cpu.normalizedHeights;

    return td;
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

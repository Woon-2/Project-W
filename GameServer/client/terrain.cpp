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
// Manifest parsing
// ---------------------------------------------------------------------------

struct TerrainManifest {
    std::string heightMapPath;
    std::string metaPath;
    std::vector<std::string> splatPaths;
    std::vector<std::string> diffusePaths;
    std::vector<std::string> normalPaths;
};

static TerrainManifest parseManifest(const std::filesystem::path& manifestPath) {
    TerrainManifest result;

    auto ifs = std::ifstream(manifestPath, std::ios::binary);
    DISPLAY_ERROR_STR(ifs.good(),
        "[Terrain] parseManifest: cannot open " + manifestPath.string(), true);

    readHeadTag(ifs, "Terrain");

    // TerrainName (skip)
    readHeadTag(ifs, "TerrainName");
    readString(ifs);
    readTailTag(ifs, "TerrainName");

    // The exporter writes mappings in insertion order:
    //   HeightMap -> SplatPath(s) -> DiffusePath / NormalPath (alternating per layer) -> MetaData
    // So we must not assume a fixed order after TerrainName; read tag-by-tag instead.
    while (ifs) {
        auto rawTag = readString(ifs);
        if (isTailTag(rawTag, "Terrain")) {
            break;
        }
        auto tagName = untagHead(rawTag);
        auto value   = readString(ifs);
        readString(ifs); // consume tail tag

        if (tagName == "HeightMap") {
            result.heightMapPath = value;
        } else if (tagName == "MetaData") {
            result.metaPath = value;
        } else if (tagName == "SplatPath") {
            result.splatPaths.push_back(value);
        } else if (tagName == "DiffusePath") {
            result.diffusePaths.push_back(value);
        } else if (tagName == "NormalPath") {
            result.normalPaths.push_back(value);
        }
    }

    return result;
}

// ---------------------------------------------------------------------------
// Metadata parsing
// ---------------------------------------------------------------------------

struct TerrainMeta {
    int   heightmapResolution;
    int   alphamapResolution;
    float sizeX, sizeY, sizeZ;
    int   layerCount;
    struct LayerInfo {
        float tileSizeX, tileSizeY;
        float tileOffsetX, tileOffsetY;
    };
    std::vector<LayerInfo> layers;
};

static TerrainMeta parseMeta(const std::filesystem::path& metaPath) {
    TerrainMeta m{};
    auto ifs = std::ifstream(metaPath, std::ios::binary);
    DISPLAY_ERROR_STR(ifs.good(),
        "[Terrain] parseMeta: cannot open " + metaPath.string(), true);

    // Raw binary layout: no tags
    m.heightmapResolution = readInteger(ifs);
    m.alphamapResolution  = readInteger(ifs);
    m.sizeX = readFloat(ifs);
    m.sizeY = readFloat(ifs);
    m.sizeZ = readFloat(ifs);
    m.layerCount = readInteger(ifs);
    m.layers.resize(m.layerCount);
    for (auto& layer : m.layers) {
        layer.tileSizeX   = readFloat(ifs);
        layer.tileSizeY   = readFloat(ifs);
        layer.tileOffsetX = readFloat(ifs);
        layer.tileOffsetY = readFloat(ifs);
    }
    return m;
}

// ---------------------------------------------------------------------------
// Mesh building
// ---------------------------------------------------------------------------

// Clamps x to [0, N-1] range for safe neighbor height access.
static int clampIdx(int x, int N) {
    return x < 0 ? 0 : (x >= N ? N - 1 : x);
}

// Reads height.raw and builds a fully indexed grid mesh on the GPU.
// Uses 32-bit indices to support large heightmaps (N > 256).
static Mesh buildTerrainMesh(
    const std::filesystem::path& heightRawPath,
    const TerrainMeta& meta,
    ID3D12Device* device,
    ID3D12GraphicsCommandList* cmdList,
    Fence& fence
) {
    const int N = meta.heightmapResolution;

    // --- Load height.raw ---
    auto hifs = std::ifstream(heightRawPath, std::ios::binary);
    DISPLAY_ERROR_STR(hifs.good(),
        "[Terrain] buildTerrainMesh: cannot open " + heightRawPath.string(), true);

    // Unity writes heights in y (row) outer, x (col) inner order.
    auto rawHeights = std::vector<u16t>(static_cast<size_t>(N) * N);
    hifs.read(reinterpret_cast<char*>(rawHeights.data()),
              static_cast<std::streamsize>(N) * N * sizeof(u16t));

    // Helper: normalized height at (x, y), clamped.
    auto height = [&](int x, int y) -> float {
        return rawHeights[clampIdx(y, N) * N + clampIdx(x, N)] / 65535.f;
    };

    // --- Build vertices ---
    const float dx = (N > 1) ? meta.sizeX / (N - 1) : 0.f;
    const float dz = (N > 1) ? meta.sizeZ / (N - 1) : 0.f;

    auto positions  = std::vector<XMFLOAT3>(static_cast<size_t>(N) * N);
    auto normals    = std::vector<XMFLOAT3>(static_cast<size_t>(N) * N);
    auto tangents   = std::vector<XMFLOAT3>(static_cast<size_t>(N) * N);
    auto bitangents = std::vector<XMFLOAT3>(static_cast<size_t>(N) * N);
    auto uvs        = std::vector<XMFLOAT2>(static_cast<size_t>(N) * N);

    for (int y = 0; y < N; ++y) {
        for (int x = 0; x < N; ++x) {
            const int   idx = y * N + x;
            const float fx  = static_cast<float>(x) / (N - 1);
            const float fz  = static_cast<float>(y) / (N - 1);
            const float fy  = height(x, y);

            positions[idx] = XMFLOAT3(fx * meta.sizeX, fy * meta.sizeY, fz * meta.sizeZ);
            uvs[idx]       = XMFLOAT2(fx, fz);

            // Central-difference normal in world space.
            // Gradient in x: (h(x+1,y) - h(x-1,y)) * sizeY / (2 * dx)
            // Gradient in z: (h(x,y+1) - h(x,y-1)) * sizeY / (2 * dz)
            const float gx = (dx > 0.f) ? (height(x - 1, y) - height(x + 1, y)) * meta.sizeY / (2.f * dx) : 0.f;
            const float gz = (dz > 0.f) ? (height(x, y - 1) - height(x, y + 1)) * meta.sizeY / (2.f * dz) : 0.f;

            auto n = DirectX::XMVector3Normalize(DirectX::XMVectorSet(gx, 1.f, gz, 0.f));
            DirectX::XMStoreFloat3(&normals[idx], n);

            // Tangent: world-space dP/dU direction (U increases with x).
            // dP/dU = (sizeX, (h(x+1,y)-h(x-1,y))*sizeY/(2*dx), 0)
            //       = (1, -gx_unsigned, 0) after direction reduction
            const float rawTy = (dx > 0.f) ? (height(x + 1, y) - height(x - 1, y)) * meta.sizeY / (2.f * dx) : 0.f;
            auto rawT = DirectX::XMVector3Normalize(DirectX::XMVectorSet(1.f, rawTy, 0.f, 0.f));

            // Bitangent: world-space dP/dV direction (V increases with y/z).
            const float rawBy = (dz > 0.f) ? (height(x, y + 1) - height(x, y - 1)) * meta.sizeY / (2.f * dz) : 0.f;
            auto rawB = DirectX::XMVector3Normalize(DirectX::XMVectorSet(0.f, rawBy, 1.f, 0.f));

            // Gram-Schmidt: orthogonalize tangent against normal, then derive bitangent.
            auto t = DirectX::XMVector3Normalize(
                DirectX::XMVectorSubtract(rawT,
                    DirectX::XMVectorScale(n, DirectX::XMVectorGetX(DirectX::XMVector3Dot(rawT, n))))
            );
            auto b = DirectX::XMVector3Cross(n, t);

            DirectX::XMStoreFloat3(&tangents[idx],   t);
            DirectX::XMStoreFloat3(&bitangents[idx], b);
        }
    }

    // --- Build 32-bit indices ---
    const int cellsX = N - 1;
    const int cellsZ = N - 1;
    auto indices = std::vector<u32t>(static_cast<size_t>(cellsX) * cellsZ * 6u);
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

    gSharedLog << "[Terrain] TerrainMesh built: " << N << "x" << N
               << " vertices, " << indices.size() / 3 << " triangles\n";

    return mesh;
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

TerrainData loadTerrainFromFiles(
    const std::filesystem::path& terrainDir,
    ID3D12Device* device,
    ID3D12GraphicsCommandList* cmdList,
    std::unordered_map<std::string, Texture>& texHashMap,
    DescriptorPool& texPool,
    Fence& fenceToAssociate
) {
    // 1. Parse manifest for file paths
    const auto manifestPath = terrainDir / "terrain_manifest.bin";
    auto manifest = parseManifest(manifestPath);

    // Resolve paths: if the path is not absolute, treat it as relative to working dir.
    auto resolvePath = [&](const std::string& pathStr) -> std::filesystem::path {
        auto p = std::filesystem::path(pathStr);
        if (!p.is_absolute() && !std::filesystem::exists(p)) {
            // Fallback: look in terrainDir using the filename only
            auto fallback = terrainDir / p.filename();
            if (std::filesystem::exists(fallback)) {
                return fallback;
            }
        }
        return p;
    };

    // 2. Parse terrain metadata
    auto meta = parseMeta(resolvePath(manifest.metaPath));

    // 3. Build terrain mesh from heightmap
    TerrainData terrain;
    terrain.heightmapResolution = meta.heightmapResolution;
    terrain.alphamapResolution  = meta.alphamapResolution;
    terrain.sizeX = meta.sizeX;
    terrain.sizeY = meta.sizeY;
    terrain.sizeZ = meta.sizeZ;
    terrain.layerCount = meta.layerCount;

    terrain.mesh = buildTerrainMesh(
        resolvePath(manifest.heightMapPath),
        meta, device, cmdList, fenceToAssociate
    );

    // 4. Load splat map (take first splat; each RGBA covers 4 layers)
    if (!manifest.splatPaths.empty()) {
        terrain.splatMap = loadAndRegisterTexture(
            resolvePath(manifest.splatPaths[0]),
            "Terrain_SplatMap_0",
            device, cmdList, texHashMap, texPool, fenceToAssociate
        );
        gSharedLog << "[Terrain] Splat map loaded: " << manifest.splatPaths[0] << "\n";
    } else {
        markTextureInvalid(terrain.splatMap);
    }

    // 5. Load per-layer textures
    terrain.layers.resize(meta.layerCount);
    for (int i = 0; i < meta.layerCount; ++i) {
        auto& layer         = terrain.layers[i];
        layer.tileSizeX     = meta.layers[i].tileSizeX;
        layer.tileSizeY     = meta.layers[i].tileSizeY;
        layer.tileOffsetX   = meta.layers[i].tileOffsetX;
        layer.tileOffsetY   = meta.layers[i].tileOffsetY;

        if (i < static_cast<int>(manifest.diffusePaths.size())) {
            const auto& diffusePath = manifest.diffusePaths[i];
            layer.diffuse = loadAndRegisterTexture(
                resolvePath(diffusePath),
                "Terrain_Layer" + std::to_string(i) + "_Diffuse",
                device, cmdList, texHashMap, texPool, fenceToAssociate
            );
            gSharedLog << "[Terrain] Layer " << i << " diffuse loaded: " << diffusePath << "\n";
        } else {
            markTextureInvalid(layer.diffuse);
        }

        if (i < static_cast<int>(manifest.normalPaths.size())) {
            const auto& normalPath = manifest.normalPaths[i];
            layer.normalMap = loadAndRegisterTexture(
                resolvePath(normalPath),
                "Terrain_Layer" + std::to_string(i) + "_Normal",
                device, cmdList, texHashMap, texPool, fenceToAssociate
            );
            gSharedLog << "[Terrain] Layer " << i << " normal map loaded: " << normalPath << "\n";
        } else {
            markTextureInvalid(layer.normalMap);
        }
    }

    gSharedLog << "[Terrain] Load complete: " << meta.layerCount << " layers, "
               << "world size (" << meta.sizeX << ", " << meta.sizeY << ", " << meta.sizeZ << ")\n";

    return terrain;
}

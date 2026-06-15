#include "pch.hpp"
#include "terrainChunkManager.hpp"
#include "gfx.hpp"
#include "pbrPipeline.hpp"
#include "BVPipeline.hpp"   // [TEMP DEBUG] prop BV visualization
#include "physicsWorld.hpp"
#include "threadPool.hpp"
#include "errorHandling.hpp"   // setD3DName
#include "../common/scatterTransform.hpp"

#include <cmath>
#include <chrono>

namespace {

// Where tree / detail-mesh prop models extracted with ModelExtractor live. The chunk
// index's prototype Name maps to "{dir}/{Name}.bin" (Name == ModelExtractor targetName).
const std::filesystem::path kPropModelDir = "../resources/models/props/";

// Transforms an AABB by a matrix (8-corner refit). Used to derive an instance's
// world AABB for culling and the future scatter collision broadphase.
AABB MU_CALLCONV transformAABB(const AABB& local, mu::Mat4x4 m) {
    const mu::Vec3 c = local.center;
    const mu::Vec3 h = local.size * 0.5f;
    mu::Vec3 lo( 1e30f,  1e30f,  1e30f);
    mu::Vec3 hi(-1e30f, -1e30f, -1e30f);
    for (int i = 0; i < 8; ++i) {
        const mu::Vec3 corner(
            c.x() + ((i & 1) ? h.x() : -h.x()),
            c.y() + ((i & 2) ? h.y() : -h.y()),
            c.z() + ((i & 4) ? h.z() : -h.z()));
        mu::Vec4 v(corner, 1.f);
        v *= m;
        const mu::Vec3 p(v.x(), v.y(), v.z());
        lo = mu::Vec3(std::min(lo.x(), p.x()), std::min(lo.y(), p.y()), std::min(lo.z(), p.z()));
        hi = mu::Vec3(std::max(hi.x(), p.x()), std::max(hi.y(), p.y()), std::max(hi.z(), p.z()));
    }
    return AABB{ .center = (lo + hi) * 0.5f, .size = hi - lo };
}

// Builds a shared unit cross-quad mesh (two intersecting vertical quads) for billboard
// grass. Two-sided is achieved by emitting back-facing duplicate triangles, so the
// regular back-face-culling PSO renders both sides without a dedicated pipeline.
// Layout matches the PBR pipelines (Position/Normal/Tangent/Bitangent/UV).
Mesh buildCrossQuadMesh(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList, Fence& fence) {
    std::vector<XMFLOAT3> positions, normals, tangents, bitangents;
    std::vector<XMFLOAT2> uvs;
    std::vector<u32t>     indices;

    auto addQuad = [&](XMFLOAT3 p0, XMFLOAT3 p1, XMFLOAT3 p2, XMFLOAT3 p3, XMFLOAT3 n) {
        const u32t base = static_cast<u32t>(positions.size());
        positions.insert(positions.end(), { p0, p1, p2, p3 });
        for (int i = 0; i < 4; ++i) {
            normals.push_back(n);
            tangents.push_back(XMFLOAT3{ 1.f, 0.f, 0.f });
            bitangents.push_back(XMFLOAT3{ 0.f, 1.f, 0.f });
        }
        uvs.insert(uvs.end(), { {0.f,1.f}, {1.f,1.f}, {1.f,0.f}, {0.f,0.f} });
        indices.insert(indices.end(), { base+0, base+1, base+2, base+0, base+2, base+3 });
    };

    // Unit quad spans x/z in [-0.5,0.5], y in [0,1] (base at the ground plane).
    // Plane A (faces +Z) and its back (+ -Z), plane B (faces +X) and its back.
    addQuad({-.5f,0.f,0.f}, {.5f,0.f,0.f}, {.5f,1.f,0.f}, {-.5f,1.f,0.f}, { 0.f,0.f, 1.f});
    addQuad({.5f,0.f,0.f}, {-.5f,0.f,0.f}, {-.5f,1.f,0.f}, {.5f,1.f,0.f},  { 0.f,0.f,-1.f});
    addQuad({0.f,0.f,.5f}, {0.f,0.f,-.5f}, {0.f,1.f,-.5f}, {0.f,1.f,.5f},  { 1.f,0.f, 0.f});
    addQuad({0.f,0.f,-.5f}, {0.f,0.f,.5f}, {0.f,1.f,.5f}, {0.f,1.f,-.5f},  {-1.f,0.f, 0.f});

    const size_t posBytes = positions.size()  * sizeof(XMFLOAT3);
    const size_t nrmBytes = normals.size()     * sizeof(XMFLOAT3);
    const size_t tanBytes = tangents.size()    * sizeof(XMFLOAT3);
    const size_t bitBytes = bitangents.size()  * sizeof(XMFLOAT3);
    const size_t uvBytes  = uvs.size()         * sizeof(XMFLOAT2);
    const size_t idxBytes = indices.size()     * sizeof(u32t);

    auto upload = [&](const void* data, size_t bytes, BufferCreationType type, const char* dbgName,
                      D3D12_RESOURCE_STATES finalState) {
        auto gpu = createBufferResource(device, nullptr, bytes, type);
        auto up  = createBufferResource(device, data, bytes, BufferCreationType::UploadBuffer);
        setD3DName(gpu.Get(), dbgName);
        copyResource(cmdList, up.Get(), gpu.Get(), D3D12_RESOURCE_STATE_GENERIC_READ, finalState);
        fence.associatedResources_.push_back(std::move(up));
        return gpu;
    };

    auto vbPos = upload(positions.data(),  posBytes, BufferCreationType::VertexBuffer, "ScatterBillboard_VB_Position",  D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
    auto vbNrm = upload(normals.data(),    nrmBytes, BufferCreationType::VertexBuffer, "ScatterBillboard_VB_Normal",    D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
    auto vbTan = upload(tangents.data(),   tanBytes, BufferCreationType::VertexBuffer, "ScatterBillboard_VB_Tangent",   D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
    auto vbBit = upload(bitangents.data(), bitBytes, BufferCreationType::VertexBuffer, "ScatterBillboard_VB_Bitangent", D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
    auto vbUV  = upload(uvs.data(),        uvBytes,  BufferCreationType::VertexBuffer, "ScatterBillboard_VB_UV",        D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
    auto ib    = upload(indices.data(),    idxBytes, BufferCreationType::IndexBuffer,  "ScatterBillboard_IB",           D3D12_RESOURCE_STATE_INDEX_BUFFER);

    Mesh mesh;
    mesh.name = "ScatterBillboard";
    mesh.bounds = AABB{ .center = mu::Vec3(0.f, 0.5f, 0.f), .size = mu::Vec3(1.f, 1.f, 1.f) };

    mesh.vbViews.emplace_back(vbPos->GetGPUVirtualAddress(), static_cast<UINT>(posBytes), static_cast<UINT>(sizeof(XMFLOAT3)));
    mesh.vbViews.emplace_back(vbNrm->GetGPUVirtualAddress(), static_cast<UINT>(nrmBytes), static_cast<UINT>(sizeof(XMFLOAT3)));
    mesh.vbViews.emplace_back(vbTan->GetGPUVirtualAddress(), static_cast<UINT>(tanBytes), static_cast<UINT>(sizeof(XMFLOAT3)));
    mesh.vbViews.emplace_back(vbBit->GetGPUVirtualAddress(), static_cast<UINT>(bitBytes), static_cast<UINT>(sizeof(XMFLOAT3)));
    mesh.vbViews.emplace_back(vbUV->GetGPUVirtualAddress(),  static_cast<UINT>(uvBytes),  static_cast<UINT>(sizeof(XMFLOAT2)));

    mesh.vbs.push_back(std::move(vbPos)); mesh.vbIdxMap.try_emplace("ScatterBillboard_VB_Position",  0u);
    mesh.vbs.push_back(std::move(vbNrm)); mesh.vbIdxMap.try_emplace("ScatterBillboard_VB_Normal",    1u);
    mesh.vbs.push_back(std::move(vbTan)); mesh.vbIdxMap.try_emplace("ScatterBillboard_VB_Tangent",   2u);
    mesh.vbs.push_back(std::move(vbBit)); mesh.vbIdxMap.try_emplace("ScatterBillboard_VB_Bitangent", 3u);
    mesh.vbs.push_back(std::move(vbUV));  mesh.vbIdxMap.try_emplace("ScatterBillboard_VB_UV",        4u);

    mesh.subMeshes.emplace_back("ScatterBillboard_SubMesh",
        D3D12_INDEX_BUFFER_VIEW{
            .BufferLocation = ib->GetGPUVirtualAddress(),
            .SizeInBytes    = static_cast<UINT>(idxBytes),
            .Format         = DXGI_FORMAT_R32_UINT
        });
    mesh.ibs.push_back(std::move(ib));
    return mesh;
}

}  // namespace

void TerrainChunkManager::init(GFX& gfx, PhysicsWorld& physicsWorld, ThreadPool* threadPool,
                               const std::filesystem::path& terrainDir) {
    gfx_          = &gfx;
    physicsWorld_ = &physicsWorld;
    threadPool_   = threadPool;
    terrainDir_   = terrainDir;

    index_ = parseChunkIndex(terrainDir_);
    if (index_.chunks.empty()) {
        gSharedLog << "[ChunkManager] No chunks found in index.\n";
        return;
    }

    chunkSizeX_ = index_.chunks.front().sizeX;
    chunkSizeZ_ = index_.chunks.front().sizeZ;

    indexByCoord_.clear();
    for (const auto& e : index_.chunks)
        indexByCoord_[packCoord(e.col, e.row)] = &e;

    // Load the shared layer palette once on the main thread (touches DescriptorPool +
    // texHashMap). Chunks themselves stream in on demand via update().
    gfx_->recordTerrainResourceLoad(
        [&](ID3D12Device* device, ID3D12GraphicsCommandList* cmdList,
            DescriptorPool& texPool, Fence& fence) {
            palette_ = loadLayerPalette(index_, terrainDir_, device, cmdList,
                                        texHashMap_, texPool, fence);
        },
        /*wait=*/true);

    // Load scatter (tree/detail/billboard) assets once and resolve the prototype table.
    loadScatterAssets();

    gSharedLog << "[ChunkManager] Initialized: " << index_.chunks.size()
               << " chunks indexed, palette loaded. Streaming on demand.\n";
}

void TerrainChunkManager::loadScatterAssets() {
    const auto& protos = index_.scatterPrototypes;
    if (protos.empty()) return;

    resolvedProtos_.assign(protos.size(), ResolvedScatterProto{});

    // Count billboards up front so billboardMaterials_ never reallocates (we take
    // pointers into it). propModels_ is node-based, so its element pointers are stable.
    int billboardCount = 0;
    for (const auto& p : protos)
        if (p.kind == ScatterPrototype::Kind::BillboardGrass) ++billboardCount;
    billboardMaterials_.assign(billboardCount, Material{});

    // --- pass 1: queue model + texture loads ---
    bool anyBillboard = false;
    for (const auto& p : protos) {
        if (p.kind == ScatterPrototype::Kind::BillboardGrass) {
            anyBillboard = true;
            if (!p.texturePath.empty() && !texHashMap_.contains(p.name)) {
                // Resolve the stored engine path, falling back to terrainDir/filename.
                std::filesystem::path texPath = p.texturePath;
                if (!texPath.is_absolute() && !std::filesystem::exists(texPath)) {
                    auto fb = terrainDir_ / texPath.filename();
                    if (std::filesystem::exists(fb)) texPath = fb;
                }
                gfx_->addRequestTextureLoad(RequestTextureLoad{
                    .name            = p.name,
                    .texturePath     = texPath,
                    .pDest           = &texHashMap_[p.name],
                    .pTexHashMap     = &texHashMap_,
                    .needsUploadInfo = false
                });
            }
        } else {
            if (!propModels_.contains(p.name)) {
                const auto modelPath = kPropModelDir / (p.name + ".bin");
                if (!std::filesystem::exists(modelPath)) {
                    gSharedLog << "[ChunkManager] Scatter prop model not found: "
                               << modelPath.string() << " (prototype '" << p.name
                               << "'). Extract it with ModelExtractor (targetName == this Name).\n";
                    continue;
                }
                Model& dest = propModels_[p.name];   // node-stable address
                gfx_->addRequestModelLoad(RequestModelLoad{
                    .modelPath   = modelPath,
                    .pTexHashMap = &texHashMap_,
                    .pDest       = &dest
                });
            }
        }
    }
    gfx_->loadRequestedAssets();

    // Build the shared billboard cross-quad mesh on the main thread (needs device/cmdList).
    if (anyBillboard) {
        gfx_->recordTerrainResourceLoad(
            [&](ID3D12Device* device, ID3D12GraphicsCommandList* cmdList,
                DescriptorPool&, Fence& fence) {
                billboardMesh_ = buildCrossQuadMesh(device, cmdList, fence);
            },
            /*wait=*/true);
    }

    // --- pass 2: resolve render parts ---
    int bbIdx = 0;
    for (std::size_t i = 0; i < protos.size(); ++i) {
        const auto& p = protos[i];
        auto& rp = resolvedProtos_[i];

        if (p.kind == ScatterPrototype::Kind::BillboardGrass) {
            Material& mat = billboardMaterials_[bbIdx++];
            // Billboards have only an albedo (grass cutout); the rest are disabled.
            mat.mapAlbedo.idxSrv.idxRange = -1; mat.mapAlbedo.idxUav.idxRange = -1;
            mat.mapMetallicSmoothness.idxSrv.idxRange = -1; mat.mapMetallicSmoothness.idxUav.idxRange = -1;
            mat.mapNormal.idxSrv.idxRange = -1; mat.mapNormal.idxUav.idxRange = -1;
            mat.mapEmmisive.idxSrv.idxRange = -1; mat.mapEmmisive.idxUav.idxRange = -1;
            mat.mapAmbientOcclusion.idxSrv.idxRange = -1; mat.mapAmbientOcclusion.idxUav.idxRange = -1;
            auto tit = texHashMap_.find(p.name);
            if (tit != texHashMap_.end()) mat.mapAlbedo = tit->second;   // valid SRV from load
            mat.constantAlbedo      = XMFLOAT4{ 1.f, 1.f, 1.f, 1.f };
            mat.constantRoughness   = 1.f;
            mat.constantMetallic    = 0.f;
            mat.constantAOStrength  = 0.f;
            mat.constantEmmisive    = XMFLOAT3{ 0.f, 0.f, 0.f };
            mat.constantAlphaCutoff = kFoliageAlphaCutoff;

            if (!billboardMesh_.subMeshes.empty()) {
                rp.parts.push_back(ScatterDrawPart{
                    .mesh = &billboardMesh_, .subMesh = &billboardMesh_.subMeshes[0],
                    .material = &mat, .meshXform = mu::scaleH(mu::Vec3(1.f, 1.f, 1.f)) });
            }
            // Billboard local bounds: the unit cross-quad scaled later by the instance.
            rp.localBounds = billboardMesh_.bounds;
            rp.collidable  = false;
            continue;
        }

        // Tree / mesh-detail: point at the loaded prop model's parts.
        auto mit = propModels_.find(p.name);
        if (mit == propModels_.end() || mit->second.meshWithDressXforms.empty()) {
            gSharedLog << "[ChunkManager] Scatter prototype '" << p.name
                       << "' has no loaded model (expected " << (kPropModelDir / (p.name + ".bin")).string()
                       << "). Skipping.\n";
            continue;
        }
        Model& model = mit->second;
        rp.model      = &model;
        rp.collidable = !model.bvh.empty();

        AABB lb{ .center = mu::Vec3(0.f, 0.f, 0.f), .size = mu::Vec3(0.f, 0.f, 0.f) };
        bool haveBounds = false;
        for (auto& [mesh, meshXform] : model.meshWithDressXforms) {
            if (mesh.materialSets.empty()) continue;
            auto& materials = mesh.materialSets[0].materials;
            for (std::size_t s = 0; s < mesh.subMeshes.size(); ++s) {
                if (s >= materials.size()) break;
                // Enable foliage alpha test for every scatter material. Opaque trunk
                // textures (albedo a == 1) are unaffected; leaf atlases get cut out.
                materials[s].constantAlphaCutoff = kFoliageAlphaCutoff;
                rp.parts.push_back(ScatterDrawPart{
                    .mesh = &mesh, .subMesh = &mesh.subMeshes[s],
                    .material = &materials[s], .meshXform = meshXform });
            }
            const AABB wb = transformAABB(mesh.bounds, meshXform);
            if (!haveBounds) { lb = wb; haveBounds = true; }
            else {
                const mu::Vec3 lo0 = lb.center - lb.size * 0.5f, hi0 = lb.center + lb.size * 0.5f;
                const mu::Vec3 lo1 = wb.center - wb.size * 0.5f, hi1 = wb.center + wb.size * 0.5f;
                const mu::Vec3 lo(std::min(lo0.x(),lo1.x()), std::min(lo0.y(),lo1.y()), std::min(lo0.z(),lo1.z()));
                const mu::Vec3 hi(std::max(hi0.x(),hi1.x()), std::max(hi0.y(),hi1.y()), std::max(hi0.z(),hi1.z()));
                lb = AABB{ .center = (lo + hi) * 0.5f, .size = hi - lo };
            }
        }
        rp.localBounds = lb;
    }

    gSharedLog << "[ChunkManager] Scatter assets loaded: " << propModels_.size()
               << " prop models, " << billboardCount << " billboard prototypes.\n";
}

TerrainChunkManager::ScatterWorldXform
TerrainChunkManager::resolveInstanceXform(const LoadedChunk& slot, const ScatterInstance& inst) const {
    const mu::Vec3 offset = worldOffset(slot.entry.col, slot.entry.row);
    const auto&    proto  = index_.scatterPrototypes[inst.protoIdx];

    // Ground-snap to this chunk's height field (height map matches Unity's, so this
    // only removes float drift) and place at the chunk world offset. The server
    // mirrors this exact ground-snap so prop colliders are identical on both peers.
    mu::Vec3 pos = offset + inst.posLocal;
    if (!slot.data.heightField.empty())
        pos = mu::Vec3(pos.x(), offset.y() + slot.data.heightField.getHeightAt(inst.posLocal.x(), inst.posLocal.z()), pos.z());

    const mu::Vec3 scl = (proto.kind == ScatterPrototype::Kind::BillboardGrass)
        ? mu::Vec3(proto.billboardWidth * inst.scaleW, proto.billboardHeight * inst.scaleH, proto.billboardWidth * inst.scaleW)
        : mu::Vec3(inst.scaleW, inst.scaleH, inst.scaleW);

    return ScatterWorldXform{ pos, mu::NQuat(inst.rot.x, inst.rot.y, inst.rot.z, inst.rot.w), scl };
}

void TerrainChunkManager::resolveChunkScatter(LoadedChunk& slot) {
    slot.scatter.clear();
    if (resolvedProtos_.empty() || slot.entry.scatter.empty()) return;

    slot.scatter.reserve(slot.entry.scatter.size());

    for (const auto& inst : slot.entry.scatter) {
        if (inst.protoIdx >= resolvedProtos_.size()) continue;

        const ScatterWorldXform x = resolveInstanceXform(slot, inst);
        const mu::Mat4x4 world = makeScatterWorld(x.pos, x.rot, x.scale);

        slot.scatter.push_back(ScatterInstanceResolved{
            .protoIdx = inst.protoIdx,
            .world    = world,
            .worldAABB = transformAABB(resolvedProtos_[inst.protoIdx].localBounds, world)
        });
    }
}

void TerrainChunkManager::registerChunkScatterCollider(LoadedChunk& slot) {
    if (!physicsWorld_ || resolvedProtos_.empty() || slot.entry.scatter.empty()) return;

    std::vector<ScatterCollider::InstanceInput> insts;
    for (const auto& inst : slot.entry.scatter) {
        if (inst.protoIdx >= resolvedProtos_.size()) continue;
        const auto& rp = resolvedProtos_[inst.protoIdx];
        if (!rp.collidable || !rp.model) continue;   // billboards / no-BVH props are skipped

        const ScatterWorldXform x = resolveInstanceXform(slot, inst);
        insts.push_back(ScatterCollider::InstanceInput{ &rp.model->bvh, x.pos, x.rot, x.scale });
    }
    if (insts.empty()) return;

    auto collider = std::make_unique<ScatterCollider>(insts);
    if (!collider->empty())
        slot.scatterHandle = physicsWorld_->registerScatter(std::move(collider));
}

void TerrainChunkManager::submitScatterDrawEvents(GFX& gfx, const LoadedChunk& slot) const {
    if (slot.scatter.empty()) return;
    const bool  useDeferred  = (gfx.renderPath() == GFX::RenderPath::Deferred);
    // GPU Hi-Z occlusion for BVH props is only available on the deferred path with culling on.
    const bool  hiZForProps  = useDeferred && gfx.isHiZCullEnabled();
    const float detailCullR2 = kDetailCullRadius * kDetailCullRadius;
    const float occluderR2   = kPropOccluderRadius * kPropOccluderRadius;

    // [TEMP DEBUG] draw the bounding volumes of collidable props (only those that HAVE a BV),
    // in world space, matching the geometry ScatterCollider uses for collision.
    // Set to false (or delete this block) to revert.
    static constexpr bool kDebugDrawPropBVs = false;

    for (const auto& inst : slot.scatter) {
        if constexpr (kDebugDrawPropBVs) {
            const auto& rp = resolvedProtos_[inst.protoIdx];
            if (rp.collidable && rp.model) {
                for (const auto& node : rp.model->bvh.nodes) {
                    if (!node.isLeaf()) continue;
                    std::visit([&](auto&& s) {
                        using S = std::decay_t<decltype(s)>;
                        mu::Mat4x4 local;
                        if constexpr (std::is_same_v<S, AABB>)
                            local = mu::Mat4x4(mu::scale(s.size)) * mu::translate(s.center);
                        else
                            local = mu::Mat4x4(mu::scale(s.halfExtents * 2.f))
                                  * mu::Mat4x4(s.orient) * mu::translate(s.center);
                        gfx.addDrawEvent(BVPipeline::DrawEvent{
                            .world   = local * inst.world,
                            .bvModel = BVPipeline::BVModel::Box,
                            .color   = mu::Vec4{ 1.f, 1.f, 0.f, 1.f } });
                    }, node.shape);
                }
            }
        }

        const auto& proto  = resolvedProtos_[inst.protoIdx];
        const bool  hasBVH = proto.collidable;   // model && !model->bvh.empty()

        // Culling policy by BVH presence:
        //   no BVH  (grass/flowers/bushes) → distance cull around the player only.
        //   has BVH (trees/mesh props)     → per-instance VFC; survivors go through the
        //                                    GPU Hi-Z occlusion path (when available), and
        //                                    near-camera ones additionally act as occluders.
        bool occludee = false;
        if (!hasBVH) {
            if (hasCullCenter_) {
                const float dx = inst.worldAABB.center.x() - cullCenter_.x();
                const float dz = inst.worldAABB.center.z() - cullCenter_.z();
                if (dx * dx + dz * dz > detailCullR2) continue;
            }
        } else {
            if (hasFrustum_ && !intersects(frustum_, inst.worldAABB)) continue;

            if (hiZForProps) {
                occludee = true;

                const float dxc = inst.worldAABB.center.x() - cameraPos_.x();
                const float dyc = inst.worldAABB.center.y() - cameraPos_.y();
                const float dzc = inst.worldAABB.center.z() - cameraPos_.z();
                if (dxc * dxc + dyc * dyc + dzc * dzc < occluderR2) {
                    for (const auto& part : proto.parts) {
                        gfx.addOccluder(PBRDeferredPipeline::OccluderInfo{
                            .mesh = part.mesh, .subMesh = part.subMesh,
                            .world = part.meshXform * inst.world });
                    }
                }
            }
        }

        for (const auto& part : proto.parts) {
            const mu::Mat4x4 world = part.meshXform * inst.world;
            if (useDeferred) {
                gfx.addDrawEvent(PBRDeferredPipeline::DrawEvent{
                    .world = world, .mesh = part.mesh, .subMesh = part.subMesh,
                    .material = part.material, .viewFrustumCulled = false,
                    .shadowCulled = false, .occludeeCandidate = occludee });
            } else {
                gfx.addDrawEvent(PBRPipeline::DrawEvent{
                    .world = world, .mesh = part.mesh, .subMesh = part.subMesh,
                    .material = part.material, .viewFrustumCulled = false, .shadowCulled = false });
            }
        }
    }
}

void TerrainChunkManager::update(mu::Vec3 playerWorldPos, Milliseconds dt) {
    if (index_.chunks.empty()) return;

    // Center for scatter detail distance-culling (see submitScatterDrawEvents).
    cullCenter_    = playerWorldPos;
    hasCullCenter_ = true;

    const float dtSec = std::chrono::duration<float>(dt).count();

    // Resolve the player's current chunk; keep the last known one if the player is
    // momentarily over a hole / outside the populated region.
    auto cur = chunkCoordAtWorld(playerWorldPos.x(), playerWorldPos.z());
    if (cur) lastCur_ = cur;

    std::unordered_set<int64_t> desired;
    int64_t curKey = static_cast<int64_t>(-1);
    if (lastCur_) {
        desired = computeDesired(lastCur_->first, lastCur_->second);
        curKey  = packCoord(lastCur_->first, lastCur_->second);
    }

    int loading = 0;
    for (auto& [k, s] : chunks_)
        if (s.state == State::Loading) ++loading;

    // Start loads for desired chunks not yet present; cancel pending expiry for ones
    // that re-entered the desired set.
    for (int64_t key : desired) {
        auto it = chunks_.find(key);
        if (it == chunks_.end()) {
            if (loading < maxConcurrentLoads_) {
                startLoad(*indexByCoord_.at(key));
                ++loading;
            }
        } else if (it->second.state == State::Expiring) {
            it->second.state = State::Ready;
            it->second.expireSeconds = 0.f;
        }
    }

    // Expire / unload chunks outside the desired set (never the chunk underfoot).
    std::vector<int64_t> toUnload;
    for (auto& [key, slot] : chunks_) {
        if (key == curKey) {
            if (slot.state == State::Expiring) { slot.state = State::Ready; slot.expireSeconds = 0.f; }
            continue;
        }
        if (desired.count(key) > 0) continue;

        if (slot.state == State::Ready) {
            slot.state = State::Expiring;
            slot.expireSeconds = 0.f;
        }
        if (slot.state == State::Expiring) {
            slot.expireSeconds += dtSec;
            if (slot.expireSeconds >= graceSeconds_) toUnload.push_back(key);
        }
        // Loading chunks outside desired are left to finish; they expire next pass.
    }
    for (int64_t key : toUnload) {
        auto it = chunks_.find(key);
        if (it == chunks_.end()) continue;
        unloadChunk(it->second);
        chunks_.erase(it);
    }

    drainCompletedBuilds();
    tickGraveyard();
}

std::unordered_set<int64_t>
TerrainChunkManager::computeDesired(int curCol, int curRow) const {
    std::unordered_set<int64_t> visited;
    const int64_t startKey = packCoord(curCol, curRow);
    if (indexByCoord_.find(startKey) == indexByCoord_.end()) return visited;

    std::vector<std::pair<int64_t, int>> queue;   // (key, hop depth)
    queue.push_back({ startKey, 0 });
    visited.insert(startKey);

    for (std::size_t qi = 0; qi < queue.size(); ++qi) {
        const auto [key, depth] = queue[qi];
        if (depth >= maxHop_) continue;
        auto it = indexByCoord_.find(key);
        if (it == indexByCoord_.end()) continue;
        for (const auto& [nc, nr] : it->second->neighbors) {
            const int64_t nkey = packCoord(nc, nr);
            if (indexByCoord_.find(nkey) == indexByCoord_.end()) continue;
            if (visited.insert(nkey).second)
                queue.push_back({ nkey, depth + 1 });
        }
    }
    return visited;
}

void TerrainChunkManager::startLoad(const ChunkIndexEntry& entry) {
    const int64_t key = packCoord(entry.col, entry.row);
    auto& slot = chunks_[key];
    slot.entry = entry;
    slot.state = State::Loading;
    slot.expireSeconds = 0.f;
    slot.cpu = std::make_unique<ChunkCpuBuild>();

    ChunkCpuBuild*         cpuPtr = slot.cpu.get();
    const ChunkIndexEntry* ePtr   = indexByCoord_.at(key);

    // The CPU build (height parse + vertex/normal/tangent/index/heightField generation)
    // touches no shared mutable state, so it is safe on a worker thread.
    if (threadPool_) {
        threadPool_->addJob([cpuPtr, ePtr, dir = terrainDir_]() {
            buildChunkCpu(*ePtr, dir, *cpuPtr);
        });
    } else {
        buildChunkCpu(*ePtr, terrainDir_, *cpuPtr);
    }
}

void TerrainChunkManager::drainCompletedBuilds() {
    int budget = maxGpuFinalizePerFrame_;
    std::vector<int64_t> toErase;

    for (auto& [key, slot] : chunks_) {
        if (budget <= 0) break;
        if (slot.state != State::Loading || !slot.cpu) continue;
        if (!slot.cpu->done.load(std::memory_order_acquire)) continue;

        if (slot.cpu->failed) {
            toErase.push_back(key);
            continue;
        }

        // GPU finalize on the main thread (brief wait reclaims the upload buffers).
        ChunkCpuBuild* cpuPtr = slot.cpu.get();
        gfx_->recordTerrainResourceLoad(
            [&](ID3D12Device* device, ID3D12GraphicsCommandList* cmdList,
                DescriptorPool& texPool, Fence& fence) {
                slot.data = finalizeChunkGpu(*cpuPtr, palette_, terrainDir_, device, cmdList,
                                             texHashMap_, texPool, fence);
            },
            /*wait=*/true);

        slot.cpu.reset();
        activateChunkRenderAndPhysics(slot);   // sets State::Ready
        --budget;
    }

    for (int64_t key : toErase) chunks_.erase(key);
}

void TerrainChunkManager::activateChunkRenderAndPhysics(LoadedChunk& slot) {
    const mu::Vec3 offset = worldOffset(slot.entry.col, slot.entry.row);

    auto obj = std::make_shared<TerrainObject>();
    obj->body().setMotionType(MotionType::Static);
    obj->setPos(offset);
    obj->setTerrainData(&slot.data);
    obj->activateOcclusion(true);
    obj->update(Milliseconds(0), 1.f);   // populate renderState_.world (chunk is static)

    slot.object = std::move(obj);

    if (physicsWorld_ && !slot.data.heightField.empty()) {
        slot.physHandle = physicsWorld_->registerTerrain(
            &slot.object->body(), &slot.data.heightField);
    }

    // Resolve this chunk's scattered foliage instances (resident; see header).
    resolveChunkScatter(slot);

    // Register a static prop collider for this chunk's collidable scatter (trees/rocks).
    registerChunkScatterCollider(slot);

    slot.state = State::Ready;
}

void TerrainChunkManager::unloadChunk(LoadedChunk& slot) {
    if (physicsWorld_ && slot.physHandle != static_cast<std::size_t>(-1)) {
        physicsWorld_->unregisterTerrain(slot.physHandle);
        slot.physHandle = static_cast<std::size_t>(-1);
    }
    if (physicsWorld_ && slot.scatterHandle != static_cast<std::size_t>(-1)) {
        physicsWorld_->unregisterScatter(slot.scatterHandle);
        slot.scatterHandle = static_cast<std::size_t>(-1);
    }

    PendingUnload pu;
    pu.framesLeft  = graveyardFrameDelay_;
    pu.splatSrvIdx = (slot.data.splatMap.idxSrv.idxResource >= 0)
                   ? slot.data.splatMap.idxSrv.idxResource : -1;

    // Move the splat GPU resource owner (the texHashMap_ entry) into the graveyard so
    // it is released only after the GPU is no longer referencing it. Palette textures
    // are shared and are NOT freed here.
    const std::string splatKey = "Terrain_Splat_" + std::to_string(slot.entry.col)
                               + "_" + std::to_string(slot.entry.row);
    auto tit = texHashMap_.find(splatKey);
    if (tit != texHashMap_.end()) {
        pu.retainedTex = std::move(tit->second);
        texHashMap_.erase(tit);
    }

    pu.data   = std::move(slot.data);
    pu.object = std::move(slot.object);
    graveyard_.push_back(std::move(pu));
}

void TerrainChunkManager::tickGraveyard() {
    for (auto& pu : graveyard_) --pu.framesLeft;

    for (std::size_t i = 0; i < graveyard_.size();) {
        if (graveyard_[i].framesLeft <= 0) {
            if (graveyard_[i].splatSrvIdx >= 0 && gfx_)
                gfx_->bindlessTexPool().free(graveyard_[i].splatSrvIdx);
            graveyard_[i] = std::move(graveyard_.back());
            graveyard_.pop_back();
        } else {
            ++i;
        }
    }
}

void TerrainChunkManager::submitDrawEvents(GFX& gfx) {
    for (auto& [key, slot] : chunks_) {
        if ((slot.state == State::Ready || slot.state == State::Expiring) && slot.object) {
            slot.object->render(gfx);
            submitScatterDrawEvents(gfx, slot);
        }
    }
}

float TerrainChunkManager::readyFractionAround(mu::Vec3 worldPos) const {
    if (index_.chunks.empty()) return 1.f;

    const auto cur = chunkCoordAtWorld(worldPos.x(), worldPos.z());
    if (!cur) return 1.f;

    const auto desired = computeDesired(cur->first, cur->second);
    if (desired.empty()) return 1.f;

    int ready = 0;
    for (const int64_t key : desired) {
        auto it = chunks_.find(key);
        if (it == chunks_.end()) continue;
        const auto state = it->second.state;
        if ((state == State::Ready || state == State::Expiring) && it->second.object) {
            ++ready;
        }
    }

    return static_cast<float>(ready) / static_cast<float>(desired.size());
}

const TerrainChunkManager::LoadedChunk*
TerrainChunkManager::findReady(int col, int row) const {
    auto it = chunks_.find(packCoord(col, row));
    if (it == chunks_.end()) return nullptr;
    if (it->second.state != State::Ready && it->second.state != State::Expiring) return nullptr;
    return &it->second;
}

std::optional<std::pair<int,int>>
TerrainChunkManager::chunkCoordAtWorld(float x, float z) const {
    if (chunkSizeX_ <= 0.f || chunkSizeZ_ <= 0.f) return std::nullopt;
    const int col = static_cast<int>(std::floor(x / chunkSizeX_));
    const int row = static_cast<int>(std::floor(z / chunkSizeZ_));
    if (indexByCoord_.find(packCoord(col, row)) == indexByCoord_.end())
        return std::nullopt;
    return std::make_pair(col, row);
}

float MU_CALLCONV TerrainChunkManager::heightAtWorld(float x, float z) const {
    auto coord = chunkCoordAtWorld(x, z);
    if (!coord) return 0.f;
    const LoadedChunk* c = findReady(coord->first, coord->second);
    if (!c) return 0.f;
    const mu::Vec3 off = worldOffset(coord->first, coord->second);
    return off.y() + c->data.heightField.getHeightAt(x - off.x(), z - off.z());
}

mu::Vec3 MU_CALLCONV TerrainChunkManager::normalAtWorld(float x, float z) const {
    auto coord = chunkCoordAtWorld(x, z);
    if (!coord) return mu::Vec3(0.f, 1.f, 0.f);
    const LoadedChunk* c = findReady(coord->first, coord->second);
    if (!c) return mu::Vec3(0.f, 1.f, 0.f);
    const mu::Vec3 off = worldOffset(coord->first, coord->second);
    return c->data.heightField.getNormalAt(x - off.x(), z - off.z());
}

#include "pch.hpp"
#include "mesh.hpp"
#include "gfxUtil.hpp"
#include "errorHandling.hpp"
#include "binaryImport.hpp"

// 1x1x1 큐브 메시를 생성한다.
// @return Mesh
// 메시 로드에 임시 업로드 버퍼들이 사용된다.
// 사용된 업로드 버퍼들은 전달된 펜스에 연관되므로,
// 펜스에서 GPU 작업 완료를 검사한 후 이 업로드 버퍼들을 해제하도록 하자.
Mesh buildCubeMesh(
	ID3D12Device* device, ID3D12GraphicsCommandList* cmdList,
    std::unordered_map<std::string, Texture>& texHashMap,
    DescriptorPool& texPool, Fence& fenceToAssociate
) {
    static const auto positions = std::vector<XMFLOAT3>{
        XMFLOAT3(-0.5f,-0.5f,-0.5f),    // triangle 1
        XMFLOAT3(-0.5f,-0.5f, 0.5f),
        XMFLOAT3(-0.5f, 0.5f, 0.5f),
        XMFLOAT3(0.5f, 0.5f,-0.5f),     // triangle 2 ...
        XMFLOAT3(-0.5f,-0.5f,-0.5f),
        XMFLOAT3(-0.5f, 0.5f,-0.5f), 
        XMFLOAT3(0.5f,-0.5f, 0.5f),     
        XMFLOAT3(-0.5f,-0.5f,-0.5f),
        XMFLOAT3(0.5f,-0.5f,-0.5f),
        XMFLOAT3(0.5f, 0.5f,-0.5f),     
        XMFLOAT3(0.5f,-0.5f,-0.5f),
        XMFLOAT3(-0.5f,-0.5f,-0.5f),
        XMFLOAT3(-0.5f,-0.5f,-0.5f),    
        XMFLOAT3(-0.5f, 0.5f, 0.5f),
        XMFLOAT3(-0.5f, 0.5f,-0.5f),
        XMFLOAT3(0.5f,-0.5f, 0.5f),     
        XMFLOAT3(-0.5f,-0.5f, 0.5f),
        XMFLOAT3(-0.5f,-0.5f,-0.5f),
        XMFLOAT3(-0.5f, 0.5f, 0.5f),    
        XMFLOAT3(-0.5f,-0.5f, 0.5f),
        XMFLOAT3(0.5f,-0.5f, 0.5f),
        XMFLOAT3(0.5f, 0.5f, 0.5f),     
        XMFLOAT3(0.5f,-0.5f,-0.5f),
        XMFLOAT3(0.5f, 0.5f,-0.5f),
        XMFLOAT3(0.5f,-0.5f,-0.5f),     
        XMFLOAT3(0.5f, 0.5f, 0.5f),
        XMFLOAT3(0.5f,-0.5f, 0.5f),
        XMFLOAT3(0.5f, 0.5f, 0.5f),     
        XMFLOAT3(0.5f, 0.5f,-0.5f),
        XMFLOAT3(-0.5f, 0.5f,-0.5f),
        XMFLOAT3(0.5f, 0.5f, 0.5f),     
        XMFLOAT3(-0.5f, 0.5f,-0.5f),
        XMFLOAT3(-0.5f, 0.5f, 0.5f),
        XMFLOAT3(0.5f, 0.5f, 0.5f),     
        XMFLOAT3(-0.5f, 0.5f, 0.5f),
        XMFLOAT3(0.5f,-0.5f, 0.5f)
    };

    static const auto normals = std::vector<XMFLOAT3>{
        XMFLOAT3(-1.f, 0.f, 0.f),   // triangle 1
        XMFLOAT3(-1.f, 0.f, 0.f),
        XMFLOAT3(-1.f, 0.f, 0.f),
        XMFLOAT3(0.f, 0.f, -1.f),   // triangle 2 ...
        XMFLOAT3(0.f, 0.f, -1.f),
        XMFLOAT3(0.f, 0.f, -1.f),
        XMFLOAT3(0.f, -1.f, 0.f),
        XMFLOAT3(0.f, -1.f, 0.f),
        XMFLOAT3(0.f, -1.f, 0.f),
        XMFLOAT3(0.f, 0.f, -1.f),
        XMFLOAT3(0.f, 0.f, -1.f),
        XMFLOAT3(0.f, 0.f, -1.f),
        XMFLOAT3(-1.f, 0.f, 0.f),
        XMFLOAT3(-1.f, 0.f, 0.f),
        XMFLOAT3(-1.f, 0.f, 0.f),
        XMFLOAT3(0.f, -1.f, 0.f),
        XMFLOAT3(0.f, -1.f, 0.f),
        XMFLOAT3(0.f, -1.f, 0.f),
        XMFLOAT3(0.f, 0.f, 1.f),
        XMFLOAT3(0.f, 0.f, 1.f),
        XMFLOAT3(0.f, 0.f, 1.f),
        XMFLOAT3(1.f, 0.f, 0.f),
        XMFLOAT3(1.f, 0.f, 0.f),
        XMFLOAT3(1.f, 0.f, 0.f),
        XMFLOAT3(1.f, 0.f, 0.f),
        XMFLOAT3(1.f, 0.f, 0.f),
        XMFLOAT3(1.f, 0.f, 0.f),
        XMFLOAT3(0.f, 1.f, 0.f),
        XMFLOAT3(0.f, 1.f, 0.f),
        XMFLOAT3(0.f, 1.f, 0.f),
        XMFLOAT3(0.f, 1.f, 0.f),
        XMFLOAT3(0.f, 1.f, 0.f),
        XMFLOAT3(0.f, 1.f, 0.f),
        XMFLOAT3(0.f, 0.f, 1.f),
        XMFLOAT3(0.f, 0.f, 1.f),
        XMFLOAT3(0.f, 0.f, 1.f),
    };

    static const auto uvs = std::vector<XMFLOAT2>{
        XMFLOAT2(0.000059f, 1.0f - 0.000004f),  // triangle 1
        XMFLOAT2(0.335973f, 1.0f - 0.335903f),
        XMFLOAT2(0.000103f, 1.0f - 0.336048f),
        XMFLOAT2(1.000023f, 1.0f - 0.000013f),  // triangle 2 ...
        XMFLOAT2(0.999958f, 1.0f - 0.336064f),
        XMFLOAT2(0.667979f, 1.0f - 0.335851f),
        XMFLOAT2(0.667979f, 1.0f - 0.335851f),  
        XMFLOAT2(0.667969f, 1.0f - 0.671889f),
        XMFLOAT2(0.336024f, 1.0f - 0.671877f),
        XMFLOAT2(1.000023f, 1.0f - 0.000013f),  
        XMFLOAT2(0.667979f, 1.0f - 0.335851f),
        XMFLOAT2(0.668104f, 1.0f - 0.000013f),
        XMFLOAT2(0.000059f, 1.0f - 0.000004f),  
        XMFLOAT2(0.336098f, 1.0f - 0.000071f),
        XMFLOAT2(0.335973f, 1.0f - 0.335903f),
        XMFLOAT2(0.667979f, 1.0f - 0.335851f),  
        XMFLOAT2(0.336024f, 1.0f - 0.671877f),
        XMFLOAT2(0.335973f, 1.0f - 0.335903f),
        XMFLOAT2(1.000004f, 1.0f - 0.671847f),  
        XMFLOAT2(0.667979f, 1.0f - 0.335851f),
        XMFLOAT2(0.999958f, 1.0f - 0.336064f),
        XMFLOAT2(0.668104f, 1.0f - 0.000013f),  
        XMFLOAT2(0.667979f, 1.0f - 0.335851f),
        XMFLOAT2(0.335973f, 1.0f - 0.335903f),
        XMFLOAT2(0.335973f, 1.0f - 0.335903f),  
        XMFLOAT2(0.336098f, 1.0f - 0.000071f),
        XMFLOAT2(0.668104f, 1.0f - 0.000013f),
        XMFLOAT2(0.000103f, 1.0f - 0.336048f),  
        XMFLOAT2(0.336024f, 1.0f - 0.671877f),
        XMFLOAT2(0.000004f, 1.0f - 0.671870f),
        XMFLOAT2(0.000103f, 1.0f - 0.336048f),  
        XMFLOAT2(0.335973f, 1.0f - 0.335903f),
        XMFLOAT2(0.336024f, 1.0f - 0.671877f),
        XMFLOAT2(0.667969f, 1.0f - 0.671889f),  
        XMFLOAT2(0.667979f, 1.0f - 0.335851f),
        XMFLOAT2(1.000004f, 1.0f - 0.671847f)
    };

    static const auto indices = std::vector<u16t>{
        0u, 1u, 2u, 3u, 4u, 5u, 6u, 7u, 8u, 9u, 10u, 11u, 12u, 13u, 14u, 15u, 16u, 17u,
        18u, 19u, 20u, 21u, 22u, 23u, 24u, 25u, 26u, 27u, 28u, 29u, 30u, 31u, 32u, 33u, 34u, 35u
    };

    // 정점 버퍼들 구축
    // 속성마다 별도의 버퍼를 만든다.
	auto vbPosition = createBufferResource(device, nullptr, positions.size() * sizeof(XMFLOAT3), BufferCreationType::VertexBuffer);
    setD3DName(vbPosition.Get(), "CubeMesh_VB_Position");
	auto vbPositionu = createBufferResource(device, positions.data(), positions.size() * sizeof(XMFLOAT3), BufferCreationType::UploadBuffer);
    setD3DName(vbPositionu.Get(), "CubeMesh_VB_Position_Upload");

	copyResource( cmdList, vbPositionu.Get(), vbPosition.Get(), D3D12_RESOURCE_STATE_GENERIC_READ,
		D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER
	);

    auto vbNormal = createBufferResource(device, nullptr, normals.size() * sizeof(XMFLOAT3), BufferCreationType::VertexBuffer);
    setD3DName(vbNormal.Get(), "CubeMesh_VB_Normal");
    auto vbNormalu = createBufferResource(device, normals.data(), normals.size() * sizeof(XMFLOAT3), BufferCreationType::UploadBuffer);
    setD3DName(vbNormalu.Get(), "CubeMesh_VB_Normal_Upload");

    copyResource( cmdList, vbNormalu.Get(), vbNormal.Get(), D3D12_RESOURCE_STATE_GENERIC_READ,
        D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER
    );

    auto vbUV = createBufferResource(device, nullptr, uvs.size() * sizeof(XMFLOAT3), BufferCreationType::VertexBuffer);
    setD3DName(vbUV.Get(), "CubeMesh_VB_UV");
    auto vbUVu = createBufferResource(device, uvs.data(), uvs.size() * sizeof(XMFLOAT3), BufferCreationType::UploadBuffer);
    setD3DName(vbUVu.Get(), "CubeMesh_VB_UV_Upload");

    copyResource( cmdList, vbUVu.Get(), vbUV.Get(), D3D12_RESOURCE_STATE_GENERIC_READ,
        D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER
    );

    // 인덱스 버퍼 구축
	auto ib = createBufferResource(device, nullptr, indices.size() * sizeof(u16t), BufferCreationType::IndexBuffer);
    setD3DName(ib.Get(), "CubeMesh_IB");
	auto ibu = createBufferResource(device, indices.data(), indices.size() * sizeof(u16t), BufferCreationType::UploadBuffer);
    setD3DName(ibu.Get(), "CubeMesh_IB_Upload");

	copyResource( cmdList, ibu.Get(), ib.Get(), D3D12_RESOURCE_STATE_GENERIC_READ,
		D3D12_RESOURCE_STATE_INDEX_BUFFER
	);

    // 만든 버퍼들을 조합하여 메시 구축
    // 사용한 업로드 버퍼들을 별도로 리턴값에 포함시켜
    // 업로드 버퍼들이 소멸하지 않게 한다.
    // (업로드 버퍼들은 gpu가 실제로 copy를 수행할 때까지 살아있어야 한다.)
	auto mesh = Mesh{ .name = "CubeMesh" };

    // Vertex Buffer View 구성
    mesh.vbViews.emplace_back(
        /* .BufferLocation = */ vbPosition->GetGPUVirtualAddress(),
        /* .SizeInBytes = */ static_cast<UINT>(positions.size() * sizeof(XMFLOAT3)),
        /* .StrideInBytes = */ static_cast<UINT>( sizeof(XMFLOAT3) )
    );
    mesh.vbViews.emplace_back(
        /* .BufferLocation = */ vbNormal->GetGPUVirtualAddress(),
        /* .SizeInBytes = */ static_cast<UINT>(normals.size() * sizeof(XMFLOAT3)),
        /* .StrideInBytes = */ static_cast<UINT>( sizeof(XMFLOAT3) )
    );
    mesh.vbViews.emplace_back(
        /* .BufferLocation = */ vbUV->GetGPUVirtualAddress(),
        /* .SizeInBytes = */ static_cast<UINT>(uvs.size() * sizeof(XMFLOAT2)),
        /* .StrideInBytes = */ static_cast<UINT>( sizeof(XMFLOAT2) )
    );

    if (!texHashMap.contains("CubeMesh_Albedo")) {
        Texture::Type type{};
        auto [pPair, _] = texHashMap.try_emplace("CubeMesh_Albedo", loadTexture(device, cmdList, "CubeMesh_Albedo.dds", fenceToAssociate, type));
        gSharedLog << "[Resource Load] File I/O: 텍스처 CubeMesh_Albedo 로드 완료 - CubeMesh_Albedo.dds\n";
        createSRV(device, pPair->second, texPool);
        pPair->second.idxSrv.idxSampler = etoi(Samplers::TrilinearWrap);
    }

    // SubMesh 구성
    mesh.subMeshes.emplace_back(
        /* .name = */ "CubeMesh_SubMesh",
        /* .ibView = */ D3D12_INDEX_BUFFER_VIEW {
            .BufferLocation = ib->GetGPUVirtualAddress(),
            .SizeInBytes = static_cast<UINT>(indices.size() * sizeof(u16t)),
            .Format = DXGI_FORMAT_R16_UINT
        }
    );

    auto& defMaterialSet = mesh.materialSets.emplace_back(
        /* .name = */ "CubeMesh_DefaultMaterialSet",
        /* .materials = */ std::vector<Material>{
            Material{
                .constantAlbedo = XMFLOAT4(0.9f, 0.9f ,0.9f, 1.f),
                .constantRoughness = 0.3f,
                .constantMetallic = 0.15f,
                .constantAOStrength = 0.f,
                .constantEmmisive = XMFLOAT3(0.f, 0.f, 0.f)
            }
        }
    );
    defMaterialSet.materials[0].mapAlbedo.idxSrv.idxRange = -1;
    defMaterialSet.materials[0].mapAlbedo.idxUav.idxRange = -1;
    defMaterialSet.materials[0].mapMetallicSmoothness.idxSrv.idxRange = -1;
    defMaterialSet.materials[0].mapMetallicSmoothness.idxUav.idxRange = -1;
    defMaterialSet.materials[0].mapNormal.idxSrv.idxRange = -1;
    defMaterialSet.materials[0].mapNormal.idxUav.idxRange = -1;
    defMaterialSet.materials[0].mapEmmisive.idxSrv.idxRange = -1;
    defMaterialSet.materials[0].mapEmmisive.idxUav.idxRange = -1;
    defMaterialSet.materials[0].mapAmbientOcclusion.idxSrv.idxRange = -1;
    defMaterialSet.materials[0].mapAmbientOcclusion.idxUav.idxRange = -1;

    // 자료구조 등록 (Vertex Buffer View와 SubMesh는 위에서 등록하였음)
	mesh.vbs.push_back(std::move(vbPosition));
    mesh.vbIdxMap.try_emplace("CubeMesh_VB_Position", 0u);
    mesh.vbs.push_back(std::move(vbNormal));
    mesh.vbIdxMap.try_emplace("CubeMesh_VB_Normal", 1u);
    mesh.vbs.push_back(std::move(vbUV));
    mesh.vbIdxMap.try_emplace("CubeMesh_VB_UV", 2u);
    mesh.ibs.push_back(std::move(ib));

    gSharedLog << "[Resource Load] CubeMesh 구축 완료\n";

    fenceToAssociate.associatedResources_.push_back(std::move(vbPositionu));
    fenceToAssociate.associatedResources_.push_back(std::move(vbNormalu));
    fenceToAssociate.associatedResources_.push_back(std::move(vbUVu));
    fenceToAssociate.associatedResources_.push_back(std::move(ibu));

    return mesh;
}

// Procedural UV sphere. 5 vertex attributes for the PBR(Deferred) input layout
// (Position/Normal/Tangent/Bitangent/UV) + 16-bit indices. Material is a smooth
// metal with no textures, for inspecting IBL reflections.
Mesh buildSphereMesh(
    ID3D12Device* device, ID3D12GraphicsCommandList* cmdList,
    Fence& fenceToAssociate, float radius, u32t stacks, u32t slices
) {
    constexpr float kPi = 3.14159265358979f;

    std::vector<XMFLOAT3> positions;
    std::vector<XMFLOAT3> normals;
    std::vector<XMFLOAT3> tangents;
    std::vector<XMFLOAT3> bitangents;
    std::vector<XMFLOAT2> uvs;

    for (u32t i = 0u; i <= stacks; ++i) {
        const float v     = static_cast<float>(i) / static_cast<float>(stacks);
        const float theta = v * kPi;            // 0 (top) .. PI (bottom)
        const float sinT  = std::sin(theta);
        const float cosT  = std::cos(theta);
        for (u32t j = 0u; j <= slices; ++j) {
            const float u    = static_cast<float>(j) / static_cast<float>(slices);
            const float phi  = u * 2.f * kPi;   // 0 .. 2PI
            const float sinP = std::sin(phi);
            const float cosP = std::cos(phi);

            const XMFLOAT3 n = XMFLOAT3(sinT * cosP, cosT, sinT * sinP);
            positions.emplace_back(n.x * radius, n.y * radius, n.z * radius);
            normals.emplace_back(n);
            // tangent along +phi (east); bitangent = cross(normal, tangent)
            const XMFLOAT3 t = XMFLOAT3(-sinP, 0.f, cosP);
            tangents.emplace_back(t);
            bitangents.emplace_back(
                n.y * t.z - n.z * t.y,
                n.z * t.x - n.x * t.z,
                n.x * t.y - n.y * t.x
            );
            uvs.emplace_back(u, v);
        }
    }

    std::vector<u16t> indices;
    const u32t ringStride = slices + 1u;
    for (u32t i = 0u; i < stacks; ++i) {
        for (u32t j = 0u; j < slices; ++j) {
            const u16t row0 = static_cast<u16t>(i * ringStride + j);
            const u16t row1 = static_cast<u16t>((i + 1u) * ringStride + j);
            // DX back-face cull (CW front). If the sphere ever renders inside-out,
            // swap the last two indices of each triangle.
            indices.push_back(row0);
            indices.push_back(static_cast<u16t>(row0 + 1u));
            indices.push_back(row1);
            indices.push_back(static_cast<u16t>(row0 + 1u));
            indices.push_back(static_cast<u16t>(row1 + 1u));
            indices.push_back(row1);
        }
    }

    // Build a default(GPU) vertex buffer + an upload buffer, record the copy, and
    // keep the upload buffer alive via the fence until the GPU finishes.
    auto makeVB = [&](const void* data, std::size_t bytes, const char* name) {
        auto def = createBufferResource(device, nullptr, bytes, BufferCreationType::VertexBuffer);
        setD3DName(def.Get(), name);
        auto upl = createBufferResource(device, data, bytes, BufferCreationType::UploadBuffer);
        copyResource(cmdList, upl.Get(), def.Get(), D3D12_RESOURCE_STATE_GENERIC_READ,
            D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
        fenceToAssociate.associatedResources_.push_back(std::move(upl));
        return def;
    };

    auto vbPosition  = makeVB(positions.data(),  positions.size()  * sizeof(XMFLOAT3), "MetalSphereMesh_VB_Position");
    auto vbNormal    = makeVB(normals.data(),    normals.size()    * sizeof(XMFLOAT3), "MetalSphereMesh_VB_Normal");
    auto vbTangent   = makeVB(tangents.data(),   tangents.size()   * sizeof(XMFLOAT3), "MetalSphereMesh_VB_Tangent");
    auto vbBitangent = makeVB(bitangents.data(), bitangents.size() * sizeof(XMFLOAT3), "MetalSphereMesh_VB_Bitangent");
    auto vbUV        = makeVB(uvs.data(),        uvs.size()        * sizeof(XMFLOAT2), "MetalSphereMesh_VB_UV");

    auto ib  = createBufferResource(device, nullptr, indices.size() * sizeof(u16t), BufferCreationType::IndexBuffer);
    setD3DName(ib.Get(), "MetalSphereMesh_IB");
    auto ibu = createBufferResource(device, indices.data(), indices.size() * sizeof(u16t), BufferCreationType::UploadBuffer);
    copyResource(cmdList, ibu.Get(), ib.Get(), D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_INDEX_BUFFER);
    fenceToAssociate.associatedResources_.push_back(std::move(ibu));

    auto mesh = Mesh{ .name = "MetalSphereMesh" };

    mesh.vbViews.emplace_back(vbPosition->GetGPUVirtualAddress(),  static_cast<UINT>(positions.size()  * sizeof(XMFLOAT3)), static_cast<UINT>(sizeof(XMFLOAT3)));
    mesh.vbViews.emplace_back(vbNormal->GetGPUVirtualAddress(),    static_cast<UINT>(normals.size()    * sizeof(XMFLOAT3)), static_cast<UINT>(sizeof(XMFLOAT3)));
    mesh.vbViews.emplace_back(vbTangent->GetGPUVirtualAddress(),   static_cast<UINT>(tangents.size()   * sizeof(XMFLOAT3)), static_cast<UINT>(sizeof(XMFLOAT3)));
    mesh.vbViews.emplace_back(vbBitangent->GetGPUVirtualAddress(), static_cast<UINT>(bitangents.size() * sizeof(XMFLOAT3)), static_cast<UINT>(sizeof(XMFLOAT3)));
    mesh.vbViews.emplace_back(vbUV->GetGPUVirtualAddress(),        static_cast<UINT>(uvs.size()        * sizeof(XMFLOAT2)), static_cast<UINT>(sizeof(XMFLOAT2)));

    mesh.subMeshes.emplace_back(
        /* .name = */ "MetalSphereMesh_SubMesh",
        /* .ibView = */ D3D12_INDEX_BUFFER_VIEW{
            .BufferLocation = ib->GetGPUVirtualAddress(),
            .SizeInBytes    = static_cast<UINT>(indices.size() * sizeof(u16t)),
            .Format         = DXGI_FORMAT_R16_UINT
        }
    );

    auto& matSet = mesh.materialSets.emplace_back(
        /* .name = */ "MetalSphereMesh_DefaultMaterialSet",
        /* .materials = */ std::vector<Material>{
            Material{
                .constantAlbedo     = XMFLOAT4(0.95f, 0.95f, 0.97f, 1.f),
                .constantRoughness  = 0.1f,
                .constantMetallic   = 1.0f,
                .constantAOStrength = 0.f,
                .constantEmmisive   = XMFLOAT3(0.f, 0.f, 0.f)
            }
        }
    );
    auto& mat = matSet.materials[0];
    mat.mapAlbedo.idxSrv.idxRange = -1;             mat.mapAlbedo.idxUav.idxRange = -1;
    mat.mapMetallicSmoothness.idxSrv.idxRange = -1; mat.mapMetallicSmoothness.idxUav.idxRange = -1;
    mat.mapNormal.idxSrv.idxRange = -1;             mat.mapNormal.idxUav.idxRange = -1;
    mat.mapEmmisive.idxSrv.idxRange = -1;           mat.mapEmmisive.idxUav.idxRange = -1;
    mat.mapAmbientOcclusion.idxSrv.idxRange = -1;   mat.mapAmbientOcclusion.idxUav.idxRange = -1;

    mesh.vbs.push_back(std::move(vbPosition));   mesh.vbIdxMap.try_emplace("MetalSphereMesh_VB_Position",  0u);
    mesh.vbs.push_back(std::move(vbNormal));     mesh.vbIdxMap.try_emplace("MetalSphereMesh_VB_Normal",    1u);
    mesh.vbs.push_back(std::move(vbTangent));    mesh.vbIdxMap.try_emplace("MetalSphereMesh_VB_Tangent",   2u);
    mesh.vbs.push_back(std::move(vbBitangent));  mesh.vbIdxMap.try_emplace("MetalSphereMesh_VB_Bitangent", 3u);
    mesh.vbs.push_back(std::move(vbUV));         mesh.vbIdxMap.try_emplace("MetalSphereMesh_VB_UV",        4u);
    mesh.ibs.push_back(std::move(ib));

    return mesh;
}

Mesh buildPointMesh( ID3D12Device* device, ID3D12GraphicsCommandList* cmdList, std::unordered_map<std::string, Texture>& texHashMap, DescriptorPool& texPool, Fence& fenceToAssociate )
{
    static const auto positions = std::vector<XMFLOAT3>{
        XMFLOAT3(0.f, 0.f, 0.f)
	};

    static const auto sizes = std::vector<float>{
        1.f
	};

    static const auto indices = std::vector<u16t>{
		0u
	};

	// 정점 버퍼들 구축
	auto vbPosition = createBufferResource( device, nullptr, positions.size() * sizeof( XMFLOAT3 ), BufferCreationType::VertexBuffer );
	setD3DName( vbPosition.Get(), "PointMesh_VB_Position" );
	auto vbPositionu = createBufferResource( device, positions.data(), positions.size() * sizeof( XMFLOAT3 ), BufferCreationType::UploadBuffer );
	setD3DName( vbPositionu.Get(), "PointMesh_VB_Position_Upload" );

    copyResource( cmdList, vbPositionu.Get(), vbPosition.Get(), D3D12_RESOURCE_STATE_GENERIC_READ,
        D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER
    );

	auto vbSize = createBufferResource( device, nullptr, sizes.size() * sizeof( float ), BufferCreationType::VertexBuffer );
	setD3DName( vbSize.Get(), "PointMesh_VB_Size" );
	auto vbSizeu = createBufferResource( device, sizes.data(), sizes.size() * sizeof( float ), BufferCreationType::UploadBuffer );
	setD3DName( vbSizeu.Get(), "PointMesh_VB_Size_Upload" );

	copyResource( cmdList, vbSizeu.Get(), vbSize.Get(), D3D12_RESOURCE_STATE_GENERIC_READ,
        D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER
    );

	// 인덱스 버퍼 구축
	auto ib = createBufferResource( device, nullptr, indices.size() * sizeof( u16t ), BufferCreationType::IndexBuffer );
	setD3DName( ib.Get(), "PointMesh_IB" );
	auto ibu = createBufferResource( device, indices.data(), indices.size() * sizeof( u16t ), BufferCreationType::UploadBuffer );
	setD3DName( ibu.Get(), "PointMesh_IB_Upload" );

	copyResource( cmdList, ibu.Get(), ib.Get(), D3D12_RESOURCE_STATE_GENERIC_READ,
        D3D12_RESOURCE_STATE_INDEX_BUFFER 
    );

	// 만든 버퍼들을 조합하여 메시 구축
	auto mesh = Mesh{};

	// Vertex Buffer View 구성
	mesh.vbViews.emplace_back(
		/* .BufferLocation = */ vbPosition->GetGPUVirtualAddress(),
		/* .SizeInBytes = */ static_cast<UINT>(positions.size() * sizeof( XMFLOAT3 )),
		/* .StrideInBytes = */ static_cast<UINT>(sizeof( XMFLOAT3 ))
	);

	mesh.vbViews.emplace_back(
		/* .BufferLocation = */ vbSize->GetGPUVirtualAddress(),
		/* .SizeInBytes = */ static_cast<UINT>(sizes.size() * sizeof( float )),
		/* .StrideInBytes = */ static_cast<UINT>(sizeof( float ))
	);

    if ( !texHashMap.contains( "PointMesh_Albedo" ) ) {
        Texture::Type type{};
        auto [pPair, _] = texHashMap.try_emplace( "PointMesh_Albedo", loadTexture( device, cmdList, "PointMesh_Albedo.dds", fenceToAssociate, type ) );
        createSRV( device, pPair->second, texPool );
        pPair->second.idxSrv.idxSampler = etoi( Samplers::TrilinearWrap );
    }

	// SubMesh 구성
    // SubMesh 구성
    mesh.subMeshes.emplace_back(
        /* .name = */ "PointMesh_SubMesh",
        /* .ibView = */ D3D12_INDEX_BUFFER_VIEW {
            .BufferLocation = ib->GetGPUVirtualAddress(),
            .SizeInBytes = static_cast<UINT>(indices.size() * sizeof(u16t)),
            .Format = DXGI_FORMAT_R16_UINT
        }
    );

    auto& defMaterialSet = mesh.materialSets.emplace_back(
        /* .name = */ "PointMesh_DefaultMaterialSet",
        /* .materials = */ std::vector<Material>{
            Material{
                .constantAlbedo = XMFLOAT4(0.9f, 0.9f ,0.9f, 1.f),
                .constantRoughness = 0.3f,
                .constantMetallic = 0.15f,
                .constantAOStrength = 0.f,
                .constantEmmisive = XMFLOAT3(0.f, 0.f, 0.f)
            }
        }
    );
    defMaterialSet.materials[0].mapAlbedo.idxSrv.idxRange = -1;
    defMaterialSet.materials[0].mapAlbedo.idxUav.idxRange = -1;
    defMaterialSet.materials[0].mapMetallicSmoothness.idxSrv.idxRange = -1;
    defMaterialSet.materials[0].mapMetallicSmoothness.idxUav.idxRange = -1;
    defMaterialSet.materials[0].mapNormal.idxSrv.idxRange = -1;
    defMaterialSet.materials[0].mapNormal.idxUav.idxRange = -1;
    defMaterialSet.materials[0].mapEmmisive.idxSrv.idxRange = -1;
    defMaterialSet.materials[0].mapEmmisive.idxUav.idxRange = -1;
    defMaterialSet.materials[0].mapAmbientOcclusion.idxSrv.idxRange = -1;
    defMaterialSet.materials[0].mapAmbientOcclusion.idxUav.idxRange = -1;

    defMaterialSet.materials[0].mapAlbedo = cloneTextureIdxOnly( texHashMap.at( "PointMesh_Albedo" ) );

	// 자료구조 등록 (Vertex Buffer View와 SubMesh는 위에서 등록하였음)
	mesh.vbs.push_back( std::move( vbPosition ) );
	mesh.vbIdxMap.try_emplace( "PointMesh_VB_Position", 0u );
	mesh.vbs.push_back( std::move( vbSize ) );
	mesh.vbIdxMap.try_emplace( "PointMesh_VB_Size", 1u );
    mesh.ibs.push_back(std::move(ib));

    gSharedLog << "[Resource Load] PointMesh 구축 완료\n";

	fenceToAssociate.associatedResources_.push_back( std::move( vbPositionu ) );
	fenceToAssociate.associatedResources_.push_back( std::move( vbSizeu ) );
	fenceToAssociate.associatedResources_.push_back( std::move( ibu ) );

	return mesh;
}

// 바이너리 파일의 Bone Socket Type을 나타내는 문자열로부터
// 실제 열거형 값을 얻어낸다.
Bone::SocketType convertStrToBoneSocketType(const std::string& boneSocketTypeStr) {
    if (boneSocketTypeStr == "None") {
        return Bone::SocketType::None;
    }
    else if (boneSocketTypeStr == "RightHand") {
        return Bone::SocketType::RightHand;
    }
    else if (boneSocketTypeStr == "LeftHand") {
        return Bone::SocketType::LeftHand;
    }

    DISPLAY_ERROR_STR( false, "[GFX Error] convertStrToBoneSocketType: 알 수 없는 BoneSocketType 값 \""s
        + boneSocketTypeStr + "\"을 읽었습니다.\n",
        false
    );
    return Bone::SocketType::None;
}

// 바이너리 파일의 Skeleton Enumeration을 나타내는 문자열로부터
// 실제 열거형 값을 얻어낸다.
SkeletonEnumeration convertStrToSkeletonEnum(const std::string& skeletonEnumerationStr) {
    if (skeletonEnumerationStr == "Humanoid") {
        return SkeletonEnumeration::Humanoid;
    }
    else if (skeletonEnumerationStr == "Unique") {
        return SkeletonEnumeration::Unique;
    }

    DISPLAY_ERROR_STR( false, "[GFX Error] convertStrToSkeletonEnum: 알 수 없는 SkeletonEnumeration 값 \""s
        + skeletonEnumerationStr + "\"을 읽었습니다.\n",
        false
    );
    return SkeletonEnumeration::Humanoid;
}

// 텍스처 매핑 정보를 읽어들인다.
// 텍스처 이름을 key로 삼아 texHashMap에 쿼리를 해보고,
// 텍스처가 존재하지 않는다면 알아낸 경로를 통해 텍스처를 로드해 key와 함께 등록한다.
// 로드된 텍스처로 texPool에서 srv를 할당받아 생성하고, 샘플링 정보를 추출한다.
// 그리고 텍스처의 srv와 uav에 해당하는 bindless index에
// 풀에서의 인덱스와 샘플러 인덱스를 채워넣는다.
void importTexture( std::ifstream& ifs, ID3D12Device* device,
    ID3D12GraphicsCommandList* cmdList, std::unordered_map<std::string, Texture>& texHashMap,
    DescriptorPool& texPool, Fence& fenceToAssociate
) {
    std::string key{};
    std::string path{};
    // 기본값들로 초기화
    D3D12_FILTER filterMode = D3D12_FILTER_MIN_MAG_MIP_POINT;
    D3D12_TEXTURE_ADDRESS_MODE addrModeU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    D3D12_TEXTURE_ADDRESS_MODE addrModeV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    D3D12_TEXTURE_ADDRESS_MODE addrModeW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    UINT anisoLevel = 1u;

    for (;;) {
        const auto str = readString(ifs);
        if (isTailTag(str, "Item")) {
            break;
        }

        const auto tag = untagHead(str);

        // 텍스처 이름 추출
        if (tag == "TextureName") {
            key = readString(ifs);
            readTailTag(ifs, "TextureName");
        }
        // Address Mode(Wrap Mode) 추출
        else if (tag == "WrapModeU") {
            const auto wrapModeUStr = readString(ifs);
            if (wrapModeUStr == "Repeat") {
                addrModeU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
            }
            else if (wrapModeUStr == "Clamp") {
                addrModeU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
            }
            else if (wrapModeUStr == "Mirror") {
                addrModeU = D3D12_TEXTURE_ADDRESS_MODE_MIRROR;
            }
            else {
                DISPLAY_ERROR_STR(false, "[GFX Error] importTextureMapping: 알수 없는 텍스처 wrap mode "s
                    + wrapModeUStr + "을 읽었습니다.", false
                );
            }

            readTailTag(ifs, "WrapModeU");
        }
        else if (tag == "WrapModeV") {
            const auto wrapModeVStr = readString(ifs);
            if (wrapModeVStr == "Repeat") {
                addrModeV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
            }
            else if (wrapModeVStr == "Clamp") {
                addrModeV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
            }
            else if (wrapModeVStr == "Mirror") {
                addrModeV = D3D12_TEXTURE_ADDRESS_MODE_MIRROR;
            }
            else {
                DISPLAY_ERROR_STR(false, "[GFX Error] importTextureMapping: 알수 없는 텍스처 wrap mode "s
                    + wrapModeVStr + "을 읽었습니다.", false
                );
            }

            readTailTag(ifs, "WrapModeV");
        }
        else if (tag == "WrapModeW") {
            const auto wrapModeWStr = readString(ifs);
            if (wrapModeWStr == "Repeat") {
                addrModeW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
            }
            else if (wrapModeWStr == "Clamp") {
                addrModeW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
            }
            else if (wrapModeWStr == "Mirror") {
                addrModeW = D3D12_TEXTURE_ADDRESS_MODE_MIRROR;
            }
            else {
                DISPLAY_ERROR_STR(false, "[GFX Error] importTextureMapping: 알수 없는 텍스처 wrap mode "s
                    + wrapModeWStr + "을 읽었습니다.", false
                );
            }

            readTailTag(ifs, "WrapModeW");
        }
        // Filter Mode 추출
        else if (tag == "FilterMode") {
            const auto filterModeStr = readString(ifs);
            if (filterModeStr == "Point") {
                filterMode = D3D12_FILTER_MIN_MAG_MIP_POINT;
            }
            else if (filterModeStr == "Bilinear") {
                filterMode = D3D12_FILTER_MIN_MAG_LINEAR_MIP_POINT;
            }
            else if (filterModeStr == "Trilinear") {
                filterMode = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
            }
            else if (filterModeStr == "Anisotropic") {
                filterMode = D3D12_FILTER_ANISOTROPIC;
            }
            else {
                DISPLAY_ERROR_STR(false, "[GFX Error] importTextureMapping: 알수 없는 텍스처 filter mode "s
                    + filterModeStr + "을 읽었습니다.", false
                );
            }

            readTailTag(ifs, "FilterMode");
        }
        // 비등방성 필터링 레벨(최대 레벨) 추출
        else if (tag == "AnisoLevel") {
            anisoLevel = static_cast<UINT>(readInteger(ifs));
            readTailTag(ifs, "AnisoLevel");
        }
        // 경로 추출
        else if (tag == "Path") {
            path = readString(ifs);
            readTailTag(ifs, "Path");
        }
        else {
            DISPLAY_ERROR_STR(false, "[File I/O Error] importTextureMapping: 알 수 없는 태그 "s
                + tag + "를 읽었습니다.", true
            );
        }
    }

    // texHashMap에 없다면 알아낸 경로를 통해 텍스처를 load해 key와 함께 등록
    if (!texHashMap.contains(key)) {
        Texture::Type type{};
        auto [pPair, _] = texHashMap.try_emplace( key, loadTexture(device, cmdList, path, fenceToAssociate, type) );

        // 풀에서 srv를 할당받아 생성
        createSRV(device, pPair->second, texPool);
        
        // 샘플러 인덱스를 계산해서 채워넣기
        const auto samplerIdx = calcIdxBindlessSampler(filterMode, addrModeU, addrModeV, addrModeW, anisoLevel);
        pPair->second.idxSrv.idxSampler = samplerIdx;
        pPair->second.idxUav.idxSampler = samplerIdx;
    }
}

// 텍스처 매핑 정보를 읽어들인다.
// 텍스처 이름을 key로 삼아 texHashMap에 쿼리를 해보고,
// 텍스처가 존재하지 않는다면 알아낸 경로를 통해 텍스처를 로드해 key와 함께 등록한다.
// 로드된 텍스처로 texPool에서 srv를 할당받아 생성하고, 샘플링 정보를 추출한다.
// 그리고 텍스처의 srv와 uav에 해당하는 bindless index에
// 풀에서의 인덱스와 샘플러 인덱스를 채워넣는다.
void importTextureMapping( std::ifstream& ifs, ID3D12Device* device,
    ID3D12GraphicsCommandList* cmdList, std::unordered_map<std::string, Texture>& texHashMap,
    DescriptorPool& texPool, Fence& fenceToAssociate
) {
    readHeadTag(ifs, "TextureMapping");
    for (;;) {
        const auto str = readString(ifs);
        if (isTailTag(str, "TextureMapping")) {
            break;
        }

        const auto tag = untagHead(str);

        // 텍스처 이름, 텍스처 샘플링 정보, 텍스처 경로 읽기
        if (tag == "Item") {
            importTexture(ifs, device, cmdList, texHashMap, texPool, fenceToAssociate);
        }
        else {
            DISPLAY_ERROR_STR(false, "[File I/O Error] importTextureMapping: 알 수 없는 태그 "s
                + tag + "를 읽었습니다.", true
            );
        }
    }
}

// 메시 하나를 읽어들인다.
// 메시의 정점 버퍼들을 구축하며,
// 서브메시 정보를 읽어들여 서브메시 각각의 인덱스 버퍼를 구축한다.
void importMesh( std::ifstream& ifs, ID3D12Device* device,
    ID3D12GraphicsCommandList* cmdList, const std::string& name,
    Fence& fenceToAssociate, Mesh& mesh
) {
    // AABB 구축
    readHeadTag(ifs, "Bounds");
    const auto center = readVec3(ifs, "Center");
    const auto extents = readVec3(ifs, "Extents");
    const auto mins = readVec3(ifs, "Min");
    const auto maxs = readVec3(ifs, "Max");
    readTailTag(ifs, "Bounds");

    mesh.bounds = AABB{
        .center = XMLoadFloat3(&center),
        .size = mu::Vec3(XMLoadFloat3(&extents)) * 2.f
    };

    // 정점 버퍼들 구축
    readHeadTag(ifs, "VertexBuffers");

    // Keep the position/skinning vertex arrays alive until the submesh section so the
    // energy-orb death FX can cache each submesh's first vertex (SubMesh::firstVertex*).
    std::vector<XMFLOAT3> positionsCache;
    std::vector<XMINT4>   boneIndicesCache;
    std::vector<XMFLOAT4> boneWeightsCache;

    for (;;) {
        const auto str = readString(ifs);
        if (isTailTag(str, "VertexBuffers")) {
            break;
        }

        const auto tag = untagHead(str);

        // Positions: float3
        if (tag == "Positions") {
            positionsCache = readVec3s(ifs);
            const auto& positions = positionsCache;

            auto vbPosition = createBufferResource(device, nullptr, positions.size() * sizeof(XMFLOAT3), BufferCreationType::VertexBuffer);
            setD3DName(vbPosition.Get(), name + "_VB_Position"s);
	        auto vbPositionu = createBufferResource(device, positions.data(), positions.size() * sizeof(XMFLOAT3), BufferCreationType::UploadBuffer);
            setD3DName(vbPositionu.Get(), name + "_VB_Position_Upload"s);

	        copyResource( cmdList, vbPositionu.Get(), vbPosition.Get(), D3D12_RESOURCE_STATE_GENERIC_READ,
		        D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER
	        );

            mesh.vbViews.emplace_back(
                /* .BufferLocation = */ vbPosition->GetGPUVirtualAddress(),
                /* .SizeInBytes = */ static_cast<UINT>(positions.size() * sizeof(XMFLOAT3)),
                /* .StrideInBytes = */ static_cast<UINT>( sizeof(XMFLOAT3) )
            );
            mesh.vbIdxMap.try_emplace(name + "_VB_Position"s, static_cast<u32t>(mesh.vbs.size()));

            mesh.vbs.push_back(std::move(vbPosition));
            fenceToAssociate.associatedResources_.push_back(std::move(vbPositionu));

            readTailTag(ifs, "Positions");
        }
        // Colors: float4
        else if (tag == "Colors") {
            auto colors = readVec4s(ifs);
            readTailTag(ifs, "Colors");
        }
        // Normals: float3
        else if (tag == "Normals") {
            auto normals = readVec3s(ifs);

            auto vbNormal = createBufferResource(device, nullptr, normals.size() * sizeof(XMFLOAT3), BufferCreationType::VertexBuffer);
            setD3DName(vbNormal.Get(), name + "_VB_Normal"s);
	        auto vbNormalu = createBufferResource(device, normals.data(), normals.size() * sizeof(XMFLOAT3), BufferCreationType::UploadBuffer);
            setD3DName(vbNormalu.Get(), name + "_VB_Normal_Upload"s);

	        copyResource( cmdList, vbNormalu.Get(), vbNormal.Get(), D3D12_RESOURCE_STATE_GENERIC_READ,
		        D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER
	        );

            mesh.vbViews.emplace_back(
                /* .BufferLocation = */ vbNormal->GetGPUVirtualAddress(),
                /* .SizeInBytes = */ static_cast<UINT>(normals.size() * sizeof(XMFLOAT3)),
                /* .StrideInBytes = */ static_cast<UINT>( sizeof(XMFLOAT3) )
            );
            mesh.vbIdxMap.try_emplace(name + "_VB_Normal"s, static_cast<u32t>(mesh.vbs.size()));

            mesh.vbs.push_back(std::move(vbNormal));
            fenceToAssociate.associatedResources_.push_back(std::move(vbNormalu));

            readTailTag(ifs, "Normals");
        }
        // Tangents: float3
        else if (tag == "Tangents") {
            auto tangents = readVec3s(ifs);

            auto vbTangent = createBufferResource(device, nullptr, tangents.size() * sizeof(XMFLOAT3), BufferCreationType::VertexBuffer);
            setD3DName(vbTangent.Get(), name + "_VB_Tangent"s);
	        auto vbTangentu = createBufferResource(device, tangents.data(), tangents.size() * sizeof(XMFLOAT3), BufferCreationType::UploadBuffer);
            setD3DName(vbTangentu.Get(), name + "_VB_Tangent_Upload"s);

	        copyResource( cmdList, vbTangentu.Get(), vbTangent.Get(), D3D12_RESOURCE_STATE_GENERIC_READ,
		        D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER
	        );

            mesh.vbViews.emplace_back(
                /* .BufferLocation = */ vbTangent->GetGPUVirtualAddress(),
                /* .SizeInBytes = */ static_cast<UINT>(tangents.size() * sizeof(XMFLOAT3)),
                /* .StrideInBytes = */ static_cast<UINT>( sizeof(XMFLOAT3) )
            );
            mesh.vbIdxMap.try_emplace(name + "_VB_Tangent"s, static_cast<u32t>(mesh.vbs.size()));

            mesh.vbs.push_back(std::move(vbTangent));
            fenceToAssociate.associatedResources_.push_back(std::move(vbTangentu));

            readTailTag(ifs, "Tangents");
        }
        // Bitangents: float3
        else if (tag == "Bitangents") {
            auto bitangents = readVec3s(ifs);

            auto vbBitangent = createBufferResource(device, nullptr, bitangents.size() * sizeof(XMFLOAT3), BufferCreationType::VertexBuffer);
            setD3DName(vbBitangent.Get(), name + "_VB_Bitangent"s);
	        auto vbBitangentu = createBufferResource(device, bitangents.data(), bitangents.size() * sizeof(XMFLOAT3), BufferCreationType::UploadBuffer);
            setD3DName(vbBitangentu.Get(), name + "_VB_Bitangent_Upload"s);

	        copyResource( cmdList, vbBitangentu.Get(), vbBitangent.Get(), D3D12_RESOURCE_STATE_GENERIC_READ,
		        D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER
	        );

            mesh.vbViews.emplace_back(
                /* .BufferLocation = */ vbBitangent->GetGPUVirtualAddress(),
                /* .SizeInBytes = */ static_cast<UINT>(bitangents.size() * sizeof(XMFLOAT3)),
                /* .StrideInBytes = */ static_cast<UINT>( sizeof(XMFLOAT3) )
            );
            mesh.vbIdxMap.try_emplace(name + "_VB_Bitangent"s, static_cast<u32t>(mesh.vbs.size()));

            mesh.vbs.push_back(std::move(vbBitangent));
            fenceToAssociate.associatedResources_.push_back(std::move(vbBitangentu));

            readTailTag(ifs, "Bitangents");
        }
        // uv0s: float2
        else if (tag == "TextureCoords0") {
            auto uvs = readVec2s(ifs);

            auto vbUV = createBufferResource(device, nullptr, uvs.size() * sizeof(XMFLOAT2), BufferCreationType::VertexBuffer);
            setD3DName(vbUV.Get(), name + "_VB_UV"s);
	        auto vbUVu = createBufferResource(device, uvs.data(), uvs.size() * sizeof(XMFLOAT2), BufferCreationType::UploadBuffer);
            setD3DName(vbUVu.Get(), name + "_VB_UV_Upload"s);

	        copyResource( cmdList, vbUVu.Get(), vbUV.Get(), D3D12_RESOURCE_STATE_GENERIC_READ,
		        D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER
	        );

            mesh.vbViews.emplace_back(
                /* .BufferLocation = */ vbUV->GetGPUVirtualAddress(),
                /* .SizeInBytes = */ static_cast<UINT>(uvs.size() * sizeof(XMFLOAT2)),
                /* .StrideInBytes = */ static_cast<UINT>( sizeof(XMFLOAT2) )
            );
            mesh.vbIdxMap.try_emplace(name + "_VB_UV"s, static_cast<u32t>(mesh.vbs.size()));

            mesh.vbs.push_back(std::move(vbUV));
            fenceToAssociate.associatedResources_.push_back(std::move(vbUVu));

            readTailTag(ifs, "TextureCoords0");
        }
        // uv1s: float2
        else if (tag == "TextureCoords1") {
            auto uvs = readVec2s(ifs);
            readTailTag(ifs, "TextureCoords1");
        }
        // boneIndices: uint4
        else if (tag == "BoneIndices") {
            boneIndicesCache = readInt4s(ifs);
            const auto& boneIndices = boneIndicesCache;

            auto vbBoneIndices = createBufferResource(device, nullptr, boneIndices.size() * sizeof(XMUINT4), BufferCreationType::VertexBuffer);
            setD3DName(vbBoneIndices.Get(), name + "_VB_BoneIndices"s);
	        auto vbBoneIndicesu = createBufferResource(device, boneIndices.data(), boneIndices.size() * sizeof(XMUINT4), BufferCreationType::UploadBuffer);
            setD3DName(vbBoneIndicesu.Get(), name + "_VB_BoneIndices_Upload"s);

	        copyResource( cmdList, vbBoneIndicesu.Get(), vbBoneIndices.Get(), D3D12_RESOURCE_STATE_GENERIC_READ,
		        D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER
	        );

            mesh.vbViews.emplace_back(
                /* .BufferLocation = */ vbBoneIndices->GetGPUVirtualAddress(),
                /* .SizeInBytes = */ static_cast<UINT>(boneIndices.size() * sizeof(XMUINT4)),
                /* .StrideInBytes = */ static_cast<UINT>( sizeof(XMUINT4) )
            );
            mesh.vbIdxMap.try_emplace(name + "_VB_BoneIndices"s, static_cast<u32t>(mesh.vbs.size()));

            mesh.vbs.push_back(std::move(vbBoneIndices));
            fenceToAssociate.associatedResources_.push_back(std::move(vbBoneIndicesu));

            readTailTag(ifs, "BoneIndices");
        }
        // boneWeights: float4
        else if (tag == "BoneWeights") {
            boneWeightsCache = readVec4s(ifs);
            const auto& boneWeights = boneWeightsCache;

            auto vbBoneWeights = createBufferResource(device, nullptr, boneWeights.size() * sizeof(XMFLOAT4), BufferCreationType::VertexBuffer);
            setD3DName(vbBoneWeights.Get(), name + "_VB_BoneWeights"s);
	        auto vbBoneWeightsu = createBufferResource(device, boneWeights.data(), boneWeights.size() * sizeof(XMFLOAT4), BufferCreationType::UploadBuffer);
            setD3DName(vbBoneWeightsu.Get(), name + "_VB_BoneWeights_Upload"s);

	        copyResource( cmdList, vbBoneWeightsu.Get(), vbBoneWeights.Get(), D3D12_RESOURCE_STATE_GENERIC_READ,
		        D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER
	        );

            mesh.vbViews.emplace_back(
                /* .BufferLocation = */ vbBoneWeights->GetGPUVirtualAddress(),
                /* .SizeInBytes = */ static_cast<UINT>(boneWeights.size() * sizeof(XMFLOAT4)),
                /* .StrideInBytes = */ static_cast<UINT>( sizeof(XMFLOAT4) )
            );
            mesh.vbIdxMap.try_emplace(name + "_VB_BoneWeights"s, static_cast<u32t>(mesh.vbs.size()));

            mesh.vbs.push_back(std::move(vbBoneWeights));
            fenceToAssociate.associatedResources_.push_back(std::move(vbBoneWeightsu));

            readTailTag(ifs, "BoneWeights");
        }
        else {
            DISPLAY_ERROR_STR(false, "[File I/O Error] importMesh: 알 수 없는 태그 "s
                + tag + "를 읽었습니다.", true
            );
        }
    }

    // Cache each submesh's first vertex (energy-orb sphere centering) from its first index.
    auto cacheSubmeshFirstVertex = [&](SubMesh& sm, u32t firstIdx, bool valid) {
        if (!valid) return;
        if (firstIdx < positionsCache.size())   { sm.firstVertexPos = positionsCache[firstIdx]; sm.hasFirstVertex = true; }
        if (firstIdx < boneIndicesCache.size())  sm.firstVertexBones   = boneIndicesCache[firstIdx];
        if (firstIdx < boneWeightsCache.size())  sm.firstVertexWeights = boneWeightsCache[firstIdx];
    };

    // 서브메시들 구축
    readHeadTag(ifs, "Submeshes");
    const auto submeshCnt = readInteger(ifs, "SubmeshCnt");
    const auto maxIdx = readInteger(ifs, "MaxIndex");

    // 최고 인덱스가 65536 미만이라면 uint16_t로 인덱스를 표현할 수 있다.
    // 그렇게 표현하면 인덱스 버퍼에 쓰이는 gpu 메모리 크기를 절반으로 절약할 수 있다.
    if (maxIdx < 65536) {
        for (int i = 0; i < submeshCnt; ++i) {
            auto indices = readU16s(ifs, "Submesh");

            auto ib = createBufferResource(device, nullptr, indices.size() * sizeof(u16t), BufferCreationType::IndexBuffer);
            setD3DName(ib.Get(), name + "_IB"s);
	        auto ibu = createBufferResource(device, indices.data(), indices.size() * sizeof(u16t), BufferCreationType::UploadBuffer);
            setD3DName(ibu.Get(), name + "_IB_Upload"s);

	        copyResource( cmdList, ibu.Get(), ib.Get(), D3D12_RESOURCE_STATE_GENERIC_READ,
		        D3D12_RESOURCE_STATE_INDEX_BUFFER
	        );

            const auto subMeshName = name + "_SubMesh"s + std::to_string(i);

            mesh.subMeshes.emplace_back(
                /* .name = */ subMeshName,
                /* .ibView = */ D3D12_INDEX_BUFFER_VIEW {
                    .BufferLocation = ib->GetGPUVirtualAddress(),
                    .SizeInBytes = static_cast<UINT>(indices.size() * sizeof(u16t)),
                    .Format = DXGI_FORMAT_R16_UINT
                }
            );
            cacheSubmeshFirstVertex(mesh.subMeshes.back(),
                indices.empty() ? 0u : static_cast<u32t>(indices[0]), !indices.empty());

            mesh.ibs.push_back(std::move(ib));
            fenceToAssociate.associatedResources_.push_back(std::move(ibu));
        }
    }
    // 최고 인덱스가 65536 이상인 경우
    else {
        for (int i = 0; i < submeshCnt; ++i) {
            auto indices = readIntegers(ifs, "Submesh");

            auto ib = createBufferResource(device, nullptr, indices.size() * sizeof(i32t), BufferCreationType::IndexBuffer);
            setD3DName(ib.Get(), name + "_IB"s);
	        auto ibu = createBufferResource(device, indices.data(), indices.size() * sizeof(i32t), BufferCreationType::UploadBuffer);
            setD3DName(ibu.Get(), name + "_IB_Upload"s);

	        copyResource( cmdList, ibu.Get(), ib.Get(), D3D12_RESOURCE_STATE_GENERIC_READ,
		        D3D12_RESOURCE_STATE_INDEX_BUFFER
	        );

            const auto subMeshName = name + "_SubMesh"s + std::to_string(i);

            mesh.subMeshes.emplace_back(
                /* .name = */ subMeshName,
                /* .ibView = */ D3D12_INDEX_BUFFER_VIEW {
                    .BufferLocation = ib->GetGPUVirtualAddress(),
                    .SizeInBytes = static_cast<UINT>(indices.size() * sizeof(i32t)),
                    .Format = DXGI_FORMAT_R16_UINT
                }
            );
            cacheSubmeshFirstVertex(mesh.subMeshes.back(),
                indices.empty() ? 0u : static_cast<u32t>(indices[0]), !indices.empty());

            mesh.ibs.push_back(std::move(ib));
            fenceToAssociate.associatedResources_.push_back(std::move(ibu));
        }
    }
    readTailTag(ifs, "Submeshes");

    readTailTag(ifs, "Mesh");
}

// 재질 집합의 내용을 읽어들인다.
// 텍스처 매핑 정보를 통해 미리 로드했던 텍스처들이
// 각 재질에 연결된다.
// 
// 한 재질 집합의 재질의 개수는 메시의 서브메시 개수와 같고,
// 각 재질은 동일한 인덱스의 서브메시에 대응된다.
// * 커스터마이징 프리셋 개념이라 이해하면 편하다.
void importMaterials( std::ifstream& ifs, ID3D12Device* device,
    ID3D12GraphicsCommandList* cmdList, 
    std::unordered_map<std::string, Texture>& texHashMap,
    Fence& fenceToAssociate, MaterialSet& materialSet
) {
    auto materialCnt = readInteger(ifs, "MaterialCnt");
    materialSet.materials.resize(materialCnt);
    // Bindless Index 초기화
    // idxRange를 -1로 써주는 것으로 그 텍스처가 존재하지 않음을 표현한다.
    for (auto& material : materialSet.materials) {
        material.mapAlbedo.idxSrv.idxRange = -1;
        material.mapAlbedo.idxUav.idxRange = -1;
        material.mapMetallicSmoothness.idxSrv.idxRange = -1;
        material.mapMetallicSmoothness.idxUav.idxRange = -1;
        material.mapNormal.idxSrv.idxRange = -1;
        material.mapNormal.idxUav.idxRange = -1;
        material.mapEmmisive.idxSrv.idxRange = -1;
        material.mapEmmisive.idxUav.idxRange = -1;
        material.mapAmbientOcclusion.idxSrv.idxRange = -1;
        material.mapAmbientOcclusion.idxUav.idxRange = -1;
    }

    for (int i = 0; i < materialCnt; ++i) {
        readHeadTag(ifs, "Material");
        auto& material = materialSet.materials[i];

        for (;;) {
            const auto str = readString(ifs);
            if (isTailTag(str, "Material")) {
                break;
            }

            const auto tag = untagHead(str);
            // 상수 읽어들이기 ======================
            // 알베도 색상 상수
            if (tag == "cAlbedo") {
                const auto albedo = readColor(ifs);
                material.constantAlbedo = albedo;
                readTailTag(ifs, "cAlbedo");
            }
            // 자체발광 색상 상수
            else if (tag == "cEmmisive") {
                const auto emmisive = readColor(ifs);
                material.constantEmmisive = XMFLOAT3(emmisive.x, emmisive.y, emmisive.z);
                readTailTag(ifs, "cEmmisive");
            }
            // 매끄러움 상수 (거칠기 상수의 역)
            else if (tag == "cSmoothness") {
                const auto smoothness = readFloat(ifs);
                material.constantRoughness = 1.f - smoothness;
                readTailTag(ifs, "cSmoothness");
            }
            // 금속성 상수
            else if (tag == "cMetallic") {
                const auto metallic = readFloat(ifs);
                material.constantRoughness = 1.f - metallic;
                readTailTag(ifs, "cMetallic");
            }
            // 주변광 차폐 적용 강도 상수
            else if (tag == "cAOStrength") {
                const auto aoStrength = readFloat(ifs);
                material.constantAOStrength = aoStrength;
                readTailTag(ifs, "cAOStrength");
            }
            // ======================================
            // 텍스처 읽어들이기 ====================
            // 텍스처 매핑 정보를 통해 미리 로드했던 텍스처들을
            // texHashMap에서 찾아 재질에 연결한다.
            
            // 알베도 텍스처
            else if (tag == "AlbedoMap") {
                const auto albedoMapKey = readString(ifs);
                material.mapAlbedo = cloneTextureIdxOnly(texHashMap.at(albedoMapKey));
                readTailTag(ifs, "AlbedoMap");
            }
            // 노멀 텍스처
            else if (tag == "NormalMap") {
                const auto normalMapKey = readString(ifs);
                material.mapNormal = cloneTextureIdxOnly(texHashMap.at(normalMapKey));
                readTailTag(ifs, "NormalMap");
            }
            // 금속성과 매끄러움 텍스처
            else if (tag == "MetallicSmoothnessMap") {
                const auto metallicSmoothnessMapKey = readString(ifs);
                material.mapMetallicSmoothness
                    = cloneTextureIdxOnly(texHashMap.at(metallicSmoothnessMapKey));
                readTailTag(ifs, "MetallicSmoothnessMap");
            }
            // 자체발광 텍스처
            else if (tag == "EmmisiveMap") {
                const auto emmisiveMapKey = readString(ifs);
                material.mapEmmisive = cloneTextureIdxOnly(texHashMap.at(emmisiveMapKey));
                readTailTag(ifs, "EmmisiveMap");
            }
            // 차폐도 텍스처
            else if (tag == "AOMap") {
                const auto aoMapKey = readString(ifs);
                material.mapAmbientOcclusion
                    = cloneTextureIdxOnly(texHashMap.at(aoMapKey));
                readTailTag(ifs, "AOMap");
            }
            // ======================================
            else {
                DISPLAY_ERROR_STR(false, "[File I/O Error] importMaterials: 알 수 없는 태그 "s
                    + tag + "를 읽었습니다.", true
                );
            }
        }

    }

    readTailTag(ifs, "Materials");
}

// 재질 집합 정보를 읽어서 메시에 반영한다.
// 여러 개의 재질 집합을 사용하는 메시는
// 메시를 그릴 때에 어떤 재질 집합을 사용할지 선택해서 그리도록 한다.
// 
// 한 재질 집합의 재질의 개수는 메시의 서브메시 개수와 같고,
// 각 재질은 동일한 인덱스의 서브메시에 대응된다.
// * 커스터마이징 프리셋 개념이라 이해하면 편하다.
void importMaterialSets( std::ifstream& ifs, ID3D12Device* device,
    ID3D12GraphicsCommandList* cmdList, 
    std::unordered_map<std::string, Texture>& texHashMap,
    Fence& fenceToAssociate, Mesh& mesh
) {
    const auto materialSetCnt = readInteger(ifs, "MaterialSetCnt");

    for (int i = 0; i < materialSetCnt; ++i) {
        readHeadTag(ifs, "MaterialSet");
        const auto materialSetName = readText(ifs, "Name");

        readHeadTag(ifs, "Materials");
        auto& newMaterialSet = mesh.materialSets.emplace_back(
            /* .name = */ materialSetName,
            /* .materials = */ std::vector<Material>(mesh.subMeshes.size())
        );
        newMaterialSet.materials.reserve(mesh.subMeshes.size());
        importMaterials(ifs, device, cmdList, texHashMap, fenceToAssociate, newMaterialSet);

        readTailTag(ifs, "MaterialSet");
    }

    readTailTag(ifs, "MaterialSets");
}

// 기하 계층구조의 특정 노드를 읽어들이고
// 그 자식 노드들을 재귀적으로 읽어들인다.
// 노드에는 변환 행렬, 메시와 재질 정보가 담겨 있다.
// 노드와 메시, 변환 행렬은 각각 일대일 대응이다.
// 메시는 여러 개의 재질 집합을 가질 수 있는데,
// 하나의 재질 집합 내의 재질들은 모두 메시의 각 서브메시와 일대일 대응된다.
// 재질 집합을 교체하는 것으로 전체 메시의 질감을 바꿀 수 있다.
void importTransform( std::ifstream& ifs, ID3D12Device* device,
    ID3D12GraphicsCommandList* cmdList, 
    std::unordered_map<std::string, Texture>& texHashMap,
    Fence& fenceToAssociate, Model& model
) {
    readHeadTag(ifs, "Node");
    // 노드의 이름
    const auto name = readString(ifs);

    // 노드의 변환 행렬들
    const auto localMat = readMatrix(ifs, "LocalMatrix");
    const auto dressMat = readMatrix(ifs, "DressMatrix");

    auto& pair = model.meshWithDressXforms.emplace_back(
        /* .mesh = */ Mesh{ .name = name }, // 모델 노드와 메시는 이름을 공유한다.
        /* .dressXform = */ mu::Mat4x4(XMLoadFloat4x4(&dressMat))
    );

    for (;;) {
        const auto str = readString(ifs);
        if (isTailTag(str, "Node")) {
            break;
        }

        const auto tag = untagHead(str);

        // 메시 읽어들이기
        if (tag == "Mesh") {
            importMesh(ifs, device, cmdList, name, fenceToAssociate, pair.mesh);
        }
        // 여러 개의 재질 집합을 지원하는 경우
        else if (tag == "MaterialSets") {
            importMaterialSets(ifs, device, cmdList, texHashMap, fenceToAssociate, pair.mesh);
        }
        // 여러 개의 재질 집합을 지원하지 않는 경우
        // 기본 재질 집합을 구축해 사용 (0번 인덱스)
        else if (tag == "Materials") {
            auto& defMaterialSet = pair.mesh.materialSets.emplace_back(
                /* .name = */ pair.mesh.name + "_MaterialSet_Default",
                /* .materials = */ std::vector<Material>()
            );
            defMaterialSet.materials.reserve(pair.mesh.subMeshes.size());
            importMaterials(ifs, device, cmdList, texHashMap, fenceToAssociate, defMaterialSet);
        }
        // 자식 노드 읽어들이기
        else if (tag == "ChildCnt") {
            const auto childCnt = readInteger(ifs);
            readTailTag(ifs, "ChildCnt");
            readHeadTag(ifs, "Children");
            for (int i = 0; i < childCnt; ++i) {
                importTransform(ifs, device, cmdList, texHashMap, fenceToAssociate, model);
            }
            readTailTag(ifs, "Children");
        }
        else {
            DISPLAY_ERROR_STR(false, "[File I/O Error] importTransform: 알 수 없는 태그 "s
                + tag + "를 읽었습니다.", true
            );
        }
    }

    gSharedLog << "[Resource Load] 모델 노드 " << name << " 구축 완료\n";
}

// 기하구조를 읽어들인다.
// 기하구조의 루트노드에 대한 importTransform 호출을 수행한다.
void importGeometry( std::ifstream& ifs, ID3D12Device* device,
    ID3D12GraphicsCommandList* cmdList,
    std::unordered_map<std::string, Texture>& texHashMap,
    Fence& fenceToAssociate, Model& model
) {
    readHeadTag(ifs, "Geometry");
    const auto nodeCnt = readInteger(ifs, "NodeCnt");
    const auto modelScale = readVec3(ifs, "ModelScale");
    model.baseScale = DirectX::XMLoadFloat3(&modelScale);
    importTransform(ifs, device, cmdList, texHashMap, fenceToAssociate, model);
    readTailTag(ifs, "Geometry");
}

// --- BVH build helpers ---

struct ImportedBVBox {
    std::string name;
    std::string boneName;
    DirectX::XMFLOAT3 center;
    DirectX::XMFLOAT3 size;      // full extents (width, height, depth)
    DirectX::XMFLOAT3 rotEuler;  // degrees
    bool              isStatic = false;  // Unity GameObject static flag
};

static bool isZeroEuler(const DirectX::XMFLOAT3& e) {
    return std::abs(e.x) < 1e-4f && std::abs(e.y) < 1e-4f && std::abs(e.z) < 1e-4f;
}

static mu::NQuat eulerDegsToQuat(const DirectX::XMFLOAT3& e) {
    constexpr float toRad = DirectX::XM_PI / 180.f;
    return mu::NQuat(mu::Degree(e.z), mu::Degree(e.x), mu::Degree(e.y));
}

static AABB computeNodeBounds(const std::variant<AABB, OBB>& shape) {
    return std::visit([](auto&& s) -> AABB {
        using T = std::decay_t<decltype(s)>;
        if constexpr (std::is_same_v<T, OBB>) return obbToAABB(s);
        else                                   return s;
    }, shape);
}

static AABB unionAABB(const AABB& a, const AABB& b) {
    const mu::Vec3 minA = a.center - a.size * 0.5f;
    const mu::Vec3 maxA = a.center + a.size * 0.5f;
    const mu::Vec3 minB = b.center - b.size * 0.5f;
    const mu::Vec3 maxB = b.center + b.size * 0.5f;
    const mu::Vec3 mn = mu::min(minA, minB);
    const mu::Vec3 mx = mu::max(maxA, maxB);
    return AABB{ (mn + mx) * 0.5f, mx - mn };
}

static BVHNode makeBVHNode(const ImportedBVBox& box) {
    BVHNode node;
    node.name     = box.name;
    node.boneName = box.boneName;

    const mu::Vec3 center = DirectX::XMLoadFloat3(&box.center);
    const mu::Vec3 size   = DirectX::XMLoadFloat3(&box.size);

    // static이면서 AABB로 표현 가능(로컬 회전이 0)한 박스만 AABB로 로드한다.
    // 그 외(애니메이션 본에 붙은 박스, 회전이 있는 박스, 동적 오브젝트)는 OBB로 로드해
    // 월드 회전이 충돌 판정에 정확히 반영되도록 한다.
    if (box.isStatic && isZeroEuler(box.rotEuler)) {
        node.shape = AABB{ center, size };
    } else {
        node.shape = OBB{ center, size * 0.5f, eulerDegsToQuat(box.rotEuler) };
    }
    node.bounds = computeNodeBounds(node.shape);
    return node;
}

// Builds a BVH from LOD-based box arrays.
// LOD 0 = root level (1 coarse box expected), LOD 1 = sub-BVs, LOD 2 = finer sub-BVs.
// Each LOD N+1 box is assigned to the nearest center LOD N parent.
static BVH buildBVHFromLODs(const std::vector<std::vector<ImportedBVBox>>& allLODs) {
    BVH bvh;
    if (allLODs.empty() || allLODs[0].empty()) return bvh;

    if (allLODs.size() == 1) {
        for (const auto& box : allLODs[0])
            bvh.nodes.push_back(makeBVHNode(box));
        return bvh;
    }

    // Build LOD 0 root nodes first (indices 0 .. n0-1)
    for (const auto& box : allLODs[0])
        bvh.nodes.push_back(makeBVHNode(box));

    int parentStart = 0;
    int parentEnd   = (int)bvh.nodes.size();

    for (int lod = 1; lod < (int)allLODs.size(); ++lod) {
        const int childStart = (int)bvh.nodes.size();

        for (const auto& box : allLODs[lod]) {
            BVHNode child = makeBVHNode(box);

            const mu::Vec3 childCenter = std::visit(
                [](auto&& s) { return s.center; }, child.shape);

            int bestParent = parentStart;
            float bestDist = std::numeric_limits<float>::max();
            for (int p = parentStart; p < parentEnd; ++p) {
                const mu::Vec3 pCenter = std::visit(
                    [](auto&& s) { return s.center; }, bvh.nodes[p].shape);
                const float d = (childCenter - pCenter).len2();
                if (d < bestDist) { bestDist = d; bestParent = p; }
            }

            bvh.nodes[bestParent].children.push_back((int)bvh.nodes.size());
            bvh.nodes.push_back(std::move(child));
        }

        // Expand parent bounds to encompass new children
        for (int p = parentStart; p < parentEnd; ++p) {
            for (int c : bvh.nodes[p].children)
                bvh.nodes[p].bounds = unionAABB(bvh.nodes[p].bounds, bvh.nodes[c].bounds);
        }

        parentStart = childStart;
        parentEnd   = (int)bvh.nodes.size();
    }

    return bvh;
}

// Resolves BVHNode::boneName -> BVHNode::boneIdx for all nodes in bvh.
// Must be called after the skeleton is loaded (importSkeleton).
static void resolveBVHBoneIndices(BVH& bvh, const Skeleton& skeleton) {
    if (!skeleton.bones) return;
    const auto& bones = *skeleton.bones;
    for (auto& node : bvh.nodes) {
        if (node.boneName.empty()) continue;
        for (const auto& bone : bones) {
            if (bone.name == node.boneName) {
                node.boneIdx = bone.boneIdx;
                break;
            }
        }
    }
}

// Reads bounding volume data in the Unity ModelExtractor LOD format and
// builds the model's BVH. Format: BoundingVolumes > LODCount, LOD > Box.
void importBoundingVolumes(std::ifstream& ifs, Model& model) {
    readHeadTag(ifs, "BoundingVolumes");

    const int lodCount = readInteger(ifs, "LODCount");
    if (lodCount == 0) {
        readTailTag(ifs, "BoundingVolumes");
        return;
    }

    std::vector<std::vector<ImportedBVBox>> allLODs(lodCount);

    for (int lod = 0; lod < lodCount; ++lod) {
        readHeadTag(ifs, "LOD");
        const int lodIdx   = readInteger(ifs, "Index");
        const int boxCount = readInteger(ifs, "BoxCount");

        const int target = (lodIdx >= 0 && lodIdx < lodCount) ? lodIdx : lod;
        allLODs[target].reserve(boxCount);

        for (int b = 0; b < boxCount; ++b) {
            readHeadTag(ifs, "Box");
            ImportedBVBox box;
            box.name     = readText(ifs, "Name");
            box.boneName = readText(ifs, "Bone");
            box.center   = readVec3(ifs, "Center");
            box.size     = readVec3(ifs, "Size");
            box.rotEuler = readVec3(ifs, "Rotation");
            box.isStatic = readInteger(ifs, "IsStatic") != 0;
            allLODs[target].push_back(box);
            readTailTag(ifs, "Box");

            gSharedLog << "[Resource Load] BV box " << box.name << " (LOD " << target << ")\n";
        }

        readTailTag(ifs, "LOD");
    }

    model.bvh = buildBVHFromLODs(allLODs);
    gSharedLog << "[Resource Load] BVH 구축 완료 - " << model.bvh.nodes.size() << " nodes\n";

    readTailTag(ifs, "BoundingVolumes");
}

// 모델의 본을 읽어들인다.
// 대상 본의 임포트 이후, 자식 본들을 재귀적으로 임포트한다.
[[maybe_unused]] std::size_t importBone(
    std::ifstream& ifs, Model& model, i32t& boneIdx
) {
    readHeadTag(ifs, "Bone");

    auto& bone = (*model.skeleton.bones)[boneIdx];
    bone.name = readText(ifs, "Name");
    // toDress(bind)는 모델 고유 scale이 베이크된 dress 공간 행렬, toLocal(inverse bind)은
    // 그 역행렬이다. 추출기가 toLocal = inverse(toDress)로 기록하므로 스트림 값을 그대로
    // 사용한다. (메시 정점도 추출 시 같은 dress 공간으로 베이크되어 임포트는 scale을
    // 신경 쓸 필요가 없다 — graphicsArchitecture.md "모델 scale 베이크" 참고.)
    const auto toDress = readMatrix(ifs, "Dress");
    bone.toDress = DirectX::XMLoadFloat4x4(&toDress);
    const auto toLocal = readMatrix(ifs, "ToLocal");
    bone.toLocal = DirectX::XMLoadFloat4x4(&toLocal);

    bone.boneIdx = boneIdx++;

    const auto socketTypeStr = readText(ifs, "SocketType");
    bone.socketType = convertStrToBoneSocketType(socketTypeStr);
    if (bone.socketType != Bone::SocketType::None) {
        model.skeleton.socketToBoneIdx.try_emplace(bone.socketType, bone.boneIdx);
    }

    readHeadTag(ifs, "Children");

    const auto childCnt = readInteger(ifs, "ChildCnt");
    bone.children.reserve(childCnt);

    for (int i = 0; i < childCnt; ++i) {
        auto& child = (*model.skeleton.bones)[ importBone(ifs, model, boneIdx) ];
        bone.children.push_back(&child);
    }

    readTailTag(ifs, "Children");

    readTailTag(ifs, "Bone");

    gSharedLog << "[Resource Load] 본 " << bone.name << " 구축 완료\n";

    return bone.boneIdx;
}

// 모델의 스켈레톤을 읽어들인다.
// 추출하는 과정에서 무조건 0번 본이 루트 본이므로,
// 0번 본부터 본 트리를 순회하며 모든 본을 읽어들인다.
void importSkeleton(std::ifstream& ifs, Model& model) {
    readHeadTag(ifs, "Skeleton");

    auto& skeleton = model.skeleton;

    skeleton.name = readText(ifs, "Name");

    const auto skeletonEnumerationStr = readText(ifs, "SkeletonEnumeration");
    skeleton.skeletonEnumeration = convertStrToSkeletonEnum(skeletonEnumerationStr);

    const auto boneCnt = readInteger(ifs, "Count");
    skeleton.bones = std::make_shared<std::vector<Bone>>(boneCnt);

    i32t boneIdxAux{};
    importBone(ifs, model, boneIdxAux);

    readTailTag(ifs, "Skeleton");

    skeleton.pRoot = &(*skeleton.bones)[0];

    gSharedLog << "[Resource Load] 스켈레톤 " << skeleton.name << "구축 완료\n";
}

// 해당 모델의 socket으로부터의 offset들을 읽는다.
void importSocketOffsets(std::ifstream& ifs, Model& model) {
    readHeadTag(ifs, "SocketOffsets");

    const auto socketOffsetCnt = readInteger(ifs, "SocketOffsetCnt");
    for (int i = 0; i < socketOffsetCnt; ++i) {
        readHeadTag(ifs, "SocketOffset");

        const auto socketTypeStr = readText(ifs, "SocketType");
        const auto trs = readVec3(ifs, "Translation");
        const auto rot = readVec4(ifs, "Rotation");
        const auto scl = readVec3(ifs, "Scale");

        const auto socketType = convertStrToBoneSocketType(socketTypeStr);
        model.socketOffsets.try_emplace( socketType,
            mu::Mat4x4(mu::scale(DirectX::XMLoadFloat3(&scl)))
            * mu::Mat4x4(mu::NQuat(DirectX::XMLoadFloat4(&rot)))
            * mu::translate(DirectX::XMLoadFloat3(&trs))
        );

        readTailTag(ifs, "SocketOffset");
    }

    readTailTag(ifs, "SocketOffsets");
}

// 모델의 아이템 정보를 읽어들인다.
// 현재는 socket으로부터의 offset들을 읽는다.
void importItemData(std::ifstream& ifs, Model& model) {
    readHeadTag(ifs, "ItemData");
    importSocketOffsets(ifs, model);
    readTailTag(ifs, "ItemData");
}

// Reads ragdoll body/joint definitions exported from GoblinRagdollConfig (Unity).
// Mirrors ExtractRagdollConfig in ModelExtractor.cs.
static void importRagdollConfig(std::ifstream& ifs, Model& model)
{
    readHeadTag(ifs, "RagdollConfig");

    RagdollDef def{};

    const int bodyCnt = readInteger(ifs, "BodyCnt");
    def.bones.reserve(bodyCnt);
    for (int i = 0; i < bodyCnt; ++i) {
        readHeadTag(ifs, "Body");
        BoneBoxDef bd{};
        bd.bodyName       = readText(ifs, "BodyName");
        bd.boneName       = readText(ifs, "BoneName");
        const auto he     = readVec3(ifs, "HalfExtents");
        const auto ctr    = readVec3(ifs, "Center");
        const auto re     = readVec3(ifs, "RotEuler");
        bd.halfExtents    = mu::Vec3(XMLoadFloat3(&he));
        bd.center         = mu::Vec3(XMLoadFloat3(&ctr));
        bd.rotEuler       = mu::Vec3(XMLoadFloat3(&re));
        bd.mass           = readFloat(ifs, "Mass");
        bd.linearDamping  = readFloat(ifs, "LinearDamping");
        bd.angularDamping = readFloat(ifs, "AngularDamping");
        bd.friction       = readFloat(ifs, "Friction");
        bd.restitution    = readFloat(ifs, "Restitution");
        bd.noiseImpulse   = readFloat(ifs, "NoiseImpulse");
        def.bones.push_back(std::move(bd));
        readTailTag(ifs, "Body");
    }

    const int jointCnt = readInteger(ifs, "JointCnt");
    def.joints.reserve(jointCnt);
    for (int i = 0; i < jointCnt; ++i) {
        readHeadTag(ifs, "Joint");
        JointDef jd{};
        jd.parentBodyName  = readText(ifs, "ParentBody");
        jd.childBodyName   = readText(ifs, "ChildBody");
        jd.parentBoneName  = readText(ifs, "ParentBone");
        jd.childBoneName   = readText(ifs, "ChildBone");
        const auto typeStr = readText(ifs, "JointType");
        if      (typeStr == "Hinge")      jd.type = JointType::Hinge;
        else if (typeStr == "BallSocket") jd.type = JointType::BallSocket;
        else                              jd.type = JointType::ConeTwist;

        if (jd.type == JointType::Hinge) {
            const auto axis = readVec3(ifs, "AxisLocalA");
            jd.axisLocalA   = mu::Vec3(XMLoadFloat3(&axis));
            jd.minAngle     = readFloat(ifs, "MinAngle");
            jd.maxAngle     = readFloat(ifs, "MaxAngle");
        } else if (jd.type == JointType::ConeTwist) {
            jd.coneHalfAngle = readFloat(ifs, "ConeHalfAngle");
            jd.twistLimit    = readFloat(ifs, "TwistLimit");
        }

        def.joints.push_back(std::move(jd));
        readTailTag(ifs, "Joint");
    }

    model.ragdollDef = std::move(def);
    gSharedLog << "[Resource Load] Ragdoll config: " << bodyCnt << " bodies, " << jointCnt << " joints\n";

    readTailTag(ifs, "RagdollConfig");
}

// 바이너리 파일로부터 모델을 읽어온다.
// 메시들을 생성하며 각 메시들의 버텍스 버퍼와 서브메시, 그리고 재질 집합을 생성한다.
// 그 과정에서 필요한 텍스처들이 texHashMap에 존재하지 않는다면, 로드한다.
// (로드되는 텍스처의 경로들은 바이너리 파일 내에 적혀있다.)
// 모델에 스켈레톤이 있는 경우, 스켈레톤 정보도 로드한다.
// 모델에 아이템 정보가 있는 경우, 아이템 정보도 로드한다.
// * 수정 시 주의사항: 유니티의 추출 스크립트와 구조가 대칭이어야 한다.
Model loadModelFromFile( const std::filesystem::path& path,
	ID3D12Device* device, ID3D12GraphicsCommandList* cmdList,
	std::unordered_map<std::string, Texture>& texHashMap,
	DescriptorPool& texPool, Fence& fenceToAssociate	
) {
    Model ret{};

    auto ifs = std::ifstream(path, std::ios::binary);
    DISPLAY_ERROR_STR(ifs.good(), "[File I/O Error]: loadModelFromFile: "s + path.string() + " 파일을 열 수 없습니다."s, false);
    if (!ifs) {
        return ret;
    }

    ret.name = readText(ifs, "ModelName");

    // 텍스처의 물리적 로드는 여기서 이루어진다.
    importTextureMapping(ifs, device, cmdList, texHashMap, texPool, fenceToAssociate);

    importGeometry(ifs, device, cmdList, texHashMap, fenceToAssociate, ret);
    importBoundingVolumes(ifs, ret);

    const auto hasSkeleton = static_cast<bool>(readInteger(ifs, "HasSkeleton"));
    if (hasSkeleton) {
        importSkeleton(ifs, ret);
        resolveBVHBoneIndices(ret.bvh, ret.skeleton);
    }

    // 해당 모델이 아이템으로 쓰일 수 있는 모델이라면,
    // 아이템 정보를 추출한다.
    // (소켓에 장착 가능할 경우 소켓으로부터의 오프셋 등)
    const auto hasWeaponInfo = static_cast<bool>(readInteger(ifs, "HasWeaponInfo"));
    if (hasWeaponInfo) {
        importItemData(ifs, ret);
    }

    const auto hasRagdollConfig = static_cast<bool>(readInteger(ifs, "HasRagdollConfig"));
    if (hasRagdollConfig) {
        importRagdollConfig(ifs, ret);
    }

    gSharedLog << "[Resource Load] File I/O: 모델 " << ret.name << '(' << path << ") 로드 완료\n";

    return ret;
}

// 샘플링 정보, 경로 정보를 읽어 텍스처 객체를 구축한다.
// 스카이박스의 이름도 지정한다.
void importSkyboxCubemap( std::ifstream& ifs,
	ID3D12Device* device, ID3D12GraphicsCommandList* cmdList,
	DescriptorPool& texCubePool, Fence& fenceToAssociate,
    Skybox& skybox
) {
    std::string name{};
    std::string path{};
    // 기본값들로 초기화
    D3D12_FILTER filterMode = D3D12_FILTER_MIN_MAG_MIP_POINT;
    D3D12_TEXTURE_ADDRESS_MODE addrModeU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    D3D12_TEXTURE_ADDRESS_MODE addrModeV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    D3D12_TEXTURE_ADDRESS_MODE addrModeW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    UINT anisoLevel = 1u;

    for (;;) {
        const auto str = readString(ifs);
        if (isTailTag(str, "Cube")) {
            break;
        }

        const auto tag = untagHead(str);

        // 텍스처 이름 추출
        if (tag == "Name") {
            name = readString(ifs);
            readTailTag(ifs, "Name");
        }
        // Address Mode(Wrap Mode) 추출
        else if (tag == "WrapModeU") {
            const auto wrapModeUStr = readString(ifs);
            if (wrapModeUStr == "Repeat") {
                addrModeU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
            }
            else if (wrapModeUStr == "Clamp") {
                addrModeU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
            }
            else if (wrapModeUStr == "Mirror") {
                addrModeU = D3D12_TEXTURE_ADDRESS_MODE_MIRROR;
            }
            else {
                DISPLAY_ERROR_STR(false, "[GFX Error] importTextureMapping: 알수 없는 텍스처 wrap mode "s
                    + wrapModeUStr + "을 읽었습니다.", false
                );
            }

            readTailTag(ifs, "WrapModeU");
        }
        else if (tag == "WrapModeV") {
            const auto wrapModeVStr = readString(ifs);
            if (wrapModeVStr == "Repeat") {
                addrModeV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
            }
            else if (wrapModeVStr == "Clamp") {
                addrModeV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
            }
            else if (wrapModeVStr == "Mirror") {
                addrModeV = D3D12_TEXTURE_ADDRESS_MODE_MIRROR;
            }
            else {
                DISPLAY_ERROR_STR(false, "[GFX Error] importTextureMapping: 알수 없는 텍스처 wrap mode "s
                    + wrapModeVStr + "을 읽었습니다.", false
                );
            }

            readTailTag(ifs, "WrapModeV");
        }
        else if (tag == "WrapModeW") {
            const auto wrapModeWStr = readString(ifs);
            if (wrapModeWStr == "Repeat") {
                addrModeW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
            }
            else if (wrapModeWStr == "Clamp") {
                addrModeW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
            }
            else if (wrapModeWStr == "Mirror") {
                addrModeW = D3D12_TEXTURE_ADDRESS_MODE_MIRROR;
            }
            else {
                DISPLAY_ERROR_STR(false, "[GFX Error] importTextureMapping: 알수 없는 텍스처 wrap mode "s
                    + wrapModeWStr + "을 읽었습니다.", false
                );
            }

            readTailTag(ifs, "WrapModeW");
        }
        // Filter Mode 추출
        else if (tag == "FilterMode") {
            const auto filterModeStr = readString(ifs);
            if (filterModeStr == "Point") {
                filterMode = D3D12_FILTER_MIN_MAG_MIP_POINT;
            }
            else if (filterModeStr == "Bilinear") {
                filterMode = D3D12_FILTER_MIN_MAG_LINEAR_MIP_POINT;
            }
            else if (filterModeStr == "Trilinear") {
                filterMode = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
            }
            else if (filterModeStr == "Anisotropic") {
                filterMode = D3D12_FILTER_ANISOTROPIC;
            }
            else {
                DISPLAY_ERROR_STR(false, "[GFX Error] importTextureMapping: 알수 없는 텍스처 filter mode "s
                    + filterModeStr + "을 읽었습니다.", false
                );
            }

            readTailTag(ifs, "FilterMode");
        }
        // 비등방성 필터링 레벨(최대 레벨) 추출
        else if (tag == "AnisoLevel") {
            anisoLevel = static_cast<UINT>(readInteger(ifs));
            readTailTag(ifs, "AnisoLevel");
        }
        // 경로 추출
        else if (tag == "DDSPath") {
            path = readString(ifs);
            readTailTag(ifs, "DDSPath");
        }
        else {
            DISPLAY_ERROR_STR(false, "[File I/O Error] importTextureMapping: 알 수 없는 태그 "s
                + tag + "를 읽었습니다.", true
            );
        }
    }

    // 텍스처 구축
    Texture::Type type{};
    skybox.texSkybox = loadTexture(device, cmdList, path, fenceToAssociate, type);
    skybox.name = name;

    // TODO : mip levels를 dds에서 읽은 내용으로 설정해야 한다.
    // 큐브맵 텍스처는 D3D12_SHADER_RESOURCE_VIEW_DESC를 기본값(null)으로 사용하면 안 그려진다.
    // 직접 작성해서 넘겨주어야 한다.
    createSRV( device, skybox.texSkybox, D3D12_SHADER_RESOURCE_VIEW_DESC{
		.Format = DXGI_FORMAT_R8G8B8A8_UNORM,
		.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE,
		.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
		.TextureCube = D3D12_TEXCUBE_SRV{
			.MostDetailedMip = 0u,
			.MipLevels = 10u
		}
	}, texCubePool );

    skybox.texSkybox.idxSrv.idxRange = etoi(Texture::Type::TexCube);
    skybox.texSkybox.idxUav.idxRange = etoi(Texture::Type::TexCube);

    // 샘플러 인덱스를 계산해서 채워넣기
    const auto samplerIdx = calcIdxBindlessSampler(filterMode, addrModeU, addrModeV, addrModeW, anisoLevel);
    skybox.texSkybox.idxSrv.idxSampler = samplerIdx;
    skybox.texSkybox.idxUav.idxSampler = samplerIdx;
}

// 스카이박스에서 사용되는 텍스처들에 대한 텍스처 매핑 정보를 읽어들인다.
// 각 매핑 정보에서 얻어낸 샘플링 정보, 경로 정보를 바탕으로
// 텍스처 객체를 구축한다.
void importSkyboxTextureMapping( std::ifstream& ifs,
	ID3D12Device* device, ID3D12GraphicsCommandList* cmdList,
	DescriptorPool& texCubePool, Fence& fenceToAssociate,
    Skybox& skybox
) {
    readHeadTag(ifs, "TextureMapping");

    for (;;) {
        const auto str = readString(ifs);
        if (isTailTag(str, "TextureMapping")) {
            break;
        }

        const auto tag = untagHead(str);
        if (tag == "Cube") {
            importSkyboxCubemap(ifs, device, cmdList, texCubePool, fenceToAssociate, skybox);
        }
        else {
            DISPLAY_ERROR_STR(false, "[File I/O Error] importSkyboxTextureMapping: 알 수 없는 태그 "s
                + tag + "를 읽었습니다.", true
            );
        }
    }
}

// 스카이박스에 담겨 있는 재질 정보들을 읽어들인다.
void importSkyboxMaterials(std::ifstream& ifs) {
    const auto materialCnt = readInteger(ifs, "MaterialCnt");

    for (int i = 0; i < materialCnt; ++i) {
        readHeadTag(ifs, "Material");
        const auto matName = readText(ifs, "Cubemap");
        readTailTag(ifs, "Material");
    }
}

// 바이너리 파일로부터 스카이박스를 읽어온다.
// 파일에 적혀있는 큐브맵 텍스처 정보를 통해 스카이박스 재질을 완성한다.
// 이 함수는 무조건 연관된 텍스처를 로드하므로, 중복호출되지 않도록 주의한다.
// * 수정 시 주의사항: 유니티의 추출 스크립트와 구조가 대칭이어야 한다.
Skybox loadSkyboxFromFile( const std::filesystem::path& path,
	ID3D12Device* device, ID3D12GraphicsCommandList* cmdList,
	DescriptorPool& texCubePool, Fence& fenceToAssociate
) {
    Skybox ret{};

    auto ifs = std::ifstream(path, std::ios::binary);
    DISPLAY_ERROR_STR(ifs.good(), "[File I/O Error]: loadSkyboxFromFile: "s + path.string() + " 파일을 열 수 없습니다."s, false);
    if (!ifs) {
        return ret;
    }

    importSkyboxTextureMapping(ifs, device, cmdList, texCubePool, fenceToAssociate, ret);
    importSkyboxMaterials(ifs);

    gSharedLog << "[Resource Load] File I/O: 스카이박스 " << ret.name << '(' << path << ") 로드 완료\n";

    return ret;
}

// .meshbin v1 포맷에서 메시를 로드한다.
// 포맷:
//   [u8*8]  magic = "MESHBIN\0"
//   [u8]    version = 1
//   [u32]   vertexCount
//   [u32]   indexCount
//   [struct { float3 pos, float2 uv }] * vertexCount
//   [u16] * indexCount
//   [u8]    texturePathLen
//   [char * texturePathLen]
std::pair<Mesh, std::string> loadMeshBin(
	const std::filesystem::path& path,
	ID3D12Device* device,
	ID3D12GraphicsCommandList* cmdList,
	Fence& fenceToAssociate
) {
	auto ret = std::pair<Mesh, std::string>{};
	auto& mesh = ret.first;

	auto ifs = std::ifstream(path, std::ios::binary);
	DISPLAY_ERROR_STR(ifs.good(), "[File I/O Error]: loadMeshBin: "s + path.string() + " 파일을 열 수 없습니다."s, false);
	if (!ifs) return ret;

	// magic
	char magic[8]{};
	ifs.read(magic, 8);
	DISPLAY_ERROR_STR(std::string_view(magic, 7) == "MESHBIN",
		"[File I/O Error]: loadMeshBin: invalid magic in "s + path.string(), false);
	if (std::string_view(magic, 7) != "MESHBIN") return ret;

	// version
	uint8_t version{};
	ifs.read(reinterpret_cast<char*>(&version), 1);
	DISPLAY_ERROR_STR(version == 1u || version == 2u,
		"[File I/O Error]: loadMeshBin: unsupported version "s + std::to_string(version), false);
	if (version != 1u && version != 2u) return ret;

	// v2: flags (bit 0 = hasColor, bit 1 = hasNormal)
	uint8_t flags = 0u;
	if (version == 2u)
		ifs.read(reinterpret_cast<char*>(&flags), 1);
	const bool hasColor  = (flags & 0x1u) != 0u;
	const bool hasNormal = (flags & 0x2u) != 0u;

	// counts
	uint32_t vertexCount{}, indexCount{};
	ifs.read(reinterpret_cast<char*>(&vertexCount), 4);
	ifs.read(reinterpret_cast<char*>(&indexCount), 4);

	// vertex data: interleaved position(float3) + uv(float2)
	struct RawVertex { XMFLOAT3 pos; XMFLOAT2 uv; };
	auto rawVertices = std::vector<RawVertex>(vertexCount);
	ifs.read(reinterpret_cast<char*>(rawVertices.data()), vertexCount * sizeof(RawVertex));

	// split into separate position and uv arrays
	auto positions = std::vector<XMFLOAT3>(vertexCount);
	auto uvs       = std::vector<XMFLOAT2>(vertexCount);
	for (uint32_t i = 0; i < vertexCount; ++i) {
		positions[i] = rawVertices[i].pos;
		uvs[i]       = rawVertices[i].uv;
	}

	// v2: color channel (flat float4 array, after pos+uv block)
	auto colors = std::vector<XMFLOAT4>{};
	if (hasColor) {
		colors.resize(vertexCount);
		ifs.read(reinterpret_cast<char*>(colors.data()), vertexCount * sizeof(XMFLOAT4));
	}

	// v2: normal channel (flat float3 array, after color block)
	auto normals = std::vector<XMFLOAT3>{};
	if (hasNormal) {
		normals.resize(vertexCount);
		ifs.read(reinterpret_cast<char*>(normals.data()), vertexCount * sizeof(XMFLOAT3));
	}

	// indices
	auto indices = std::vector<u16t>(indexCount);
	ifs.read(reinterpret_cast<char*>(indices.data()), indexCount * sizeof(u16t));

	// texture path
	uint8_t texPathLen{};
	ifs.read(reinterpret_cast<char*>(&texPathLen), 1);
	auto texPath = std::string(texPathLen, '\0');
	ifs.read(texPath.data(), texPathLen);
	ret.second = texPath;

	const auto meshName = path.stem().string();
	mesh.name = meshName;

	// build GPU buffers: position VB (Slot 0)
	auto vbPos  = createBufferResource(device, nullptr, positions.size() * sizeof(XMFLOAT3), BufferCreationType::VertexBuffer);
	setD3DName(vbPos.Get(), (meshName + "_VB_Position").c_str());
	auto vbPosu = createBufferResource(device, positions.data(), positions.size() * sizeof(XMFLOAT3), BufferCreationType::UploadBuffer);
	setD3DName(vbPosu.Get(), (meshName + "_VB_Position_Upload").c_str());
	copyResource(cmdList, vbPosu.Get(), vbPos.Get(), D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);

	// uv VB (Slot 1)
	auto vbUV  = createBufferResource(device, nullptr, uvs.size() * sizeof(XMFLOAT2), BufferCreationType::VertexBuffer);
	setD3DName(vbUV.Get(), (meshName + "_VB_UV").c_str());
	auto vbUVu = createBufferResource(device, uvs.data(), uvs.size() * sizeof(XMFLOAT2), BufferCreationType::UploadBuffer);
	setD3DName(vbUVu.Get(), (meshName + "_VB_UV_Upload").c_str());
	copyResource(cmdList, vbUVu.Get(), vbUV.Get(), D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);

	// color VB (Slot 2, v2 only)
	ComPtr<ID3D12Resource> vbColor{}, vbColoru{};
	if (hasColor) {
		vbColor  = createBufferResource(device, nullptr, colors.size() * sizeof(XMFLOAT4), BufferCreationType::VertexBuffer);
		setD3DName(vbColor.Get(), (meshName + "_VB_Color").c_str());
		vbColoru = createBufferResource(device, colors.data(), colors.size() * sizeof(XMFLOAT4), BufferCreationType::UploadBuffer);
		setD3DName(vbColoru.Get(), (meshName + "_VB_Color_Upload").c_str());
		copyResource(cmdList, vbColoru.Get(), vbColor.Get(), D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
	}

	// normal VB (v2, bit1)
	ComPtr<ID3D12Resource> vbNormal{}, vbNormalu{};
	if (hasNormal) {
		vbNormal  = createBufferResource(device, nullptr, normals.size() * sizeof(XMFLOAT3), BufferCreationType::VertexBuffer);
		setD3DName(vbNormal.Get(), (meshName + "_VB_Normal").c_str());
		vbNormalu = createBufferResource(device, normals.data(), normals.size() * sizeof(XMFLOAT3), BufferCreationType::UploadBuffer);
		setD3DName(vbNormalu.Get(), (meshName + "_VB_Normal_Upload").c_str());
		copyResource(cmdList, vbNormalu.Get(), vbNormal.Get(), D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
	}

	// IB
	auto ib  = createBufferResource(device, nullptr, indices.size() * sizeof(u16t), BufferCreationType::IndexBuffer);
	setD3DName(ib.Get(), (meshName + "_IB").c_str());
	auto ibu = createBufferResource(device, indices.data(), indices.size() * sizeof(u16t), BufferCreationType::UploadBuffer);
	setD3DName(ibu.Get(), (meshName + "_IB_Upload").c_str());
	copyResource(cmdList, ibu.Get(), ib.Get(), D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_INDEX_BUFFER);

	// VB views (Position=0, UV=1 always; Color and Normal at dynamic indices)
	mesh.vbViews.emplace_back(vbPos->GetGPUVirtualAddress(), static_cast<UINT>(positions.size() * sizeof(XMFLOAT3)), static_cast<UINT>(sizeof(XMFLOAT3)));
	mesh.vbViews.emplace_back(vbUV->GetGPUVirtualAddress(), static_cast<UINT>(uvs.size() * sizeof(XMFLOAT2)), static_cast<UINT>(sizeof(XMFLOAT2)));
	mesh.vbIdxMap.try_emplace(meshName + "_VB_Position", 0u);
	mesh.vbIdxMap.try_emplace(meshName + "_VB_UV", 1u);
	if (hasColor) {
		mesh.vbIdxMap.try_emplace(meshName + "_VB_Color", static_cast<u32t>(mesh.vbViews.size()));
		mesh.vbViews.emplace_back(vbColor->GetGPUVirtualAddress(), static_cast<UINT>(colors.size() * sizeof(XMFLOAT4)), static_cast<UINT>(sizeof(XMFLOAT4)));
	}
	if (hasNormal) {
		mesh.vbIdxMap.try_emplace(meshName + "_VB_Normal", static_cast<u32t>(mesh.vbViews.size()));
		mesh.vbViews.emplace_back(vbNormal->GetGPUVirtualAddress(), static_cast<UINT>(normals.size() * sizeof(XMFLOAT3)), static_cast<UINT>(sizeof(XMFLOAT3)));
	}

	// IB + submesh
	mesh.subMeshes.emplace_back(
		meshName + "_SubMesh",
		D3D12_INDEX_BUFFER_VIEW{
			.BufferLocation = ib->GetGPUVirtualAddress(),
			.SizeInBytes    = static_cast<UINT>(indices.size() * sizeof(u16t)),
			.Format         = DXGI_FORMAT_R16_UINT
		}
	);

	// ownership
	mesh.vbs.push_back(std::move(vbPos));
	mesh.vbs.push_back(std::move(vbUV));
	if (hasColor)  mesh.vbs.push_back(std::move(vbColor));
	if (hasNormal) mesh.vbs.push_back(std::move(vbNormal));
	mesh.ibs.push_back(std::move(ib));

	fenceToAssociate.associatedResources_.push_back(std::move(vbPosu));
	fenceToAssociate.associatedResources_.push_back(std::move(vbUVu));
	if (hasColor)  fenceToAssociate.associatedResources_.push_back(std::move(vbColoru));
	if (hasNormal) fenceToAssociate.associatedResources_.push_back(std::move(vbNormalu));
	fenceToAssociate.associatedResources_.push_back(std::move(ibu));

	gSharedLog << "[Resource Load] loadMeshBin: " << meshName << " (" << path << ") loaded, "
		<< vertexCount << " verts, " << indexCount << " indices, "
		<< "color=" << (hasColor ? "yes" : "no") << ", normal=" << (hasNormal ? "yes" : "no")
		<< ", tex=" << texPath << '\n';

	return ret;
}
#include "mesh.hpp"
#include "gfxUtil.hpp"
#include "errorHandling.hpp"

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
        auto [pPair, _] = texHashMap.try_emplace("CubeMesh_Albedo", loadTexture(device, cmdList, "CubeMesh_Albedo.dds", fenceToAssociate));
        gSharedLog << "[Resource Load] File I/O: 텍스처 CubeMesh_Albedo 로드 완료 - CubeMesh_Albedo.dds\n";
        createSRV(device, pPair->second, texPool);
        pPair->second.idxSrv.idxSampler = etoi(Samplers::TrilinearWrap);
    }

    // SubMesh 구성
    mesh.subMeshes.try_emplace(
        "CubeMesh_SubMesh", SubMesh{
            .name = "CubeMesh_SubMesh",
            .ibView = D3D12_INDEX_BUFFER_VIEW {
                .BufferLocation = ib->GetGPUVirtualAddress(),
                .SizeInBytes = static_cast<UINT>(indices.size() * sizeof(u16t)),
                .Format = DXGI_FORMAT_R16_UINT
            },
            .material = Material {
                .mapAlbedo = cloneTextureIdxOnly(texHashMap.at("CubeMesh_Albedo")),
                .constantAlbedo = XMFLOAT4(0.f, 0.f, 0.f, -1.f),
                .constantRoughness = 0.3f,
                .constantMetallic = 0.15f,
                .constantAOStrength = 0.f,
                .constantEmmisive = XMFLOAT3(0.f, 0.f, 0.f)
            }
        }
    );

    // 자료구조 등록 (Vertex Buffer View와 SubMesh는 위에서 등록하였음)
	mesh.vbs.push_back(std::move(vbPosition));
    mesh.vbIdxMap.try_emplace("CubeMesh_VB_Position", 0u);
    mesh.vbs.push_back(std::move(vbNormal));
    mesh.vbIdxMap.try_emplace("CubeMesh_VB_Normal", 1u);
    mesh.vbs.push_back(std::move(vbUV));
    mesh.vbIdxMap.try_emplace("CubeMesh_VB_UV", 2u);
	mesh.ibs.try_emplace("CubeMesh_IB", std::move(ib));

    gSharedLog << "[Resource Load] CubeMesh 구축 완료\n";

    fenceToAssociate.associatedResources_.push_back(std::move(vbPositionu));
    fenceToAssociate.associatedResources_.push_back(std::move(vbNormalu));
    fenceToAssociate.associatedResources_.push_back(std::move(vbUVu));
    fenceToAssociate.associatedResources_.push_back(std::move(ibu));

    return mesh;
}

// 바이너리 파일에서 데이터를 읽는데 쓰이는 유틸리티 함수
void readHeadTag(std::ifstream& ifs, const std::string& expectedSource) {
    char tmpBuffer[32]{'\0'};
    unsigned char sz{};

    const auto expected = "<"s + expectedSource + ":>"s;
    ifs.read(reinterpret_cast<char*>(&sz), sizeof(unsigned char));
    ifs.read(tmpBuffer, sz);

    std::string wExpected{};
    wExpected.assign(expected.begin(), expected.end());

    std::string wReceived{};
    wReceived.assign(tmpBuffer, tmpBuffer + sz);

    DISPLAY_ERROR_STR(expected == tmpBuffer,
        "[File I/O Error] readHeadTag: "s + wExpected + " 토큰을 기대했지만 "s
        + wReceived + " 토큰을 받았습니다.", true
    );
}

// 바이너리 파일에서 데이터를 읽는데 쓰이는 유틸리티 함수
void readTailTag(std::ifstream& ifs, const std::string& expectedSource) {
    char tmpBuffer[32]{'\0'};
    unsigned char sz{};

    const auto expected = "</"s + expectedSource + ">"s;
    ifs.read(reinterpret_cast<char*>(&sz), sizeof(unsigned char));
    ifs.read(tmpBuffer, sz);

    std::string wExpected{};
    wExpected.assign(expected.begin(), expected.end());

    std::string wReceived{};
    wReceived.assign(tmpBuffer, tmpBuffer + sz);

    DISPLAY_ERROR_STR(expected == tmpBuffer,
        "[File I/O Error] readTailTag: "s + wExpected + " 토큰을 기대했지만 "s
        + wReceived + " 토큰을 받았습니다.", true
    );
}

// 바이너리 파일에서 데이터를 읽는데 쓰이는 유틸리티 함수
bool isTailTag(const std::string& str, const std::string& expectedSource) {
    const auto expected = "</"s + expectedSource + ">"s;
    return str == expected;
}

// 바이너리 파일에서 데이터를 읽는데 쓰이는 유틸리티 함수
std::string readString(std::ifstream& ifs) {
    char tmpBuffer[64]{'\0'};
    unsigned char sz{};

    ifs.read(reinterpret_cast<char*>(&sz), sizeof(unsigned char));
    ifs.read(tmpBuffer, sz);

    return std::string(tmpBuffer, tmpBuffer + sz);
}

// 바이너리 파일에서 데이터를 읽는데 쓰이는 유틸리티 함수
std::string readText(std::ifstream& ifs, const char* tagSource) {
    readHeadTag(ifs, tagSource);
    auto ret = readString(ifs);
    readTailTag(ifs, tagSource);
    return ret;
}

// 바이너리 파일에서 데이터를 읽는데 쓰이는 유틸리티 함수
std::string untagHead(const std::string& tag) {
    return tag.substr(1u, tag.size() - 1u - 2u);
}

// 바이너리 파일에서 데이터를 읽는데 쓰이는 유틸리티 함수
int readInteger(std::ifstream& ifs) {
    int ret{};
    ifs.read(reinterpret_cast<char*>(&ret), sizeof(int));
    return ret;
}

// 바이너리 파일에서 데이터를 읽는데 쓰이는 유틸리티 함수
int readInteger(std::ifstream& ifs, const char* tagSource) {
    readHeadTag(ifs, tagSource);
    int ret{};
    ifs.read(reinterpret_cast<char*>(&ret), sizeof(int));
    readTailTag(ifs, tagSource);
    return ret;
}

// 바이너리 파일에서 데이터를 읽는데 쓰이는 유틸리티 함수
std::vector<int> readIntegers(std::ifstream& ifs, const char* tagSource) {
    readHeadTag(ifs, tagSource);
    auto cnt = readInteger(ifs, "Cnt");
    auto ret = std::vector<int>(cnt);
    for (int i = 0; i < cnt; ++i) {
        ifs.read(reinterpret_cast<char*>(&ret[i]), sizeof(int));
    }
    readTailTag(ifs, tagSource);
    return ret;
}

// 바이너리 파일에서 데이터를 읽는데 쓰이는 유틸리티 함수
std::vector<u16t> readU16s(std::ifstream& ifs, const char* tagSource) {
    readHeadTag(ifs, tagSource);
    auto cnt = readInteger(ifs, "Cnt");
    auto ret = std::vector<u16t>(cnt);
    for (int i = 0; i < cnt; ++i) {
        ifs.read(reinterpret_cast<char*>(&ret[i]), sizeof(u16t));
    }
    readTailTag(ifs, tagSource);
    return ret;
}

// 바이너리 파일에서 데이터를 읽는데 쓰이는 유틸리티 함수
float readFloat(std::ifstream& ifs) {
    float ret{};
    ifs.read(reinterpret_cast<char*>(&ret), sizeof(float));
    return ret;
}

// 바이너리 파일에서 데이터를 읽는데 쓰이는 유틸리티 함수
float readFloat(std::ifstream& ifs, const char* tagSource) {
    readHeadTag(ifs, tagSource);
    float ret{};
    ifs.read(reinterpret_cast<char*>(&ret), sizeof(float));
    readTailTag(ifs, tagSource);
    return ret;
}

// 바이너리 파일에서 데이터를 읽는데 쓰이는 유틸리티 함수
XMFLOAT4 readColor(std::ifstream& ifs) {
    XMFLOAT4 ret{};
    ifs.read(reinterpret_cast<char*>(&ret), sizeof(XMFLOAT4));
    return ret;
}

// 바이너리 파일에서 데이터를 읽는데 쓰이는 유틸리티 함수
XMFLOAT4X4 readMatrix(std::ifstream& ifs, const char* tagSource) {
    readHeadTag(ifs, tagSource);
    XMFLOAT4X4 ret{};
    ifs.read(reinterpret_cast<char*>(&ret), sizeof(XMFLOAT4X4));
    readTailTag(ifs, tagSource);
    return ret;
}

// 바이너리 파일에서 데이터를 읽는데 쓰이는 유틸리티 함수
std::vector<XMFLOAT2> readVec2s(std::ifstream& ifs) {
    auto cnt = readInteger(ifs, "Cnt");
    auto ret = std::vector<XMFLOAT2>(cnt);
    for (int i = 0; i < cnt; ++i) {
        ifs.read(reinterpret_cast<char*>(&ret[i]), sizeof(XMFLOAT2));
    }
    return ret;
}

// 바이너리 파일에서 데이터를 읽는데 쓰이는 유틸리티 함수
std::vector<XMFLOAT3> readVec3s(std::ifstream& ifs) {
    auto cnt = readInteger(ifs, "Cnt");
    auto ret = std::vector<XMFLOAT3>(cnt);
    for (int i = 0; i < cnt; ++i) {
        ifs.read(reinterpret_cast<char*>(&ret[i]), sizeof(XMFLOAT3));
    }
    return ret;
}

// 바이너리 파일에서 데이터를 읽는데 쓰이는 유틸리티 함수
std::vector<XMFLOAT4> readVec4s(std::ifstream& ifs) {
    auto cnt = readInteger(ifs, "Cnt");
    auto ret = std::vector<XMFLOAT4>(cnt);
    for (int i = 0; i < cnt; ++i) {
        ifs.read(reinterpret_cast<char*>(&ret[i]), sizeof(XMFLOAT4));
    }
    return ret;
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
        auto [pPair, _] = texHashMap.try_emplace( key, loadTexture(device, cmdList, path, fenceToAssociate) );
        gSharedLog << "[Resource Load] File I/O: 텍스처 " + key + " 로드 완료 - " + path << '\n';

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

        // (텍스처 이름, 텍스처 경로) 쌍 읽기
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
// 서브메시의 재질정보는 importMaterials에서 반영된다.
void importMesh( std::ifstream& ifs, ID3D12Device* device,
    ID3D12GraphicsCommandList* cmdList, const std::string& name,
    Fence& fenceToAssociate, Mesh& mesh
) {
    // 정점 버퍼들 구축
    readHeadTag(ifs, "VertexBuffers");
    
    for (;;) {
        const auto str = readString(ifs);
        if (isTailTag(str, "VertexBuffers")) {
            break;
        }

        const auto tag = untagHead(str);

        // Positions: float3
        if (tag == "Positions") {
            auto positions = readVec3s(ifs);

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
        else {
            DISPLAY_ERROR_STR(false, "[File I/O Error] importMesh: 알 수 없는 태그 "s
                + tag + "를 읽었습니다.", true
            );
        }
    }

    // 서브메시들 구축 (재질, 인덱스 버퍼 중 인덱스 버퍼만 생성)
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

            // SubMesh의 Material은 별도 반영
            mesh.subMeshes.try_emplace(
                subMeshName, SubMesh{
                    .name = subMeshName,
                    .ibView = D3D12_INDEX_BUFFER_VIEW {
                        .BufferLocation = ib->GetGPUVirtualAddress(),
                        .SizeInBytes = static_cast<UINT>(indices.size() * sizeof(u16t)),
                        .Format = DXGI_FORMAT_R16_UINT
                    }
                }
            );

            mesh.ibs.try_emplace(subMeshName + "_IB", std::move(ib));
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

            // SubMesh의 Material은 별도 반영
            mesh.subMeshes.try_emplace(
                subMeshName, SubMesh{
                    .name = subMeshName,
                    .ibView = D3D12_INDEX_BUFFER_VIEW {
                        .BufferLocation = ib->GetGPUVirtualAddress(),
                        .SizeInBytes = static_cast<UINT>(indices.size() * sizeof(i32t)),
                        .Format = DXGI_FORMAT_R16_UINT
                    }
                }
            );

            mesh.ibs.try_emplace(subMeshName + "_IB", std::move(ib));
            fenceToAssociate.associatedResources_.push_back(std::move(ibu));
        }
    }
    readTailTag(ifs, "Submeshes");

    readTailTag(ifs, "Mesh");
}

// 노드의 재질 정보를 읽어들인다.
// 현재 읽어들인 재질 정보는 노드의 메시의 모든 서브메시에
// 일괄적으로 저장되도록 구현되어있다.
void importMaterials( std::ifstream& ifs, ID3D12Device* device,
    ID3D12GraphicsCommandList* cmdList, 
    std::unordered_map<std::string, Texture>& texHashMap,
    Fence& fenceToAssociate, Mesh& mesh
) {
    // Bindless Index 초기화
    // idxRange를 -1로 써주는 것으로 그 텍스처가 존재하지 않음을 표현한다.
    for (auto& [submeshKey, submesh] : mesh.subMeshes) {
        submesh.material.mapAlbedo.idxSrv.idxRange = -1;
        submesh.material.mapAlbedo.idxUav.idxRange = -1;
        submesh.material.mapMetallicSmoothness.idxSrv.idxRange = -1;
        submesh.material.mapMetallicSmoothness.idxUav.idxRange = -1;
        submesh.material.mapNormal.idxSrv.idxRange = -1;
        submesh.material.mapNormal.idxUav.idxRange = -1;
        submesh.material.mapEmmisive.idxSrv.idxRange = -1;
        submesh.material.mapEmmisive.idxUav.idxRange = -1;
        submesh.material.mapAmbientOcclusion.idxSrv.idxRange = -1;
        submesh.material.mapAmbientOcclusion.idxUav.idxRange = -1;
    }
    auto materialCnt = readInteger(ifs, "MaterialCnt");

    for (int i = 0; i < materialCnt; ++i) {
        readHeadTag(ifs, "Material");

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
                for (auto& [submeshKey, submesh] : mesh.subMeshes) {
                    submesh.material.constantAlbedo = albedo;
                }
                readTailTag(ifs, "cAlbedo");
            }
            // 자체발광 색상 상수
            else if (tag == "cEmmisive") {
                const auto emmisive = readColor(ifs);
                for (auto& [submeshKey, submesh] : mesh.subMeshes) {
                    submesh.material.constantEmmisive = XMFLOAT3(emmisive.x, emmisive.y, emmisive.z);
                }
                readTailTag(ifs, "cEmmisive");
            }
            // 매끄러움 상수 (거칠기 상수의 역)
            else if (tag == "cSmoothness") {
                const auto smoothness = readFloat(ifs);
                for (auto& [submeshKey, submesh] : mesh.subMeshes) {
                    submesh.material.constantRoughness = 1.f - smoothness;
                }
                readTailTag(ifs, "cSmoothness");
            }
            // 금속성 상수
            else if (tag == "cMetallic") {
                const auto metallic = readFloat(ifs);
                for (auto& [submeshKey, submesh] : mesh.subMeshes) {
                    submesh.material.constantRoughness = 1.f - metallic;
                }
                readTailTag(ifs, "cMetallic");
            }
            // 주변광 차폐 적용 강도 상수
            else if (tag == "cAOStrength") {
                const auto aoStrength = readFloat(ifs);
                for (auto& [submeshKey, submesh] : mesh.subMeshes) {
                    submesh.material.constantAOStrength = aoStrength;
                }
                readTailTag(ifs, "cAOStrength");
            }
            // ======================================
            // 텍스처 읽어들이기 ====================
            // 텍스처 매핑 정보를 통해 미리 로드했던 텍스처들을
            // texHashMap에서 찾아 메시의 모든 서브메시에 연결한다.
            
            // 알베도 텍스처
            else if (tag == "AlbedoMap") {
                const auto albedoMapKey = readString(ifs);
                for (auto& [submeshKey, submesh] : mesh.subMeshes) {
                    submesh.material.mapAlbedo = cloneTextureIdxOnly(texHashMap.at(albedoMapKey));
                }
                readTailTag(ifs, "AlbedoMap");
            }
            // 노멀 텍스처
            else if (tag == "NormalMap") {
                const auto normalMapKey = readString(ifs);
                for (auto& [submeshKey, submesh] : mesh.subMeshes) {
                    submesh.material.mapNormal = cloneTextureIdxOnly(texHashMap.at(normalMapKey));
                }
                readTailTag(ifs, "NormalMap");
            }
            // 금속성과 매끄러움 텍스처
            else if (tag == "MetallicSmoothnessMap") {
                const auto metallicSmoothnessMapKey = readString(ifs);
                for (auto& [submeshKey, submesh] : mesh.subMeshes) {
                    submesh.material.mapMetallicSmoothness
                        = cloneTextureIdxOnly(texHashMap.at(metallicSmoothnessMapKey));
                }
                readTailTag(ifs, "MetallicSmoothnessMap");
            }
            // 자체발광 텍스처
            else if (tag == "EmmisiveMap") {
                const auto emmisiveMapKey = readString(ifs);
                for (auto& [submeshKey, submesh] : mesh.subMeshes) {
                    submesh.material.mapEmmisive = cloneTextureIdxOnly(texHashMap.at(emmisiveMapKey));
                }
                readTailTag(ifs, "EmmisiveMap");
            }
            // 차폐도 텍스처
            else if (tag == "AOMap") {
                const auto aoMapKey = readString(ifs);
                for (auto& [submeshKey, submesh] : mesh.subMeshes) {
                    submesh.material.mapAmbientOcclusion
                        = cloneTextureIdxOnly(texHashMap.at(aoMapKey));
                }
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

// 기하 계층구조의 특정 노드를 읽어들이고
// 그 자식 노드들을 재귀적으로 읽어들인다.
// 노드에는 변환 행렬, 메시와 재질 정보가 담겨 있다.
// 텍스처 매핑 정보를 통해 미리 로드했던 텍스처들이 서브메시들의 재질과 연결된다.
// 노드와 메시, 변환 행렬은 각각 일대일 대응이고,
// 노드와 재질(서브메시에 포함)은 일대다 대응이다.
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

        if (tag == "Mesh") {
            importMesh(ifs, device, cmdList, name, fenceToAssociate, pair.mesh);
        }
        else if (tag == "Materials") {
            importMaterials(ifs, device, cmdList, texHashMap, fenceToAssociate, pair.mesh);    
        }
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
            DISPLAY_ERROR_STR(false, "[File I/O Error] importMaterials: 알 수 없는 태그 "s
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
    importTransform(ifs, device, cmdList, texHashMap, fenceToAssociate, model);
    readTailTag(ifs, "Geometry");
}

// 바이너리 파일로부터 모델을 읽어온다.
// 메시들을 생성하며 각 메시들의 버텍스 버퍼와 서브메시(인덱스버퍼, 재질)을 생성한다.
// 그 과정에서 필요한 텍스처들이 texHashMap에 존재하지 않는다면, 로드한다.
// (로드되는 텍스처의 경로들은 바이너리 파일 내에 적혀있다.)
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
    importTextureMapping(ifs, device, cmdList, texHashMap, texPool, fenceToAssociate);
    importGeometry(ifs, device, cmdList, texHashMap, fenceToAssociate, ret);
    gSharedLog << "[Resource Load] File I/O: 모델 " << ret.name << '(' << path << ") 로드 완료\n";

    return ret;
}
#include "renderer.hpp"

Renderer::Renderer(gfx::d3d12engine::Core& core)
    : shader_( core.device(), core.root(),
        gfx::d3d12::ShaderPBRIllumination::Config{
            .maxInstanceCnt = 0x1000u,
            .maxDrawcallCnt = 0x1000u,
            .maxLightCnt = 0x100u
        }, gfx::d3d12::InputLayout::Spec::separated
    ), renderPass_( core.device(), shader_,
        gfx::d3d12::convClientToVP( core.window().client() )
    ), shaderCube_( core.device(), core.root() ),
    renderPassCube_( core.device(), shaderCube_,
        gfx::d3d12::convClientToVP( core.window().client() )
    ), cubeRefModel_(), cubeModel_() {
    core.prepareGPUResLoad();
    auto cmdList = core.fetchCmdList();
    cubeRefModel_ = CubeRefModel( core, cmdList, 0.5f, 0.5f, 0.5f );
    cubeRefModel_.arrangeVBs( core.device(), cmdList, 1u,
        std::vector<std::vector<gfx::Vertex::Properties>>{
            { gfx::Vertex::Properties::Position3D }
        }
    );
    cubeModel_ = gfx::d3d12::Model( cubeRefModel_ );
    core.finishGPUResLoad();
}

void Renderer::layoutVBs( gfx::d3d12engine::Core& core,
    const gfx::d3d12::RefModelStorage::ID& key,
    std::size_t layoutIdx
) {
    core.layoutRefModelVBs( key, layoutIdx, shader_.inputLayout() );
}

void Renderer::init(gfx::d3d12engine::Scene& scene) {
    renderPass_.init(scene);
    renderPassCube_.trackModel( &cubeModel_ );
}

void Renderer::render(gfx::d3d12engine::Core& core, gfx::d3d12engine::Scene& scene) {
    // renderPass_.update(scene);

    // auto cmdList = core.fetchCmdList();

    // shader_.bindRootParams( cmdList );
    // renderPass_.preRender( cmdList );
    // renderPass_.render( cmdList );
    // renderPass_.postRender( cmdList );

    auto cmdList = core.fetchCmdList();

    shaderCube_.bindRootParams( cmdList );
    renderPassCube_.preRender( cmdList );
    renderPassCube_.render( cmdList );
    renderPassCube_.postRender( cmdList );
}

CubeRefModel::CubeRefModel( gfx::d3d12engine::Core& core, gfx::d3d12::D3D12GfxCmdList& cmdList,
    float width, float height, float depth
) {
    auto& device = core.device();

    nodeStorage_.emplace_back( gfx::d3d12::RefModel::Node(this) );
    pRoot_ = &nodeStorage_.back();

    auto refMesh = gfx::d3d12::RefMesh(pRoot_);

    auto vbPosMem = std::vector<std::uint8_t>(sizeof(gfx::dx::XMFLOAT3) * 6 * 6);

    float fx = width * 0.5f;
    float fy = height * 0.5f;
    float fz = depth * 0.5f;
    float zOffset = 0.5f;

    gfx::dx::XMFLOAT3 pos{};
    std::size_t i = 0;

    //ⓐ 앞면(Front) 사각형의 위쪽 삼각형
    pos = gfx::dx::XMFLOAT3(-fx, +fy, zOffset-fz);
    std::memcpy(vbPosMem.data() + sizeof(gfx::dx::XMFLOAT3) * i++, &pos, sizeof(gfx::dx::XMFLOAT3));
    pos = gfx::dx::XMFLOAT3(+fx, -fy, zOffset-fz);
    std::memcpy(vbPosMem.data() + sizeof(gfx::dx::XMFLOAT3) * i++, &pos, sizeof(gfx::dx::XMFLOAT3));
    pos = gfx::dx::XMFLOAT3(+fx, +fy, zOffset-fz);
    std::memcpy(vbPosMem.data() + sizeof(gfx::dx::XMFLOAT3) * i++, &pos, sizeof(gfx::dx::XMFLOAT3));
    //ⓑ 앞면(Front) 사각형의 아래쪽 삼각형
    pos = gfx::dx::XMFLOAT3(+fx, -fy, zOffset-fz);
    std::memcpy(vbPosMem.data() + sizeof(gfx::dx::XMFLOAT3) * i++, &pos, sizeof(gfx::dx::XMFLOAT3));
    pos = gfx::dx::XMFLOAT3(-fx, +fy, zOffset-fz);
    std::memcpy(vbPosMem.data() + sizeof(gfx::dx::XMFLOAT3) * i++, &pos, sizeof(gfx::dx::XMFLOAT3));
    pos = gfx::dx::XMFLOAT3(-fx, -fy, zOffset-fz);
    std::memcpy(vbPosMem.data() + sizeof(gfx::dx::XMFLOAT3) * i++, &pos, sizeof(gfx::dx::XMFLOAT3));
    //ⓒ 윗면(Top) 사각형의 위쪽 삼각형
    pos = gfx::dx::XMFLOAT3(-fx, +fy, zOffset+fz);
    std::memcpy(vbPosMem.data() + sizeof(gfx::dx::XMFLOAT3) * i++, &pos, sizeof(gfx::dx::XMFLOAT3));
    pos = gfx::dx::XMFLOAT3(+fx, +fy, zOffset+fz);
    std::memcpy(vbPosMem.data() + sizeof(gfx::dx::XMFLOAT3) * i++, &pos, sizeof(gfx::dx::XMFLOAT3));
    pos = gfx::dx::XMFLOAT3(+fx, +fy, zOffset-fz);
    std::memcpy(vbPosMem.data() + sizeof(gfx::dx::XMFLOAT3) * i++, &pos, sizeof(gfx::dx::XMFLOAT3));
    //ⓓ 윗면(Top) 사각형의 아래쪽 삼각형
    pos = gfx::dx::XMFLOAT3(-fx, +fy, zOffset+fz);
    std::memcpy(vbPosMem.data() + sizeof(gfx::dx::XMFLOAT3) * i++, &pos, sizeof(gfx::dx::XMFLOAT3));
    pos = gfx::dx::XMFLOAT3(+fx, +fy, zOffset-fz);
    std::memcpy(vbPosMem.data() + sizeof(gfx::dx::XMFLOAT3) * i++, &pos, sizeof(gfx::dx::XMFLOAT3));
    pos = gfx::dx::XMFLOAT3(-fx, +fy, zOffset-fz);
    std::memcpy(vbPosMem.data() + sizeof(gfx::dx::XMFLOAT3) * i++, &pos, sizeof(gfx::dx::XMFLOAT3));
    //ⓔ 뒷면(Back) 사각형의 위쪽 삼각형
    pos = gfx::dx::XMFLOAT3(-fx, -fy, zOffset+fz);
    std::memcpy(vbPosMem.data() + sizeof(gfx::dx::XMFLOAT3) * i++, &pos, sizeof(gfx::dx::XMFLOAT3));
    pos = gfx::dx::XMFLOAT3(+fx, -fy, zOffset+fz);
    std::memcpy(vbPosMem.data() + sizeof(gfx::dx::XMFLOAT3) * i++, &pos, sizeof(gfx::dx::XMFLOAT3));
    pos = gfx::dx::XMFLOAT3(+fx, +fy, zOffset+fz);
    std::memcpy(vbPosMem.data() + sizeof(gfx::dx::XMFLOAT3) * i++, &pos, sizeof(gfx::dx::XMFLOAT3));
    //ⓕ 뒷면(Back) 사각형의 아래쪽 삼각형
    pos = gfx::dx::XMFLOAT3(-fx, -fy, zOffset+fz);
    std::memcpy(vbPosMem.data() + sizeof(gfx::dx::XMFLOAT3) * i++, &pos, sizeof(gfx::dx::XMFLOAT3));
    pos = gfx::dx::XMFLOAT3(+fx, +fy, zOffset+fz);
    std::memcpy(vbPosMem.data() + sizeof(gfx::dx::XMFLOAT3) * i++, &pos, sizeof(gfx::dx::XMFLOAT3));
    pos = gfx::dx::XMFLOAT3(-fx, +fy, zOffset+fz);
    std::memcpy(vbPosMem.data() + sizeof(gfx::dx::XMFLOAT3) * i++, &pos, sizeof(gfx::dx::XMFLOAT3));
    //ⓖ 아래면(Bottom) 사각형의 위쪽 삼각형
    pos = gfx::dx::XMFLOAT3(-fx, -fy, zOffset-fz);
    std::memcpy(vbPosMem.data() + sizeof(gfx::dx::XMFLOAT3) * i++, &pos, sizeof(gfx::dx::XMFLOAT3));
    pos = gfx::dx::XMFLOAT3(+fx, -fy, zOffset-fz);
    std::memcpy(vbPosMem.data() + sizeof(gfx::dx::XMFLOAT3) * i++, &pos, sizeof(gfx::dx::XMFLOAT3));
    pos = gfx::dx::XMFLOAT3(+fx, -fy, zOffset+fz);
    std::memcpy(vbPosMem.data() + sizeof(gfx::dx::XMFLOAT3) * i++, &pos, sizeof(gfx::dx::XMFLOAT3));
    //ⓗ 아래면(Bottom) 사각형의 아래쪽 삼각형
    pos = gfx::dx::XMFLOAT3(-fx, -fy, zOffset-fz);
    std::memcpy(vbPosMem.data() + sizeof(gfx::dx::XMFLOAT3) * i++, &pos, sizeof(gfx::dx::XMFLOAT3));
    pos = gfx::dx::XMFLOAT3(+fx, -fy, zOffset+fz);
    std::memcpy(vbPosMem.data() + sizeof(gfx::dx::XMFLOAT3) * i++, &pos, sizeof(gfx::dx::XMFLOAT3));
    pos = gfx::dx::XMFLOAT3(-fx, -fy, zOffset+fz);
    std::memcpy(vbPosMem.data() + sizeof(gfx::dx::XMFLOAT3) * i++, &pos, sizeof(gfx::dx::XMFLOAT3));
    //ⓘ 옆면(Left) 사각형의 위쪽 삼각형
    pos = gfx::dx::XMFLOAT3(-fx, +fy, zOffset+fz);
    std::memcpy(vbPosMem.data() + sizeof(gfx::dx::XMFLOAT3) * i++, &pos, sizeof(gfx::dx::XMFLOAT3));
    pos = gfx::dx::XMFLOAT3(-fx, +fy, zOffset-fz);
    std::memcpy(vbPosMem.data() + sizeof(gfx::dx::XMFLOAT3) * i++, &pos, sizeof(gfx::dx::XMFLOAT3));
    pos = gfx::dx::XMFLOAT3(-fx, -fy, zOffset-fz);
    std::memcpy(vbPosMem.data() + sizeof(gfx::dx::XMFLOAT3) * i++, &pos, sizeof(gfx::dx::XMFLOAT3));
    //ⓙ 옆면(Left) 사각형의 아래쪽 삼각형
    pos = gfx::dx::XMFLOAT3(-fx, +fy, zOffset+fz);
    std::memcpy(vbPosMem.data() + sizeof(gfx::dx::XMFLOAT3) * i++, &pos, sizeof(gfx::dx::XMFLOAT3));
    pos = gfx::dx::XMFLOAT3(-fx, -fy, zOffset-fz);
    std::memcpy(vbPosMem.data() + sizeof(gfx::dx::XMFLOAT3) * i++, &pos, sizeof(gfx::dx::XMFLOAT3));
    pos = gfx::dx::XMFLOAT3(-fx, -fy, zOffset+fz);
    std::memcpy(vbPosMem.data() + sizeof(gfx::dx::XMFLOAT3) * i++, &pos, sizeof(gfx::dx::XMFLOAT3));
    //ⓚ 옆면(Right) 사각형의 위쪽 삼각형
    pos = gfx::dx::XMFLOAT3(+fx, +fy, zOffset-fz);
    std::memcpy(vbPosMem.data() + sizeof(gfx::dx::XMFLOAT3) * i++, &pos, sizeof(gfx::dx::XMFLOAT3));
    pos = gfx::dx::XMFLOAT3(+fx, +fy, zOffset+fz);
    std::memcpy(vbPosMem.data() + sizeof(gfx::dx::XMFLOAT3) * i++, &pos, sizeof(gfx::dx::XMFLOAT3));
    pos = gfx::dx::XMFLOAT3(+fx, -fy, zOffset+fz);
    std::memcpy(vbPosMem.data() + sizeof(gfx::dx::XMFLOAT3) * i++, &pos, sizeof(gfx::dx::XMFLOAT3));
    //ⓛ 옆면(Right) 사각형의 아래쪽 삼각형
    pos = gfx::dx::XMFLOAT3(+fx, +fy, zOffset-fz);
    std::memcpy(vbPosMem.data() + sizeof(gfx::dx::XMFLOAT3) * i++, &pos, sizeof(gfx::dx::XMFLOAT3));
    pos = gfx::dx::XMFLOAT3(+fx, -fy, zOffset+fz);
    std::memcpy(vbPosMem.data() + sizeof(gfx::dx::XMFLOAT3) * i++, &pos, sizeof(gfx::dx::XMFLOAT3));
    pos = gfx::dx::XMFLOAT3(+fx, -fy, zOffset-fz);
    std::memcpy(vbPosMem.data() + sizeof(gfx::dx::XMFLOAT3) * i++, &pos, sizeof(gfx::dx::XMFLOAT3));

    refMesh.vbLayouts_.emplace_back();
    auto& primaryVBs = refMesh.vbLayouts_.back();

    const auto vbPosMemSize = vbPosMem.size();

    primaryVBs.emplace_back(device, cmdList, std::move(vbPosMem),
        vbPosMemSize, sizeof(dx::XMFLOAT3),
        std::bitset<etoi(gfx::Vertex::Properties::SIZE)>{ 1ull << etoi(gfx::Vertex::Properties::Position3D) }
    );


    auto indexIbMem = std::vector<std::uint8_t>(sizeof(std::uint16_t) * 6 * 6);

    for (int i = 0; i < 36; ++i) {
        *reinterpret_cast<std::uint16_t*>(indexIbMem.data() + sizeof(std::uint16_t) * i) = i;
    }

    auto refSubmesh = gfx::d3d12::RefSubmesh(&refMesh);
    refSubmesh.topology_ = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    refSubmesh.ib_ = gfx::d3d12::IndexBuffer(device, cmdList, std::move(indexIbMem), DXGI_FORMAT_R16_UINT, 36u);

    refMesh.submeshes_.push_back(std::move(refSubmesh));

    pRoot_->addMesh(std::move(refMesh));
}
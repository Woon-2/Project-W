#include "d3d12model.hpp"

namespace gfx {

namespace d3d12 {

Model::Model( d3d12::Core& core, d3d12::D3D12RenderContext& ctx, const gfx::Model& model,
    Core::UpBufIdx vbUpIdx, Core::UpBufIdx ibUpIdx, std::size_t vbSerialIdx, std::size_t ibSerialIdx
) : meshes_(), children_(), name_(model.name()), vbSerialIdx_(vbSerialIdx), ibSerialIdx_(ibSerialIdx) {
    for (const auto& [mesh, name] : model.meshes()) {
        meshes_.emplace_back( Mesh( core, ctx, mesh,
            serializeVbIdx(vbUpIdx), serializeIbIdx(ibUpIdx)
        ), name );
    }
    for (const auto& child : model.children()) {
        children_.push_back(Model(core, ctx, child, vbUpIdx, ibUpIdx, vbSerialIdx_, ibSerialIdx_));
    }
}

void Model::bind(ID3D12GraphicsCommandList* pCmdList) const {
    for (const auto& mesh : meshes_) {
        mesh.mesh.bind(pCmdList);
    }
    for (const auto& child : children_) {
        child.bind(pCmdList);
    }
}

void Model::draw(ID3D12GraphicsCommandList* pCmdList) const {
    for (const auto& mesh : meshes_) {
        mesh.mesh.draw(pCmdList);
    }
    for (const auto& child : children_) {
        child.draw(pCmdList);
    }
}

Core::UpBufIdx Model::serializeVbIdx(const Core::UpBufIdx& idx) {
    return idx + std::to_string(vbSerialIdx_++);
}

Core::UpBufIdx Model::serializeIbIdx(const Core::UpBufIdx& idx) {
    return idx + std::to_string(ibSerialIdx_++);
}

Model Model::popChild(std::string_view name) {
    auto retPos = std::remove_if( children_.begin(), children_.end(),
        [&name](const Model& child) {
            return child.name() == name;
        }
    );

    if (retPos == children_.end()) {
        throw std::runtime_error("Child not found");
    }

    auto ret = std::move(*retPos);

    children_.erase(retPos, children_.end());

    return ret;
}

const Model& Model::child(std::string_view name) const {
    auto it = std::find_if( children_.begin(), children_.end(), [&name](const Model& child) {
        return child.name() == name;
    } );

    if (it == children_.end()) {
        throw std::runtime_error("Child not found");
    }

    return *it;
}

Mesh Model::popMesh(std::string_view name) {
    auto retPos = std::remove_if( meshes_.begin(), meshes_.end(),
        [&name](const NamedMesh& nm) {
            return nm.name == name;
        }
    );

    if (retPos == meshes_.end()) {
        throw std::runtime_error("Mesh not found");
    }

    auto ret = std::move(retPos->mesh);

    meshes_.erase(retPos, meshes_.end());

    return ret;
}

const Mesh& Model::mesh(std::string_view name) const {
    auto it = std::find_if( meshes_.begin(), meshes_.end(), [&name](const NamedMesh& nm) {
        return nm.name == name;
    } );

    if (it == meshes_.end()) {
        throw std::runtime_error("Mesh not found");
    }

    return it->mesh;
}


}   // namespace d3d12

}   // namespace gfx
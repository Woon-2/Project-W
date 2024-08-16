#include "d3d12model.hpp"

namespace gfx {

namespace d3d12 {

Model::Model( d3d12::Core& core, d3d12::D3D12RenderContext& ctx, const gfx::Model& model,
    Core::UpBufIdx vbUpIdx, Core::UpBufIdx ibUpIdx,
    std::shared_ptr<std::size_t> pVbSerialIdx, std::shared_ptr<std::size_t> pIbSerialIdx
) : coordSys_(model.coord()), meshes_(), children_(), name_(model.name()), pVbSerialIdx_(pVbSerialIdx), pIbSerialIdx_(pIbSerialIdx) {
    for (const auto& [mesh, name] : model.meshes()) {
        meshes_.emplace_back( Mesh( core, ctx, mesh,
            serializeVbIdx(vbUpIdx), serializeIbIdx(ibUpIdx)
        ), name );
    }
    for (const auto& child : model.children()) {

        auto mod = Model(core, ctx, child, vbUpIdx, ibUpIdx, pVbSerialIdx_, pIbSerialIdx_);
        children_.push_back(std::move(mod));
        children_.back().coord().setParent(&coordSys_);
    }
}

void Model::completeInit(d3d12::Core& core) const {
    for (const auto& mesh : meshes_) {
        mesh.mesh.completeInit(core);
    }
    for (const auto& child : children_) {
        child.completeInit(core);
    }
}

Core::UpBufIdx Model::serializeVbIdx(const Core::UpBufIdx& idx) {
    return idx + std::to_string((*pVbSerialIdx_)++);
}

Core::UpBufIdx Model::serializeIbIdx(const Core::UpBufIdx& idx) {
    return idx + std::to_string((*pIbSerialIdx_)++);
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
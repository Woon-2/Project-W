#ifndef __AssimpLoaderD3d12_HPP
#define __AssimpLoaderD3d12_HPP

#include "assimpLoader.hpp"

#include "inputLayout.hpp"

#include "d3d12core.hpp"
#include "d3d12mesh.hpp"
#include "d3d12model.hpp"

namespace gfx {

namespace d3d12 {

class AssimpLoader : public gfx::AssimpLoader {
public:
    Mesh buildMesh(Core& core, D3D12RenderContext& ctx) const {
        return buildMesh(core, ctx, scene()->mMeshes[0]);
    }
    Mesh buildMesh(Core& core, D3D12RenderContext& ctx, const gfx::InputLayout& inputLayout) const {
        return buildMesh(core, ctx, scene()->mMeshes[0], inputLayout);
    }
    Mesh buildMesh(Core& core, D3D12RenderContext& ctx, const aiMesh* mesh) const {
        return Mesh(core, ctx, buildGeneralMesh(mesh));
    }
    Mesh buildMesh(Core& core, D3D12RenderContext& ctx, const aiMesh* mesh, const gfx::InputLayout& inputLayout) const {
        return Mesh(core, ctx, buildGeneralMesh(mesh, inputLayout));
    }
    Model buildModel(Core& core, D3D12RenderContext& ctx) const {
        auto model = Model(scene()->mRootNode->mName.C_Str());
        processAiNode(core, ctx, scene()->mRootNode, scene(), model);
        return model;
    }
    Model buildModel(Core& core, D3D12RenderContext& ctx, const gfx::InputLayout& inputLayout) const {
        auto model = Model(scene()->mRootNode->mName.C_Str());
        processAiNode(core, ctx, scene()->mRootNode, scene(), model, inputLayout);
        return model;
    }

private:
    void processAiNode(Core& core, D3D12RenderContext& ctx, const aiNode* node, const aiScene* scene, Model& model) const;
    void processAiNode(Core& core, D3D12RenderContext& ctx, const aiNode* node, const aiScene* scene, Model& model, const gfx::InputLayout& inputLayout) const;
};

}   // namespace gfx::d3d12

}   // namespace gfx

#endif // __AssimpLoaderD3d12_HPP
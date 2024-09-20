#include "assimpLoaderD3d12.hpp"

namespace gfx {

namespace d3d12 {

void AssimpLoader::processAiNode( Core& core, D3D12RenderContext& ctx,
    const aiNode* node, const aiScene* scene, Model& model
) const {
    for (auto i = 0u; i < node->mNumMeshes; ++i) {
        auto aimesh = scene->mMeshes[node->mMeshes[i]];

        auto mesh = buildGeneralMesh(aimesh);
        if (mesh.vb().byteWidth() != 0 && mesh.ib().size() != 0) {
            model.addMesh(Mesh(core, ctx, mesh), aimesh->mName.C_Str());
        }
    }

    for (auto i = 0u; i < node->mNumChildren; ++i) {
        auto aiChild = node->mChildren[i];
        auto child = Model(aiChild->mName.C_Str());
        processAiNode(core, ctx, aiChild, scene, child);
        if (child.meshes().size() > 0) {
            model.addChild(std::move(child), aiChild->mTransformation);
        }
    }
}

void AssimpLoader::processAiNode( Core& core, D3D12RenderContext& ctx,
    const aiNode* node, const aiScene* scene, Model& model, const gfx::InputLayout& inputLayout
) const {
    for (auto i = 0u; i < node->mNumMeshes; ++i) {
        auto aimesh = scene->mMeshes[node->mMeshes[i]];

        auto mesh = buildGeneralMesh(aimesh, inputLayout);

        for (const auto& elem : inputLayout) {
            if (!mesh.vb().contains(elem.prop)) {
                throw std::runtime_error("Input layout mismatch");
            }
        }

        if (mesh.vb().byteWidth() != 0 && mesh.ib().size() != 0) {
            model.addMesh(Mesh(core, ctx, mesh), aimesh->mName.C_Str());
        }
    }

    for (auto i = 0u; i < node->mNumChildren; ++i) {
        auto aiChild = node->mChildren[i];
        auto child = Model(aiChild->mName.C_Str());
        processAiNode(core, ctx, aiChild, scene, child, inputLayout);
        if (child.meshes().size() > 0) {
            model.addChild(std::move(child), aiChild->mTransformation);
        }
    }
}

}   // namespace d3d12

}   // namespace gfx
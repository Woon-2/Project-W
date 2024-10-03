#ifndef __AssimpLoaderD3d12_HPP
#define __AssimpLoaderD3d12_HPP

#include "assimpLoader.hpp"

#include "inputLayout.hpp"

#include "d3d12core.hpp"
#include "d3d12mesh.hpp"
#include "d3d12model.hpp"
#include "d3d12texture.hpp"
#include "d3d12materialTree.hpp"

#include <map>
#include <filesystem>

namespace gfx {

namespace d3d12 {

class AssimpLoader : public gfx::AssimpLoader {
public:
    using PathTexMap = std::map<std::filesystem::path, const Texture*>;

    Mesh buildMesh(Core& core, D3D12RenderContext& ctx) const {
        return buildMesh(core, ctx, scene()->mMeshes[0]);
    }

    Mesh buildMesh(Core& core, D3D12RenderContext& ctx, const gfx::InputLayout& inputLayout) const {
        return buildMesh(core, ctx, scene()->mMeshes[0], inputLayout);
    }

    Mesh buildMesh(Core& core, D3D12RenderContext& ctx, const aiMesh* mesh) const {
        return Mesh( core, ctx, buildGeneralMesh(
            VBFlags{
                etoi(Vertex::Properties::Position3D),
                etoi(Vertex::Properties::Normal3D),
                etoi(Vertex::Properties::TexCoord2D0),
                etoi(Vertex::Properties::Tangent3D),
                etoi(Vertex::Properties::Bitangent3D),
                etoi(Vertex::Properties::Color3D),
                etoi(Vertex::Properties::Color4D)
            },
        mesh ) );
    }

    Mesh buildMesh(Core& core, D3D12RenderContext& ctx, const aiMesh* mesh, const gfx::InputLayout& inputLayout) const {
        return Mesh( core, ctx, buildGeneralMesh( inputLayout.flags(), mesh ) );
    }

    Model buildModel(Core& core, D3D12RenderContext& ctx) const {
        auto model = Model(scene()->mRootNode->mName.C_Str());
        processAiNode(core, ctx, scene()->mRootNode, scene(), model);
        model.coord().traverse();
        return model;
    }

    Model buildModel(Core& core, D3D12RenderContext& ctx, const gfx::InputLayout& inputLayout) const {
        auto model = Model(scene()->mRootNode->mName.C_Str());
        processAiNode(core, ctx, scene()->mRootNode, scene(), model, inputLayout);
        model.coord().traverse();
        return model;
    }

    Material buildMaterial(Core& core, const PathTexMap& pathTexmap) const {
        return buildMaterial(core, pathTexmap, scene()->mMaterials[0]);
    }

    Material buildMaterial(Core& core, const PathTexMap& pathTexmap, const aiMaterial* material) const;

    MaterialTree buildMaterialTree(Core& core, const PathTexMap& pathTexmap) const {
        auto matTree = MaterialTree();
        processAiNodeMaterial(core, pathTexmap, scene()->mRootNode, scene(), matTree);
        return matTree;
    }

private:
    void processAiNode(Core& core, D3D12RenderContext& ctx, const aiNode* node, const aiScene* scene, Model& model) const;
    void processAiNode(Core& core, D3D12RenderContext& ctx, const aiNode* node, const aiScene* scene, Model& model, const gfx::InputLayout& inputLayout) const;
    void processAiNodeMaterial(Core& core, const PathTexMap& pathTexmap, const aiNode* node, const aiScene* scene, MaterialTree& matTree) const;
};

}   // namespace gfx::d3d12

}   // namespace gfx

#endif // __AssimpLoaderD3d12_HPP
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

    std::optional<Mesh> buildMesh(Core& core, D3D12RenderContext& ctx) const {
        return buildMesh(core, ctx, scene()->mMeshes[0]);
    }

    std::optional<Mesh> buildMesh(Core& core, D3D12RenderContext& ctx, const aiMesh* mesh) const {
        return buildMesh( core, ctx, mesh, VBFlags{
            1 << etoi(Vertex::Properties::Position3D),
            1 << etoi(Vertex::Properties::Normal3D),
            1 << etoi(Vertex::Properties::TexCoord2D0),
            1 << etoi(Vertex::Properties::Tangent3D),
            1 << etoi(Vertex::Properties::Bitangent3D),
            1 << etoi(Vertex::Properties::Color3D),
            1 << etoi(Vertex::Properties::Color4D)
        } );
    }

    std::optional<Mesh> buildMesh(Core& core, D3D12RenderContext& ctx, const aiMesh* mesh, const VBFlags& flags) const {
        auto gm = buildGeneralMesh( flags, mesh );
        if (gm.vbs().empty() || gm.ib().empty()) {
            return std::nullopt;
        }
        return Mesh( core, ctx, std::move(gm) );
    }

    Model buildModel(Core& core, D3D12RenderContext& ctx) const {
        return buildModel( core, ctx, VBFlags{
            1 << etoi(Vertex::Properties::Position3D),
            1 << etoi(Vertex::Properties::Normal3D),
            1 << etoi(Vertex::Properties::TexCoord2D0),
            1 << etoi(Vertex::Properties::Tangent3D),
            1 << etoi(Vertex::Properties::Bitangent3D),
            1 << etoi(Vertex::Properties::Color3D),
            1 << etoi(Vertex::Properties::Color4D)
        } );
    }

    Model buildModel(Core& core, D3D12RenderContext& ctx, const VBFlags& vbFlags) const {
        auto model = Model(scene()->mRootNode->mName.C_Str());
        processAiNode(core, ctx, scene()->mRootNode, scene(), model, vbFlags);
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
    void processAiNode(Core& core, D3D12RenderContext& ctx, const aiNode* node, const aiScene* scene, Model& model) const {
        processAiNode( core, ctx, node, scene, model, VBFlags{
            1 << etoi(Vertex::Properties::Position3D),
            1 << etoi(Vertex::Properties::Normal3D),
            1 << etoi(Vertex::Properties::TexCoord2D0),
            1 << etoi(Vertex::Properties::Tangent3D),
            1 << etoi(Vertex::Properties::Bitangent3D),
            1 << etoi(Vertex::Properties::Color3D),
            1 << etoi(Vertex::Properties::Color4D)
        } );
    }
    void processAiNode(Core& core, D3D12RenderContext& ctx, const aiNode* node, const aiScene* scene, Model& model, const VBFlags& flags) const;
    void processAiNodeMaterial(Core& core, const PathTexMap& pathTexmap, const aiNode* node, const aiScene* scene, MaterialTree& matTree) const;
};

}   // namespace gfx::d3d12

}   // namespace gfx

#endif // __AssimpLoaderD3d12_HPP
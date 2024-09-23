#include "assimpLoaderD3d12.hpp"

namespace gfx {

namespace d3d12 {

Material AssimpLoader::buildMaterial( Core& core, const PathTexMap& pathTexmap,
    const aiMaterial* material
) const {
    Material mat;

    aiColor3D color;
    if (AI_SUCCESS == material->Get(AI_MATKEY_COLOR_DIFFUSE, color)) {
        mat.pushProperty( Material::Properties::DiffuseColor, Material::Property{
            .vec4 = mu::Vec4(color.r, color.g, color.b, 1.0f)
        } );
    }
    if (AI_SUCCESS == material->Get(AI_MATKEY_COLOR_AMBIENT, color)) {
        mat.pushProperty( Material::Properties::AmbientColor, Material::Property{
            .vec4 = mu::Vec4(color.r, color.g, color.b, 1.0f)
        } );
    }
    if (AI_SUCCESS == material->Get(AI_MATKEY_COLOR_SPECULAR, color)) {
        mat.pushProperty( Material::Properties::SpecularColor, Material::Property{
            .vec4 = mu::Vec4(color.r, color.g, color.b, 1.0f)
        } );
    }
    if (AI_SUCCESS == material->Get(AI_MATKEY_COLOR_EMISSIVE, color)) {
        mat.pushProperty( Material::Properties::EmissiveColor, Material::Property{
            .vec4 = mu::Vec4(color.r, color.g, color.b, 1.0f)
        } );
    }

    float shininess;
    if (AI_SUCCESS == material->Get(AI_MATKEY_SHININESS, shininess)) {
        mat.pushProperty( Material::Properties::Shininess, Material::Property{
            .scalar = shininess
        } );
    }

    float opacity;
    if (AI_SUCCESS == material->Get(AI_MATKEY_OPACITY, opacity)) {
        mat.pushProperty( Material::Properties::Opacity, Material::Property{
            .scalar = opacity
        } );
    }

    aiString path;
    if (AI_SUCCESS == material->GetTexture(aiTextureType_DIFFUSE, 0, &path)) {
        if (auto tex = pathTexmap.find(path.C_Str()); tex != pathTexmap.end()) {
            mat.pushTexture(core, Material::Maps::Diffuse, *tex->second);
        }
    }

    if (AI_SUCCESS == material->GetTexture(aiTextureType_AMBIENT, 0, &path)) {
        if (auto tex = pathTexmap.find(path.C_Str()); tex != pathTexmap.end()) {
            mat.pushTexture(core, Material::Maps::Ambient, *tex->second);
        }
    }

    if (AI_SUCCESS == material->GetTexture(aiTextureType_SPECULAR, 0, &path)) {
        if (auto tex = pathTexmap.find(path.C_Str()); tex != pathTexmap.end()) {
            mat.pushTexture(core, Material::Maps::Specular, *tex->second);
        }
    }

    if (AI_SUCCESS == material->GetTexture(aiTextureType_EMISSIVE, 0, &path)) {
        if (auto tex = pathTexmap.find(path.C_Str()); tex != pathTexmap.end()) {
            mat.pushTexture(core, Material::Maps::Emissive, *tex->second);
        }
    }

    if (AI_SUCCESS == material->GetTexture(aiTextureType_NORMALS, 0, &path)) {
        if (auto tex = pathTexmap.find(path.C_Str()); tex != pathTexmap.end()) {
            mat.pushTexture(core, Material::Maps::Normal, *tex->second);
        }
    }

    if (AI_SUCCESS == material->GetTexture(aiTextureType_DIFFUSE_ROUGHNESS, 0, &path)) {
        if (auto tex = pathTexmap.find(path.C_Str()); tex != pathTexmap.end()) {
            mat.pushTexture(core, Material::Maps::Roughness, *tex->second);
        }
    }

    if (AI_SUCCESS == material->GetTexture(aiTextureType_METALNESS, 0, &path)) {
        if (auto tex = pathTexmap.find(path.C_Str()); tex != pathTexmap.end()) {
            mat.pushTexture(core, Material::Maps::Metallic, *tex->second);
        }
    }

    if (AI_SUCCESS == material->GetTexture(aiTextureType_HEIGHT, 0, &path)) {
        if (auto tex = pathTexmap.find(path.C_Str()); tex != pathTexmap.end()) {
            mat.pushTexture(core, Material::Maps::Height, *tex->second);
        }
    }

    if (AI_SUCCESS == material->GetTexture(aiTextureType_OPACITY, 0, &path)) {
        if (auto tex = pathTexmap.find(path.C_Str()); tex != pathTexmap.end()) {
            mat.pushTexture(core, Material::Maps::Opacity, *tex->second);
        }
    }

    return mat;
}

void AssimpLoader::processAiNode( Core& core, D3D12RenderContext& ctx,
    const aiNode* node, const aiScene* scene, Model& model
) const {
    for (auto i = 0u; i < node->mNumMeshes; ++i) {
        auto aimesh = scene->mMeshes[node->mMeshes[i]];

        if (aimesh->mNumVertices == 0 || aimesh->mNumFaces == 0) {
            continue;
        }
        
        model.addMesh(Mesh(core, ctx, buildGeneralMesh(aimesh)), aimesh->mName.C_Str());
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

        if (aimesh->mNumVertices == 0 || aimesh->mNumFaces == 0) {
            continue;
        }

        auto mesh = buildGeneralMesh(aimesh, inputLayout);

        for (const auto& elem : inputLayout) {
            if (!mesh.vb().contains(elem.prop)) {
                throw std::runtime_error("Input layout mismatch");
            }
        }
        
        model.addMesh(Mesh(core, ctx, mesh), aimesh->mName.C_Str());
    }

    for (auto i = 0u; i < node->mNumChildren; ++i) {
        auto aiChild = node->mChildren[i];
        auto child = Model(aiChild->mName.C_Str());
        processAiNode(core, ctx, aiChild, scene, child, inputLayout);
        model.addChild(std::move(child), aiChild->mTransformation);
    }
}

void AssimpLoader::processAiNodeMaterial( Core& core, const PathTexMap& pathTexmap, const aiNode* node,
    const aiScene* scene, MaterialTree& matTree
) const {
    for (auto i = 0u; i < node->mNumMeshes; ++i) {
        auto aimesh = scene->mMeshes[node->mMeshes[i]];

        if (aimesh->mNumVertices == 0 || aimesh->mNumFaces == 0) {
            continue;
        }

        auto aimaterial = scene->mMaterials[aimesh->mMaterialIndex];
        matTree.addMaterial( buildMaterial(core, pathTexmap, aimaterial) );
    }

    for (auto i = 0u; i < node->mNumChildren; ++i) {
        auto aiChild = node->mChildren[i];
        auto child = MaterialTree();
        processAiNodeMaterial(core, pathTexmap, aiChild, scene, child);
        matTree.addChild(std::move(child));
    }
}

}   // namespace d3d12

}   // namespace gfx
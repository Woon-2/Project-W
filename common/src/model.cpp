#include "model.hpp"

#include "assimp/Importer.hpp"
#include "assimp/scene.h"
#include "assimp/postprocess.h"

namespace gfx {

const Model& Model::child(std::string_view name) const {
    auto it = std::find_if( children_.begin(), children_.end(), [&name](const Model* child) {
        return child->name() == name;
    } );

    if (it == children_.end()) {
        throw std::runtime_error("Child not found");
    }

    return **it;
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

namespace {

void processAiNode(const aiNode* node, const aiScene* scene, Model& model) {
    for (auto i = 0u; i < node->mNumMeshes; ++i) {
        model.addMesh( detail::getMeshFromAiNode(node, scene, i),
            scene->mMeshes[node->mMeshes[i]]->mName.C_Str()
        );
        model.coord() << node->mTransformation;
    }

    for (auto i = 0u; i < node->mNumChildren; ++i) {
        auto child = Model(node->mChildren[i]->mName.C_Str());
        processAiNode(node->mChildren[i], scene, child);
        model.addChild(child);
    }
}

void processAiNode(const aiNode* node, const aiScene* scene, Model& model, const InputLayout& il) {
    for (auto i = 0u; i < node->mNumMeshes; ++i) {
        auto tmpMesh = detail::getMeshFromAiNode(node, scene, i);
        model.addMesh( Mesh( convert(tmpMesh.vb(), il), std::move(tmpMesh.ib()) ),
            scene->mMeshes[node->mMeshes[i]]->mName.C_Str()
        );
    }

    for (auto i = 0u; i < node->mNumChildren; ++i) {
        auto child = Model(node->mChildren[i]->mName.C_Str());
        processAiNode(node->mChildren[i], scene, child, il);
        model.addChild(child);
    }
}

}   // gfx::anonymus_namespace

Model loadModel(const std::filesystem::path& path) {
    auto importer = Assimp::Importer();

    auto flag = aiProcess_Triangulate | aiProcess_JoinIdenticalVertices
        | aiProcess_MakeLeftHanded;

    auto scene = importer.ReadFile(path.string(), flag);

    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
        throw;  // TODO: add exception
    }

    auto model = Model(scene->mRootNode->mName.C_Str());

    processAiNode(scene->mRootNode, scene, model);

    return model;
}

Model loadModel(const std::filesystem::path& path, const InputLayout& il) {
    auto importer = Assimp::Importer();

    auto flag = aiProcess_Triangulate | aiProcess_JoinIdenticalVertices
        | aiProcess_MakeLeftHanded;

    auto scene = importer.ReadFile(path.string(), flag);

    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
        throw;  // TODO: add exception
    }

    auto model = Model(scene->mRootNode->mName.C_Str());

    processAiNode(scene->mRootNode, scene, model, il);

    return model;
}

}   // namespace gfx
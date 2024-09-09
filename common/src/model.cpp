#include "model.hpp"

#include "assimp/Importer.hpp"
#include "assimp/scene.h"
#include "assimp/postprocess.h"

namespace gfx {

const Model& Model::child(std::string_view name) const {
    auto it = std::find_if( children_.begin(), children_.end(), [&name](const Model& child) {
        return child.name() == name;
    } );

    if (it == children_.end()) {
        throw std::runtime_error("Child not found");
    }

    return *it;
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

namespace {

void processAiNode(const aiNode* node, const aiScene* scene, Model& model) {
    for (auto i = 0u; i < node->mNumMeshes; ++i) {
        model.addMesh( detail::getMeshFromAiNode(node, scene, i),
            scene->mMeshes[node->mMeshes[i]]->mName.C_Str()
        );
    }

    for (auto i = 0u; i < node->mNumChildren; ++i) {
        auto child = Model(node->mChildren[i]->mName.C_Str());
        processAiNode(node->mChildren[i], scene, child);
        model.addChild(std::move(child), node->mChildren[i]->mTransformation);
    }
}

void processAiNode(const aiNode* node, const aiScene* scene, Model& model, const InputLayout& il) {
    for (auto i = 0u; i < node->mNumMeshes; ++i) {
        auto tmpMesh = detail::getMeshFromAiNode(node, scene, i);

        for (const auto& elem : il) {
            if (!tmpMesh.vb().contains(elem.prop)) {
                throw std::runtime_error("Input layout mismatch");
            }
        }

        if (tmpMesh.vb().byteWidth() != 0 && tmpMesh.ib().size() != 0) {
            model.addMesh( Mesh( convert(tmpMesh.vb(), il), std::move(tmpMesh.ib()) ),
                scene->mMeshes[node->mMeshes[i]]->mName.C_Str()
            );
        }
    }

    for (auto i = 0u; i < node->mNumChildren; ++i) {
        auto child = Model(node->mChildren[i]->mName.C_Str());
        processAiNode(node->mChildren[i], scene, child, il);
        if (child.meshes().size() > 0) {
            model.addChild(std::move(child), node->mChildren[i]->mTransformation);
        }
    }
}

}   // gfx::anonymus_namespace

Model loadModel(const std::filesystem::path& path) {
    auto importer = Assimp::Importer();

    auto flag = aiProcess_Triangulate | aiProcess_JoinIdenticalVertices
        | aiProcess_ConvertToLeftHanded | aiProcess_SortByPType;

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
        | aiProcess_ConvertToLeftHanded | aiProcess_SortByPType;

    auto scene = importer.ReadFile(path.string(), flag);

    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
        throw;  // TODO: add exception
    }

    auto model = Model(scene->mRootNode->mName.C_Str());

    processAiNode(scene->mRootNode, scene, model, il);

    return model;
}

}   // namespace gfx
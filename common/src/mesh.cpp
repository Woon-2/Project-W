#include "mesh.hpp"

#include "assimp/Importer.hpp"
#include "assimp/postprocess.h"

#include <iostream>

namespace gfx {

namespace detail {

Mesh getMeshFromAiNode(const aiNode* node, const aiScene* scene, unsigned int meshIdx) {
    auto mesh = scene->mMeshes[node->mMeshes[meshIdx]];
    auto vb = VertexBuffer();
    auto ib = Mesh::IndexCont();

    // calc stride
    auto stride = std::size_t(0);

    if (mesh->HasPositions()) {
        stride += sizeof(aiVector3D);
    }

    if (mesh->HasNormals()) {
        stride += sizeof(aiVector3D);
    }

    if (mesh->HasTextureCoords(0)) {
        stride += sizeof(aiVector2D);
    }

    if (mesh->HasVertexColors(0)) {
        stride += sizeof(aiColor4D);
    }

    vb.configStride(stride);

    auto accOffset = VertexBuffer::offset_t(0);

    // construct vertex buffer
    if (mesh->HasPositions()) {
        vb.configProperty(Vertex::Properties::Position, accOffset);
        vb.constructProperty(Vertex::Properties::Position,
            mesh->mVertices, sizeof(aiVector3D), mesh->mNumVertices, sizeof(aiVector3D)
        );
        accOffset += sizeof(aiVector3D);
    }

    if (mesh->HasNormals()) {
        vb.configProperty(Vertex::Properties::Normal, accOffset);
        vb.constructProperty(Vertex::Properties::Normal,
            mesh->mNormals, sizeof(aiVector3D), mesh->mNumVertices, sizeof(aiVector3D)
        );
        accOffset += sizeof(aiVector3D);
    }

    if (mesh->HasTextureCoords(0)) {
        vb.configProperty(Vertex::Properties::TexCoord, accOffset);
        vb.constructProperty(Vertex::Properties::TexCoord,
            mesh->mTextureCoords[0], sizeof(aiVector2D), mesh->mNumVertices, sizeof(aiVector3D)
        );
        accOffset += sizeof(aiVector2D);
    }

    if (mesh->HasVertexColors(0)) {
        vb.configProperty(Vertex::Properties::Color, accOffset);
        vb.constructProperty(Vertex::Properties::Color,
           mesh->mColors[0], sizeof(aiColor4D), mesh->mNumVertices, sizeof(aiColor4D)
        );
        accOffset += sizeof(aiColor4D);
    }

    // construct index buffer
    auto out = std::back_inserter(ib);
    for (std::size_t i = 0; i < mesh->mNumFaces; ++i) {
        auto face = mesh->mFaces[i];

        if (face.mNumIndices != 3u) {
            continue;
        }


        for (std::size_t j = 0; j < face.mNumIndices; ++j) {
            *out++ = face.mIndices[j];
        }
    }

    return Mesh(std::move(vb), std::move(ib));
}

}   // namespace gfx::detail

namespace {

Mesh getFirstMeshFromAiNode(const aiNode* node, const aiScene* scene) {
    for (auto i = 0u; i < node->mNumMeshes; ++i) {
        auto mesh = scene->mMeshes[node->mMeshes[i]];
        if (mesh->mNumFaces > 0) {
            return detail::getMeshFromAiNode(node, scene, i);
        }
    }

    for (auto i = 0u; i < node->mNumChildren; ++i) {
        return getFirstMeshFromAiNode(node->mChildren[i], scene);
    }

    throw std::runtime_error("No meshes found");

    return Mesh();
}

Mesh getFirstMeshFromAiScene(const aiScene* scene) {
    if (scene->mRootNode->mNumMeshes == 0) {
        if (scene->mRootNode->mChildren[0]->mNumMeshes == 0) {
            throw std::runtime_error("No meshes found");
        }
        return getFirstMeshFromAiNode(scene->mRootNode->mChildren[0], scene);
    }

    return getFirstMeshFromAiNode(scene->mRootNode, scene);
}

}   // namespace gfx::anonymus_namespace

Mesh loadMesh(const std::filesystem::path& path) {
    auto importer = Assimp::Importer();
    auto scene = importer.ReadFile( path.string(), aiProcess_Triangulate
        | aiProcess_JoinIdenticalVertices | aiProcess_ConvertToLeftHanded
        | aiProcess_SortByPType    
    );

    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
        throw std::runtime_error("Failed to load mesh: " + path.string());
    }

    return getFirstMeshFromAiScene(scene);
}

Mesh loadMesh(const std::filesystem::path& path, const InputLayout& il) {
    auto mesh = loadMesh(path);

    for (const auto& elem : il) {
        if (!mesh.contains(elem.prop)) {
            throw std::runtime_error("The requested input layout is not supported by the mesh");
        }
    }

    return Mesh(convert(mesh.vb(), il), std::move(mesh.ib()));
}

}   // namespace gfx
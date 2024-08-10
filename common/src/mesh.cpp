#include "mesh.hpp"

#include "assimp/Importer.hpp"
#include "assimp/scene.h"
#include "assimp/postprocess.h"

namespace gfx {

Mesh loadMesh(const std::filesystem::path& path) {
    auto importer = Assimp::Importer();

    auto flag = aiProcess_Triangulate | aiProcess_JoinIdenticalVertices
        | aiProcess_MakeLeftHanded;

    auto scene = importer.ReadFile(path.string(), flag);

    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
        throw;  // TODO: add exception
    }

    auto vb = VertexBuffer();
    auto ib = Mesh::IndexCont();

    auto mesh = scene->mMeshes[0];

    // calc stride
    auto stride = std::size_t(0);

    if (mesh->HasPositions()) {
        stride += sizeof(aiVector3D);    
    }

    if (mesh->HasNormals()) {
        stride += sizeof(aiVector3D);
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
        for (std::size_t j = 0; j < face.mNumIndices; ++j) {
            *out++ = face.mIndices[j];
        }
    }

    return Mesh(std::move(vb), std::move(ib));
}

Mesh loadMesh(const std::filesystem::path& path, const InputLayout& il) {
    auto mesh = loadMesh(path);
    return Mesh(convert(mesh.vb(), il), std::move(mesh.ib()));
}

}   // namespace gfx
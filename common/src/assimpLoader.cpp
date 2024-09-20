#include "assimpLoader.hpp"

namespace gfx {

void AssimpLoader::load(const std::filesystem::path& path, Flags flags) {
    state_ = std::make_unique<State>();
    state_->scene = state_->importer.ReadFile(path.string(), flags);

    if ( !state_->scene || state_->scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE
        || !state_->scene->mRootNode
    ) {
        throw std::runtime_error("Failed to load model");
    }
}

const Mesh AssimpLoader::buildGeneralMesh(const aiMesh* mesh) const {
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

}   // namespace gfx
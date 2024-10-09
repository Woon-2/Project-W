#include "assimpLoader.hpp"

#include "gfxExcept.hpp"

namespace gfx {

void AssimpLoader::load(const std::filesystem::path& path, Flags flags) {
    state_ = std::make_unique<State>();
    state_->scene = state_->importer.ReadFile(path.string(), flags);
    state_->path = path;

    if ( !state_->scene || state_->scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE
        || !state_->scene->mRootNode
    ) {
        throw std::runtime_error("Failed to load model");
    }
}

Mesh AssimpLoader::buildGeneralMesh(const VBFlags& flags, const aiMesh* mesh) const {
    auto vbs = Mesh::Cont<VertexBuffer>();
    vbs.reserve(flags.size());

    for (const auto& flag : flags) {
        auto vb = VertexBuffer();

        // calculate stride while checking the flags
        auto stride = std::size_t(0);

        if (flag.test(etoi(Vertex::Properties::Position3D))) {
            if (!mesh->HasPositions()) {
                continue;
            }
            stride += sizeof(aiVector3D);

        }
        if (flag.test(etoi(Vertex::Properties::Normal3D))) {
            if (!mesh->HasNormals()) {
                continue;
            }
            stride += sizeof(aiVector3D);
        }
        if (flag.test(etoi(Vertex::Properties::TexCoord2D0))) {
            if (!mesh->HasTextureCoords(0)) {
                continue;
            }
            stride += sizeof(aiVector2D);
        }
        if (flag.test(etoi(Vertex::Properties::Tangent3D))) {
            if (!mesh->HasTangentsAndBitangents()) {
                continue;
            }
            stride += sizeof(aiVector3D);
        }
        if (flag.test(etoi(Vertex::Properties::Bitangent3D))) {
            if (!mesh->HasTangentsAndBitangents()) {
                continue;
            }
            stride += sizeof(aiVector3D);
        }
        if (flag.test(etoi(Vertex::Properties::Color3D))) {
            if (!mesh->HasVertexColors(0)) {
                continue;
            }
            stride += sizeof(aiColor4D);    // assimp stores vertex color in aiColor4D
        }
        if (flag.test(etoi(Vertex::Properties::Color4D))) {
            if (!mesh->HasVertexColors(0)) {
                continue;
            }
            stride += sizeof(aiColor4D);
        }

        vb.configStride(stride);

        // layout the properties in order of the flags

        auto accOffset = VertexBuffer::offset_t(0);

        if (flag.test(etoi(Vertex::Properties::Position3D))) {
            vb.configProperty(Vertex::Properties::Position3D, accOffset);
            vb.constructProperty( Vertex::Properties::Position3D,
                mesh->mVertices, mesh->mNumVertices, sizeof(aiVector3D)
            );
            accOffset += sizeof(aiVector3D);
        }

        if (flag.test(etoi(Vertex::Properties::Normal3D))) {
            vb.configProperty(Vertex::Properties::Normal3D, accOffset);
            vb.constructProperty( Vertex::Properties::Normal3D,
                mesh->mNormals, mesh->mNumVertices, sizeof(aiVector3D)
            );
            accOffset += sizeof(aiVector3D);
        }

        if (flag.test(etoi(Vertex::Properties::TexCoord2D0))) {
            vb.configProperty(Vertex::Properties::TexCoord2D0, accOffset);
            vb.constructProperty( Vertex::Properties::TexCoord2D0,
                mesh->mTextureCoords[0], mesh->mNumVertices, sizeof(aiVector3D)
            );
            accOffset += sizeof(aiVector2D);
        }

        if (flag.test(etoi(Vertex::Properties::Tangent3D))) {
            vb.configProperty(Vertex::Properties::Tangent3D, accOffset);
            vb.constructProperty( Vertex::Properties::Tangent3D,
                mesh->mTangents, mesh->mNumVertices, sizeof(aiVector3D)
            );
            accOffset += sizeof(aiVector3D);
        }

        if (flag.test(etoi(Vertex::Properties::Bitangent3D))) {
            vb.configProperty(Vertex::Properties::Bitangent3D, accOffset);
            vb.constructProperty( Vertex::Properties::Bitangent3D,
                mesh->mBitangents, mesh->mNumVertices, sizeof(aiVector3D)
            );
            accOffset += sizeof(aiVector3D);
        }

        if (flag.test(etoi(Vertex::Properties::Color3D))) {
            vb.configProperty(Vertex::Properties::Color3D, accOffset);
            vb.constructProperty( Vertex::Properties::Color3D,
                mesh->mColors[0], mesh->mNumVertices, sizeof(aiColor4D)
            );
            accOffset += sizeof(aiColor3D);
        }

        if (flag.test(etoi(Vertex::Properties::Color4D))) {
            vb.configProperty(Vertex::Properties::Color4D, accOffset);
            vb.constructProperty( Vertex::Properties::Color4D,
                mesh->mColors[0], mesh->mNumVertices, sizeof(aiColor4D)
            );
            accOffset += sizeof(aiColor4D);
        }

        if (vb.byteWidth() > 0) {
            vbs.push_back(std::move(vb));
        }
    }

    auto ib = Mesh::Cont<Mesh::Index>();

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

    return Mesh(std::move(vbs), std::move(ib));
}

}   // namespace gfx
#ifndef __AssimpLoader_HPP
#define __AssimpLoader_HPP

#include "assimp/scene.h"
#include "assimp/Importer.hpp"
#include "assimp/postprocess.h"

#include "mesh.hpp"
#include "model.hpp"
#include "inputLayout.hpp"

#include "enumUtil.hpp"

#include <vector>
#include <memory>
#include <filesystem>

namespace gfx {

class AssimpLoader {
public:
    enum class Flag : std::uint32_t {
        calcTangentSpace = aiProcess_CalcTangentSpace,
        convertToLeftHanded = aiProcess_ConvertToLeftHanded,
        debone = aiProcess_Debone,
        dropNormals = aiProcess_DropNormals,
        embedTextures = aiProcess_EmbedTextures,
        findDegenerates = aiProcess_FindDegenerates,
        findInstances = aiProcess_FindInstances,
        findInvalidData = aiProcess_FindInvalidData,
        fixInfacingNormals = aiProcess_FixInfacingNormals,
        flipUVs = aiProcess_FlipUVs,
        flipWindingOrder = aiProcess_FlipWindingOrder,
        forceGenNormals = aiProcess_ForceGenNormals,
        // genBoundingBoxes = aiProcess_GenBoundingBoxes,
        genNormals = aiProcess_GenNormals,
        genSmoothNormals = aiProcess_GenSmoothNormals,
        genUVCoords = aiProcess_GenUVCoords,
        globalScale = aiProcess_GlobalScale,
        improveCacheLocality = aiProcess_ImproveCacheLocality,
        joinIdenticalVertices = aiProcess_JoinIdenticalVertices,
        limitBoneWeights = aiProcess_LimitBoneWeights,
        makeLeftHanded = aiProcess_MakeLeftHanded,
        optimizeGraph = aiProcess_OptimizeGraph,
        optimizeMeshes = aiProcess_OptimizeMeshes,
        populateArmatureData = aiProcess_PopulateArmatureData,
        preTransformVertices = aiProcess_PreTransformVertices,
        removeComponent = aiProcess_RemoveComponent,
        removeRedundantMaterials = aiProcess_RemoveRedundantMaterials,
        sortByPType = aiProcess_SortByPType,
        splitByBoneCount = aiProcess_SplitByBoneCount,
        splitLargeMeshes = aiProcess_SplitLargeMeshes,
        transformUVCoords = aiProcess_TransformUVCoords,
        triangulate = aiProcess_Triangulate,
        validateDataStructure = aiProcess_ValidateDataStructure
    };

    using Flags = std::underlying_type_t<Flag>;

private:
    struct State{
        Assimp::Importer importer;
        std::filesystem::path path;
        const aiScene* scene;
    };

protected:
    const Mesh buildGeneralMesh(const aiMesh* mesh) const;
    const Mesh buildGeneralMesh(const aiMesh* mesh, const InputLayout& inputLayout) const {
        auto tmp = buildGeneralMesh(mesh);
        return Mesh(convert(tmp.vb(), inputLayout), tmp.ib());
    }

public:
    void load(const std::filesystem::path& path, Flags flags);
    const aiScene* scene() const {
        if (!state_) {
            throw std::runtime_error("No scene loaded");
        }
        return state_->scene;
    }
    const std::filesystem::path loadedPath() const {
        if (!state_) {
            throw std::runtime_error("No scene loaded");
        }
        return state_->path;
    }

private:
    std::unique_ptr<State> state_;
};

DEFINE_ENUM_LOGICAL_OP_ALL(AssimpLoader::Flag);

}   // namespace gfx

#endif  // __AssimpLoader_HPP
#ifndef __AssetSystem_HPP
#define __AssetSystem_HPP

#include "assetMap.hpp"

#include "d3d12core.hpp"
#include "d3d12model.hpp"
#include "d3d12texture.hpp"
#include "d3d12materialTree.hpp"
#include "assimpLoaderD3d12.hpp"

#include <map>
#include <string>
#include <filesystem>
#include <vector>

class AssetSystem {
public:
    friend class AssetLinker;

    using Key = AssetDesc::Key;
    AssetSystem(gfx::d3d12::Core& core, std::size_t fenceIdx = 0);

    void loadAssets();

private:
    void allocCtx();
    void freeCtx();

    void loadTexture(const Key& key, const std::filesystem::path& path);
    void loadModel(const Key& key, const std::filesystem::path& path); 
    void loadMaterialTree(const Key& key, const std::filesystem::path& path);

    bool containsTexture(const Key& key) const { return textures_.contains(key); }
    bool containsModel(const Key& key) const { return models_.contains(key); }
    bool containsMaterialTree(const Key& key) const { return matTrees_.contains(key); }

    const gfx::d3d12::Texture* texture(const Key& key) const {
        if (auto it = textures_.find(key); it != textures_.end()) {
            return &it->second;
        }

        throw std::runtime_error("Texture with key " + key + " not found");
    }

    const gfx::d3d12::Model* model(const Key& key) const {
        if (auto it = models_.find(key); it != models_.end()) {
            return &it->second;
        }

        throw std::runtime_error("Model with key " + key + " not found");
    }

    const gfx::d3d12::MaterialTree* materialTree(const Key& key) const {
        if (auto it = matTrees_.find(key); it != matTrees_.end()) {
            return &it->second;
        }

        throw std::runtime_error("MaterialTree with key " + key + " not found");
    }

    std::map<Key, gfx::d3d12::Texture> textures_;
    std::map<std::filesystem::path, const gfx::d3d12::Texture*> texPaths_;
    std::map<Key, gfx::d3d12::Model> models_;
    std::map<Key, gfx::d3d12::MaterialTree> matTrees_;
    std::unique_ptr<gfx::d3d12::D3D12RenderContext> pCtx_;
    gfx::d3d12::Core* pCore_;
    std::size_t fenceIdx_;
};

class AssetLinker {
public:
    friend class AssetSystem;

    void configAsset(assetIDs::ID id) {
        // It doesn't check duplication.
        // The duplication will introduce a slight overhead, but it's not a big deal.
        requiredAssets_.push_back(id);
    }

private:
    void loadRequiredAssets(AssetSystem& assetSys);

    std::vector<assetIDs::ID> requiredAssets_;
};

#endif  // __AssetSystem_HPP
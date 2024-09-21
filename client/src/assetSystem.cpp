#include "assetSystem.hpp"

#include "ecs.hpp"

#include "inputLayoutPresets.hpp"

#include <ranges>
#include <algorithm>

AssetSystem::AssetSystem(gfx::d3d12::Core& core, std::size_t fenceIdx)
    : textures_(), texPaths_(), models_(), matTrees_(),
    pCtx_(), pCore_(&core), fenceIdx_(fenceIdx) {

}

void AssetSystem::loadAssets() {
    for (auto entity : ecs::gSystems[typeid(AssetSystem).name()]->entites_) {
        auto& assetLinker = ecs::GetComponent<AssetLinker>(entity);
        assetLinker.loadRequiredAssets(*this);
    }
}

void AssetSystem::allocCtx() {
    using Ctx = gfx::d3d12::D3D12RenderContext;
    pCtx_ = std::unique_ptr<Ctx>( static_cast<Ctx*>(pCore_->createContext().release()) );

    pCore_->preRender();
    pCtx_->preRender();
}

void AssetSystem::freeCtx() {
    pCtx_->postRender();
    pCore_->postRender();

    pCore_->signalGpu(fenceIdx_);
    pCore_->waitGpu(fenceIdx_);

    for (auto& [key, tex] : textures_) {
        tex.completeInit();
    }

    for (auto& [key, model] : models_) {
        model.completeInit();
    }

    pCtx_.reset();

    assimpLoaders_.clear();
}

void AssetSystem::loadTexture(const Key& key, const std::filesystem::path& path) {
    if (!pCtx_) {
        throw std::runtime_error("No render context allocated");
    }

    if (textures_.contains(key)) {
        throw std::runtime_error("Texture with key " + key + " already loaded");
    }

    auto [it, _] = textures_.try_emplace(key, gfx::d3d12::Texture(*pCore_, *pCtx_, path));
    texPaths_.try_emplace(path, &it->second);
}

void AssetSystem::loadModel(const Key& key, const std::filesystem::path& path) {
    if (!pCtx_) {
        throw std::runtime_error("No render context allocated");
    }

    if (models_.contains(key)) {
        throw std::runtime_error("Model with key " + key + " already loaded");
    }

    using Flag = gfx::d3d12::AssimpLoader::Flag;

    if (auto loader = assimpLoaders_.find(path); loader != assimpLoaders_.end()) {
        models_.try_emplace( key, loader->second.buildModel(
            *pCore_, *pCtx_, gfx::makeInputLayoutPreset( gfx::InputLayoutPreset::Pos3Norm3Tex2 )
        ) );

        return;
    }

    auto loader = gfx::d3d12::AssimpLoader();

    loader.load( path, Flag::triangulate | Flag::joinIdenticalVertices
        | Flag::convertToLeftHanded | Flag::sortByPType
    );

    models_.try_emplace( key, loader.buildModel(
        *pCore_, *pCtx_, gfx::makeInputLayoutPreset( gfx::InputLayoutPreset::Pos3Norm3Tex2 )
    ) );

    assimpLoaders_.try_emplace( path, std::move(loader) );
}

void AssetSystem::loadMaterialTree(const Key& key, const std::filesystem::path& path) {
    if (matTrees_.contains(key)) {
        throw std::runtime_error("MaterialTree with key " + key + " already loaded");
    }

    if (auto loader = assimpLoaders_.find(path); loader != assimpLoaders_.end()) {
        matTrees_.try_emplace( key, loader->second.buildMaterialTree(*pCore_, texPaths_) );
        return;
    }

    auto loader = gfx::d3d12::AssimpLoader();

    loader.load(path, 0);

    matTrees_.try_emplace( key, loader.buildMaterialTree(*pCore_, texPaths_) );

    assimpLoaders_.try_emplace( path, std::move(loader) );
}

void AssetLinker::loadRequiredAssets(AssetSystem& assetSys) {
    for (auto id : requiredAssets_) {
        const auto& desc = detail::gAssetDescs[id];
        switch (desc.type) {
        case AssetType::Model:
            if (!assetSys.containsModel(desc.key)) {
                assetSys.loadModel( desc.key, desc.path );
            }
            break;

        case AssetType::Texture:
            if (!assetSys.containsTexture(desc.key)) {
                assetSys.loadTexture( desc.key, desc.path );
            }
            break;

        case AssetType::MaterialTree:
            if (!assetSys.containsMaterialTree(desc.key)) {
                assetSys.loadMaterialTree( desc.key, desc.path );
            }
            break;

        default:
            throw std::runtime_error("Asset type not supported");
        }
    }
}
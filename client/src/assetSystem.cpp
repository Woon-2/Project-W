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
    for (auto& weakAL : components<AssetLinker>()) {
        auto al = weakAL.lock();
        if (!al) {
            throw ECS_EXCEPT("AssetLinker component is not valid");
        }

        al->loadRequiredAssets(*this);
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
        return;
    }

    auto [it, _] = textures_.try_emplace(key, gfx::d3d12::Texture(*pCore_, *pCtx_, path));
    texPaths_.try_emplace(path, &it->second);
}

void AssetSystem::loadModel(const Key& key, const std::filesystem::path& path) {
    if (!pCtx_) {
        throw std::runtime_error("No render context allocated");
    }

    if (models_.contains(key)) {
        return;
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
    if (auto loader = assimpLoaders_.find(path); loader != assimpLoaders_.end()) {
        matTrees_.try_emplace( key, loader->second.buildMaterialTree(*pCore_, texPaths_) );
        return;
    }

    if (matTrees_.contains(key)) {
        return;
    }

    auto loader = gfx::d3d12::AssimpLoader();

    loader.load(path, 0);

    matTrees_.try_emplace( key, loader.buildMaterialTree(*pCore_, texPaths_) );

    assimpLoaders_.try_emplace( path, std::move(loader) );
}

void AssetLinker::loadRequiredAssets(AssetSystem& assetSys) {
    for (auto id : requiredAssets_) {
        const auto& descs = detail::gAssetDescs[id];
        for (const auto& desc : descs) {
            switch (desc.type) {
            case AssetType::Model:
                assetSys.loadModel( desc.key, desc.path );
                break;

            case AssetType::Texture:
                assetSys.loadTexture( desc.key, desc.path );
                break;

            case AssetType::MaterialTree:
                assetSys.loadMaterialTree( desc.key, desc.path );
                break;

            default:
                throw std::runtime_error("Asset type not supported");
            }
        }
    }
}
#include "assetSystem.hpp"

#include "ecs.hpp"

#include "inputLayoutPresets.hpp"

#include <ranges>
#include <algorithm>

AssetSystem::AssetSystem(gfx::d3d12::Core& core, std::size_t fenceIdx)
    : textures_(), models_(), pCtx_(), pCore_(&core), fenceIdx_(fenceIdx) {

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

    for (const auto& [key, tex] : textures_) {
        tex.completeInit(*pCore_);
    }

    for (const auto& [key, model] : models_) {
        model.completeInit(*pCore_);
    }

    pCtx_.reset();
}

void AssetSystem::loadTexture(const Key& key, const std::filesystem::path& path) {
    if (!pCtx_) {
        throw std::runtime_error("No render context allocated");
    }

    if (textures_.contains(key)) {
        throw std::runtime_error("Texture with key " + key + " already loaded");
    }

    textures_.try_emplace(key, gfx::d3d12::Texture(*pCore_, *pCtx_, path, key));
}

void AssetSystem::loadModel(const Key& key, const std::filesystem::path& path) {
    if (!pCtx_) {
        throw std::runtime_error("No render context allocated");
    }

    if (models_.contains(key)) {
        throw std::runtime_error("Model with key " + key + " already loaded");
    }

    models_.try_emplace( key, gfx::d3d12::Model( *pCore_, *pCtx_,
        gfx::loadModel( path, gfx::makeInputLayoutPreset( gfx::InputLayoutPreset::Pos3Norm3Tex2 ) ),
        key + "_vb", key + "_ib"
    ) );
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

        default:
            throw std::runtime_error("Asset type not supported");
        }
    }
}
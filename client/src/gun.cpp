#include "gun.hpp"

#include "resourcePath.hpp"
#include "d3d12InputLayoutPresets.hpp"

#include <ranges>

void Gun::loadAssets(gfx::d3d12::Core& core, std::size_t fenceIdx) {
    auto pCtx = std::unique_ptr<gfx::d3d12::D3D12RenderContext>(
        static_cast<gfx::d3d12::D3D12RenderContext*>( core.createContext().release() )
    );

    core.preRender();
    pCtx->preRender();

    sMesh = gfx::d3d12::Mesh( core, *pCtx,
        gfx::loadMesh( resourcePath/"models/Gun _obj/Gun.obj",
            core.inputLayout( gfx::d3d12::inputLayoutName( gfx::InputLayoutPreset::Pos3Norm3Tex2) )
        ), "gun_vb", "gun_ib"
    );

    constexpr auto shininess = 2.f;

    sMaterial = gfx::d3d12::Material( core,
        std::views::single( gfx::d3d12::Material::Properties::Diffuse ),
        std::views::single( gfx::d3d12::Texture(core, *pCtx, resourcePath/"models/Gun _obj/Gun.png", "gunD") ),
        shininess
    );
    auto tmpTex = gfx::d3d12::Texture(core, *pCtx, resourcePath / "models/Gun _obj/Gun.png", "gunS");


    pCtx->postRender();
    core.postRender();

    core.signalGpu(fenceIdx);
    core.waitGpu(fenceIdx);

    sMesh.completeInit(core);
    sMaterial.completeInit(core);
    tmpTex.completeInit(core);

    sMaterial.pushTexture(core, gfx::d3d12::Material::Properties::Specular,
        std::move(tmpTex)
    );
}

void MU_CALLCONV Gun::update(mu::Vec3 pos, mu::NQuat rot) {
    coord_.setLocalXform(rot.mat4() * mu::translate(pos));
}

gfx::d3d12::Mesh Gun::sMesh;
gfx::d3d12::Material Gun::sMaterial;
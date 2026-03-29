#include "pch.hpp"
#include "skybox.hpp"

void SkyboxObject::render(GFX& gfx) {
	for (auto& [mesh, _] : pModel_->meshWithDressXforms) {
		for (std::size_t i = 0u; i < mesh.subMeshes.size(); ++i) {
			gfx.addDrawEvent( SkyboxPipeline::DrawEvent{
				.mesh = &mesh,
				.subMesh = &mesh.subMeshes[i],
				.texSkybox = &pSkyboxMaterial_->texSkybox
			} );
		}
	}
}
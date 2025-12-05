#include "pch.hpp"
#include "basicPlayerHpUI.hpp"
#include "errorHandling.hpp"

BasicPlayerHpUI::BasicPlayerHpUI() {
	mu::Vec3 scale{ 1024.0f / 2.0f, 64.0f / 2.0f, 1.0f };
	mu::Vec3 translation{ 512.0f, 40.0f, 0.0f };
	world_ = mu::Mat4x4( mu::scale( scale ) ) * mu::translate( translation );
}

void BasicPlayerHpUI::update( Milliseconds deltaTime ) {
}

void BasicPlayerHpUI::render( GFX& gfx ) {
	if ( pTex_ ) {
		gfx.addDrawEvent( UIPipeline::DrawEvent{
			.world = world_,
			.pTex = pTex_
		} );
	}
}


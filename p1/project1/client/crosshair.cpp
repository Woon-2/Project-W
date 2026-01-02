#include "pch.hpp"
#include "crosshair.hpp"

void Crosshair::update( Milliseconds deltaTime ) {
	world_ = mu::Mat4x4( mu::scale( mu::Vec3(scale_, 1.f) ) ) * mu::translate( mu::Vec3(pivot_, 0.f) );
}

void Crosshair::render( GFX& gfx ) {
	if ( pTex_ ) {
		gfx.addDrawEvent( UIPipeline::DrawEvent{
			.world = world_,
			.pTex = pTex_
		} );
	}
}
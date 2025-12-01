#include "basicPlayerHpUI.hpp"
#include "errorHandling.hpp"

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


#include "spriteAnimation.hpp"
#include "errorHandling.hpp"

void SpriteAnimation::update( Milliseconds deltaTime ) {
	// 24fps로 애니메이션 재생
}

void SpriteAnimation::render( GFX& gfx ) {
	gfx.addDrawEvent( BillboardPipeline::DrawEvent{
		.world = world_,
		} );
	
}
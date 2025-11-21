#include "spriteAnimation.hpp"
#include "errorHandling.hpp"

void SpriteAnimation::setTexture( const std::vector<Texture>& textures )
{
	sprites_ = textures; 
	pCurrentTexture_ = sprites_[currentFrameIdx_];
}

void SpriteAnimation::update( Milliseconds deltaTime ) {
	// 24fps로 애니메이션 재생
	frameAcc_ += deltaTime.count();
	if ( frameAcc_ >= frmaeDuration_ ) {
		frameAcc_ -= frmaeDuration_;
		// 다음 프레임으로 전환
		currentFrameIdx_ = (currentFrameIdx_ + 1) % sprites_.size();
	}
	pCurrentTexture_ = sprites_[currentFrameIdx_];
}

void SpriteAnimation::render( GFX& gfx ) {
	gfx.addDrawEvent( BillboardPipeline::DrawEvent{
		.world = world_,
		.pTex = &pCurrentTexture_
		} );
	
}
#include "pch.hpp"
#include "spriteAnimation.hpp"
#include "gfx.hpp"
#include "errorHandling.hpp"
#include "binaryImport.hpp"

void SpriteAnimation::init(const SpriteAnimationClip* pAnimData) {
	pAnimData_ = pAnimData;
	const auto m = static_cast<float>( pAnimData->frames.size() * pAnimData->frameTime / pAnimData->duration );
	if (pAnimData->type == SpriteAnimType::RandomAdvance) {
		distRandomAdvance_ = std::normal_distribution<float>(m, m);
	}
}

void SpriteAnimation::update( Milliseconds deltaTime ) {
	if (done_) {
		return;
	}

	timeAcc_ += deltaTime * speed_;

	if (pAnimData_->type == SpriteAnimType::RandomAdvance) {
		while (timeAcc_ >= pAnimData_->frames[currFrameIdx_].time) {
			timeAcc_ -= pAnimData_->frames[currFrameIdx_].time;
			currFrameIdx_ += static_cast<std::size_t>(
				std::max(distRandomAdvance_(gRandomEngine), 1.f)
			);

			if (currFrameIdx_ >= pAnimData_->frames.size() - 1u) {
				done_ = true;
				break;
			}
		}
		return;
	}

	while (timeAcc_ >= pAnimData_->frames[currFrameIdx_ + 1].time) {
		++currFrameIdx_;
	}

	if (pAnimData_->type == SpriteAnimType::Once) {
		if (timeAcc_ >= pAnimData_->duration) {
			done_ = true;
		}
	}
	else if (pAnimData_->type == SpriteAnimType::Loop) {
		if (timeAcc_ >= pAnimData_->duration) {
			timeAcc_ -= static_cast<int>(timeAcc_ / pAnimData_->duration) * pAnimData_->duration;
			currFrameIdx_ = 0;
		}
	}
}

void SpriteAnimation::render( GFX& gfx ) {
	gfx.addDrawEvent( BillboardPipeline::DrawEvent{
		.world = world_,
		.pTex = &pAnimData_->frames[currFrameIdx_].sprite
	} );
}
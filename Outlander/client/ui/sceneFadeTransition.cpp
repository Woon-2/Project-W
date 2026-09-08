#include "pch.hpp"
#include "sceneFadeTransition.hpp"

#include "../gfx.hpp"
#include "uiShapes.hpp"

namespace UI {

bool SceneFadeTransition::start(MidpointAction action, Timing timing) {
	if (active()) {
		return false;
	}

	constexpr float kMinDurationSec = 0.001f;
	timing.fadeOutSeconds = std::max(kMinDurationSec, timing.fadeOutSeconds);
	timing.fadeInSeconds = std::max(kMinDurationSec, timing.fadeInSeconds);

	timing_ = timing;
	elapsedSec_ = 0.f;
	midpointAction_ = std::move(action);
	phase_ = Phase::FadeOut;
	return true;
}

void SceneFadeTransition::update(float deltaTimeSec) {
	if (!active()) {
		return;
	}

	elapsedSec_ += std::max(0.f, deltaTimeSec);

	if (phase_ == Phase::FadeOut) {
		if (elapsedSec_ < timing_.fadeOutSeconds) {
			return;
		}

		// Switch phase before the callback so the new scene is rendered fully
		// covered even when the callback rebuilds its entire UI tree.
		phase_ = Phase::FadeIn;
		elapsedSec_ = 0.f;
		auto action = std::move(midpointAction_);
		if (action) {
			action();
		}
		return;
	}

	if (elapsedSec_ >= timing_.fadeInSeconds) {
		cancel();
	}
}

void SceneFadeTransition::render(
	GFX& gfx, float screenWidth, float screenHeight) const {
	const float alpha = opacity();
	if (alpha <= 0.f || screenWidth <= 0.f || screenHeight <= 0.f) {
		return;
	}

	uiShapes::quad(gfx, gfx.solidColorTex(),
		screenWidth * 0.5f, screenHeight * 0.5f,
		screenWidth, screenHeight, 0.f,
		XMFLOAT4{ 0.f, 0.f, 0.f, alpha });
}

void SceneFadeTransition::cancel() {
	phase_ = Phase::None;
	elapsedSec_ = 0.f;
	midpointAction_ = {};
}

float SceneFadeTransition::opacity() const {
	float alpha = 0.f;
	switch (phase_) {
	case Phase::FadeOut:
		alpha = std::clamp(elapsedSec_ / timing_.fadeOutSeconds, 0.f, 1.f);
		break;
	case Phase::FadeIn:
		alpha = 1.f - std::clamp(elapsedSec_ / timing_.fadeInSeconds, 0.f, 1.f);
		break;
	case Phase::None:
		return 0.f;
	}

	// Smoothstep removes visible speed discontinuities at both endpoints.
	return alpha * alpha * (3.f - 2.f * alpha);
}

} // namespace UI

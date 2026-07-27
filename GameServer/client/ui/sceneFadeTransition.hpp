#ifndef CLIENT_UI_SCENE_FADE_TRANSITION_HPP
#define CLIENT_UI_SCENE_FADE_TRANSITION_HPP

#include <functional>

class GFX;

namespace UI {

// UI-pipeline full-screen fade that remains independent from UIManager's widget
// tree. This lets it cover widget-tree rebuilds performed during scene changes.
class SceneFadeTransition {
public:
	struct Timing {
		float fadeOutSeconds = 0.45f;
		float fadeInSeconds = 0.55f;
	};

	using MidpointAction = std::function<void()>;

	// Starts a fade-out, invokes the action once at full black, then fades in.
	// Returns false without changing the current transition when already active.
	bool start(MidpointAction action, Timing timing = {});

	void update(float deltaTimeSec);
	void render(GFX& gfx, float screenWidth, float screenHeight) const;
	void cancel();

	bool active() const { return phase_ != Phase::None; }

private:
	enum class Phase { None, FadeOut, FadeIn };

	float opacity() const;

	Phase phase_ = Phase::None;
	Timing timing_{};
	float elapsedSec_ = 0.f;
	MidpointAction midpointAction_{};
};

} // namespace UI

#endif // CLIENT_UI_SCENE_FADE_TRANSITION_HPP

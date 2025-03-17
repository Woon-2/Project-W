#ifndef __player_HPP
#define __player_HPP

#include "ecs.hpp"
#include "d3d12engine/d3d12Engine.hpp"

class Player : public ecs::Entity {
public:
	Player();

	void init(gfx::d3d12engine::Core& core);
	void MU_CALLCONV addCamera(mu::Vec3 offset, gameEngine::CoordRoot& coordRoot) {
		addCamera(offset, gfx::d3d12::Camera::Config(), coordRoot);
	}
	void MU_CALLCONV addCamera( mu::Vec3 offset, const gfx::d3d12::Camera::Config& config,
		gameEngine::CoordRoot& coordRoot
	) {
		addCamera(offset, 0.f, config, coordRoot);
	}
	void MU_CALLCONV addCamera( mu::Vec3 offset, float timeLag,
		gameEngine::CoordRoot& coordRoot
	) {
		addCamera(offset, timeLag, gfx::d3d12::Camera::Config(), coordRoot);
	}
	void MU_CALLCONV addCamera( mu::Vec3 offset, float timeLag,
		const gfx::d3d12::Camera::Config& config, gameEngine::CoordRoot& coordRoot
	);
	void update(float deltaTime);
	void postUpdate(float deltaTime);

private:
	gfx::coord::System coordRot_;
};

#endif	// __player_HPP


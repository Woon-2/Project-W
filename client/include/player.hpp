#ifndef __player_HPP
#define __player_HPP

#include "ecs.hpp"
#include "d3d12engine/d3d12Engine.hpp"

class Player : public ecs::Entity {
public:
	void init(gfx::d3d12engine::Core& core);
	void MU_CALLCONV addCamera(mu::Vec3 offset) {
		addCamera(offset, gfx::d3d12::Camera::Config());
	}
	void MU_CALLCONV addCamera(mu::Vec3 offset, const gfx::d3d12::Camera::Config& config) {
		addCamera(offset, 0.f, config);
	}
	void MU_CALLCONV addCamera(mu::Vec3 offset, float timeLag) {
		addCamera(offset, timeLag, gfx::d3d12::Camera::Config());
	}
	void MU_CALLCONV addCamera( mu::Vec3 offset, float timeLag,
		const gfx::d3d12::Camera::Config& config
	);
	void update(float deltaTime);
	void postUpdate();
};

#endif	// __player_HPP


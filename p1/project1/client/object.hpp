#ifndef __object_HPP
#define __object_HPP

#include "pch.hpp"
#include "gfx.hpp"
#include "physics.hpp"

struct RenderState {
	mu::Mat4x4 world;
	std::vector<mu::Mat4x4> worldBVs;
};

class Object {
public:
	void update(Milliseconds deltaTime, float tPhysicInterpolation);
	void render(GFX& gfx);

	void setModel(const Model* pModel);
	const Model* model() const { return pModel_; }

	void MU_CALLCONV setPos(mu::Vec3 newPos);
	mu::Vec3 MU_CALLCONV pos() const { return currPhysicState_.pos; }
	void MU_CALLCONV setOmega(mu::Vec3 newOmega);
	mu::Vec3 MU_CALLCONV omega() const { return currPhysicState_.omega; }
	void MU_CALLCONV setOrient(mu::NQuat newOrient);
	mu::NQuat MU_CALLCONV orient() const { return currPhysicState_.orient; }
	void MU_CALLCONV setScale(mu::Vec3 newScale);
	mu::Vec3 MU_CALLCONV scale() const { return currPhysicState_.scale; }
	mu::Vec3 MU_CALLCONV forward() const { return forward_; }
	mu::Vec3 MU_CALLCONV right() const { return right_; }
	mu::Vec3 MU_CALLCONV up() const { return up_; }

	PhysicState& physicState() { return currPhysicState_; }
	void proceedPhysicState() { prevPhysicState_ = currPhysicState_; }

	void setMaterialSetIdx(u32t idx) { materialSetIdx_ = idx; }
	u32t mateiralSetIdx() const { return materialSetIdx_; }

	void enableBVRendering() { willRenderBV_ = true; }
	void disableBVRendering() { willRenderBV_ = false; }

	void setId( i32t id ) {	id_ = id; }
	i32t getId( ) const { return id_; }

private:
	const Model* pModel_ = nullptr;

	PhysicState prevPhysicState_{};
	PhysicState currPhysicState_{};

	RenderState renderState_{};

	bool willRenderBV_ = false;

	mu::Vec3 forward_{};
	mu::Vec3 right_{};
	mu::Vec3 up_{};

	u32t materialSetIdx_ = 0u;
	i32t id_{ -1 };
};

#endif	// __object_HPP
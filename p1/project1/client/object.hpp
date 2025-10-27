#ifndef __object_HPP
#define __object_HPP

#include "pch.hpp"
#include "gfx.hpp"

struct PhysicSnapshot {
	mu::Vec3 pos{};
	mu::Vec3 omega{};
	mu::NQuat orient{};
	float scale = 0.f;
};

enum class PhysicEvaluationMethod {
	LinearInterpolation
};

class Object {
public:
	void setMesh(const Mesh* pMesh) { pMesh_ = pMesh; }
	void update(Milliseconds deltaTime);
	void render(GFX& gfx);

	void MU_CALLCONV setPos(mu::Vec3 newPos);
	mu::Vec3 MU_CALLCONV pos() const { return physicSnapshots_.back().pos; }
	void MU_CALLCONV setOmega(mu::Vec3 newOmega);
	mu::Vec3 MU_CALLCONV omega() const { return physicSnapshots_.back().omega; }
	void MU_CALLCONV setOrient(mu::NQuat newOrient);
	mu::NQuat MU_CALLCONV orient() const { return physicSnapshots_.back().orient; }
	void setScale(float newScale);
	float scale() const { return physicSnapshots_.back().scale; }

private:
	void physicalUpdate();

	mu::Mat4x4 world_{};
	const Mesh* pMesh_ = nullptr;

	std::list<PhysicSnapshot> physicSnapshots_ = std::list<PhysicSnapshot>(1u);
	PhysicEvaluationMethod physicEvaluationMethod_
		= PhysicEvaluationMethod::LinearInterpolation;

	Seconds physicUpdateAcc_{0s};
	Seconds physicUpdateInterval_{1s/60.f};
};

#endif	// __object_HPP
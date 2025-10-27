#ifndef __object_HPP
#define __object_HPP

#include "pch.hpp"
#include "gfx.hpp"

class Object {
public:
	mu::Vec3 pos_{};
	mu::Vec3 omega_{};
	mu::NQuat orient_{};
	float scale_ = 0.f;

	void setMesh(const Mesh* pMesh) { pMesh_ = pMesh; }
	void update(Milliseconds deltaTime);
	void render(GFX& gfx);

private:
	mu::Mat4x4 world_{};
	const Mesh* pMesh_ = nullptr;
};

#endif	// __object_HPP
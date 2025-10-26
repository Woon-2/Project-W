#ifndef __object_HPP
#define __object_HPP

#include "pch.hpp"
#include "gfx.hpp"

class Object {
public:
	Object();

	void setMesh(const Mesh* pMesh) { pMesh_ = pMesh; }
	void update(Milliseconds deltaTime);
	void render(GFX& gfx);

private:
	mu::Mat4x4 world_{};
	mu::Degree rotationDegree_{0.f};
	const Mesh* pMesh_ = nullptr;
};

#endif	// __object_HPP
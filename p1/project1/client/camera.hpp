#ifndef __camera_HPP
#define __camera_HPP

#include "pch.hpp"
#include "gfx.hpp"
#include "object.hpp"

class Camera {
public:
	void setTargetObject(const std::shared_ptr<Object>& pObject) { pTargetObject_ = pObject; }
	std::shared_ptr<Object> pTargetObject() const { return pTargetObject_.lock(); }
	void MU_CALLCONV setOffsetFromTarget(mu::Vec3 offset) { offsetFromTarget_ = offset; }
	mu::Vec3 MU_CALLCONV offsetFromTarget() const { return offsetFromTarget_; }

	void update();
	void updateGFX(GFX& gfx);

	void setPerspective(mu::Degree fovy, float aspect, float nearz, float farz);
	void setOrtho(float minX, float minY, float maxX, float maxY, float minZ, float maxZ);
	
private:
	std::weak_ptr<Object> pTargetObject_{};
	mu::Vec3 offsetFromTarget_{};

	// perspective¿ë
	mu::Degree fovy_ = 90.f;
	float aspect_ = 1.f;
	float nearz_ = 0.01f;
	float farz_ = 500.f;

	// ortho¿ë
	float minX_ = -100.f;
	float maxX_ = 100.f;
	float minY_ = -100.f;
	float maxY_ = 100.f;
	float minZ_ = -100.f;
	float maxZ_ = 100.f;

	mu::Mat4x4 view_{};
	mu::Mat4x4 proj_{};
};

#endif	// __camera_HPP
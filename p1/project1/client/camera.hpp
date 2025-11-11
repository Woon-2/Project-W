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
	
	mu::Vec3 MU_CALLCONV pos() const { return position_; }

private:
	std::weak_ptr<Object> pTargetObject_{};
	mu::Vec3 offsetFromTarget_{};

	mu::Vec3 position_{};

	// perspective¿ë
	float fovy_;
	float aspect_;
	float nearz_;
	float farz_;

	// ortho¿ë
	float minX_;
	float maxX_;
	float minY_;
	float maxY_;
	float minZ_;
	float maxZ_;

	mu::Mat4x4 view_{};
	mu::Mat4x4 proj_{};
};

#endif	// __camera_HPP
#ifndef __camera_HPP
#define __camera_HPP

#include "pch.hpp"
#include "gfx.hpp"
#include "object.hpp"

class Camera {
public:
	void setTargetObject(const std::shared_ptr<Object>& pObject) { pTargetObject_ = pObject; }
	std::shared_ptr<Object> pTargetObject() const { return pTargetObject_.lock(); }
	// 타겟 오브젝트에 대한 카메라의 위치 오프셋을 설정한다.
	void MU_CALLCONV setOffsetFromTarget(mu::Vec3 offset) { offsetFromTarget_ = offset; }
	mu::Vec3 MU_CALLCONV offsetFromTarget() const { return offsetFromTarget_; }
	// 타겟 오브젝트에 대한 카메라의 초점 오프셋을 설정한다.
	void MU_CALLCONV setOffsetTargetPivot(mu::Vec3 offset) { offsetTargetPivot_ = offset; }
	mu::Vec3 MU_CALLCONV offsetTargetPivot() const { return offsetTargetPivot_; }

	void update();
	void updateGFX(GFX& gfx);

	void setPerspective(mu::Degree fovy, float aspect, float nearz, float farz);
	void setOrtho(float minX, float minY, float maxX, float maxY, float minZ, float maxZ);
	
private:
	std::weak_ptr<Object> pTargetObject_{};
	// 타겟 오브젝트에 대한 카메라의 위치 오프셋
	mu::Vec3 offsetFromTarget_{};
	// 타겟 오브젝트에 대한 카메라의 초점 오프셋
	mu::Vec3 offsetTargetPivot_{};

	// perspective용
	mu::Degree fovy_ = 90.f;
	float aspect_ = 1.f;
	float nearz_ = 0.01f;
	float farz_ = 500.f;

	// ortho용
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
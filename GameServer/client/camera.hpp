#ifndef __camera_HPP
#define __camera_HPP

#include "gfx.hpp"
#include "object.hpp"

class PhysicsWorld;

class Camera {
public:
	void setTargetObject(const std::shared_ptr<Object>& pObject) { pTargetObject_ = pObject; }
	std::shared_ptr<Object> pTargetObject() const { return pTargetObject_.lock(); }
	// 타겟 오브젝트에 대한 카메라의 위치 오프셋을 설정한다.
	void MU_CALLCONV setOffsetFromTarget(mu::Vec3 offset) { offsetFromTarget_ = offset; }
	mu::Vec3 MU_CALLCONV offsetFromTarget() const { return offsetFromTarget_; }
	// 카메라 자체의 회전을 설정한다.
	// (이 회전과 타겟 오브젝트의 회전이 순서대로 offsetFromTarget에 대해 적용되어 뷰 행렬이 만들어진다.)
	void MU_CALLCONV setOffsetFromTargetPreRotation(mu::NQuat rotation) { offsetFromTargetPreRotation_ = rotation; }
	mu::NQuat MU_CALLCONV offsetFromTargetPreRotation() const { return offsetFromTargetPreRotation_; }
	// 타겟 오브젝트에 대한 카메라의 초점 오프셋을 설정한다.
	// (예를 들어 인간형 모델의 경우 타겟 오브젝트의 중심은 골반쪽이므로,
	// 카메라가 얼굴을 쳐다볼 수 있도록 초점 오프셋을 위로 주면 좋을 것이다.)
	void MU_CALLCONV setOffsetTargetPivot(mu::Vec3 offset) { offsetTargetPivot_ = offset; }
	mu::Vec3 MU_CALLCONV offsetTargetPivot() const { return offsetTargetPivot_; }

	void update(Milliseconds deltaTime);
	void updateGFX(GFX& gfx);

	void setPhysicsWorld(PhysicsWorld* pw) { physicsWorld_ = pw; }

	void setPerspective(mu::Degree fovy, float aspect, float nearz, float farz);
	void setOrtho(float minX, float minY, float maxX, float maxY, float minZ, float maxZ);

	void setStiffness(float stiffness) { stiffness_ = stiffness; }
	float stiffness() const { return stiffness_; }
	void setDamping(float damping) { damping_ = damping; }
	float damping() const { return damping_; }

	mu::Vec3 MU_CALLCONV eye() const { return eye_; }
	mu::Vec3 MU_CALLCONV at() const { return at_; }

	mu::Mat4x4 MU_CALLCONV view() const { return view_; }
	mu::Mat4x4 MU_CALLCONV proj() const { return proj_; }

private:
	std::weak_ptr<Object> pTargetObject_{};
	// 타겟 오브젝트에 대한 카메라의 위치 오프셋
	mu::Vec3 offsetFromTarget_{};
	// 카메라 자체의 회전 (offsetFromTarget_ 벡터를 회전시킨다.)
	mu::NQuat offsetFromTargetPreRotation_{};
	// 타겟 오브젝트에 대한 카메라의 초점 오프셋
	mu::Vec3 offsetTargetPivot_{};

	// 스프링 기능 관련 변수들
	mu::Vec3 springPos_{};          // 충돌 미적용 스프링 위치 (스프링 앵커)
	mu::Vec3 velocity_{};
	float stiffness_ = 200.f;
	float damping_ = 20.f;
	bool springInitialized_ = false;

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

	// 셰이더 전달용으로만 쓰이는 변수들,
	// 카메라의 위치 갱신은 타겟 오브젝트와 오프셋들 및 회전으로 결정된다.
	mu::Vec3 eye_{};
	mu::Vec3 at_{};
	mu::Mat4x4 view_{};
	mu::Mat4x4 proj_{};

	PhysicsWorld* physicsWorld_  = nullptr;
	float currentArmLength_      = 0.f;   // actual arm length (may be shorter than desired)
	float armReturnRate_         = 3.f;   // units/sec, slow-out return speed
	float cameraRadius_          = 0.15f;  // sphere pad for obstacle queries
};

// Returns the screen-space pixel position of a world-space point.
// Returns false if the point is behind the near plane or outside the NDC frustum.
inline bool worldToScreen(
	mu::Vec3 worldPos,
	const mu::Mat4x4& view, const mu::Mat4x4& proj,
	float screenW, float screenH,
	float& outX, float& outY)
{
	mu::Vec4 clip = mu::Vec4(worldPos, 1.f) * (view * proj);
	if (clip.w() <= 0.f) return false;
	const float ndcX = clip.x() / clip.w();
	const float ndcY = clip.y() / clip.w();
	if (ndcX < -1.f || ndcX > 1.f || ndcY < -1.f || ndcY > 1.f) return false;
	outX = (ndcX + 1.f) * 0.5f * screenW;
	outY = (1.f - ndcY) * 0.5f * screenH;
	return true;
}

#endif	// __camera_HPP
#ifndef headless_bot_behavior_hpp
#define headless_bot_behavior_hpp

#include "Common.hpp"
#include <cmath>

namespace hc {

struct MoveSample {
	DirectX::XMFLOAT3 pos;
	DirectX::XMFLOAT3 velocity;
};

// 렌더링/입력 없이 가짜 움직임을 생성한다. 기본은 스폰 위치(center) 주위 원형 이동.
// center 는 입장 시 S_Enter 의 myInfo.pos 로 설정 → 맵 유효 영역 안에서 움직이고 NPC 근처를 유지.
class BotBehavior {
public:
	void init(const DirectX::XMFLOAT3& center, float radius, float angularSpeed, float phase) {
		center_  = center;
		radius_  = radius;
		angular_ = angularSpeed;
		angle_   = phase;
		// 시작 위치를 원 위의 한 점으로 잡고 prev 도 동일하게 두어 첫 velocity 가 폭발하지 않게 한다.
		pos_ = pointAt(angle_);
	}

	// dt(초) 만큼 진행한 새 위치와 그로부터 유도한 velocity 를 돌려준다.
	MoveSample step(float dt) {
		angle_ += angular_ * dt;
		DirectX::XMFLOAT3 next = pointAt(angle_);
		DirectX::XMFLOAT3 vel{
			(next.x - pos_.x) / dt,
			0.0f,
			(next.z - pos_.z) / dt,
		};
		pos_ = next;
		return MoveSample{ next, vel };
	}

private:
	DirectX::XMFLOAT3 pointAt(float angle) const {
		return DirectX::XMFLOAT3{
			center_.x + radius_ * std::cos(angle),
			center_.y,
			center_.z + radius_ * std::sin(angle),
		};
	}

	DirectX::XMFLOAT3 center_{0, 0, 0};
	DirectX::XMFLOAT3 pos_{0, 0, 0};
	float radius_  = 3.0f;
	float angular_ = 1.5f;
	float angle_   = 0.0f;
};

} // namespace hc

#endif // headless_bot_behavior_hpp

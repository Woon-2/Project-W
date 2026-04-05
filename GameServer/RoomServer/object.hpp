#ifndef room_server_object_hpp
#define room_server_object_hpp

#include "physics.hpp"

struct PhysicState {
	mu::Vec3 pos{};
	mu::Vec3 velocity{};	// Physic System의 step 단계에서 적분될 속도
	mu::Vec3 omega{};	// Physic System의 step 단계에서 적분될 각속도
	mu::Vec3 evVelocity{};	// Physic System 및 그 외 갱신 과정을 거쳐 최종 평가된 속도
	mu::Vec3 evOmega{};	// Physic System 및 그 외 갱신 과정을 거쳐 최종 평가된 속도
	mu::NQuat orient{};
	mu::Vec3 scale{};

	BVH bvh{};
};

struct Model;

// 물체의 상태는 PhysicState, RenderState 두 층위로 관리된다.
// 위치, 방향, 크기와 같이 물리량에 관련된 정보는 PhysicState에 보관되고
// 월드 변환 행렬, 모델 정보와 같이 렌더링에 관련된 정보는 RenderState에 보관된다.
//
// PhysicState의 갱신 주기와 RenderState의 갱신 주기는 다르다.
// (게임 객체가 서로 다른 주기로 갱신한다.)
// 객체의 물리량 갱신은 PhysicSystem이 도맡아 하므로,
// Object::update는 그 내용을 바탕으로 RenderState의 갱신을 주요 과제로 삼는다.
// 게임 객체는 이전 PhysicState와 현재 PhysicState의 내용을 보관하여,
// Object::update를 호출한 시점에 맞게 두 PhysicState를 보간해 월드변환들을 갱신한다.
class Object {
public:
	// 물리 시뮬레이션과 별개로 게임 객체의
	// 자체적인 갱신 루틴을 필요로 할 때 이 함수에 작성한다.
	void update(Milliseconds deltaTime);

	// 모델을 설정한다.
	// 모델에 바운딩 볼륨이 존재할 경우, 월드 공간 바운딩 볼륨을 구축한다.
	// (모델의 바운딩 볼륨을 기반으로 게임 객체의 월드 변환을 적용한
	//  월드 공간 바운딩 볼륨을 따로 두어야 월드 공간 충돌 처리가 가능하다.)
	void setModel(const Model* pModel);

	// 게임 객체의 위치를 갱신한다.
	// PhysicState의 AABB와 Bounding Rect 역시 갱신된다.
	void MU_CALLCONV setPos(mu::Vec3 newPos);
	mu::Vec3 pos() const { return physicState_.pos; }
	// 게임 객체의 속도를 갱신한다.
	// 이전 PhysicState와 현재 PhysicState의 속도가 모두 갱신된다.
	void MU_CALLCONV setVelocity(mu::Vec3 newVelocity);
	mu::Vec3 velocity() const { return physicState_.velocity; }
	// 게임 객체의 각속도를 갱신한다.
	void MU_CALLCONV setOmega(mu::Vec3 newOmega);
	mu::Vec3 omega() const { return physicState_.omega; }
	// 게임 객체의 방향을 갱신한다.
	// 게임 객체의 방향 벡터들도 전부 갱신된다.
	void MU_CALLCONV setOrient(mu::NQuat newOrient);
	mu::NQuat orient() const { return physicState_.orient; }
	// 게임 객체의 크기를 갱신한다.
	// PhysicState의 AABB와 Bounding Rect 역시 갱신된다.
	void MU_CALLCONV setScale(mu::Vec3 newScale);
	mu::Vec3 scale() const { return physicState_.scale; }

	mu::Vec3 forward() const { return forward_; }
	mu::Vec3 right() const { return right_; }
	mu::Vec3 up() const { return up_; }

	PhysicState& physicState() { return physicState_; }

	// 지정한 PhysicState의 pos/scale/orient를 기반으로 BVH를 world-space로 재빌드한다.
	// PhysicSystem 등 외부 시스템에서 직접 pos를 수정한 뒤 호출한다.
	void rebuildBVH(PhysicState& state) const;

	// 재질 집합 인덱스를 설정한다.
	void setMaterialSetIdx(uint32 idx) { materialSetIdx_ = idx; }
	uint32 materialSetIdx() const { return materialSetIdx_; }

	void setId(uint32 id) { id_ = id; }
	uint32 getId( ) const { return id_; }

	void setOldPos(float x, float z) {
		oldX_ = x;
		oldZ_ = z;
	}
	float oldX() const { return oldX_; }
	float oldZ() const { return oldZ_; }

	//void setPlayerYaw(float yaw) { playerYaw_ = yaw; }
	void setCameraPitch(float pitch) { cameraPitch_ = pitch; }
	float cameraPitch() const { return cameraPitch_; }

	void setHp(int32 hp) { hp_ = hp; }
	int32 hp() const { return hp_; }

	void setLastMoveTimestamp(uint32 timestamp) { lastMoveTimestamp_ = timestamp; }
	uint32 lastMoveTimestamp() const { return lastMoveTimestamp_; }

	void setLastFireTime(Milliseconds time) { lastFireTime_ = time; }
	Milliseconds lastFireTime() const { return lastFireTime_; }
	Milliseconds fireCooldown() const { return fireCooldown_; }

	bool reloading() const { return reloading_; }
	void startReloading() { 
		reloading_ = true;
		reloadStartTime_ = SteadyClock::now().time_since_epoch();
	}
	bool finishReloading() { 
		Milliseconds currTime = SteadyClock::now().time_since_epoch();
		if (currTime - reloadStartTime_ >= reloadCooldown_) {
			reloading_ = false;
			return true;
		}
		return false;
	}

private:
	// object(player) 좌표의 스냅샷을 저장하기 위한 oldX_, oldZ_
	float oldX_{};
	float oldZ_{};
	PhysicState physicState_{};

	mu::Vec3 forward_{};
	mu::Vec3 right_{};
	mu::Vec3 up_{};

	const Model* pModel_ = nullptr;

	//float playerYaw_{};
	float cameraPitch_{};

	uint32 materialSetIdx_ = 0u;
	int32 id_{ -1 };

	int32 hp_{100};

	uint32 lastMoveTimestamp_{0u};

	// 서버 시간을 사용한다.
	Milliseconds lastFireTime_{0ms};
	Milliseconds fireCooldown_{200ms};

	bool reloading_{false};
	Milliseconds reloadStartTime_{0ms};
	Milliseconds reloadCooldown_{2000ms};
};

class Player : public Object {
public:
	Player() = default;
	Player(Object&& base) : Object(std::move(base)) {}
};

class Goblin : public Object {
public:
	Goblin() = default;
	Goblin(Object&& base) : Object(std::move(base)) {}
};

class Cube : public Object {
public:
	Cube() = default;
	Cube(Object&& base) : Object(std::move(base)) {}
};

#endif // room_server_object_hpp
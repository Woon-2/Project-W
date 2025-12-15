#ifndef __object_HPP
#define __object_HPP

#include "gfx.hpp"
#include "collision.hpp"
#include "animation.hpp"
#include "event.hpp"

class AssetManager;
class Object;
class Timer;

class AnimBlenderVanguard : public AnimBlender {
public:
	class EventBus : public IEventBus {
	public:
		void receive(const BasicEvent* event, Seconds deltaTime, EventList& evList, Timer& timer, void* pVoidOwner) override;
	};

	void init(const AssetManager& assetManager);
	// pOwner의 물리 정보에 따라
	// 애니메이션 블렌딩 상태를 갱신한다.
	void update(Seconds deltaTime, void* pOwner) override;
	void onCalcLocal(PassKey<AnimSystem>) override;

	IEventBus* eventBus() override { return &eventBus_; }

private:
	std::vector<AnimFrame> framesBlended_{};
	EventBus eventBus_{};

	Seconds accMotionless_ = 0s;
	Milliseconds cooldownFire_ = 0ms;
	Seconds animTimeIdle_ = 0s;
	Seconds animTimeIdleAim_ = 0s;
	Seconds animTimeRun_ = 0s;
	float tIdle_ = 0.f;
	float tIdleAim_ = 0.f;
	float tRunForward_ = 0.f;
	float tRunBackward_ = 0.f;
	float tRunLeft_ = 0.f;
	float tRunRight_ = 0.f;
	float tHit_ = 0.f;
};

struct PhysicState {
	mu::Vec3 pos{};
	mu::Vec3 velocity{};
	mu::Vec3 omega{};
	mu::NQuat orient{};
	mu::Vec3 scale{};

	std::vector<AABB> aabbs{};
};

// 물체의 렌더링과 관련된 상태
// 물체를 렌더링하는데 필요한 월드 변환 행렬,
// 물체의 바운딩 볼륨을 렌더링하는데 필요한 월드 변환 행렬,
// 모델 정보를 보관한다.
struct RenderState {
	mu::Mat4x4 world;
	mu::Vec3 pos{};
	mu::NQuat orient{};
	mu::Vec3 scale{};
	std::vector<mu::Mat4x4> worldBVs;
	std::unique_ptr<AnimBlender> animBlender;
	const Model* pModel;
};

struct Equipment {
	Bone::SocketType socketType;
	std::unique_ptr<Object> object;
};

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
	class EventBus : public IEventBus {
	public:
		void receive(const BasicEvent* event, Seconds deltaTime, EventList& evList, Timer& timer, void* pVoidOwner) override;
	};

	// @brief 게임 객체의 RenderState와 방향 벡터들을 갱신한다.
	//		RenderState는 이전 PhysicState와 현재 PhysicState를 보간하여 얻어지고,
	//      방향 벡터들은 현재 PhysicState의 내용으로 계산한다.
	// @param deltaTime 마지막 프레임으로부터 경과한 시간
	// @param tPhysicInterpolation 이전 PhysicState와 현재 PhysicState의 보간 비율
	//		(게임 객체가 계산해서 일괄적으로 전달해야 한다.)
	void update(Milliseconds deltaTime, float tPhysicInterpolation);
	void MU_CALLCONV render(GFX& gfx, mu::Mat4x4 offsetXform = mu::Mat4x4());

	void setAnimBlender(AnimSystem& animSystem, const AssetManager& assetManager) {
		renderState_.animBlender = std::make_unique<AnimBlenderVanguard>();
		static_cast<AnimBlenderVanguard*>(renderState_.animBlender.get())->init(assetManager);

		animSystem.trackAnimBlender(renderState_.animBlender.get());
	}

	// 모델을 설정한다.
	// 모델이 있는 게임 객체는 render 시 GFX에 DrawEvent를 제출한다.
	// 모델에 바운딩 볼륨이 존재할 경우, 월드 공간 바운딩 볼륨을 구축한다.
	// (모델의 바운딩 볼륨을 기반으로 게임 객체의 월드 변환을 적용한
	//  월드 공간 바운딩 볼륨을 따로 두어야 월드 공간 충돌 처리가 가능하다.)
	void setModel(const Model* pModel);
	const Model* model() const { return renderState_.pModel; }

	// 게임 객체의 위치를 갱신한다.
	// 이전 PhysicState와 현재 PhysicState의 위치가 모두 갱신된다.
	// 각 PhysicState의 AABB 역시 갱신된다.
	void MU_CALLCONV setPos(mu::Vec3 newPos);
	mu::Vec3 MU_CALLCONV pos() const { return currPhysicState_.pos; }
	// 게임 객체의 속도를 갱신한다.
	// 이전 PhysicState와 현재 PhysicState의 속도가 모두 갱신된다.
	void MU_CALLCONV setVelocity(mu::Vec3 newVelocity);
	mu::Vec3 MU_CALLCONV velocity() const { return currPhysicState_.velocity; }
	// 게임 객체의 각속도를 갱신한다.
	// 이전 PhysicState와 현재 PhysicState의 각속도가 모두 갱신된다.
	void MU_CALLCONV setOmega(mu::Vec3 newOmega);
	mu::Vec3 MU_CALLCONV omega() const { return currPhysicState_.omega; }
	// 게임 객체의 방향을 갱신한다.
	// 이전 PhysicState와 현재 PhysicState의 방향이 모두 갱신된다.
	// 게임 객체의 방향 벡터들도 전부 갱신된다.
	void MU_CALLCONV setOrient(mu::NQuat newOrient);
	mu::NQuat MU_CALLCONV orient() const { return currPhysicState_.orient; }
	// 게임 객체의 크기를 갱신한다.
	// 이전 PhysicState와 현재 PhysicState의 크기가 모두 갱신된다.
	// 각 PhysicState의 AABB 역시 갱신된다.
	void MU_CALLCONV setScale(mu::Vec3 newScale);
	mu::Vec3 MU_CALLCONV scale() const { return currPhysicState_.scale; }

	mu::Vec3 MU_CALLCONV forward() const { return forward_; }
	mu::Vec3 MU_CALLCONV right() const { return right_; }
	mu::Vec3 MU_CALLCONV up() const { return up_; }

	PhysicState& physicState() { return currPhysicState_; }
	const RenderState& renderState() const { return renderState_; }
	// PhysicState를 새로운 상태로 전환한다.
	// 이 함수의 호출 시점의 PhysicState가 prevPhysicState로서 저장된다.
	// 게임 객체에서 호출한다.
	void proceedPhysicState() { prevPhysicState_ = currPhysicState_; }

	// 재질 집합 인덱스를 설정한다.
	void setMaterialSetIdx(u32t idx) { materialSetIdx_ = idx; }
	u32t mateiralSetIdx() const { return materialSetIdx_; }

	void equip(Equipment&& equipment);
	void disequip(Bone::SocketType socketType);

	Equipment* getEquipment(Bone::SocketType socketType);
	const Equipment* getEquipment(Bone::SocketType socketType) const;

	// 바운딩 볼륨 렌더링을 활성화한다.
	void enableBVRendering() { willRenderBV_ = true; }
	// 바운딩 볼륨 렌더링을 비활성화한다.
	void disableBVRendering() { willRenderBV_ = false; }

	void setId( i32t id ) {	id_ = id; }
	i32t getId( ) const { return id_; }

	void MU_CALLCONV setServerPos( mu::Vec3 pos ) {
		prevPhysicState_ = currPhysicState_;
		currPhysicState_.pos = pos;
	}

	IEventBus* eventBus() { return &eventBus_; }


private:
	PhysicState prevPhysicState_{};
	PhysicState currPhysicState_{};

	RenderState renderState_{};

	std::vector<Equipment> equipments_{};

	EventBus eventBus_{};

	bool willRenderBV_ = false;

	mu::Vec3 forward_{};
	mu::Vec3 right_{};
	mu::Vec3 up_{};

	u32t materialSetIdx_ = 0u;
	i32t id_{ -1 };
};

#endif	// __object_HPP
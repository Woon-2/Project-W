#ifndef __object_HPP
#define __object_HPP

#include "gfx.hpp"
#include "collision.hpp"
#include "animation.hpp"
#include "event.hpp"

class AssetManager;
class Object;
class Timer;

class AnimBlenderPlayer : public AnimBlender {
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

	Seconds runAnimTime() const { return animTimeRun_; }
	Seconds runDuration() const { return targetClip("Player_Run_Forward")->duration; }
	bool isRunning() const { return (tRunForward_ + tRunBackward_ + tRunLeft_ + tRunRight_) > 0.f; }

private:
	std::vector<AnimFrame> framesBlended_{};
	EventBus eventBus_{};

	Seconds accMotionless_ = 0s;
	Milliseconds cooldownHit_ = 0ms;
	Milliseconds cooldownDeath_ = 0ms;
	Seconds animTimeIdle0_ = 0s;
	Seconds animTimeIdle1_ = 0s;
	Seconds animTimeHit_ = 0s;
	Seconds animTimeRun_ = 0s;
	Seconds animTimeDeath_ = 0s;
	float tIdle0_ = 0.f;
	float tIdle1_ = 0.f;
	float tRunForward_ = 0.f;
	float tRunBackward_ = 0.f;
	float tRunLeft_ = 0.f;
	float tRunRight_ = 0.f;
	// hit 애니메이션 블렌딩 비율은 death 다음으로 가장 우선순위가 높게 계산된다.
	// 다른 모든 애니메이션의 블렌딩 비율을 낮추고 최대 0.75만큼의 비율을 차지한다.
	// 모든 블렌딩이 일어난 후에 결과 프레임과 hit 애니메이션 프레임을
	// tHit_으로 보간하게 된다.
	float tHit_ = 0.f;
	// death 애니메이션은 가장 우선순위가 높게 계산된다.
	// cooldownDeath_의 값이 0보다 큰 동안은 다른 애니메이션과 최종적으로 블렌딩되며
	// 페이드인이 이루어지고, cooldownDeath_의 값이 0이 되면 완전히 1의 비율을 차지한다.
	float tDeath_ = 0.f;
	bool dead_ = false;
};

class AnimBlenderGoblin : public AnimBlender {
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

	Milliseconds cooldownAttack_ = 0ms;
	Milliseconds cooldownHit_ = 0ms;
	Milliseconds cooldownDeath_ = 0ms;
	Seconds animTimeIdle_ = 0s;
	Seconds animTimeWalk_ = 0s;
	Seconds animTimeAttack_ = 0s;
	Seconds animTimeHit_ = 0s;
	Seconds animTimeDeath_ = 0s;
	float tIdle_ = 0.f;
	float tWalk_ = 0.f;
	float tAttack_ = 0.f;
	// hit 애니메이션 블렌딩 비율은 death 다음으로 가장 우선순위가 높게 계산된다.
	// 다른 모든 애니메이션의 블렌딩 비율을 낮추고 최대 0.75만큼의 비율을 차지한다.
	// 모든 블렌딩이 일어난 후에 결과 프레임과 hit 애니메이션 프레임을
	// tHit_으로 보간하게 된다.
	float tHit_ = 0.f;
	// death 애니메이션은 가장 우선순위가 높게 계산된다.
	// cooldownDeath_의 값이 0보다 큰 동안은 다른 애니메이션과 최종적으로 블렌딩되며
	// 페이드인이 이루어지고, cooldownDeath_의 값이 0이 되면 완전히 1의 비율을 차지한다.
	float tDeath_ = 0.f;
	bool dead_ = false;
};

class AnimBlenderAnubis : public AnimBlender {
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

	Milliseconds cooldownAttack_ = 0ms;
	Milliseconds cooldownHit_ = 0ms;
	Milliseconds cooldownDeath_ = 0ms;
	Seconds animTimeIdle_ = 0s;
	Seconds animTimeWalk_ = 0s;
	Seconds animTimeAttack_ = 0s;
	Seconds animTimeHit_ = 0s;
	Seconds animTimeDeath_ = 0s;
	float tIdle_ = 0.f;
	float tWalk_ = 0.f;
	float tAttack_ = 0.f;
	// hit 애니메이션 블렌딩 비율은 death 다음으로 가장 우선순위가 높게 계산된다.
	// 다른 모든 애니메이션의 블렌딩 비율을 낮추고 최대 0.75만큼의 비율을 차지한다.
	// 모든 블렌딩이 일어난 후에 결과 프레임과 hit 애니메이션 프레임을
	// tHit_으로 보간하게 된다.
	float tHit_ = 0.f;
	// death 애니메이션은 가장 우선순위가 높게 계산된다.
	// cooldownDeath_의 값이 0보다 큰 동안은 다른 애니메이션과 최종적으로 블렌딩되며
	// 페이드인이 이루어지고, cooldownDeath_의 값이 0이 되면 완전히 1의 비율을 차지한다.
	float tDeath_ = 0.f;
	bool dead_ = false;
};

class AnimBlenderBat : public AnimBlender {
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

	Milliseconds cooldownAttack_ = 0ms;
	Milliseconds cooldownHit_ = 0ms;
	Milliseconds cooldownDeath_ = 0ms;
	Seconds animTimeIdle_ = 0s;
	Seconds animTimeWalk_ = 0s;
	Seconds animTimeAttack_ = 0s;
	Seconds animTimeHit_ = 0s;
	Seconds animTimeDeath_ = 0s;
	float tIdle_ = 0.f;
	float tWalk_ = 0.f;
	float tAttack_ = 0.f;
	// hit 애니메이션 블렌딩 비율은 death 다음으로 가장 우선순위가 높게 계산된다.
	// 다른 모든 애니메이션의 블렌딩 비율을 낮추고 최대 0.75만큼의 비율을 차지한다.
	// 모든 블렌딩이 일어난 후에 결과 프레임과 hit 애니메이션 프레임을
	// tHit_으로 보간하게 된다.
	float tHit_ = 0.f;
	// death 애니메이션은 가장 우선순위가 높게 계산된다.
	// cooldownDeath_의 값이 0보다 큰 동안은 다른 애니메이션과 최종적으로 블렌딩되며
	// 페이드인이 이루어지고, cooldownDeath_의 값이 0이 되면 완전히 1의 비율을 차지한다.
	float tDeath_ = 0.f;
	bool dead_ = false;
};

class AnimBlenderBomber : public AnimBlender {
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

	Milliseconds cooldownAttack_ = 0ms;
	Milliseconds cooldownHit_ = 0ms;
	Milliseconds cooldownDeath_ = 0ms;
	Seconds animTimeIdle_ = 0s;
	Seconds animTimeWalk_ = 0s;
	Seconds animTimeAttack_ = 0s;
	Seconds animTimeHit_ = 0s;
	Seconds animTimeDeath_ = 0s;
	float tIdle_ = 0.f;
	float tWalk_ = 0.f;
	float tAttack_ = 0.f;
	// hit 애니메이션 블렌딩 비율은 death 다음으로 가장 우선순위가 높게 계산된다.
	// 다른 모든 애니메이션의 블렌딩 비율을 낮추고 최대 0.75만큼의 비율을 차지한다.
	// 모든 블렌딩이 일어난 후에 결과 프레임과 hit 애니메이션 프레임을
	// tHit_으로 보간하게 된다.
	float tHit_ = 0.f;
	// death 애니메이션은 가장 우선순위가 높게 계산된다.
	// cooldownDeath_의 값이 0보다 큰 동안은 다른 애니메이션과 최종적으로 블렌딩되며
	// 페이드인이 이루어지고, cooldownDeath_의 값이 0이 되면 완전히 1의 비율을 차지한다.
	float tDeath_ = 0.f;
	bool dead_ = false;
};

class AnimBlenderDemon : public AnimBlender {
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

	Milliseconds cooldownAttack_ = 0ms;
	Milliseconds cooldownHit_ = 0ms;
	Milliseconds cooldownDeath_ = 0ms;
	Seconds animTimeIdle_ = 0s;
	Seconds animTimeWalk_ = 0s;
	Seconds animTimeAttack_ = 0s;
	Seconds animTimeHit_ = 0s;
	Seconds animTimeDeath_ = 0s;
	float tIdle_ = 0.f;
	float tWalk_ = 0.f;
	float tAttack_ = 0.f;
	// hit 애니메이션 블렌딩 비율은 death 다음으로 가장 우선순위가 높게 계산된다.
	// 다른 모든 애니메이션의 블렌딩 비율을 낮추고 최대 0.75만큼의 비율을 차지한다.
	// 모든 블렌딩이 일어난 후에 결과 프레임과 hit 애니메이션 프레임을
	// tHit_으로 보간하게 된다.
	float tHit_ = 0.f;
	// death 애니메이션은 가장 우선순위가 높게 계산된다.
	// cooldownDeath_의 값이 0보다 큰 동안은 다른 애니메이션과 최종적으로 블렌딩되며
	// 페이드인이 이루어지고, cooldownDeath_의 값이 0이 되면 완전히 1의 비율을 차지한다.
	float tDeath_ = 0.f;
	bool dead_ = false;
};

class AnimBlenderDragon : public AnimBlender {
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

	Milliseconds cooldownAttack_ = 0ms;
	Milliseconds cooldownHit_ = 0ms;
	Milliseconds cooldownDeath_ = 0ms;
	Seconds animTimeIdle_ = 0s;
	Seconds animTimeWalk_ = 0s;
	Seconds animTimeAttack_ = 0s;
	Seconds animTimeHit_ = 0s;
	Seconds animTimeDeath_ = 0s;
	float tIdle_ = 0.f;
	float tWalk_ = 0.f;
	float tAttack_ = 0.f;
	// hit 애니메이션 블렌딩 비율은 death 다음으로 가장 우선순위가 높게 계산된다.
	// 다른 모든 애니메이션의 블렌딩 비율을 낮추고 최대 0.75만큼의 비율을 차지한다.
	// 모든 블렌딩이 일어난 후에 결과 프레임과 hit 애니메이션 프레임을
	// tHit_으로 보간하게 된다.
	float tHit_ = 0.f;
	// death 애니메이션은 가장 우선순위가 높게 계산된다.
	// cooldownDeath_의 값이 0보다 큰 동안은 다른 애니메이션과 최종적으로 블렌딩되며
	// 페이드인이 이루어지고, cooldownDeath_의 값이 0이 되면 완전히 1의 비율을 차지한다.
	float tDeath_ = 0.f;
	bool dead_ = false;
};

class AnimBlenderEyeball : public AnimBlender {
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

	Milliseconds cooldownAttack_ = 0ms;
	Milliseconds cooldownHit_ = 0ms;
	Milliseconds cooldownDeath_ = 0ms;
	Seconds animTimeIdle_ = 0s;
	Seconds animTimeWalk_ = 0s;
	Seconds animTimeAttack_ = 0s;
	Seconds animTimeHit_ = 0s;
	Seconds animTimeDeath_ = 0s;
	float tIdle_ = 0.f;
	float tWalk_ = 0.f;
	float tAttack_ = 0.f;
	// hit 애니메이션 블렌딩 비율은 death 다음으로 가장 우선순위가 높게 계산된다.
	// 다른 모든 애니메이션의 블렌딩 비율을 낮추고 최대 0.75만큼의 비율을 차지한다.
	// 모든 블렌딩이 일어난 후에 결과 프레임과 hit 애니메이션 프레임을
	// tHit_으로 보간하게 된다.
	float tHit_ = 0.f;
	// death 애니메이션은 가장 우선순위가 높게 계산된다.
	// cooldownDeath_의 값이 0보다 큰 동안은 다른 애니메이션과 최종적으로 블렌딩되며
	// 페이드인이 이루어지고, cooldownDeath_의 값이 0이 되면 완전히 1의 비율을 차지한다.
	float tDeath_ = 0.f;
	bool dead_ = false;
};

class AnimBlenderFishman : public AnimBlender {
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

	Milliseconds cooldownAttack_ = 0ms;
	Milliseconds cooldownHit_ = 0ms;
	Milliseconds cooldownDeath_ = 0ms;
	Seconds animTimeIdle_ = 0s;
	Seconds animTimeWalk_ = 0s;
	Seconds animTimeAttack_ = 0s;
	Seconds animTimeHit_ = 0s;
	Seconds animTimeDeath_ = 0s;
	float tIdle_ = 0.f;
	float tWalk_ = 0.f;
	float tAttack_ = 0.f;
	// hit 애니메이션 블렌딩 비율은 death 다음으로 가장 우선순위가 높게 계산된다.
	// 다른 모든 애니메이션의 블렌딩 비율을 낮추고 최대 0.75만큼의 비율을 차지한다.
	// 모든 블렌딩이 일어난 후에 결과 프레임과 hit 애니메이션 프레임을
	// tHit_으로 보간하게 된다.
	float tHit_ = 0.f;
	// death 애니메이션은 가장 우선순위가 높게 계산된다.
	// cooldownDeath_의 값이 0보다 큰 동안은 다른 애니메이션과 최종적으로 블렌딩되며
	// 페이드인이 이루어지고, cooldownDeath_의 값이 0이 되면 완전히 1의 비율을 차지한다.
	float tDeath_ = 0.f;
	bool dead_ = false;
};

class AnimBlenderGargoyle : public AnimBlender {
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

	Milliseconds cooldownAttack_ = 0ms;
	Milliseconds cooldownHit_ = 0ms;
	Milliseconds cooldownDeath_ = 0ms;
	Seconds animTimeIdle_ = 0s;
	Seconds animTimeWalk_ = 0s;
	Seconds animTimeAttack_ = 0s;
	Seconds animTimeHit_ = 0s;
	Seconds animTimeDeath_ = 0s;
	float tIdle_ = 0.f;
	float tWalk_ = 0.f;
	float tAttack_ = 0.f;
	// hit 애니메이션 블렌딩 비율은 death 다음으로 가장 우선순위가 높게 계산된다.
	// 다른 모든 애니메이션의 블렌딩 비율을 낮추고 최대 0.75만큼의 비율을 차지한다.
	// 모든 블렌딩이 일어난 후에 결과 프레임과 hit 애니메이션 프레임을
	// tHit_으로 보간하게 된다.
	float tHit_ = 0.f;
	// death 애니메이션은 가장 우선순위가 높게 계산된다.
	// cooldownDeath_의 값이 0보다 큰 동안은 다른 애니메이션과 최종적으로 블렌딩되며
	// 페이드인이 이루어지고, cooldownDeath_의 값이 0이 되면 완전히 1의 비율을 차지한다.
	float tDeath_ = 0.f;
	bool dead_ = false;
};

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
	Object() = default;
	virtual ~Object() = default;
	Object(Object&&) noexcept = default;
	Object& operator=(Object&&) noexcept = default;
		 
	// @brief 게임 객체의 RenderState와 방향 벡터들을 갱신한다.
	//		RenderState는 이전 PhysicState와 현재 PhysicState를 보간하여 얻어지고,
	//      방향 벡터들은 현재 PhysicState의 내용으로 계산한다.
	// @param deltaTime 마지막 프레임으로부터 경과한 시간
	// @param tPhysicInterpolation 이전 PhysicState와 현재 PhysicState의 보간 비율
	//		(게임 객체가 계산해서 일괄적으로 전달해야 한다.)
	virtual void update(Milliseconds deltaTime, float tPhysicInterpolation);
	virtual void MU_CALLCONV render(GFX& gfx, mu::Mat4x4 offsetXform = mu::Mat4x4());

	virtual void setAnimBlender(AnimSystem& animSystem, const AssetManager& assetManager) {}

	// 모델을 설정한다.
	// 모델이 있는 게임 객체는 render 시 GFX에 DrawEvent를 제출한다.
	// 모델에 바운딩 볼륨이 존재할 경우, 월드 공간 바운딩 볼륨을 구축한다.
	// (모델의 바운딩 볼륨을 기반으로 게임 객체의 월드 변환을 적용한
	//  월드 공간 바운딩 볼륨을 따로 두어야 월드 공간 충돌 처리가 가능하다.)
	void setModel(const Model* pModel);

	// 지정한 PhysicState의 pos/scale/orient를 기반으로 BVH를 world-space로 재빌드한다.
	// PhysicSystem 등 외부 시스템에서 직접 pos를 수정한 뒤 호출한다.
	void rebuildBVH(PhysicState& state) const;
	const Model* model() const { return renderState_.pModel; }

	// 게임 객체의 위치를 갱신한다.
	// 이전 PhysicState와 현재 PhysicState의 위치가 모두 갱신된다.
	// 각 PhysicState의 BVH 역시 갱신된다.
	void MU_CALLCONV setPos(mu::Vec3 newPos);
	// 게임 객체의 위치를 갱신한다.
	// 현재 PhysicState의 위치만 갱신된다.
	void MU_CALLCONV setCurrPos(mu::Vec3 pos);
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
	// 각 PhysicState의 BVH 역시 갱신된다.
	void MU_CALLCONV setScale(mu::Vec3 newScale);
	mu::Vec3 MU_CALLCONV scale() const { return currPhysicState_.scale; }

	mu::Vec3 MU_CALLCONV forward() const { return forward_; }
	mu::Vec3 MU_CALLCONV right() const { return right_; }
	mu::Vec3 MU_CALLCONV up() const { return up_; }

	PhysicState& physicState() { return currPhysicState_; }
	const PhysicState& physicState() const { return currPhysicState_; }
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

	virtual IEventBus* eventBus() { return &gNullEventBus; }

	void setHp(i32t hp) { hp_ = hp; }
	i32t hp() const { return hp_; }

protected:
	PhysicState prevPhysicState_{};
	PhysicState currPhysicState_{};

	RenderState renderState_{};

	std::vector<Equipment> equipments_{};

	bool willRenderBV_ = false;

	mu::Vec3 forward_{};
	mu::Vec3 right_{};
	mu::Vec3 up_{};

	u32t materialSetIdx_ = 0u;
	i32t id_{ -1 };

	i32t hp_{};

private:
};

class Cube : public Object {
public:
	Cube() = default;
	Cube(Object&& base)
		: Object(std::move(base)) {}
private:

};

class Player : public Object {
public:
	Player() = default;
	Player(Object&& base)
		: Object(std::move(base)) {}

	class EventBus : public IEventBus {
	public:
		void receive(const BasicEvent* event, Seconds deltaTime, EventList& evList, Timer& timer, void* pVoidOwner) override;
	};

	IEventBus* eventBus() override { return &eventBus_; }

	void setAnimBlender(AnimSystem& animSystem, const AssetManager& assetManager) override {
		renderState_.animBlender = std::make_unique<AnimBlenderPlayer>();
		static_cast<AnimBlenderPlayer*>(renderState_.animBlender.get())->init(assetManager);

		animSystem.trackAnimBlender(renderState_.animBlender.get());
	}

private:
	EventBus eventBus_{};
};

class Goblin : public Object {
public:
	Goblin() = default;
	Goblin(Object&& base)
		: Object(std::move(base)) {}

	class EventBus : public IEventBus {
	public:
		void receive(const BasicEvent* event, Seconds deltaTime, EventList& evList, Timer& timer, void* pVoidOwner) override;
	};

	IEventBus* eventBus() override { return &eventBus_; }

	void setAnimBlender(AnimSystem& animSystem, const AssetManager& assetManager) override {
		renderState_.animBlender = std::make_unique<AnimBlenderGoblin>();
		static_cast<AnimBlenderGoblin*>(renderState_.animBlender.get())->init(assetManager);

		animSystem.trackAnimBlender(renderState_.animBlender.get());
	}

private:
	EventBus eventBus_{};
};

class Anubis : public Object {
public:
	Anubis() = default;
	Anubis(Object&& base)
		: Object(std::move(base)) {}

	class EventBus : public IEventBus {
	public:
		void receive(const BasicEvent* event, Seconds deltaTime, EventList& evList, Timer& timer, void* pVoidOwner) override;
	};

	IEventBus* eventBus() override { return &eventBus_; }

	void setAnimBlender(AnimSystem& animSystem, const AssetManager& assetManager) override {
		renderState_.animBlender = std::make_unique<AnimBlenderAnubis>();
		static_cast<AnimBlenderAnubis*>(renderState_.animBlender.get())->init(assetManager);

		animSystem.trackAnimBlender(renderState_.animBlender.get());
	}

private:
	EventBus eventBus_{};
};

class Bat : public Object {
public:
	Bat() = default;
	Bat(Object&& base)
		: Object(std::move(base)) {}

	class EventBus : public IEventBus {
	public:
		void receive(const BasicEvent* event, Seconds deltaTime, EventList& evList, Timer& timer, void* pVoidOwner) override;
	};

	IEventBus* eventBus() override { return &eventBus_; }

	void setAnimBlender(AnimSystem& animSystem, const AssetManager& assetManager) override {
		renderState_.animBlender = std::make_unique<AnimBlenderBat>();
		static_cast<AnimBlenderBat*>(renderState_.animBlender.get())->init(assetManager);

		animSystem.trackAnimBlender(renderState_.animBlender.get());
	}

private:
	EventBus eventBus_{};
};

class Bomber : public Object {
public:
	Bomber() = default;
	Bomber(Object&& base)
		: Object(std::move(base)) {}

	class EventBus : public IEventBus {
	public:
		void receive(const BasicEvent* event, Seconds deltaTime, EventList& evList, Timer& timer, void* pVoidOwner) override;
	};

	IEventBus* eventBus() override { return &eventBus_; }

	void setAnimBlender(AnimSystem& animSystem, const AssetManager& assetManager) override {
		renderState_.animBlender = std::make_unique<AnimBlenderBomber>();
		static_cast<AnimBlenderBomber*>(renderState_.animBlender.get())->init(assetManager);

		animSystem.trackAnimBlender(renderState_.animBlender.get());
	}

private:
	EventBus eventBus_{};
};

class Demon : public Object {
public:
	Demon() = default;
	Demon(Object&& base)
		: Object(std::move(base)) {}

	class EventBus : public IEventBus {
	public:
		void receive(const BasicEvent* event, Seconds deltaTime, EventList& evList, Timer& timer, void* pVoidOwner) override;
	};

	IEventBus* eventBus() override { return &eventBus_; }

	void setAnimBlender(AnimSystem& animSystem, const AssetManager& assetManager) override {
		renderState_.animBlender = std::make_unique<AnimBlenderDemon>();
		static_cast<AnimBlenderDemon*>(renderState_.animBlender.get())->init(assetManager);

		animSystem.trackAnimBlender(renderState_.animBlender.get());
	}

private:
	EventBus eventBus_{};
};

class Dragon : public Object {
public:
	Dragon() = default;
	Dragon(Object&& base)
		: Object(std::move(base)) {}

	class EventBus : public IEventBus {
	public:
		void receive(const BasicEvent* event, Seconds deltaTime, EventList& evList, Timer& timer, void* pVoidOwner) override;
	};

	IEventBus* eventBus() override { return &eventBus_; }

	void setAnimBlender(AnimSystem& animSystem, const AssetManager& assetManager) override {
		renderState_.animBlender = std::make_unique<AnimBlenderDragon>();
		static_cast<AnimBlenderDragon*>(renderState_.animBlender.get())->init(assetManager);

		animSystem.trackAnimBlender(renderState_.animBlender.get());
	}

private:
	EventBus eventBus_{};
};

class Eyeball : public Object {
public:
	Eyeball() = default;
	Eyeball(Object&& base)
		: Object(std::move(base)) {}

	class EventBus : public IEventBus {
	public:
		void receive(const BasicEvent* event, Seconds deltaTime, EventList& evList, Timer& timer, void* pVoidOwner) override;
	};

	IEventBus* eventBus() override { return &eventBus_; }

	void setAnimBlender(AnimSystem& animSystem, const AssetManager& assetManager) override {
		renderState_.animBlender = std::make_unique<AnimBlenderEyeball>();
		static_cast<AnimBlenderEyeball*>(renderState_.animBlender.get())->init(assetManager);

		animSystem.trackAnimBlender(renderState_.animBlender.get());
	}

private:
	EventBus eventBus_{};
};

class Fishman : public Object {
public:
	Fishman() = default;
	Fishman(Object&& base)
		: Object(std::move(base)) {}

	class EventBus : public IEventBus {
	public:
		void receive(const BasicEvent* event, Seconds deltaTime, EventList& evList, Timer& timer, void* pVoidOwner) override;
	};

	IEventBus* eventBus() override { return &eventBus_; }

	void setAnimBlender(AnimSystem& animSystem, const AssetManager& assetManager) override {
		renderState_.animBlender = std::make_unique<AnimBlenderFishman>();
		static_cast<AnimBlenderFishman*>(renderState_.animBlender.get())->init(assetManager);

		animSystem.trackAnimBlender(renderState_.animBlender.get());
	}

private:
	EventBus eventBus_{};
};

class Gargoyle : public Object {
public:
	Gargoyle() = default;
	Gargoyle(Object&& base)
		: Object(std::move(base)) {}

	class EventBus : public IEventBus {
	public:
		void receive(const BasicEvent* event, Seconds deltaTime, EventList& evList, Timer& timer, void* pVoidOwner) override;
	};

	IEventBus* eventBus() override { return &eventBus_; }

	void setAnimBlender(AnimSystem& animSystem, const AssetManager& assetManager) override {
		renderState_.animBlender = std::make_unique<AnimBlenderGargoyle>();
		static_cast<AnimBlenderGargoyle*>(renderState_.animBlender.get())->init(assetManager);

		animSystem.trackAnimBlender(renderState_.animBlender.get());
	}

private:
	EventBus eventBus_{};
};

struct TerrainData;

// Game entity wrapping TerrainData with a world transform.
// Mirrors the Object/Model relationship: TerrainObject owns placement,
// TerrainData owns the heavy mesh and texture resources.
class TerrainObject : public Object {
public:
	TerrainObject() = default;
	TerrainObject(Object&& base) : Object(std::move(base)) {}

	void setTerrainData(const TerrainData* data) { terrainData_ = data; }
	const TerrainData* terrainData() const { return terrainData_; }

	void MU_CALLCONV render(GFX& gfx, mu::Mat4x4 offsetXform = mu::Mat4x4()) override;

private:
	const TerrainData* terrainData_ = nullptr;
};

#endif	// __object_HPP
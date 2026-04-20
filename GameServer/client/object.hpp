#ifndef __object_HPP
#define __object_HPP

#include "gfx.hpp"
#include "collision.hpp"
#include "animation.hpp"
#include "event.hpp"
#include "rigidBody.hpp"

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

	void triggerDeath() override {
		animTimeDeath_ = 0s;
		cooldownDeath_ = 200ms;
		dead_ = true;
	}

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

	void triggerDeath() override {
		animTimeDeath_ = 0s;
		cooldownDeath_ = 200ms;
		dead_ = true;
	}

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
	bool shouldCull = false;
	bool willOcclude = false;
};

struct Equipment {
	Bone::SocketType socketType;
	std::unique_ptr<Object> object;
};

// Object는 RigidBody (인라인 멤버 body_)와 RenderState 두 층위로 관리된다.
// 위치/방향/크기 등 물리량은 body_에 보관되고,
// 월드 변환 행렬·모델 정보 등 렌더링 관련 정보는 RenderState에 보관된다.
//
// PhysicsWorld::step()이 body_의 prev/curr를 갱신하고,
// Object::update()가 두 스냅샷을 보간하여 RenderState를 갱신한다.
class Object {
public:
	Object() = default;
	virtual ~Object() = default;
	Object(Object&&) noexcept = default;
	Object& operator=(Object&&) noexcept = default;
		 
	// @brief 게임 객체의 RenderState와 방향 벡터들을 갱신한다.
	//		RenderState는 body_.prev()와 body_.curr()를 보간하여 얻어지고,
	//      방향 벡터들은 body_.curr()의 내용으로 계산한다.
	// @param deltaTime 마지막 프레임으로부터 경과한 시간
	// @param tPhysicInterpolation body_.prev()와 body_.curr()의 보간 비율
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

	// body_의 현재 pos/scale/orient와 renderState_의 모델/애니메이션을 바탕으로
	// body_.worldBVH()를 월드 공간으로 재빌드한다.
	// setPos/setOrient/setScale/setModel 호출 시 자동으로 호출되며,
	// PhysicsWorld가 integrate 후 BVH를 동기화하기 위해 콜백으로도 사용된다.
	void rebuildBodyBVH();
	const Model* model() const { return renderState_.pModel; }

	// 위치를 갱신한다. prev/curr 모두 동기화 (텔레포트: 보간 아티팩트 없음).
	void MU_CALLCONV setPos(mu::Vec3 newPos);
	// curr 위치만 갱신한다 (PhysicsWorld가 적분 후 호출).
	void MU_CALLCONV setCurrPos(mu::Vec3 pos);
	mu::Vec3 MU_CALLCONV pos() const { return body_.pos(); }
	// 속도(linearVel)를 갱신한다. 매 프레임 game 로직이 0으로 초기화 후 설정.
	void MU_CALLCONV setVelocity(mu::Vec3 newVelocity);
	mu::Vec3 MU_CALLCONV velocity() const { return body_.linearVel(); }
	// 각속도를 갱신한다.
	void MU_CALLCONV setOmega(mu::Vec3 newOmega);
	mu::Vec3 MU_CALLCONV omega() const { return body_.omega(); }
	// 방향을 갱신한다. prev/curr 모두 동기화. 방향 벡터도 갱신.
	void MU_CALLCONV setOrient(mu::NQuat newOrient);
	mu::NQuat MU_CALLCONV orient() const { return body_.orient(); }
	// 크기를 갱신한다. prev/curr 모두 동기화. BVH 재빌드.
	void MU_CALLCONV setScale(mu::Vec3 newScale);
	mu::Vec3 MU_CALLCONV scale() const { return body_.scale(); }

	mu::Vec3 MU_CALLCONV forward() const { return forward_; }
	mu::Vec3 MU_CALLCONV right() const { return right_; }
	mu::Vec3 MU_CALLCONV up() const { return up_; }

	// Physics body accessor. PhysicsWorld 등록 시 &body()를 전달한다.
	RigidBody&       body()       { return body_; }
	const RigidBody& body() const { return body_; }
	// CombatSystem/DebugBVView 등 BVH 접근용 편의 accessor.
	const BVH& worldBVH() const { return body_.worldBVH(); }
	const RenderState& renderState() const { return renderState_; }
	AnimBlender* animBlender() const { return renderState_.animBlender.get(); }

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

	void setDead(bool dead) {
		isDead_ = dead;
		if (dead && renderState_.animBlender) {
			renderState_.animBlender->triggerDeath();
		}
	}
	bool isDead() const { return isDead_; }
	void setMaxHp(i32t v) { maxHp_ = v; }
	i32t maxHp() const    { return maxHp_; }

	void setCulled(bool culled) { renderState_.shouldCull = culled; }
	void activateOcclusion(bool willOcclude) { renderState_.willOcclude = true; }

protected:
	RigidBody body_{};

	RenderState renderState_{};

	std::vector<Equipment> equipments_{};

	bool willRenderBV_ = false;

	mu::Vec3 forward_{};
	mu::Vec3 right_{};
	mu::Vec3 up_{};

	u32t materialSetIdx_ = 0u;
	i32t id_{ -1 };

	i32t hp_{};
	bool isDead_ = false;
	i32t maxHp_{};

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

	// 원격 플레이어 네트워크 보간 상태. 로컬 플레이어에서는 사용되지 않음.
	// onlineGame.cpp의 Game 루프에서 관리됨.
	Seconds netInterpDuration_{ 1s / 20.f };
	Seconds netInterpAcc_{ 0s };

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
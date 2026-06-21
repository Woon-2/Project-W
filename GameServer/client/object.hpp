#ifndef __object_HPP
#define __object_HPP

#include "gfx.hpp"
#include "collision.hpp"
#include "animation.hpp"
#include "event.hpp"
#include "rigidBody.hpp"
#include "ragdoll.hpp"

class AssetManager;
class Object;
class Timer;
class PhysicsWorld;

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

// 고블린 애니메이션 블렌더. 5-클립 세트(Idle/Walk/Attack/Hit/Death)를
// 속력 기반으로 블렌딩한다. 몬스터별 클립이 추가될 수 있으므로 각 몬스터마다
// 독립된 블렌더 클래스를 둔다(공용 블렌더로 일반화하지 않음).
class AnimBlenderGoblin : public AnimBlender {
public:
	class EventBus : public IEventBus {
	public:
		void receive(const BasicEvent* event, Seconds deltaTime, EventList& evList, Timer& timer, void* pVoidOwner) override;
	};

	void init(const Model* model, const std::vector<std::shared_ptr<AnimClip>>& anims);
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

	// 다중 공격 클립 지원: 로드된 공격 클립 풀네임의 순서 목록.
	// lua PlayAnimation.attackIndex가 이 목록의 인덱스로 해석된다(EvAttack로 전파).
	// init에서 실제 로드된 클립만 채우며, 비면 공격 애니메이션이 비활성화된다.
	std::vector<std::string> attackClips_{};
	std::string currentAttackClip_{};   // 현재 선택된 공격 클립(비면 공격 없음)
};

// 뱀 애니메이션 블렌더. AnimBlenderGoblin과 동일한 5-클립 구조이나
// 향후 뱀 고유 클립 추가를 위해 별도 클래스로 둔다.
class AnimBlenderSnake : public AnimBlender {
public:
	class EventBus : public IEventBus {
	public:
		void receive(const BasicEvent* event, Seconds deltaTime, EventList& evList, Timer& timer, void* pVoidOwner) override;
	};

	void init(const Model* model, const std::vector<std::shared_ptr<AnimClip>>& anims);
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
	float tHit_ = 0.f;
	float tDeath_ = 0.f;
	bool dead_ = false;

	// 다중 공격 클립 지원([[AnimBlenderGoblin]]과 동일). attackClips_ 인덱스 = attackIndex.
	std::vector<std::string> attackClips_{};
	std::string currentAttackClip_{};
};

// 버섯 애니메이션 블렌더. 동일한 5-클립 구조이나 향후 버섯 고유 클립
// 추가를 위해 별도 클래스로 둔다.
class AnimBlenderMushroom : public AnimBlender {
public:
	class EventBus : public IEventBus {
	public:
		void receive(const BasicEvent* event, Seconds deltaTime, EventList& evList, Timer& timer, void* pVoidOwner) override;
	};

	void init(const Model* model, const std::vector<std::shared_ptr<AnimClip>>& anims);
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
	float tHit_ = 0.f;
	float tDeath_ = 0.f;
	bool dead_ = false;

	// 다중 공격 클립 지원([[AnimBlenderGoblin]]과 동일). attackClips_ 인덱스 = attackIndex.
	std::vector<std::string> attackClips_{};
	std::string currentAttackClip_{};
};

// Monster skill-caster AnimBlenders (Bomber / Birdy / Slime / Treant). Same shape
// as AnimBlenderGoblin; only the clip prefix + attack-clip list differ (set in init).
class AnimBlenderBomber : public AnimBlender {
public:
	class EventBus : public IEventBus {
	public:
		void receive(const BasicEvent* event, Seconds deltaTime, EventList& evList, Timer& timer, void* pVoidOwner) override;
	};

	void init(const Model* model, const std::vector<std::shared_ptr<AnimClip>>& anims);
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
	float tHit_ = 0.f;
	float tDeath_ = 0.f;
	bool dead_ = false;
	std::vector<std::string> attackClips_{};
	std::string currentAttackClip_{};
};

class AnimBlenderBirdy : public AnimBlender {
public:
	class EventBus : public IEventBus {
	public:
		void receive(const BasicEvent* event, Seconds deltaTime, EventList& evList, Timer& timer, void* pVoidOwner) override;
	};

	void init(const Model* model, const std::vector<std::shared_ptr<AnimClip>>& anims);
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
	float tHit_ = 0.f;
	float tDeath_ = 0.f;
	bool dead_ = false;
	std::vector<std::string> attackClips_{};
	std::string currentAttackClip_{};
};

class AnimBlenderSlime : public AnimBlender {
public:
	class EventBus : public IEventBus {
	public:
		void receive(const BasicEvent* event, Seconds deltaTime, EventList& evList, Timer& timer, void* pVoidOwner) override;
	};

	void init(const Model* model, const std::vector<std::shared_ptr<AnimClip>>& anims);
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
	float tHit_ = 0.f;
	float tDeath_ = 0.f;
	bool dead_ = false;
	std::vector<std::string> attackClips_{};
	std::string currentAttackClip_{};
};

class AnimBlenderTreant : public AnimBlender {
public:
	class EventBus : public IEventBus {
	public:
		void receive(const BasicEvent* event, Seconds deltaTime, EventList& evList, Timer& timer, void* pVoidOwner) override;
	};

	void init(const Model* model, const std::vector<std::shared_ptr<AnimClip>>& anims);
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
	float tHit_ = 0.f;
	float tDeath_ = 0.f;
	bool dead_ = false;
	std::vector<std::string> attackClips_{};
	std::string currentAttackClip_{};
};

// Temporary boss caster blender: the current boss resource only provides
// Boss_Idle / Boss_Walk / Boss_Attack, so this intentionally ignores Hit/Death.
class AnimBlenderBoss : public AnimBlender {
public:
	class EventBus : public IEventBus {
	public:
		void receive(const BasicEvent* event, Seconds deltaTime, EventList& evList, Timer& timer, void* pVoidOwner) override;
	};

	void init(const Model* model, const std::vector<std::shared_ptr<AnimClip>>& anims);
	void update(Seconds deltaTime, void* pOwner) override;
	void onCalcLocal(PassKey<AnimSystem>) override;

	IEventBus* eventBus() override { return &eventBus_; }

private:
	std::vector<AnimFrame> framesBlended_{};
	EventBus eventBus_{};

	Milliseconds cooldownAttack_ = 0ms;
	Seconds animTimeIdle_ = 0s;
	Seconds animTimeWalk_ = 0s;
	Seconds animTimeAttack_ = 0s;
	float tIdle_ = 0.f;
	float tWalk_ = 0.f;
	float tAttack_ = 0.f;
	std::string currentAttackClip_{ "Boss_Attack" };
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
	bool viewFrustumCulled = false;
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
// Combat allegiance. A skill hitbox only damages factions in its hostile mask,
// so allies (same faction) never hit each other. Neutral attacks and is hit by
// nothing by default. Faction is set at object creation sites.
enum class Faction : u8t { Neutral = 0, Players, Monsters };

inline u32t factionBit(Faction f) { return 1u << static_cast<u32t>(f); }
inline u32t hostileMask(Faction f) {
	switch (f) {
	case Faction::Players:  return factionBit(Faction::Monsters);
	case Faction::Monsters: return factionBit(Faction::Players);
	default:                return 0u;   // Neutral: attacks nothing
	}
}

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

	// 로비 대기실 슬롯 포트레이트 전용 렌더. 메인 씬 카메라/렌더패스와 무관하게,
	// 스킨드 메시 draw event를 포워드 PBRSkinnedPipeline 포트레이트 채널(slot)로 제출한다.
	// 컬링 플래그(메인 카메라 기준)는 무시하고 항상 그린다.
	void MU_CALLCONV renderPortrait(GFX& gfx, u32t slot);

	virtual void setAnimBlender(AnimSystem& animSystem, const AssetManager& assetManager) {}

	// 이미 init된 AnimBlender 인스턴스를 채택한다(소유권 이전). 기존 블렌더는
	// AnimSystem에서 추적 해제 후 교체된다. setAnimBlender가 클래스마다 고정된 블렌더
	// 타입을 쓰는 것과 달리, 런타임에 임의의 블렌더로 교체할 때 쓴다(에디터 캐스터 핫스왑).
	void adoptAnimBlender(std::unique_ptr<AnimBlender> blender, AnimSystem& animSystem);

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
	mu::Vec3 MU_CALLCONV scale() const { return instanceScale_; }

	mu::Vec3 MU_CALLCONV forward() const { return forward_; }
	mu::Vec3 MU_CALLCONV right() const { return right_; }
	mu::Vec3 MU_CALLCONV up() const { return up_; }

	// Physics body accessor. PhysicsWorld 등록 시 &body()를 전달한다.
	RigidBody&       body()       { return body_; }
	const RigidBody& body() const { return body_; }

	// 접지 중력 게이팅 (character controller grounded gravity gating).
	// 물리 step() 직후, 이번 step이 만든 terrain 접촉을 보고 이 body가 접지 상태인지
	// 판정한 뒤 body_.gravityScale()을 설정한다(접지=0, 공중=1). 접지 시 작은 하강
	// 속도를 0으로 스냅(ground-snap)해 중력↔접촉 솔버의 미세 진동(jitter)을 제거한다.
	// 공중 전환(낭떠러지/점프/넉백)은 즉시 일어나 낙하가 정상 동작한다.
	// 반드시 physicsWorld.step() 이후(렌더 프레임이 아닌 물리 step 루프)에서 호출한다.
	void updateGroundedGravityGate(const PhysicsWorld& world, Seconds physicsDt);

	// 현재 접지 판정 결과(읽기 전용). updateGroundedGravityGate()가 갱신한다.
	bool isGrounded() const { return grounded_; }
	// CombatSystem/DebugBVView 등 BVH 접근용 편의 accessor.
	const BVH& worldBVH() const { return body_.worldBVH(); }

	// Hi-Z occlusion culling용 월드 공간 cull AABB.
	// rest-pose mesh->bounds × world는 스킨/랙돌 변형을 반영하지 못하므로,
	// 현재 포즈(애니메이션·랙돌)를 따라가는 body_.worldBVH()의 본 부착 노드들의
	// 합집합 AABB(여유 마진 포함)를 반환한다.
	// 본 부착 노드가 없으면(비스킨 모델) nullopt → 호출부가 기존 경로로 폴백한다.
	std::optional<AABB> worldCullBounds() const;
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

	// 래그돌 접근자(베이스 기본값: 래그돌 없음). 래그돌을 보유하는 몬스터 클래스가
	// 오버라이드한다. idMonsterMap_ 등 Object* 기반 통합 순회에서 종류와 무관하게 접근하기 위함.
	// (eventBus()/setAnimBlender()와 동일한 no-op 가상 패턴)
	virtual Ragdoll* ragdoll() { return nullptr; }
	virtual bool ragdollPendingActivation() const { return false; }
	virtual void setRagdollPendingActivation(bool) {}
	virtual mu::Vec3 ragdollInitVelocity() const { return {}; }
	virtual void MU_CALLCONV setRagdollInitVelocity(mu::Vec3) {}

	void setHp(i32t hp) { hp_ = hp; }
	i32t hp() const { return hp_; }

	Faction faction() const { return faction_; }
	void setFaction(Faction f) { faction_ = f; }

	// 차단벽(barrier) 모드: 서버가 전술 중 토글. 활성 시 클라가 로컬 플레이어를
	// 이 객체 밖으로 position 기반 분리(resolveBarrierSeparation)로 밀어낸다.
	// 몬스터 종류와 무관한 Object 레벨 일반 플래그.
	void setBarrierActive(bool v) { barrierActive_ = v; }
	bool isBarrierActive() const  { return barrierActive_; }

	// 서버 전술 상태가 지정하는 피격 impulse 면역. barrier 여부와 분리되어 보스에도 적용된다.
	void setHitImpulseImmune(bool v) { hitImpulseImmune_ = v; }
	bool hitImpulseImmune() const    { return hitImpulseImmune_; }

	// 모델 상태(isDead_ 플래그)만 갱신한다.
	// 사망/부활 애니메이션은 EvDeath/EvRespawn 이벤트가 EventBus를 통해 구동한다.
	void setDead(bool dead) {
		isDead_ = dead;
	}
	bool isDead() const { return isDead_; }
	// Hidden: removed from view/update entirely (not a corpse). Driven by S_NpcHide; cleared on respawn.
	// Type-agnostic so it transfers as-is when dedicated NPC types replace the shared goblin model.
	void setHidden(bool v) { hidden_ = v; }
	bool hidden() const    { return hidden_; }
	void setMaxHp(i32t v) { maxHp_ = v; }
	i32t maxHp() const    { return maxHp_; }

	void setFrustumCulled(bool v) { renderState_.viewFrustumCulled = v; }
	bool isFrustumCulled() const  { return renderState_.viewFrustumCulled; }
	void setShadowCulled(bool v)  { shadowLightFrustumCulled_ = v; }
	bool isShadowCulled() const   { return shadowLightFrustumCulled_; }
	void activateOcclusion(bool willOcclude) { renderState_.willOcclude = true; }

	void setRenderObjectId(u32t id) { renderObjectId_ = id; }
	u32t renderObjectId() const     { return renderObjectId_; }
	void setHiZCulled(bool v)       { hiZCulled_ = v; }
	bool isHiZCulled() const        { return hiZCulled_; }

	// When true the mesh is not rendered: the corpse has dissolved into energy orbs.
	void setHiddenByOrb(bool v)     { hiddenByOrb_ = v; }
	bool isHiddenByOrb() const      { return hiddenByOrb_; }

	// --- Energy-orb absorption ripples (body-surface emissive wave) ---
	// When an energy orb is absorbed, a ripple is spawned at the contact point and
	// fed into this object's deferred-skinned per-instance data. The shader renders
	// an expanding emissive ring of colorHDR across the mesh surface (GB2 emissive,
	// auto-bloomed). Local effect: only the absorbing player accumulates ripples.
	// Ripples are aged in update() and evicted when older than the shader's lifetime.
	struct BodyRipple {
		// Anchor stored relative to the object position at trigger time, so the ring
		// follows the body as the player moves (re-anchored to the live position each
		// frame in render) instead of staying pinned to a world point.
		mu::Vec3 offset{};
		mu::Vec3 colorHDR{ 1.f, 1.f, 1.f };
		float    age       = 0.f;
		float    intensity = 1.f;
	};
	static constexpr int kMaxBodyRipples = 4;   // must match shader MAX_ABSORB_RIPPLES
	void MU_CALLCONV addBodyRipple(mu::Vec3 contactPosW, mu::Vec3 colorHDR, float intensity = 1.f);
	const std::vector<BodyRipple>& bodyRipples() const { return bodyRipples_; }

	// Network interpolation state for server-position-driven objects (remote players
	// AND monsters). Each S_Move resets netInterpAcc_ to 0; the owner calls update()
	// with t = min(netInterpAcc_ / netInterpDuration_, 1), so render lerps prev->curr
	// over one move interval and HOLDS at curr once moves stop. This avoids the
	// prev<->curr oscillation that the physics-step clock (tPhysicInterpolation)
	// produces when moves are sparse/stopped (it cycles 0->1 every physics step).
	Seconds netInterpDuration_{ 1s / 20.f };
	Seconds netInterpAcc_{ 0s };

	// BV rendering color for collision visualization.
	// Default green: no collision. Red: terrain-object. Blue: object-object.
	void MU_CALLCONV setBVColor(mu::Vec4 color) { bvColor_ = color; }
	mu::Vec4         bvColor()            const { return bvColor_; }

protected:
	// 모델 고유 scale(modelBaseScale_)과 게임플레이 per-instance scale(instanceScale_)을
	// 합성해 body_.scale()로 적용하고 BVH를 재빌드한다. setModel/setScale에서 호출.
	void applyCompositeScale();

	RigidBody body_{};

	// body_.scale() = modelBaseScale_ * instanceScale_ (component-wise).
	mu::Vec3 modelBaseScale_{ 1.f, 1.f, 1.f };   // setModel에서 pModel->baseScale 흡수 (모델 고유 scale)
	mu::Vec3 instanceScale_ { 1.f, 1.f, 1.f };   // setScale이 쓰는 게임플레이 scale (기본 1)

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
	bool hidden_ = false;
	i32t maxHp_{};
	Faction faction_ = Faction::Neutral;
	bool barrierActive_ = false;
	bool hitImpulseImmune_ = false;

	// --- 접지 중력 게이팅 상태 (updateGroundedGravityGate가 관리) ---
	// grounded_      : 현재 접지 여부(중력 게이트 off 상태와 동일).
	// groundedSteps_ : 연속 접지 step 수. 게이트를 끄기 전 짧은 지속을 요구해
	//                  단발성 접촉으로 인한 상태 flicker를 방지한다. 공중 전환은
	//                  즉시(0으로 리셋) 일어나 낙하가 지연 없이 동작한다.
	bool grounded_      = false;
	int  groundedSteps_ = 0;

	u32t     renderObjectId_          = std::numeric_limits<u32t>::max();
	bool     hiZCulled_               = false;
	bool     hiddenByOrb_             = false;  // corpse dissolved into energy orbs
	bool     shadowLightFrustumCulled_ = false;
	std::vector<BodyRipple> bodyRipples_{};     // active absorption ripples (local player)
	mu::Vec4 bvColor_{ 0.f, 1.f, 0.f, 1.f };

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

	// netInterpDuration_ / netInterpAcc_ are now on Object (shared by remote players
	// and monsters — both are server-position-driven and need network interpolation).

private:
	EventBus eventBus_{};
};

// 각 몬스터는 Object를 직접 상속한다. 공용 Monster 베이스로 일반화하지 않고,
// ragdoll 상태와 Hit/Death/Attack/Respawn을 처리하는 EventBus를 클래스마다 복제한다.
// 행동/클립이 몬스터별로 분기하므로 의도적으로 패턴만 반복한다.
class Goblin : public Object {
public:
	Goblin() = default;
	Goblin(Object&& base) : Object(std::move(base)) {}

	class EventBus : public IEventBus {
	public:
		void receive(const BasicEvent* event, Seconds deltaTime, EventList& evList, Timer& timer, void* pVoidOwner) override;
	};

	IEventBus* eventBus() override { return &eventBus_; }

	Ragdoll* ragdoll() override { return &ragdoll_; }
	bool ragdollPendingActivation() const override { return ragdollPendingActivation_; }
	void setRagdollPendingActivation(bool v) override { ragdollPendingActivation_ = v; }
	mu::Vec3 ragdollInitVelocity() const override { return ragdollInitVelocity_; }
	void MU_CALLCONV setRagdollInitVelocity(mu::Vec3 v) override { ragdollInitVelocity_ = v; }

	void setAnimBlender(AnimSystem& animSystem, const AssetManager& assetManager) override;

private:
	EventBus eventBus_{};
	Ragdoll  ragdoll_{};
	bool     ragdollPendingActivation_ = false;
	mu::Vec3 ragdollInitVelocity_{};
};

// Goblin과 같은 리그(91본, 동일 이름/순서)와 Goblin_* 클립을 공유하는 모델 변형(전술 전투
// 중간보스 전용 외형). 별개 종(species)이 아니라 Goblin의 스킨 교체이므로, 다른 몬스터들과
// 달리 Object 직접 상속을 반복하지 않고 Goblin을 상속해 EventBus/ragdoll을 그대로 재사용한다.
class Hobgoblin : public Goblin {
public:
	Hobgoblin() = default;
	Hobgoblin(Object&& base) : Goblin(std::move(base)) {}

	void setAnimBlender(AnimSystem& animSystem, const AssetManager& assetManager) override;
};

class Snake : public Object {
public:
	Snake() = default;
	Snake(Object&& base) : Object(std::move(base)) {}

	class EventBus : public IEventBus {
	public:
		void receive(const BasicEvent* event, Seconds deltaTime, EventList& evList, Timer& timer, void* pVoidOwner) override;
	};

	IEventBus* eventBus() override { return &eventBus_; }

	Ragdoll* ragdoll() override { return &ragdoll_; }
	bool ragdollPendingActivation() const override { return ragdollPendingActivation_; }
	void setRagdollPendingActivation(bool v) override { ragdollPendingActivation_ = v; }
	mu::Vec3 ragdollInitVelocity() const override { return ragdollInitVelocity_; }
	void MU_CALLCONV setRagdollInitVelocity(mu::Vec3 v) override { ragdollInitVelocity_ = v; }

	void setAnimBlender(AnimSystem& animSystem, const AssetManager& assetManager) override;

private:
	EventBus eventBus_{};
	Ragdoll  ragdoll_{};
	bool     ragdollPendingActivation_ = false;
	mu::Vec3 ragdollInitVelocity_{};
};

class Mushroom : public Object {
public:
	Mushroom() = default;
	Mushroom(Object&& base) : Object(std::move(base)) {}

	class EventBus : public IEventBus {
	public:
		void receive(const BasicEvent* event, Seconds deltaTime, EventList& evList, Timer& timer, void* pVoidOwner) override;
	};

	IEventBus* eventBus() override { return &eventBus_; }

	Ragdoll* ragdoll() override { return &ragdoll_; }
	bool ragdollPendingActivation() const override { return ragdollPendingActivation_; }
	void setRagdollPendingActivation(bool v) override { ragdollPendingActivation_ = v; }
	mu::Vec3 ragdollInitVelocity() const override { return ragdollInitVelocity_; }
	void MU_CALLCONV setRagdollInitVelocity(mu::Vec3 v) override { ragdollInitVelocity_ = v; }

	void setAnimBlender(AnimSystem& animSystem, const AssetManager& assetManager) override;

private:
	EventBus eventBus_{};
	Ragdoll  ragdoll_{};
	bool     ragdollPendingActivation_ = false;
	mu::Vec3 ragdollInitVelocity_{};
};

// Monster skill-caster Object classes (Bomber / Birdy / Slime / Treant).
// Each is the Goblin/Mushroom pattern (EventBus + Ragdoll); only the name differs.
class Bomber : public Object {
public:
	Bomber() = default;
	Bomber(Object&& base) : Object(std::move(base)) {}

	class EventBus : public IEventBus {
	public:
		void receive(const BasicEvent* event, Seconds deltaTime, EventList& evList, Timer& timer, void* pVoidOwner) override;
	};

	IEventBus* eventBus() override { return &eventBus_; }

	Ragdoll* ragdoll() override { return &ragdoll_; }
	bool ragdollPendingActivation() const override { return ragdollPendingActivation_; }
	void setRagdollPendingActivation(bool v) override { ragdollPendingActivation_ = v; }
	mu::Vec3 ragdollInitVelocity() const override { return ragdollInitVelocity_; }
	void MU_CALLCONV setRagdollInitVelocity(mu::Vec3 v) override { ragdollInitVelocity_ = v; }

	void setAnimBlender(AnimSystem& animSystem, const AssetManager& assetManager) override;

private:
	EventBus eventBus_{};
	Ragdoll  ragdoll_{};
	bool     ragdollPendingActivation_ = false;
	mu::Vec3 ragdollInitVelocity_{};
};

class Birdy : public Object {
public:
	Birdy() = default;
	Birdy(Object&& base) : Object(std::move(base)) {}

	class EventBus : public IEventBus {
	public:
		void receive(const BasicEvent* event, Seconds deltaTime, EventList& evList, Timer& timer, void* pVoidOwner) override;
	};

	IEventBus* eventBus() override { return &eventBus_; }

	Ragdoll* ragdoll() override { return &ragdoll_; }
	bool ragdollPendingActivation() const override { return ragdollPendingActivation_; }
	void setRagdollPendingActivation(bool v) override { ragdollPendingActivation_ = v; }
	mu::Vec3 ragdollInitVelocity() const override { return ragdollInitVelocity_; }
	void MU_CALLCONV setRagdollInitVelocity(mu::Vec3 v) override { ragdollInitVelocity_ = v; }

	void setAnimBlender(AnimSystem& animSystem, const AssetManager& assetManager) override;

private:
	EventBus eventBus_{};
	Ragdoll  ragdoll_{};
	bool     ragdollPendingActivation_ = false;
	mu::Vec3 ragdollInitVelocity_{};
};

class Slime : public Object {
public:
	Slime() = default;
	Slime(Object&& base) : Object(std::move(base)) {}

	class EventBus : public IEventBus {
	public:
		void receive(const BasicEvent* event, Seconds deltaTime, EventList& evList, Timer& timer, void* pVoidOwner) override;
	};

	IEventBus* eventBus() override { return &eventBus_; }

	Ragdoll* ragdoll() override { return &ragdoll_; }
	bool ragdollPendingActivation() const override { return ragdollPendingActivation_; }
	void setRagdollPendingActivation(bool v) override { ragdollPendingActivation_ = v; }
	mu::Vec3 ragdollInitVelocity() const override { return ragdollInitVelocity_; }
	void MU_CALLCONV setRagdollInitVelocity(mu::Vec3 v) override { ragdollInitVelocity_ = v; }

	void setAnimBlender(AnimSystem& animSystem, const AssetManager& assetManager) override;

private:
	EventBus eventBus_{};
	Ragdoll  ragdoll_{};
	bool     ragdollPendingActivation_ = false;
	mu::Vec3 ragdollInitVelocity_{};
};

class Treant : public Object {
public:
	Treant() = default;
	Treant(Object&& base) : Object(std::move(base)) {}

	class EventBus : public IEventBus {
	public:
		void receive(const BasicEvent* event, Seconds deltaTime, EventList& evList, Timer& timer, void* pVoidOwner) override;
	};

	IEventBus* eventBus() override { return &eventBus_; }

	Ragdoll* ragdoll() override { return &ragdoll_; }
	bool ragdollPendingActivation() const override { return ragdollPendingActivation_; }
	void setRagdollPendingActivation(bool v) override { ragdollPendingActivation_ = v; }
	mu::Vec3 ragdollInitVelocity() const override { return ragdollInitVelocity_; }
	void MU_CALLCONV setRagdollInitVelocity(mu::Vec3 v) override { ragdollInitVelocity_ = v; }

	void setAnimBlender(AnimSystem& animSystem, const AssetManager& assetManager) override;

private:
	EventBus eventBus_{};
	Ragdoll  ragdoll_{};
	bool     ragdollPendingActivation_ = false;
	mu::Vec3 ragdollInitVelocity_{};
};

// Grandbaum: Treant 변종 미드보스 보스. 같은 리그/애니(Treant_*)·EventBus·래그돌을 그대로 상속하고
// 전용 모델(modelGrandbaum)만 쓴다(Hobgoblin이 Goblin을 확장하는 관계와 동일).
class Grandbaum : public Treant {
public:
	Grandbaum() = default;
	Grandbaum(Object&& base) : Treant(std::move(base)) {}

	void setAnimBlender(AnimSystem& animSystem, const AssetManager& assetManager) override;
};

// Isys: Birdy 변종 미드보스 보스. 같은 리그/애니(Birdy_*)·EventBus·래그돌을 상속하고 전용 모델(modelIsys)만 쓴다.
class Isys : public Birdy {
public:
	Isys() = default;
	Isys(Object&& base) : Birdy(std::move(base)) {}

	void setAnimBlender(AnimSystem& animSystem, const AssetManager& assetManager) override;
};

// 거점(Stronghold): 데미지를 받는 정적 구조물. 고블린/플레이어처럼 EventBus로
// Hit/Death/Respawn을 일관되게 처리하되, 애니메이션이 없으므로 AnimBlender는 갖지 않는다
// (setAnimBlender 미오버라이드 → renderState_.animBlender == nullptr).
class Stronghold : public Object {
public:
	Stronghold() = default;
	Stronghold(Object&& base)
		: Object(std::move(base)) {}

	void MU_CALLCONV render(GFX& gfx, mu::Mat4x4 offsetXform = mu::Mat4x4()) override;

	class EventBus : public IEventBus {
	public:
		void receive(const BasicEvent* event, Seconds deltaTime, EventList& evList, Timer& timer, void* pVoidOwner) override;
	};

	IEventBus* eventBus() override { return &eventBus_; }

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

#include "rspch.hpp"
#include "object.hpp"
#include "Model.hpp"

// 모델을 설정한다.
// 모델에 바운딩 볼륨이 존재할 경우, 월드 공간 바운딩 볼륨을 구축한다.
// (모델의 바운딩 볼륨을 기반으로 게임 객체의 월드 변환을 적용한
//  월드 공간 바운딩 볼륨을 따로 두어야 월드 공간 충돌 처리가 가능하다.)
void Object::setModel(const Model* pModel){
	DISPLAY_ERROR_STR(pModel != nullptr, "[Game Error] Object::setModel: 널 모델이 전달되었습니다.", false);
	if (pModel == nullptr) {
		return;
	}

	pModel_ = pModel;
	rebuildBVH(physicState_);
}

// 물리 시뮬레이션과 별개로 게임 객체의
	// 자체적인 갱신 루틴을 필요로 할 때 이 함수에 작성한다.
void Object::update(Milliseconds deltaTime) {

}

// 게임 객체의 위치를 갱신한다.
// 이전 PhysicState와 현재 PhysicState의 위치가 모두 갱신된다.
// 각 PhysicState의 충돌체(volumes) 역시 갱신된다.
void MU_CALLCONV Object::setPos(mu::Vec3 newPos) {
	physicState_.pos = newPos;

	if (pModel_ && !pModel_->bvh.empty()) {
		rebuildBVH(physicState_);
	}
}

// 게임 객체의 속도를 갱신한다.
// 이전 PhysicState와 현재 PhysicState의 속도가 모두 갱신된다.
void MU_CALLCONV Object::setVelocity(mu::Vec3 newVelocity) {
	physicState_.velocity = newVelocity;
}

// 게임 객체의 각속도를 갱신한다.
// 이전 PhysicState와 현재 PhysicState의 각속도가 모두 갱신된다.
void MU_CALLCONV Object::setOmega(mu::Vec3 newOmega) {
	physicState_.omega = newOmega;
}

// 게임 객체의 방향을 갱신한다.
// 이전 PhysicState와 현재 PhysicState의 방향이 모두 갱신된다.
// 게임 객체의 방향 벡터들도 전부 갱신된다.
void MU_CALLCONV Object::setOrient(mu::NQuat newOrient) {
	physicState_.orient = newOrient;
	right_ = physicState_.orient.rotate(mu::Vec3(1.f, 0.f, 0.f));
	up_ = physicState_.orient.rotate(mu::Vec3(0.f, 1.f, 0.f));
	forward_ = physicState_.orient.rotate(mu::Vec3(0.f, 0.f, 1.f));

	// BVH nodes with OBB shapes track orientation and must be rebuilt.
	if (pModel_ && !pModel_->bvh.empty()) {
		rebuildBVH(physicState_);
	}
}

// 게임 객체의 크기를 갱신한다.
// 이전 PhysicState와 현재 PhysicState의 크기가 모두 갱신된다.
// 각 PhysicState의 AABB 역시 갱신된다.
void MU_CALLCONV Object::setScale(mu::Vec3 newScale) {
	physicState_.scale = newScale;

	if (pModel_ && !pModel_->bvh.empty()) {
		rebuildBVH(physicState_);
	}
}

// Rebuilds the world-space BVH in `state` from the model's local-space BVH template.
// Tree structure (children indices) is preserved; only shape/bounds values are transformed.
//
// For bone-attached nodes (boneIdx >= 0) with an active animBlender:
//   boneToWorld = bone.toDress * finalXformData()[boneIdx] * objWorldMat
//   (bone local -> dress -> animated dress -> world; same chain as equipment socket rendering)
//   center is transformed as a homogeneous point; result is always OBB.
//
// For root-only nodes (boneIdx == -1) or when no animBlender is present:
//   AABB: apply pos + scale.
//   OBB:  apply pos + scale + orient (composed with local OBB orient).
void Object::rebuildBVH(PhysicState& state) const {
	if (!pModel_ || pModel_->bvh.empty()) return;

	const auto& localBVH = pModel_->bvh;

	// Object world matrix (same formula as renderState_.world, physics-state based)
	const mu::Mat4x4 objWorld = mu::Mat4x4(mu::scale(state.scale))
		* mu::Mat4x4(state.orient)
		* mu::translate(state.pos);

	state.bvh.nodes.resize(localBVH.nodes.size());
	for (std::size_t i = 0; i < localBVH.nodes.size(); ++i) {
		const auto& src = localBVH.nodes[i];
		auto& dst = state.bvh.nodes[i];

		dst.children = src.children;
		dst.name = src.name;
		dst.boneIdx = src.boneIdx;

		const bool useBone = false;


		dst.shape = std::visit([&](auto&& s) -> std::variant<AABB, OBB> {
			using T = std::decay_t<decltype(s)>;
			if constexpr (std::is_same_v<T, AABB>) {
				return AABB{
					s.center * state.scale + state.pos,
					s.size * state.scale,
				};
			}
			else {
				mu::NQuat worldOrient = state.orient;
				worldOrient *= s.orient;
				return OBB{
					state.orient.rotate(s.center * state.scale) + state.pos,
					s.halfExtents * state.scale,
					worldOrient,
				};
			}
			}, src.shape);


		dst.bounds = std::visit([](auto&& s) -> AABB {
			using T = std::decay_t<decltype(s)>;
			if constexpr (std::is_same_v<T, OBB>) return obbToAABB(s);
			else                                   return s;
			}, dst.shape);
	}
}
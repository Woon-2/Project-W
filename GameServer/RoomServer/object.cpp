#include "rspch.hpp"
#include "object.hpp"
#include "Model.hpp"
#include "GameSession.hpp"

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

/*--------------
     Goblin
--------------*/

GoblinUpdateResult Goblin::update(Seconds dt, const std::vector<GameSession*>& sessions) {
	if ( hp() <= 0 ) {
		return {};
	}

	// 가장 가까운 플레이어 탐색
	GameSession* nearestSession = nullptr;
	float nearestDist = std::numeric_limits<float>::max();

	for (auto s : sessions) {
		float d = (s->player()->pos() - pos()).len();
		if (d < nearestDist) {
			nearestDist = d;
			nearestSession = s;
		}
	}

	GoblinUpdateResult result{};
	auto& velocity = result.velocity;

	switch (aiState_) {
	case GoblinAIState::Patrol: {
		if (nearestDist < aggroRange_) {
			aiState_ = GoblinAIState::Chase;
			break;
		}

		auto toTarget = patrolTarget_ - pos();

		if (toTarget.len2() < 0.25f) {
			static std::mt19937 rng{std::random_device{}()};
			std::uniform_real_distribution<float> angleDist(0.f, mu::pi * 2.f);

			float angle = angleDist(rng);
			patrolTarget_ = spawnPos_ + mu::Vec3(std::cos(angle) * 5.f, 0.f, std::sin(angle) * 5.f);
		}
		else {
			auto dir = mu::NVec3(toTarget);

			velocity = mu::Vec3(dir) * moveSpeed_;
			setPos(pos() + velocity * dt.count());

			float yaw = std::atan2(dir.x(), dir.z());
			setOrient(mu::NQuat(mu::Radian(), mu::Radian(), mu::Radian(yaw)));
		}
		break;
	}
	case GoblinAIState::Chase: {
		if (nearestDist < attackRange_) {
			aiState_ = GoblinAIState::Attack;
			break;
		}
		if (nearestDist > deaggroRange_) {
			aiState_ = GoblinAIState::Return;
			break;
		}

		auto toPlayer = nearestSession->player()->pos() - pos();
		auto dir = mu::NVec3(toPlayer);

		velocity = mu::Vec3(dir) * moveSpeed_;
		setPos(pos() + velocity * dt.count());

		float yaw = std::atan2(dir.x(), dir.z());
		setOrient(mu::NQuat(mu::Radian(), mu::Radian(), mu::Radian(yaw)));
		break;
	}
	case GoblinAIState::Attack: {
		if (nearestDist > attackRange_) {
			aiState_ = GoblinAIState::Chase;
			break;
		}

		auto toPlayer = nearestSession->player()->pos() - pos();
		setOrient(mu::NQuat(mu::Radian(), mu::Radian(),
			mu::Radian(std::atan2(toPlayer.x(), toPlayer.z())))
		);

		if (attackCooldown_ > 0s) {
			attackCooldown_ -= dt;
		}
		else {
			auto player = nearestSession->player();

			int32 newHp = std::max(player->hp() - attackDamage_, 0);
			player->setHp(newHp);

			result.hit = {static_cast<uint16>(nearestSession->id()), newHp};

			attackCooldown_ = attackCooldownMax_;
		}
		break;
	}
	case GoblinAIState::Return: {
		if (nearestDist < aggroRange_) {
			aiState_ = GoblinAIState::Chase;
			break;
		}

		auto toSpawn = spawnPos_ - pos();

		if (toSpawn.len2() < 0.25f) {
			setPos(spawnPos_);
			patrolTarget_ = spawnPos_;
			aiState_ = GoblinAIState::Patrol;
			break;
		}

		auto dir = mu::NVec3(toSpawn);

		velocity = mu::Vec3(dir) * moveSpeed_;
		setPos(pos() + velocity * dt.count());

		float yaw = std::atan2(dir.x(), dir.z());
		setOrient(mu::NQuat(mu::Radian(), mu::Radian(), mu::Radian(yaw)));
		break;
	}
	}

	return result;
}

void Goblin::recordSnapshot(uint64 serverMs) {
	posHistory_[historyHead_] = {serverMs, pos()};
	historyHead_ = (historyHead_ + 1) % historySize_;
}

mu::Vec3 Goblin::rewindPos(uint64 targetMs) const {
	for (int32 i = 1; i <= historySize_; ++i) {
		int32 idx = (historyHead_ - i + historySize_) % historySize_;
		if ( posHistory_[ idx ].serverMs <= targetMs ) {
			return posHistory_[ idx ].pos;
		}
	}

	// 히스토리 전체가 targetMs보다 최신이면 가장 오래된 항목으로 클램프
	return posHistory_[historyHead_ % historySize_].pos;
}

#include "rspch.hpp"
#include "object.hpp"
#include "Model.hpp"
#include "GameSession.hpp"

void Object::setModel(const Model* pModel){
	DISPLAY_ERROR_STR(pModel != nullptr, "[Game Error] Object::setModel: null model.", false);
	if (pModel == nullptr) {
		return;
	}

	pModel_ = pModel;
	rebuildBodyBVH();
}

void Object::update(Milliseconds deltaTime) {

}

void MU_CALLCONV Object::setPos(mu::Vec3 newPos) {
	body_.setPos(newPos);

	if (pModel_ && !pModel_->bvh.empty()) {
		rebuildBodyBVH();
	}
}

void MU_CALLCONV Object::setOrient(mu::NQuat newOrient) {
	body_.setOrient(newOrient);
	right_   = body_.orient().rotate(mu::Vec3(1.f, 0.f, 0.f));
	up_      = body_.orient().rotate(mu::Vec3(0.f, 1.f, 0.f));
	forward_ = body_.orient().rotate(mu::Vec3(0.f, 0.f, 1.f));

	if (pModel_ && !pModel_->bvh.empty()) {
		rebuildBodyBVH();
	}
}

void MU_CALLCONV Object::setScale(mu::Vec3 newScale) {
	body_.setScale(newScale);

	if (pModel_ && !pModel_->bvh.empty()) {
		rebuildBodyBVH();
	}
}

// Rebuilds the world-space BVH in body_ from the model's local-space BVH template.
// Tree structure (children indices) is preserved; only shape/bounds values are transformed.
void Object::rebuildBodyBVH() {
	if (!pModel_ || pModel_->bvh.empty()) return;

	const auto& localBVH = pModel_->bvh;
	BVH& worldBVH = body_.worldBVH();

	const mu::Vec3  pos    = body_.pos();
	const mu::NQuat orient = body_.orient();
	const mu::Vec3  scale  = body_.scale();

	const mu::Mat4x4 objWorld = mu::Mat4x4(mu::scale(scale))
		* mu::Mat4x4(orient)
		* mu::translate(pos);

	worldBVH.nodes.resize(localBVH.nodes.size());
	for (std::size_t i = 0; i < localBVH.nodes.size(); ++i) {
		const auto& src = localBVH.nodes[i];
		auto& dst = worldBVH.nodes[i];

		dst.children = src.children;
		dst.name = src.name;
		dst.boneIdx = src.boneIdx;

		dst.shape = std::visit([&](auto&& s) -> std::variant<AABB, OBB> {
			using T = std::decay_t<decltype(s)>;
			if constexpr (std::is_same_v<T, AABB>) {
				return AABB{
					s.center * scale + pos,
					s.size * scale,
				};
			}
			else {
				mu::NQuat worldOrient = orient;
				worldOrient *= s.orient;
				return OBB{
					orient.rotate(s.center * scale) + pos,
					s.halfExtents * scale,
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

	// Freeze rotation: physics solver may accumulate angular velocity on a Dynamic body.
	body().setOmega(mu::Vec3{});

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

			velocity = mu::Vec3(dir.x(), 0.f, dir.z()) * moveSpeed_;
			setLinearVel(mu::Vec3(velocity.x(), body().linearVel().y(), velocity.z()));

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

		velocity = mu::Vec3(dir.x(), 0.f, dir.z()) * moveSpeed_;
		setLinearVel(mu::Vec3(velocity.x(), body().linearVel().y(), velocity.z()));

		float yaw = std::atan2(dir.x(), dir.z());
		setOrient(mu::NQuat(mu::Radian(), mu::Radian(), mu::Radian(yaw)));
		break;
	}
	case GoblinAIState::Attack: {
		if (nearestDist > attackRange_) {
			aiState_ = GoblinAIState::Chase;
			break;
		}

		setLinearVel(mu::Vec3(0.f, body().linearVel().y(), 0.f));

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
			setLinearVel(mu::Vec3{});
			setPos(spawnPos_);
			body().snapToCurrent();
			patrolTarget_ = spawnPos_;
			aiState_ = GoblinAIState::Patrol;
			break;
		}

		auto dir = mu::NVec3(toSpawn);

		velocity = mu::Vec3(dir.x(), 0.f, dir.z()) * moveSpeed_;
		setLinearVel(mu::Vec3(velocity.x(), body().linearVel().y(), velocity.z()));

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

	return posHistory_[historyHead_ % historySize_].pos;
}

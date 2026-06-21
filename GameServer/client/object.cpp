#include "pch.hpp"
#include "object.hpp"
#include "physicsWorld.hpp"
#include "terrainPipeline.hpp"
#include "terrainDeferredPipeline.hpp"
#include "errorHandling.hpp"
#include "AssetManager.hpp"
#include "Timer.hpp"

void AnimBlenderPlayer::init(const AssetManager& assetManager) {
	setSkeleton(assetManager.modelPlayer()->skeleton);
	framesBlended_.resize(skeleton().bones->size());
	for (auto& clip : assetManager.playerAnimations()) {
		pushTargetClip(clip->name, clip);
	}
}

// pOwner의 물리 정보에 따라
// 애니메이션 블렌딩 상태를 갱신한다.
void AnimBlenderPlayer::update(Seconds deltaTime, void* pVoidOwner) {
	auto pOwner = static_cast<Object*>(pVoidOwner);
	setOwnerPos(pOwner->pos());
	
	// 객체의 속력이 runThreshold를 넘는지를 기준으로
	// run 애니메이션이 필요한지 idle 애니메이션이 필요한지 판단한다.
	// runThreshold를 부드럽게 감싸는 blendRange를 설정하여
	// 객체의 속력이 blendRange 내부에 있다면 0과 1 사이의 tRun 값이 구해진다.
	// run 애니메이션의 가중치는 tRun, idle 애니메이션의 가중치는 1 - tRun이 된다.
	const auto runThreshold = 0.1f;

	// 객체의 속력 구하기
	const auto speed = pOwner->velocity().len();

	// blendRange 설정
	const auto runBlendRangeStart = runThreshold - 0.05f;
	const auto runBlendRangeEnd = runThreshold + 5.f;
	// tRun 구하기
	const auto tRun = std::clamp( (speed - runBlendRangeStart) / (runBlendRangeEnd - runBlendRangeStart), 0.f, 1.f );

	// tIdle 구하기 (tIdle0_ 및 tIdle1_)
	// 움직이지 않은 채 motionlessThreshold 시간이 지났다면 idle1 애니메이션을,
	// 그렇지 않으면 조준 idle0 애니메이션을 재생하도록 한다.
	const auto aimlessThreshold = 2s;
	const auto aimBlendRangeStart = aimlessThreshold - 100ms;
	const auto aimBlendRangeEnd = aimlessThreshold + 400ms;

	const auto tIdleBase = 1.f - tRun;

	// Idle 애니메이션들은 그냥 계속 돌린다.
	// 딱히 멈추지 않아도 부자연스럽진 않다.
	animTimeIdle0_ += deltaTime;
	const auto durationIdle = targetClip("Player_Idle0")->duration;
	while (animTimeIdle0_ > durationIdle) {
		animTimeIdle0_ -= durationIdle;
	}
	animTimeIdle1_ += deltaTime;
	const auto durationIdleAim = targetClip("Player_Idle1")->duration;
	while (animTimeIdle1_ > durationIdle) {
		animTimeIdle1_ -= durationIdle;
	}
	tIdle0_ = tIdleBase;

	// run 애니메이션이 필요하다고 판단되었으면,
	// 객체가 움직이고 있는 방향과 바라보고 있는 방향을 통해
	// 좌우상하 움직임 애니메이션을 블렌딩한다.
	if (tRun > 0.f) {
		// 속도와 right 벡터의 내적을 통해 blend space에서의 좌표를 구할 수 있다.
		const auto blendSpaceX = mu::dot(pOwner->velocity(), pOwner->right());
		const auto blendSpaceY = mu::dot(pOwner->velocity(), pOwner->forward());
	
		// blend space 좌표를 바탕으로 각 애니메이션의 가중치를 정한다.
		auto wForward = std::max(0.f, blendSpaceY);
		auto wBackward = std::max(0.f, -blendSpaceY);
		auto wLeft = std::max(0.f, -blendSpaceX);
		auto wRight = std::max(0.f, blendSpaceX);

		// 가중치의 총합이 1이 되게끔 한다.
		float total = wForward + wBackward + wLeft + wRight;
		// total이 0이되어 nan이 나는 것을 방지한다.
		if (total < 1e-4f) {
			wForward += 1e-2f;
			total += 1e-2f;
		}

		tRunForward_ = tRun * wForward / total;
		tRunBackward_ = tRun * wBackward / total;
		tRunLeft_ = tRun * wLeft / total;
		tRunRight_ = tRun * wRight / total;

		animTimeRun_ += deltaTime;

		if (tRun >= 1.f) {
			accMotionless_ = 0s;
		}
	}
	else {
		// 완전한 idle 애니메이션이 재생되고 있다면
		// run 애니메이션과 연관된 변수들은 초기화한다.
		tRunForward_ = 0.f;
		tRunBackward_ = 0.f;
		tRunLeft_ = 0.f;
		tRunRight_ = 0.f;

		animTimeRun_ = 0s;

		accMotionless_ += deltaTime;
	}

	// death 애니메이션은 가장 우선순위가 높게 계산된다.
	// cooldownDeath_의 값이 0보다 큰 동안은 다른 애니메이션과 최종적으로 블렌딩되며
	// 페이드인이 이루어지고, cooldownDeath_의 값이 0이 되면 완전히 1의 비율을 차지한다.
	if (dead_) {
		animTimeDeath_ += deltaTime;

		if (cooldownDeath_ > 0ms) {
			tDeath_ = 1.f - std::clamp( cooldownDeath_ / 300ms, 0.f, 1.f );
		}
		else {
			tDeath_ = 1.f;
		}

		cooldownDeath_ -= deltaTime;
	}
	// hit 애니메이션 블렌딩 비율은 death 다음으로 가장 우선순위가 높게 계산된다.
	// 다른 모든 애니메이션의 블렌딩 비율을 낮추고 최대 0.75만큼의 비율을 차지한다.
	// 모든 블렌딩이 일어난 후에 결과 프레임과 hit 애니메이션 프레임을
	// tHit_으로 보간하게 된다.
	else if (cooldownHit_ > 0ms) {
		// 2배속 재생
		animTimeHit_ += deltaTime * 2.f;
		
		tHit_ = 0.75f * std::clamp( cooldownHit_ / 600ms, 0.f, 1.f );

		cooldownHit_ -= deltaTime;
	}
	else {
		animTimeHit_ = 0s;
		tHit_ = 0.f;
	}

	// 4방향 run 애니메이션들은 재생 시간이 비슷하다.
	// Run_Forward 애니메이션의 duration을 대표로 사용해도 부자연스럽지 않다.
	const auto durationRun = targetClip("Player_Run_Forward")->duration;
	while (animTimeRun_ > durationRun) {
		animTimeRun_ -= durationRun;
	}

}

void AnimBlenderPlayer::onCalcLocal(PassKey<AnimSystem>) {
	// update에서 구한 애니메이션 가중치들로 블렌딩을 수행한다.

	// 개별 애니메이션의 프레임을 업데이트한다.
	updateFrames("Player_Idle0", animTimeIdle0_);
	updateFrames("Player_Idle1", animTimeIdle1_);
	updateFrames("Player_Hit", animTimeHit_);
	updateFrames("Player_Death", animTimeDeath_);
	updateFrames("Player_Run_Forward", animTimeRun_);
	updateFrames("Player_Run_Backward", animTimeRun_);
	updateFrames("Player_Run_Left", animTimeRun_);
	updateFrames("Player_Run_Right", animTimeRun_);

	if (mode_ == Mode::Baked) {
		// death는 우선도가 가장 높음
		if (tDeath_ > 0.01f) {
			auto& deathClip = targetClip("Player_Death");
			finalBakedClipId_ = deathClip->id;
			finalBakedClipFrame_ = static_cast<int>( deathClip->bakedSampleRate * animTimeDeath_.count() );
		}
		else {
			// baked animation에는 hit 제외시킴
			float weights[] = { tIdle0_, tIdle1_, tRunForward_, tRunBackward_, tRunLeft_, tRunRight_ };
			Seconds animTimes[] = { animTimeIdle0_, animTimeIdle1_, animTimeRun_, animTimeRun_, animTimeRun_, animTimeRun_ };
			const AnimClip* clips[] = {
				targetClip("Player_Idle0").get(),
				targetClip("Player_Idle1").get(),
				targetClip("Player_Run_Forward").get(),
				targetClip("Player_Run_Backward").get(),
				targetClip("Player_Run_Left").get(),
				targetClip("Player_Run_Right").get()
			};
			auto i = static_cast<int>( std::distance(std::begin(weights), std::ranges::max_element(weights)) );
			finalBakedClipId_ = clips[i]->id;
			finalBakedClipFrame_ = static_cast<int>( clips[i]->bakedSampleRate * animTimes[i].count() );
		}
		// std::ranges::max_element()

	}
	else /* if (mode_ == Mode::Keyframe) */ {
		auto& localXforms = localXformData();
		auto& framesIdle0 = curFrames("Player_Idle0");
		auto& framesIdle1 = curFrames("Player_Idle1");
		auto& framesHit = curFrames("Player_Hit");
		auto& framesDeath = curFrames("Player_Death");
		auto& framesRunForward = curFrames("Player_Run_Forward");
		auto& framesRunBackward = curFrames("Player_Run_Backward");
		auto& framesRunLeft = curFrames("Player_Run_Left");
		auto& framesRunRight = curFrames("Player_Run_Right");

		// 애니메이션의 프레임들을 블렌딩한다.
		for (std::size_t i = 0u; i < framesBlended_.size(); ++i) {
			WeightedAnimFrame frames[] = {
				WeightedAnimFrame{ .frame = framesIdle0[i], .w = tIdle0_ },
				WeightedAnimFrame{ .frame = framesIdle1[i], .w = tIdle1_ },
				WeightedAnimFrame{ .frame = framesRunForward[i], .w = tRunForward_ },
				WeightedAnimFrame{ .frame = framesRunBackward[i], .w = tRunBackward_ },
				WeightedAnimFrame{ .frame = framesRunLeft[i], .w = tRunLeft_ },
				WeightedAnimFrame{ .frame = framesRunRight[i], .w = tRunRight_ },
			};
			framesBlended_[i] = sumWeightedAnimFrames(frames);
			// hit animation 보간 (nlerp 쓰면 팔꿈치 꼬임)
			framesBlended_[i] = lerpAnimFrames(framesBlended_[i], framesHit[i], tHit_);
			// death animation 보간
			framesBlended_[i] = lerpAnimFrames(framesBlended_[i], framesDeath[i], tDeath_);
		}
		std::ranges::transform(framesBlended_, localXforms.begin(), convertAnimFrameToMatrix);
	}
}

void AnimBlenderPlayer::EventBus::receive(const BasicEvent* event, Seconds deltaTime, EventList& evList, Timer& timer, void* pVoidOwner) {
	auto pOwner = static_cast<AnimBlenderPlayer*>(pVoidOwner);

	switch (event->type) {
	//case EventType::Fire:
	//	// 조준 여부와 관계없이 발사 시
	//	// 움직임이 없었던 시간을 누산하는 accMotionless_를 0으로 만든다.
	//	pOwner->accMotionless_ = 0s;
	//	// 비조준 상태였다면, 조준한 후 사격해야 하므로
	//	// 발사까지 딜레이가 있다.
	//	if (pOwner->tIdle_ > 0.1f) {
	//		pOwner->cooldownFire_ = 120ms;
	//		timer.enqueueJob( DelayedJob{
	//			.job = [&evList, shooterId = static_cast<const EvFire*>(event)->shooterId](){
	//				holdEvent(evList, EvMuzzleFlash(shooterId));
	//			},
	//			.executeAt = timer.lastTp() + 120ms
	//		} );
	//	}
	//	// 조준 상태였다면, 딜레이 없이 바로 사격한다.
	//	else {
	//		holdEvent(evList, EvMuzzleFlash(static_cast<const EvFire*>(event)->shooterId));
	//	}
	//	break;

	case EventType::Hit:
	// 플레이어는 전용 공격 클립이 없어 hit 오버레이를 공격 모션으로 재사용한다.
	case EventType::Attack:
		pOwner->animTimeHit_ = 0s;
		pOwner->cooldownHit_ = 600ms;
		break;

	case EventType::Death:
		pOwner->animTimeDeath_ = 0s;
		pOwner->cooldownDeath_ = 200ms;
		pOwner->dead_ = true;
		break;

	case EventType::Respawn:
		pOwner->dead_          = false;
		pOwner->tDeath_        = 0.f;
		pOwner->animTimeDeath_ = 0s;
		pOwner->cooldownDeath_ = 0ms;
		break;

	default:
		break;
	}
}

// ---------------------------------------------------------------------------
// AnimBlenderGoblin
// ---------------------------------------------------------------------------
void AnimBlenderGoblin::init(const Model* model, const std::vector<std::shared_ptr<AnimClip>>& anims) {
	setSkeleton(model->skeleton);
	framesBlended_.resize(skeleton().bones->size());
	for (auto& clip : anims)
		pushTargetClip(clip->name, clip);

	// 로드된 공격 클립만으로 순서 목록을 구성한다(lua attackIndex가 이 목록을 인덱싱).
	// 레거시 단일 "Goblin_Attack"은 폴백으로 마지막에 둬 기존 에셋이 계속 동작하게 한다.
	auto loaded = [&](const char* n) {
		for (auto& c : anims) if (c->name == n) return true;
		return false;
	};
	for (const char* name : { "Goblin_Attack1", "Goblin_Attack2", "Goblin_Attack3" })
		if (loaded(name)) attackClips_.push_back(name);
	if (!attackClips_.empty()) currentAttackClip_ = attackClips_.front();
}

void AnimBlenderGoblin::update(Seconds deltaTime, void* pVoidOwner) {
	auto pOwner = static_cast<Object*>(pVoidOwner);
	setOwnerPos(pOwner->pos());

	// 객체의 속력이 walkThreshold를 넘는지를 기준으로
	// walk 애니메이션이 필요한지 idle 애니메이션이 필요한지 판단한다.
	// walkThreshold를 부드럽게 감싸는 blendRange를 설정하여
	// 객체의 속력이 blendRange 내부에 있다면 0과 1 사이의 tWalk 값이 구해진다.
	// walk 애니메이션의 가중치는 tWalk, idle 애니메이션의 가중치는 1 - tWalk이 된다.
	const auto walkThreshold = 0.06f;

	const auto speed = pOwner->velocity().len();

	const auto walkBlendRangeStart = walkThreshold - 0.03f;
	const auto walkBlendRangeEnd   = walkThreshold + 3.f;
	const auto targetTWalk = std::clamp( (speed - walkBlendRangeStart) / (walkBlendRangeEnd - walkBlendRangeStart), 0.f, 1.f );

	tWalk_ += (targetTWalk - tWalk_) * (1.f - std::exp(-deltaTime.count() / 0.12f));
	tIdle_ = 1.f - tWalk_;

	animTimeIdle_ += deltaTime;
	const auto durationIdle = targetClip("Goblin_Idle")->duration;
	while (animTimeIdle_ > durationIdle) animTimeIdle_ -= durationIdle;

	if (tWalk_ > 0.f) {
		animTimeWalk_ += deltaTime;
		const auto durationWalk = targetClip("Goblin_Walk")->duration;
		while (animTimeWalk_ > durationWalk) animTimeWalk_ -= durationWalk;
	}

	if (cooldownAttack_ > 0ms && !currentAttackClip_.empty()) {
		const auto durationAttack = targetClip(currentAttackClip_)->duration;
		animTimeAttack_ = std::min(durationAttack, animTimeAttack_ + deltaTime);
		tAttack_ = std::clamp( animTimeAttack_ / 100ms, 0.f, 1.f );
		cooldownAttack_ -= deltaTime;
	}
	else {
		animTimeAttack_ = 0s;
		tAttack_ = 0.f;
	}

	// death 애니메이션은 가장 우선순위가 높게 계산된다.
	// cooldownDeath_의 값이 0보다 큰 동안은 다른 애니메이션과 최종적으로 블렌딩되며
	// 페이드인이 이루어지고, cooldownDeath_의 값이 0이 되면 완전히 1의 비율을 차지한다.
	if (dead_) {
		animTimeDeath_ += deltaTime;
		if (cooldownDeath_ > 0ms)
			tDeath_ = 1.f - std::clamp( cooldownDeath_ / 300ms, 0.f, 1.f );
		else
			tDeath_ = 1.f;
		cooldownDeath_ -= deltaTime;
	}
	// hit 애니메이션 블렌딩 비율은 death 다음으로 가장 우선순위가 높게 계산된다.
	// 다른 모든 애니메이션의 블렌딩 비율을 낮추고 최대 0.75만큼의 비율을 차지한다.
	// 모든 블렌딩이 일어난 후에 결과 프레임과 hit 애니메이션 프레임을
	// tHit_으로 보간하게 된다.
	else if (cooldownHit_ > 0ms) {
		animTimeHit_ += deltaTime * 2.f;   // 2배속 재생
		tHit_ = 0.75f * std::clamp( cooldownHit_ / 600ms, 0.f, 1.f );
		cooldownHit_ -= deltaTime;
	}
	else {
		animTimeHit_ = 0s;
		tHit_ = 0.f;
	}
}

void AnimBlenderGoblin::onCalcLocal(PassKey<AnimSystem>) {
	const bool hasAttack = !currentAttackClip_.empty();

	updateFrames("Goblin_Idle",   animTimeIdle_);
	updateFrames("Goblin_Walk",   animTimeWalk_);
	if (hasAttack) updateFrames(currentAttackClip_, animTimeAttack_);
	updateFrames("Goblin_Hit",    animTimeHit_);
	updateFrames("Goblin_Death",  animTimeDeath_);

	auto& localXforms  = localXformData();
	auto& framesIdle   = curFrames("Goblin_Idle");
	auto& framesWalk   = curFrames("Goblin_Walk");
	auto* framesAttack = hasAttack ? &curFrames(currentAttackClip_) : nullptr;
	auto& framesHit    = curFrames("Goblin_Hit");
	auto& framesDeath  = curFrames("Goblin_Death");

	if (mode_ == Mode::Baked) {
		// death는 우선도가 가장 높음
		if (tDeath_ > 0.01f) {
			auto& deathClip = targetClip("Goblin_Death");
			finalBakedClipId_    = deathClip->id;
			finalBakedClipFrame_ = static_cast<int>( deathClip->bakedSampleRate * animTimeDeath_.count() );
		}
		// 그 다음 공격 애니메이션 우선도 높게 주기
		else if (hasAttack && tAttack_ > 0.01f) {
			auto& attackClip = targetClip(currentAttackClip_);
			finalBakedClipId_    = attackClip->id;
			finalBakedClipFrame_ = static_cast<int>( attackClip->bakedSampleRate * animTimeAttack_.count() );
		}
		else {
			// baked animation에는 hit 제외시킴
			float   weights[]   = { tIdle_, tWalk_ };
			Seconds animTimes[] = { animTimeIdle_, animTimeWalk_ };
			const AnimClip* clips[] = {
				targetClip("Goblin_Idle").get(),
				targetClip("Goblin_Walk").get()
			};
			auto i = static_cast<int>( std::distance(std::begin(weights), std::ranges::max_element(weights)) );
			finalBakedClipId_    = clips[i]->id;
			finalBakedClipFrame_ = static_cast<int>( clips[i]->bakedSampleRate * animTimes[i].count() );
		}
	}
	else /* if (mode_ == Mode::Keyframe) */ {
		for (std::size_t i = 0u; i < framesBlended_.size(); ++i) {
			WeightedAnimFrame frames[] = {
				WeightedAnimFrame{ .frame = framesIdle[i], .w = tIdle_ },
				WeightedAnimFrame{ .frame = framesWalk[i], .w = tWalk_ }
			};
			framesBlended_[i] = sumWeightedAnimFrames(frames);
			if (framesAttack) framesBlended_[i] = lerpAnimFrames(framesBlended_[i], (*framesAttack)[i], tAttack_);
			framesBlended_[i] = lerpAnimFrames(framesBlended_[i], framesHit[i],    tHit_);
			framesBlended_[i] = lerpAnimFrames(framesBlended_[i], framesDeath[i],  tDeath_);
		}
		std::ranges::transform(framesBlended_, localXforms.begin(), convertAnimFrameToMatrix);
	}
}

void AnimBlenderGoblin::EventBus::receive(const BasicEvent* event, Seconds deltaTime, EventList& evList, Timer& timer, void* pVoidOwner) {
	auto pOwner = static_cast<AnimBlenderGoblin*>(pVoidOwner);

	switch (event->type) {
	case EventType::Hit:
		pOwner->animTimeHit_ = 0s;
		pOwner->cooldownHit_ = 600ms;
		break;

	case EventType::Death:
		pOwner->animTimeDeath_ = 0s;
		pOwner->cooldownDeath_ = 200ms;
		pOwner->dead_ = true;
		break;

	case EventType::Attack:
		// attackIndex로 어떤 공격 클립을 재생할지 선택한다(목록 범위로 clamp).
		if (!pOwner->attackClips_.empty()) {
			const auto idx = std::clamp<int>(
				static_cast<const EvAttack*>(event)->attackIndex, 0,
				static_cast<int>(pOwner->attackClips_.size()) - 1);
			pOwner->currentAttackClip_ = pOwner->attackClips_[idx];
			pOwner->animTimeAttack_ = 0s;
			pOwner->cooldownAttack_ = 3000ms;
		}
		break;

	case EventType::Respawn:
		pOwner->dead_          = false;
		pOwner->tDeath_        = 0.f;
		pOwner->animTimeDeath_ = 0s;
		pOwner->cooldownDeath_ = 0ms;
		break;

	default:
		break;
	}
}

// ---------------------------------------------------------------------------
// AnimBlenderSnake
// ---------------------------------------------------------------------------
void AnimBlenderSnake::init(const Model* model, const std::vector<std::shared_ptr<AnimClip>>& anims) {
	setSkeleton(model->skeleton);
	framesBlended_.resize(skeleton().bones->size());
	for (auto& clip : anims)
		pushTargetClip(clip->name, clip);

	// 로드된 공격 클립만으로 순서 목록 구성(lua attackIndex가 인덱싱). 레거시 "Snake_Attack" 폴백.
	auto loaded = [&](const char* n) {
		for (auto& c : anims) if (c->name == n) return true;
		return false;
	};
	for (const char* name : { "Snake_Attack1", "Snake_Attack" })
		if (loaded(name)) attackClips_.push_back(name);
	if (!attackClips_.empty()) currentAttackClip_ = attackClips_.front();
}

void AnimBlenderSnake::update(Seconds deltaTime, void* pVoidOwner) {
	auto pOwner = static_cast<Object*>(pVoidOwner);
	setOwnerPos(pOwner->pos());

	const auto walkThreshold = 0.06f;

	const auto speed = pOwner->velocity().len();

	const auto walkBlendRangeStart = walkThreshold - 0.03f;
	const auto walkBlendRangeEnd   = walkThreshold + 3.f;
	const auto targetTWalk = std::clamp( (speed - walkBlendRangeStart) / (walkBlendRangeEnd - walkBlendRangeStart), 0.f, 1.f );

	tWalk_ += (targetTWalk - tWalk_) * (1.f - std::exp(-deltaTime.count() / 0.12f));
	tIdle_ = 1.f - tWalk_;

	animTimeIdle_ += deltaTime;
	const auto durationIdle = targetClip("Snake_Walk")->duration;
	while (animTimeIdle_ > durationIdle) animTimeIdle_ -= durationIdle;

	if (tWalk_ > 0.f) {
		animTimeWalk_ += deltaTime;
		const auto durationWalk = targetClip("Snake_Walk")->duration;
		while (animTimeWalk_ > durationWalk) animTimeWalk_ -= durationWalk;
	}

	if (cooldownAttack_ > 0ms && !currentAttackClip_.empty()) {
		const auto durationAttack = targetClip(currentAttackClip_)->duration;
		animTimeAttack_ = std::min(durationAttack, animTimeAttack_ + deltaTime);
		tAttack_ = std::clamp( animTimeAttack_ / 100ms, 0.f, 1.f );
		cooldownAttack_ -= deltaTime;
	}
	else {
		animTimeAttack_ = 0s;
		tAttack_ = 0.f;
	}

	if (dead_) {
		animTimeDeath_ += deltaTime;
		if (cooldownDeath_ > 0ms)
			tDeath_ = 1.f - std::clamp( cooldownDeath_ / 300ms, 0.f, 1.f );
		else
			tDeath_ = 1.f;
		cooldownDeath_ -= deltaTime;
	}
	else if (cooldownHit_ > 0ms) {
		animTimeHit_ += deltaTime * 2.f;   // 2배속 재생
		tHit_ = 0.75f * std::clamp( cooldownHit_ / 600ms, 0.f, 1.f );
		cooldownHit_ -= deltaTime;
	}
	else {
		animTimeHit_ = 0s;
		tHit_ = 0.f;
	}
}

void AnimBlenderSnake::onCalcLocal(PassKey<AnimSystem>) {
	const bool hasAttack = !currentAttackClip_.empty();

	updateFrames("Snake_Walk",   animTimeIdle_);
	updateFrames("Snake_Walk",   animTimeWalk_);
	if (hasAttack) updateFrames(currentAttackClip_, animTimeAttack_);
	updateFrames("Snake_Hit",    animTimeHit_);
	updateFrames("Snake_Death",  animTimeDeath_);

	auto& localXforms  = localXformData();
	auto& framesIdle   = curFrames("Snake_Walk");
	auto& framesWalk   = curFrames("Snake_Walk");
	auto* framesAttack = hasAttack ? &curFrames(currentAttackClip_) : nullptr;
	auto& framesHit    = curFrames("Snake_Hit");
	auto& framesDeath  = curFrames("Snake_Death");

	if (mode_ == Mode::Baked) {
		if (tDeath_ > 0.01f) {
			auto& deathClip = targetClip("Snake_Death");
			finalBakedClipId_    = deathClip->id;
			finalBakedClipFrame_ = static_cast<int>( deathClip->bakedSampleRate * animTimeDeath_.count() );
		}
		else if (hasAttack && tAttack_ > 0.01f) {
			auto& attackClip = targetClip(currentAttackClip_);
			finalBakedClipId_    = attackClip->id;
			finalBakedClipFrame_ = static_cast<int>( attackClip->bakedSampleRate * animTimeAttack_.count() );
		}
		else {
			float   weights[]   = { tIdle_, tWalk_ };
			Seconds animTimes[] = { animTimeIdle_, animTimeWalk_ };
			const AnimClip* clips[] = {
				targetClip("Snake_Walk").get(),
				targetClip("Snake_Walk").get()
			};
			auto i = static_cast<int>( std::distance(std::begin(weights), std::ranges::max_element(weights)) );
			finalBakedClipId_    = clips[i]->id;
			finalBakedClipFrame_ = static_cast<int>( clips[i]->bakedSampleRate * animTimes[i].count() );
		}
	}
	else /* if (mode_ == Mode::Keyframe) */ {
		for (std::size_t i = 0u; i < framesBlended_.size(); ++i) {
			WeightedAnimFrame frames[] = {
				WeightedAnimFrame{ .frame = framesIdle[i], .w = tIdle_ },
				WeightedAnimFrame{ .frame = framesWalk[i], .w = tWalk_ }
			};
			framesBlended_[i] = sumWeightedAnimFrames(frames);
			if (framesAttack) framesBlended_[i] = lerpAnimFrames(framesBlended_[i], (*framesAttack)[i], tAttack_);
			framesBlended_[i] = lerpAnimFrames(framesBlended_[i], framesHit[i],    tHit_);
			framesBlended_[i] = lerpAnimFrames(framesBlended_[i], framesDeath[i],  tDeath_);
		}
		std::ranges::transform(framesBlended_, localXforms.begin(), convertAnimFrameToMatrix);
	}
}

void AnimBlenderSnake::EventBus::receive(const BasicEvent* event, Seconds deltaTime, EventList& evList, Timer& timer, void* pVoidOwner) {
	auto pOwner = static_cast<AnimBlenderSnake*>(pVoidOwner);

	switch (event->type) {
	case EventType::Hit:
		pOwner->animTimeHit_ = 0s;
		pOwner->cooldownHit_ = 600ms;
		break;

	case EventType::Death:
		pOwner->animTimeDeath_ = 0s;
		pOwner->cooldownDeath_ = 200ms;
		pOwner->dead_ = true;
		break;

	case EventType::Attack:
		if (!pOwner->attackClips_.empty()) {
			const auto idx = std::clamp<int>(
				static_cast<const EvAttack*>(event)->attackIndex, 0,
				static_cast<int>(pOwner->attackClips_.size()) - 1);
			pOwner->currentAttackClip_ = pOwner->attackClips_[idx];
			pOwner->animTimeAttack_ = 0s;
			pOwner->cooldownAttack_ = 3000ms;
		}
		break;

	case EventType::Respawn:
		pOwner->dead_          = false;
		pOwner->tDeath_        = 0.f;
		pOwner->animTimeDeath_ = 0s;
		pOwner->cooldownDeath_ = 0ms;
		break;

	default:
		break;
	}
}

// ---------------------------------------------------------------------------
// AnimBlenderMushroom
// ---------------------------------------------------------------------------
void AnimBlenderMushroom::init(const Model* model, const std::vector<std::shared_ptr<AnimClip>>& anims) {
	setSkeleton(model->skeleton);
	framesBlended_.resize(skeleton().bones->size());
	for (auto& clip : anims)
		pushTargetClip(clip->name, clip);

	// 로드된 공격 클립만으로 순서 목록 구성(lua attackIndex가 인덱싱). 레거시 "Mushroom_Attack" 폴백.
	auto loaded = [&](const char* n) {
		for (auto& c : anims) if (c->name == n) return true;
		return false;
	};
	for (const char* name : { "Mushroom_Attack1", "Mushroom_Attack2", "Mushroom_Attack" })
		if (loaded(name)) attackClips_.push_back(name);
	if (!attackClips_.empty()) currentAttackClip_ = attackClips_.front();
}

void AnimBlenderMushroom::update(Seconds deltaTime, void* pVoidOwner) {
	auto pOwner = static_cast<Object*>(pVoidOwner);
	setOwnerPos(pOwner->pos());

	const auto walkThreshold = 0.06f;

	const auto speed = pOwner->velocity().len();

	const auto walkBlendRangeStart = walkThreshold - 0.03f;
	const auto walkBlendRangeEnd   = walkThreshold + 3.f;
	const auto targetTWalk = std::clamp( (speed - walkBlendRangeStart) / (walkBlendRangeEnd - walkBlendRangeStart), 0.f, 1.f );

	tWalk_ += (targetTWalk - tWalk_) * (1.f - std::exp(-deltaTime.count() / 0.12f));
	tIdle_ = 1.f - tWalk_;

	animTimeIdle_ += deltaTime;
	const auto durationIdle = targetClip("Mushroom_Idle")->duration;
	while (animTimeIdle_ > durationIdle) animTimeIdle_ -= durationIdle;

	if (tWalk_ > 0.f) {
		animTimeWalk_ += deltaTime;
		const auto durationWalk = targetClip("Mushroom_Walk")->duration;
		while (animTimeWalk_ > durationWalk) animTimeWalk_ -= durationWalk;
	}

	if (cooldownAttack_ > 0ms && !currentAttackClip_.empty()) {
		const auto durationAttack = targetClip(currentAttackClip_)->duration;
		animTimeAttack_ = std::min(durationAttack, animTimeAttack_ + deltaTime);
		tAttack_ = std::clamp( animTimeAttack_ / 100ms, 0.f, 1.f );
		cooldownAttack_ -= deltaTime;
	}
	else {
		animTimeAttack_ = 0s;
		tAttack_ = 0.f;
	}

	if (dead_) {
		animTimeDeath_ += deltaTime;
		if (cooldownDeath_ > 0ms)
			tDeath_ = 1.f - std::clamp( cooldownDeath_ / 300ms, 0.f, 1.f );
		else
			tDeath_ = 1.f;
		cooldownDeath_ -= deltaTime;
	}
	else if (cooldownHit_ > 0ms) {
		animTimeHit_ += deltaTime * 2.f;   // 2배속 재생
		tHit_ = 0.75f * std::clamp( cooldownHit_ / 600ms, 0.f, 1.f );
		cooldownHit_ -= deltaTime;
	}
	else {
		animTimeHit_ = 0s;
		tHit_ = 0.f;
	}
}

void AnimBlenderMushroom::onCalcLocal(PassKey<AnimSystem>) {
	const bool hasAttack = !currentAttackClip_.empty();

	updateFrames("Mushroom_Idle",   animTimeIdle_);
	updateFrames("Mushroom_Walk",   animTimeWalk_);
	if (hasAttack) updateFrames(currentAttackClip_, animTimeAttack_);
	updateFrames("Mushroom_Hit",    animTimeHit_);
	updateFrames("Mushroom_Death",  animTimeDeath_);

	auto& localXforms  = localXformData();
	auto& framesIdle   = curFrames("Mushroom_Idle");
	auto& framesWalk   = curFrames("Mushroom_Walk");
	auto* framesAttack = hasAttack ? &curFrames(currentAttackClip_) : nullptr;
	auto& framesHit    = curFrames("Mushroom_Hit");
	auto& framesDeath  = curFrames("Mushroom_Death");

	if (mode_ == Mode::Baked) {
		if (tDeath_ > 0.01f) {
			auto& deathClip = targetClip("Mushroom_Death");
			finalBakedClipId_    = deathClip->id;
			finalBakedClipFrame_ = static_cast<int>( deathClip->bakedSampleRate * animTimeDeath_.count() );
		}
		else if (hasAttack && tAttack_ > 0.01f) {
			auto& attackClip = targetClip(currentAttackClip_);
			finalBakedClipId_    = attackClip->id;
			finalBakedClipFrame_ = static_cast<int>( attackClip->bakedSampleRate * animTimeAttack_.count() );
		}
		else {
			float   weights[]   = { tIdle_, tWalk_ };
			Seconds animTimes[] = { animTimeIdle_, animTimeWalk_ };
			const AnimClip* clips[] = {
				targetClip("Mushroom_Idle").get(),
				targetClip("Mushroom_Walk").get()
			};
			auto i = static_cast<int>( std::distance(std::begin(weights), std::ranges::max_element(weights)) );
			finalBakedClipId_    = clips[i]->id;
			finalBakedClipFrame_ = static_cast<int>( clips[i]->bakedSampleRate * animTimes[i].count() );
		}
	}
	else /* if (mode_ == Mode::Keyframe) */ {
		for (std::size_t i = 0u; i < framesBlended_.size(); ++i) {
			WeightedAnimFrame frames[] = {
				WeightedAnimFrame{ .frame = framesIdle[i], .w = tIdle_ },
				WeightedAnimFrame{ .frame = framesWalk[i], .w = tWalk_ }
			};
			framesBlended_[i] = sumWeightedAnimFrames(frames);
			if (framesAttack) framesBlended_[i] = lerpAnimFrames(framesBlended_[i], (*framesAttack)[i], tAttack_);
			framesBlended_[i] = lerpAnimFrames(framesBlended_[i], framesHit[i],    tHit_);
			framesBlended_[i] = lerpAnimFrames(framesBlended_[i], framesDeath[i],  tDeath_);
		}
		std::ranges::transform(framesBlended_, localXforms.begin(), convertAnimFrameToMatrix);
	}
}

void AnimBlenderMushroom::EventBus::receive(const BasicEvent* event, Seconds deltaTime, EventList& evList, Timer& timer, void* pVoidOwner) {
	auto pOwner = static_cast<AnimBlenderMushroom*>(pVoidOwner);

	switch (event->type) {
	case EventType::Hit:
		pOwner->animTimeHit_ = 0s;
		pOwner->cooldownHit_ = 600ms;
		break;

	case EventType::Death:
		pOwner->animTimeDeath_ = 0s;
		pOwner->cooldownDeath_ = 200ms;
		pOwner->dead_ = true;
		break;

	case EventType::Attack:
		if (!pOwner->attackClips_.empty()) {
			const auto idx = std::clamp<int>(
				static_cast<const EvAttack*>(event)->attackIndex, 0,
				static_cast<int>(pOwner->attackClips_.size()) - 1);
			pOwner->currentAttackClip_ = pOwner->attackClips_[idx];
			pOwner->animTimeAttack_ = 0s;
			pOwner->cooldownAttack_ = 3000ms;
		}
		break;

	case EventType::Respawn:
		pOwner->dead_          = false;
		pOwner->tDeath_        = 0.f;
		pOwner->animTimeDeath_ = 0s;
		pOwner->cooldownDeath_ = 0ms;
		break;

	default:
		break;
	}
}

// ---------------------------------------------------------------------------
// AnimBlenderBoss
// ---------------------------------------------------------------------------
void AnimBlenderBoss::init(const Model* model, const std::vector<std::shared_ptr<AnimClip>>& anims) {
	setSkeleton(model->skeleton);
	framesBlended_.resize(skeleton().bones->size());
	for (auto& clip : anims)
		pushTargetClip(clip->name, clip);
}

void AnimBlenderBoss::update(Seconds deltaTime, void* pVoidOwner) {
	auto pOwner = static_cast<Object*>(pVoidOwner);
	setOwnerPos(pOwner->pos());

	const auto walkThreshold = 0.06f;
	const auto speed = pOwner->velocity().len();
	const auto walkBlendRangeStart = walkThreshold - 0.03f;
	const auto walkBlendRangeEnd = walkThreshold + 3.f;
	const auto targetTWalk = std::clamp((speed - walkBlendRangeStart) / (walkBlendRangeEnd - walkBlendRangeStart), 0.f, 1.f);

	tWalk_ += (targetTWalk - tWalk_) * (1.f - std::exp(-deltaTime.count() / 0.12f));
	tIdle_ = 1.f - tWalk_;

	animTimeIdle_ += deltaTime;
	const auto durationIdle = targetClip("Boss_Idle")->duration;
	while (animTimeIdle_ > durationIdle) animTimeIdle_ -= durationIdle;

	if (tWalk_ > 0.f) {
		animTimeWalk_ += deltaTime;
		const auto durationWalk = targetClip("Boss_Walk")->duration;
		while (animTimeWalk_ > durationWalk) animTimeWalk_ -= durationWalk;
	}

	if (cooldownAttack_ > 0ms) {
		const auto durationAttack = targetClip(currentAttackClip_)->duration;
		animTimeAttack_ = std::min(durationAttack, animTimeAttack_ + deltaTime);
		tAttack_ = std::clamp(animTimeAttack_ / 100ms, 0.f, 1.f);
		cooldownAttack_ -= deltaTime;
	}
	else {
		animTimeAttack_ = 0s;
		tAttack_ = 0.f;
	}
}

void AnimBlenderBoss::onCalcLocal(PassKey<AnimSystem>) {
	updateFrames("Boss_Idle", animTimeIdle_);
	updateFrames("Boss_Walk", animTimeWalk_);
	updateFrames(currentAttackClip_, animTimeAttack_);

	auto& localXforms = localXformData();
	auto& framesIdle = curFrames("Boss_Idle");
	auto& framesWalk = curFrames("Boss_Walk");
	auto& framesAttack = curFrames(currentAttackClip_);

	if (mode_ == Mode::Baked) {
		if (tAttack_ > 0.01f) {
			auto& attackClip = targetClip(currentAttackClip_);
			finalBakedClipId_ = attackClip->id;
			finalBakedClipFrame_ = static_cast<int>(attackClip->bakedSampleRate * animTimeAttack_.count());
		}
		else {
			float weights[] = { tIdle_, tWalk_ };
			Seconds animTimes[] = { animTimeIdle_, animTimeWalk_ };
			const AnimClip* clips[] = {
				targetClip("Boss_Idle").get(),
				targetClip("Boss_Walk").get()
			};
			auto i = static_cast<int>(std::distance(std::begin(weights), std::ranges::max_element(weights)));
			finalBakedClipId_ = clips[i]->id;
			finalBakedClipFrame_ = static_cast<int>(clips[i]->bakedSampleRate * animTimes[i].count());
		}
	}
	else {
		for (std::size_t i = 0u; i < framesBlended_.size(); ++i) {
			WeightedAnimFrame frames[] = {
				WeightedAnimFrame{ .frame = framesIdle[i], .w = tIdle_ },
				WeightedAnimFrame{ .frame = framesWalk[i], .w = tWalk_ }
			};
			framesBlended_[i] = sumWeightedAnimFrames(frames);
			framesBlended_[i] = lerpAnimFrames(framesBlended_[i], framesAttack[i], tAttack_);
		}
		std::ranges::transform(framesBlended_, localXforms.begin(), convertAnimFrameToMatrix);
	}
}

void AnimBlenderBoss::EventBus::receive(const BasicEvent* event, Seconds deltaTime, EventList& evList, Timer& timer, void* pVoidOwner) {
	auto pOwner = static_cast<AnimBlenderBoss*>(pVoidOwner);

	switch (event->type) {
	case EventType::Attack:
		pOwner->animTimeAttack_ = 0s;
		pOwner->cooldownAttack_ = 3000ms;
		break;
	case EventType::Respawn:
		pOwner->animTimeAttack_ = 0s;
		pOwner->cooldownAttack_ = 0ms;
		pOwner->tAttack_ = 0.f;
		break;
	default:
		break;
	}
}

// 모델을 설정한다.
// 모델이 있는 게임 객체는 render 시 GFX에 DrawEvent를 제출한다.
// 모델에 바운딩 볼륨이 존재할 경우, 월드 공간 바운딩 볼륨을 구축한다.
// (모델의 바운딩 볼륨을 기반으로 게임 객체의 월드 변환을 적용한
//  월드 공간 바운딩 볼륨을 따로 두어야 월드 공간 충돌 처리가 가능하다.)
void Object::adoptAnimBlender(std::unique_ptr<AnimBlender> blender, AnimSystem& animSystem) {
	if (renderState_.animBlender)
		animSystem.untrackAnimBlender(renderState_.animBlender.get());
	if (blender)
		animSystem.trackAnimBlender(blender.get());
	renderState_.animBlender = std::move(blender);
}

void Object::setModel(const Model* pModel){
	DISPLAY_ERROR_STR(pModel != nullptr, "[Game Error] Object::setModel: 널 모델이 전달되었습니다.", false);
	if (pModel == nullptr) {
		return;
	}

	renderState_.pModel = pModel;
	modelBaseScale_ = pModel->baseScale;   // 모델 고유 scale 흡수 (vertex bake 대신 런타임 적용)
	applyCompositeScale();                 // body_.scale() 갱신 + rebuildBodyBVH
	renderState_.worldBVs.resize(body_.worldBVH().nodes.size());
}

// @brief 게임 객체의 RenderState와 방향 벡터들을 갱신한다.
//		RenderState는 이전 PhysicState와 현재 PhysicState를 보간하여 얻어지고,
//      방향 벡터들은 현재 PhysicState의 내용으로 계산한다.
// @param deltaTime 마지막 프레임으로부터 경과한 시간
// @param tPhysicInterpolation 이전 PhysicState와 현재 PhysicState의 보간 비율
//		(게임 객체가 계산해서 일괄적으로 전달해야 한다.)
// Absorption-ripple lifetime (seconds). Must match RIPPLE_LIFE in pbrDeferredSkinned.hlsl
// so the CPU evicts a ripple exactly when the shader's time-fade reaches zero.
static constexpr float kBodyRippleLife = 1.0f;

void Object::update(Milliseconds deltaTime, float tPhysicInterpolation) {
	// Age + evict absorption ripples first, independent of cull/hidden state, so a
	// triggered ripple always plays out even if the body is briefly culled.
	if (!bodyRipples_.empty()) {
		const float dt = Seconds(deltaTime).count();
		for (auto& r : bodyRipples_) r.age += dt;
		std::erase_if(bodyRipples_, [](const BodyRipple& r) { return r.age >= kBodyRippleLife; });
	}

	if (hidden_) return;   // hidden NPC: skip interpolation/anim entirely (S_NpcHide; restored on respawn)
	const auto& prev = body_.prev();
	const auto& curr = body_.curr();
	const auto t = tPhysicInterpolation;

	// 방향 벡터 갱신 (physics state 기반, 로직에서도 사용)
	right_ = curr.orient.rotate(mu::Vec3(1.f, 0.f, 0.f));
	up_ = curr.orient.rotate(mu::Vec3(0.f, 1.f, 0.f));
	forward_ = curr.orient.rotate(mu::Vec3(0.f, 0.f, 1.f));

	if (renderState_.viewFrustumCulled || hiZCulled_)
		return;

	// 렌더 상태 갱신
	auto& pos = renderState_.pos;
	pos = PhysicsWorld::interpolatePos(body_, t);
	auto& orient = renderState_.orient;
	orient = PhysicsWorld::interpolateOrient(body_, t);
	auto& scale = renderState_.scale;
	scale = mu::lerp(prev.scale, curr.scale, t);

	const auto pModel = renderState_.pModel;

	renderState_.world = mu::Mat4x4(mu::scale(scale)) * mu::Mat4x4(orient) * mu::translate(pos);
	if (pModel && !pModel->bvh.empty()) {
		const auto& localBVH = pModel->bvh;
		const auto& skeleton = pModel->skeleton;
		const bool  hasBones = renderState_.animBlender
		                    && skeleton.bones && !skeleton.bones->empty();
		renderState_.worldBVs.resize(localBVH.nodes.size());
		for (std::size_t i = 0; i < localBVH.nodes.size(); ++i) {
			const auto& node    = localBVH.nodes[i];
			const bool  useBone = hasBones && node.boneIdx >= 0
			                   && node.boneIdx < (int)skeleton.bones->size();
			if (useBone) {
				const auto&      bone        = skeleton.bones->at(node.boneIdx);
				const mu::Mat4x4 boneToWorld = bone.toDress
											 * renderState_.animBlender->finalXformData()[node.boneIdx]
				                             * renderState_.world;
				std::visit([&](auto&& s) {
					using T = std::decay_t<decltype(s)>;
					mu::Vec3  lc, lh;
					mu::NQuat lo{};
					if constexpr (std::is_same_v<T, AABB>) { lc = s.center; lh = s.size * 0.5f; }
					else { lc = s.center; lh = s.halfExtents; lo = s.orient; }
					const mu::Vec3  wc = mu::Vec3(mu::Vec4(lc, 1.f) * boneToWorld);
					const mu::Vec3  wh = lh * scale;
					// Orientation from a scale-free matrix (mirrors rebuildBodyBVH); the scaled world
						// distorts quatRotMat. Extents already take object scale via `wh = lh * scale`.
						const mu::Mat4x4 boneToWorldRigid = bone.toDress
							* renderState_.animBlender->finalXformData()[node.boneIdx]
							* mu::Mat4x4(orient) * mu::translate(pos);
						const mu::NQuat wo = lo * mu::NQuat{ mu::Quat{ mu::quatRotMat(boneToWorldRigid.get()) } };
					renderState_.worldBVs[i] = mu::Mat4x4(mu::scale(wh * 2.f))
					                         * mu::Mat4x4(wo)
					                         * mu::translate(wc);
				}, node.shape);
			} else {
				std::visit([&](auto&& localShape) {
					using T = std::decay_t<decltype(localShape)>;
					if constexpr (std::is_same_v<T, AABB>) {
						const auto bvCenter = localShape.center * scale + pos;
						const auto bvSize   = localShape.size * scale;
						renderState_.worldBVs[i] = mu::Mat4x4(mu::scale(bvSize)) * mu::translate(bvCenter);
					} else {
						mu::NQuat worldOrient = orient;
						worldOrient *= localShape.orient;
						const auto bvCenter = orient.rotate(localShape.center * scale) + pos;
						renderState_.worldBVs[i] = mu::Mat4x4(mu::scale(localShape.halfExtents * scale * 2.f))
						                         * mu::Mat4x4(worldOrient)
						                         * mu::translate(bvCenter);
					}
				}, node.shape);
			}
		}
	}

	if (renderState_.animBlender) {
		renderState_.animBlender->update(deltaTime, this);
	}

	// 부속 객체 갱신
	for (auto& equipment : equipments_) {
		equipment.object->update(deltaTime, tPhysicInterpolation);
	}
}

void MU_CALLCONV Object::addBodyRipple(mu::Vec3 contactPosW, mu::Vec3 colorHDR, float intensity) {
	// Evict the oldest when full so the newest absorption is always shown.
	if (bodyRipples_.size() >= static_cast<std::size_t>(kMaxBodyRipples))
		bodyRipples_.erase(bodyRipples_.begin());
	// Store relative to the current body position; render re-anchors to the live pos.
	bodyRipples_.push_back(BodyRipple{ contactPosW - renderState_.pos, colorHDR, 0.f, intensity });
}

void MU_CALLCONV Object::render(GFX& gfx, mu::Mat4x4 offsetXform) {
	if (hidden_) return;   // hidden NPC: not drawn (no corpse). Cleared on respawn.
	const auto pModel = renderState_.pModel;
	if (pModel) {
		const bool useDeferred  = (gfx.renderPath() == GFX::RenderPath::Deferred);
		const bool culled       = renderState_.viewFrustumCulled;
		const bool shadowCulled = shadowLightFrustumCulled_;

		// Hi-Z cull용 월드 공간 AABB(포즈/랙돌 추종). 스킨드 deferred 이벤트에만 전달한다.
		const std::optional<AABB> cullBounds = worldCullBounds();
		const bool     hasCullBounds = cullBounds.has_value();
		const mu::Vec3 cullMin = hasCullBounds
			? cullBounds->center - cullBounds->size * 0.5f : mu::Vec3{};
		const mu::Vec3 cullMax = hasCullBounds
			? cullBounds->center + cullBounds->size * 0.5f : mu::Vec3{};

		for (auto& [mesh, meshXform] : pModel->meshWithDressXforms) {
			const bool isSkinned = renderState_.animBlender
				&& mesh.vbIdxMap.contains(mesh.name + "_VB_BoneIndices");

			// baked 스키닝 경로 준비 여부.
			// 생성 직후 updatePriorities()가 거리 기반으로 mode_=Baked 로 바꾸지만,
			// finalBakedClipId_/Frame_ 은 onCalcLocal()의 Baked 분기에서만 세팅된다.
			// AnimSystem::update()는 time-slice(0.01s) 안에서 priority 순으로 일부 blender만
			// 처리하므로, 첫 프레임엔 onCalcLocal 이 한 번도 안 불려 finalBakedClipId_=0(int 기본값)
			// 인 채로 렌더될 수 있다. clip->id 는 전역 bindless descriptor 인덱스라 0은
			// 애니메이션이 아닌 텍스처(descriptor 0)를 가리키고, 이를 4x4 행렬로 읽으면
			// garbage 스키닝 → 캐릭터가 길게 늘어나는 stretch 가 발생한다.
			// 유효한 baked clip 이 준비되기 전엔 boneXforms(미갱신 시 identity=T-pose) 경로를 쓴다.
			bool bakedReady = false;
			if (isSkinned) {
				auto* ab = renderState_.animBlender.get();
				bakedReady = ab->mode() == AnimBlender::Mode::Baked
				          && ab->hasEverUpdated() && ab->finalBakedClipId() > 0;
			}

			for (std::size_t i = 0u; i < mesh.subMeshes.size(); ++i) {
				if (useDeferred) {
					if (isSkinned) {
						auto de = PBRDeferredSkinnedPipeline::DrawEvent{
							.world             = meshXform * offsetXform * renderState_.world,
							.boneXforms        = renderState_.animBlender->finalXformData(),
							.mesh              = &mesh,
							.subMesh           = &mesh.subMeshes[i],
							.material          = &mesh.materialSets[materialSetIdx_].materials[i],
							.renderObjectId    = renderObjectId_,
							.bakedClipId       = bakedReady
								? renderState_.animBlender->finalBakedClipId() : -1,
							.bakedClipFrame    = bakedReady
								? renderState_.animBlender->finalBakedClipFrame() : -1,
							.viewFrustumCulled = culled,
							.shadowCulled      = shadowCulled,
							.worldCullMin       = cullMin,
							.worldCullMax       = cullMax,
							.hasWorldCullBounds = hasCullBounds,
						};
						// Energy-orb absorption ripples (local player body wave). Empty for
						// every other skinned object -> rippleCount 0 -> no shader cost.
						const int nr = std::min<int>(static_cast<int>(bodyRipples_.size()),
							PBRDeferredSkinnedPipeline::DrawEvent::kMaxRipples);
						for (int r = 0; r < nr; ++r) {
							const auto& br = bodyRipples_[r];
							// Re-anchor to the live body position so the ring tracks the player.
							de.ripplePosAge[r]         = mu::Vec4(renderState_.pos + br.offset, br.age);
							de.rippleColorIntensity[r] = mu::Vec4(br.colorHDR, br.intensity);
						}
						de.rippleCount = static_cast<u32t>(nr);
						gfx.addDrawEvent(std::move(de));
					}
					else {
						gfx.addDrawEvent(PBRDeferredPipeline::DrawEvent{
							.world             = meshXform * offsetXform * renderState_.world,
							.mesh              = &mesh,
							.subMesh           = &mesh.subMeshes[i],
							.material          = &mesh.materialSets[materialSetIdx_].materials[i],
							.viewFrustumCulled = culled,
							.shadowCulled      = shadowCulled,
						});
					}
				}
				else {
					if (isSkinned) {
						gfx.addDrawEvent(PBRSkinnedPipeline::DrawEvent{
							.world             = meshXform * offsetXform * renderState_.world,
							.boneXforms        = renderState_.animBlender->finalXformData(),
							.mesh              = &mesh,
							.subMesh           = &mesh.subMeshes[i],
							.material          = &mesh.materialSets[materialSetIdx_].materials[i],
							.viewFrustumCulled = culled,
							.shadowCulled      = shadowCulled,
						});
					}
					else {
						gfx.addDrawEvent(PBRPipeline::DrawEvent{
							.world             = meshXform * offsetXform * renderState_.world,
							.mesh              = &mesh,
							.subMesh           = &mesh.subMeshes[i],
							.material          = &mesh.materialSets[materialSetIdx_].materials[i],
							.viewFrustumCulled = culled,
							.shadowCulled      = shadowCulled,
						});
					}
				}
			}
		}
	}

	/*if (willRenderBV_ && pModel && !pModel->bvh.empty()) {
		for (std::size_t i = 0u; i < pModel->bvh.nodes.size(); ++i) {
			gfx.addDrawEvent( BVPipeline::DrawEvent{
				.world   = offsetXform * renderState_.worldBVs[i],
				.bvModel = BVPipeline::BVModel::Box,
				.color   = bvColor_
			} );
		}
	}*/

	// 부속 객체 렌더링
	if (renderState_.animBlender) {
		auto& skeleton = renderState_.pModel->skeleton;

		for (auto& equipment : equipments_) {
			// 소켓이 달린 본의 변환을 반영해주어야 한다.
			// animBlender의 finalXformData는 Dress->Local->Animation->Dress의 변환 내용을 담고 있으므로
			// Local->Dress 변환을 앞에 넣어주어야 한다.
			// (부속 객체를 부모 객체의 좌표계로 연결하는 과정이라 생각하면 좋다.)
			//
			// 또한, 아이템마다 각 소켓에 부착되었을 때 오프셋이 존재하므로,
			// 해당 오프셋을 맨 앞에 곱해준다.
			//
			// 따라서 행렬곱 적용 순서는
			// 1. 아이템의 소켓으로부터의 offset 행렬
			// 2. 소켓 본을 dress 공간으로 옮기는 행렬
			// 3. 소켓 본의 애니메이션된 최종 행렬
			// 4. 객체의 월드변환 행렬(renderState_.world) 또는 부모 부속 객체의 최종 오프셋 행렬(offsetXform)
			// (둘 중 하나는 무조건 단위 행렬이다. 둘을 곱셈으로 이은 것에 헷갈리지 말자.)
			// 이다.
			// equipment는 cullObjects()에서 독립적으로 컬링되지 않으므로 부모의 컬링 상태를 상속한다.
			equipment.object->setFrustumCulled(renderState_.viewFrustumCulled);
			equipment.object->setShadowCulled(shadowLightFrustumCulled_);
			equipment.object->render( gfx,
				equipment.object->renderState_.pModel->socketOffsets.at(equipment.socketType)
				* skeleton.bones->at( skeleton.socketToBoneIdx.at(equipment.socketType) ).toDress
				* renderState_.animBlender->finalXformData()[ skeleton.socketToBoneIdx.at(equipment.socketType) ]
				* offsetXform * renderState_.world
			);
		}
	}
}

void MU_CALLCONV Object::renderPortrait(GFX& gfx, u32t slot) {
	const auto pModel = renderState_.pModel;
	if (!pModel) return;

	for (auto& [mesh, meshXform] : pModel->meshWithDressXforms) {
		const bool isSkinned = renderState_.animBlender
			&& mesh.vbIdxMap.contains(mesh.name + "_VB_BoneIndices");
		if (!isSkinned) continue;   // 포트레이트는 스킨드 캐릭터만 대상으로 한다.

		for (std::size_t i = 0u; i < mesh.subMeshes.size(); ++i) {
			gfx.addLobbyPortraitDrawEvent(slot, PBRSkinnedPipeline::DrawEvent{
				.world             = meshXform * renderState_.world,
				.boneXforms        = renderState_.animBlender->finalXformData(),
				.mesh              = &mesh,
				.subMesh           = &mesh.subMeshes[i],
				.material          = &mesh.materialSets[materialSetIdx_].materials[i],
				.viewFrustumCulled = false,   // 포트레이트 카메라는 항상 캐릭터를 본다.
				.shadowCulled      = true,    // shadow pass 미수행.
			});
		}
	}
}

void MU_CALLCONV Object::setPos(mu::Vec3 newPos) {
	body_.setPos(newPos);
	body_.snapToCurrent();
	if (renderState_.pModel && !renderState_.pModel->bvh.empty())
		rebuildBodyBVH();
}

void MU_CALLCONV Object::setCurrPos(mu::Vec3 newPos) {
	body_.setPos(newPos);
	if (renderState_.pModel && !renderState_.pModel->bvh.empty())
		rebuildBodyBVH();
}

void MU_CALLCONV Object::setVelocity(mu::Vec3 newVelocity) {
	body_.setLinearVel(newVelocity);
}

void Object::updateGroundedGravityGate(const PhysicsWorld& world, Seconds physicsDt) {
	// physicsDt: 호출이 물리 step과 1:1로 일어남을 문서화하는 인자. 현재 지속 판정은
	// step 카운트 기반이라 직접 쓰지 않는다(시그니처는 향후 확장 대비).
	(void)physicsDt;
	// 캐릭터 컨트롤러 표준 패턴(Unity/UE)의 grounded gravity gating + ground snap.
	// Dynamic body만 의미가 있다(Kinematic/Static은 중력을 받지 않는다).
	if (body_.motionType() != MotionType::Dynamic) {
		grounded_      = false;
		groundedSteps_ = 0;
		body_.setGravityScale(1.f);
		return;
	}

	// 접지 판정 파라미터.
	//  kGroundNormalY    : 접촉 법선의 위쪽 정렬 임계값. terrain 접촉은 항상 Y-up
	//                      (collision.cpp testVertex)이지만, 일반화를 위해 충분히
	//                      위를 향하는 접촉만 지면으로 인정한다(가파른 벽 제외).
	//  kRiseMaxSpeed     : 이보다 빠르게 상승 중이면(점프/넉백) 절대 접지로 보지 않는다.
	//  kSnapMaxSpeed     : 접지 시 0으로 스냅할 하강 속도의 상한. 더 빠른 하강은
	//                      막 착지한 큰 낙하이므로 솔버가 처리하도록 둔다.
	//  kGroundedStepsMin : 게이트를 끄기 전 요구하는 연속 접지 step 수(상태 안정화).
	constexpr float kGroundNormalY    = 0.7f;   // ~45도 이내의 지면만 접지로 인정
	constexpr float kRiseMaxSpeed     = 0.05f;  // m/s, 상승 중이면 공중 판정
	constexpr float kSnapMaxSpeed     = 1.0f;   // m/s, 스냅 대상 하강 속도 상한
	constexpr int   kGroundedStepsMin = 2;      // 연속 접지 step 요구치

	// 이번 step이 생성한 terrain 접촉 중, 이 body의 지면 접촉(법선이 위를 향함)을 찾는다.
	// terrain 접촉에서는 dynamic body가 항상 bodyA(B=terrain), 법선은 B→A=terrain→body.
	bool hasGroundContact = false;
	world.forEachContact([&](const ContactConstraint& cc) {
		if (hasGroundContact)            return;
		if (!cc.isTerrainContact())      return;
		if (cc.bodyA != &body_)          return;   // 이 body의 접촉만
		for (int i = 0; i < cc.count; ++i) {
			if (mu::Vec3(cc.contacts[i].normal).y() >= kGroundNormalY) {
				hasGroundContact = true;
				break;
			}
		}
	});

	// 상승 중(점프/넉백)에는 즉시 공중 판정 → 중력 켜짐 → 정상 상승/낙하.
	const float vy = body_.linearVel().y();
	const bool  rising = vy > kRiseMaxSpeed;

	if (hasGroundContact && !rising) {
		// 접지 후보: 연속 step 카운트를 늘린다(상태 안정화).
		if (groundedSteps_ < kGroundedStepsMin)
			++groundedSteps_;
	} else {
		// 지면 접촉이 사라졌거나 상승 중: 즉시 공중으로 전환(낙하 지연 없음).
		groundedSteps_ = 0;
	}

	grounded_ = (groundedSteps_ >= kGroundedStepsMin);

	if (grounded_) {
		// 중력 게이트 off: 다음 step부터 중력이 접촉 솔버와 싸우지 않는다.
		body_.setGravityScale(0.f);
		// Ground-snap: 작은 하강 속도를 0으로 클램프해 잔여 튐을 제거한다.
		// 상승 속도는 절대 건드리지 않는다(점프/넉백 보존). 큰 하강 속도(막 착지)는
		// 솔버가 처리하도록 그대로 둔다.
		if (vy < 0.f && vy > -kSnapMaxSpeed) {
			const auto v = body_.linearVel();
			body_.setLinearVel(mu::Vec3(v.x(), 0.f, v.z()));
		}
	} else {
		// 공중: 중력 복원 → 점프/낙하가 이전과 동일하게 동작한다.
		body_.setGravityScale(1.f);
	}
}

void MU_CALLCONV Object::setOmega(mu::Vec3 newOmega) {
	body_.setOmega(newOmega);
}

void MU_CALLCONV Object::setOrient(mu::NQuat newOrient) {
	body_.setOrient(newOrient);
	body_.snapToCurrent();
	right_   = body_.orient().rotate(mu::Vec3(1.f, 0.f, 0.f));
	up_      = body_.orient().rotate(mu::Vec3(0.f, 1.f, 0.f));
	forward_ = body_.orient().rotate(mu::Vec3(0.f, 0.f, 1.f));
	if (renderState_.pModel && !renderState_.pModel->bvh.empty())
		rebuildBodyBVH();
}

// 모델 고유 scale과 게임플레이 per-instance scale을 component-wise 합성해 body_에 적용한다.
// setModel/setScale 호출 순서와 무관하게 합성식이 동일해 정합한다.
void Object::applyCompositeScale() {
	body_.setScale(modelBaseScale_ * instanceScale_);
	body_.snapToCurrent();
	if (renderState_.pModel && !renderState_.pModel->bvh.empty())
		rebuildBodyBVH();
}

void MU_CALLCONV Object::setScale(mu::Vec3 newScale) {
	instanceScale_ = newScale;
	applyCompositeScale();
}

void Object::equip(Equipment&& equipment) { 
	equipment.object->setPos(mu::Vec3());
	equipment.object->setVelocity(mu::Vec3());
	equipment.object->setOmega(mu::Vec3());
	equipment.object->setOrient(mu::NQuat());
	equipments_.push_back(std::move(equipment));
}

void Object::disequip(Bone::SocketType socketType) { 
	auto toRemoves = std::ranges::remove_if( equipments_, [socketType](const Equipment& equipment) { 
		return equipment.socketType == socketType; 
	} );

	for (auto& toRemove : toRemoves) {
		toRemove.object->body_.setPos(body_.pos());
		toRemove.object->body_.setOrient(body_.orient());
		// 부모의 합성 scale을 게임플레이 scale로 물려준다(자식 모델 고유 scale은 보존).
		// setScale 내부에서 snapToCurrent까지 수행된다.
		toRemove.object->setScale(scale());
	}

	equipments_.erase(toRemoves.begin(), toRemoves.end());
	
	// 필요하다면 본의 최종 변환으로부터 T, R을 추출해 분리된 오브젝트에 복원해내도록 한다.
}

Equipment* Object::getEquipment(Bone::SocketType socketType) {
	auto it = std::ranges::find( equipments_, socketType,
		[](const Equipment& equipment) { return equipment.socketType; }
	);
	
	if (it == equipments_.end()) {
		return nullptr;
	}

	return &*it;
}

const Equipment* Object::getEquipment(Bone::SocketType socketType) const {
	auto it = std::ranges::find( equipments_, socketType,
		[](const Equipment& equipment) { return equipment.socketType; }
	);
	
	if (it == equipments_.end()) {
		return nullptr;
	}

	return &*it;
}

// Rebuilds the world-space BVH stored in body_.worldBVH() from the model's
// local-space BVH template.  Tree structure (children indices) is preserved;
// only shape/bounds values are transformed.
//
// For bone-attached nodes (boneIdx >= 0) with an active animBlender:
//   boneToWorld = bone.toDress * finalXformData()[boneIdx] * objWorldMat
//   center is transformed as a homogeneous point; result is always OBB.
//
// For root-only nodes (boneIdx == -1) or when no animBlender is present:
//   AABB: apply pos + scale.
//   OBB:  apply pos + scale + orient (composed with local OBB orient).
void Object::rebuildBodyBVH() {
	const auto pModel = renderState_.pModel;
	if (!pModel || pModel->bvh.empty()) return;

	const auto& localBVH = pModel->bvh;
	const auto& skeleton = pModel->skeleton;
	const bool  hasBones = renderState_.animBlender
	                    && skeleton.bones && !skeleton.bones->empty();

	const mu::Vec3  bPos    = body_.pos();
	const mu::NQuat bOrient = body_.orient();
	const mu::Vec3  bScale  = body_.scale();

	// Object world matrix (physics-state based, same formula as renderState_.world)
	const mu::Mat4x4 objWorld = mu::Mat4x4(mu::scale(bScale))
	                          * mu::Mat4x4(bOrient)
	                          * mu::translate(bPos);
	const mu::Mat4x4 objRigid = mu::Mat4x4(bOrient)
	                          * mu::translate(bPos);

	auto& worldBVH = body_.worldBVH();
	worldBVH.nodes.resize(localBVH.nodes.size());
	for (std::size_t i = 0; i < localBVH.nodes.size(); ++i) {
		const auto& src = localBVH.nodes[i];
		auto&       dst = worldBVH.nodes[i];

		dst.children = src.children;
		dst.name     = src.name;
		dst.boneIdx  = src.boneIdx;

		const bool useBone = hasBones && src.boneIdx >= 0
		                  && src.boneIdx < (int)skeleton.bones->size();

		if (useBone) {
			const auto&      bone       = skeleton.bones->at(src.boneIdx);
			const auto&      boneXforms = renderState_.animBlender->finalXformData();
			// bone local -> animated dress -> object world
			const mu::Mat4x4 boneToWorld = bone.toDress * boneXforms[src.boneIdx] * objWorld;
			const mu::Mat4x4 boneToWorldRigid = bone.toDress * boneXforms[src.boneIdx] * objRigid;

			dst.shape = std::visit([&](auto&& s) -> std::variant<AABB, OBB> {
				using T = std::decay_t<decltype(s)>;
				mu::Vec3  localCenter, localHalfExtents;
				mu::NQuat localOrient{};
				if constexpr (std::is_same_v<T, AABB>) {
					localCenter      = s.center;
					localHalfExtents = s.size * 0.5f;
				} else {
					localCenter      = s.center;
					localHalfExtents = s.halfExtents;
					localOrient      = s.orient;
				}
				// Transform center as a homogeneous point through boneToWorld
				const mu::Vec3  worldCenter      = mu::Vec3(mu::Vec4(localCenter, 1.f) * boneToWorld);
				// Scale halfExtents by root object scale only (bone transforms are rigid)
				const mu::Vec3  worldHalfExtents = localHalfExtents * bScale;
				// Extract rotation from boneToWorld and compose with local orient
				const mu::NQuat boneWorldOrient{ mu::Quat{ mu::quatRotMat(boneToWorldRigid.get()) } };
				const mu::NQuat worldOrient      = localOrient * boneWorldOrient;
				return OBB{ worldCenter, worldHalfExtents, worldOrient };
			}, src.shape);
		} else {
			// Rigid (non-bone) path shared with ScatterCollider. Correctly turns a
			// rotated axis-aligned model box into a world OBB (previously the
			// rotation was dropped for AABB shapes).
			dst.shape = transformShapeRigid(src.shape, bPos, bOrient, bScale);
		}

		dst.bounds = std::visit([](auto&& s) -> AABB {
			using T = std::decay_t<decltype(s)>;
			if constexpr (std::is_same_v<T, OBB>) return obbToAABB(s);
			else                                   return s;
		}, dst.shape);
	}
}

// Hi-Z occlusion용 월드 공간 cull AABB.
// body_.worldBVH()의 본 부착 노드들은 bone.toDress * finalXformData * objWorld로
// 현재 포즈(랙돌 포함)를 따라가므로, 그 합집합이 실제로 그려지는 변형 메시의
// 월드 공간 범위를 보수적으로 감싼다. rebuildBodyBVH()가 컬링과 무관하게 매 프레임
// 갱신하므로 이 값도 자기강화 컬링 없이 visibility 변화를 추종한다.
std::optional<AABB> Object::worldCullBounds() const {
	const auto& nodes = body_.worldBVH().nodes;
	if (nodes.empty()) return std::nullopt;

	constexpr float kBig = std::numeric_limits<float>::max();
	mu::Vec3 mn( kBig,  kBig,  kBig);
	mu::Vec3 mx(-kBig, -kBig, -kBig);
	bool any = false;

	auto accumulate = [&](const BVHNode& n) {
		const mu::Vec3 c = n.bounds.center;
		const mu::Vec3 h = n.bounds.size * 0.5f;
		mn = mu::min(mn, c - h);
		mx = mu::max(mx, c + h);
		any = true;
	};

	// 본 부착 노드만 합집합한다. 비부착(boneIdx < 0) 노드는 물리 바디 변환만
	// 반영해 랙돌 시 사망 시점 위치에 고정되므로, 추종성을 해치지 않도록 제외.
	for (const auto& n : nodes)
		if (n.boneIdx >= 0) accumulate(n);

	// 본 부착 노드가 하나도 없으면(비스킨 모델) 전체 노드로 폴백.
	if (!any)
		for (const auto& n : nodes) accumulate(n);

	if (!any) return std::nullopt;

	// 충돌 프록시 박스가 시각 메시(옷/피부)보다 약간 작을 수 있으므로 여유를 둔다.
	const mu::Vec3 margin = (mx - mn) * 0.15f;
	mn = mn - margin;
	mx = mx + margin;
	return AABB{ (mn + mx) * 0.5f, mx - mn };
}

void Player::EventBus::receive(const BasicEvent* event, Seconds deltaTime, EventList& evList, Timer& timer, void* pVoidOwner) {
	auto* pOwner = static_cast<Player*>(pVoidOwner);
	switch (event->type) {
	case EventType::Hit:
		if (pOwner) {
			pOwner->hp_ = std::max(static_cast<const EvHit*>(event)->hp, 0);
			if (pOwner->renderState_.animBlender) {
				pOwner->renderState_.animBlender->eventBus()->receive(
					event, deltaTime, evList, timer,
					pOwner->renderState_.animBlender.get()
				);
			}
			holdEvent(evList, EvBlood(pOwner->getId()));
		}
		break;

	case EventType::Death:
		if (pOwner) {
			pOwner->hp_ = 0;
			pOwner->isDead_ = true;
			if (pOwner->renderState_.animBlender) {
				pOwner->renderState_.animBlender->eventBus()->receive(
					event, deltaTime, evList, timer,
					pOwner->renderState_.animBlender.get()
				);
			}
		}
		break;

	case EventType::Attack:
		if (pOwner && pOwner->renderState_.animBlender) {
			pOwner->renderState_.animBlender->eventBus()->receive(
				event, deltaTime, evList, timer,
				pOwner->renderState_.animBlender.get()
			);
		}
		break;

	case EventType::Respawn:
		if (pOwner) {
			pOwner->isDead_ = false;
			if (pOwner->renderState_.animBlender) {
				pOwner->renderState_.animBlender->eventBus()->receive(
					event, deltaTime, evList, timer,
					pOwner->renderState_.animBlender.get()
				);
			}
		}
		break;

	default:
		break;
	}
}

void Goblin::EventBus::receive(const BasicEvent* event, Seconds deltaTime, EventList& evList, Timer& timer, void* pVoidOwner) {
	auto pOwner = static_cast<Goblin*>(pVoidOwner);
	switch (event->type) {
	case EventType::Hit:
		if (pOwner) {
			if (pOwner->renderState_.animBlender) {
				pOwner->renderState_.animBlender->eventBus()->receive(
					event, deltaTime, evList, timer,
					pOwner->renderState_.animBlender.get()
				);
			}
			// holdEvent(evList, EvBlood(pOwner->getId()));
			pOwner->hp_ = std::max( static_cast<const EvHit*>(event)->hp, 0 );

			// 온라인 게임의 경우 서버에서 죽음 판정을 수행하므로
			// 공용 코드인 이곳에서 death event를 다루지 않는다.
		}
		break;

	case EventType::Death:
		if (pOwner && !pOwner->isDead_) {
			pOwner->isDead_ = true;
			if (pOwner->renderState_.animBlender) {
				pOwner->renderState_.animBlender->eventBus()->receive(
					event, deltaTime, evList, timer,
					pOwner->renderState_.animBlender.get()
				);
			}
			pOwner->hp_ = 0;
			if (pOwner->ragdoll_.isBuilt())
				pOwner->ragdollPendingActivation_ = true;
		}
		break;

	case EventType::Attack:
		if (pOwner) {
			if (pOwner->renderState_.animBlender) {
				pOwner->renderState_.animBlender->eventBus()->receive(
					event, deltaTime, evList, timer,
					pOwner->renderState_.animBlender.get()
				);
			}
		}
		break;

	case EventType::Respawn:
		if (pOwner) {
			pOwner->isDead_ = false;
			if (pOwner->renderState_.animBlender) {
				pOwner->renderState_.animBlender->eventBus()->receive(
					event, deltaTime, evList, timer,
					pOwner->renderState_.animBlender.get()
				);
			}
		}
		break;

	default:
		break;
	}
}

void Snake::EventBus::receive(const BasicEvent* event, Seconds deltaTime, EventList& evList, Timer& timer, void* pVoidOwner) {
	auto pOwner = static_cast<Snake*>(pVoidOwner);
	switch (event->type) {
	case EventType::Hit:
		if (pOwner) {
			if (pOwner->renderState_.animBlender) {
				pOwner->renderState_.animBlender->eventBus()->receive(
					event, deltaTime, evList, timer,
					pOwner->renderState_.animBlender.get()
				);
			}
			pOwner->hp_ = std::max( static_cast<const EvHit*>(event)->hp, 0 );
		}
		break;

	case EventType::Death:
		if (pOwner && !pOwner->isDead_) {
			pOwner->isDead_ = true;
			if (pOwner->renderState_.animBlender) {
				pOwner->renderState_.animBlender->eventBus()->receive(
					event, deltaTime, evList, timer,
					pOwner->renderState_.animBlender.get()
				);
			}
			pOwner->hp_ = 0;
			if (pOwner->ragdoll_.isBuilt())
				pOwner->ragdollPendingActivation_ = true;
		}
		break;

	case EventType::Attack:
		if (pOwner) {
			if (pOwner->renderState_.animBlender) {
				pOwner->renderState_.animBlender->eventBus()->receive(
					event, deltaTime, evList, timer,
					pOwner->renderState_.animBlender.get()
				);
			}
		}
		break;

	case EventType::Respawn:
		if (pOwner) {
			pOwner->isDead_ = false;
			if (pOwner->renderState_.animBlender) {
				pOwner->renderState_.animBlender->eventBus()->receive(
					event, deltaTime, evList, timer,
					pOwner->renderState_.animBlender.get()
				);
			}
		}
		break;

	default:
		break;
	}
}

void Mushroom::EventBus::receive(const BasicEvent* event, Seconds deltaTime, EventList& evList, Timer& timer, void* pVoidOwner) {
	auto pOwner = static_cast<Mushroom*>(pVoidOwner);
	switch (event->type) {
	case EventType::Hit:
		if (pOwner) {
			if (pOwner->renderState_.animBlender) {
				pOwner->renderState_.animBlender->eventBus()->receive(
					event, deltaTime, evList, timer,
					pOwner->renderState_.animBlender.get()
				);
			}
			pOwner->hp_ = std::max( static_cast<const EvHit*>(event)->hp, 0 );
		}
		break;

	case EventType::Death:
		if (pOwner && !pOwner->isDead_) {
			pOwner->isDead_ = true;
			if (pOwner->renderState_.animBlender) {
				pOwner->renderState_.animBlender->eventBus()->receive(
					event, deltaTime, evList, timer,
					pOwner->renderState_.animBlender.get()
				);
			}
			pOwner->hp_ = 0;
			if (pOwner->ragdoll_.isBuilt())
				pOwner->ragdollPendingActivation_ = true;
		}
		break;

	case EventType::Attack:
		if (pOwner) {
			if (pOwner->renderState_.animBlender) {
				pOwner->renderState_.animBlender->eventBus()->receive(
					event, deltaTime, evList, timer,
					pOwner->renderState_.animBlender.get()
				);
			}
		}
		break;

	case EventType::Respawn:
		if (pOwner) {
			pOwner->isDead_ = false;
			if (pOwner->renderState_.animBlender) {
				pOwner->renderState_.animBlender->eventBus()->receive(
					event, deltaTime, evList, timer,
					pOwner->renderState_.animBlender.get()
				);
			}
		}
		break;

	default:
		break;
	}
}

// Bomber/Birdy/Slime/Treant Object EventBus -- identical to Goblin::EventBus::receive
// (forward Hit/Death/Attack/Respawn to the AnimBlender, mirror server HP, set ragdoll
// pending on death). Online death judgment is server-authoritative; no local Death post.
void Bomber::EventBus::receive(const BasicEvent* event, Seconds deltaTime, EventList& evList, Timer& timer, void* pVoidOwner) {
	auto pOwner = static_cast<Bomber*>(pVoidOwner);
	switch (event->type) {
	case EventType::Hit:
		if (pOwner) {
			if (pOwner->renderState_.animBlender)
				pOwner->renderState_.animBlender->eventBus()->receive(event, deltaTime, evList, timer, pOwner->renderState_.animBlender.get());
			pOwner->hp_ = std::max(static_cast<const EvHit*>(event)->hp, 0);
		}
		break;
	case EventType::Death:
		if (pOwner && !pOwner->isDead_) {
			pOwner->isDead_ = true;
			if (pOwner->renderState_.animBlender)
				pOwner->renderState_.animBlender->eventBus()->receive(event, deltaTime, evList, timer, pOwner->renderState_.animBlender.get());
			pOwner->hp_ = 0;
			if (pOwner->ragdoll_.isBuilt()) pOwner->ragdollPendingActivation_ = true;
		}
		break;
	case EventType::Attack:
	case EventType::Respawn:
		if (pOwner) {
			if (event->type == EventType::Respawn) pOwner->isDead_ = false;
			if (pOwner->renderState_.animBlender)
				pOwner->renderState_.animBlender->eventBus()->receive(event, deltaTime, evList, timer, pOwner->renderState_.animBlender.get());
		}
		break;
	default:
		break;
	}
}

void Birdy::EventBus::receive(const BasicEvent* event, Seconds deltaTime, EventList& evList, Timer& timer, void* pVoidOwner) {
	auto pOwner = static_cast<Birdy*>(pVoidOwner);
	switch (event->type) {
	case EventType::Hit:
		if (pOwner) {
			if (pOwner->renderState_.animBlender)
				pOwner->renderState_.animBlender->eventBus()->receive(event, deltaTime, evList, timer, pOwner->renderState_.animBlender.get());
			pOwner->hp_ = std::max(static_cast<const EvHit*>(event)->hp, 0);
		}
		break;
	case EventType::Death:
		if (pOwner && !pOwner->isDead_) {
			pOwner->isDead_ = true;
			if (pOwner->renderState_.animBlender)
				pOwner->renderState_.animBlender->eventBus()->receive(event, deltaTime, evList, timer, pOwner->renderState_.animBlender.get());
			pOwner->hp_ = 0;
			if (pOwner->ragdoll_.isBuilt()) pOwner->ragdollPendingActivation_ = true;
		}
		break;
	case EventType::Attack:
	case EventType::Respawn:
		if (pOwner) {
			if (event->type == EventType::Respawn) pOwner->isDead_ = false;
			if (pOwner->renderState_.animBlender)
				pOwner->renderState_.animBlender->eventBus()->receive(event, deltaTime, evList, timer, pOwner->renderState_.animBlender.get());
		}
		break;
	default:
		break;
	}
}

void Slime::EventBus::receive(const BasicEvent* event, Seconds deltaTime, EventList& evList, Timer& timer, void* pVoidOwner) {
	auto pOwner = static_cast<Slime*>(pVoidOwner);
	switch (event->type) {
	case EventType::Hit:
		if (pOwner) {
			if (pOwner->renderState_.animBlender)
				pOwner->renderState_.animBlender->eventBus()->receive(event, deltaTime, evList, timer, pOwner->renderState_.animBlender.get());
			pOwner->hp_ = std::max(static_cast<const EvHit*>(event)->hp, 0);
		}
		break;
	case EventType::Death:
		if (pOwner && !pOwner->isDead_) {
			pOwner->isDead_ = true;
			if (pOwner->renderState_.animBlender)
				pOwner->renderState_.animBlender->eventBus()->receive(event, deltaTime, evList, timer, pOwner->renderState_.animBlender.get());
			pOwner->hp_ = 0;
			if (pOwner->ragdoll_.isBuilt()) pOwner->ragdollPendingActivation_ = true;
		}
		break;
	case EventType::Attack:
	case EventType::Respawn:
		if (pOwner) {
			if (event->type == EventType::Respawn) pOwner->isDead_ = false;
			if (pOwner->renderState_.animBlender)
				pOwner->renderState_.animBlender->eventBus()->receive(event, deltaTime, evList, timer, pOwner->renderState_.animBlender.get());
		}
		break;
	default:
		break;
	}
}

void Treant::EventBus::receive(const BasicEvent* event, Seconds deltaTime, EventList& evList, Timer& timer, void* pVoidOwner) {
	auto pOwner = static_cast<Treant*>(pVoidOwner);
	switch (event->type) {
	case EventType::Hit:
		if (pOwner) {
			if (pOwner->renderState_.animBlender)
				pOwner->renderState_.animBlender->eventBus()->receive(event, deltaTime, evList, timer, pOwner->renderState_.animBlender.get());
			pOwner->hp_ = std::max(static_cast<const EvHit*>(event)->hp, 0);
		}
		break;
	case EventType::Death:
		if (pOwner && !pOwner->isDead_) {
			pOwner->isDead_ = true;
			if (pOwner->renderState_.animBlender)
				pOwner->renderState_.animBlender->eventBus()->receive(event, deltaTime, evList, timer, pOwner->renderState_.animBlender.get());
			pOwner->hp_ = 0;
			if (pOwner->ragdoll_.isBuilt()) pOwner->ragdollPendingActivation_ = true;
		}
		break;
	case EventType::Attack:
	case EventType::Respawn:
		if (pOwner) {
			if (event->type == EventType::Respawn) pOwner->isDead_ = false;
			if (pOwner->renderState_.animBlender)
				pOwner->renderState_.animBlender->eventBus()->receive(event, deltaTime, evList, timer, pOwner->renderState_.animBlender.get());
		}
		break;
	default:
		break;
	}
}

void Goblin::setAnimBlender(AnimSystem& animSystem, const AssetManager& assetManager) {
	auto blender = std::make_unique<AnimBlenderGoblin>();
	blender->init(assetManager.modelGoblin(), assetManager.goblinAnimations());
	animSystem.trackAnimBlender(blender.get());
	renderState_.animBlender = std::move(blender);
}

// Hobgoblin은 Goblin과 동일한 메시(메시 로컬 정점 동일)·리그(91본)이고, 큰 외형은 루트 스케일이
// 반영된 bind pose(Dress)와 meshXform으로 표현된다. 같은 Goblin_* 클립/baked 클립을 그대로
// 재사용해도(루트 스케일은 스키닝 켤레에서 상쇄됨) keyframe·baked 양쪽 모두 동일하게 올바른 결과를
// 낸다 — 단, 바인드 포즈 불변식(toLocal == inverse(toDress))이 적재 시 강제되어야 한다(mesh.cpp 참조).
void Hobgoblin::setAnimBlender(AnimSystem& animSystem, const AssetManager& assetManager) {
	auto blender = std::make_unique<AnimBlenderGoblin>();
	blender->init(assetManager.modelHobgoblin(), assetManager.goblinAnimations());
	animSystem.trackAnimBlender(blender.get());
	renderState_.animBlender = std::move(blender);
}

void Snake::setAnimBlender(AnimSystem& animSystem, const AssetManager& assetManager) {
	auto blender = std::make_unique<AnimBlenderSnake>();
	blender->init(assetManager.modelSnake(), assetManager.snakeAnimations());
	animSystem.trackAnimBlender(blender.get());
	renderState_.animBlender = std::move(blender);
}

void Mushroom::setAnimBlender(AnimSystem& animSystem, const AssetManager& assetManager) {
	auto blender = std::make_unique<AnimBlenderMushroom>();
	blender->init(assetManager.modelMushroom(), assetManager.mushroomAnimations());
	animSystem.trackAnimBlender(blender.get());
	renderState_.animBlender = std::move(blender);
}

// Monster skill-caster implementations (Bomber/Birdy/Slime/Treant). Each is the
// AnimBlenderMushroom implementation with a different clip prefix + attack-clip list.
void AnimBlenderBomber::init(const Model* model, const std::vector<std::shared_ptr<AnimClip>>& anims) {
	setSkeleton(model->skeleton);
	framesBlended_.resize(skeleton().bones->size());
	for (auto& clip : anims)
		pushTargetClip(clip->name, clip);

	auto loaded = [&](const char* n) {
		for (auto& c : anims) if (c->name == n) return true;
		return false;
	};
	for (const char* name : { "Bomber_Attack1", "Bomber_Attack" })
		if (loaded(name)) attackClips_.push_back(name);
	if (!attackClips_.empty()) currentAttackClip_ = attackClips_.front();
}

void AnimBlenderBomber::update(Seconds deltaTime, void* pVoidOwner) {
	auto pOwner = static_cast<Object*>(pVoidOwner);
	setOwnerPos(pOwner->pos());

	const auto walkThreshold = 0.06f;
	const auto speed = pOwner->velocity().len();
	const auto walkBlendRangeStart = walkThreshold - 0.03f;
	const auto walkBlendRangeEnd   = walkThreshold + 3.f;
	const auto targetTWalk = std::clamp( (speed - walkBlendRangeStart) / (walkBlendRangeEnd - walkBlendRangeStart), 0.f, 1.f );

	tWalk_ += (targetTWalk - tWalk_) * (1.f - std::exp(-deltaTime.count() / 0.12f));
	tIdle_ = 1.f - tWalk_;

	animTimeIdle_ += deltaTime;
	const auto durationIdle = targetClip("Bomber_Idle")->duration;
	while (animTimeIdle_ > durationIdle) animTimeIdle_ -= durationIdle;

	if (tWalk_ > 0.f) {
		animTimeWalk_ += deltaTime;
		const auto durationWalk = targetClip("Bomber_Walk")->duration;
		while (animTimeWalk_ > durationWalk) animTimeWalk_ -= durationWalk;
	}

	if (cooldownAttack_ > 0ms && !currentAttackClip_.empty()) {
		const auto durationAttack = targetClip(currentAttackClip_)->duration;
		animTimeAttack_ = std::min(durationAttack, animTimeAttack_ + deltaTime);
		tAttack_ = std::clamp( animTimeAttack_ / 100ms, 0.f, 1.f );
		cooldownAttack_ -= deltaTime;
	}
	else {
		animTimeAttack_ = 0s;
		tAttack_ = 0.f;
	}

	if (dead_) {
		animTimeDeath_ += deltaTime;
		if (cooldownDeath_ > 0ms)
			tDeath_ = 1.f - std::clamp( cooldownDeath_ / 300ms, 0.f, 1.f );
		else
			tDeath_ = 1.f;
		cooldownDeath_ -= deltaTime;
	}
	else if (cooldownHit_ > 0ms) {
		animTimeHit_ += deltaTime * 2.f;
		tHit_ = 0.75f * std::clamp( cooldownHit_ / 600ms, 0.f, 1.f );
		cooldownHit_ -= deltaTime;
	}
	else {
		animTimeHit_ = 0s;
		tHit_ = 0.f;
	}
}

void AnimBlenderBomber::onCalcLocal(PassKey<AnimSystem>) {
	const bool hasAttack = !currentAttackClip_.empty();

	updateFrames("Bomber_Idle",   animTimeIdle_);
	updateFrames("Bomber_Walk",   animTimeWalk_);
	if (hasAttack) updateFrames(currentAttackClip_, animTimeAttack_);
	updateFrames("Bomber_Hit",    animTimeHit_);
	updateFrames("Bomber_Death",  animTimeDeath_);

	auto& localXforms  = localXformData();
	auto& framesIdle   = curFrames("Bomber_Idle");
	auto& framesWalk   = curFrames("Bomber_Walk");
	auto* framesAttack = hasAttack ? &curFrames(currentAttackClip_) : nullptr;
	auto& framesHit    = curFrames("Bomber_Hit");
	auto& framesDeath  = curFrames("Bomber_Death");

	if (mode_ == Mode::Baked) {
		if (tDeath_ > 0.01f) {
			auto& deathClip = targetClip("Bomber_Death");
			finalBakedClipId_    = deathClip->id;
			finalBakedClipFrame_ = static_cast<int>( deathClip->bakedSampleRate * animTimeDeath_.count() );
		}
		else if (hasAttack && tAttack_ > 0.01f) {
			auto& attackClip = targetClip(currentAttackClip_);
			finalBakedClipId_    = attackClip->id;
			finalBakedClipFrame_ = static_cast<int>( attackClip->bakedSampleRate * animTimeAttack_.count() );
		}
		else {
			float   weights[]   = { tIdle_, tWalk_ };
			Seconds animTimes[] = { animTimeIdle_, animTimeWalk_ };
			const AnimClip* clips[] = {
				targetClip("Bomber_Idle").get(),
				targetClip("Bomber_Walk").get()
			};
			auto i = static_cast<int>( std::distance(std::begin(weights), std::ranges::max_element(weights)) );
			finalBakedClipId_    = clips[i]->id;
			finalBakedClipFrame_ = static_cast<int>( clips[i]->bakedSampleRate * animTimes[i].count() );
		}
	}
	else /* if (mode_ == Mode::Keyframe) */ {
		for (std::size_t i = 0u; i < framesBlended_.size(); ++i) {
			WeightedAnimFrame frames[] = {
				WeightedAnimFrame{ .frame = framesIdle[i], .w = tIdle_ },
				WeightedAnimFrame{ .frame = framesWalk[i], .w = tWalk_ }
			};
			framesBlended_[i] = sumWeightedAnimFrames(frames);
			if (framesAttack) framesBlended_[i] = lerpAnimFrames(framesBlended_[i], (*framesAttack)[i], tAttack_);
			framesBlended_[i] = lerpAnimFrames(framesBlended_[i], framesHit[i],    tHit_);
			framesBlended_[i] = lerpAnimFrames(framesBlended_[i], framesDeath[i],  tDeath_);
		}
		std::ranges::transform(framesBlended_, localXforms.begin(), convertAnimFrameToMatrix);
	}
}

void AnimBlenderBomber::EventBus::receive(const BasicEvent* event, Seconds deltaTime, EventList& evList, Timer& timer, void* pVoidOwner) {
	auto pOwner = static_cast<AnimBlenderBomber*>(pVoidOwner);
	switch (event->type) {
	case EventType::Hit:
		pOwner->animTimeHit_ = 0s;
		pOwner->cooldownHit_ = 600ms;
		break;
	case EventType::Death:
		pOwner->animTimeDeath_ = 0s;
		pOwner->cooldownDeath_ = 200ms;
		pOwner->dead_ = true;
		break;
	case EventType::Attack:
		if (!pOwner->attackClips_.empty()) {
			const auto idx = std::clamp<int>(
				static_cast<const EvAttack*>(event)->attackIndex, 0,
				static_cast<int>(pOwner->attackClips_.size()) - 1);
			pOwner->currentAttackClip_ = pOwner->attackClips_[idx];
			pOwner->animTimeAttack_ = 0s;
			pOwner->cooldownAttack_ = 3000ms;
		}
		break;
	case EventType::Respawn:
		pOwner->dead_          = false;
		pOwner->tDeath_        = 0.f;
		pOwner->animTimeDeath_ = 0s;
		pOwner->cooldownDeath_ = 0ms;
		break;
	default:
		break;
	}
}

void Bomber::setAnimBlender(AnimSystem& animSystem, const AssetManager& assetManager) {
	auto blender = std::make_unique<AnimBlenderBomber>();
	blender->init(assetManager.modelBomber(), assetManager.bomberAnimations());
	animSystem.trackAnimBlender(blender.get());
	renderState_.animBlender = std::move(blender);
}

void AnimBlenderBirdy::init(const Model* model, const std::vector<std::shared_ptr<AnimClip>>& anims) {
	setSkeleton(model->skeleton);
	framesBlended_.resize(skeleton().bones->size());
	for (auto& clip : anims)
		pushTargetClip(clip->name, clip);

	auto loaded = [&](const char* n) {
		for (auto& c : anims) if (c->name == n) return true;
		return false;
	};
	for (const char* name : { "Birdy_Attack1", "Birdy_Attack2", "Birdy_Attack" })
		if (loaded(name)) attackClips_.push_back(name);
	if (!attackClips_.empty()) currentAttackClip_ = attackClips_.front();
}

void AnimBlenderBirdy::update(Seconds deltaTime, void* pVoidOwner) {
	auto pOwner = static_cast<Object*>(pVoidOwner);
	setOwnerPos(pOwner->pos());

	const auto walkThreshold = 0.06f;
	const auto speed = pOwner->velocity().len();
	const auto walkBlendRangeStart = walkThreshold - 0.03f;
	const auto walkBlendRangeEnd   = walkThreshold + 3.f;
	const auto targetTWalk = std::clamp( (speed - walkBlendRangeStart) / (walkBlendRangeEnd - walkBlendRangeStart), 0.f, 1.f );

	tWalk_ += (targetTWalk - tWalk_) * (1.f - std::exp(-deltaTime.count() / 0.12f));
	tIdle_ = 1.f - tWalk_;

	animTimeIdle_ += deltaTime;
	const auto durationIdle = targetClip("Birdy_Idle")->duration;
	while (animTimeIdle_ > durationIdle) animTimeIdle_ -= durationIdle;

	if (tWalk_ > 0.f) {
		animTimeWalk_ += deltaTime;
		const auto durationWalk = targetClip("Birdy_Walk")->duration;
		while (animTimeWalk_ > durationWalk) animTimeWalk_ -= durationWalk;
	}

	if (cooldownAttack_ > 0ms && !currentAttackClip_.empty()) {
		const auto durationAttack = targetClip(currentAttackClip_)->duration;
		animTimeAttack_ = std::min(durationAttack, animTimeAttack_ + deltaTime);
		tAttack_ = std::clamp( animTimeAttack_ / 100ms, 0.f, 1.f );
		cooldownAttack_ -= deltaTime;
	}
	else {
		animTimeAttack_ = 0s;
		tAttack_ = 0.f;
	}

	if (dead_) {
		animTimeDeath_ += deltaTime;
		if (cooldownDeath_ > 0ms)
			tDeath_ = 1.f - std::clamp( cooldownDeath_ / 300ms, 0.f, 1.f );
		else
			tDeath_ = 1.f;
		cooldownDeath_ -= deltaTime;
	}
	else if (cooldownHit_ > 0ms) {
		animTimeHit_ += deltaTime * 2.f;
		tHit_ = 0.75f * std::clamp( cooldownHit_ / 600ms, 0.f, 1.f );
		cooldownHit_ -= deltaTime;
	}
	else {
		animTimeHit_ = 0s;
		tHit_ = 0.f;
	}
}

void AnimBlenderBirdy::onCalcLocal(PassKey<AnimSystem>) {
	const bool hasAttack = !currentAttackClip_.empty();

	updateFrames("Birdy_Idle",   animTimeIdle_);
	updateFrames("Birdy_Walk",   animTimeWalk_);
	if (hasAttack) updateFrames(currentAttackClip_, animTimeAttack_);
	updateFrames("Birdy_Hit",    animTimeHit_);
	updateFrames("Birdy_Death",  animTimeDeath_);

	auto& localXforms  = localXformData();
	auto& framesIdle   = curFrames("Birdy_Idle");
	auto& framesWalk   = curFrames("Birdy_Walk");
	auto* framesAttack = hasAttack ? &curFrames(currentAttackClip_) : nullptr;
	auto& framesHit    = curFrames("Birdy_Hit");
	auto& framesDeath  = curFrames("Birdy_Death");

	if (mode_ == Mode::Baked) {
		if (tDeath_ > 0.01f) {
			auto& deathClip = targetClip("Birdy_Death");
			finalBakedClipId_    = deathClip->id;
			finalBakedClipFrame_ = static_cast<int>( deathClip->bakedSampleRate * animTimeDeath_.count() );
		}
		else if (hasAttack && tAttack_ > 0.01f) {
			auto& attackClip = targetClip(currentAttackClip_);
			finalBakedClipId_    = attackClip->id;
			finalBakedClipFrame_ = static_cast<int>( attackClip->bakedSampleRate * animTimeAttack_.count() );
		}
		else {
			float   weights[]   = { tIdle_, tWalk_ };
			Seconds animTimes[] = { animTimeIdle_, animTimeWalk_ };
			const AnimClip* clips[] = {
				targetClip("Birdy_Idle").get(),
				targetClip("Birdy_Walk").get()
			};
			auto i = static_cast<int>( std::distance(std::begin(weights), std::ranges::max_element(weights)) );
			finalBakedClipId_    = clips[i]->id;
			finalBakedClipFrame_ = static_cast<int>( clips[i]->bakedSampleRate * animTimes[i].count() );
		}
	}
	else /* if (mode_ == Mode::Keyframe) */ {
		for (std::size_t i = 0u; i < framesBlended_.size(); ++i) {
			WeightedAnimFrame frames[] = {
				WeightedAnimFrame{ .frame = framesIdle[i], .w = tIdle_ },
				WeightedAnimFrame{ .frame = framesWalk[i], .w = tWalk_ }
			};
			framesBlended_[i] = sumWeightedAnimFrames(frames);
			if (framesAttack) framesBlended_[i] = lerpAnimFrames(framesBlended_[i], (*framesAttack)[i], tAttack_);
			framesBlended_[i] = lerpAnimFrames(framesBlended_[i], framesHit[i],    tHit_);
			framesBlended_[i] = lerpAnimFrames(framesBlended_[i], framesDeath[i],  tDeath_);
		}
		std::ranges::transform(framesBlended_, localXforms.begin(), convertAnimFrameToMatrix);
	}
}

void AnimBlenderBirdy::EventBus::receive(const BasicEvent* event, Seconds deltaTime, EventList& evList, Timer& timer, void* pVoidOwner) {
	auto pOwner = static_cast<AnimBlenderBirdy*>(pVoidOwner);
	switch (event->type) {
	case EventType::Hit:
		pOwner->animTimeHit_ = 0s;
		pOwner->cooldownHit_ = 600ms;
		break;
	case EventType::Death:
		pOwner->animTimeDeath_ = 0s;
		pOwner->cooldownDeath_ = 200ms;
		pOwner->dead_ = true;
		break;
	case EventType::Attack:
		if (!pOwner->attackClips_.empty()) {
			const auto idx = std::clamp<int>(
				static_cast<const EvAttack*>(event)->attackIndex, 0,
				static_cast<int>(pOwner->attackClips_.size()) - 1);
			pOwner->currentAttackClip_ = pOwner->attackClips_[idx];
			pOwner->animTimeAttack_ = 0s;
			pOwner->cooldownAttack_ = 3000ms;
		}
		break;
	case EventType::Respawn:
		pOwner->dead_          = false;
		pOwner->tDeath_        = 0.f;
		pOwner->animTimeDeath_ = 0s;
		pOwner->cooldownDeath_ = 0ms;
		break;
	default:
		break;
	}
}

void Birdy::setAnimBlender(AnimSystem& animSystem, const AssetManager& assetManager) {
	auto blender = std::make_unique<AnimBlenderBirdy>();
	blender->init(assetManager.modelBirdy(), assetManager.birdyAnimations());
	animSystem.trackAnimBlender(blender.get());
	renderState_.animBlender = std::move(blender);
}

void AnimBlenderSlime::init(const Model* model, const std::vector<std::shared_ptr<AnimClip>>& anims) {
	setSkeleton(model->skeleton);
	framesBlended_.resize(skeleton().bones->size());
	for (auto& clip : anims)
		pushTargetClip(clip->name, clip);

	auto loaded = [&](const char* n) {
		for (auto& c : anims) if (c->name == n) return true;
		return false;
	};
	for (const char* name : { "Slime_Attack1", "Slime_Attack" })
		if (loaded(name)) attackClips_.push_back(name);
	if (!attackClips_.empty()) currentAttackClip_ = attackClips_.front();
}

void AnimBlenderSlime::update(Seconds deltaTime, void* pVoidOwner) {
	auto pOwner = static_cast<Object*>(pVoidOwner);
	setOwnerPos(pOwner->pos());

	const auto walkThreshold = 0.06f;
	const auto speed = pOwner->velocity().len();
	const auto walkBlendRangeStart = walkThreshold - 0.03f;
	const auto walkBlendRangeEnd   = walkThreshold + 3.f;
	const auto targetTWalk = std::clamp( (speed - walkBlendRangeStart) / (walkBlendRangeEnd - walkBlendRangeStart), 0.f, 1.f );

	tWalk_ += (targetTWalk - tWalk_) * (1.f - std::exp(-deltaTime.count() / 0.12f));
	tIdle_ = 1.f - tWalk_;

	animTimeIdle_ += deltaTime;
	const auto durationIdle = targetClip("Slime_Idle")->duration;
	while (animTimeIdle_ > durationIdle) animTimeIdle_ -= durationIdle;

	if (tWalk_ > 0.f) {
		animTimeWalk_ += deltaTime;
		const auto durationWalk = targetClip("Slime_Walk")->duration;
		while (animTimeWalk_ > durationWalk) animTimeWalk_ -= durationWalk;
	}

	if (cooldownAttack_ > 0ms && !currentAttackClip_.empty()) {
		const auto durationAttack = targetClip(currentAttackClip_)->duration;
		animTimeAttack_ = std::min(durationAttack, animTimeAttack_ + deltaTime);
		tAttack_ = std::clamp( animTimeAttack_ / 100ms, 0.f, 1.f );
		cooldownAttack_ -= deltaTime;
	}
	else {
		animTimeAttack_ = 0s;
		tAttack_ = 0.f;
	}

	if (dead_) {
		animTimeDeath_ += deltaTime;
		if (cooldownDeath_ > 0ms)
			tDeath_ = 1.f - std::clamp( cooldownDeath_ / 300ms, 0.f, 1.f );
		else
			tDeath_ = 1.f;
		cooldownDeath_ -= deltaTime;
	}
	else if (cooldownHit_ > 0ms) {
		animTimeHit_ += deltaTime * 2.f;
		tHit_ = 0.75f * std::clamp( cooldownHit_ / 600ms, 0.f, 1.f );
		cooldownHit_ -= deltaTime;
	}
	else {
		animTimeHit_ = 0s;
		tHit_ = 0.f;
	}
}

void AnimBlenderSlime::onCalcLocal(PassKey<AnimSystem>) {
	const bool hasAttack = !currentAttackClip_.empty();

	updateFrames("Slime_Idle",   animTimeIdle_);
	updateFrames("Slime_Walk",   animTimeWalk_);
	if (hasAttack) updateFrames(currentAttackClip_, animTimeAttack_);
	updateFrames("Slime_Hit",    animTimeHit_);
	updateFrames("Slime_Death",  animTimeDeath_);

	auto& localXforms  = localXformData();
	auto& framesIdle   = curFrames("Slime_Idle");
	auto& framesWalk   = curFrames("Slime_Walk");
	auto* framesAttack = hasAttack ? &curFrames(currentAttackClip_) : nullptr;
	auto& framesHit    = curFrames("Slime_Hit");
	auto& framesDeath  = curFrames("Slime_Death");

	if (mode_ == Mode::Baked) {
		if (tDeath_ > 0.01f) {
			auto& deathClip = targetClip("Slime_Death");
			finalBakedClipId_    = deathClip->id;
			finalBakedClipFrame_ = static_cast<int>( deathClip->bakedSampleRate * animTimeDeath_.count() );
		}
		else if (hasAttack && tAttack_ > 0.01f) {
			auto& attackClip = targetClip(currentAttackClip_);
			finalBakedClipId_    = attackClip->id;
			finalBakedClipFrame_ = static_cast<int>( attackClip->bakedSampleRate * animTimeAttack_.count() );
		}
		else {
			float   weights[]   = { tIdle_, tWalk_ };
			Seconds animTimes[] = { animTimeIdle_, animTimeWalk_ };
			const AnimClip* clips[] = {
				targetClip("Slime_Idle").get(),
				targetClip("Slime_Walk").get()
			};
			auto i = static_cast<int>( std::distance(std::begin(weights), std::ranges::max_element(weights)) );
			finalBakedClipId_    = clips[i]->id;
			finalBakedClipFrame_ = static_cast<int>( clips[i]->bakedSampleRate * animTimes[i].count() );
		}
	}
	else /* if (mode_ == Mode::Keyframe) */ {
		for (std::size_t i = 0u; i < framesBlended_.size(); ++i) {
			WeightedAnimFrame frames[] = {
				WeightedAnimFrame{ .frame = framesIdle[i], .w = tIdle_ },
				WeightedAnimFrame{ .frame = framesWalk[i], .w = tWalk_ }
			};
			framesBlended_[i] = sumWeightedAnimFrames(frames);
			if (framesAttack) framesBlended_[i] = lerpAnimFrames(framesBlended_[i], (*framesAttack)[i], tAttack_);
			framesBlended_[i] = lerpAnimFrames(framesBlended_[i], framesHit[i],    tHit_);
			framesBlended_[i] = lerpAnimFrames(framesBlended_[i], framesDeath[i],  tDeath_);
		}
		std::ranges::transform(framesBlended_, localXforms.begin(), convertAnimFrameToMatrix);
	}
}

void AnimBlenderSlime::EventBus::receive(const BasicEvent* event, Seconds deltaTime, EventList& evList, Timer& timer, void* pVoidOwner) {
	auto pOwner = static_cast<AnimBlenderSlime*>(pVoidOwner);
	switch (event->type) {
	case EventType::Hit:
		pOwner->animTimeHit_ = 0s;
		pOwner->cooldownHit_ = 600ms;
		break;
	case EventType::Death:
		pOwner->animTimeDeath_ = 0s;
		pOwner->cooldownDeath_ = 200ms;
		pOwner->dead_ = true;
		break;
	case EventType::Attack:
		if (!pOwner->attackClips_.empty()) {
			const auto idx = std::clamp<int>(
				static_cast<const EvAttack*>(event)->attackIndex, 0,
				static_cast<int>(pOwner->attackClips_.size()) - 1);
			pOwner->currentAttackClip_ = pOwner->attackClips_[idx];
			pOwner->animTimeAttack_ = 0s;
			pOwner->cooldownAttack_ = 3000ms;
		}
		break;
	case EventType::Respawn:
		pOwner->dead_          = false;
		pOwner->tDeath_        = 0.f;
		pOwner->animTimeDeath_ = 0s;
		pOwner->cooldownDeath_ = 0ms;
		break;
	default:
		break;
	}
}

void Slime::setAnimBlender(AnimSystem& animSystem, const AssetManager& assetManager) {
	auto blender = std::make_unique<AnimBlenderSlime>();
	blender->init(assetManager.modelSlime(), assetManager.slimeAnimations());
	animSystem.trackAnimBlender(blender.get());
	renderState_.animBlender = std::move(blender);
}

// Treant attackIndex mapping: 0=SpinKick, 1=Clap, 2=Punch
void AnimBlenderTreant::init(const Model* model, const std::vector<std::shared_ptr<AnimClip>>& anims) {
	setSkeleton(model->skeleton);
	framesBlended_.resize(skeleton().bones->size());
	for (auto& clip : anims)
		pushTargetClip(clip->name, clip);

	auto loaded = [&](const char* n) {
		for (auto& c : anims) if (c->name == n) return true;
		return false;
	};
	for (const char* name : { "Treant_SpinKick", "Treant_Clap", "Treant_Punch", "Treant_Attack" })
		if (loaded(name)) attackClips_.push_back(name);
	if (!attackClips_.empty()) currentAttackClip_ = attackClips_.front();
}

void AnimBlenderTreant::update(Seconds deltaTime, void* pVoidOwner) {
	auto pOwner = static_cast<Object*>(pVoidOwner);
	setOwnerPos(pOwner->pos());

	const auto walkThreshold = 0.06f;
	const auto speed = pOwner->velocity().len();
	const auto walkBlendRangeStart = walkThreshold - 0.03f;
	const auto walkBlendRangeEnd   = walkThreshold + 3.f;
	const auto targetTWalk = std::clamp( (speed - walkBlendRangeStart) / (walkBlendRangeEnd - walkBlendRangeStart), 0.f, 1.f );

	tWalk_ += (targetTWalk - tWalk_) * (1.f - std::exp(-deltaTime.count() / 0.12f));
	tIdle_ = 1.f - tWalk_;

	animTimeIdle_ += deltaTime;
	const auto durationIdle = targetClip("Treant_Idle")->duration;
	while (animTimeIdle_ > durationIdle) animTimeIdle_ -= durationIdle;

	if (tWalk_ > 0.f) {
		animTimeWalk_ += deltaTime;
		const auto durationWalk = targetClip("Treant_Walk")->duration;
		while (animTimeWalk_ > durationWalk) animTimeWalk_ -= durationWalk;
	}

	if (cooldownAttack_ > 0ms && !currentAttackClip_.empty()) {
		const auto durationAttack = targetClip(currentAttackClip_)->duration;
		animTimeAttack_ = std::min(durationAttack, animTimeAttack_ + deltaTime);
		tAttack_ = std::clamp( animTimeAttack_ / 100ms, 0.f, 1.f );
		cooldownAttack_ -= deltaTime;
	}
	else {
		animTimeAttack_ = 0s;
		tAttack_ = 0.f;
	}

	if (dead_) {
		animTimeDeath_ += deltaTime;
		if (cooldownDeath_ > 0ms)
			tDeath_ = 1.f - std::clamp( cooldownDeath_ / 300ms, 0.f, 1.f );
		else
			tDeath_ = 1.f;
		cooldownDeath_ -= deltaTime;
	}
	else if (cooldownHit_ > 0ms) {
		animTimeHit_ += deltaTime * 2.f;
		tHit_ = 0.75f * std::clamp( cooldownHit_ / 600ms, 0.f, 1.f );
		cooldownHit_ -= deltaTime;
	}
	else {
		animTimeHit_ = 0s;
		tHit_ = 0.f;
	}
}

void AnimBlenderTreant::onCalcLocal(PassKey<AnimSystem>) {
	const bool hasAttack = !currentAttackClip_.empty();

	updateFrames("Treant_Idle",   animTimeIdle_);
	updateFrames("Treant_Walk",   animTimeWalk_);
	if (hasAttack) updateFrames(currentAttackClip_, animTimeAttack_);
	updateFrames("Treant_Hit",    animTimeHit_);
	updateFrames("Treant_Death",  animTimeDeath_);

	auto& localXforms  = localXformData();
	auto& framesIdle   = curFrames("Treant_Idle");
	auto& framesWalk   = curFrames("Treant_Walk");
	auto* framesAttack = hasAttack ? &curFrames(currentAttackClip_) : nullptr;
	auto& framesHit    = curFrames("Treant_Hit");
	auto& framesDeath  = curFrames("Treant_Death");

	if (mode_ == Mode::Baked) {
		if (tDeath_ > 0.01f) {
			auto& deathClip = targetClip("Treant_Death");
			finalBakedClipId_    = deathClip->id;
			finalBakedClipFrame_ = static_cast<int>( deathClip->bakedSampleRate * animTimeDeath_.count() );
		}
		else if (hasAttack && tAttack_ > 0.01f) {
			auto& attackClip = targetClip(currentAttackClip_);
			finalBakedClipId_    = attackClip->id;
			finalBakedClipFrame_ = static_cast<int>( attackClip->bakedSampleRate * animTimeAttack_.count() );
		}
		else {
			float   weights[]   = { tIdle_, tWalk_ };
			Seconds animTimes[] = { animTimeIdle_, animTimeWalk_ };
			const AnimClip* clips[] = {
				targetClip("Treant_Idle").get(),
				targetClip("Treant_Walk").get()
			};
			auto i = static_cast<int>( std::distance(std::begin(weights), std::ranges::max_element(weights)) );
			finalBakedClipId_    = clips[i]->id;
			finalBakedClipFrame_ = static_cast<int>( clips[i]->bakedSampleRate * animTimes[i].count() );
		}
	}
	else /* if (mode_ == Mode::Keyframe) */ {
		for (std::size_t i = 0u; i < framesBlended_.size(); ++i) {
			WeightedAnimFrame frames[] = {
				WeightedAnimFrame{ .frame = framesIdle[i], .w = tIdle_ },
				WeightedAnimFrame{ .frame = framesWalk[i], .w = tWalk_ }
			};
			framesBlended_[i] = sumWeightedAnimFrames(frames);
			if (framesAttack) framesBlended_[i] = lerpAnimFrames(framesBlended_[i], (*framesAttack)[i], tAttack_);
			framesBlended_[i] = lerpAnimFrames(framesBlended_[i], framesHit[i],    tHit_);
			framesBlended_[i] = lerpAnimFrames(framesBlended_[i], framesDeath[i],  tDeath_);
		}
		std::ranges::transform(framesBlended_, localXforms.begin(), convertAnimFrameToMatrix);
	}
}

void AnimBlenderTreant::EventBus::receive(const BasicEvent* event, Seconds deltaTime, EventList& evList, Timer& timer, void* pVoidOwner) {
	auto pOwner = static_cast<AnimBlenderTreant*>(pVoidOwner);
	switch (event->type) {
	case EventType::Hit:
		pOwner->animTimeHit_ = 0s;
		pOwner->cooldownHit_ = 600ms;
		break;
	case EventType::Death:
		pOwner->animTimeDeath_ = 0s;
		pOwner->cooldownDeath_ = 200ms;
		pOwner->dead_ = true;
		break;
	case EventType::Attack:
		if (!pOwner->attackClips_.empty()) {
			const auto idx = std::clamp<int>(
				static_cast<const EvAttack*>(event)->attackIndex, 0,
				static_cast<int>(pOwner->attackClips_.size()) - 1);
			pOwner->currentAttackClip_ = pOwner->attackClips_[idx];
			pOwner->animTimeAttack_ = 0s;
			pOwner->cooldownAttack_ = 3000ms;
		}
		break;
	case EventType::Respawn:
		pOwner->dead_          = false;
		pOwner->tDeath_        = 0.f;
		pOwner->animTimeDeath_ = 0s;
		pOwner->cooldownDeath_ = 0ms;
		break;
	default:
		break;
	}
}

void Treant::setAnimBlender(AnimSystem& animSystem, const AssetManager& assetManager) {
	auto blender = std::make_unique<AnimBlenderTreant>();
	blender->init(assetManager.modelTreant(), assetManager.treantAnimations());
	animSystem.trackAnimBlender(blender.get());
	renderState_.animBlender = std::move(blender);
}

// Grandbaum: Treant와 동일 리그/클립을 재사용하되 전용 modelGrandbaum로 굽는다(Hobgoblin↔Goblin과 동일).
void Grandbaum::setAnimBlender(AnimSystem& animSystem, const AssetManager& assetManager) {
	auto blender = std::make_unique<AnimBlenderTreant>();
	blender->init(assetManager.modelGrandbaum(), assetManager.treantAnimations());
	animSystem.trackAnimBlender(blender.get());
	renderState_.animBlender = std::move(blender);
}

// Isys: Birdy와 동일 리그/클립을 재사용하되 전용 modelIsys로 굽는다.
void Isys::setAnimBlender(AnimSystem& animSystem, const AssetManager& assetManager) {
	auto blender = std::make_unique<AnimBlenderBirdy>();
	blender->init(assetManager.modelIsys(), assetManager.birdyAnimations());
	animSystem.trackAnimBlender(blender.get());
	renderState_.animBlender = std::move(blender);
}

// 거점 이벤트 처리: 고블린 핸들러에서 AnimBlender 포워딩·래그돌만 제거한 형태.
// HP/사망 상태만 갱신한다(서버 권위; death 판정은 서버가 수행).
void Stronghold::EventBus::receive(const BasicEvent* event, Seconds deltaTime, EventList& evList, Timer& timer, void* pVoidOwner) {
	auto pOwner = static_cast<Stronghold*>(pVoidOwner);
	if (!pOwner) return;
	switch (event->type) {
	case EventType::Hit:
		pOwner->hp_ = std::max( static_cast<const EvHit*>(event)->hp, 0 );
		break;

	case EventType::Death:
		if (!pOwner->isDead_) {
			pOwner->isDead_ = true;
			pOwner->hp_ = 0;
		}
		break;

	case EventType::Respawn:
		pOwner->isDead_ = false;
		break;

	default:
		break;
	}
}

void Stronghold::render(GFX& gfx, mu::Mat4x4 /*offsetXform*/) {
	for (auto& [mesh, meshXform] : renderState_.pModel->meshWithDressXforms) {
		for ( auto& submesh : mesh.subMeshes ) { 
			gfx.addOccluder( PBRDeferredPipeline::OccluderInfo{
				.mesh = &mesh, .subMesh = &submesh,
				.world = meshXform * renderState_.world 
			} );
		}
	}
	Object::render(gfx);
}

void TerrainObject::render(GFX& gfx, mu::Mat4x4 /*offsetXform*/) {
	if (!terrainData_ || terrainData_->mesh.subMeshes.empty()) return;

	if (renderState_.willOcclude && gfx.renderPath() == GFX::RenderPath::Deferred) {
		gfx.addOccluder(TerrainDeferredPipeline::OccluderInfo{
			.terrain = terrainData_,
			.world   = renderState_.world
		});
	}

	if (gfx.renderPath() == GFX::RenderPath::Deferred) {
		gfx.addDrawEvent(TerrainDeferredPipeline::DrawEvent{
			.terrain = terrainData_,
			.world   = renderState_.world,
			.shadowCulled = isShadowCulled()
		});
	} else {
		gfx.addDrawEvent(TerrainPipeline::DrawEvent{
			.terrain = terrainData_,
			.world   = renderState_.world,
			.shadowCulled = isShadowCulled()
		});
	}
}

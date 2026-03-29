#include "pch.hpp"
#include "object.hpp"
#include "terrainPipeline.hpp"
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
	
	// 객체의 속력이 runThreshold를 넘는지를 기준으로
	// run 애니메이션이 필요한지 idle 애니메이션이 필요한지 판단한다.
	// runThreshold를 부드럽게 감싸는 blendRange를 설정하여
	// 객체의 속력이 blendRange 내부에 있다면 0과 1 사이의 tRun 값이 구해진다.
	// run 애니메이션의 가중치는 tRun, idle 애니메이션의 가중치는 1 - tRun이 된다.
	const auto runThreshold = 0.1f;

	// 객체의 속력 구하기
	const auto speed = pOwner->physicState().evVelocity.len();

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
		const auto blendSpaceX = mu::dot(pOwner->physicState().evVelocity, pOwner->right());
		const auto blendSpaceY = mu::dot(pOwner->physicState().evVelocity, pOwner->forward());
	
		// blend space 좌표를 바탕으로 각 애니메이션의 가중치를 정한다.
		const auto wForward = std::max(0.f, blendSpaceY);
		const auto wBackward = std::max(0.f, -blendSpaceY);
		const auto wLeft = std::max(0.f, -blendSpaceX);
		const auto wRight = std::max(0.f, blendSpaceX);

		// 가중치의 총합이 1이 되게끔 한다.
		float total = wForward + wBackward + wLeft + wRight;

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

	priority_ = 0.f;
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
		pOwner->animTimeHit_ = 0s;
		pOwner->cooldownHit_ = 600ms;
		break;

	case EventType::Death:
		pOwner->animTimeDeath_ = 0s;
		pOwner->cooldownDeath_ = 200ms;
		pOwner->dead_ = true;
		break;

	default:
		break;
	}
}

void AnimBlenderGoblin::init(const AssetManager& assetManager) {
	setSkeleton(assetManager.modelGoblin()->skeleton);
	framesBlended_.resize(skeleton().bones->size());
	for (auto& clip : assetManager.goblinAnimations()) {
		pushTargetClip(clip->name, clip);
	}
}

// pOwner의 물리 정보에 따라
// 애니메이션 블렌딩 상태를 갱신한다.
void AnimBlenderGoblin::update(Seconds deltaTime, void* pVoidOwner) {
	auto pOwner = static_cast<Object*>(pVoidOwner);

	// 객체의 속력이 walkThreshold를 넘는지를 기준으로
	// walk 애니메이션이 필요한지 idle 애니메이션이 필요한지 판단한다.
	// walkThreshold를 부드럽게 감싸는 blendRange를 설정하여
	// 객체의 속력이 blendRange 내부에 있다면 0과 1 사이의 tWalk 값이 구해진다.
	// walk 애니메이션의 가중치는 tWalk, idle 애니메이션의 가중치는 1 - tWalk이 된다.
	const auto walkThreshold = 0.06f;

	// 객체의 속력 구하기
	const auto speed = pOwner->physicState().evVelocity.len();

	// blendRange 설정
	const auto walkBlendRangeStart = walkThreshold - 0.03f;
	const auto walkBlendRangeEnd = walkThreshold + 3.f;
	// tWalk 구하기
	tWalk_ = std::clamp( (speed - walkBlendRangeStart) / (walkBlendRangeEnd - walkBlendRangeStart), 0.f, 1.f );

	// tIdle 구하기
	tIdle_ = 1.f - tWalk_;

	// Idle 애니메이션들은 그냥 계속 돌린다.
	// 딱히 멈추지 않아도 부자연스럽진 않다.
	animTimeIdle_ += deltaTime;
	const auto durationIdle = targetClip("Goblin_Idle")->duration;
	while (animTimeIdle_ > durationIdle) {
		animTimeIdle_ -= durationIdle;
	}

	if (cooldownAttack_ > 0ms) {
		const auto durationAttack = targetClip("Goblin_Attack")->duration;
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

	priority_ = 0.f;
}

void AnimBlenderGoblin::onCalcLocal(PassKey<AnimSystem>) {
	// update에서 구한 애니메이션 가중치들로 블렌딩을 수행한다.

	// 개별 애니메이션의 프레임을 업데이트한다.
	updateFrames("Goblin_Idle", animTimeIdle_);
	updateFrames("Goblin_Walk", animTimeWalk_);
	updateFrames("Goblin_Attack", animTimeAttack_);
	updateFrames("Goblin_Hit", animTimeHit_);
	updateFrames("Goblin_Death", animTimeDeath_);

	auto& localXforms = localXformData();
	auto& framesIdle = curFrames("Goblin_Idle");
	auto& framesWalk = curFrames("Goblin_Walk");
	auto& framesAttack = curFrames("Goblin_Attack");
	auto& framesHit = curFrames("Goblin_Hit");
	auto& framesDeath = curFrames("Goblin_Death");

	// 애니메이션의 프레임들을 블렌딩한다.
	for (std::size_t i = 0u; i < framesBlended_.size(); ++i) {
		WeightedAnimFrame frames[] = {
			WeightedAnimFrame{ .frame = framesIdle[i], .w = tIdle_ },
			WeightedAnimFrame{ .frame = framesWalk[i], .w = tWalk_ }
		};
		framesBlended_[i] = sumWeightedAnimFrames(frames);
		// attack animation 보간
		framesBlended_[i] = lerpAnimFrames(framesBlended_[i], framesAttack[i], tAttack_);
		// hit animation 보간 (nlerp 쓰면 팔꿈치 꼬임)
		framesBlended_[i] = lerpAnimFrames(framesBlended_[i], framesHit[i], tHit_);
		// death animation 보간
		framesBlended_[i] = lerpAnimFrames(framesBlended_[i], framesDeath[i], tDeath_);
	}
	std::ranges::transform(framesBlended_, localXforms.begin(), convertAnimFrameToMatrix);
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
		pOwner->animTimeAttack_ = 0s;
		pOwner->cooldownAttack_ = 3000ms;
		break;

	default:
		break;
	}
}

void AnimBlenderAnubis::init(const AssetManager& assetManager) {
	setSkeleton(assetManager.modelAnubis()->skeleton);
	framesBlended_.resize(skeleton().bones->size());
	for (auto& clip : assetManager.anubisAnimations()) {
		pushTargetClip(clip->name, clip);
	}
}

// pOwner의 물리 정보에 따라
// 애니메이션 블렌딩 상태를 갱신한다.
void AnimBlenderAnubis::update(Seconds deltaTime, void* pVoidOwner) {
	auto pOwner = static_cast<Object*>(pVoidOwner);

	// 객체의 속력이 walkThreshold를 넘는지를 기준으로
	// walk 애니메이션이 필요한지 idle 애니메이션이 필요한지 판단한다.
	// walkThreshold를 부드럽게 감싸는 blendRange를 설정하여
	// 객체의 속력이 blendRange 내부에 있다면 0과 1 사이의 tWalk 값이 구해진다.
	// walk 애니메이션의 가중치는 tWalk, idle 애니메이션의 가중치는 1 - tWalk이 된다.
	const auto walkThreshold = 0.06f;

	// 객체의 속력 구하기
	const auto speed = pOwner->physicState().evVelocity.len();

	// blendRange 설정
	const auto walkBlendRangeStart = walkThreshold - 0.03f;
	const auto walkBlendRangeEnd = walkThreshold + 3.f;
	// tWalk 구하기
	tWalk_ = std::clamp( (speed - walkBlendRangeStart) / (walkBlendRangeEnd - walkBlendRangeStart), 0.f, 1.f );

	// tIdle 구하기
	tIdle_ = 1.f - tWalk_;

	// Idle 애니메이션들은 그냥 계속 돌린다.
	// 딱히 멈추지 않아도 부자연스럽진 않다.
	animTimeIdle_ += deltaTime;
	const auto durationIdle = targetClip("Anubis_Idle")->duration;
	while (animTimeIdle_ > durationIdle) {
		animTimeIdle_ -= durationIdle;
	}

	if (cooldownAttack_ > 0ms) {
		const auto durationAttack = targetClip("Anubis_Attack")->duration;
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

	priority_ = 0.f;
}

void AnimBlenderAnubis::onCalcLocal(PassKey<AnimSystem>) {
	// update에서 구한 애니메이션 가중치들로 블렌딩을 수행한다.

	// 개별 애니메이션의 프레임을 업데이트한다.
	updateFrames("Anubis_Idle", animTimeIdle_);
	updateFrames("Anubis_Walk", animTimeWalk_);
	updateFrames("Anubis_Attack", animTimeAttack_);
	updateFrames("Anubis_Hit", animTimeHit_);
	updateFrames("Anubis_Death", animTimeDeath_);

	auto& localXforms = localXformData();
	auto& framesIdle = curFrames("Anubis_Idle");
	auto& framesWalk = curFrames("Anubis_Walk");
	auto& framesAttack = curFrames("Anubis_Attack");
	auto& framesHit = curFrames("Anubis_Hit");
	auto& framesDeath = curFrames("Anubis_Death");

	// 애니메이션의 프레임들을 블렌딩한다.
	for (std::size_t i = 0u; i < framesBlended_.size(); ++i) {
		WeightedAnimFrame frames[] = {
			WeightedAnimFrame{ .frame = framesIdle[i], .w = tIdle_ },
			WeightedAnimFrame{ .frame = framesWalk[i], .w = tWalk_ }
		};
		framesBlended_[i] = sumWeightedAnimFrames(frames);
		// attack animation 보간
		framesBlended_[i] = lerpAnimFrames(framesBlended_[i], framesAttack[i], tAttack_);
		// hit animation 보간 (nlerp 쓰면 팔꿈치 꼬임)
		framesBlended_[i] = lerpAnimFrames(framesBlended_[i], framesHit[i], tHit_);
		// death animation 보간
		framesBlended_[i] = lerpAnimFrames(framesBlended_[i], framesDeath[i], tDeath_);
	}
	std::ranges::transform(framesBlended_, localXforms.begin(), convertAnimFrameToMatrix);
}

void AnimBlenderAnubis::EventBus::receive(const BasicEvent* event, Seconds deltaTime, EventList& evList, Timer& timer, void* pVoidOwner) {
	auto pOwner = static_cast<AnimBlenderAnubis*>(pVoidOwner);

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
		pOwner->animTimeAttack_ = 0s;
		pOwner->cooldownAttack_ = 3000ms;
		break;

	default:
		break;
	}
}

void AnimBlenderBat::init(const AssetManager& assetManager) {
	setSkeleton(assetManager.modelBat()->skeleton);
	framesBlended_.resize(skeleton().bones->size());
	for (auto& clip : assetManager.batAnimations()) {
		pushTargetClip(clip->name, clip);
	}
}

// pOwner의 물리 정보에 따라
// 애니메이션 블렌딩 상태를 갱신한다.
void AnimBlenderBat::update(Seconds deltaTime, void* pVoidOwner) {
	auto pOwner = static_cast<Object*>(pVoidOwner);

	// 객체의 속력이 walkThreshold를 넘는지를 기준으로
	// walk 애니메이션이 필요한지 idle 애니메이션이 필요한지 판단한다.
	// walkThreshold를 부드럽게 감싸는 blendRange를 설정하여
	// 객체의 속력이 blendRange 내부에 있다면 0과 1 사이의 tWalk 값이 구해진다.
	// walk 애니메이션의 가중치는 tWalk, idle 애니메이션의 가중치는 1 - tWalk이 된다.
	const auto walkThreshold = 0.06f;

	// 객체의 속력 구하기
	const auto speed = pOwner->physicState().evVelocity.len();

	// blendRange 설정
	const auto walkBlendRangeStart = walkThreshold - 0.03f;
	const auto walkBlendRangeEnd = walkThreshold + 3.f;
	// tWalk 구하기
	tWalk_ = std::clamp( (speed - walkBlendRangeStart) / (walkBlendRangeEnd - walkBlendRangeStart), 0.f, 1.f );

	// tIdle 구하기
	tIdle_ = 1.f - tWalk_;

	// Idle 애니메이션들은 그냥 계속 돌린다.
	// 딱히 멈추지 않아도 부자연스럽진 않다.
	animTimeIdle_ += deltaTime;
	const auto durationIdle = targetClip("Bat_Idle")->duration;
	while (animTimeIdle_ > durationIdle) {
		animTimeIdle_ -= durationIdle;
	}

	if (cooldownAttack_ > 0ms) {
		const auto durationAttack = targetClip("Bat_Attack")->duration;
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

	priority_ = 0.f;
}

void AnimBlenderBat::onCalcLocal(PassKey<AnimSystem>) {
	// update에서 구한 애니메이션 가중치들로 블렌딩을 수행한다.

	// 개별 애니메이션의 프레임을 업데이트한다.
	updateFrames("Bat_Idle", animTimeIdle_);
	updateFrames("Bat_Fly", animTimeWalk_);
	updateFrames("Bat_Attack", animTimeAttack_);
	updateFrames("Bat_Hit", animTimeHit_);
	updateFrames("Bat_Death", animTimeDeath_);

	auto& localXforms = localXformData();
	auto& framesIdle = curFrames("Bat_Idle");
	auto& framesWalk = curFrames("Bat_Fly");
	auto& framesAttack = curFrames("Bat_Attack");
	auto& framesHit = curFrames("Bat_Hit");
	auto& framesDeath = curFrames("Bat_Death");

	// 애니메이션의 프레임들을 블렌딩한다.
	for (std::size_t i = 0u; i < framesBlended_.size(); ++i) {
		WeightedAnimFrame frames[] = {
			WeightedAnimFrame{ .frame = framesIdle[i], .w = tIdle_ },
			WeightedAnimFrame{ .frame = framesWalk[i], .w = tWalk_ }
		};
		framesBlended_[i] = sumWeightedAnimFrames(frames);
		// attack animation 보간
		framesBlended_[i] = lerpAnimFrames(framesBlended_[i], framesAttack[i], tAttack_);
		// hit animation 보간 (nlerp 쓰면 팔꿈치 꼬임)
		framesBlended_[i] = lerpAnimFrames(framesBlended_[i], framesHit[i], tHit_);
		// death animation 보간
		framesBlended_[i] = lerpAnimFrames(framesBlended_[i], framesDeath[i], tDeath_);
	}
	std::ranges::transform(framesBlended_, localXforms.begin(), convertAnimFrameToMatrix);
}

void AnimBlenderBat::EventBus::receive(const BasicEvent* event, Seconds deltaTime, EventList& evList, Timer& timer, void* pVoidOwner) {
	auto pOwner = static_cast<AnimBlenderBat*>(pVoidOwner);

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
		pOwner->animTimeAttack_ = 0s;
		pOwner->cooldownAttack_ = 3000ms;
		break;

	default:
		break;
	}
}

void AnimBlenderBomber::init(const AssetManager& assetManager) {
	setSkeleton(assetManager.modelBomber()->skeleton);
	framesBlended_.resize(skeleton().bones->size());
	for (auto& clip : assetManager.bomberAnimations()) {
		pushTargetClip(clip->name, clip);
	}
}

// pOwner의 물리 정보에 따라
// 애니메이션 블렌딩 상태를 갱신한다.
void AnimBlenderBomber::update(Seconds deltaTime, void* pVoidOwner) {
	auto pOwner = static_cast<Object*>(pVoidOwner);

	// 객체의 속력이 walkThreshold를 넘는지를 기준으로
	// walk 애니메이션이 필요한지 idle 애니메이션이 필요한지 판단한다.
	// walkThreshold를 부드럽게 감싸는 blendRange를 설정하여
	// 객체의 속력이 blendRange 내부에 있다면 0과 1 사이의 tWalk 값이 구해진다.
	// walk 애니메이션의 가중치는 tWalk, idle 애니메이션의 가중치는 1 - tWalk이 된다.
	const auto walkThreshold = 0.06f;

	// 객체의 속력 구하기
	const auto speed = pOwner->physicState().evVelocity.len();

	// blendRange 설정
	const auto walkBlendRangeStart = walkThreshold - 0.03f;
	const auto walkBlendRangeEnd = walkThreshold + 3.f;
	// tWalk 구하기
	tWalk_ = std::clamp( (speed - walkBlendRangeStart) / (walkBlendRangeEnd - walkBlendRangeStart), 0.f, 1.f );

	// tIdle 구하기
	tIdle_ = 1.f - tWalk_;

	// Idle 애니메이션들은 그냥 계속 돌린다.
	// 딱히 멈추지 않아도 부자연스럽진 않다.
	animTimeIdle_ += deltaTime;
	const auto durationIdle = targetClip("Bomber_Idle")->duration;
	while (animTimeIdle_ > durationIdle) {
		animTimeIdle_ -= durationIdle;
	}

	if (cooldownAttack_ > 0ms) {
		const auto durationAttack = targetClip("Bomber_Attack")->duration;
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

	priority_ = 0.f;
}

void AnimBlenderBomber::onCalcLocal(PassKey<AnimSystem>) {
	// update에서 구한 애니메이션 가중치들로 블렌딩을 수행한다.

	// 개별 애니메이션의 프레임을 업데이트한다.
	updateFrames("Bomber_Idle", animTimeIdle_);
	updateFrames("Bomber_Fly", animTimeWalk_);
	updateFrames("Bomber_Attack", animTimeAttack_);
	updateFrames("Bomber_Hit", animTimeHit_);
	updateFrames("Bomber_Death", animTimeDeath_);

	auto& localXforms = localXformData();
	auto& framesIdle = curFrames("Bomber_Idle");
	auto& framesWalk = curFrames("Bomber_Fly");
	auto& framesAttack = curFrames("Bomber_Attack");
	auto& framesHit = curFrames("Bomber_Hit");
	auto& framesDeath = curFrames("Bomber_Death");

	// 애니메이션의 프레임들을 블렌딩한다.
	for (std::size_t i = 0u; i < framesBlended_.size(); ++i) {
		WeightedAnimFrame frames[] = {
			WeightedAnimFrame{ .frame = framesIdle[i], .w = tIdle_ },
			WeightedAnimFrame{ .frame = framesWalk[i], .w = tWalk_ }
		};
		framesBlended_[i] = sumWeightedAnimFrames(frames);
		// attack animation 보간
		framesBlended_[i] = lerpAnimFrames(framesBlended_[i], framesAttack[i], tAttack_);
		// hit animation 보간 (nlerp 쓰면 팔꿈치 꼬임)
		framesBlended_[i] = lerpAnimFrames(framesBlended_[i], framesHit[i], tHit_);
		// death animation 보간
		framesBlended_[i] = lerpAnimFrames(framesBlended_[i], framesDeath[i], tDeath_);
	}
	std::ranges::transform(framesBlended_, localXforms.begin(), convertAnimFrameToMatrix);
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
		pOwner->animTimeAttack_ = 0s;
		pOwner->cooldownAttack_ = 3000ms;
		break;

	default:
		break;
	}
}

void AnimBlenderDemon::init(const AssetManager& assetManager) {
	setSkeleton(assetManager.modelDemon()->skeleton);
	framesBlended_.resize(skeleton().bones->size());
	for (auto& clip : assetManager.demonAnimations()) {
		pushTargetClip(clip->name, clip);
	}
}

// pOwner의 물리 정보에 따라
// 애니메이션 블렌딩 상태를 갱신한다.
void AnimBlenderDemon::update(Seconds deltaTime, void* pVoidOwner) {
	auto pOwner = static_cast<Object*>(pVoidOwner);

	// 객체의 속력이 walkThreshold를 넘는지를 기준으로
	// walk 애니메이션이 필요한지 idle 애니메이션이 필요한지 판단한다.
	// walkThreshold를 부드럽게 감싸는 blendRange를 설정하여
	// 객체의 속력이 blendRange 내부에 있다면 0과 1 사이의 tWalk 값이 구해진다.
	// walk 애니메이션의 가중치는 tWalk, idle 애니메이션의 가중치는 1 - tWalk이 된다.
	const auto walkThreshold = 0.06f;

	// 객체의 속력 구하기
	const auto speed = pOwner->physicState().evVelocity.len();

	// blendRange 설정
	const auto walkBlendRangeStart = walkThreshold - 0.03f;
	const auto walkBlendRangeEnd = walkThreshold + 3.f;
	// tWalk 구하기
	tWalk_ = std::clamp( (speed - walkBlendRangeStart) / (walkBlendRangeEnd - walkBlendRangeStart), 0.f, 1.f );

	// tIdle 구하기
	tIdle_ = 1.f - tWalk_;

	// Idle 애니메이션들은 그냥 계속 돌린다.
	// 딱히 멈추지 않아도 부자연스럽진 않다.
	animTimeIdle_ += deltaTime;
	const auto durationIdle = targetClip("Demon_Idle")->duration;
	while (animTimeIdle_ > durationIdle) {
		animTimeIdle_ -= durationIdle;
	}

	if (cooldownAttack_ > 0ms) {
		const auto durationAttack = targetClip("Demon_Attack")->duration;
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

	priority_ = 0.f;
}

void AnimBlenderDemon::onCalcLocal(PassKey<AnimSystem>) {
	// update에서 구한 애니메이션 가중치들로 블렌딩을 수행한다.

	// 개별 애니메이션의 프레임을 업데이트한다.
	updateFrames("Demon_Idle", animTimeIdle_);
	updateFrames("Demon_Walk", animTimeWalk_);
	updateFrames("Demon_Attack", animTimeAttack_);
	updateFrames("Demon_Hit", animTimeHit_);
	updateFrames("Demon_Death", animTimeDeath_);

	auto& localXforms = localXformData();
	auto& framesIdle = curFrames("Demon_Idle");
	auto& framesWalk = curFrames("Demon_Walk");
	auto& framesAttack = curFrames("Demon_Attack");
	auto& framesHit = curFrames("Demon_Hit");
	auto& framesDeath = curFrames("Demon_Death");

	// 애니메이션의 프레임들을 블렌딩한다.
	for (std::size_t i = 0u; i < framesBlended_.size(); ++i) {
		WeightedAnimFrame frames[] = {
			WeightedAnimFrame{ .frame = framesIdle[i], .w = tIdle_ },
			WeightedAnimFrame{ .frame = framesWalk[i], .w = tWalk_ }
		};
		framesBlended_[i] = sumWeightedAnimFrames(frames);
		// attack animation 보간
		framesBlended_[i] = lerpAnimFrames(framesBlended_[i], framesAttack[i], tAttack_);
		// hit animation 보간 (nlerp 쓰면 팔꿈치 꼬임)
		framesBlended_[i] = lerpAnimFrames(framesBlended_[i], framesHit[i], tHit_);
		// death animation 보간
		framesBlended_[i] = lerpAnimFrames(framesBlended_[i], framesDeath[i], tDeath_);
	}
	std::ranges::transform(framesBlended_, localXforms.begin(), convertAnimFrameToMatrix);
}

void AnimBlenderDemon::EventBus::receive(const BasicEvent* event, Seconds deltaTime, EventList& evList, Timer& timer, void* pVoidOwner) {
	auto pOwner = static_cast<AnimBlenderDemon*>(pVoidOwner);

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
		pOwner->animTimeAttack_ = 0s;
		pOwner->cooldownAttack_ = 3000ms;
		break;

	default:
		break;
	}
}

void AnimBlenderDragon::init(const AssetManager& assetManager) {
	setSkeleton(assetManager.modelDragon()->skeleton);
	framesBlended_.resize(skeleton().bones->size());
	for (auto& clip : assetManager.dragonAnimations()) {
		pushTargetClip(clip->name, clip);
	}
}

// pOwner의 물리 정보에 따라
// 애니메이션 블렌딩 상태를 갱신한다.
void AnimBlenderDragon::update(Seconds deltaTime, void* pVoidOwner) {
	auto pOwner = static_cast<Object*>(pVoidOwner);

	// 객체의 속력이 walkThreshold를 넘는지를 기준으로
	// walk 애니메이션이 필요한지 idle 애니메이션이 필요한지 판단한다.
	// walkThreshold를 부드럽게 감싸는 blendRange를 설정하여
	// 객체의 속력이 blendRange 내부에 있다면 0과 1 사이의 tWalk 값이 구해진다.
	// walk 애니메이션의 가중치는 tWalk, idle 애니메이션의 가중치는 1 - tWalk이 된다.
	const auto walkThreshold = 0.06f;

	// 객체의 속력 구하기
	const auto speed = pOwner->physicState().evVelocity.len();

	// blendRange 설정
	const auto walkBlendRangeStart = walkThreshold - 0.03f;
	const auto walkBlendRangeEnd = walkThreshold + 3.f;
	// tWalk 구하기
	tWalk_ = std::clamp( (speed - walkBlendRangeStart) / (walkBlendRangeEnd - walkBlendRangeStart), 0.f, 1.f );

	// tIdle 구하기
	tIdle_ = 1.f - tWalk_;

	// Idle 애니메이션들은 그냥 계속 돌린다.
	// 딱히 멈추지 않아도 부자연스럽진 않다.
	animTimeIdle_ += deltaTime;
	const auto durationIdle = targetClip("Dragon_Idle")->duration;
	while (animTimeIdle_ > durationIdle) {
		animTimeIdle_ -= durationIdle;
	}

	if (cooldownAttack_ > 0ms) {
		const auto durationAttack = targetClip("Dragon_Attack")->duration;
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

	priority_ = 0.f;
}

void AnimBlenderDragon::onCalcLocal(PassKey<AnimSystem>) {
	// update에서 구한 애니메이션 가중치들로 블렌딩을 수행한다.

	// 개별 애니메이션의 프레임을 업데이트한다.
	updateFrames("Dragon_Idle", animTimeIdle_);
	updateFrames("Dragon_Walk", animTimeWalk_);
	updateFrames("Dragon_Attack", animTimeAttack_);
	updateFrames("Dragon_Hit", animTimeHit_);
	updateFrames("Dragon_Death", animTimeDeath_);

	auto& localXforms = localXformData();
	auto& framesIdle = curFrames("Dragon_Idle");
	auto& framesWalk = curFrames("Dragon_Walk");
	auto& framesAttack = curFrames("Dragon_Attack");
	auto& framesHit = curFrames("Dragon_Hit");
	auto& framesDeath = curFrames("Dragon_Death");

	// 애니메이션의 프레임들을 블렌딩한다.
	for (std::size_t i = 0u; i < framesBlended_.size(); ++i) {
		WeightedAnimFrame frames[] = {
			WeightedAnimFrame{ .frame = framesIdle[i], .w = tIdle_ },
			WeightedAnimFrame{ .frame = framesWalk[i], .w = tWalk_ }
		};
		framesBlended_[i] = sumWeightedAnimFrames(frames);
		// attack animation 보간
		framesBlended_[i] = lerpAnimFrames(framesBlended_[i], framesAttack[i], tAttack_);
		// hit animation 보간 (nlerp 쓰면 팔꿈치 꼬임)
		framesBlended_[i] = lerpAnimFrames(framesBlended_[i], framesHit[i], tHit_);
		// death animation 보간
		framesBlended_[i] = lerpAnimFrames(framesBlended_[i], framesDeath[i], tDeath_);
	}
	std::ranges::transform(framesBlended_, localXforms.begin(), convertAnimFrameToMatrix);
}

void AnimBlenderDragon::EventBus::receive(const BasicEvent* event, Seconds deltaTime, EventList& evList, Timer& timer, void* pVoidOwner) {
	auto pOwner = static_cast<AnimBlenderDragon*>(pVoidOwner);

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
		pOwner->animTimeAttack_ = 0s;
		pOwner->cooldownAttack_ = 3000ms;
		break;

	default:
		break;
	}
}

void AnimBlenderEyeball::init(const AssetManager& assetManager) {
	setSkeleton(assetManager.modelEyeball()->skeleton);
	framesBlended_.resize(skeleton().bones->size());
	for (auto& clip : assetManager.eyeballAnimations()) {
		pushTargetClip(clip->name, clip);
	}
}

// pOwner의 물리 정보에 따라
// 애니메이션 블렌딩 상태를 갱신한다.
void AnimBlenderEyeball::update(Seconds deltaTime, void* pVoidOwner) {
	auto pOwner = static_cast<Object*>(pVoidOwner);

	// 객체의 속력이 walkThreshold를 넘는지를 기준으로
	// walk 애니메이션이 필요한지 idle 애니메이션이 필요한지 판단한다.
	// walkThreshold를 부드럽게 감싸는 blendRange를 설정하여
	// 객체의 속력이 blendRange 내부에 있다면 0과 1 사이의 tWalk 값이 구해진다.
	// walk 애니메이션의 가중치는 tWalk, idle 애니메이션의 가중치는 1 - tWalk이 된다.
	const auto walkThreshold = 0.06f;

	// 객체의 속력 구하기
	const auto speed = pOwner->physicState().evVelocity.len();

	// blendRange 설정
	const auto walkBlendRangeStart = walkThreshold - 0.03f;
	const auto walkBlendRangeEnd = walkThreshold + 3.f;
	// tWalk 구하기
	tWalk_ = std::clamp( (speed - walkBlendRangeStart) / (walkBlendRangeEnd - walkBlendRangeStart), 0.f, 1.f );

	// tIdle 구하기
	tIdle_ = 1.f - tWalk_;

	// Idle 애니메이션들은 그냥 계속 돌린다.
	// 딱히 멈추지 않아도 부자연스럽진 않다.
	animTimeIdle_ += deltaTime;
	const auto durationIdle = targetClip("Eyeball_Idle")->duration;
	while (animTimeIdle_ > durationIdle) {
		animTimeIdle_ -= durationIdle;
	}

	if (cooldownAttack_ > 0ms) {
		const auto durationAttack = targetClip("Eyeball_Attack")->duration;
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

	priority_ = 0.f;
}

void AnimBlenderEyeball::onCalcLocal(PassKey<AnimSystem>) {
	// update에서 구한 애니메이션 가중치들로 블렌딩을 수행한다.

	// 개별 애니메이션의 프레임을 업데이트한다.
	updateFrames("Eyeball_Idle", animTimeIdle_);
	updateFrames("Eyeball_Walk", animTimeWalk_);
	updateFrames("Eyeball_Attack", animTimeAttack_);
	updateFrames("Eyeball_Hit", animTimeHit_);
	updateFrames("Eyeball_Death", animTimeDeath_);

	auto& localXforms = localXformData();
	auto& framesIdle = curFrames("Eyeball_Idle");
	auto& framesWalk = curFrames("Eyeball_Walk");
	auto& framesAttack = curFrames("Eyeball_Attack");
	auto& framesHit = curFrames("Eyeball_Hit");
	auto& framesDeath = curFrames("Eyeball_Death");

	// 애니메이션의 프레임들을 블렌딩한다.
	for (std::size_t i = 0u; i < framesBlended_.size(); ++i) {
		WeightedAnimFrame frames[] = {
			WeightedAnimFrame{ .frame = framesIdle[i], .w = tIdle_ },
			WeightedAnimFrame{ .frame = framesWalk[i], .w = tWalk_ }
		};
		framesBlended_[i] = sumWeightedAnimFrames(frames);
		// attack animation 보간
		framesBlended_[i] = lerpAnimFrames(framesBlended_[i], framesAttack[i], tAttack_);
		// hit animation 보간 (nlerp 쓰면 팔꿈치 꼬임)
		framesBlended_[i] = lerpAnimFrames(framesBlended_[i], framesHit[i], tHit_);
		// death animation 보간
		framesBlended_[i] = lerpAnimFrames(framesBlended_[i], framesDeath[i], tDeath_);
	}
	std::ranges::transform(framesBlended_, localXforms.begin(), convertAnimFrameToMatrix);
}

void AnimBlenderEyeball::EventBus::receive(const BasicEvent* event, Seconds deltaTime, EventList& evList, Timer& timer, void* pVoidOwner) {
	auto pOwner = static_cast<AnimBlenderEyeball*>(pVoidOwner);

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
		pOwner->animTimeAttack_ = 0s;
		pOwner->cooldownAttack_ = 3000ms;
		break;

	default:
		break;
	}
}

void AnimBlenderFishman::init(const AssetManager& assetManager) {
	setSkeleton(assetManager.modelFishman()->skeleton);
	framesBlended_.resize(skeleton().bones->size());
	for (auto& clip : assetManager.fishmanAnimations()) {
		pushTargetClip(clip->name, clip);
	}
}

// pOwner의 물리 정보에 따라
// 애니메이션 블렌딩 상태를 갱신한다.
void AnimBlenderFishman::update(Seconds deltaTime, void* pVoidOwner) {
	auto pOwner = static_cast<Object*>(pVoidOwner);

	// 객체의 속력이 walkThreshold를 넘는지를 기준으로
	// walk 애니메이션이 필요한지 idle 애니메이션이 필요한지 판단한다.
	// walkThreshold를 부드럽게 감싸는 blendRange를 설정하여
	// 객체의 속력이 blendRange 내부에 있다면 0과 1 사이의 tWalk 값이 구해진다.
	// walk 애니메이션의 가중치는 tWalk, idle 애니메이션의 가중치는 1 - tWalk이 된다.
	const auto walkThreshold = 0.06f;

	// 객체의 속력 구하기
	const auto speed = pOwner->physicState().evVelocity.len();

	// blendRange 설정
	const auto walkBlendRangeStart = walkThreshold - 0.03f;
	const auto walkBlendRangeEnd = walkThreshold + 3.f;
	// tWalk 구하기
	tWalk_ = std::clamp( (speed - walkBlendRangeStart) / (walkBlendRangeEnd - walkBlendRangeStart), 0.f, 1.f );

	// tIdle 구하기
	tIdle_ = 1.f - tWalk_;

	// Idle 애니메이션들은 그냥 계속 돌린다.
	// 딱히 멈추지 않아도 부자연스럽진 않다.
	animTimeIdle_ += deltaTime;
	const auto durationIdle = targetClip("Fishman_Idle")->duration;
	while (animTimeIdle_ > durationIdle) {
		animTimeIdle_ -= durationIdle;
	}

	if (cooldownAttack_ > 0ms) {
		const auto durationAttack = targetClip("Fishman_Attack")->duration;
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

	priority_ = 0.f;
}

void AnimBlenderFishman::onCalcLocal(PassKey<AnimSystem>) {
	// update에서 구한 애니메이션 가중치들로 블렌딩을 수행한다.

	// 개별 애니메이션의 프레임을 업데이트한다.
	updateFrames("Fishman_Idle", animTimeIdle_);
	updateFrames("Fishman_Walk", animTimeWalk_);
	updateFrames("Fishman_Attack", animTimeAttack_);
	updateFrames("Fishman_Hit", animTimeHit_);
	updateFrames("Fishman_Death", animTimeDeath_);

	auto& localXforms = localXformData();
	auto& framesIdle = curFrames("Fishman_Idle");
	auto& framesWalk = curFrames("Fishman_Walk");
	auto& framesAttack = curFrames("Fishman_Attack");
	auto& framesHit = curFrames("Fishman_Hit");
	auto& framesDeath = curFrames("Fishman_Death");

	// 애니메이션의 프레임들을 블렌딩한다.
	for (std::size_t i = 0u; i < framesBlended_.size(); ++i) {
		WeightedAnimFrame frames[] = {
			WeightedAnimFrame{ .frame = framesIdle[i], .w = tIdle_ },
			WeightedAnimFrame{ .frame = framesWalk[i], .w = tWalk_ }
		};
		framesBlended_[i] = sumWeightedAnimFrames(frames);
		// attack animation 보간
		framesBlended_[i] = lerpAnimFrames(framesBlended_[i], framesAttack[i], tAttack_);
		// hit animation 보간 (nlerp 쓰면 팔꿈치 꼬임)
		framesBlended_[i] = lerpAnimFrames(framesBlended_[i], framesHit[i], tHit_);
		// death animation 보간
		framesBlended_[i] = lerpAnimFrames(framesBlended_[i], framesDeath[i], tDeath_);
	}
	std::ranges::transform(framesBlended_, localXforms.begin(), convertAnimFrameToMatrix);
}

void AnimBlenderFishman::EventBus::receive(const BasicEvent* event, Seconds deltaTime, EventList& evList, Timer& timer, void* pVoidOwner) {
	auto pOwner = static_cast<AnimBlenderFishman*>(pVoidOwner);

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
		pOwner->animTimeAttack_ = 0s;
		pOwner->cooldownAttack_ = 3000ms;
		break;

	default:
		break;
	}
}

void AnimBlenderGargoyle::init(const AssetManager& assetManager) {
	setSkeleton(assetManager.modelGargoyle()->skeleton);
	framesBlended_.resize(skeleton().bones->size());
	for (auto& clip : assetManager.gargoyleAnimations()) {
		pushTargetClip(clip->name, clip);
	}
}

// pOwner의 물리 정보에 따라
// 애니메이션 블렌딩 상태를 갱신한다.
void AnimBlenderGargoyle::update(Seconds deltaTime, void* pVoidOwner) {
	auto pOwner = static_cast<Object*>(pVoidOwner);

	// 객체의 속력이 walkThreshold를 넘는지를 기준으로
	// walk 애니메이션이 필요한지 idle 애니메이션이 필요한지 판단한다.
	// walkThreshold를 부드럽게 감싸는 blendRange를 설정하여
	// 객체의 속력이 blendRange 내부에 있다면 0과 1 사이의 tWalk 값이 구해진다.
	// walk 애니메이션의 가중치는 tWalk, idle 애니메이션의 가중치는 1 - tWalk이 된다.
	const auto walkThreshold = 0.06f;

	// 객체의 속력 구하기
	const auto speed = pOwner->physicState().evVelocity.len();

	// blendRange 설정
	const auto walkBlendRangeStart = walkThreshold - 0.03f;
	const auto walkBlendRangeEnd = walkThreshold + 3.f;
	// tWalk 구하기
	tWalk_ = std::clamp( (speed - walkBlendRangeStart) / (walkBlendRangeEnd - walkBlendRangeStart), 0.f, 1.f );

	// tIdle 구하기
	tIdle_ = 1.f - tWalk_;

	// Idle 애니메이션들은 그냥 계속 돌린다.
	// 딱히 멈추지 않아도 부자연스럽진 않다.
	animTimeIdle_ += deltaTime;
	const auto durationIdle = targetClip("Gargoyle_Idle")->duration;
	while (animTimeIdle_ > durationIdle) {
		animTimeIdle_ -= durationIdle;
	}

	if (cooldownAttack_ > 0ms) {
		const auto durationAttack = targetClip("Gargoyle_Attack")->duration;
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

	priority_ = 0.f;
}

void AnimBlenderGargoyle::onCalcLocal(PassKey<AnimSystem>) {
	// update에서 구한 애니메이션 가중치들로 블렌딩을 수행한다.

	// 개별 애니메이션의 프레임을 업데이트한다.
	updateFrames("Gargoyle_Idle", animTimeIdle_);
	updateFrames("Gargoyle_Walk", animTimeWalk_);
	updateFrames("Gargoyle_Attack", animTimeAttack_);
	updateFrames("Gargoyle_Hit", animTimeHit_);
	updateFrames("Gargoyle_Death", animTimeDeath_);

	auto& localXforms = localXformData();
	auto& framesIdle = curFrames("Gargoyle_Idle");
	auto& framesWalk = curFrames("Gargoyle_Walk");
	auto& framesAttack = curFrames("Gargoyle_Attack");
	auto& framesHit = curFrames("Gargoyle_Hit");
	auto& framesDeath = curFrames("Gargoyle_Death");

	// 애니메이션의 프레임들을 블렌딩한다.
	for (std::size_t i = 0u; i < framesBlended_.size(); ++i) {
		WeightedAnimFrame frames[] = {
			WeightedAnimFrame{ .frame = framesIdle[i], .w = tIdle_ },
			WeightedAnimFrame{ .frame = framesWalk[i], .w = tWalk_ }
		};
		framesBlended_[i] = sumWeightedAnimFrames(frames);
		// attack animation 보간
		framesBlended_[i] = lerpAnimFrames(framesBlended_[i], framesAttack[i], tAttack_);
		// hit animation 보간 (nlerp 쓰면 팔꿈치 꼬임)
		framesBlended_[i] = lerpAnimFrames(framesBlended_[i], framesHit[i], tHit_);
		// death animation 보간
		framesBlended_[i] = lerpAnimFrames(framesBlended_[i], framesDeath[i], tDeath_);
	}
	std::ranges::transform(framesBlended_, localXforms.begin(), convertAnimFrameToMatrix);
}

void AnimBlenderGargoyle::EventBus::receive(const BasicEvent* event, Seconds deltaTime, EventList& evList, Timer& timer, void* pVoidOwner) {
	auto pOwner = static_cast<AnimBlenderGargoyle*>(pVoidOwner);

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
		pOwner->animTimeAttack_ = 0s;
		pOwner->cooldownAttack_ = 3000ms;
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
void Object::setModel(const Model* pModel){
	DISPLAY_ERROR_STR(pModel != nullptr, "[Game Error] Object::setModel: 널 모델이 전달되었습니다.", false);
	if (pModel == nullptr) {
		return;
	}

	renderState_.pModel = pModel;
	rebuildBVH(currPhysicState_);
	prevPhysicState_.bvh = currPhysicState_.bvh;
	renderState_.worldBVs.resize(currPhysicState_.bvh.nodes.size());
}

// @brief 게임 객체의 RenderState와 방향 벡터들을 갱신한다.
//		RenderState는 이전 PhysicState와 현재 PhysicState를 보간하여 얻어지고,
//      방향 벡터들은 현재 PhysicState의 내용으로 계산한다.
// @param deltaTime 마지막 프레임으로부터 경과한 시간
// @param tPhysicInterpolation 이전 PhysicState와 현재 PhysicState의 보간 비율
//		(게임 객체가 계산해서 일괄적으로 전달해야 한다.)
void Object::update(Milliseconds deltaTime, float tPhysicInterpolation) {
	const auto& prev = prevPhysicState_;
	const auto& curr = currPhysicState_;
	const auto t = tPhysicInterpolation;

	// 방향 벡터 갱신
	right_ = curr.orient.rotate(mu::Vec3(1.f, 0.f, 0.f));
	up_ = curr.orient.rotate(mu::Vec3(0.f, 1.f, 0.f));
	forward_ = curr.orient.rotate(mu::Vec3(0.f, 0.f, 1.f));
	
	// 렌더 상태 갱신
	auto& pos = renderState_.pos;
	pos = mu::lerp(prev.pos, curr.pos, t);
	auto& orient = renderState_.orient;
	orient = mu::slerp(prev.orient, curr.orient, t);	// 쿼터니언
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
					const mu::NQuat wo = lo * mu::NQuat{ mu::Quat{ mu::quatRotMat(boneToWorld.get()) } };
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

void MU_CALLCONV Object::render(GFX& gfx, mu::Mat4x4 offsetXform) {
	const auto pModel = renderState_.pModel;
	if (pModel) {
		for (auto& [mesh, meshXform] : pModel->meshWithDressXforms) {
			const bool isSkinned = renderState_.animBlender
				&& mesh.vbIdxMap.contains(mesh.name + "_VB_BoneIndices");

			for (std::size_t i = 0u; i < mesh.subMeshes.size(); ++i) {
				if (isSkinned) {
					gfx.addDrawEvent(PBRSkinnedPipeline::DrawEvent{
						.world = meshXform * offsetXform * renderState_.world,
						.boneXforms = renderState_.animBlender->finalXformData(),
						.mesh = &mesh,
						.subMesh = &mesh.subMeshes[i],
						.material = &mesh.materialSets[materialSetIdx_].materials[i],
					});
				}
				else {
					gfx.addDrawEvent(PBRPipeline::DrawEvent{
						.world = meshXform * offsetXform * renderState_.world,
						.mesh = &mesh,
						.subMesh = &mesh.subMeshes[i],
						.material = &mesh.materialSets[materialSetIdx_].materials[i],
					});
				}
			}
		}
	}

	if (willRenderBV_ && pModel && !pModel->bvh.empty()) {
		for (std::size_t i = 0u; i < pModel->bvh.nodes.size(); ++i) {
			gfx.addDrawEvent( BVPipeline::DrawEvent{
				.world   = offsetXform * renderState_.worldBVs[i],
				.bvModel = BVPipeline::BVModel::Box
			} );
		}
	}

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
			equipment.object->render( gfx,
				equipment.object->renderState_.pModel->socketOffsets.at(equipment.socketType)
				* skeleton.bones->at( skeleton.socketToBoneIdx.at(equipment.socketType) ).toDress
				* renderState_.animBlender->finalXformData()[ skeleton.socketToBoneIdx.at(equipment.socketType) ]
				* offsetXform * renderState_.world
			);
		}
	}
}

// 게임 객체의 위치를 갱신한다.
// 이전 PhysicState와 현재 PhysicState의 위치가 모두 갱신된다.
// 각 PhysicState의 충돌체(volumes) 역시 갱신된다.
void MU_CALLCONV Object::setPos(mu::Vec3 newPos) {
	prevPhysicState_.pos = newPos;
	currPhysicState_.pos = newPos;

	const auto pModel = renderState_.pModel;

	if (pModel && !pModel->bvh.empty()) {
		rebuildBVH(currPhysicState_);
		prevPhysicState_.bvh = currPhysicState_.bvh;
	}
}

// 게임 객체의 위치를 갱신한다.
// 현재 PhysicState의 위치만 갱신된다.
void MU_CALLCONV Object::setCurrPos(mu::Vec3 newPos) {
	currPhysicState_.pos = newPos;

	const auto pModel = renderState_.pModel;

	if (pModel && !pModel->bvh.empty()) {
		rebuildBVH(currPhysicState_);
	}
}

// 게임 객체의 속도를 갱신한다.
// 이전 PhysicState와 현재 PhysicState의 속도가 모두 갱신된다.
void MU_CALLCONV Object::setVelocity(mu::Vec3 newVelocity) {
	prevPhysicState_.velocity = newVelocity;
	currPhysicState_.velocity = newVelocity;
}

// 게임 객체의 각속도를 갱신한다.
// 이전 PhysicState와 현재 PhysicState의 각속도가 모두 갱신된다.
void MU_CALLCONV Object::setOmega(mu::Vec3 newOmega) {
	prevPhysicState_.omega = newOmega;
	currPhysicState_.omega = newOmega;
}

// 게임 객체의 방향을 갱신한다.
// 이전 PhysicState와 현재 PhysicState의 방향이 모두 갱신된다.
// 게임 객체의 방향 벡터들도 전부 갱신된다.
void MU_CALLCONV Object::setOrient(mu::NQuat newOrient) {
	prevPhysicState_.orient = newOrient;
	currPhysicState_.orient = newOrient;
	right_   = currPhysicState_.orient.rotate(mu::Vec3(1.f, 0.f, 0.f));
	up_      = currPhysicState_.orient.rotate(mu::Vec3(0.f, 1.f, 0.f));
	forward_ = currPhysicState_.orient.rotate(mu::Vec3(0.f, 0.f, 1.f));

	// BVH nodes with OBB shapes track orientation and must be rebuilt.
	const auto pModel = renderState_.pModel;
	if (pModel && !pModel->bvh.empty()) {
		rebuildBVH(currPhysicState_);
		prevPhysicState_.bvh = currPhysicState_.bvh;
	}
}

// 게임 객체의 크기를 갱신한다.
// 이전 PhysicState와 현재 PhysicState의 크기가 모두 갱신된다.
// 각 PhysicState의 AABB 역시 갱신된다.
void MU_CALLCONV Object::setScale(mu::Vec3 newScale) {
	prevPhysicState_.scale = newScale;
	currPhysicState_.scale = newScale;

	const auto pModel = renderState_.pModel;

	if (pModel && !pModel->bvh.empty()) {
		rebuildBVH(currPhysicState_);
		prevPhysicState_.bvh = currPhysicState_.bvh;
	}
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
		toRemove.object->prevPhysicState_ = prevPhysicState_;
		toRemove.object->currPhysicState_ = currPhysicState_;
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
	const auto pModel = renderState_.pModel;
	if (!pModel || pModel->bvh.empty()) return;

	const auto& localBVH = pModel->bvh;
	const auto& skeleton = pModel->skeleton;
	const bool  hasBones = renderState_.animBlender
	                    && skeleton.bones && !skeleton.bones->empty();

	// Object world matrix (same formula as renderState_.world, physics-state based)
	const mu::Mat4x4 objWorld = mu::Mat4x4(mu::scale(state.scale))
	                          * mu::Mat4x4(state.orient)
	                          * mu::translate(state.pos);

	state.bvh.nodes.resize(localBVH.nodes.size());
	for (std::size_t i = 0; i < localBVH.nodes.size(); ++i) {
		const auto& src = localBVH.nodes[i];
		auto&       dst = state.bvh.nodes[i];

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
				const mu::Vec3  worldHalfExtents = localHalfExtents * state.scale;
				// Extract rotation from boneToWorld and compose with local orient
				const mu::NQuat boneWorldOrient{ mu::Quat{ mu::quatRotMat(boneToWorld.get()) } };
				const mu::NQuat worldOrient      = localOrient * boneWorldOrient;
				return OBB{ worldCenter, worldHalfExtents, worldOrient };
			}, src.shape);
		} else {
			dst.shape = std::visit([&](auto&& s) -> std::variant<AABB, OBB> {
				using T = std::decay_t<decltype(s)>;
				if constexpr (std::is_same_v<T, AABB>) {
					return AABB{
						s.center * state.scale + state.pos,
						s.size   * state.scale,
					};
				} else {
					mu::NQuat worldOrient = state.orient;
					worldOrient *= s.orient;
					return OBB{
						state.orient.rotate(s.center * state.scale) + state.pos,
						s.halfExtents * state.scale,
						worldOrient,
					};
				}
			}, src.shape);
		}

		dst.bounds = std::visit([](auto&& s) -> AABB {
			using T = std::decay_t<decltype(s)>;
			if constexpr (std::is_same_v<T, OBB>) return obbToAABB(s);
			else                                   return s;
		}, dst.shape);
	}
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
		if (pOwner) {
			if (pOwner->renderState_.animBlender) {
				pOwner->renderState_.animBlender->eventBus()->receive(
					event, deltaTime, evList, timer,
					pOwner->renderState_.animBlender.get()
				);
			}
			pOwner->hp_ = 0;
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
	
	default:
		break;
	}
}

void Anubis::EventBus::receive(const BasicEvent* event, Seconds deltaTime, EventList& evList, Timer& timer, void* pVoidOwner) {
	auto pOwner = static_cast<Anubis*>(pVoidOwner);
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
		if (pOwner) {
			if (pOwner->renderState_.animBlender) {
				pOwner->renderState_.animBlender->eventBus()->receive(
					event, deltaTime, evList, timer,
					pOwner->renderState_.animBlender.get()
				);
			}
			pOwner->hp_ = 0;
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
	
	default:
		break;
	}
}

void Bat::EventBus::receive(const BasicEvent* event, Seconds deltaTime, EventList& evList, Timer& timer, void* pVoidOwner) {
	auto pOwner = static_cast<Bat*>(pVoidOwner);
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
		if (pOwner) {
			if (pOwner->renderState_.animBlender) {
				pOwner->renderState_.animBlender->eventBus()->receive(
					event, deltaTime, evList, timer,
					pOwner->renderState_.animBlender.get()
				);
			}
			pOwner->hp_ = 0;
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
	
	default:
		break;
	}
}

void Bomber::EventBus::receive(const BasicEvent* event, Seconds deltaTime, EventList& evList, Timer& timer, void* pVoidOwner) {
	auto pOwner = static_cast<Bomber*>(pVoidOwner);
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
		if (pOwner) {
			if (pOwner->renderState_.animBlender) {
				pOwner->renderState_.animBlender->eventBus()->receive(
					event, deltaTime, evList, timer,
					pOwner->renderState_.animBlender.get()
				);
			}
			pOwner->hp_ = 0;
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
	
	default:
		break;
	}
}

void Demon::EventBus::receive(const BasicEvent* event, Seconds deltaTime, EventList& evList, Timer& timer, void* pVoidOwner) {
	auto pOwner = static_cast<Demon*>(pVoidOwner);
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
		if (pOwner) {
			if (pOwner->renderState_.animBlender) {
				pOwner->renderState_.animBlender->eventBus()->receive(
					event, deltaTime, evList, timer,
					pOwner->renderState_.animBlender.get()
				);
			}
			pOwner->hp_ = 0;
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
	
	default:
		break;
	}
}

void Dragon::EventBus::receive(const BasicEvent* event, Seconds deltaTime, EventList& evList, Timer& timer, void* pVoidOwner) {
	auto pOwner = static_cast<Dragon*>(pVoidOwner);
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
		if (pOwner) {
			if (pOwner->renderState_.animBlender) {
				pOwner->renderState_.animBlender->eventBus()->receive(
					event, deltaTime, evList, timer,
					pOwner->renderState_.animBlender.get()
				);
			}
			pOwner->hp_ = 0;
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
	
	default:
		break;
	}
}

void Eyeball::EventBus::receive(const BasicEvent* event, Seconds deltaTime, EventList& evList, Timer& timer, void* pVoidOwner) {
	auto pOwner = static_cast<Eyeball*>(pVoidOwner);
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
		if (pOwner) {
			if (pOwner->renderState_.animBlender) {
				pOwner->renderState_.animBlender->eventBus()->receive(
					event, deltaTime, evList, timer,
					pOwner->renderState_.animBlender.get()
				);
			}
			pOwner->hp_ = 0;
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
	
	default:
		break;
	}
}

void Fishman::EventBus::receive(const BasicEvent* event, Seconds deltaTime, EventList& evList, Timer& timer, void* pVoidOwner) {
	auto pOwner = static_cast<Fishman*>(pVoidOwner);
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
		if (pOwner) {
			if (pOwner->renderState_.animBlender) {
				pOwner->renderState_.animBlender->eventBus()->receive(
					event, deltaTime, evList, timer,
					pOwner->renderState_.animBlender.get()
				);
			}
			pOwner->hp_ = 0;
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
	
	default:
		break;
	}
}

void Gargoyle::EventBus::receive(const BasicEvent* event, Seconds deltaTime, EventList& evList, Timer& timer, void* pVoidOwner) {
	auto pOwner = static_cast<Gargoyle*>(pVoidOwner);
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
		if (pOwner) {
			if (pOwner->renderState_.animBlender) {
				pOwner->renderState_.animBlender->eventBus()->receive(
					event, deltaTime, evList, timer,
					pOwner->renderState_.animBlender.get()
				);
			}
			pOwner->hp_ = 0;
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
	
	default:
		break;
	}
}

void TerrainObject::render(GFX& gfx, mu::Mat4x4 /*offsetXform*/) {
	if (!terrainData_ || terrainData_->mesh.subMeshes.empty()) return;
	gfx.addDrawEvent(TerrainPipeline::DrawEvent{
		.terrain = terrainData_,
		.world   = renderState_.world
	});
}

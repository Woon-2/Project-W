#include "pch.hpp"
#include "object.hpp"
#include "errorHandling.hpp"
#include "AssetManager.hpp"
#include "Timer.hpp"

void AnimBlenderVanguard::init(const AssetManager& assetManager) {
	setSkeleton(assetManager.modelPlayer()->skeleton);
	framesBlended_.resize(skeleton().bones->size());
	for (auto& clip : assetManager.vanguardAnimations()) {
		pushTargetClip(clip->name, clip);
	}
}

// pOwner의 물리 정보에 따라
// 애니메이션 블렌딩 상태를 갱신한다.
void AnimBlenderVanguard::update(Seconds deltaTime, void* pVoidOwner) {
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

	// tIdle 구하기 (tIdle_ 및 tIdleAim_)
	// 움직이지 않은 채 aimlessThreshold 시간이 지났다면 비조준 Idle 애니메이션을,
	// 그렇지 않으면 조준 Idle 애니메이션을 재생하도록 한다.
	const auto aimlessThreshold = 2s;
	const auto aimBlendRangeStart = aimlessThreshold - 100ms;
	const auto aimBlendRangeEnd = aimlessThreshold + 400ms;

	const auto tIdleBase = 1.f - tRun;
	// cooldownFire_이 0보다 크다는 건 발사 이벤트(EvFire)로 인해
	// 비조준 idle -> 조준 idle 애니메이션 전환이 일어났다는 말이다.
	// cooldownFire_이 0에 가까울수록 조준 idle 애니메이션의 비율이 더 높아진다.
	if (cooldownFire_ > 0ms) {
		cooldownFire_ -= deltaTime;
		tIdle_ = tIdleBase * std::clamp( cooldownFire_ / 120ms, 0.f, 1.f );
		tIdleAim_ = tIdleBase - tIdle_;
	}
	else {
		tIdle_ = tIdleBase * std::clamp( (accMotionless_ - aimBlendRangeStart) / (aimBlendRangeEnd - aimBlendRangeStart), 0.f, 1.f );
		tIdleAim_ = tIdleBase - tIdle_;
	}

	// Idle 애니메이션들은 그냥 계속 돌린다.
	// 딱히 멈추지 않아도 부자연스럽진 않다.
	animTimeIdle_ += deltaTime;
	const auto durationIdle = targetClip("Vanguard_Idle")->duration;
	while (animTimeIdle_ > durationIdle) {
		animTimeIdle_ -= durationIdle;
	}
	animTimeIdleAim_ += deltaTime;
	const auto durationIdleAim = targetClip("Vanguard_Idle_Aim")->duration;
	while (animTimeIdleAim_ > durationIdle) {
		animTimeIdleAim_ -= durationIdle;
	}

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
		// 1.5배속 재생
		animTimeDeath_ += deltaTime * 1.5f;

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
		// 3배속 재생
		animTimeHit_ += deltaTime * 3.f;
		
		tHit_ = 0.75f * std::clamp( cooldownHit_ / 600ms, 0.f, 1.f );

		cooldownHit_ -= deltaTime;
	}
	else {
		animTimeHit_ = 0s;
		tHit_ = 0.f;
	}

	// 4방향 run 애니메이션들은 재생 시간이 비슷하다.
	// Run_Forward 애니메이션의 duration을 대표로 사용해도 부자연스럽지 않다.
	const auto durationRun = targetClip("Vanguard_Run_Forward")->duration;
	while (animTimeRun_ > durationRun) {
		animTimeRun_ -= durationRun;
	}

	priority_ = 0.f;
}

void AnimBlenderVanguard::onCalcLocal(PassKey<AnimSystem>) {
	// update에서 구한 애니메이션 가중치들로 블렌딩을 수행한다.

	// 개별 애니메이션의 프레임을 업데이트한다.
	updateFrames("Vanguard_Idle", animTimeIdle_);
	updateFrames("Vanguard_Idle_Aim", animTimeIdle_);
	updateFrames("Vanguard_Hit", animTimeHit_);
	updateFrames("Vanguard_Death", animTimeDeath_);
	updateFrames("Vanguard_Run_Forward", animTimeRun_);
	updateFrames("Vanguard_Run_Backward", animTimeRun_);
	updateFrames("Vanguard_Run_Left", animTimeRun_);
	updateFrames("Vanguard_Run_Right", animTimeRun_);

	auto& localXforms = localXformData();
	auto& framesIdle = curFrames("Vanguard_Idle");
	auto& framesIdleAim = curFrames("Vanguard_Idle_Aim");
	auto& framesHit = curFrames("Vanguard_Hit");
	auto& framesDeath = curFrames("Vanguard_Death");
	auto& framesRunForward = curFrames("Vanguard_Run_Forward");
	auto& framesRunBackward = curFrames("Vanguard_Run_Backward");
	auto& framesRunLeft = curFrames("Vanguard_Run_Left");
	auto& framesRunRight = curFrames("Vanguard_Run_Right");

	// 애니메이션의 프레임들을 블렌딩한다.
	for (std::size_t i = 0u; i < framesBlended_.size(); ++i) {
		WeightedAnimFrame frames[] = {
			WeightedAnimFrame{ .frame = framesIdle[i], .w = tIdle_ },
			WeightedAnimFrame{ .frame = framesIdleAim[i], .w = tIdleAim_ },
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

void AnimBlenderVanguard::EventBus::receive(const BasicEvent* event, Seconds deltaTime, EventList& evList, Timer& timer, void* pVoidOwner) {
	auto pOwner = static_cast<AnimBlenderVanguard*>(pVoidOwner);

	switch (event->type) {
	case EventType::Fire:
		// 조준 여부와 관계없이 발사 시
		// 움직임이 없었던 시간을 누산하는 accMotionless_를 0으로 만든다.
		pOwner->accMotionless_ = 0s;
		// 비조준 상태였다면, 조준한 후 사격해야 하므로
		// 발사까지 딜레이가 있다.
		if (pOwner->tIdle_ > 0.1f) {
			pOwner->cooldownFire_ = 120ms;
			timer.enqueueJob( DelayedJob{
				.job = [&evList](){ holdEvent(evList, EvMuzzleFlash{}); },
				.executeAt = timer.lastTp() + 120ms
			} );
		}
		// 조준 상태였다면, 딜레이 없이 바로 사격한다.
		else {
			holdEvent(evList, EvMuzzleFlash{});
		}
		break;

	case EventType::Hit:
		pOwner->animTimeHit_ = 0s;
		pOwner->cooldownHit_ = 600ms;
		holdEvent(evList, EvBlood{});
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
	currPhysicState_.aabbs.resize(pModel->aabbs.size());
	for (std::size_t i = 0u; i < currPhysicState_.aabbs.size(); ++i) {
		currPhysicState_.aabbs[i].center
			= pModel->aabbs[i].center * currPhysicState_.scale
			+ currPhysicState_.pos;
		currPhysicState_.aabbs[i].size
			= pModel->aabbs[i].size * currPhysicState_.scale;
	}
	prevPhysicState_.aabbs = currPhysicState_.aabbs;
	renderState_.worldBVs.resize(currPhysicState_.aabbs.size());
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
	for (std::size_t i = 0; i < currPhysicState_.aabbs.size(); ++i) {
		const auto center = pModel->aabbs[i].center * scale + pos;
		const auto size = pModel->aabbs[i].size * scale;
		renderState_.worldBVs[i] = mu::Mat4x4(mu::scale(size)) * mu::translate(center);
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
		if (renderState_.animBlender) {
			for (auto& [mesh, meshXform] : pModel->meshWithDressXforms) {
				for (std::size_t i = 0u; i < mesh.subMeshes.size(); ++i) {
					gfx.addDrawEvent(PBRSkinnedPipeline::DrawEvent{
						.world = meshXform * offsetXform * renderState_.world,
						.boneXforms = renderState_.animBlender->finalXformData(),
						.mesh = &mesh,
						.subMesh = &mesh.subMeshes[i],
						.material = &mesh.materialSets[materialSetIdx_].materials[i],
					});
				}
			}
		}
		else {
			for (auto& [mesh, meshXform] : pModel->meshWithDressXforms) {
				for (std::size_t i = 0u; i < mesh.subMeshes.size(); ++i) {
					gfx.addDrawEvent(PBRPipeline::DrawEvent{
						.world = meshXform * offsetXform * renderState_.world,
						.mesh = &mesh,
						.subMesh = &mesh.subMeshes[i],
						.material = &mesh.materialSets[materialSetIdx_].materials[i]
					});
				}
			}
		}
	}

	if (willRenderBV_) {
		for (std::size_t i = 0u; i < currPhysicState_.aabbs.size(); ++i) {
			gfx.addDrawEvent( BVPipeline::DrawEvent{
				.world = offsetXform * renderState_.worldBVs[i],
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
// 각 PhysicState의 AABB 역시 갱신된다.
void MU_CALLCONV Object::setPos(mu::Vec3 newPos) {
	prevPhysicState_.pos = newPos;
	currPhysicState_.pos = newPos;

	const auto pModel = renderState_.pModel;

	if (pModel) {
		currPhysicState_.aabbs.resize(pModel->aabbs.size());
		for (std::size_t i = 0u; i < currPhysicState_.aabbs.size(); ++i) {
			currPhysicState_.aabbs[i].center
				= pModel->aabbs[i].center * currPhysicState_.scale
				+ currPhysicState_.pos;
			currPhysicState_.aabbs[i].size
				= pModel->aabbs[i].size * currPhysicState_.scale;
		}
		prevPhysicState_.aabbs = currPhysicState_.aabbs;
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
	right_ = currPhysicState_.orient.rotate(mu::Vec3(1.f, 0.f, 0.f));
	up_ = currPhysicState_.orient.rotate(mu::Vec3(0.f, 1.f, 0.f));
	forward_ = currPhysicState_.orient.rotate(mu::Vec3(0.f, 0.f, 1.f));
}

// 게임 객체의 크기를 갱신한다.
// 이전 PhysicState와 현재 PhysicState의 크기가 모두 갱신된다.
// 각 PhysicState의 AABB 역시 갱신된다.
void MU_CALLCONV Object::setScale(mu::Vec3 newScale) {
	prevPhysicState_.scale = newScale;
	currPhysicState_.scale = newScale;

	const auto pModel = renderState_.pModel;

	if (pModel) {
		currPhysicState_.aabbs.resize(pModel->aabbs.size());
		for (std::size_t i = 0u; i < currPhysicState_.aabbs.size(); ++i) {
			currPhysicState_.aabbs[i].center
				= pModel->aabbs[i].center * currPhysicState_.scale
				+ currPhysicState_.pos;
			currPhysicState_.aabbs[i].size
				= pModel->aabbs[i].size * currPhysicState_.scale;
		}
		prevPhysicState_.aabbs = currPhysicState_.aabbs;
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

void Object::EventBus::receive(const BasicEvent* event, Seconds deltaTime, EventList& evList, Timer& timer, void* pVoidOwner) {
	auto pOwner = static_cast<Object*>(pVoidOwner);
	switch (event->type) {
	case EventType::Fire:
		if (pOwner && pOwner->renderState_.animBlender) {
			pOwner->renderState_.animBlender->eventBus()->receive(
				event, deltaTime, evList, timer,
				pOwner->renderState_.animBlender.get()
			);
			pOwner->ammo_ = static_cast<const EvFire*>(event)->bulletCount;
		}
		break;

	case EventType::Hit:
		if (pOwner) {
			if (pOwner->renderState_.animBlender) {
				pOwner->renderState_.animBlender->eventBus()->receive(
					event, deltaTime, evList, timer,
					pOwner->renderState_.animBlender.get()
				);
			}
			pOwner->hp_ = std::max( static_cast<const EvHit*>(event)->hp, 0 );
			if (pOwner->hp_ == 0) {
				holdEvent( evList, EvDeath(pOwner->getId()) );
			}
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
	
	default:
		break;
	}
}
#include "pch.hpp"
#include "animation.hpp"
#include "errorHandling.hpp"

mu::Mat4x4 MU_CALLCONV convertAnimFrameToMatrix(AnimFrame frame) {
	return mu::Mat4x4(mu::scale(frame.scale)) * mu::Mat4x4(frame.rotation)
		* mu::translate(frame.translation);
}

AnimFrame MU_CALLCONV lerpAnimFrames(AnimFrame lhs, AnimFrame rhs, float t) {
	AnimFrame ret{};
	ret.translation = mu::lerp(lhs.translation, rhs.translation, t);
	ret.rotation = mu::slerp(lhs.rotation, rhs.rotation, t);
	ret.scale = mu::lerp(lhs.scale, rhs.scale, t);
	return ret;
}

void AnimBlender::pushTargetClip(
	const std::string& key, const std::shared_ptr<const AnimClip>& pTargetClip
) {
	DISPLAY_ERROR_STR( skeleton_, "[Animation Error] AnimBlender::pushTargetClip: "s
		+ "AnimBlender 객체에 스켈레톤이 할당되어 있지 않습니다.",
		false
	);

	DISPLAY_ERROR_STR( skeleton_->skeletonEnumeration == pTargetClip->skeletonEnumeration,
		"[Animation Error] AnimBlender::pushTargetClip: 스켈레톤과 애니메이션 클립이 호환되지 않습니다.",
		false
	);

	DISPLAY_ERROR_STR( !frameInfoMap_.contains(key),
		"[Animation Error] AnimBlender::pushTargetClip: key \""s + key
		+ "\"이(가) 이미 AnimBlender에 등록되어 있었습니다.",
		false
	);


	if ( !skeleton_
		|| skeleton_->skeletonEnumeration != pTargetClip->skeletonEnumeration
		|| frameInfoMap_.contains(key)
	) {
		return;
	}

	auto [pPair, _] = frameInfoMap_.try_emplace(key);
	auto& frameInfo = pPair->second;

	frameInfo.targetClip = pTargetClip;
	frameInfo.frameCache_.resize(skeleton_->bones->size());
	frameInfo.keyFrameIteratorCache_.resize(skeleton_->bones->size());
}

void AnimBlender::popTargetClip(const std::string& key) {
	frameInfoMap_.erase(key);
}

void AnimBlender::updateFrames(const std::string& key, Seconds elapsed) {
	DISPLAY_ERROR_STR( frameInfoMap_.contains(key),
		"[Animation Error] AnimBlender::updateFrames: key \""s + key
		+ "\"이(가) AnimBlender에 등록되어 있지 않습니다.",
		false
	);

	if (!frameInfoMap_.contains(key)) {
		return;
	}

	auto& frameInfo = frameInfoMap_.at(key);

	for (std::size_t i = 0; i < frameInfo.frameCache_.size(); ++i) {
		auto itCurKeyFrame = frameInfo.keyFrameIteratorCache_[i];
		auto itNextKeyFrame = std::next(itCurKeyFrame);

		while (itNextKeyFrame->time > elapsed) {
			itCurKeyFrame = itNextKeyFrame;
			itNextKeyFrame = std::next(itCurKeyFrame);
		}

		frameInfo.keyFrameIteratorCache_[i] = itCurKeyFrame;

		frameInfo.frameCache_[i] = lerpAnimFrames( *itCurKeyFrame, *itNextKeyFrame,
			(elapsed - itCurKeyFrame->time) / (itNextKeyFrame->time - itCurKeyFrame->time)	
		);
	}
}

void AnimBlender::setSkeleton(const std::shared_ptr<Skeleton>& pSkeleton) {
	DISPLAY_ERROR_STR( skeleton_, "[Animation Error] AnimBlender::setSkeleton: "s
		+ "전달된 스켈레톤 포인터가 널 포인터입니다.",
		false
	);

	skeleton_ = pSkeleton;
	boneXformCache_.resize(skeleton_->bones->size());
}

void AnimBlender::onCalcWorld(PassKey<AnimSystem>) {
	traverseBone(*skeleton_->pRoot);
}

void MU_CALLCONV AnimBlender::traverseBone(const Bone& bone, mu::Mat4x4 parentXform) {
	auto& xform = boneXformCache_[bone.boneIdx];
	xform *= parentXform;

	for (const auto* pChild: bone.children) {
		traverseBone(*pChild, xform);
	}
}

void AnimBlender::onCalcFinal(PassKey<AnimSystem>) {
	auto& bones = *skeleton_->bones;
	for (std::size_t i = 0; i < bones.size(); ++i) {
		boneXformCache_[i] = bones[i].toLocal * boneXformCache_[i];
	}
}

std::vector<mu::Mat4x4>& AnimBlender::localXformData() {
	DISPLAY_ERROR_STR( stage_ == Stage::committedLocal,
		"[Animation Error] AnimBlender::localXformData: "s
		+ "localXformData에 접근하기 위해선 AnimBlender의 상태가 committedLocal이어야 합니다.\n"
		+ "애니메이션이 다른 단계로 이미 전이했거나, 로컬 변환 계산이 끝나지 않아 캐시의 내용이 유효하지 않습니다.",
		false
	);
	return boneXformCache_;
}

std::vector<mu::Mat4x4>& AnimBlender::worldXformData() {
	DISPLAY_ERROR_STR( stage_ == Stage::committedWorld,
		"[Animation Error] AnimBlender::worldXformData: "s
		+ "worldXformData에 접근하기 위해선 AnimBlender의 상태가 committedWorld이어야 합니다.\n"
		+ "애니메이션이 다른 단계로 이미 전이했거나, 로컬 변환 계산이 끝나지 않아 캐시의 내용이 유효하지 않습니다.",
		false
	);
	return boneXformCache_;
}

std::vector<mu::Mat4x4>& AnimBlender::finalXformData() {
	DISPLAY_ERROR_STR( stage_ == Stage::committedFinal,
		"[Animation Error] AnimBlender::finalXformData: "s
		+ "finalXformData에 접근하기 위해선 AnimBlender의 상태가 committedFinal이어야 합니다.\n"
		+ "애니메이션이 다른 단계로 이미 전이했거나, 로컬 변환 계산이 끝나지 않아 캐시의 내용이 유효하지 않습니다.",
		false
	);
	return boneXformCache_;
}

void AnimSystem::update(Seconds timeSlice) {
	auto tp = HighResolutionClock::now();

	for (std::size_t i = 0; i < jobSize_; ++i) {
		auto job = jobQueue_.top();
		jobQueue_.pop();

		job.blender->onCalcLocal({});
		job.blender->setStage({}, AnimBlender::Stage::committedLocal);

		job.blender->onCalcWorld({});
		job.blender->setStage({}, AnimBlender::Stage::committedWorld);

		job.blender->onCalcFinal({});
		job.blender->setStage({}, AnimBlender::Stage::committedFinal);
	}

	auto elapsed = HighResolutionClock::now() - tp;

	float ratio = elapsed / timeSlice;
	float error = ratio - 1.f;

	static constexpr float Kp = 0.1f;

	jobSize_ -= jobSize_ * error * Kp;
	// jobSize_ = std::clamp(jobSize_, minJobCnt_, maxJobCnt_);
}
#include "pch.hpp"
#include "animation.hpp"
#include "errorHandling.hpp"
#include "binaryImport.hpp"

// 전달된 애니메이션 프레임을 행렬로 변환한다.
// lerpAnimFrames 함수로 여러 프레임을 보간한 최종 프레임을
// 이 함수를 통해 행렬로 변환해서 사용해야 한다.
mu::Mat4x4 MU_CALLCONV convertAnimFrameToMatrix(AnimFrame frame) {
	return mu::Mat4x4(mu::scale(frame.scale)) * mu::Mat4x4(frame.rotation)
		* mu::translate(frame.translation);
}

// 두 애니메이션 프레임을 인자 t로 선형보간한다.
// translation과 scale에 대해선 mu::lerp가,
// rotation에 대해선 mu::slerp가 사용된다.
AnimFrame MU_CALLCONV lerpAnimFrames(AnimFrame lhs, AnimFrame rhs, float t) {
	AnimFrame ret{};
	ret.translation = mu::lerp(lhs.translation, rhs.translation, t);
	ret.rotation = mu::slerp(lhs.rotation, rhs.rotation, t);
	ret.scale = mu::lerp(lhs.scale, rhs.scale, t);
	return ret;
}

void importAnimClip(std::ifstream& ifs, AnimClip& clip) {
	readHeadTag(ifs, "Clip");

	clip.name = readText(ifs, "Name");
	const auto skeletonEnumerationString = readText(ifs, "SkeletonEnumeration");
	clip.skeletonEnumeration = convertStrToSkeletonEnum(skeletonEnumerationString);
	clip.duration = Seconds( readFloat(ifs, "Duration") );
	const auto wrapModeStr = readText(ifs, "WrapMode");

	if (wrapModeStr == "Default" || wrapModeStr == "Loop") {
		clip.flags |= etoi(AnimClipFlag::Loop);	
	}
	else if (wrapModeStr == "Once") {
		// no-op
	}
	else {
		DISPLAY_ERROR_STR( false, "[File I/O Error] importAnimClip: 알 수 없는 wrapMode 값 \""s
			+ wrapModeStr + "\"을 읽었습니다.",
			false
		);
	}

	for (auto& keyFrames : clip.keyFramesOfBones) {
		const auto boneIdx = readInteger(ifs, "Bone");
		const auto keyFrameCnt = readInteger(ifs, "KeyFrameCnt");

		keyFrames.resize(keyFrameCnt);
		for (auto& keyFrame : keyFrames) {
			readHeadTag(ifs, "KeyFrame");

			keyFrame.time = Seconds( readFloat(ifs, "Time") );

			const auto trs = readVec3(ifs, "Translation");
			keyFrame.translation = DirectX::XMLoadFloat3(&trs);

			const auto rot = readVec4(ifs, "Rotation");
			keyFrame.rotation = mu::NQuat(DirectX::XMLoadFloat4(&rot), mu::NQuat::NoNormalize_t{});

			const auto scl = readVec3(ifs, "Scale");
			keyFrame.scale = DirectX::XMLoadFloat3(&scl);

			readTailTag(ifs, "KeyFrame");
		}
	}

	readTailTag(ifs, "Clip");

	gSharedLog << "[Resource Load] 애니메이션 클립 " << clip.name << " 로드 완료\n";
}

std::vector<AnimClip> loadAnimClipsFromFile(const std::filesystem::path& path) {
	std::vector<AnimClip> ret{};

    auto ifs = std::ifstream(path, std::ios::binary);
    DISPLAY_ERROR_STR(ifs.good(), "[File I/O Error]: loadAnimClipsFromFile: "s + path.string() + " 파일을 열 수 없습니다."s, false);
    if (!ifs) {
        return ret;
    }

	const auto animationSetName = readText(ifs, "AnimationSetName");
	const auto clipCnt = readInteger(ifs, "ClipCnt");
	const auto boneCnt = readInteger(ifs, "BoneCnt");

	for (int i = 0; i < clipCnt; ++i) {
		auto& clip = ret.emplace_back();
		clip.keyFramesOfBones.resize(boneCnt);
		importAnimClip(ifs, clip);
	}

    gSharedLog << "[Resource Load] File I/O: 애니메이션 세트 " << animationSetName << '(' << path << ") 로드 완료\n";

    return ret;
}

// 대상 애니메이션 클립을 추가한다.
// 웬만하면 상속한 클래스에서 초기화시점에 사용할 클립들을 전부 push해놓고
// 게임 객체의 상태에 맞게 클립들을 선택해 프레임 업데이트 및 블렌딩을 수행하도록 한다.
void AnimBlender::pushTargetClip(
	const std::string& key, const std::shared_ptr<const AnimClip>& pTargetClip
) {
	DISPLAY_ERROR_STR( skeleton_.pRoot, "[Animation Error] AnimBlender::pushTargetClip: "s
		+ "AnimBlender 객체에 스켈레톤이 할당되어 있지 않습니다.",
		false
	);

	DISPLAY_ERROR_STR( skeleton_.skeletonEnumeration == pTargetClip->skeletonEnumeration,
		"[Animation Error] AnimBlender::pushTargetClip: 스켈레톤과 애니메이션 클립이 호환되지 않습니다.",
		false
	);

	DISPLAY_ERROR_STR( !frameInfoMap_.contains(key),
		"[Animation Error] AnimBlender::pushTargetClip: key \""s + key
		+ "\"이(가) 이미 AnimBlender에 등록되어 있었습니다.",
		false
	);


	if ( !skeleton_.pRoot
		|| skeleton_.skeletonEnumeration != pTargetClip->skeletonEnumeration
		|| frameInfoMap_.contains(key)
	) {
		return;
	}

	auto [pPair, _] = frameInfoMap_.try_emplace(key);
	auto& frameInfo = pPair->second;

	frameInfo.targetClip = pTargetClip;
	frameInfo.frameCache_.resize(skeleton_.bones->size());
	frameInfo.keyFrameIteratorCache_.resize(skeleton_.bones->size());
}

// 대상 애니메이션 클립을 제거한다.
void AnimBlender::popTargetClip(const std::string& key) {
	frameInfoMap_.erase(key);
}

// 스켈레톤을 설정한다.
// 애니메이션 클립들을 추가하기 전에 스켈레톤이 먼저 설정되어야 한다.
void AnimBlender::setSkeleton(const Skeleton& skeleton) {
	skeleton_ = skeleton;
	boneXformCache_.resize(skeleton_.bones->size());
}

// 본들의 로컬 변환을 스켈레톤의 본 트리를 순회하며
// 드레스 공간 변환으로 환원한다.
void AnimBlender::onCalcDress(PassKey<AnimSystem>) {
	traverseBone(*skeleton_.pRoot);
}

void MU_CALLCONV AnimBlender::traverseBone(const Bone& bone, mu::Mat4x4 parentXform) {
	auto& xform = boneXformCache_[bone.boneIdx];
	xform *= parentXform;

	for (const auto* pChild: bone.children) {
		traverseBone(*pChild, xform);
	}
}

// 본들의 최종 변환 행렬들을 계산한다.
// 스켈레톤의 본들은 기본적으로 드레스 공간에 있기 때문에,
// 1. 드레스 공간 -> 본 공간 이전
// 2. 본 공간에서 애니메이션 프레임에 따른 변환 수행
// 3. 본 공간 -> 드레스 공간 이전
// 의 단계가 필요한데,
// onCalcLocal이 2., onCalcDress가 3.의 단계를 수행했다.
// 그 결과 행렬의 앞쪽에 본들의 toLocal 행렬을 곱해주어
// 1.의 단계를 수행한다.
void AnimBlender::onCalcFinal(PassKey<AnimSystem>) {
	auto& bones = *skeleton_.bones;
	for (std::size_t i = 0; i < bones.size(); ++i) {
		boneXformCache_[i] = bones[i].toLocal * boneXformCache_[i];
	}
}

// 본들의 로컬 변환 행렬들에 접근한다.
// onCalcLocal에서 사용하도록 한다.
std::vector<mu::Mat4x4>& AnimBlender::localXformData() {
	DISPLAY_ERROR_STR( stage_ == Stage::committedLocal,
		"[Animation Error] AnimBlender::localXformData: "s
		+ "localXformData에 접근하기 위해선 AnimBlender의 상태가 committedLocal이어야 합니다.\n"
		+ "애니메이션이 다른 단계로 이미 전이했거나, 로컬 변환 계산이 끝나지 않아 캐시의 내용이 유효하지 않습니다.",
		false
	);
	return boneXformCache_;
}

// 본들의 드레스 공간 변환 행렬들에 접근한다.
std::vector<mu::Mat4x4>& AnimBlender::dressXformData() {
	DISPLAY_ERROR_STR( stage_ == Stage::committedDress,
		"[Animation Error] AnimBlender::dressXformData: "s
		+ "dressXformData에 접근하기 위해선 AnimBlender의 상태가 committedDress이어야 합니다.\n"
		+ "애니메이션이 다른 단계로 이미 전이했거나, 로컬 변환 계산이 끝나지 않아 캐시의 내용이 유효하지 않습니다.",
		false
	);
	return boneXformCache_;
}

// 본들의 최종 변환 행렬들을 반환한다.
// 렌더링 시에는 이 행렬들을 참조한다.
std::vector<mu::Mat4x4>& AnimBlender::finalXformData() {
	DISPLAY_ERROR_STR( stage_ == Stage::committedFinal,
		"[Animation Error] AnimBlender::finalXformData: "s
		+ "finalXformData에 접근하기 위해선 AnimBlender의 상태가 committedFinal이어야 합니다.\n"
		+ "애니메이션이 다른 단계로 이미 전이했거나, 로컬 변환 계산이 끝나지 않아 캐시의 내용이 유효하지 않습니다.",
		false
	);
	return boneXformCache_;
}

// key에 해당하는 클립을 elapsed 만큼의 시간이 지났을 때의 프레임으로 갱신한다.
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

// 등록된 AnimBlender 객체들을 priority로 max-heapify하고
// 모든 AnimBlender 객체들을 업데이트하거나 주어진 timeSlice가 소진될 때까지
// AnimBlender 객체들을 업데이트한다.
// AnimBlender::onCalcLocal, AnimBlender::onCalcDress, AnimBlender::onCalcFinal이
// 순서대로 불리며 업데이트된다.
void AnimSystem::update(Seconds timeSlice) {
	auto tp = HighResolutionClock::now();
	Seconds elapsed = 0s;

	// priority 기준 max-heapify
	std::ranges::make_heap(blenders_, std::less<>{}, &AnimBlender::priority);

	std::size_t cntProcessed = 0u;	// 처리한 블렌더 수
	std::size_t jobCnt = 0u;	// 처리한 job 수
	while (elapsed < timeSlice && cntProcessed < blenders_.size()) {
		// 남은 블렌더의 수가 jobSize_보다 작을 수 있다.
		// 그럴 경우엔 남은 블렌더의 수만큼 블렌더들을 처리하도록 한다.
		const auto iteration = std::min(jobSize_, blenders_.size() - cntProcessed);

		for (std::size_t i = 0; i < iteration; ++i) {
			std::pop_heap(blenders_.begin(), blenders_.end());
			auto& blender = blenders_.front();

			blender->onCalcLocal({});
			blender->setStage({}, AnimBlender::Stage::committedLocal);

			blender->onCalcDress({});
			blender->setStage({}, AnimBlender::Stage::committedDress);

			blender->onCalcFinal({});
			blender->setStage({}, AnimBlender::Stage::committedFinal);
		}

		cntProcessed += iteration;
		++jobCnt;
		elapsed += tp - HighResolutionClock::now();
	}

	// jobSize_ 비례 제어
	const float ratio = static_cast<float>(jobCnt) / 8.f;	// 목적: job의 개수가 8개에 근접하기
	const float error = ratio - 1.f;

	static constexpr float Kp = 0.1f;

	jobSize_ -= std::size_t(jobSize_ * error * Kp);
	jobSize_ = std::max(jobSize_, 1ull);
}
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

// 전달된 애니메이션 프레임들의 가중합을 계산한다.
// rotation은 normalized linear interpolation으로 계산된다.
AnimFrame MU_CALLCONV sumWeightedAnimFrames(std::span<WeightedAnimFrame> frames) {
	AnimFrame ret{};
	auto tmpQuat = mu::Quat(0.f, 0.f, 0.f, 0.f);

	for (auto& weightedFrame : frames) {
		ret.translation += weightedFrame.frame.translation * weightedFrame.w;
		tmpQuat += weightedFrame.frame.rotation * weightedFrame.w;
		ret.scale += weightedFrame.frame.scale * weightedFrame.w;
	}
	ret.rotation = mu::NQuat(tmpQuat);

	return ret;
}

void importAnimClip(std::ifstream& ifs, AnimClip& clip, GFX& gfx) {
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

	// 키 프레임 임포트
	readHeadTag(ifs, "KeyFrames");

	for (auto& keyFrames : clip.keyFramesOfBones) {
		const auto boneIdx = readInteger(ifs, "Bone");
		const auto keyFrameCnt = readInteger(ifs, "KeyFrameCnt");

		keyFrames.resize(keyFrameCnt + 1u);	// sentinel 값을 위함
		for (int i = 0; i < keyFrameCnt; ++i) {
			auto& keyFrame = keyFrames[i];

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

		// sentinel 값 추가: 애니메이션 시간은 +inf, 변환은 마지막 프레임과 같음
		keyFrames[keyFrameCnt].time = Seconds(std::numeric_limits<float>::max());
		keyFrames[keyFrameCnt].translation = keyFrames[keyFrameCnt - 1].translation;
		keyFrames[keyFrameCnt].rotation = keyFrames[keyFrameCnt - 1].rotation;
		keyFrames[keyFrameCnt].scale = keyFrames[keyFrameCnt - 1].scale;
	}
	
	readTailTag(ifs, "KeyFrames");

	// baked samples 임포트
	readHeadTag(ifs, "BakedSamples");

	clip.bakedSampleRate = readFloat(ifs, "SampleRate");

	for (auto& bakedSamples : clip.bakedSamplesOfBones) {
		const auto boneIdx = readInteger(ifs, "Bone");
		const auto sampleCnt = readInteger(ifs, "sampleCnt");

		bakedSamples.resize(sampleCnt + 1u);	// sentinel 값을 위함
		for (int i = 0; i < sampleCnt; ++i) {
			auto& sample = bakedSamples[i];

			readHeadTag(ifs, "Sample");
			const auto mtx = readMatrix(ifs, "Matrix");
			sample = mu::Mat4x4( DirectX::XMLoadFloat4x4(&mtx) );
			readTailTag(ifs, "Sample");
		}

		// sentinel 값 추가: 애니메이션 시간은 +inf, 변환은 마지막 프레임과 같음
		bakedSamples[sampleCnt] = bakedSamples[sampleCnt - 1];
	}

	readTailTag(ifs, "BakedSamples");

	readTailTag(ifs, "Clip");

	// d3d12단 baked sample 텍스처 생성 및 srv 등록
	gfx.addRequestBakeAnimation( RequestBakeAnimation{
		.samples = clip.bakedSamplesOfBones,
		.pDest = &clip.bakedSamples
	});

	gSharedLog << "[Resource Load] 애니메이션 클립 \"" << clip.name << "\"로드 완료\n";
}

std::vector<AnimClip> loadAnimClipsFromFile(const std::filesystem::path& path, GFX& gfx) {
	std::vector<AnimClip> ret{};

    auto ifs = std::ifstream(path, std::ios::binary);
    DISPLAY_ERROR_STR(ifs.good(), "[File I/O Error]: loadAnimClipsFromFile: "s + path.string() + " 파일을 열 수 없습니다."s, false);
    if (!ifs) {
        return ret;
    }

	const auto animationSetName = readText(ifs, "AnimationSetName");
	const auto clipCnt = readInteger(ifs, "ClipCnt");
	const auto boneCnt = readInteger(ifs, "BoneCnt");

	ret.reserve(clipCnt);

	for (int i = 0; i < clipCnt; ++i) {
		auto& clip = ret.emplace_back();
		clip.keyFramesOfBones.resize(boneCnt);
		clip.bakedSamplesOfBones.resize(boneCnt);
		importAnimClip(ifs, clip, gfx);
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
	for (std::size_t i = 0u; i < frameInfo.keyFrameIteratorCache_.size(); ++i) {
		frameInfo.keyFrameIteratorCache_[i] = frameInfo.targetClip->keyFramesOfBones[i].begin();
	}
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
	if (mode_ == Mode::Baked) {
		return;
	}
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
	if (mode_ == Mode::Baked) {
		return;
	}
	auto& bones = *skeleton_.bones;
	for (std::size_t i = 0; i < bones.size(); ++i) {
		boneXformCache_[i] = bones[i].toLocal * boneXformCache_[i];
	}
}

// 본들의 로컬 변환 행렬들에 접근한다.
// onCalcLocal에서 사용하도록 한다.
std::vector<mu::Mat4x4>& AnimBlender::localXformData() {
	//DISPLAY_ERROR_STR( stage_ == Stage::committedLocal,
	//	"[Animation Error] AnimBlender::localXformData: "s
	//	+ "localXformData에 접근하기 위해선 AnimBlender의 상태가 committedLocal이어야 합니다.\n"
	//	+ "애니메이션이 다른 단계로 이미 전이했거나, 로컬 변환 계산이 끝나지 않아 캐시의 내용이 유효하지 않습니다.",
	//	false
	//);
	return boneXformCache_;
}

// 본들의 드레스 공간 변환 행렬들에 접근한다.
std::vector<mu::Mat4x4>& AnimBlender::dressXformData() {
	//DISPLAY_ERROR_STR( stage_ == Stage::committedDress,
	//	"[Animation Error] AnimBlender::dressXformData: "s
	//	+ "dressXformData에 접근하기 위해선 AnimBlender의 상태가 committedDress이어야 합니다.\n"
	//	+ "애니메이션이 다른 단계로 이미 전이했거나, 로컬 변환 계산이 끝나지 않아 캐시의 내용이 유효하지 않습니다.",
	//	false
	//);
	return boneXformCache_;
}

// 본들의 최종 변환 행렬들을 반환한다.
// 렌더링 시에는 이 행렬들을 참조한다.
std::vector<mu::Mat4x4>& AnimBlender::finalXformData() {
	//DISPLAY_ERROR_STR( stage_ == Stage::committedFinal,
	//	"[Animation Error] AnimBlender::finalXformData: "s
	//	+ "finalXformData에 접근하기 위해선 AnimBlender의 상태가 committedFinal이어야 합니다.\n"
	//	+ "애니메이션이 다른 단계로 이미 전이했거나, 로컬 변환 계산이 끝나지 않아 캐시의 내용이 유효하지 않습니다.",
	//	false
	//);
	return boneXformCache_;
}

const std::vector<mu::Mat4x4>& AnimBlender::finalXformData() const {
	return boneXformCache_;
}

// key에 해당하는 클립을 elapsed 만큼의 시간이 지났을 때의 프레임으로 갱신한다.
void AnimBlender::updateFrames(const std::string& key, Seconds elapsed) {
	updateLag_ = 0s;
	priority_ = 0.f;

	DISPLAY_ERROR_STR( frameInfoMap_.contains(key),
		"[Animation Error] AnimBlender::updateFrames: key \""s + key
		+ "\"이(가) AnimBlender에 등록되어 있지 않습니다.",
		false
	);

	if (!frameInfoMap_.contains(key)) {
		return;
	}

	if (mode_ == Mode::Keyframe) {
		updateFramesKeyframeMode(key, elapsed);
	}
	else /* if (mode_ == Mode::Baked) */ {
		updateFramesBakedMode(key, elapsed);
	}
}

// key에 해당하는 클립을 elapsed 만큼의 시간이 지났을 때의 프레임으로 갱신한다.
void AnimBlender::updateFramesKeyframeMode(const std::string& key, Seconds elapsed) {
	auto& frameInfo = frameInfoMap_.at(key);

	for (std::size_t i = 0; i < frameInfo.frameCache_.size(); ++i) {
		auto itCurKeyFrame = frameInfo.keyFrameIteratorCache_[i];
		auto itNextKeyFrame = std::next(itCurKeyFrame);

		while (elapsed < itCurKeyFrame->time) {
			itNextKeyFrame = itCurKeyFrame;
			itCurKeyFrame = std::prev(itCurKeyFrame);
		}

		while (elapsed > itNextKeyFrame->time) {
			itCurKeyFrame = itNextKeyFrame;
			itNextKeyFrame = std::next(itCurKeyFrame);
		}

		frameInfo.keyFrameIteratorCache_[i] = itCurKeyFrame;

		frameInfo.frameCache_[i] = lerpAnimFrames( *itCurKeyFrame, *itNextKeyFrame,
			(elapsed - itCurKeyFrame->time) / (itNextKeyFrame->time - itCurKeyFrame->time)	
		);
	}
}

// key에 해당하는 클립을 elapsed 만큼의 시간이 지났을 때의 프레임으로 갱신한다.
void AnimBlender::updateFramesBakedMode(const std::string& key, Seconds elapsed) {
	auto& frameInfo = frameInfoMap_.at(key);
	frameInfo.bakedSampleIdx = static_cast<std::size_t>(
		std::min( elapsed.count() * frameInfo.targetClip->bakedSampleRate,
			frameInfo.targetClip->duration.count() * frameInfo.targetClip->bakedSampleRate
		)
	);
}

void AnimBlender::updatePriority(PassKey<AnimSystem>, Seconds dt, mu::Vec3 refPos) {
    const float dist = (cachedPos_ - refPos).len();

    constexpr float kDistScale = 50.f;
    const float d = dist / kDistScale;

    // 거리 기반 weight
    const float wDist = 1.f / (1.f + d * d);

	mode_ = wDist < 0.75f ? Mode::Baked : Mode::Keyframe;

    // 시간 기반 weight
    const float t = static_cast<float>(updateLag_.count());
    constexpr float kTimeScale = 0.5f; // 0.2~0.5

    // saturating 형태 (폭주 방지)
    const float wTime = 1.f + (t / (t + kTimeScale));

    // 최종 priority 누적
    priority_ = wDist * wTime;

    // lastUpdateTime_ 누적
    updateLag_ += dt;
}

void AnimSystem::updatePriorities(Seconds dt, mu::Vec3 refPos) {
	for (auto* b : blenders_) {
		b->updatePriority({}, dt, refPos);
	}
}

// 등록된 AnimBlender 객체들을 priority로 max-heapify하고
// 모든 AnimBlender 객체들을 업데이트하거나 주어진 timeSlice가 소진될 때까지
// AnimBlender 객체들을 업데이트한다.
// AnimBlender::onCalcLocal, AnimBlender::onCalcDress, AnimBlender::onCalcFinal이
// 순서대로 불리며 업데이트된다.
void AnimSystem::update(Seconds timeSlice) {
	// culled 블렌더를 뒤로 밀고, visible range([begin, visibleEnd))만 처리한다.
	const auto visibleEnd = std::partition(blenders_.begin(), blenders_.end(),
		[](AnimBlender* b) { return !b->isCulled(); });
	const std::size_t visibleCount =
		static_cast<std::size_t>(std::distance(blenders_.begin(), visibleEnd));

	if (visibleCount == 0u) return;

	auto tp = HighResolutionClock::now();
	Seconds elapsed = 0s;

	// priority 기준 max-heapify (visible range만)
	std::ranges::make_heap(
		std::ranges::subrange(blenders_.begin(), visibleEnd),
		std::less<>{}, &AnimBlender::priority);

	std::size_t cntProcessed = 0u;	// 처리한 블렌더 수
	std::size_t jobCnt = 0u;	// 처리한 job 수
	while (elapsed < timeSlice && cntProcessed < visibleCount) {
		// 남은 블렌더의 수가 jobSize_보다 작을 수 있다.
		// 그럴 경우엔 남은 블렌더의 수만큼 블렌더들을 처리하도록 한다.
		const auto iteration = std::min(jobSize_, visibleCount - cntProcessed);

		for (std::size_t i = 0; i < iteration; ++i) {
			const auto pLast = std::prev(visibleEnd, i);
			std::pop_heap(blenders_.begin(), pLast);
			auto& blender = *std::prev(pLast);

			blender->onCalcLocal({});
			blender->setStage({}, AnimBlender::Stage::committedLocal);

			blender->onCalcDress({});
			blender->setStage({}, AnimBlender::Stage::committedDress);

			blender->onCalcFinal({});
			blender->setStage({}, AnimBlender::Stage::committedFinal);
		}

		cntProcessed += iteration;
		++jobCnt;
		elapsed = HighResolutionClock::now() - tp;
	}

	// --- jobSize_ 제어 로직 (시간 초과 기반 안전 장치 추가) ---
    // 만약 타임 슬라이스를 초과해서 끝난 거라면, jobSize_가 너무 커서 정밀 제어가 안 된 것일 수 있음.
    // 반대로 시간이 남았는데 jobCnt가 너무 많다면 jobSize_를 키워 오버헤드를 줄임.
    if (elapsed >= timeSlice) {
        // 시간이 부족한 상황: 배치가 너무 커서 시계 체크를 제때 못했을 수 있으므로 조금 줄임
        jobSize_ = std::max(1ULL, static_cast<std::size_t>(jobSize_ * 0.85f));
    } else if (cntProcessed == visibleCount && jobCnt > 8) {
        // 시간이 충분하고 모든 블렌더를 처리했는데 Job 개수가 너무 많음 -> 배치 크기를 키움
        jobSize_ = static_cast<std::size_t>(jobSize_ * 1.15f);
    }
}
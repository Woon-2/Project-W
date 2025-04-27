#include "game/animSystem.hpp"

#include <cmath>

#include "TMP.hpp"

Bone::Bone(Bone&& other) noexcept
    : toParent_(other.toParent_), toLocal_(other.toLocal_),
    boneIdx_(std::exchange(other.boneIdx_, -1)), name_(std::move(other.name_)),
    children_(std::move(other.children_)), pSkeleton_(std::exchange(other.pSkeleton_, nullptr)) {
    for (auto& child : children_) {
        child->pSkeleton_ = this->pSkeleton_;
    }
}

Bone& Bone::operator=(Bone&& other) noexcept {
    if (this == &other) {
        return *this;
    }

    toParent_ = other.toParent_;
    toLocal_ = other.toLocal_;
    boneIdx_ = std::exchange(other.boneIdx_, -1);
    name_ = std::move(other.name_);
    children_ = std::move(other.children_);
    pSkeleton_ = std::exchange(other.pSkeleton_, nullptr);

    for (auto& child : children_) {
        child->pSkeleton_ = this->pSkeleton_;
    }

    return *this;
}

void Bone::addChild(Bone* child) {
    child->pSkeleton_ = pSkeleton_;
    children_.push_back(child);
}

Skeleton::Skeleton(Skeleton&& other) noexcept
    : boneStorage_(other.boneStorage_.size()),
    pRoot_(nullptr) {
    auto pOtherFirstBone = other.boneStorage_.data();

    pRoot_ = boneStorage_.data() + (other.pRoot_ - pOtherFirstBone);

    for (std::size_t i = 0; i < other.boneStorage_.size(); ++i) {
        auto& bone = other.boneStorage_[i];
        auto& newBone = boneStorage_[i];

        newBone = Bone(this);
        newBone.toParent_ = bone.toParent_;
        newBone.toLocal_ = bone.toLocal_;
        newBone.boneIdx_ = bone.boneIdx_;
        newBone.name_ = std::move(bone.name_);

        for (auto pChild : bone.children_) {
            newBone.addChild(boneStorage_.data() + (pChild - pOtherFirstBone));
        }

        bone.children_.clear();
        bone.pSkeleton_ = nullptr;
    }

    other.boneStorage_.clear();
    other.pRoot_ = nullptr;
}

Skeleton& Skeleton::operator=(Skeleton&& other) noexcept {
    if (this == &other) {
        return *this;
    }

    boneStorage_.resize(other.boneStorage_.size());

    auto pOtherFirstBone = other.boneStorage_.data();

    pRoot_ = boneStorage_.data() + (other.pRoot_ - pOtherFirstBone);

    for (std::size_t i = 0; i < other.boneStorage_.size(); ++i) {
        auto& bone = other.boneStorage_[i];
        auto& newBone = boneStorage_[i];

        newBone = Bone(this);
        newBone.toParent_ = bone.toParent_;
        newBone.toLocal_ = bone.toLocal_;
        newBone.boneIdx_ = bone.boneIdx_;
        newBone.name_ = std::move(bone.name_);

        for (auto pChild : bone.children_) {
            newBone.addChild(boneStorage_.data() + (pChild - pOtherFirstBone));
        }

        bone.children_.clear();
        bone.pSkeleton_ = nullptr;
    }

    other.boneStorage_.clear();
    other.pRoot_ = nullptr;

    return *this;
}

Skeleton Skeleton::loadHierarchyFromFile(const std::filesystem::path& path) {
    auto in = std::ifstream(path, std::ios::binary);
    if (!in) {
        throw std::runtime_error("Failed to open file: " + path.string());
    }

    return loadHierarchyFromStream(in);
}

Skeleton Skeleton::loadHierarchyFromStream(std::istream& in) {
    Skeleton skeleton;

    char pstrToken[64] = { '\0' };

	std::uint8_t nStrLength = 0;
	std::size_t nReads = 0;

    readStream(in, nStrLength);
    readStream(in, pstrToken, nStrLength);
    if (strcmp(pstrToken, "<Skeleton:>")) {
        throw std::runtime_error("expected Skeleton token but got: " + std::string(pstrToken));
    }

    readStream(in, nStrLength);
    readStream(in, pstrToken, nStrLength);
    if (strcmp(pstrToken, "<BoneCnt:>")) {
        throw std::runtime_error("expected BoneCnt token but got: " + std::string(pstrToken));
    }

    
    int nBones = 0;
    readStream(in, nBones);
    skeleton.boneStorage_.reserve(nBones);

    for (;;) {
        readStream(in, nStrLength);
        readStream(in, pstrToken, nStrLength);

        if (!strcmp(pstrToken, "<Bone:>")) {
            auto& rootBone = skeleton.boneStorage_.emplace_back(&skeleton);
            skeleton.pRoot_ = &rootBone;
            loadBonesFromStream(in, rootBone, skeleton);
        }
        else if (!strcmp(pstrToken, "</Skeleton>")) {
            break;
        }
        else {
            throw std::runtime_error("expected Bone or Skeleton end token but got: " + std::string(pstrToken));
        }
    }
    
    return skeleton;
}

void Skeleton::loadBonesFromStream(std::istream& in, Bone& bone, Skeleton& skeleton) {
    char pstrToken[64] = { '\0' };

	std::uint8_t nStrLength = 0;
	std::size_t nReads = 0;

    dx::XMFLOAT4X4 xform{};
    int intVal{};

    readStream(in, nStrLength);
    auto boneName = std::string(nStrLength, '\0');
    readStream(in, boneName.data(), nStrLength);

    for (;;) {
        readStream(in, nStrLength);
        readStream(in, pstrToken, nStrLength);

        if (!strcmp(pstrToken, "<BoneIndex:>")) {
            readStream(in, intVal);
            bone.boneIdx_ = intVal;
        }
        else if (!strcmp(pstrToken, "<Xform:>")) {
            readStream(in, xform);
            bone.toParent_ = mu::Mat4x4(DirectX::XMLoadFloat4x4(&xform));
        }
        else if (!strcmp(pstrToken, "<BindPose:>")) {
            readStream(in, xform);
            bone.toLocal_ = mu::Mat4x4(DirectX::XMLoadFloat4x4(&xform));
        }
        else if (!strcmp(pstrToken, "<Children:>")) {
            int nChilds = 0;
            readStream(in, nChilds);
            if (nChilds > 0) {
                for (int i = 0; i < nChilds; ++i) {
                    auto& child = skeleton.boneStorage_.emplace_back(&skeleton);

                    readStream(in, nStrLength);
                    readStream(in, pstrToken, nStrLength);
                    if (strcmp(pstrToken, "<Bone:>")) {
                        throw std::runtime_error("Bone token expected but got: " + std::string(pstrToken));
                    }

                    loadBonesFromStream(in, child, skeleton);
                    bone.addChild(&child);
                }
            }
        }
        else if (!strcmp(pstrToken, "</Bone>")) {
            break;
        }
    }
}

void AnimClip::loadKeyFrameClipFromStream(std::istream& in, AnimClip& animClip) {
    char pstrToken[64] = { '\0' };

	std::uint8_t nStrLength = 0;
	std::size_t nReads = 0;


    readStream(in, nStrLength);
    auto clipName = std::string(nStrLength, '\0');
    readStream(in, clipName.data(), nStrLength);
    animClip.name_ = clipName;

    // allocate memory spaces for key frames
    // with extracted bone count and key frame count
    readStream(in, nStrLength);
    readStream(in, pstrToken, nStrLength);
    if (std::strcmp(pstrToken, "<BoneCnt:>")) {
        throw std::runtime_error("expected BoneCnt token but got: " + std::string(pstrToken));
    }

    int boneCnt{};
    readStream(in, boneCnt);
    animClip.keyFrames_.resize(boneCnt);

    readStream(in, nStrLength);
    readStream(in, pstrToken, nStrLength);
    if (std::strcmp(pstrToken, "<Duration:>")) {
        throw std::runtime_error("expected Duration token but got: " + std::string(pstrToken));
    }

    float duration{};
    readStream(in, duration);
    animClip.duration_ = Milliseconds(duration * 1000.f);

    readStream(in, nStrLength);
    readStream(in, pstrToken, nStrLength);
    if (std::strcmp(pstrToken, "<KeyFrames:>")) {
        throw std::runtime_error("expected KeyFrames token but got: " + std::string(pstrToken));
    }
    int keyFrameCnt{};
    readStream(in, keyFrameCnt);

    for (int i = 0; i < boneCnt; ++i) {
        auto& keyFrames = animClip.keyFrames_[i];
        // we assume that the key frame count for each bone
        // is averaged to be the whole key frame count divided by 16.
        keyFrames.reserve(keyFrameCnt / 16u);
    }

    // read key frames
    for (int keyFrameIdx = 0; keyFrameIdx < keyFrameCnt; ++keyFrameIdx) {
        readStream(in, nStrLength);
        readStream(in, pstrToken, nStrLength);
        if (std::strcmp(pstrToken, "<KeyFrame:>")) {
            throw std::runtime_error("expected KeyFrame token but got: " + std::string(pstrToken));
        }

        float time{};
        readStream(in, time);

        for (;;) {
            readStream(in, nStrLength);
            readStream(in, pstrToken, nStrLength);

            if (!std::strcmp(pstrToken, "<BoneIdx:>")) {
                int boneIdx{};
                readStream(in, boneIdx);

                auto& keyFrame = animClip.keyFrames_[boneIdx].emplace_back();

                keyFrame.time = time;

                dx::XMFLOAT3 pos{};
                readStream(in, pos);
                keyFrame.pos = mu::Vec3(dx::XMLoadFloat3(&pos));

                dx::XMFLOAT4 rot{};
                readStream(in, rot);
                keyFrame.rot = mu::NQuat(dx::XMLoadFloat4(&rot));

                dx::XMFLOAT3 scale{};
                readStream(in, scale);
                keyFrame.scale = mu::Vec3(dx::XMLoadFloat3(&scale));
            }
            else if (!std::strcmp(pstrToken, "</KeyFrame>")) {
                break;
            }
            else {
                throw std::runtime_error("expected BoneIdx or KeyFrame end token but got: " + std::string(pstrToken));
            }
        }
    }

    readStream(in, nStrLength);
    readStream(in, pstrToken, nStrLength);

    if (std::strcmp(pstrToken, "</AnimationClip>")) {
        throw std::runtime_error("expected AnimationClip end token but got: " + std::string(pstrToken));
    }

    // temporary
    animClip.flags_ = etoi(AnimClip::Flags::Loop);
}

void AnimClip::loadPresampledClipFromStream(std::istream& in, AnimClip& animClip) {
    char pstrToken[64] = { '\0' };

	std::uint8_t nStrLength = 0;
	std::size_t nReads = 0;
    dx::XMFLOAT4X4 xform{};


    readStream(in, nStrLength);
    auto clipName = std::string(nStrLength, '\0');
    readStream(in, clipName.data(), nStrLength);
    animClip.name_ = clipName;

    // allocate memory spaces for samples
    // with extracted bone count and sample count
    readStream(in, nStrLength);
    readStream(in, pstrToken, nStrLength);
    if (std::strcmp(pstrToken, "<BoneCnt:>")) {
        throw std::runtime_error("expected BoneCnt token but got: " + std::string(pstrToken));
    }

    int boneCnt{};
    readStream(in, boneCnt);
    animClip.samples_.resize(boneCnt);

    readStream(in, nStrLength);
    readStream(in, pstrToken, nStrLength);
    if (std::strcmp(pstrToken, "<Duration:>")) {
        throw std::runtime_error("expected Duration token but got: " + std::string(pstrToken));
    }

    float duration{};
    readStream(in, duration);
    animClip.duration_ = Milliseconds(duration * 1000.f);

    readStream(in, nStrLength);
    readStream(in, pstrToken, nStrLength);
    if (std::strcmp(pstrToken, "<Samples:>")) {
        throw std::runtime_error("expected Samples token but got: " + std::string(pstrToken));
    }
    int sampleCnt{};
    readStream(in, sampleCnt);
    animClip.sampleCnt_ = sampleCnt;

    for (int i = 0; i < boneCnt; ++i) {
        auto& samples = animClip.samples_[i];
        samples.reserve(sampleCnt);
    }

    // read samples
    for (int sampleIdx = 0; sampleIdx < sampleCnt; ++sampleIdx) {
        readStream(in, nStrLength);
        readStream(in, pstrToken, nStrLength);
        if (std::strcmp(pstrToken, "<Sample:>")) {
            throw std::runtime_error("expected Sample token but got: " + std::string(pstrToken));
        }

        for (int i = 0; i < boneCnt; ++i) {
            readStream(in, nStrLength);
            readStream(in, pstrToken, nStrLength);
            if (std::strcmp(pstrToken, "<Xform:>")) {
                throw std::runtime_error("expected Xform token but got: " + std::string(pstrToken));
            }
            readStream(in, xform);
            animClip.samples_[i].emplace_back(
                mu::Mat4x4(DirectX::XMLoadFloat4x4(&xform))
            );
        }

        readStream(in, nStrLength);
        readStream(in, pstrToken, nStrLength);
        if (std::strcmp(pstrToken, "</Sample>")) {
            throw std::runtime_error("expected Sample end token but got: " + std::string(pstrToken));
        }
    }

    readStream(in, nStrLength);
    readStream(in, pstrToken, nStrLength);

    if (std::strcmp(pstrToken, "</AnimationClip>")) {
        throw std::runtime_error("expected AnimationClip end token but got: " + std::string(pstrToken));
    }

    // temporary
    animClip.flags_ = etoi(AnimClip::Flags::Loop);
}

std::vector<AnimClip> AnimClip::loadClipsFromStream(std::istream& in) {
    std::vector<AnimClip> animClips;

    char pstrToken[64] = { '\0' };

    std::uint8_t nStrLength = 0;
    std::size_t nReads = 0;

    readStream(in, nStrLength);
    readStream(in, pstrToken, nStrLength);
    if (std::strcmp(pstrToken, "<KeyFrameAnimationClips:>")) {
        throw std::runtime_error("expected KeyFrameAnimationClips token but got: " + std::string(pstrToken));
    }

    int clipCnt{};
    readStream(in, clipCnt);
    animClips.reserve(clipCnt);

    for (int i = 0; i < clipCnt; ++i) {
        readStream(in, nStrLength);
        readStream(in, pstrToken, nStrLength);

        if (std::strcmp(pstrToken, "<AnimationClip:>")) {
            throw std::runtime_error("expected AnimationClip token but got: " + std::string(pstrToken));
        }

        AnimClip::loadKeyFrameClipFromStream(in, animClips.emplace_back());
    }

    readStream(in, nStrLength);
    readStream(in, pstrToken, nStrLength);
    if (std::strcmp(pstrToken, "</KeyFrameAnimationClips>")) {
        throw std::runtime_error("expected KeyFrameAnimationClips end token but got: " + std::string(pstrToken));
    }

    readStream(in, nStrLength);
    readStream(in, pstrToken, nStrLength);
    if (std::strcmp(pstrToken, "<PresampledAnimationClips:>")) {
        throw std::runtime_error("expected PresampledAnimationClips token but got: " + std::string(pstrToken));
    }

    int presampledClipCnt{};
    readStream(in, presampledClipCnt);

    if (presampledClipCnt != clipCnt) {
        throw std::runtime_error("expected PresampledAnimationClips count to be equal to KeyFrameAnimationClips count.");
    }

    for (int i = 0; i < presampledClipCnt; ++i) {
        readStream(in, nStrLength);
        readStream(in, pstrToken, nStrLength);

        if (std::strcmp(pstrToken, "<AnimationClip:>")) {
            throw std::runtime_error("expected AnimationClip token but got: " + std::string(pstrToken));
        }

        AnimClip::loadPresampledClipFromStream(in, animClips.at(i));
    }

    readStream(in, nStrLength);
    readStream(in, pstrToken, nStrLength);

    if (std::strcmp(pstrToken, "</PresampledAnimationClips>")) {
        throw std::runtime_error("expected PresampledAnimationClips end token but got: " + std::string(pstrToken));
    }

    return animClips;
}

mu::Mat4x4 MU_CALLCONV AnimClip::sample(BoneIdx boneIdx, Milliseconds elapsed) const {
    auto& samples = samples_[boneIdx];
    auto sampleIdx = static_cast<std::size_t>(elapsed / sampleInterval());
    if (sampleIdx >= samples.size()) {
        sampleIdx = samples.size() - 1;
    }
    return samples[sampleIdx];
}

SkeletonAnimClipsPair loadSkeletonAndAnimClipFromFile(
    const std::filesystem::path& animBinaryPath
) {
    SkeletonAnimClipsPair pair;

    auto in = std::ifstream(animBinaryPath, std::ios::binary);

    pair.skeleton = Skeleton::loadHierarchyFromStream(in);
    pair.animClips = AnimClip::loadClipsFromStream(in);

    return pair;
}

AnimInstance::AnimInstance( const Skeleton* pSkeleton, const AnimClip* pAnimClip,
    Milliseconds preElapsed, ClipMode clipMode, int flags
) : boneXformCache_(pSkeleton->bones().size()),
    keyFrameCache_(), pAnimClip_(pAnimClip), pSkeleton_(pSkeleton),
    elapsedTime_(preElapsed), stage_(Stage::None), speed_(1.f), weight_(0.f), clipMode_(clipMode),
    flags_(flags) {
    if (!pAnimClip_) {
        throw std::runtime_error("AnimInstance requires a valid AnimClip.");
    }

    if (!pSkeleton_) {
        throw std::runtime_error("AnimInstance requires a valid Skeleton.");
    }

    if (pAnimClip_->boneCnt() != pSkeleton_->bones().size()) {
        throw std::runtime_error("AnimClip bone count does not match Skeleton bone count.");
    }

    if (clipMode == ClipMode::KeyFrame) {
        // initialize keyFrameCache_ with the first key frame of each bone
        keyFrameCache_.reserve(pSkeleton_->bones().size());
        for (std::size_t i = 0; i < pSkeleton->bones().size(); ++i) {
            keyFrameCache_.push_back(pAnimClip_->keyFrameBegin(static_cast<BoneIdx>(i)));
        }
    }
}

void AnimInstance::update(Milliseconds deltaTime) {
    if (stage_ != Stage::None) {
        throw std::runtime_error(
            "[Description] AnimInstance::update: expected AnimInstance stage: "
            + std::to_string(etoi(Stage::None)) +
            " but got: " + std::to_string(etoi(stage_))
        );
    }

    elapsedTime_ += deltaTime * speed_;

    if (elapsedTime_ > pAnimClip_->duration()) {

        if (!(flags_ & AnimClip::Flags::Loop)) {
            stage_ = Stage::CalcLocal;
            return;
        }
        elapsedTime_ -= pAnimClip_->duration();

        if (clipMode_ == ClipMode::KeyFrame) {
            // reset key frame cache
            for (std::size_t i = 0; i < pSkeleton_->bones().size(); ++i) {
                keyFrameCache_[i] = pAnimClip_->keyFrameBegin(static_cast<BoneIdx>(i));
            }
        }
    }

    if (clipMode_ == ClipMode::KeyFrame) {
        for (std::size_t i = 0; i < pSkeleton_->bones().size(); ++i) {
            auto& keyFrame = keyFrameCache_[i];
            auto nextKeyFrame = std::next(keyFrame);

            // every key frame must end with sentinal key frame
            // which has time = float_max
            while (nextKeyFrame->time * 1000.f <= elapsedTime_.count()) {
                keyFrame = nextKeyFrame;
                ++nextKeyFrame;
            }
        }
    }
    else /* if clipMode_ == ClipMode::Presampled */ {
        // skip sampling.
        // empty samples means that the clip is sampled in another way that user defined.
        // the user defined sampling should much performant than the one in this function.
        if (!pAnimClip_->presampleData().empty()) {
            for (std::size_t i = 0; i < pSkeleton_->bones().size(); ++i) {
                boneXformCache_[i] = pAnimClip_->sample(static_cast<BoneIdx>(i), elapsedTime_);
            }
        }
    }

    stage_ = Stage::CalcLocal;
}

TaskCompute AnimInstance::calcLocals(AnimSystem& animSystem) {
    // the animations played during AnimController::update()
    // via Animation Sequences are not updated yet but valid.
    // so we accept the stage_ to be CalcLocal or None.
    if (stage_ != Stage::CalcLocal && stage_ != Stage::None) {
        throw std::runtime_error(
            "[Description] AnimInstance::calcLocals: expected AnimInstance stage: "
            + std::to_string(etoi(Stage::CalcLocal)) +
            " but got: " + std::to_string(etoi(stage_))
        );
    }

    if (clipMode_ == ClipMode::Presampled) {
        // skip local transform calculation
        co_await std::suspend_always{};
        stage_ = Stage::CalcWorld;
        co_return;
    }

    // calculate local transforms for each bone
    // with compute shader

    // lhsFrame, rhsFrame, ratio => localMat
    std::vector<std::size_t> indices;
    indices.reserve(keyFrameCache_.size());

    for (auto& lhsFrame : keyFrameCache_) {
        auto rhsFrame = std::next(lhsFrame);

        auto ratio = (elapsedTime_.count() / 1000.f - lhsFrame->time) / (rhsFrame->time - lhsFrame->time);
        indices.push_back( animSystem.addKeyFramePair(
            *lhsFrame, *rhsFrame, ratio
        ) );
    }

    co_await std::suspend_always{};

    for (std::size_t i = 0u; i < indices.size(); ++i) {
        boneXformCache_[i] = animSystem.getXform(indices[i]);
    }

    stage_ = Stage::CalcWorld;
}

void AnimInstance::calcWorlds(AnimSystem& animSystem) {
    if (stage_ != Stage::CalcWorld) {
        throw std::runtime_error(
            "[Description] AnimInstance::calcWorlds: expected AnimInstance stage: "
            + std::to_string(etoi(Stage::CalcWorld)) +
            " but got: " + std::to_string(etoi(stage_))
        );
    }

    if (clipMode_ == ClipMode::Presampled) {
        // skip world transform calculation
        stage_ = Stage::CalcFinal;
        return;
    }

    auto pRootBone = pSkeleton_->root();

    traverseBone(*pRootBone);

    stage_ = Stage::CalcFinal;
}

TaskCompute AnimInstance::calcFinals(AnimSystem& animSystem) {
    if (stage_ != Stage::CalcFinal) {
        throw std::runtime_error(
            "[Description] AnimInstance::calcFinals: expected AnimInstance stage: "
            + std::to_string(etoi(Stage::CalcFinal)) +
            " but got: " + std::to_string(etoi(stage_))
        );
    }

    if (clipMode_ == ClipMode::Presampled) {
        // skip final transform calculation
        co_await std::suspend_always{};
        stage_ = Stage::None;
        co_return;
    }

    // calculate final transforms for each bone
    // with compute shader
    std::vector<std::size_t> indices;
    indices.reserve(boneXformCache_.size());

    for (std::size_t i = 0u; i < boneXformCache_.size(); ++i) {
        auto a = mu::inverse(pSkeleton_->bones()[i].toLocalMatrix());
        auto b = boneXformCache_[i];

        indices.push_back(animSystem.addXformPair(
            pSkeleton_->bones()[i].toLocalMatrix(), boneXformCache_[i]
        ));
    }

    co_await std::suspend_always{};

    for (std::size_t i = 0u; i < indices.size(); ++i) {
        boneXformCache_[i] = animSystem.getXform(indices[i]);
    }

    stage_ = Stage::None;
}

void AnimInstance::traverseBone(const Bone& bone, const mu::Mat4x4& parentXform) {
    boneXformCache_[bone.boneIdx()] *= parentXform;

    for (const auto& child : bone.children()) {
        traverseBone(*child, boneXformCache_[bone.boneIdx()]);
    }
}

std::coroutine_handle<> AnimController::play( const std::string& key,
    Milliseconds preElapsed, AnimInstance::ClipMode clipMode
) {
    const auto& animClip = clipInfo(key);

    const auto itDupl = std::ranges::find_if(insts_, [&key](const auto& pair) {
        return pair.first == key;
    });
    
    if (itDupl != insts_.end()) {
        itDupl->second.setElapsed(preElapsed);
    
        if (animClip.flags() & AnimClip::Flags::Loop) {
            return resetAnimSequence(key, loopImpl(key, std::noop_coroutine(), preElapsed, *this));
        }
        else {
            return resetAnimSequence(key, onceImpl(key, std::noop_coroutine(), preElapsed, *this));
        }
    }

    insts_.emplace_back(key,
        AnimInstance(pSkeleton_, &animClip, preElapsed, clipMode)
    );

    if (animClip.flags() & AnimClip::Flags::Loop) {
        std::get<std::coroutine_handle<>>(
            animSequences_.emplace_back(key, loopImpl(key, std::noop_coroutine(), preElapsed, *this), true)
        ).resume();
    }
    else {
        std::get<std::coroutine_handle<>>(
            animSequences_.emplace_back(key, onceImpl(key, std::noop_coroutine(), preElapsed, *this), true)
        ).resume();
    }

    return nullptr;
}

std::coroutine_handle<> AnimController::play( const std::string& key, Milliseconds preElapsed,
    std::coroutine_handle<> seq, AnimInstance::ClipMode clipMode
) {
    const auto& animClip = clipInfo(key);

    const auto itDupl = std::ranges::find_if(insts_, [&key](const auto& pair) {
        return pair.first == key;
    });
    
    if (itDupl != insts_.end()) {
        itDupl->second.setElapsed(preElapsed);
        return resetAnimSequence(key, seq);
    }

    insts_.emplace_back(key,
        AnimInstance(pSkeleton_, &animClip, preElapsed, clipMode)
    );

    std::get<std::coroutine_handle<>>(
        animSequences_.emplace_back(key, seq, true)
    ).resume();

    return nullptr;
}

void AnimController::update(Milliseconds deltaTime) {
    for (auto& [_, __, isNew] : animSequences_) {
        isNew = false;
    }

    fsm_.pushEvent(fsm::Event::create<Milliseconds>(evAnimUpdate, deltaTime));

    fsm_.update();
    deltaTime_ = deltaTime;
    for (auto& [key, inst] : insts_) {
        inst.update(deltaTime_);
    }

    auto cur = animSequences_.begin();
    while (cur != animSequences_.end()) {
        auto& [key, coro, isNew] = *cur;
        if (!isNew) {
            coro.resume();
        }
        if (coro.done()) {
            insts_.erase(std::next(insts_.begin(), std::distance(animSequences_.begin(), cur)));
            coro.destroy();
            cur = animSequences_.erase(cur);
        }
        else {
            ++cur;
        }
    }

    for (auto& expired : expireds_) {
        expired.destroy();
    }
    expireds_.clear();
}

std::coroutine_handle<> AnimController::resetAnimSequence(
    const std::string& key, std::coroutine_handle<> animSequence
) {
    for (auto& [k, c, _] : animSequences_) {
        if (k == key) {
            auto old = c;
            c = animSequence;
            c.resume();
            return old;
        }
    }

    return nullptr;
}

std::vector<std::coroutine_handle<>> AnimController::restoreAnimSequences(
    const std::vector<std::string>& keys
) {
    auto ret = std::vector<std::coroutine_handle<>>();
    ret.reserve(animSequences_.size());

    for (auto& [k, c, _] : animSequences_) {
        const auto it = std::ranges::find_if(keys, [&k](const auto& key) { return key != k; });
        if (it != keys.end()) {
            ret.push_back(c);
            c = removeImpl(k, std::noop_coroutine(), *this);
        }
    }

    return ret;
}

TaskAnim fadeInImpl( std::string key, Milliseconds fadeDuration,
    std::coroutine_handle<> suspended, AnimController& con
) {
    auto raii = CoroRAII(suspended);

    auto accTime = Milliseconds(0.f);
    while (accTime < fadeDuration) {
        AnimConAttorney::setWeight(key, std::clamp(accTime / fadeDuration, 0.f, 1.f), con);
        co_await std::suspend_always{};
        accTime += AnimConAttorney::getDeltaTime(con);

        const auto duration = AnimConAttorney::getDuration(key, con);
        const auto elapsed = AnimConAttorney::getElapsed(key, con);
        if (elapsed >= duration) {
            AnimConAttorney::setElapsed(key, elapsed - duration, con);
        }
    }

    raii.release();
    suspended.resume();
}

TaskAnim fadeOutImpl( std::string key, Milliseconds fadeDuration,
    std::coroutine_handle<> suspended, AnimController& con
) {
    auto raii = CoroRAII(suspended);

    auto accTime = Milliseconds(0.f);

    while (accTime < fadeDuration) {
        AnimConAttorney::setWeight(key, 1.f - std::clamp(accTime / fadeDuration, 0.f, 1.f), con);
        co_await std::suspend_always{};
        accTime += AnimConAttorney::getDeltaTime(con) * AnimConAttorney::getSpeed(key, con);
    }

    raii.release();
    suspended.resume();
}

TaskAnim removeImpl(std::string key, std::coroutine_handle<> suspended, AnimController& con) {
    auto raii = CoroRAII(suspended);

    AnimConAttorney::setWeight(key, 0.f, con);
    AnimConAttorney::setSpeed(key, 0.f, con);
    co_await std::suspend_always{};

    raii.release();
    suspended.resume();
}

TaskAnim loopImpl( std::string key, std::coroutine_handle<> suspended,
    Milliseconds preElapsed, AnimController& con
) {
    auto raii = CoroRAII(suspended);

    AnimConAttorney::setWeight(key, 1.f, con);
    for (;;) {
        co_await std::suspend_always{};
    }

    raii.release();
    suspended.resume();
}

TaskAnim onceImpl( std::string key, std::coroutine_handle<> suspended,
    Milliseconds preElapsed, AnimController& con
) {
    auto raii = CoroRAII(suspended);

    AnimConAttorney::setWeight(key, 1.f, con);
    auto elapsed = AnimConAttorney::getElapsed(key, con);
    auto duration = AnimConAttorney::getDuration(key, con);
    while (elapsed < duration) {
        co_await std::suspend_always{};
        elapsed = AnimConAttorney::getElapsed(key, con);
    }

    raii.release();
    suspended.resume();
}

TaskAnim partialImpl( std::string key, std::coroutine_handle<> suspended,
    Milliseconds preElapsed, Milliseconds endPoint, AnimController& con
) {
    auto raii = CoroRAII(suspended);

    AnimConAttorney::setWeight(key, 1.f, con);
    AnimConAttorney::setElapsed(key, preElapsed, con);
    auto elapsed = preElapsed;
    while (elapsed < endPoint) {
        co_await std::suspend_always{};
        elapsed = AnimConAttorney::getElapsed(key, con);
    }

    raii.release();
    suspended.resume();
}

TaskAnim sequencialNodeImpl( const std::string& key,
    std::coroutine_handle<> suspended, AnimController& con
) {
    auto raii = CoroRAII(suspended);

    AnimConAttorney::setWeight(key, 1.f, con);
    auto elapsed = AnimConAttorney::getElapsed(key, con);
    auto duration = AnimConAttorney::getDuration(key, con);
    while (elapsed < duration) {
        co_await std::suspend_always{};
        elapsed = AnimConAttorney::getElapsed(key, con);
    }

    raii.release();
    suspended.resume();
}

TaskAnim sequencialImpl( const std::vector<std::string>& keys,
    Milliseconds preElapsed, std::coroutine_handle<> suspended, AnimController& con
) {
    auto raii = CoroRAII(suspended);

    struct Awaitable {
        bool await_ready() { return false; }
        void await_suspend(std::coroutine_handle<> suspended) {
            pAnimCon->free(
                pAnimCon->play(key, preElapsed, sequencialNodeImpl(key, suspended, *pAnimCon))
            );
            AnimConAttorney::resetFlags(key, etoi(AnimClip::Flags::Loop), *pAnimCon);
        }
        void await_resume() {}
    
        AnimController* pAnimCon;
        std::string key;
        Milliseconds preElapsed;
    };
    
    for (const auto& key : keys) {
        co_await Awaitable{
            .pAnimCon = &con,
            .key = key,
            .preElapsed = preElapsed
        };
        preElapsed = std::max(0_ms, preElapsed - AnimConAttorney::getDuration(key, con));
    }
    
    raii.release();
    suspended.resume();
}

TaskAnimSequence fadeIn( std::string key, std::string prevKey,
    Milliseconds fadeDuration, AnimController& animCon
) {
    const auto preElapsed = AnimConAttorney::getDuration(key, animCon)
        * (AnimConAttorney::getElapsed(prevKey, animCon) / AnimConAttorney::getDuration(prevKey, animCon));

    co_await FadeIn{
        .pAnimCon = &animCon, .key = key, .fadeDuration = fadeDuration, .preElapsed = preElapsed
    };

    if (animCon.clipInfo(key).flags() & AnimClip::Flags::Loop) {
        co_await Loop{ .pAnimCon = &animCon, .key = key };
    }
    else {
        co_await Once{ .pAnimCon = &animCon, .key = key };
    }
}

TaskAnimSequence fadeIn( std::string key, std::vector<std::string> possiblePrevKeys,
    Milliseconds fadeDuration, AnimController& animCon
) {
    auto pPrevKey = std::ranges::find_if(possiblePrevKeys, [&animCon](const auto& key) {
        return AnimConAttorney::playing(key, animCon);
    });

    if (pPrevKey == std::end(possiblePrevKeys)) {
        throw std::runtime_error{ "[Description] fadeIn:: the previous animation instance key doesn't exist in possiblePrevKeys." };
    }

    const auto prevKey = *pPrevKey;
    if (prevKey != key) {
        const auto preElapsed = AnimConAttorney::getDuration(key, animCon)
            * (AnimConAttorney::getElapsed(prevKey, animCon) / AnimConAttorney::getDuration(prevKey, animCon));

        co_await FadeIn{ .pAnimCon = &animCon, .key = key, .fadeDuration = fadeDuration, .preElapsed = preElapsed };
    }

    if (animCon.clipInfo(key).flags() & AnimClip::Flags::Loop) {
        co_await Loop{ .pAnimCon = &animCon, .key = key };
    }
    else {
        co_await Once{ .pAnimCon = &animCon, .key = key };
    }
}

TaskAnimSequence fadeInSequencial( std::vector<std::string> keys,
    std::string prevKey, Milliseconds fadeDuration, AnimController& animCon
) {
    auto& key = *std::begin(keys);

    const auto firstDuration = AnimConAttorney::getDuration(key, animCon);
    const auto preElapsed = firstDuration * (AnimConAttorney::getElapsed(prevKey, animCon)
        / AnimConAttorney::getDuration(prevKey, animCon));

    co_await FadeIn{
        .pAnimCon = &animCon, .key = key, .fadeDuration = fadeDuration, .preElapsed = preElapsed
    };
    co_await Sequencial{ .pAnimCon = &animCon, .keys = std::move(keys),
        .preElapsed = std::min(preElapsed + fadeDuration, firstDuration)
    };
}

TaskAnimSequence fadeInCircular( std::vector<std::string> keys, std::string prevKey,
    Milliseconds fadeDuration, AnimController& animCon
) {
    auto& key = *std::begin(keys);

    const auto firstDuration = AnimConAttorney::getDuration(key, animCon);
    auto preElapsed = firstDuration * (AnimConAttorney::getElapsed(prevKey, animCon)
        / AnimConAttorney::getDuration(prevKey, animCon));

    co_await FadeIn{
        .pAnimCon = &animCon, .key = key, .fadeDuration = fadeDuration, .preElapsed = preElapsed
    };

    for (;;) {
        co_await Sequencial{ .pAnimCon = &animCon, .keys = keys,
            .preElapsed = std::min(preElapsed + fadeDuration, firstDuration)
        };
        preElapsed = 0_ms;
        fadeDuration = 0_ms;
    }
}

TaskAnimSequence fadeOut(std::string key, Milliseconds fadeDuration, AnimController& animCon) {
    co_await FadeOut{ .pAnimCon = &animCon, .key = key, .fadeDuration = fadeDuration };
}

TaskAnimSequence fadeOutSelect( std::vector<std::string> keys,
    Milliseconds fadeDuration, AnimController& animCon
) {
    co_await FadeOutSelect{
        .pAnimCon = &animCon, .keys = std::move(keys), .fadeDuration = fadeDuration
    };
}

TaskAnimSequence sequencial( std::vector<std::string> keys,
    AnimController& animCon, Milliseconds preElapsed
) {
    co_await Sequencial{ .pAnimCon = &animCon, .keys = std::move(keys), .preElapsed = preElapsed };
}

TaskAnimSequence circular( std::vector<std::string> keys,
    AnimController& animCon, Milliseconds preElapsed
) {
    for (;;) {
        co_await Sequencial{ .pAnimCon = &animCon, .keys = keys, .preElapsed = preElapsed };
        preElapsed = 0_ms;
    }
}

TaskAnimSequence softCircular( std::vector<std::string> keys, std::string prevKey,
    Milliseconds fadeDuration, AnimController& animCon
) {
    auto preElapsed = AnimConAttorney::getDuration(*std::begin(keys), animCon)
        * ( AnimConAttorney::getElapsed(prevKey, animCon)
            / AnimConAttorney::getDuration(prevKey, animCon)
        );

    for (;;) {
        for (auto& key : keys) {
            const auto duration = AnimConAttorney::getDuration(key, animCon);

            if (key == prevKey) {
                co_await Partial{ .pAnimCon = &animCon, .key = key,
                    .endPoint = duration - fadeDuration,
                    .preElapsed = 0_ms
                };
                continue;
            }

            fadeOut(std::move(prevKey), fadeDuration, animCon);
            co_await FadeIn{
                .pAnimCon = &animCon, .key = key, .fadeDuration = fadeDuration, .preElapsed = preElapsed
            };
            prevKey = key;
            
            co_await Partial{ .pAnimCon = &animCon, .key = key,
                .endPoint = duration - fadeDuration,
                .preElapsed = std::min(AnimConAttorney::getElapsed(key, animCon), duration)
            };

            preElapsed = 0_ms;
        }
    }
}

AnimSystem::AnimSystem( gfx::d3d12::D3D12Device& device,
    const gfx::d3d12::RootSignature& root
) : shaderAnimInterpolation_(device, root, gfx::d3d12::ShaderAnimInterpolation::Config{
        .maxKeyFrameCnt = 64'000u
    }), computePassAnimInterpolation_(device, shaderAnimInterpolation_),
    shaderMatMul_(device, root, gfx::d3d12::ShaderMatMul::Config{
        .maxMatrixCnt = 64'000u
    }), computePassMatMul_(device, shaderMatMul_),
    boneXformCache_(),
    suspendedTasks_(),
    fence_(device) {
    suspendedTasks_.reserve(10'000'000u);
}

// on anim controller's play => register anim instances' matrices on anim system
void AnimSystem::update( gfx::d3d12::D3D12CmdQueue& cmdQueue,
    gfx::d3d12::D3D12GfxCmdList& cmdList, Milliseconds deltaTime
) {
    // update timing and weights for all anim instances
    // and remove expired anim instances
    for (auto& pAnimCon : components<AnimController>()) {
        pAnimCon->update(deltaTime);
    }

    clearXformCache();

    // calculate transform for all anim instances
    // pass 1: calculate local transforms
    for (auto& pAnimCon : components<AnimController>()) {
        for (auto& [key, inst] : AnimConAttorney::getInstances(*pAnimCon)) {
            // add transforms to cache
            suspendedTasks_.push_back(inst.calcLocals(*this));
        }
    }

    cmdList.get()->SetComputeRootSignature(shaderAnimInterpolation_.rootSiganture().get().Get());
    shaderAnimInterpolation_.bindRootParams(cmdList);
    computePassAnimInterpolation_.preCompute(cmdList);
    computePassAnimInterpolation_.compute(cmdList);
    computePassAnimInterpolation_.postCompute(cmdList);

    cmdList.close();
    cmdQueue.execute(cmdList);
    fence_.signal(cmdQueue);
    fence_.wait();
    computePassAnimInterpolation_.postExecution();

    boneXformCache_ = std::move(computePassAnimInterpolation_.resultMatrices());

    // store the result matrices in the anim instance
    for (auto& suspended : suspendedTasks_) {
        suspended.resume();
    }
    suspendedTasks_.clear();

    // pass 2: calculate world transforms
    for (auto& pAnimCon : components<AnimController>()) {
        for (auto& [key, inst] : AnimConAttorney::getInstances(*pAnimCon)) {
            inst.calcWorlds(*this);
        }
    }

    // pass 3: calculate final transforms
    for (auto& pAnimCon : components<AnimController>()) {
        for (auto& [key, inst] : AnimConAttorney::getInstances(*pAnimCon)) {
            suspendedTasks_.push_back(inst.calcFinals(*this));
        }
    }

    cmdList.reset();

    cmdList.get()->SetComputeRootSignature(shaderMatMul_.rootSiganture().get().Get());
    shaderMatMul_.bindRootParams(cmdList);
    computePassMatMul_.preCompute(cmdList);
    computePassMatMul_.compute(cmdList);
    computePassMatMul_.postCompute(cmdList);

    cmdList.close();
    cmdQueue.execute(cmdList);
    fence_.signal(cmdQueue);
    fence_.wait();
    computePassMatMul_.postExecution();

    boneXformCache_ = std::move(computePassMatMul_.resultMatrices());

    // store the result matrices in the anim instance
    for (auto& suspended : suspendedTasks_) {
        suspended.resume();
    }
    suspendedTasks_.clear();

    cmdList.reset();
}
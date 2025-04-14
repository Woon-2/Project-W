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

AnimClip AnimClip::loadClipFromStream(std::istream& in) {
    AnimClip animClip;

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
        keyFrames.resize(keyFrameCnt);
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

                auto& keyFrame = animClip.keyFrames_[boneIdx][keyFrameIdx];

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

    return animClip;
}

std::vector<AnimClip> AnimClip::loadClipsFromStream(std::istream& in) {
    std::vector<AnimClip> animClips;

    char pstrToken[64] = { '\0' };

    std::uint8_t nStrLength = 0;
    std::size_t nReads = 0;

    readStream(in, nStrLength);
    readStream(in, pstrToken, nStrLength);
    if (std::strcmp(pstrToken, "<AnimationClips:>")) {
        throw std::runtime_error("expected AnimationClips token but got: " + std::string(pstrToken));
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

        animClips.push_back(AnimClip::loadClipFromStream(in));
    }

    readStream(in, nStrLength);
    readStream(in, pstrToken, nStrLength);
    if (std::strcmp(pstrToken, "</AnimationClips>")) {
        throw std::runtime_error("expected AnimationClips end token but got: " + std::string(pstrToken));
    }

    return animClips;
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

AnimInstance::AnimInstance(const Skeleton* pSkeleton, const AnimClip* pAnimClip)
    : boneXformCache_(pSkeleton->bones().size()),
    keyFrameCache_(), pAnimClip_(pAnimClip), pSkeleton_(pSkeleton),
    elapsedTime_(0_ms), stage_(Stage::None), speed_(1.f), weight_(0.f) {
    if (!pAnimClip_) {
        throw std::runtime_error("AnimInstance requires a valid AnimClip.");
    }

    if (!pSkeleton_) {
        throw std::runtime_error("AnimInstance requires a valid Skeleton.");
    }

    if (pAnimClip_->boneCnt() != pSkeleton_->bones().size()) {
        throw std::runtime_error("AnimClip bone count does not match Skeleton bone count.");
    }

    // initialize keyFrameCache_ with the first key frame of each bone
    keyFrameCache_.reserve(pSkeleton_->bones().size());
    for (std::size_t i = 0; i < pSkeleton->bones().size(); ++i) {
        keyFrameCache_.push_back(pAnimClip_->keyFrameBegin(static_cast<BoneIdx>(i)));
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

        if (!(pAnimClip_->flags() & AnimClip::Flags::Loop)) {
            return;
        }
        elapsedTime_ -= pAnimClip_->duration();

        // reset key frame cache
        for (std::size_t i = 0; i < pSkeleton_->bones().size(); ++i) {
            keyFrameCache_[i] = pAnimClip_->keyFrameBegin(static_cast<BoneIdx>(i));
        }
    }

    for (std::size_t i = 0; i < pSkeleton_->bones().size(); ++i) {
        auto& keyFrame = keyFrameCache_[i];
        auto nextKeyFrame = std::next(keyFrame);

        // every key frame must end with sentinal key frame
        // which has time = duration
        while (nextKeyFrame->time * 1000.f <= elapsedTime_.count()) {
            keyFrame = nextKeyFrame;
            ++nextKeyFrame;
        }
    }

    stage_ = Stage::CalcLocal;
}

TaskCompute AnimInstance::calcLocals(AnimSystem& animSystem) {
    if (stage_ != Stage::CalcLocal) {
        throw std::runtime_error(
            "[Description] AnimInstance::calcLocals: expected AnimInstance stage: "
            + std::to_string(etoi(Stage::CalcLocal)) +
            " but got: " + std::to_string(etoi(stage_))
        );
    }

    // calculate local transforms for each bone
    // with compute shader

    // lhsFrame, rhsFrame, ratio => localMat
    std::vector<std::size_t> indices;
    indices.reserve(keyFrameCache_.size());

    for (auto& lhsFrame : keyFrameCache_) {
        auto rhsFrame = std::next(lhsFrame);

        auto ratio = (elapsedTime_.count() - lhsFrame->time) / (rhsFrame->time - lhsFrame->time);
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

    auto pRootBone = pSkeleton_->root();

    traverseBone(*pRootBone);

    stage_ = Stage::CalcFinal;
}

void AnimInstance::traverseBone(const Bone& bone) {
    boneXformCache_[bone.boneIdx()] *= bone.toParentMatrix();

    for (const auto& child : bone.children()) {
        traverseBone(*child);
    }
}

TaskCompute AnimInstance::calcFinals(AnimSystem& animSystem) {
    if (stage_ != Stage::CalcFinal) {
        throw std::runtime_error(
            "[Description] AnimInstance::calcFinals: expected AnimInstance stage: "
            + std::to_string(etoi(Stage::CalcFinal)) +
            " but got: " + std::to_string(etoi(stage_))
        );
    }

    // calculate final transforms for each bone
    // with compute shader
    std::vector<std::size_t> indices;
    indices.reserve(boneXformCache_.size());

    for (std::size_t i = 0u; i < boneXformCache_.size(); ++i) {
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

void AnimController::play(const std::string& key) {
    const auto& animClip = clipInfo(key);
    insts_.emplace_back(key,
        AnimInstance(pSkeleton_, &animClip)
    );

    if (animClip.flags() & AnimClip::Flags::Loop) {
        animSequences_.emplace_back(key, loopImpl(key, *this));
    }
    else {
        animSequences_.emplace_back(key, onceImpl(key, *this));
    }
}

void AnimController::play(const std::string& key, std::coroutine_handle<> seq) {
    const auto& animClip = clipInfo(key);
    insts_.emplace_back(key,
        AnimInstance(pSkeleton_, &animClip)
    );

    animSequences_.emplace_back(key, seq);
}

void AnimController::update(Milliseconds deltaTime) {
    deltaTime_ = deltaTime;
    for (auto& [key, inst] : insts_) {
        inst.update(deltaTime_);
    }

    auto cur = animSequences_.begin();
    while (cur != animSequences_.end()) {
        auto& [key, coro] = *cur;
        coro.resume();
        if (coro.done()) {
            std::erase_if(insts_, [&key](const auto& pair) { return pair.first == key; });
            coro.destroy();
            cur = animSequences_.erase(cur);
        }
        else {
            ++cur;
        }
    }
}

void AnimController::setAnimSequence(const std::string& key, std::coroutine_handle<> animSequence) {
    for (auto& [k, c] : animSequences_) {
        if (k == key) {
            c = animSequence;
            break;
        }
    }
}

TaskAnim fadeInImpl(std::string key, Milliseconds fadeDuration,
    std::coroutine_handle<> suspended, AnimController& con
) {
    auto elapsed = AnimConAttorney::getElapsed(key, con);
    while (elapsed < fadeDuration) {
        AnimConAttorney::setWeight(key, std::clamp(elapsed / fadeDuration, 0.f, 1.f), con);
        co_await std::suspend_always{};
        elapsed = AnimConAttorney::getElapsed(key, con);
    }
    con.setAnimSequence(key, suspended);
}

TaskAnim fadeOutImpl(std::string key, Milliseconds fadeDuration, AnimController& con) {
    auto elapsed = Milliseconds(0.f);

    while (elapsed < fadeDuration) {
        AnimConAttorney::setWeight(key, 1.f - std::clamp(elapsed / fadeDuration, 0.f, 1.f), con);
        co_await std::suspend_always{};
        elapsed += AnimConAttorney::getDeltaTime(con) * AnimConAttorney::getSpeed(key, con);
    }
}

TaskAnim loopImpl(std::string key, AnimController& con) {
    AnimConAttorney::setWeight(key, 1.f, con);
    for (;;) {
        co_await std::suspend_always{};
    }
}

TaskAnim onceImpl(std::string key, AnimController& con) {
    AnimConAttorney::setWeight(key, 1.f, con);
    auto elapsed = AnimConAttorney::getElapsed(key, con);
    auto duration = AnimConAttorney::getDuration(key, con);
    while (elapsed < duration) {
        co_await std::suspend_always{};
        elapsed = AnimConAttorney::getElapsed(key, con);
    }
}

TaskAnim fadeIn(std::string key, Milliseconds fadeDuration, AnimController& animCon) {
    co_await FadeIn{ .pAnimCon = &animCon, .key = key, .fadeDuration = fadeDuration };
    if (animCon.clipInfo(key).flags() & AnimClip::Flags::Loop) {
        co_await Loop{ .pAnimCon = &animCon, .key = key };
    }
    else {
        co_await Once{ .pAnimCon = &animCon, .key = key };
    }
}

TaskAnim fadeOut(std::string key, Milliseconds fadeDuration, AnimController& animCon) {
    co_await FadeOut{ .pAnimCon = &animCon, .key = key, .fadeDuration = fadeDuration };
}

AnimSystem::AnimSystem( gfx::d3d12::D3D12Device& device,
    const gfx::d3d12::RootSignature& root
) : shaderMatMul_(device, root, gfx::d3d12::ShaderMatMul::Config{
        .maxMatrixCnt = 10'000'000u
    }), computePassMatMul_(device, shaderMatMul_),
    shaderAnimInterpolation_(device, root, gfx::d3d12::ShaderAnimInterpolation::Config{
        .maxKeyFrameCnt = 10'000'000u
    }), computePassAnimInterpolation_(device, shaderAnimInterpolation_),
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
    for (auto& pAnimCon : components<AnimController>()) {
        // pass 1: calculate local transforms
        for (auto& [key, inst] : AnimConAttorney::getInstances(*pAnimCon)) {
            // add transforms to cache
            suspendedTasks_.push_back(inst.calcLocals(*this));
        }

        cmdList.reset();

        computePassMatMul_.preCompute(cmdList);
        computePassMatMul_.compute(cmdList);
        computePassMatMul_.postCompute(cmdList);

        cmdList.close();
        cmdQueue.execute(cmdList);
        fence_.signal(cmdQueue);
        fence_.wait();

        boneXformCache_ = std::move(computePassMatMul_.resultMatrices());

        // store the result matrices in the anim instance
        for (auto& suspended : suspendedTasks_) {
            suspended.resume();
        }
        suspendedTasks_.clear();

        // pass 2: calculate world transforms
        for (auto& [key, inst] : AnimConAttorney::getInstances(*pAnimCon)) {
            inst.calcWorlds(*this);
        }

        // pass 3: calculate final transforms
        for (auto& [key, inst] : AnimConAttorney::getInstances(*pAnimCon)) {
            suspendedTasks_.push_back(inst.calcFinals(*this));
        }

        cmdList.reset();

        computePassMatMul_.preCompute(cmdList);
        computePassMatMul_.compute(cmdList);
        computePassMatMul_.postCompute(cmdList);

        cmdList.close();
        cmdQueue.execute(cmdList);
        fence_.signal(cmdQueue);
        fence_.wait();

        boneXformCache_ = std::move(computePassMatMul_.resultMatrices());

        // store the result matrices in the anim instance
        for (auto& suspended : suspendedTasks_) {
            suspended.resume();
        }
        suspendedTasks_.clear();
    }
}
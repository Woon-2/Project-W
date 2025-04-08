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
    animClip.duration_ = duration;

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
    elapsedTime_(0.0f), stage_(Stage::None) {
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

void AnimInstance::update(float deltaTime) {
    if (stage_ != Stage::None) {
        throw std::runtime_error(
            "[Description] AnimInstance::update: expected AnimInstance stage: "
            + std::to_string(etoi(Stage::None)) +
            " but got: " + std::to_string(etoi(stage_))
        );
    }

    elapsedTime_ += deltaTime;

    if (elapsedTime_ > pAnimClip_->duration()) {

        if (pAnimClip_->flags() & AnimClip::Flags::Loop) {
            elapsedTime_ = std::fmod(elapsedTime_, pAnimClip_->duration());

            // reset key frame cache
            for (std::size_t i = 0; i < pSkeleton_->bones().size(); ++i) {
                keyFrameCache_[i] = pAnimClip_->keyFrameBegin(static_cast<BoneIdx>(i));
            }
        }
        else {
            // do something, e.g., stop the animation or clamp to duration
            return;
        }
    }

    for (std::size_t i = 0; i < pSkeleton_->bones().size(); ++i) {
        auto& keyFrame = keyFrameCache_[i];
        auto nextKeyFrame = std::next(keyFrame);

        // every key frame must end with sentinal key frame
        // which has time = duration
        while (nextKeyFrame->time <= elapsedTime_) {
            keyFrame = nextKeyFrame;
            ++nextKeyFrame;
        }
    }

    stage_ = Stage::CalcLocal;
}

void AnimInstance::calcLocals() {
    if (stage_ != Stage::CalcLocal) {
        throw std::runtime_error(
            "[Description] AnimInstance::calcLocals: expected AnimInstance stage: "
            + std::to_string(etoi(Stage::CalcLocal)) +
            " but got: " + std::to_string(etoi(stage_))
        );
    }

    // calculate local transforms for each bone
    // with compute shader

    // lhsMat, rhsMat, ratio => localMat

    stage_ = Stage::CalcWorld;
}

void AnimInstance::calcWorlds() {
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

void AnimInstance::calcFinals() {
    if (stage_ != Stage::CalcFinal) {
        throw std::runtime_error(
            "[Description] AnimInstance::calcFinals: expected AnimInstance stage: "
            + std::to_string(etoi(Stage::CalcFinal)) +
            " but got: " + std::to_string(etoi(stage_))
        );
    }

    // calculate final transforms for each bone
    // with compute shader

    stage_ = Stage::None;
}
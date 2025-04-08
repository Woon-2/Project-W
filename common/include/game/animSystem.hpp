#ifndef __AnimSystem_HPP
#define __AnimSystem_HPP

#ifndef DXMATH_VEC_UTIL
#define DXMATH_VEC_UTIL
#endif
#ifndef DXMATH_MAT_UTIL
#define DXMATH_MAT_UTIL
#endif
#ifndef DXMATH_QUAT_UTIL
#define DXMATH_QUAT_UTIL
#endif
#include "mathUtil.hpp"
#include "enumUtil.hpp"

#include <string>
#include <vector>
#include <filesystem>
#include <fstream>
#include <map>

class Bone {
public:
    friend class Skeleton;

    Bone(const Skeleton* pSkeleton = nullptr)
        : toParent_(), toLocal_(), name_(), children_(), pSkeleton_(pSkeleton),
        boneIdx_(-1) {}
    Bone(const Bone& other) = delete;
    Bone(Bone&& other) noexcept;
    Bone& operator=(const Bone& other) = delete;
    Bone& operator=(Bone&& other) noexcept;
    ~Bone() = default;

    void addChild(Bone* child);
    mu::Mat4x4 MU_CALLCONV toParentMatrix() const noexcept {
        return toParent_;
    }
    mu::Mat4x4 MU_CALLCONV toLocalMatrix() const noexcept {
        return toLocal_;
    }
    auto& children() noexcept { return children_; }
    const auto& children() const noexcept { return children_; }
    int boneIdx() const noexcept { return boneIdx_; }

private:
    mu::Mat4x4 toParent_;
    mu::Mat4x4 toLocal_;
    std::string name_;
    std::vector<Bone*> children_;
    const Skeleton* pSkeleton_;
    int boneIdx_;
};

class Skeleton {
public:
    Skeleton()
        : boneStorage_(), pRoot_(nullptr) {}

    ~Skeleton() = default;
    Skeleton(const Skeleton& other) = delete;
    Skeleton(Skeleton&& other) noexcept;
    Skeleton& operator=(const Skeleton& other) = delete;
    Skeleton& operator=(Skeleton&& other) noexcept;

    static Skeleton loadHierarchyFromFile(const std::filesystem::path& path);
    static Skeleton loadHierarchyFromStream(std::istream& in);

    auto& bones() noexcept { return boneStorage_; }
    const auto& bones() const noexcept { return boneStorage_; }
    Bone* root() noexcept { return pRoot_; }
    const Bone* root() const noexcept { return pRoot_; }

protected:
    static void loadBonesFromStream(std::istream& in, Bone& bone, Skeleton& skeleton);

    std::vector<Bone> boneStorage_;
    Bone* pRoot_;
};

struct KeyFrame {
    mu::Vec3 pos;
    mu::NQuat rot;
    mu::Vec3 scale;
    float time;
};

using BoneIdx = int;

class AnimClip {
public:
    enum class Flags : int {
        Loop = 0x01,
        RootMotion = 0x02
    };

    AnimClip() = default;

    static AnimClip loadClipFromStream(std::istream& in);
    static std::vector<AnimClip> loadClipsFromStream(std::istream& in);

    const KeyFrame& keyFrame(BoneIdx boneIdx, std::size_t keyFrameIdx) const {
        return keyFrames_[boneIdx][keyFrameIdx];
    }

    std::vector<KeyFrame>::const_iterator keyFrameBegin(BoneIdx boneIdx) const {
        return keyFrames_[boneIdx].begin();
    }

    std::vector<KeyFrame>::const_iterator keyFrameEnd(BoneIdx boneIdx) const {
        return keyFrames_[boneIdx].end();
    }

    std::size_t keyFrameCnt(BoneIdx boneIdx) const {
        return keyFrames_[boneIdx].size();
    }

    std::size_t boneCnt() const {
        return keyFrames_.size();
    }

    const std::string& name() const noexcept {
        return name_;
    }

    float duration() const noexcept {
        return duration_;
    }

    int flags() const noexcept {
        return flags_;
    }

private:
    std::vector< std::vector<KeyFrame> > keyFrames_; // [boneIdx][keyFrameIdx]
    std::string name_;
    float duration_;
    int flags_;
};

struct SkeletonAnimClipsPair {
    Skeleton skeleton;
    std::vector<AnimClip> animClips;
};

SkeletonAnimClipsPair loadSkeletonAndAnimClipFromFile(
    const std::filesystem::path& animBinaryPath
);

DEFINE_ENUM_LOGICAL_OP_ALL(AnimClip::Flags);

class AnimInstance {
public:
    enum class Stage {
        None,
        CalcLocal,
        CalcWorld,
        CalcFinal
    };

    AnimInstance(const Skeleton* pSkeleton, const AnimClip* pAnimClip);

    void update(float deltaTime);
    void calcLocals();
    void calcWorlds();
    void calcFinals();

    auto& boneXformCache() noexcept { return boneXformCache_; }
    const auto& boneXformCache() const noexcept { return boneXformCache_; }
    auto& keyFrameCache() noexcept { return keyFrameCache_; }
    const auto& keyFrameCache() const noexcept { return keyFrameCache_; }

private:
    void traverseBone(const Bone& bone);

    std::vector<mu::Mat4x4> boneXformCache_;
    std::vector< std::vector<KeyFrame>::const_iterator > keyFrameCache_;
    const AnimClip* pAnimClip_;
    const Skeleton* pSkeleton_;
    float elapsedTime_;
    Stage stage_;
};

class AnimSystem {
public:

private:
};

#endif // __AnimSystem_HPP
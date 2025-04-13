#ifndef __AnimSystem_HPP
#define __AnimSystem_HPP

#include "FSM.hpp"
#include "ecs.hpp"

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

    std::size_t keyFrameCnt(BoneIdx boneIdx) const { return keyFrames_[boneIdx].size(); }
    std::size_t boneCnt() const { return keyFrames_.size(); }
    const std::string& name() const noexcept { return name_; }
    Milliseconds duration() const noexcept { return duration_; }
    int flags() const noexcept { return flags_; }

private:
    std::vector< std::vector<KeyFrame> > keyFrames_; // [boneIdx][keyFrameIdx]
    std::string name_;
    Milliseconds duration_;
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
    friend class AnimController;

    enum class Stage {
        None,
        CalcLocal,
        CalcWorld,
        CalcFinal
    };

    AnimInstance(const Skeleton* pSkeleton, const AnimClip* pAnimClip);

    void update(Milliseconds deltaTime);
    void calcLocals();
    void calcWorlds();
    void calcFinals();

    void setSpeed(float speed) noexcept { speed_ = speed; }
    void setWeight(float weight) noexcept { weight_ = weight; }
    void setElapsed(Milliseconds elapsed) noexcept { elapsedTime_ = elapsed; }

    Milliseconds elapsed() const noexcept { return elapsedTime_; }
    float speed() const noexcept { return speed_; }

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
    Milliseconds elapsedTime_;
    Stage stage_;
    float speed_;
    float weight_;
};

struct PromiseAnim;

struct TaskAnim : std::coroutine_handle<PromiseAnim> {
    using promise_type = PromiseAnim;
};

struct PromiseAnim {
    TaskAnim get_return_object() {
        return TaskAnim{ std::coroutine_handle<PromiseAnim>::from_promise(*this) };
    }
    std::suspend_always initial_suspend() { return {}; }
    std::suspend_always final_suspend() noexcept { return {}; }
    void unhandled_exception() { throw; }
    void return_void() {}
};

class AnimController : public ecs::Component {
public:
    ENABLE_COMPONENT(AnimController);
    friend struct AnimConAttorney;

    AnimController(const ecs::Entity& entity) NOEXCEPT
        : ecs::Component(entity), clipMap_(), insts_(), animSequences_(),
        deltaTime_(0_ms), pSkeleton_(nullptr) {}

    void addClip(const std::string& key, const AnimClip* pClip) {
        clipMap_.try_emplace(key, pClip);
    }

    void play(const std::string& key);
    void play(const std::string& key, std::coroutine_handle<> seq);

    void update(Milliseconds deltaTime);
    void setAnimSequence(const std::string& key, std::coroutine_handle<> animSequence);

    const AnimClip& clipInfo(const std::string& key) const {
        return *clipMap_.at(key);
    }

    const Skeleton& skeleton() const { return *pSkeleton_; }
    void setSkeleton(const Skeleton* pSkeleton) { pSkeleton_ = pSkeleton; }

private:
    std::unordered_map<std::string, const AnimClip*> clipMap_;
    std::list<std::pair<std::string, AnimInstance>> insts_;
    std::list<std::pair<std::string, std::coroutine_handle<>>> animSequences_;
    Milliseconds deltaTime_;
    const Skeleton* pSkeleton_;
};

struct AnimConAttorney {
    static Milliseconds getDeltaTime(const AnimController& con) {
        return con.deltaTime_;
    }

    static Milliseconds getElapsed(const std::string& key, const AnimController& con) {
        auto it = std::ranges::find_if(con.insts_, [&key](const auto& pair) { return pair.first == key; });
        return (it != con.insts_.end()) ? it->second.elapsed() : Milliseconds(0.f);
    }

    static Milliseconds getDuration(const std::string& key, const AnimController& con) {
        auto it = con.clipMap_.find(key);
        return (it != con.clipMap_.end()) ? it->second->duration() : Milliseconds(0.f);
    }

    static float getSpeed(const std::string& key, const AnimController& con) {
        auto it = std::ranges::find_if(con.insts_, [&key](const auto& pair) { return pair.first == key; });
        return (it != con.insts_.end()) ? it->second.speed() : 1.f;
    }

    static auto& getInstances(AnimController& con) {
        return con.insts_;
    }
    
    static void setWeight(const std::string& key, float weight, AnimController& con) {
        for (auto& [k, inst] : con.insts_) {
            if (k == key) {
                inst.setWeight(weight);
                break;
            }
        }
    }

    static void setElapsed(const std::string& key, Milliseconds elapsed, AnimController& con) {
        for (auto& [k, inst] : con.insts_) {
            if (k == key) {
                inst.setElapsed(elapsed);
                break;
            }
        }
    }
};

TaskAnim fadeInImpl(std::string key, Milliseconds fadeDuration,
    std::coroutine_handle<> suspended, AnimController& con
);
TaskAnim fadeOutImpl(std::string key, Milliseconds fadeDuration, AnimController& con);
TaskAnim loopImpl(std::string key, AnimController& con);
TaskAnim onceImpl(std::string key, AnimController& con);

struct FadeIn {
    bool await_ready() { return false; }
    void await_suspend(std::coroutine_handle<> suspended) {
        pAnimCon->play(key, fadeInImpl(key, fadeDuration, suspended, *pAnimCon));
    }
    void await_resume() {}

    AnimController* pAnimCon;
    std::string key;
    Milliseconds fadeDuration;
};

struct FadeOut {
    bool await_ready() { return false; }
    void await_suspend(std::coroutine_handle<> suspended) {
        pAnimCon->setAnimSequence(key, fadeOutImpl(key, fadeDuration, *pAnimCon));
        suspended.destroy();
    }
    void await_resume() {}

    AnimController* pAnimCon;
    std::string key;
    Milliseconds fadeDuration;
};

struct Loop {
    bool await_ready() { return false; }
    void await_suspend(std::coroutine_handle<> suspended) {
        pAnimCon->setAnimSequence(key, loopImpl(key, *pAnimCon));
        suspended.destroy();
    }
    void await_resume() {}

    AnimController* pAnimCon;
    std::string key;
};

struct Once {
    bool await_ready() { return false; }
    void await_suspend(std::coroutine_handle<> suspended) {
        pAnimCon->setAnimSequence(key, onceImpl(key, *pAnimCon));
        suspended.destroy();
    }
    void await_resume() {}

    AnimController* pAnimCon;
    std::string key;
};

TaskAnim fadeIn(std::string key, Milliseconds fadeDuration, AnimController& animCon);
TaskAnim fadeOut(std::string key, Milliseconds fadeDuration, AnimController& animCon);

class AnimSystem : public ecs::System<AnimController>{
public:
    void update(Milliseconds deltaTime);

private:
};

#endif // __AnimSystem_HPP
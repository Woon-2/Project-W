#ifndef __AnimSystem_HPP
#define __AnimSystem_HPP

#include "FSM.hpp"
#include "ecs.hpp"

#include "d3d12util/d3d12ComputePass.hpp"

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

using BoneIdx = int;

using KeyFrame = gfx::d3d12::KeyFrame;

class AnimClip {
public:
    enum class Flags : int {
        Loop = 0x01,
        RootMotion = 0x02
    };

    AnimClip() = default;

    static void loadKeyFrameClipFromStream(std::istream& in, AnimClip& animClip);
    static void loadPresampledClipFromStream(std::istream& in, AnimClip& animClip);
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

    mu::Mat4x4 MU_CALLCONV sample(BoneIdx boneIdx, Milliseconds elapsed) const;
    std::size_t keyFrameCnt(BoneIdx boneIdx) const { return keyFrames_[boneIdx].size(); }
    std::size_t sampleCnt() const { return samples_[0].size(); }
    std::size_t boneCnt() const { return keyFrames_.size(); }
    const std::string& name() const { return name_; }
    Milliseconds duration() const { return duration_; }
    Milliseconds sampleInterval() const {
        return duration_ / static_cast<float>(sampleCnt());
    }
    int flags() const noexcept { return flags_; }
    const auto& presampleData() const { return samples_; }

private:
    std::vector< std::vector<KeyFrame> > keyFrames_; // [boneIdx][keyFrameIdx]
    std::vector< std::vector<mu::Mat4x4> > samples_; // [boneIdx][sampleIdx]
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

class AnimSystem;

struct PromiseCompute;

struct TaskCompute : std::coroutine_handle<PromiseCompute> {
    using promise_type = PromiseCompute;
};

struct PromiseCompute {
    TaskCompute get_return_object() {
        return TaskCompute{ std::coroutine_handle<PromiseCompute>::from_promise(*this) };
    }
    std::suspend_never initial_suspend() { return {}; }
    std::suspend_never final_suspend() noexcept { return {}; }
    void unhandled_exception() { throw; }
    void return_void() {}
};

class AnimInstance {
public:
    enum class Stage {
        None,
        CalcLocal,
        CalcWorld
    };

    enum class ClipMode {
        KeyFrame,
        Presampled
    };

    AnimInstance(const Skeleton* pSkeleton, const AnimClip* pAnimClip, ClipMode clipMode);

    void update(Milliseconds deltaTime);
    TaskCompute calcLocals(AnimSystem& animSystem);
    void calcWorlds(AnimSystem& animSystem);

    void setSpeed(float speed) noexcept { speed_ = speed; }
    void setWeight(float weight) noexcept { weight_ = weight; }

    Milliseconds elapsed() const noexcept { return elapsedTime_; }
    float speed() const noexcept { return speed_; }
    float weight() const noexcept { return weight_; }

    auto& boneXformCache() noexcept { return boneXformCache_; }
    const auto& boneXformCache() const noexcept { return boneXformCache_; }
    auto& keyFrameCache() noexcept { return keyFrameCache_; }
    const auto& keyFrameCache() const noexcept { return keyFrameCache_; }

    const Skeleton* skeleton() const noexcept { return pSkeleton_; }
    const AnimClip* animClip() const noexcept { return pAnimClip_; }

private:
    void traverseBone(const Bone& bone, const mu::Mat4x4& parentXform = mu::Mat4x4());

    std::vector<mu::Mat4x4> boneXformCache_;
    std::vector< std::vector<KeyFrame>::const_iterator > keyFrameCache_;
    const AnimClip* pAnimClip_;
    const Skeleton* pSkeleton_;
    Milliseconds elapsedTime_;
    Stage stage_;
    float speed_;
    float weight_;
    ClipMode clipMode_;
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

    AnimController(const ecs::Entity& entity, const std::string& name) NOEXCEPT
        : ecs::Component(entity), fsm_(name), clipMap_(), insts_(), animSequences_(),
        deltaTime_(0_ms), pSkeleton_(nullptr) {}

    void addClip(const std::string& key, const AnimClip* pClip) {
        clipMap_.try_emplace(key, pClip);
    }

    void play(const std::string& key, AnimInstance::ClipMode clipMode = AnimInstance::ClipMode::KeyFrame);
    void play( const std::string& key, std::coroutine_handle<> seq,
        AnimInstance::ClipMode clipMode = AnimInstance::ClipMode::KeyFrame
    );

    void update(Milliseconds deltaTime);
    void setAnimSequence(const std::string& key, std::coroutine_handle<> animSequence);

    const AnimClip& clipInfo(const std::string& key) const {
        return *clipMap_.at(key);
    }

    const Skeleton& skeleton() const { return *pSkeleton_; }
    void setSkeleton(const Skeleton* pSkeleton) { pSkeleton_ = pSkeleton; }

    const auto& instances() const { return insts_; }

    auto& fsm() { return fsm_; }
    const auto& fsm() const { return fsm_; }

private:
    fsm::FSM fsm_;
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
    AnimSystem(gfx::d3d12::D3D12Device& device, const gfx::d3d12::RootSignature& root);
    // the cmdList must be open before calling this function,
    // and it does not close the cmdList after executing the compute pass
    void update(gfx::d3d12::D3D12CmdQueue& cmdQueue,
        gfx::d3d12::D3D12GfxCmdList& cmdList, Milliseconds deltaTime
    );

    std::size_t MU_CALLCONV addKeyFramePair(const KeyFrame& lhs, const KeyFrame& rhs, float ratio) {
        return computePassAnimInterpolation_.addKeyFramePair(lhs, rhs, ratio);
    }

    mu::Mat4x4 MU_CALLCONV getXform(std::size_t idx) const {
        return boneXformCache_.at(idx);
    }

    void clearXformCache() {
        boneXformCache_.clear();
    }

private:
    gfx::d3d12::ShaderAnimInterpolation shaderAnimInterpolation_;
    gfx::d3d12::cp::AnimInterpolation computePassAnimInterpolation_;

    std::vector<mu::Mat4x4> boneXformCache_;
    std::vector<std::coroutine_handle<>> suspendedTasks_;
    gfx::d3d12::Fence fence_;
};

#endif // __AnimSystem_HPP
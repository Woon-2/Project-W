#ifndef __animation_HPP
#define __animation_HPP

struct AnimFrame {
	mu::Vec3 translation;
	mu::NQuat rotation;
	mu::Vec3 scale;
	Seconds time;
};

mu::Mat4x4 MU_CALLCONV convertAnimFrameToMatrix(AnimFrame frame);

struct Skeleton;

struct Bone {
	mu::Mat4x4 toLocal;
	mu::Mat4x4 toParent;
	std::string name;
	std::vector<Bone*> children;
	Skeleton* skeleton;
	i32t boneIdx;
};

enum class SkeletonEnumeration {
	Humanoid,
	SIZE
};

struct Skeleton {
	std::shared_ptr< std::vector<Bone> > bones;
	Bone* pRoot;
	SkeletonEnumeration skeletonEnumeration;
};

enum class AnimClipFlag {
	Loop,
	SIZE
};

struct AnimClip {
	std::vector< std::vector<AnimFrame> > keyFramesOfBones;
	Seconds duration;
	SkeletonEnumeration skeletonEnumeration;
	u32t flags;
};

AnimFrame MU_CALLCONV lerpAnimFrames(AnimFrame lhs, AnimFrame rhs, float t);

class AnimSystem;

class AnimBlender {
public:
	enum class Stage {
		noop,
		committedLocal,
		committedWorld,
		committedFinal,
		SIZE
	};

	virtual ~AnimBlender() = default;

	void pushTargetClip(const std::string& key, const std::shared_ptr<const AnimClip>& pTargetClip);
	void popTargetClip(const std::string& key);

	void setSkeleton(const std::shared_ptr<Skeleton>& pSkeleton);

	virtual void requestUpdate(AnimSystem& animSystem) = 0;

	virtual void onCalcLocal(PassKey<AnimSystem>) = 0;
	void onCalcWorld(PassKey<AnimSystem>);
	void onCalcFinal(PassKey<AnimSystem>);

	std::vector<mu::Mat4x4>& finalXformData();

	void setStage(PassKey<AnimSystem>, Stage stage) { stage_ = stage; }
	Stage stage() const { return stage_; }

protected:
	void updateFrames(const std::string& key, Seconds elapsed);
	std::vector<AnimFrame>& curFrames(const std::string& key) {
		return frameInfoMap_.at(key).frameCache_;
	}
	const std::vector<AnimFrame>& curFrames(const std::string& key) const {
		return frameInfoMap_.at(key).frameCache_;
	}

	std::vector<mu::Mat4x4>& localXformData();
	std::vector<mu::Mat4x4>& worldXformData();
	const Skeleton* skeleton() const { return skeleton_.get(); }

private:
	struct FrameInfo {
		std::shared_ptr<const AnimClip> targetClip;
		std::vector< decltype(AnimClip::keyFramesOfBones)::value_type::const_iterator > keyFrameIteratorCache_{};
		std::vector<AnimFrame> frameCache_;
	};

	void MU_CALLCONV traverseBone(const Bone& bone, mu::Mat4x4 parentXform = mu::Mat4x4());

	std::vector<mu::Mat4x4> boneXformCache_{};
	std::map<std::string, FrameInfo> frameInfoMap_{};
	std::shared_ptr<Skeleton> skeleton_{};
	Stage stage_{Stage::noop};
};

struct RequestAnimationUpdate {
	AnimBlender* blender;
	float priority;

	auto operator<=>(const RequestAnimationUpdate& rhs) const noexcept {
		return priority <=> rhs.priority;
	}
};

class AnimSystem {
public:
	void update(Seconds timeSlice);

private:
	std::priority_queue<RequestAnimationUpdate> jobQueue_{};
	std::size_t jobSize_{100u};
};

#endif	// __animation_HPP
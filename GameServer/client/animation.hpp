#ifndef __animation_HPP
#define __animation_HPP

#include "gfx.hpp"

// 애니메이션의 한 프레임의 정보를 담는 구조체
// lerpAnimFrames 함수로 서로 다른 AnimFrame을 보간하거나
// convertAnimFrameToMatrix 함수를 통해 AnimFrame을 행렬로 변환할 수 있다.
// AnimBlender의 onCalcLocal 단계에서 주로 쓰인다.
struct AnimFrame {
	mu::Vec3 translation;
	mu::NQuat rotation;
	mu::Vec3 scale;
	Seconds time;
};

struct WeightedAnimFrame {
	AnimFrame frame;
	float w;
};

// 전달된 애니메이션 프레임을 행렬로 변환한다.
// lerpAnimFrames 함수로 여러 프레임을 보간한 최종 프레임을
// 이 함수를 통해 행렬로 변환해서 사용해야 한다.
mu::Mat4x4 MU_CALLCONV convertAnimFrameToMatrix(AnimFrame frame);

// 두 애니메이션 프레임을 인자 t로 선형보간한다.
// translation과 scale에 대해선 mu::lerp가,
// rotation에 대해선 mu::slerp가 사용된다.
AnimFrame MU_CALLCONV lerpAnimFrames(AnimFrame lhs, AnimFrame rhs, float t);
// 전달된 애니메이션 프레임들의 가중합을 계산한다.
// rotation은 normalized linear interpolation으로 계산된다.
AnimFrame MU_CALLCONV sumWeightedAnimFrames(std::span<WeightedAnimFrame> frames);

enum class AnimClipFlag {
	Loop,
	SIZE
};

// 한 개의 애니메이션 클립을 나타내는 구조체
// 키프레임들과 재생 시간, 스켈레톤 구조, 플래그를 저장한다.
struct AnimClip {
	Texture bakedSamples;
	std::string name;
	// 본별 키프레임들
	// i번째 본의 j번째 키프레임은 keyFramesOfBones[i][j]이다.
	// sentinel 값으로 time이 +infinity인 프레임을 각 본의 마지막 키프레임으로 둔다.
	std::vector< std::vector<AnimFrame> > keyFramesOfBones;
	// 본별 baked samples
	// i번째 본의 j번째 sample은 bakedSamplesOfBones[i][j]이다.
	// sentinel 값으로 time이 +infinity인 프레임을 각 본의 마지막 샘플로 둔다.
	std::vector< std::vector<mu::Mat4x4> > bakedSamplesOfBones;
	Seconds duration;	// 애니메이션이 지속될 시간
	float bakedSampleRate;	// baked samples의 fps
	// 해당 클립이 대상으로 하는 스켈레톤 구조
	SkeletonEnumeration skeletonEnumeration;
	// AnimClipFlag에 해당하는 값들과 bitwise-or를 통해 플래그를 추가하고,
	// bitwise-and를 통해 플래그를 검사한다.
	u32t flags;
	i32t id;	// unique한 애니메이션 id,
				// importAnimClip에서 baked animation texture를 만들면서 설정됨
};

std::vector<AnimClip> loadAnimClipsFromFile(const std::filesystem::path& path, GFX& gfx);

class AnimSystem;
class IEventBus;

// 게임 객체에 부착되어 해당 객체의 애니메이션 설정 및 블렌딩을 책임지는 클래스
// 개별 애니메이션이 필요한 게임 객체마다 AnimBlender 객체가 할당되어야 한다.
// 
// AnimBlender가 업데이트되기 위해서는 반드시 AnimSystem에 등록되어야 한다.
// 애니메이션 계산은 비용이 비싼 작업이므로,
// AnimBlender는 애니메이션 업데이트 방법을 정의하기만 하고,
// AnimSystem 객체가 실제 애니메이션 업데이트들의 스케줄링을 책임지며
// 로드밸런싱을 수행하도록 구현했다.
// 
// 사용 흐름은 다음과 같다.
// - 초기화: setSkeleton 호출, pushTargetClip 필요한 만큼 호출
// - 업데이트 루틴: priority getter에서 반환할 priority_ 갱신
// - 렌더링: finalXformData 호출, 변환 정보를 셰이더 입력으로 복사
// 
// 구현하려는 게임 객체에 맞게 이 클래스를 상속하여
// 각 게임 객체 타입마다의 AnimBlender를 만들도록 한다.
// 상속한 클래스는 다음의 두 책임을 지닌다.
// - onCalcLocal에서 프레임들을 업데이트하고 각 클립의 프레임들을 보간해 본의 로컬 변환 행렬들 생성
// - update에서 priority_ 및 애니메이션 시간 계산
// 
// 키프레임들을 보간해 현재 프레임들을 업데이트하고 읽을 수 있는 유틸리티와
// 현재 애니메이션 업데이트 단계에서의 변환 행렬 캐시를 접근할 수 있는 유틸리티를
// 상속한 클래스를 위해 protected로 제공한다.
class AnimBlender {
public:
	enum class Mode {
		Keyframe,
		Baked,
		SIZE
	};

	enum class Stage {
		noop,
		committedLocal,
		committedDress,
		committedFinal,
		SIZE
	};

	virtual ~AnimBlender() = default;

	// 대상 애니메이션 클립을 추가한다.
	// 웬만하면 상속한 클래스에서 초기화시점에 사용할 클립들을 전부 push해놓고
	// 게임 객체의 상태에 맞게 클립들을 선택해 프레임 업데이트 및 블렌딩을 수행하도록 한다.
	void pushTargetClip(const std::string& key, const std::shared_ptr<const AnimClip>& pTargetClip);
	// 대상 애니메이션 클립을 제거한다.
	void popTargetClip(const std::string& key);

	// 스켈레톤을 설정한다.
	// 애니메이션 클립들을 추가하기 전에 스켈레톤이 먼저 설정되어야 한다.
	void setSkeleton(const Skeleton& skeleton);

	float priority() const { return priority_; }

	void setCulled(bool v) { culled_ = v; }
	bool isCulled() const  { return culled_; }

	// AnimSystem::update에서 한 번이라도 onCalcLocal이 호출되었는지 여부.
	// 생성 직후 컬링(Hi-Z/frustum)에 걸려 한 번도 갱신되지 못한 채 방치되는 것을 막기 위해
	// 호출부(feedbackCullResultToAnim 등)에서 이 값을 확인해 최초 1회는 강제로 culled를 해제한다.
	bool hasEverUpdated() const { return hasEverUpdated_; }

	// 이벤트 수신을 지원하려면 상속 클래스에서 이 함수를 오버라이드해
	// 자신만의 EventBus 구현에 대한 포인터를 리턴하도록 한다.
	virtual IEventBus* eventBus() { return nullptr; }

	// priority_ 및 애니메이션 시간 갱신, 그 외 상속한 클래스에서 필요한 작업들을 수행한다.
	// AnimSystem에서 불리지 않는다.
	// 오브젝트의 업데이트때 직접 호출해주어야 한다.
	virtual void update(Seconds deltaTime, void* pOwner) = 0;

	// 본들의 로컬 변환 행렬을 계산한다.
	// 상속한 클래스는 updateFrames, curFrames, lerpAnimFrames 함수들을 적절히 활용하여
	// 로컬 변환 행렬을 계산해 localXformData에 저장해야 한다.
	virtual void onCalcLocal(PassKey<AnimSystem>) = 0;
	// 본들의 로컬 변환을 스켈레톤의 본 트리를 순회하며
	// 드레스 공간 변환으로 환원한다.
	void onCalcDress(PassKey<AnimSystem>);
	// 본들의 최종 변환 행렬들을 계산한다.
	// 스켈레톤의 본들은 기본적으로 드레스 공간에 있기 때문에, 업데이트 시
	// 1. 드레스 공간 -> 본 공간 이전
	// 2. 본 공간에서 애니메이션 프레임에 따른 변환 수행
	// 3. 본 공간 -> 드레스 공간 이전
	// 의 단계가 필요한데,
	// onCalcLocal이 2., onCalcDress가 3.의 단계를 수행했다.
	// 그 결과 행렬의 앞쪽에 본들의 toLocal 행렬을 곱해주어
	// 1.의 단계를 수행한다.
	void onCalcFinal(PassKey<AnimSystem>);

	// 본들의 최종 변환 행렬들을 반환한다.
	// 렌더링 시에는 이 행렬들을 참조한다.
	std::vector<mu::Mat4x4>& finalXformData();
	const std::vector<mu::Mat4x4>& finalXformData() const;
	int finalBakedClipId() const { return finalBakedClipId_; }
	int finalBakedClipFrame() const { return finalBakedClipFrame_; }

	// AnimSystem에서, 이 AnimBlender 객체의 계산 단계를 설정하기 위해 만들어진 함수
	void setStage(PassKey<AnimSystem>, Stage stage) {
		stage_ = stage;
		if (stage == Stage::committedLocal) {
			priority_ = 0.f;
			hasEverUpdated_ = true;
		}
	}
	Stage stage() const { return stage_; }

	// 이 AnimBlender의 priority를 누적 갱신한다.
	// AnimSystem::updatePriorities에서 호출된다.
	void updatePriority(PassKey<AnimSystem>, Seconds dt, mu::Vec3 refPos);

	Mode mode() const { return mode_; }

protected:
	// onCalcDress의 부모 누적 완료 직후 호출되는 훅 (Keyframe 모드 한정).
	// 파생 블렌더가 드레스 공간 행렬(dressXformData)에 프로시저럴 보정을
	// 주입할 수 있다 (예: AnimBlenderPlayer의 스파인 조준 pitch).
	virtual void onPostDress() {}

	// 본들의 로컬 변환 행렬들에 접근한다.
	// onCalcLocal에서 사용하도록 한다.
	std::vector<mu::Mat4x4>& localXformData();
	// 본들의 드레스 공간 변환 행렬들에 접근한다.
	std::vector<mu::Mat4x4>& dressXformData();
	// key에 해당하는 클립을 elapsed 만큼의 시간이 지났을 때의 프레임으로 갱신한다.
	void updateFrames(const std::string& key, Seconds elapsed);

	// key 클립의 재생 시간을 rate배로 진행시키고 클립 길이로 랩한다.
	// 각 파생 블렌더가 복붙하던 `t += dt; while (t > dur) t -= dur;`를 대체한다.
	// 랩은 fmod로 처리하므로 rate가 커도(또는 duration이 0에 가까워도) 루프가 돌지 않는다.
	void advanceClipTime(const std::string& key, Seconds& time, Seconds deltaTime,
		float rate = 1.f) const;

	// 이동 속력을 로코모션 클립의 재생 배속으로 환산한다.
	//
	// 이 시스템은 속도를 "블렌드 가중치"로 표현해왔다(idle 가중치가 클수록 보폭이 짧아진다).
	// 따라서 단순히 speedXZ/refSpeed를 쓰면 가중치가 이미 깎아놓은 보폭을 한 번 더 깎아
	// 중속 구간이 오히려 더 미끄러진다. locoWeight로 그 몫을 되돌려준다.
	//   rate = clamp( speedXZ / (refSpeed * locoWeight) )
	// 블렌드 밴드 안에서는 locoWeight ≈ speedXZ/밴드끝 이고 refSpeed를 밴드끝으로 저작하므로
	// 이 값이 1 근처로 수렴한다(= 기존 룩 보존). 가중치가 포화한 뒤(고속 구간)부터
	// speedXZ/refSpeed 그대로 배속이 붙는다 — 지금 실제로 깨져 있는 구간이 거기다.
	//
	// prevRate를 받아 갱신값을 돌려주므로 상태는 호출 측이 float 하나로 들고 있으면 된다
	// (원격 캐릭터의 속도는 20Hz 패킷 값이라 평활 없이는 배속이 계단진다).
	// locoWeight에는 공격 오버레이가 하체에서 깎아가는 몫까지 반영해서 넘겨야 한다
	// (상하체 마스크 구조를 아는 쪽은 각 파생 블렌더이므로 합성은 호출부 책임).
	static float solveLocomotionRate(float prevRate, float speedXZ, float refSpeed,
		float locoWeight, Seconds deltaTime);

	// baked 모드에서 time에 해당하는 샘플 프레임 인덱스를 구한다.
	// 클립의 샘플 개수로 클램프하므로 배속·랩 타이밍과 무관하게 텍스처 범위를 벗어나지 않는다.
	static int bakedFrameOf(const AnimClip& clip, Seconds time);
	// key에 해당하는 클립의 현재 프레임들을 접근한다.
	// 현재 프레임들은 updateFrames 함수를 통해서 갱신할 수 있다.
	std::vector<AnimFrame>& curFrames(const std::string& key) {
		return frameInfoMap_.at(key).frameCache_;
	}
	// key에 해당하는 클립의 현재 프레임들을 접근한다.
	// 현재 프레임들은 updateFrames 함수를 통해서 갱신할 수 있다.
	const std::vector<AnimFrame>& curFrames(const std::string& key) const {
		return frameInfoMap_.at(key).frameCache_;
	}

	const std::shared_ptr<const AnimClip>& targetClip(const std::string& key) const {
		return frameInfoMap_.at(key).targetClip;
	}

	const Skeleton& skeleton() const { return skeleton_; }
	void setOwnerPos(mu::Vec3 pos) { cachedPos_ = pos; }

	// Object::update() 시점의 소유자 위치를 캐싱한다.
	// AnimSystem::updatePriorities에서 priority 계산에 사용된다.
	mu::Vec3 cachedPos_{};
	Seconds updateLag_{};
	float priority_{};
	bool  culled_ = false;
	bool  hasEverUpdated_ = false;
	Mode mode_{ Mode::Keyframe };
	int finalBakedClipId_{};
	int finalBakedClipFrame_{};

private:
	// 특정한 클립의 현재 프레임 상태 및
	// 현재 프레임 상태를 갱신하기 위한 정보를 저장하는 구조체
	struct FrameInfo {
		// ----[Keyframe Mode 관련]----
		std::shared_ptr<const AnimClip> targetClip;	// 프레임 계산 대상이 되는 클립
		// 본별로 현재 어느 키프레임을 참조하고 있는지 저장하는 벡터
		// keyFrameIteratorCache_[i] == iteratorA일 때,
		// i번째 본의 현재 프레임을 계산하는 데
		// iteratorA와 std::next(iteratorA)가 가리키는 키프레임들을 보간해야 함이 표현된다.
		std::vector< decltype(AnimClip::keyFramesOfBones)::value_type::const_iterator > keyFrameIteratorCache_{};
		// 키프레임들을 보간하여 계산된 현재 프레임 정보를 저장한다.
		std::vector<AnimFrame> frameCache_;
		// Baked 모드는 프레임 캐시를 쓰지 않는다. 파생 블렌더의 onCalcLocal이
		// finalBakedClipId_/finalBakedClipFrame_만 채우고 GPU가 직접 샘플을 읽는다.
	};

	// key에 해당하는 클립을 elapsed 만큼의 시간이 지났을 때의 프레임으로 갱신한다.
	void updateFramesKeyframeMode(const std::string& key, Seconds elapsed);

	// onCalcDress에서, 본 트리를 순회하며 행렬들을 갱신하는데 사용된다.
	void MU_CALLCONV traverseBone(const Bone& bone, mu::Mat4x4 parentXform = mu::Mat4x4());

	std::vector<mu::Mat4x4> boneXformCache_{};
	std::map<std::string, FrameInfo> frameInfoMap_{};
	Skeleton skeleton_{};
	Stage stage_{ Stage::noop };
};

// 애니메이션 업데이트를 스케줄링하며 실행하는 클래스
// trackAnimBlender 함수를 통해 AnimBlender를 AnimSystem에 등록할 수 있다.
class AnimSystem {
public:
	AnimSystem() {
		blenders_.reserve(16u);
	}

	// blender를 AnimSystem에 등록한다.
	void trackAnimBlender(AnimBlender* blender) {
		blenders_.push_back(blender);
	}
	// blender를 AnimSystem에서 제거한다.
	void untrackAnimBlender(AnimBlender* blender) {
		std::erase(blenders_, blender);
	}
	// 모든 AnimBlender의 priority를 누적 갱신한다.
	// Object::update() 루프 이후, AnimSystem::update() 이전에 호출해야 한다.
	// 각 AnimBlender의 cachedPos(Object::update에서 캐싱된 위치)와
	// refPos(기준 위치, 보통 로컬 플레이어 위치) 사이의 거리를 사용해
	// priority를 누적한다.
	void updatePriorities(Seconds dt, mu::Vec3 refPos);

	// 등록된 AnimBlender 객체들을 priority로 max-heapify하고
	// 모든 AnimBlender 객체들을 업데이트하거나 주어진 timeSlice가 소진될 때까지
	// AnimBlender 객체들을 업데이트한다.
	// AnimBlender::onCalcLocal, AnimBlender::onCalcDress, AnimBlender::onCalcFinal이
	// 순서대로 불리며 업데이트된다.
	void update(Seconds timeSlice);

private:
	// priority에 따른 AnimBlender들의 max heap
	std::vector<AnimBlender*> blenders_{};
	// update에서 처리할 작업 단위(job)
	// timeSlice에서 job 처리 횟수를 8회에 근접하게 맞추기 위해
	// jobSize_를 유동적으로 갱신한다.
	//
	// (job의 개수가 너무 적으면 시간 검사를 덜 해 요청된 timeSlice를 초과할 가능성이 커지고,
	// 너무 많으면 시간 검사를 너무 많이 해 시스템콜로 인한 오버헤드가 커진다.)
	std::size_t jobSize_{40u};
};

#endif	// __animation_HPP
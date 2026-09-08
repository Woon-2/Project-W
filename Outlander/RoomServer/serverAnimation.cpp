#include "rspch.hpp"
#include "serverAnimation.hpp"
#include "Model.hpp"
#include "binaryImport.hpp"
#include "errorHandling.hpp"

int ServerAnimClip::sampleIdxAt(float t) const {
    if (bakedSamples.empty() || bakedSamples[0].empty()) return 0;
    // -1 because the last entry is a sentinel (copy of last real sample)
    const int maxIdx = static_cast<int>(bakedSamples[0].size()) - 2;
    if (maxIdx < 0) return 0;
    const int idx = static_cast<int>(t * sampleRate);
    return std::clamp(idx, 0, maxIdx);
}

void ServerAnimState::advance(float dt) {
    if (!clip || clip->empty()) return;
    elapsedTime += dt * playbackRate;
    if (clip->loops() && elapsedTime > clip->duration && clip->duration > 0.f)
        elapsedTime = std::fmod(elapsedTime, clip->duration);
}

float ServerAnimState::locomotionRate(float speedXZ, float refSpeed, float bandEnd) {
    // What we want here is the *same clip phase* the client is at, because the server's bone
    // poses feed the hit BVH and the client's rendered pose is driven by that same clip.
    //
    // The client's rate is speed / (refSpeed * blendWeight), dividing out the blend weight
    // because its locomotion pose is diluted by idle. That weight is a pure function of speed:
    //   blendWeight = clamp((speed - bandStart) / (bandEnd - bandStart), 0, 1) ~= min(speed / bandEnd, 1)
    // (bandStart is 0.03-0.05 in every blender, negligible). Substituting collapses the whole
    // expression to a form the server can evaluate without knowing the blend state at all:
    //   rate = max(bandEnd, speed) / refSpeed
    // i.e. constant bandEnd/refSpeed throughout the blend band, then tracking speed once the
    // weight saturates. This is exact for the player and every monster blender.
    //
    // Do NOT simplify this back to speed/refSpeed clamped at 1: that only held while refSpeed
    // happened to equal bandEnd. Authored reference speeds diverge from the band ends by up to
    // 3x (Treant: refSpeed 9.0 vs bandEnd 3.06), which would put the limbs a half-cycle out.
    constexpr float kMinRate = 0.25f;   // must match AnimBlender::solveLocomotionRate
    constexpr float kMaxRate = 2.00f;
    const float target = std::max(bandEnd, speedXZ) / std::max(refSpeed, 1e-3f);
    return std::clamp(target, kMinRate, kMaxRate);
}

void ServerAnimState::switchClip(const ServerAnimClip* newClip, float startTime) {
    if (clip == newClip) return;
    clip        = newClip;
    elapsedTime = startTime;
}

void ServerAnimState::computeBoneModelXforms(const ServerSkeleton& sk,
                                              std::vector<mu::Mat4x4>& outXforms) const {
    if (!clip || clip->empty()) return;
    const int sIdx = clip->sampleIdxAt(elapsedTime);
    const int n    = sk.boneCount();
    for (int i = 0; i < n; ++i) {
        // row-major: toDress first, then bakedSample
        // result = poseTransform[i] (bone-local -> animated model space)
        outXforms[i] = sk.bones[i].toDress * clip->bakedSamples[i][sIdx];
    }
}

// ---------------------------------------------------------------------------
// .anim file loading (baked samples only, keyframes skipped)
// ---------------------------------------------------------------------------

static void skipKeyFrames(std::ifstream& ifs, int boneCnt) {
    readHeadTag(ifs, "KeyFrames");
    for (int b = 0; b < boneCnt; ++b) {
        readInteger(ifs, "Bone");
        const int kfCnt = readInteger(ifs, "KeyFrameCnt");
        for (int k = 0; k < kfCnt; ++k) {
            readHeadTag(ifs, "KeyFrame");
            readFloat(ifs, "Time");
            readVec3(ifs, "Translation");
            readVec4(ifs, "Rotation");
            readVec3(ifs, "Scale");
            readTailTag(ifs, "KeyFrame");
        }
    }
    readTailTag(ifs, "KeyFrames");
}

static ServerAnimClip importServerAnimClip(std::ifstream& ifs, int boneCnt) {
    ServerAnimClip clip;

    readHeadTag(ifs, "Clip");

    clip.name = readText(ifs, "Name");
    readText(ifs, "SkeletonEnumeration"); // not used server-side
    clip.duration = readFloat(ifs, "Duration");

    const std::string wrapMode = readText(ifs, "WrapMode");
    if (wrapMode == "Default" || wrapMode == "Loop")
        clip.flags |= static_cast<uint32_t>(ServerAnimClipFlag::Loop);

    skipKeyFrames(ifs, boneCnt);

    readHeadTag(ifs, "BakedSamples");
    clip.sampleRate = readFloat(ifs, "SampleRate");

    clip.bakedSamples.resize(boneCnt);
    for (int b = 0; b < boneCnt; ++b) {
        readInteger(ifs, "Bone");
        const int sampleCnt = readInteger(ifs, "sampleCnt");

        auto& samples = clip.bakedSamples[b];
        samples.resize(sampleCnt + 1); // +1 for sentinel
        for (int s = 0; s < sampleCnt; ++s) {
            readHeadTag(ifs, "Sample");
            const auto mtx = readMatrix(ifs, "Matrix");
            samples[s] = mu::Mat4x4(DirectX::XMLoadFloat4x4(&mtx));
            readTailTag(ifs, "Sample");
        }
        samples[sampleCnt] = samples[sampleCnt - 1]; // sentinel
    }

    readTailTag(ifs, "BakedSamples");
    readTailTag(ifs, "Clip");

    gSharedLog << "[ServerAnim] Loaded clip \"" << clip.name
               << "\" (" << boneCnt << " bones, " << clip.bakedSamples[0].size() - 1
               << " samples @ " << clip.sampleRate << " fps)\n";
    return clip;
}

const ServerAnimClip* findServerAnimClip(const std::vector<ServerAnimClip>& clips, std::string_view name) {
    for (const auto& c : clips)
        if (c.name == name) return &c;
    return nullptr;
}

void AnimController::registerClip(std::string_view key, const ServerAnimClip* clip) {
    clips_[std::string(key)] = clip;

    if (clip && isLocomotionKey(key)
        && std::find(locomotionClips_.begin(), locomotionClips_.end(), clip)
               == locomotionClips_.end()) {
        locomotionClips_.push_back(clip);
    }
}

bool AnimController::isLocomotionKey(std::string_view key) {
    return key == "Walk" || key == "Run"
        || key == "Run_Forward" || key == "Run_Backward"
        || key == "Run_Right"   || key == "Run_Left";
}

bool AnimController::switchClip(std::string_view key, float startTime) {
    auto it = clips_.find(std::string(key));
    if (it == clips_.end()) return false;
    // Never switch the live clip to null. A key registered with a missing clip
    // (findServerAnimClip returned nullptr) would otherwise null `clip`, and the
    // bone-attached collision/hit BVH only updates while a clip is active
    // (Object::updateAnimBones early-returns on null) — so it would freeze at the
    // last pose while the Dynamic body keeps integrating, desyncing the hit BVH
    // from the body (monsters fall through terrain + become unhittable). Keeping
    // the current valid clip preserves BVH tracking; the caller's intended clip
    // just doesn't play (a missing-asset symptom, not a physics break).
    if (!it->second) return false;
    ServerAnimState::switchClip(it->second, startTime);
    return true;
}

const ServerAnimClip* AnimController::findRegistered(std::string_view key) const {
    auto it = clips_.find(std::string(key));
    return (it != clips_.end()) ? it->second : nullptr;
}

bool AnimController::isPlayingLocomotion() const {
    if (!clip) return false;
    return std::find(locomotionClips_.begin(), locomotionClips_.end(), clip)
        != locomotionClips_.end();
}

// ---------------------------------------------------------------------------

std::vector<ServerAnimClip> loadServerAnimClipsFromFile(const std::filesystem::path& path) {
    std::vector<ServerAnimClip> ret;

    auto ifs = std::ifstream(path, std::ios::binary);
    DISPLAY_ERROR_STR(ifs.good(),
        "[ServerAnim] Cannot open: " + path.string(), false);
    if (!ifs) return ret;

    readText(ifs, "AnimationSetName");
    const int clipCnt = readInteger(ifs, "ClipCnt");
    const int boneCnt = readInteger(ifs, "BoneCnt");

    ret.reserve(clipCnt);
    for (int i = 0; i < clipCnt; ++i)
        ret.push_back(importServerAnimClip(ifs, boneCnt));

    gSharedLog << "[ServerAnim] Loaded " << ret.size() << " clip(s) from " << path << '\n';
    return ret;
}

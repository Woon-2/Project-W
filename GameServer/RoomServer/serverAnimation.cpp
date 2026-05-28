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
    elapsedTime += dt;
    if (clip->loops() && elapsedTime > clip->duration && clip->duration > 0.f)
        elapsedTime = std::fmod(elapsedTime, clip->duration);
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
}

bool AnimController::switchClip(std::string_view key, float startTime) {
    auto it = clips_.find(std::string(key));
    if (it == clips_.end()) return false;
    ServerAnimState::switchClip(it->second, startTime);
    return true;
}

const ServerAnimClip* AnimController::findRegistered(std::string_view key) const {
    auto it = clips_.find(std::string(key));
    return (it != clips_.end()) ? it->second : nullptr;
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

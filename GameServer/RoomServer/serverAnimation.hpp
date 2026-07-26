#ifndef room_server_serverAnimation_hpp
#define room_server_serverAnimation_hpp

#include <filesystem>
#include <vector>
#include <string>
#include <unordered_map>

struct ServerSkeleton; // defined in Model.hpp

// Flags stored in ServerAnimClip::flags.
enum class ServerAnimClipFlag : uint32_t {
    Loop = 1u << 0,
};

// Server-side animation clip: baked bone matrices only (no keyframes).
// bakedSamples[boneIdx][sampleIdx] is the GPU skinning matrix for that bone at that sample:
//   skinningMatrix = toLocal * poseTransform  (DirectXMath row-major)
// To recover poseTransform (bone-local -> model space):
//   poseTransform = toDress * bakedSamples[boneIdx][sampleIdx]
struct ServerAnimClip {
    std::string name;
    float       duration    = 0.f;
    float       sampleRate  = 30.f;
    uint32_t    flags       = 0;

    // [boneIdx][sampleIdx], last entry is sentinel (copy of previous)
    std::vector<std::vector<mu::Mat4x4>> bakedSamples;

    bool loops()      const { return (flags & static_cast<uint32_t>(ServerAnimClipFlag::Loop)) != 0; }
    bool empty()      const { return bakedSamples.empty(); }
    int  boneCount()  const { return static_cast<int>(bakedSamples.size()); }
    // Returns the sample index for elapsed time t, clamped to valid range.
    int sampleIdxAt(float t) const;
};

// Per-entity animation playback state on the server.
struct ServerAnimState {
    const ServerAnimClip* clip        = nullptr;
    float                 elapsedTime = 0.f;
    // Clip playback rate. Locomotion clips are sped up / slowed down to match the entity's
    // ground speed so the bone poses (and therefore the hit BVH built from them) line up
    // with what the client renders. Non-locomotion clips must keep this at 1.
    float                 playbackRate = 1.f;

    void advance(float dt);
    void switchClip(const ServerAnimClip* newClip, float startTime = 0.f);

    // Ground speed -> locomotion playback rate, clamped. refSpeed is the speed the clip was
    // authored for (NpcConfig::animRefSpeed); bandEnd is the speed at which the client's
    // idle->locomotion blend weight saturates (NpcConfig::animBandEnd). Kept in sync with the
    // client's AnimBlender::solveLocomotionRate -- see the derivation in the .cpp.
    static float locomotionRate(float speedXZ, float refSpeed, float bandEnd);

    // Fills outXforms with bone model-space transforms for the current frame.
    // outXforms[i] = skeleton.bones[i].toDress * bakedSamples[i][sampleIdx]
    //              = poseTransform[i]  (bone-local -> model space, row-major)
    // outXforms must already be sized to skeleton.boneCount().
    void computeBoneModelXforms(const ServerSkeleton& sk,
                                std::vector<mu::Mat4x4>& outXforms) const;
};

// Loads all animation clips from a .anim file (same format as the client).
// Keyframe data is skipped; only baked samples are retained.
std::vector<ServerAnimClip> loadServerAnimClipsFromFile(const std::filesystem::path& path);

// Returns the first clip with the given name in the set, or nullptr.
const ServerAnimClip* findServerAnimClip(const std::vector<ServerAnimClip>& clips, std::string_view name);

// Per-entity animation controller: extends ServerAnimState with named clip registration.
// Register clips by string key, then switch by key. The pointer-based switchClip() from the
// base class remains accessible via the using declaration for direct overrides.
class AnimController : public ServerAnimState {
public:
    using ServerAnimState::switchClip;  // keep pointer overload accessible

    // Associates a name key with a clip pointer. Overwrites any previous mapping for that key.
    void registerClip(std::string_view key, const ServerAnimClip* clip);

    // Looks up the clip by key and switches to it. Returns false if key is not registered.
    // NOTE: switching does not reset playbackRate -- switchClip() early-outs when the clip
    // pointer is unchanged, so the rate must always be set through setPlaybackRate().
    bool switchClip(std::string_view key, float startTime = 0.f);

    void setPlaybackRate(float rate) { playbackRate = rate; }

    // Returns the registered clip for key, or nullptr if not found.
    const ServerAnimClip* findRegistered(std::string_view key) const;

    // True when the live clip is one of the locomotion clips (the keys the AI switches to
    // while the entity is travelling). Only those may be played back off 1x -- attack,
    // hit and death clips drive skill/hit timing and must stay in real time.
    // Compares against pointers cached at registration, so this stays allocation-free on
    // the per-entity per-frame path (Object::updateAnimBones).
    bool isPlayingLocomotion() const;

private:
    // Keys the AI switches to while an entity is travelling. NPCs/boss use "Walk"/"Run";
    // players use the directional "Run_*" set (see the registrations in Room.cpp).
    static bool isLocomotionKey(std::string_view key);

    std::unordered_map<std::string, const ServerAnimClip*> clips_;
    // Locomotion clips seen by registerClip, cached so the per-frame check needs no lookup.
    std::vector<const ServerAnimClip*> locomotionClips_;
};

#endif // room_server_serverAnimation_hpp

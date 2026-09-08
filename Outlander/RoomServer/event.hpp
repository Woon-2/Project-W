#ifndef room_server_event_hpp
#define room_server_event_hpp

// Server-side event system.
// Heap-allocated (no pool); per-frame event counts are small on the server.
// Interface mirrors client/event.hpp, with EvSkillHit extended for server-specific fields.

#include <list>

using EventList = std::list<char*>;

namespace detail {
inline void* allocEvent(std::size_t sz) { return ::operator new(sz); }
inline void  freeEvent(void* p)         { ::operator delete(p); }
} // namespace detail

#define holdEvent(eventList, ...) \
    new ( \
        (eventList).emplace_back( \
            static_cast<char*>(detail::allocEvent(sizeof(decltype(__VA_ARGS__)))) \
        ) \
    ) __VA_ARGS__

#define clearEvents(eventList) \
    do { for (auto* _p : (eventList)) detail::freeEvent(_p); (eventList).clear(); } while (0)

enum class EventType : uint32 {
    SkillHit,
    CameraShake,
    SIZE
};

struct BasicEvent {
    EventType type;
};

// Extended vs. client: carries attackerId and skillAssetId for server broadcast.
struct EvSkillHit : BasicEvent {
    EvSkillHit() : BasicEvent{EventType::SkillHit} {}
    EvSkillHit(int32 targetId, int32 damage, int32 attackerId, uint32 skillAssetId)
        : BasicEvent{EventType::SkillHit},
          targetId{targetId}, damage{damage},
          attackerId{attackerId}, skillAssetId{skillAssetId} {}

    int32  targetId     { -1 };
    int32  damage       {  0 };
    int32  attackerId   { -1 };
    uint32 skillAssetId {  0 };
};

struct EvCameraShake : BasicEvent {
    EvCameraShake() : BasicEvent{EventType::CameraShake} {}
    EvCameraShake(float magnitude, Milliseconds duration)
        : BasicEvent{EventType::CameraShake}, magnitude{magnitude}, duration{duration} {}

    float        magnitude{ 0.f };
    Milliseconds duration { 0.f };
};

#endif // room_server_event_hpp

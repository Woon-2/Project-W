#ifndef mushroom_hpp
#define mushroom_hpp

#include "Npc.hpp"
#include <array>

class Mushroom : public Npc {
public:
    Mushroom() = default;
    Mushroom(Object&& base) : Npc(std::move(base)) {}

    void applyMushroomConfig();

    void     recordSnapshot(uint64 serverMs);
    mu::Vec3 rewindPos     (uint64 targetMs) const;

private:
    struct PosSnapshot {
        uint64   serverMs;
        mu::Vec3 pos;
    };

    static constexpr int32 historySize_ = 16;
    std::array<PosSnapshot, historySize_> posHistory_{};
    int32 historyHead_ = 0;
};

#endif // mushroom_hpp

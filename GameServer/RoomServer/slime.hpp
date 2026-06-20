#ifndef slime_hpp
#define slime_hpp

#include "Npc.hpp"
#include <array>

// Slime monster. Same Npc FSM as Goblin; only stats/model/anims differ.
class Slime : public Npc {
public:
    Slime() = default;
    Slime(Object&& base) : Npc(std::move(base)) {}

    void applySlimeConfig();

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

#endif // slime_hpp

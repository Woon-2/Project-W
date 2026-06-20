#ifndef bomber_hpp
#define bomber_hpp

#include "Npc.hpp"
#include <array>

// Bomber monster. Same Npc FSM as Goblin; only stats/model/anims differ.
class Bomber : public Npc {
public:
    Bomber() = default;
    Bomber(Object&& base) : Npc(std::move(base)) {}

    void applyBomberConfig();

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

#endif // bomber_hpp

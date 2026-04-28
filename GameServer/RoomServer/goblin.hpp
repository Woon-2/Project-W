#pragma once
#include "Npc.hpp"
#include <array>

class Goblin : public Npc {
public:
    Goblin() = default;
    Goblin(Object&& base) : Npc(std::move(base)) {}

    void applyGoblinConfig();

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

#ifndef treant_hpp
#define treant_hpp

#include "Npc.hpp"
#include <array>

// Treant monster. Same Npc FSM as Goblin; only stats/model/anims differ.
// Named variant: Grandbaum (shares Treant anims, different model).
class Treant : public Npc {
public:
    Treant() = default;
    Treant(Object&& base) : Npc(std::move(base)) {}

    void applyTreantConfig();

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

#endif // treant_hpp

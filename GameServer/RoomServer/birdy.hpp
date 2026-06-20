#ifndef birdy_hpp
#define birdy_hpp

#include "Npc.hpp"
#include <array>

// Birdy monster. Same Npc FSM as Goblin; only stats/model/anims differ.
// Named variant: Isys (shares Birdy anims, different model).
class Birdy : public Npc {
public:
    Birdy() = default;
    Birdy(Object&& base) : Npc(std::move(base)) {}

    void applyBirdyConfig();

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

#endif // birdy_hpp

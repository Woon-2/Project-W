#pragma once
#include "TacticalNpc.hpp"
#include "mathUtil.hpp"

// Tactical (command-driven) Bomber trooper. Mirrors TacticalGoblin: a static config provider
// plus its client render identity. Used by the Isys mid-boss encounter.
class TacticalBomber : public TacticalNpc {
public:
    TacticalBomber() = default;
    explicit TacticalBomber(Object&& base) : TacticalNpc(std::move(base), trooperConfig()) {}
    virtual ~TacticalBomber() = default;

    static TacticalNpcConfig trooperConfig();
    static constexpr ObjectType objType() { return ObjectType::Bomber; }
};

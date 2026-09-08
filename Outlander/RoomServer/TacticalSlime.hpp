#pragma once
#include "TacticalNpc.hpp"
#include "mathUtil.hpp"

// Tactical (command-driven) Slime trooper. Mirrors TacticalGoblin: a static config provider
// plus its client render identity. Used by the Grandbaum mid-boss encounter.
class TacticalSlime : public TacticalNpc {
public:
    TacticalSlime() = default;
    explicit TacticalSlime(Object&& base) : TacticalNpc(std::move(base), trooperConfig()) {}
    virtual ~TacticalSlime() = default;

    static TacticalNpcConfig trooperConfig();
    static constexpr ObjectType objType() { return ObjectType::Slime; }
};

#pragma once
#include "TacticalNpc.hpp"
#include "mathUtil.hpp"

// Tactical (command-driven) Treant trooper. Mirrors TacticalGoblin: a static config provider
// plus its client render identity. Provided for completeness so any encounter can field Treants.
class TacticalTreant : public TacticalNpc {
public:
    TacticalTreant() = default;
    explicit TacticalTreant(Object&& base) : TacticalNpc(std::move(base), trooperConfig()) {}
    virtual ~TacticalTreant() = default;

    static TacticalNpcConfig trooperConfig();
    static constexpr ObjectType objType() { return ObjectType::Treant; }
};

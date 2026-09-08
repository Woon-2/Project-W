#pragma once
#include "TacticalNpc.hpp"
#include "mathUtil.hpp"

// Tactical (command-driven) Birdy trooper (the Isys encounter calls these "Buddy" squads).
// Mirrors TacticalGoblin: a static config provider plus its client render identity.
class TacticalBirdy : public TacticalNpc {
public:
    TacticalBirdy() = default;
    explicit TacticalBirdy(Object&& base) : TacticalNpc(std::move(base), trooperConfig()) {}
    virtual ~TacticalBirdy() = default;

    static TacticalNpcConfig trooperConfig();
    static constexpr ObjectType objType() { return ObjectType::Birdy; }
};

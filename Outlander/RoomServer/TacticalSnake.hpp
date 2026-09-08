#pragma once
#include "TacticalNpc.hpp"
#include "mathUtil.hpp"

// Tactical (command-driven) Snake trooper. Mirrors TacticalGoblin: a static config provider
// plus its client render identity. Used by the Grandbaum mid-boss encounter.
class TacticalSnake : public TacticalNpc {
public:
    TacticalSnake() = default;
    explicit TacticalSnake(Object&& base) : TacticalNpc(std::move(base), trooperConfig()) {}
    virtual ~TacticalSnake() = default;

    static TacticalNpcConfig trooperConfig();
    static constexpr ObjectType objType() { return ObjectType::Snake; }
};

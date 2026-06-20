#pragma once
#include "TacticalNpc.hpp"
#include "mathUtil.hpp"

class TacticalGoblin : public TacticalNpc {
public:
    TacticalGoblin() = default;
    explicit TacticalGoblin(Object&& base) : TacticalNpc(std::move(base), trooperConfig()) {}
	virtual ~TacticalGoblin() = default;

    static TacticalNpcConfig trooperConfig();
    static TacticalNpcConfig bossConfig();
};

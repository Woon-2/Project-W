#include "rspch.hpp"
#include "goblin.hpp"

void Goblin::applyGoblinConfig() {
    NpcConfig cfg;
    cfg.maxHp          = 90.f;
    cfg.moveSpeed      = 3.f;
    cfg.attackRange    = 1.5f;
    cfg.detectionRange = 15.f;
    cfg.attackDamage   = 15.f;
    applyConfig(cfg);
    setHp(static_cast<int32>(cfg.maxHp));
}

void Goblin::recordSnapshot(uint64 serverMs) {
    posHistory_[historyHead_] = { serverMs, pos() };
    historyHead_ = (historyHead_ + 1) % historySize_;
}

mu::Vec3 Goblin::rewindPos(uint64 targetMs) const {
    for (int32 i = 1; i <= historySize_; ++i) {
        int32 idx = (historyHead_ - i + historySize_) % historySize_;
        if (posHistory_[idx].serverMs <= targetMs)
            return posHistory_[idx].pos;
    }
    return posHistory_[historyHead_ % historySize_].pos;
}

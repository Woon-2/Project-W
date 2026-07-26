#include "rspch.hpp"
#include "treant.hpp"

void Treant::applyTreantConfig() {
    NpcConfig cfg;
    cfg.maxHp          = 150.f;
    cfg.moveSpeed      = 2.0f;
    cfg.animRefSpeed   = 7.8f;   // authored speed of Treant_Walk (Grandbaum shares this clip set)
    cfg.attackRange    = 2.2f;
    cfg.detectionRange = 11.f;
    cfg.attackDamage   = 20.f;
    cfg.respawnDelay   = 18s;
    applyConfig(cfg);
    setHp(static_cast<int32>(cfg.maxHp));
}

void Treant::recordSnapshot(uint64 serverMs) {
    posHistory_[historyHead_] = { serverMs, pos() };
    historyHead_ = (historyHead_ + 1) % historySize_;
}

mu::Vec3 Treant::rewindPos(uint64 targetMs) const {
    for (int32 i = 1; i <= historySize_; ++i) {
        int32 idx = (historyHead_ - i + historySize_) % historySize_;
        if (posHistory_[idx].serverMs <= targetMs)
            return posHistory_[idx].pos;
    }
    return posHistory_[historyHead_ % historySize_].pos;
}

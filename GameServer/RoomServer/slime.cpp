#include "rspch.hpp"
#include "slime.hpp"

void Slime::applySlimeConfig() {
    NpcConfig cfg;
    cfg.maxHp          = 110.f;
    cfg.moveSpeed      = 2.0f;
    cfg.attackRange    = 1.5f;
    cfg.detectionRange = 8.f;
    cfg.attackDamage   = 9.f;
    cfg.respawnDelay   = 15s;
    applyConfig(cfg);
    setHp(static_cast<int32>(cfg.maxHp));
}

void Slime::recordSnapshot(uint64 serverMs) {
    posHistory_[historyHead_] = { serverMs, pos() };
    historyHead_ = (historyHead_ + 1) % historySize_;
}

mu::Vec3 Slime::rewindPos(uint64 targetMs) const {
    for (int32 i = 1; i <= historySize_; ++i) {
        int32 idx = (historyHead_ - i + historySize_) % historySize_;
        if (posHistory_[idx].serverMs <= targetMs)
            return posHistory_[idx].pos;
    }
    return posHistory_[historyHead_ % historySize_].pos;
}

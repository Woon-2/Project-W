#include "rspch.hpp"
#include "bomber.hpp"

void Bomber::applyBomberConfig() {
    NpcConfig cfg;
    cfg.maxHp          = 70.f;
    cfg.moveSpeed      = 3.0f;
    cfg.attackRange    = 1.6f;
    cfg.detectionRange = 10.f;
    cfg.attackDamage   = 18.f;
    cfg.respawnDelay   = 15s;
    applyConfig(cfg);
    setHp(static_cast<int32>(cfg.maxHp));
}

void Bomber::recordSnapshot(uint64 serverMs) {
    posHistory_[historyHead_] = { serverMs, pos() };
    historyHead_ = (historyHead_ + 1) % historySize_;
}

mu::Vec3 Bomber::rewindPos(uint64 targetMs) const {
    for (int32 i = 1; i <= historySize_; ++i) {
        int32 idx = (historyHead_ - i + historySize_) % historySize_;
        if (posHistory_[idx].serverMs <= targetMs)
            return posHistory_[idx].pos;
    }
    return posHistory_[historyHead_ % historySize_].pos;
}

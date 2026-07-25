#include "rspch.hpp"
#include "birdy.hpp"

void Birdy::applyBirdyConfig() {
    NpcConfig cfg;
    cfg.maxHp          = 60.f;
    cfg.moveSpeed      = 4.5f;
    cfg.animRefSpeed   = 3.0f;   // authored speed of Birdy_Walk (not measured -- default; Isys shares it)
    cfg.attackRange    = 1.4f;
    cfg.detectionRange = 12.f;
    cfg.attackDamage   = 9.f;
    cfg.respawnDelay   = 15s;
    applyConfig(cfg);
    setHp(static_cast<int32>(cfg.maxHp));
}

void Birdy::recordSnapshot(uint64 serverMs) {
    posHistory_[historyHead_] = { serverMs, pos() };
    historyHead_ = (historyHead_ + 1) % historySize_;
}

mu::Vec3 Birdy::rewindPos(uint64 targetMs) const {
    for (int32 i = 1; i <= historySize_; ++i) {
        int32 idx = (historyHead_ - i + historySize_) % historySize_;
        if (posHistory_[idx].serverMs <= targetMs)
            return posHistory_[idx].pos;
    }
    return posHistory_[historyHead_ % historySize_].pos;
}

#include "rspch.hpp"
#include "mushroom.hpp"

void Mushroom::applyMushroomConfig() {
    NpcConfig cfg;
    cfg.maxHp          = 100.f;
    cfg.moveSpeed      = 2.5f;
    cfg.animRefSpeed   = 4.5f;   // authored speed of Mushroom_Walk
    cfg.attackRange    = 1.6f;
    cfg.detectionRange = 9.f;
    cfg.attackDamage   = 18.f;
    cfg.respawnDelay   = 15s;
    applyConfig(cfg);
    setHp(static_cast<int32>(cfg.maxHp));
}

void Mushroom::recordSnapshot(uint64 serverMs) {
    posHistory_[historyHead_] = { serverMs, pos() };
    historyHead_ = (historyHead_ + 1) % historySize_;
}

mu::Vec3 Mushroom::rewindPos(uint64 targetMs) const {
    for (int32 i = 1; i <= historySize_; ++i) {
        int32 idx = (historyHead_ - i + historySize_) % historySize_;
        if (posHistory_[idx].serverMs <= targetMs)
            return posHistory_[idx].pos;
    }
    return posHistory_[historyHead_ % historySize_].pos;
}

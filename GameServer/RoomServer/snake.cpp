#include "rspch.hpp"
#include "snake.hpp"

void Snake::applySnakeConfig() {
    NpcConfig cfg;
    cfg.maxHp          = 80.f;
    cfg.moveSpeed      = 3.5f;
    cfg.attackRange    = 1.8f;
    cfg.detectionRange = 12.f;
    cfg.attackDamage   = 12.f;
    cfg.respawnDelay   = 15s;
    applyConfig(cfg);
    setHp(static_cast<int32>(cfg.maxHp));
}

void Snake::recordSnapshot(uint64 serverMs) {
    posHistory_[historyHead_] = { serverMs, pos() };
    historyHead_ = (historyHead_ + 1) % historySize_;
}

mu::Vec3 Snake::rewindPos(uint64 targetMs) const {
    for (int32 i = 1; i <= historySize_; ++i) {
        int32 idx = (historyHead_ - i + historySize_) % historySize_;
        if (posHistory_[idx].serverMs <= targetMs)
            return posHistory_[idx].pos;
    }
    return posHistory_[historyHead_ % historySize_].pos;
}

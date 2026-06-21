#include "rspch.hpp"
#include "boss.hpp"

void Boss::applyBossConfig() {
    NpcConfig cfg;
    cfg.maxHp          = 2000.f;
    cfg.moveSpeed      = 3.5f;
    cfg.attackRange    = 3.0f;
    cfg.detectionRange = 30.f;
    cfg.attackDamage   = 40.f;   // legacy fallback only; skill hitboxes are authoritative
    cfg.respawnDelay   = 60s;
    applyConfig(cfg);
    setHp(static_cast<int32>(cfg.maxHp));
}

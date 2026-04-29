#include "rspch.hpp"
#include "npcGroup.hpp"
#include "Room.hpp"
#include "GameSession.hpp"
#include <algorithm>

// ─── 생성자 ───────────────────────────────────────────────────────────────────

NpcGroup::NpcGroup(int groupId, mu::Vec3 center, float radius,
                   Milliseconds memoryDuration)
    : groupId_(groupId)
    , activityCenter_(center)
    , activityRadius_(radius)
    , memoryDuration_(memoryDuration)
{}

// ─── 멤버 관리 ────────────────────────────────────────────────────────────────

void NpcGroup::addMember(uint32 npcId) {
    members_.push_back(npcId);
}

void NpcGroup::removeMember(uint32 npcId) {
    auto it = std::find(members_.begin(), members_.end(), npcId);
    if (it != members_.end()) members_.erase(it);
}

// ─── reportSight ──────────────────────────────────────────────────────────────

void MU_CALLCONV NpcGroup::reportSight(uint32 npcId, int32 playerId,
                                        mu::Vec3 pos, Milliseconds currentMs) {
    int slot = -1;
    for (int i = 0; i < MaxPlayerCount; ++i) {
        if (memories_[i].valid && memories_[i].playerId == playerId) {
            slot = i;
            break;
        }
    }
    if (slot == -1) {
        for (int i = 0; i < MaxPlayerCount; ++i) {
            if (!memories_[i].valid) { slot = i; break; }
        }
    }
    if (slot == -1) return;

    auto& m             = memories_[slot];
    m.playerId          = playerId;
    m.reporterNpcId     = npcId;
    m.lastKnownPosition = pos;
    m.lastSeenMs        = currentMs;
    m.expireMs          = currentMs + memoryDuration_;
    m.valid             = true;
}

// ─── hasValidMemory ───────────────────────────────────────────────────────────

bool NpcGroup::hasValidMemory(Milliseconds currentMs) const {
    for (const auto& m : memories_) {
        if (m.valid && currentMs <= m.expireMs) return true;
    }
    return false;
}

// ─── getBestMemory ────────────────────────────────────────────────────────────

const SharedTargetMemory* NpcGroup::getBestMemory(Milliseconds currentMs) const {
    const SharedTargetMemory* best = nullptr;
    for (const auto& m : memories_) {
        if (!m.valid || currentMs > m.expireMs) continue;
        if (!best || m.lastSeenMs > best->lastSeenMs)
            best = &m;
    }
    return best;
}

// ─── getBestMemoryInsideActivityArea ─────────────────────────────────────────

const SharedTargetMemory* NpcGroup::getBestMemoryInsideActivityArea(Milliseconds currentMs) const {
    const SharedTargetMemory* best = nullptr;
    for (const auto& m : memories_) {
        if (!m.valid || currentMs > m.expireMs) continue;
        if (!isInsideActivityArea(m.lastKnownPosition)) continue;
        if (!best || m.lastSeenMs > best->lastSeenMs)
            best = &m;
    }
    return best;
}

// ─── isInsideActivityArea ─────────────────────────────────────────────────────

bool MU_CALLCONV NpcGroup::isInsideActivityArea(mu::Vec3 pos) const {
    return (pos - activityCenter_).len2() <= activityRadius_ * activityRadius_;
}

// ─── clearMemory ─────────────────────────────────────────────────────────────

void NpcGroup::clearMemory() {
    for (auto& m : memories_) m = SharedTargetMemory{};
}

// ─── clearMemoryIfPlayerOutside ───────────────────────────────────────────────

void NpcGroup::clearMemoryIfPlayerOutside(Room& room) {
    for (auto& m : memories_) {
        if (!m.valid) continue;
        GameSession* s = room.findLivingSessionByPlayerId(m.playerId);
        if (!s || !isInsideActivityArea(s->player()->pos()))
            m = SharedTargetMemory{};
    }
}

// ─── update ──────────────────────────────────────────────────────────────────

void NpcGroup::update(Milliseconds currentMs) {
    for (auto& m : memories_) {
        if (m.valid && currentMs > m.expireMs)
            m = SharedTargetMemory{};
    }
}

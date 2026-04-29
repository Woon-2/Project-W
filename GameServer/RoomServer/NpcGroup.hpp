#pragma once
#include "../common/mathUtil.hpp"
#include <array>
#include <vector>

class Room;

static constexpr int MaxPlayerCount = 4;

// ─── SharedTargetMemory ──────────────────────────────────────────────────────
// 그룹 내 NPC 중 한 명이 보고한 플레이어 위치 기억.
// 플레이어 1명당 슬롯 1개 (playerId로 식별).
struct SharedTargetMemory {
    int32        playerId      = -1;  // -1 = 빈 슬롯
    uint32       reporterNpcId = 0;
    mu::Vec3     lastKnownPosition{};
    Milliseconds lastSeenMs{ 0ms };
    Milliseconds expireMs  { 0ms };
    bool         valid      = false;
};

// ─── NpcGroup ────────────────────────────────────────────────────────────────
// 경량 시야 공유 그룹. 지휘 계층 없음, NPC에게 명령 없음.
// Room이 소유하고, NPC는 groupId를 통해 조회만 한다.
class NpcGroup {
public:
    NpcGroup(int groupId, mu::Vec3 center, float radius,
             Milliseconds memoryDuration = Milliseconds{ 3000 });

    // ── 멤버 관리 ──────────────────────────────────────────────────────────
    void addMember   (uint32 npcId);
    void removeMember(uint32 npcId);

    // ── 시야 공유: NPC가 플레이어를 감지하면 호출 ──────────────────────────
    void MU_CALLCONV reportSight(uint32 npcId, int32 playerId,
                                 mu::Vec3 pos, Milliseconds currentMs);

    // ── 메모리 쿼리 ────────────────────────────────────────────────────────
    bool                      hasValidMemory                 (Milliseconds currentMs) const;
    const SharedTargetMemory* getBestMemory                  (Milliseconds currentMs) const;
    const SharedTargetMemory* getBestMemoryInsideActivityArea(Milliseconds currentMs) const;

    // ── 활동 구역 경계 판정 ────────────────────────────────────────────────
    bool MU_CALLCONV isInsideActivityArea(mu::Vec3 pos) const;

    // ── 메모리 초기화 / 틱 갱신 (NPC 업데이트 전 호출) ────────────────────
    void clearMemory();
    void clearMemoryIfPlayerOutside(Room& room);
    void update(Milliseconds currentMs);

    // ── 접근자 ─────────────────────────────────────────────────────────────
    int      getGroupId() const { return groupId_; }
    mu::Vec3 getCenter () const { return activityCenter_; }
    float    getRadius () const { return activityRadius_; }

private:
    int          groupId_;
    mu::Vec3     activityCenter_;
    float        activityRadius_;
    Milliseconds memoryDuration_;

    std::vector<uint32>                          members_;
    std::array<SharedTargetMemory, MaxPlayerCount> memories_{};
};

#pragma once
#include "object.hpp"
#include <vector>
#include <optional>

class Room;
class GameSession;

// ─── NpcUpdateResult ─────────────────────────────────────────────────────────
struct NpcUpdateResult {
    struct HitInfo { uint16 targetId; int32 newHp; };
    std::optional<HitInfo> hit;
};

// ─── NpcConfig ───────────────────────────────────────────────────────────────
struct NpcConfig {
    float maxHp              = 80.f;
    float moveSpeed          = 4.f;
    float detectionRange     = 10.f;
    float attackRange        = 2.f;
    float activityZoneRadius = 28.f;
    float attackDamage       = 10.f;
    Seconds attackWindupTime { 0.4f };
    Seconds attackRecoverTime{ 0.6f };
    float separationRadius   = 4.f;
    float separationWeight   = 0.6f;
    bool  canReAggroOnReturn = true;
    int   overlapThreshold   = 2;
    float returnSpeedMult    = 2.5f;
};

// ─── NpcState ────────────────────────────────────────────────────────────────
enum class NpcState {
    Idle,           // 대기; 자율 타깃 선택
    Chase,          // 추격
    AttackWindup,   // 공격 준비 (이동 없음)
    AttackRecover,  // 공격 후 딜레이
    Return,         // 스폰 복귀
    Reposition,     // 과밀 회피 이동
    Dead,           // 종료 상태
    Investigate,    // 그룹 공유 기억 위치 조사
};

// ─── Npc ─────────────────────────────────────────────────────────────────────
class Npc : public Object {
public:
    Npc() = default;
    Npc(Object&& base, const NpcConfig& cfg = {});

    NpcUpdateResult update(Seconds dt, Room& room);

    NpcState getState()    const { return state_; }
    int32    getTargetId() const { return targetId_; }  // countNpcsTargeting용 — public 필수
    int      getGroupId()  const { return groupId_; }
    void     setGroupId(int id)  { groupId_ = id; }
    void     MU_CALLCONV setSpawnPos    (mu::Vec3 p);
    void     MU_CALLCONV setActivityZone(mu::Vec3 center, float radius);

protected:
    void applyConfig(const NpcConfig& cfg);

private:
    void transitionTo(NpcState next);

    NpcUpdateResult updateIdle         (Seconds dt, Room& room);
    NpcUpdateResult updateChase        (Seconds dt, Room& room);
    NpcUpdateResult updateAttackWindup (Seconds dt, Room& room);
    NpcUpdateResult updateAttackRecover(Seconds dt, Room& room);
    NpcUpdateResult updateReturn       (Seconds dt, Room& room);
    NpcUpdateResult updateReposition   (Seconds dt, Room& room);
    NpcUpdateResult updateDead         ();
    NpcUpdateResult updateInvestigate  (Seconds dt, Room& room);

    GameSession* selectBestTarget(Room& room) const;
    float        evaluateTargetScore(GameSession* s, Room& room) const;
    mu::Vec3     MU_CALLCONV calcSeparationForce(const std::vector<mu::Vec3>& nearby) const;
    bool         isOutsideActivityZone() const;
    bool         isOvercrowded        (const std::vector<mu::Vec3>& nearby) const;

    NpcState state_{ NpcState::Idle };
    mu::Vec3 spawnPos_{};
    mu::Vec3 activityZoneCenter_{};
    float    activityZoneRadius_{ 28.f };
    int32    targetId_{ -1 };
    int      groupId_{ -1 };

    float detectionRange_{ 10.f };
    float attackRange_   { 2.f };
    float moveSpeed_     { 4.f };
    float attackDamage_  { 10.f };
    Seconds attackWindupTime_ { 0.4f };
    Seconds attackRecoverTime_{ 0.6f };
    float separationRadius_ { 4.f };
    float separationWeight_ { 0.6f };
    bool  canReAggroOnReturn_{ true };
    int   overlapThreshold_  { 2 };
    float returnSpeedMult_   { 2.5f };

    Seconds windupTimer_    { 0s };
    Seconds recoverTimer_   { 0s };
    Seconds targetEvalTimer_{ 0s };

    mu::Vec3 repositionDir_  { 1.f, 0.f, 0.f };
    Seconds  repositionTimer_{ 0s };

    std::vector<mu::Vec3> nearbyCache_;

    static constexpr Seconds TARGET_EVAL_INTERVAL{ 0.5f };
    static constexpr Seconds REPOSITION_TIMEOUT  { 1.5f };
};

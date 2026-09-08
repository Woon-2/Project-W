#ifndef npc_hpp
#define npc_hpp

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
    float   maxHp        = 80.f;
    Seconds respawnDelay { 10.f };
    float moveSpeed          = 4.f;
    // Walk 클립이 저작된 기준 이동 속력(m/s). 서버가 이 값으로 로코모션 재생 배속을 맞춰
    // 클라의 발 미끄러짐 보정과 본 포즈를 일치시킨다.
    //
    // **이 값은 엔티티가 아니라 애니메이션 클립셋의 속성이다.** 같은 클립셋을 쓰는 변종은
    // moveSpeed가 달라도 같은 값을 써야 한다(Hobgoblin→Goblin, Grandbaum→Treant, Isys→Birdy).
    // 클라의 대응 상수(`client/object.cpp` 각 AnimBlender의 kRefSpeedWalk)와 **반드시 같은 값**.
    // 기본값 3.0은 미측정 클립용이며, 각 applyXXXConfig에서 명시적으로 지정한다.
    float animRefSpeed       = 3.f;
    // 클라 블렌더에서 idle→이동 블렌드 가중치가 1에 도달하는 속력(m/s).
    // 몬스터 블렌더는 전부 `walkThreshold(0.06) + 3.f` = 3.06으로 동일하다.
    // 보스만 run 밴드 끝(5.0)을 쓴다. refSpeed와 함께 서버 재생 배속 공식에 들어간다.
    float animBandEnd        = 3.06f;
    float detectionRange     = 10.f;
    float attackRange        = 2.f;
    float attackDamage       = 10.f;
    Seconds attackWindupTime { 0.4f };
    Seconds attackRecoverTime{ 1.f };
    float separationRadius   = 2.5f;
    float separationWeight   = 0.6f;
    bool  canReAggroOnReturn = true;
    int   overlapThreshold   = 3;
    float returnSpeedMult    = 2.5f;
    Seconds maxDirectReactDelay{ 0.3s };
    Seconds maxGroupReactDelay { 2.0s };
    float patrolRadius       = 5.f;    // 스폰 기준 배회 반경
    float patrolSpeedMult    = 0.4f;   // moveSpeed 대비 순찰 속도 배율
    Seconds minIdleTime  { 1.5s };
    Seconds maxIdleTime  { 4.0s };
    Seconds minPatrolTime{ 3.0s };
    Seconds maxPatrolTime{ 6.0s };
    float motorMaxAcceleration = 20.f;  // m/s²
    float motorMaxDeceleration = 40.f;  // m/s²
    float motorGain            = 10.f;
};

// ─── NpcState ────────────────────────────────────────────────────────────────
enum class NpcState {
    Idle,           // 대기; 자율 타깃 선택
    Patrol,         // 순찰; 스폰 근처 배회
    Chase,          // 추격
    AttackWindup,   // 공격 준비 (이동 없음)
    AttackRecover,  // 공격 후 딜레이
    Return,         // 스폰 복귀
    Reposition,     // 과밀 회피 이동
    Dead,           // 종료 상태
    Investigate,    // 그룹 공유 기억 위치 조사
};

// A skill-based attack option: the skill asset to cast plus the server anim clip
// key to play, so the hitbox bone transforms track the skill's timeline.
struct NpcAttack {
    uint32      skillId = 0;
    std::string clipKey;
};

// ─── Npc ─────────────────────────────────────────────────────────────────────
class Npc : public Object {
public:
    Npc() = default;
    Npc(Object&& base, const NpcConfig& cfg = {});

    // virtual so FinalBoss can replace the FSM tick with its BehaviorTree.
    virtual NpcUpdateResult update(Seconds dt, Room& room);
    void onHitImpulse() override;

    // 필드 일반 몬스터: 보석 1~2개.
    int32 rollGemDropCount() override { return 1 + (rand() % 2); }

    // Register a skill-based attack (called from Room::setupX). With at least one
    // attack the NPC casts skills (hitbox = authoritative damage); with none it
    // falls back to the legacy direct-damage melee in updateAttackWindup.
    void addAttack(uint32 skillId, std::string clipKey);
    bool hasSkillAttacks() const { return !attacks_.empty(); }

    NpcState getState()    const { return state_; }
    int32    maxHp()       const { return static_cast<int32>(maxHp_); }  // network/UI HP (AI math uses float maxHp_)
    int32    getTargetId() const { return targetId_; }  // countNpcsTargeting용 — public 필수
    int      getGroupId()  const { return groupId_; }
    void     setGroupId(int id)  { groupId_ = id; }
    void     MU_CALLCONV setSpawnPos    (mu::Vec3 p);
    void     MU_CALLCONV setActivityZone(mu::Vec3 center, float radius);

    // Stronghold-driven revival (fixed-pool population model): restore HP, move
    // to a new home position, and reset AI state. Replaces per-NPC self-respawn.
    void     MU_CALLCONV reviveAt(mu::Vec3 pos);

protected:
    void applyConfig(const NpcConfig& cfg);

    // Config/state accessors for derived AIs (e.g. FinalBoss BehaviorTree leaves).
    float moveSpeed()   const { return moveSpeed_; }
    float attackRange() const { return attackRange_; }
    const std::vector<NpcAttack>& attacks() const { return attacks_; }

private:
    void transitionTo(NpcState next);

    NpcUpdateResult updateIdle         (Seconds dt, Room& room);
    NpcUpdateResult updatePatrol       (Seconds dt, Room& room);
    NpcUpdateResult updateChase        (Seconds dt, Room& room);
    NpcUpdateResult updateAttackWindup (Seconds dt, Room& room);
    NpcUpdateResult updateAttackRecover(Seconds dt, Room& room);
    NpcUpdateResult updateReturn       (Seconds dt, Room& room);
    NpcUpdateResult updateReposition   (Seconds dt, Room& room);
    NpcUpdateResult updateDead         (Seconds dt);
    NpcUpdateResult updateInvestigate  (Seconds dt, Room& room);

    bool         checkAlert(Seconds dt, Room& room);  // 감지/반응 판정(전환 또는 대기 시 true)
    mu::Vec3     pickPatrolDest() const;              // 스폰 근처 랜덤 웨이포인트
    const NpcAttack& pickAttack() const;              // 다중공격 확률 선택(균등). attacks_ 비어있지 않을 때만 호출

    GameSession* selectBestVisibleTarget(Room& room) const;
    float        evaluateTargetScore(GameSession* s, Room& room) const;
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
    float patrolRadius_      { 5.f };
    float patrolSpeedMult_   { 0.4f };
    Seconds minIdleTime_  { 1.5s };  Seconds maxIdleTime_  { 4.0s };
    Seconds minPatrolTime_{ 3.0s };  Seconds maxPatrolTime_{ 6.0s };

    float   maxHp_          { 80.f };
    Seconds respawnDelay_   { 10s };
    Seconds respawnTimer_   { 0s };

    Seconds windupTimer_    { 0s };
    Seconds recoverTimer_   { 0s };
    Seconds targetEvalTimer_{ 0s };

    std::vector<NpcAttack> attacks_;       // skill-based attacks (empty => legacy melee)
    bool                   attackCast_{ false };  // one skill cast per windup

    mu::Vec3 repositionDir_  { 1.f, 0.f, 0.f };
    Seconds  repositionTimer_{ 0s };

    Seconds  idleTimer_      { 0s };
    Seconds  patrolTimer_    { 0s };
    Seconds  patrolDuration_ { 0s };
    mu::Vec3 patrolDest_     {};

    Seconds directReactTimer_   { -1s };
    Seconds groupReactTimer_    { -1s };
    Seconds maxDirectReactDelay_{ 0.3s };
    Seconds maxGroupReactDelay_ { 2.0s };

    std::vector<mu::Vec3> nearbyCache_;

    static constexpr Seconds TARGET_EVAL_INTERVAL  { 0.5f };
    static constexpr Seconds REPOSITION_TIMEOUT    { 1.5f };
};

#endif // npc_hpp
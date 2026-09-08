#ifndef final_boss_hpp
#define final_boss_hpp

#include "Npc.hpp"
#include "BehaviorTree.hpp"
#include <memory>
#include <vector>

// Final boss: a standalone field NPC (1:1 combat, NOT a tactical/platoon unit).
// Unlike normal monsters (which use the Npc FSM) the final boss is driven by a
// dedicated BehaviorTree. Its attack leaves reuse the very same skill path as the
// other monsters (Room::skillStartInternal -> authoritative hitbox damage +
// S_SkillStart broadcast); the difference is BT situational selection instead of
// the FSM's uniform-random pickAttack. It spawns only inside the boss arena zone,
// so it has no detection range: every living player is always a candidate target.
class FinalBoss : public Npc {
public:
    FinalBoss() = default;
    FinalBoss(Object&& base) : Npc(std::move(base)) {}

    void applyBossConfig();

    // Drives the BehaviorTree instead of the Npc FSM (Npc::update is virtual).
    NpcUpdateResult update(Seconds dt, Room& room) override;

    // 최종 보스: 보석 10개.
    int32 rollGemDropCount() override { return 10; }

    // Builds btRoot_. Call from Room::setupFinalBoss AFTER addAttack() so the
    // attack leaves can resolve their skill ids/clip keys by index.
    void buildBehaviorTree();

    void setDamageScale(float s) { damageScale_ = s; }

    // -- BehaviorTree leaf helpers (called from the leaf action nodes) -----------
    // Resolve the current target session (alive-checked); nullptr if none.
    GameSession* resolveTarget(Room& room) const;
    // XZ distance to the current target; very large if no target.
    float distanceToTarget(Room& room) const;
    // Score every living player (no detection range) and (re)select the best.
    void evaluateTarget(Seconds dt, Room& room);
    // Steer toward dest on the XZ plane (preserves vertical velocity).
    // The horizontal speed *ramps* toward moveSpeed()*speedMult at MOVE_ACCEL/MOVE_DECEL
    // instead of snapping: clients infer the gait from the broadcast velocity, so an
    // instant 3.5 -> 8.75 jump crosses the whole walk<->run blend band in one frame and
    // the crossfade reads as a hitch. Ramping sweeps the band over ~0.4s.
    // reorient = false keeps the current facing, so the boss side-steps toward dest
    // instead of turning into it (used by the strafe leg of engage()).
    void MU_CALLCONV moveToward(mu::Vec3 dest, float speedMult, Seconds dt, bool reorient = true);
    // Rotate (yaw only) to look at a world position.
    void MU_CALLCONV faceToward(mu::Vec3 worldPos);
    // Stop dead (horizontal velocity to zero) and collapse the speed ramp. Casts, idle and
    // death all use this -- the boss commits to a standing swing, never a walking one.
    void halt();
    // Cast the registered attack at index (halts, faces target, switches clip, fires the
    // authoritative skill via Room::skillStartInternal with damageScale_).
    void castAttack(Room& room, int attackIdx);
    // One tick of the approach behaviour: circle the target at walk speed, then commit to a
    // straight run. Called from BossEngageAction (the Combat selector's fallback leaf).
    void engage(Seconds dt, Room& room, GameSession& target);

    float bossAttackRange() const { return attackRange(); }  // Npc protected accessor
    // Distance at which the boss actually commits to a swing. Deliberately well inside
    // attackRange: committing at the very edge let the target dodge by taking one step
    // back, because the swing itself is a standing animation.
    float attackCommitRange() const { return attackRange() * ATTACK_COMMIT_FRACTION; }
    // True for a short window right after a charge lands, so the tree can answer the run-in
    // with the heavy attack instead of a filler swing.
    bool  justCharged() const { return postChargeTimer_ > 0s; }

private:
    // 거리(근접) + 저HP(마무리) + 위협(최근 보스를 때린 플레이어) 가중 합으로 타깃을 점수화한다.
    // recentDamagers = evaluateTarget이 1회 수집한 보스의 최근 damager id 목록(위협 판정용).
    float scoreTarget(GameSession* s, const std::vector<int32>& recentDamagers) const;

    std::unique_ptr<bt::BtNode> btRoot_;

    int32   targetId_{ -1 };               // session/player id, -1 = none
    Seconds targetEvalTimer_{ 0s };
    float   damageScale_{ 1.0f };           // caster damage multiplier for skill hits
    bool    raged_{ false };                // reserved for a future rage/phase branch

    // -- 접근 상태 (engage) --------------------------------------------------------
    float   curSpeed_{ 0.f };               // moveToward의 속도 램프 상태 (m/s)
    bool    charging_{ false };             // 질주 돌입 래치 (해제 = 선회 중)
    Seconds chargeTimer_{ 0s };             // 다음 질주까지 남은 선회 시간
    Seconds strafeFlipTimer_{ 0s };         // 선회 방향 반전까지 남은 시간
    Seconds postChargeTimer_{ 0s };         // 질주 직후 "강타 우선" 창 (justCharged)
    float   strafeDir_{ 1.f };              // 현재 선회 방향 [-1,1] (0을 지나며 연속 반전)
    float   strafeDirTarget_{ 1.f };        // +1 = 시계, -1 = 반시계

    static constexpr Seconds TARGET_EVAL_INTERVAL{ 0.5f };

    // -- 이동/질주 (게임플레이 튜너블) ---------------------------------------------
    // moveSpeed는 걷기 속도이고, 질주는 그 배수다. 클라는 속도로 모션을 추론하므로
    // (AnimBlenderBoss의 run 밴드 4.0~7.0) 질주 속도가 그 밴드를 넘어야 실제로 달려 보인다.
    static constexpr float RUN_SPEED_MULT = 2.5f;
    // 속도 램프(m/s^2). 걷기 3.5 <-> 질주 8.75 전환이 클라 블렌드 밴드(4.0~7.0)를 훑고
    // 지나가게 한다. 가속 12.0이면 3.5->8.75에 0.44s(2.7m), 감속 14.0이면 반대로 0.38s.
    // 너무 낮추면 돌진이 램프만 하다 끝나 "달려온다"로 안 읽힌다 -- STRAFE_RADIUS와 같이 본다.
    static constexpr float MOVE_ACCEL = 12.0f;
    static constexpr float MOVE_DECEL = 14.0f;

    // -- 교전 거리/선회 (게임플레이 튜너블) ----------------------------------------
    // 공격 확정 거리 = attackRange x 이 비율. 낮출수록 더 깊이 파고든 뒤에 휘두른다.
    static constexpr float ATTACK_COMMIT_FRACTION = 0.6f;   // 3.0 x 0.6 = 1.8m
    // 선회 시 유지하려는 반경, 그리고 반경 오차를 접선 이동에 섞는 비중.
    // 반경은 돌진 거리이기도 하다: 6.0m면 램프(2.7m) 뒤에도 1.5m 정도를 최고 속도로 달려
    // 클라가 실제로 Boss_Run을 보여준다. 4.5m로 낮추면 램프 도중에 도착해 버린다.
    static constexpr float STRAFE_RADIUS      = 6.0f;
    static constexpr float STRAFE_RADIAL_GAIN = 0.8f;
    // 이 거리를 넘으면 선회를 건너뛰고 무조건 질주로 붙는다(원거리에서 빙빙 돌지 않게).
    // STRAFE_RADIUS보다 충분히 커야 선회 중 상시 발동하지 않는다.
    static constexpr float CHARGE_FORCE_DISTANCE = 9.0f;
    // 선회 방향 반전 주기와, 선회하다 갑자기 질주로 돌입하기까지의 간격(각각 균등 랜덤).
    static constexpr Seconds STRAFE_FLIP_MIN{ 1.2f };
    static constexpr Seconds STRAFE_FLIP_MAX{ 2.8f };
    // 방향 반전 속도(초당). 즉시 뒤집으면 클라의 4방향 walk가 한 프레임에 Left<->Right로
    // 튄다 -- 0을 지나며 연속으로 넘겨야 발을 딛고 반대로 도는 것처럼 보인다.
    static constexpr float   STRAFE_TURN_RATE = 3.0f;   // -1 -> +1 완전 반전에 약 0.67s
    static constexpr Seconds CHARGE_INTERVAL_MIN{ 2.0f };
    static constexpr Seconds CHARGE_INTERVAL_MAX{ 4.5f };
    // 질주가 닿은 뒤 붙어서 몰아치는 창. 이 동안은 선회하지 않고 사거리 안에 머물러
    // 근접 쿨다운이 한두 번 더 터지게 한다(강타 Smite도 이 창에서만 선택된다).
    // 짧게 잡으면 돌진 1회당 공격 1회로 끝나 보스가 지나치게 소극적으로 보인다.
    static constexpr Seconds POST_CHARGE_WINDOW{ 3.0f };
    // 압박 중 유지 거리 = attackCommitRange x 이 비율. 이 안쪽이면 멈춰 선다
    // (틱 단위로 걷기/정지가 깜빡이지 않도록 두는 데드존).
    static constexpr float   PRESS_HOLD_FRACTION = 0.7f;

    // -- 타깃 점수 가중치/윈도우 (게임플레이 튜너블) --------------------------------
    static constexpr float        TARGET_W_PROXIMITY  = 1.0f;     // 근접 가중
    static constexpr float        TARGET_W_LOWHP      = 0.6f;     // 저HP(마무리) 가중
    static constexpr float        TARGET_W_THREAT     = 0.8f;     // 위협(최근 공격자) 가중
    static constexpr float        TARGET_STICKY_BONUS = 0.15f;    // 현재 타깃 유지 가산(히스테리시스, 0=비활성)
    static constexpr Milliseconds TARGET_THREAT_WINDOW{ 5000.f }; // 위협 인정 최근 시간창
};

#endif // final_boss_hpp

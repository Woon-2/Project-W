#ifndef behavior_tree_hpp
#define behavior_tree_hpp

#include <vector>
#include <memory>
#include <functional>

// 최종보스 전용 소형 Behavior Tree 프레임워크 (NPCAI 이식).
// 일반 NPC/미드보스는 기존 FSM을 유지하고, 최종보스만 이 트리를 사용한다.
// 컴포지트는 Running 자식을 기억해 다음 틱에 이어서 실행하며,
// Selector는 기억한 자식보다 앞선(높은 우선순위) 가지를 매 틱 재평가하여
// 인터럽트를 허용한다.
//
// tick()이 받는 BtContext(블랙보드 + 월드 접근)는 보스 포팅 단계에서 정의한다.
// 프레임워크는 ctx를 자식에게 참조로 전달만 하므로 여기서는 전방 선언으로 충분하다.
// 시간 인자는 GameServer 표준 Seconds(= std::chrono::duration<float>, sepch.hpp)를
// 쓰며, IMidBossTactic::update(Seconds, ...)와 시그니처를 맞춘다. Seconds는 PCH
// (rspch.hpp -> sepch.hpp)로 제공된다.

namespace bt {

struct BtContext;

enum class BtStatus { Success, Failure, Running };

// ─── BtNode ─────────────────────────────────────────────────────────────────

class BtNode {
public:
    virtual ~BtNode() = default;

    virtual BtStatus tick(Seconds dt, BtContext& ctx) = 0;

    // Running 도중 다른 가지에 선점당했을 때 내부 상태 초기화
    virtual void reset() {}
};

// ─── BtSelector ─────────────────────────────────────────────────────────────
// 자식을 순서대로 평가해 Success/Running이면 즉시 반환한다.
// 매 틱 첫 자식부터 재평가하므로 상위 가지가 살아나면
// 기억해 둔 Running 자식은 reset() 후 선점된다.

class BtSelector : public BtNode {
public:
    void addChild(std::unique_ptr<BtNode> child);

    BtStatus tick(Seconds dt, BtContext& ctx) override;
    void reset() override;

private:
    std::vector<std::unique_ptr<BtNode>> children_;
    int runningIndex_{ -1 };
};

// ─── BtSequence ─────────────────────────────────────────────────────────────
// 자식을 순서대로 평가해 Failure/Running이면 즉시 반환한다.
// Running 자식을 기억하고 다음 틱에 그 자식부터 재개한다 (패턴 도중 중단 방지).

class BtSequence : public BtNode {
public:
    void addChild(std::unique_ptr<BtNode> child);

    BtStatus tick(Seconds dt, BtContext& ctx) override;
    void reset() override;

private:
    std::vector<std::unique_ptr<BtNode>> children_;
    int runningIndex_{ -1 };
};

// ─── BtCondition ────────────────────────────────────────────────────────────
// bool 함수 래퍼. true=Success, false=Failure.

class BtCondition : public BtNode {
public:
    using Predicate = std::function<bool(BtContext&)>;

    explicit BtCondition(Predicate pred)
        : pred_(std::move(pred)) {}

    BtStatus tick(Seconds dt, BtContext& ctx) override;

private:
    Predicate pred_;
};

// ─── BtCooldown ─────────────────────────────────────────────────────────────
// 데코레이터: 자식이 Success를 반환하면 cooldown 동안 Failure를 반환한다.
// 자식이 Failure면 쿨다운을 시작하지 않는다.

class BtCooldown : public BtNode {
public:
    BtCooldown(Seconds cooldown, std::unique_ptr<BtNode> child)
        : cooldown_(cooldown), child_(std::move(child)) {}

    BtStatus tick(Seconds dt, BtContext& ctx) override;
    void reset() override;

private:
    Seconds cooldown_;
    Seconds timer_{ 0.f };   // 남은 쿨다운 (0 이하 = 사용 가능)
    std::unique_ptr<BtNode> child_;
};

} // namespace bt

#endif // behavior_tree_hpp

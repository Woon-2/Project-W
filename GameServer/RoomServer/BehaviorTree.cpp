#include "rspch.hpp"
#include "BehaviorTree.hpp"

namespace bt {

// ─── BtSelector ─────────────────────────────────────────────────────────────

void BtSelector::addChild(std::unique_ptr<BtNode> child) {
    children_.push_back(std::move(child));
}

BtStatus BtSelector::tick(Seconds dt, BtContext& ctx) {
    for (int i = 0; i < static_cast<int>(children_.size()); ++i) {
        BtStatus s = children_[i]->tick(dt, ctx);

        if (s == BtStatus::Running || s == BtStatus::Success) {
            // 상위 가지가 선점했으면 기억해 둔 Running 자식을 초기화
            if (runningIndex_ >= 0 && runningIndex_ != i)
                children_[runningIndex_]->reset();
            runningIndex_ = (s == BtStatus::Running) ? i : -1;
            return s;
        }

        // Failure: 기억한 자식이 실패로 끝났으면 기억 해제 후 다음 가지로
        if (runningIndex_ == i)
            runningIndex_ = -1;
    }
    return BtStatus::Failure;
}

void BtSelector::reset() {
    if (runningIndex_ >= 0)
        children_[runningIndex_]->reset();
    runningIndex_ = -1;
}

// ─── BtSequence ─────────────────────────────────────────────────────────────

void BtSequence::addChild(std::unique_ptr<BtNode> child) {
    children_.push_back(std::move(child));
}

BtStatus BtSequence::tick(Seconds dt, BtContext& ctx) {
    int start = (runningIndex_ >= 0) ? runningIndex_ : 0;

    for (int i = start; i < static_cast<int>(children_.size()); ++i) {
        BtStatus s = children_[i]->tick(dt, ctx);

        if (s == BtStatus::Running) {
            runningIndex_ = i;
            return s;
        }
        if (s == BtStatus::Failure) {
            runningIndex_ = -1;
            return s;
        }
    }
    runningIndex_ = -1;
    return BtStatus::Success;
}

void BtSequence::reset() {
    if (runningIndex_ >= 0)
        children_[runningIndex_]->reset();
    runningIndex_ = -1;
}

// ─── BtCondition ────────────────────────────────────────────────────────────

BtStatus BtCondition::tick(Seconds /*dt*/, BtContext& ctx) {
    return pred_(ctx) ? BtStatus::Success : BtStatus::Failure;
}

// ─── BtCooldown ─────────────────────────────────────────────────────────────

BtStatus BtCooldown::tick(Seconds dt, BtContext& ctx) {
    if (timer_.count() > 0.f) {
        timer_ -= dt;
        return BtStatus::Failure;
    }

    BtStatus s = child_->tick(dt, ctx);
    if (s == BtStatus::Success)
        timer_ = cooldown_;
    return s;
}

void BtCooldown::reset() {
    // 쿨다운 타이머는 유지하고 자식 진행 상태만 초기화
    child_->reset();
}

} // namespace bt

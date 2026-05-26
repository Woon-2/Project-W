#include "rspch.hpp"
#include "TacticalSquad.hpp"
#include "Room.hpp"
#include "GameSession.hpp"
#include <cassert>
#include <cmath>
#include <algorithm>

static mu::Vec3 norm3(mu::Vec3 v) {
    float l = v.len();
    return l > 0.0001f ? v * (1.f / l) : mu::Vec3(0.f, 0.f, 0.f);
}

// ─── 생성자 ───────────────────────────────────────────────────────────────────

TacticalSquad::TacticalSquad(int squadId, float memberAttackRange, float memberSeparationRadius)
    : squadId_(squadId)
    , memberAttackRange_(memberAttackRange)
    , memberSeparationRadius_(memberSeparationRadius)
{}

// ─── 멤버 관리 ────────────────────────────────────────────────────────────────

void TacticalSquad::addMember(TacticalNpc* npc) {
    assert(npc != nullptr);
    assert(memberIds_.size() == memberCache_.size());
    memberIds_.push_back(npc->getId());
    memberCache_.push_back(npc);
}

void TacticalSquad::removeMember(uint32_t npcId) {
    for (size_t i = 0; i < memberIds_.size(); ++i) {
        if (memberIds_[i] == npcId) {
            memberIds_.erase(memberIds_.begin() + static_cast<ptrdiff_t>(i));
            memberCache_.erase(memberCache_.begin() + static_cast<ptrdiff_t>(i));
            return;
        }
    }
}

void TacticalSquad::receiveOrder(const SquadOrder& order) {
    leaderlessBrawlEnabled_  = false;
    leaderlessBrawlTimer_    = 0.f;
    leaderlessRetargetTimer_ = 0.f;
    boxRefreshTimer_         = 0.f;
    currentOrder_            = order;
    orderDirty_              = true;
    wedgePrepared_           = false;
    wedgeMemberIds_.clear();
    wedgeMemberCache_.clear();
    wedgePrepareSlots_.clear();
    wedgeExitSlots_.clear();
    activeWedgeChargeId_ = 0;
}

void MU_CALLCONV TacticalSquad::updateBoxLeaderPos(mu::Vec3 pos) {
    if (currentOrder_.type == SquadOrderType::BoxAdvance)
        currentOrder_.leaderPos = pos;
}

// ─── update ───────────────────────────────────────────────────────────────────

void TacticalSquad::update(Seconds dt, Room& room) {
    if (memberIds_.empty()) return;

    float dtf = dt.count();

    if (leaderlessBrawlEnabled_) {
        updateLeaderlessBrawl(dtf, room);
        return;
    }

    if (orderDirty_) {
        pushCommandsToMembers(room);
        orderDirty_ = false;
        return;
    } else if (currentOrder_.type == SquadOrderType::BoxAdvance) {
        boxRefreshTimer_ -= dtf;
        if (boxRefreshTimer_ <= 0.f) {
            pushCommandsToMembers(room);
            boxRefreshTimer_ = 0.1f;
        }
        return;
    }
    if (currentOrder_.type == SquadOrderType::WedgeCharge &&
        !wedgePrepared_ && areMembersAtSlots()) {
        wedgePrepared_ = true;
        pushCommandsToMembers(room);
    }
    if (currentOrder_.type == SquadOrderType::WedgeCharge &&
        wedgePrepared_ && activeWedgeChargeId_ != 0 &&
        areChargeMembersComplete()) {
        room.endWedgeCharge(activeWedgeChargeId_);
        activeWedgeChargeId_ = 0;
    }
}

// ─── leaderless brawl ─────────────────────────────────────────────────────────

void TacticalSquad::updateLeaderlessBrawl(float dt, Room& room) {
    if (leaderlessBrawlTimer_ > 0.f) {
        leaderlessBrawlTimer_ -= dt;
        if (leaderlessBrawlTimer_ > 0.f)
            return;
        leaderlessRetargetTimer_ = 0.f;
    }

    leaderlessRetargetTimer_ -= dt;
    if (leaderlessRetargetTimer_ > 0.f)
        return;
    leaderlessRetargetTimer_ = LEADERLESS_RETARGET_INTERVAL;

    currentOrder_ = {};
    currentOrder_.targetId = selectNearestPlayerToSquad(room);
    currentOrder_.type = (currentOrder_.targetId != 0)
        ? SquadOrderType::Engage
        : SquadOrderType::Idle;
    orderDirty_           = false;
    wedgePrepared_        = false;
    wedgeMemberIds_.clear();
    wedgePrepareSlots_.clear();
    wedgeExitSlots_.clear();
    activeWedgeChargeId_  = 0;

    pushCommandsToMembers(room);
}

uint32_t TacticalSquad::selectNearestPlayerToSquad(Room& room) const {
    mu::Vec3 center    = calcCentroid();
    uint32_t bestId    = 0;
    float    bestDistSq = -1.f;

    for (GameSession* s : room.getLivingPlayers()) {
        if (!s) continue;
        float distSq = (center - s->player()->pos()).len2();
        uint32_t sid = static_cast<uint32_t>(s->id());
        if (bestId == 0 || distSq < bestDistSq ||
            (distSq == bestDistSq && sid < bestId)) {
            bestId     = sid;
            bestDistSq = distSq;
        }
    }
    return bestId;
}

// ─── removeDeadMembers ────────────────────────────────────────────────────────

void TacticalSquad::removeDeadMembers() {
    size_t w = 0;
    for (size_t r = 0; r < memberIds_.size(); ++r) {
        if (memberCache_[r] && memberCache_[r]->hp() > 0) {
            memberIds_[w]   = memberIds_[r];
            memberCache_[w] = memberCache_[r];
            ++w;
        }
    }
    memberIds_.resize(w);
    memberCache_.resize(w);
}

mu::Vec3 TacticalSquad::calcCentroid() const {
    mu::Vec3 sum{};
    int count = 0;
    for (TacticalNpc* tnpc : memberCache_) {
        if (tnpc && tnpc->hp() > 0) {
            sum += tnpc->pos();
            ++count;
        }
    }
    return (count > 0) ? (sum * (1.f / static_cast<float>(count))) : mu::Vec3{};
}

bool TacticalSquad::areMembersAtSlots() const {
    bool anyAlive = false;
    for (TacticalNpc* tnpc : memberCache_) {
        if (!tnpc || tnpc->hp() <= 0) continue;
        anyAlive = true;
        if (!tnpc->isAtSlot()) return false;
    }
    return anyAlive;
}

bool TacticalSquad::areChargeMembersComplete() const {
    bool anyAlive = false;
    for (TacticalNpc* tnpc : memberCache_) {
        if (!tnpc || tnpc->hp() <= 0) continue;
        anyAlive = true;
        TacticalNpcState st = tnpc->getState();
        if (st == TacticalNpcState::AttackWindup ||
            st == TacticalNpcState::AttackRecover)
            continue;
        if (!tnpc->isChargeComplete()) return false;
    }
    return anyAlive;
}

void TacticalSquad::endActiveWedgeCharge(Room& room) {
    if (activeWedgeChargeId_ == 0) return;
    room.endWedgeCharge(activeWedgeChargeId_);
    activeWedgeChargeId_ = 0;
}

// ─── 슬롯 계산 ────────────────────────────────────────────────────────────────

std::vector<mu::Vec3> MU_CALLCONV TacticalSquad::calcEncircleSlots(
    mu::Vec3 targetPos, float sectorAngle, float sectorSpan,
    float radius, int count) const
{
    std::vector<mu::Vec3> slots;
    slots.reserve(static_cast<size_t>(count));
    float arc   = sectorSpan / static_cast<float>(count);
    float start = sectorAngle - sectorSpan * 0.5f + arc * 0.5f;
    for (int i = 0; i < count; ++i) {
        float a = start + arc * static_cast<float>(i);
        slots.push_back(targetPos + mu::Vec3(std::cosf(a), 0.f, std::sinf(a)) * radius);
    }
    return slots;
}

std::vector<mu::Vec3> MU_CALLCONV TacticalSquad::calcDenseSlots(
    mu::Vec3 center, mu::Vec3 forward, int count,
    float spacingScale, float columnScale, int fixedColumnCount) const
{
    std::vector<mu::Vec3> slots;
    slots.reserve(static_cast<size_t>(count));
    if (count <= 0) return slots;

    float spacing = memberSeparationRadius_ * spacingScale;
    if (spacing < 1.2f) spacing = 1.2f;

    mu::Vec3 right(-forward.z(), 0.f, forward.x());

    int cols = fixedColumnCount;
    if (cols <= 0) {
        if (columnScale < 1.f) columnScale = 1.f;
        cols = static_cast<int>(std::ceilf(
            std::sqrtf(static_cast<float>(count)) * columnScale));
    }
    if (cols < 1) cols = 1;
    if (cols > count) cols = count;
    int rows = (count + cols - 1) / cols;

    for (int i = 0; i < count; ++i) {
        int   col    = i % cols;
        int   row    = i / cols;
        float colOff = (static_cast<float>(col) - static_cast<float>(cols - 1) * 0.5f) * spacing;
        float rowOff = (static_cast<float>(row) - static_cast<float>(rows - 1) * 0.5f) * spacing;
        slots.push_back(center + right * colOff + forward * rowOff);
    }
    return slots;
}

std::vector<mu::Vec3> MU_CALLCONV TacticalSquad::calcWedgeSlots(
    mu::Vec3 apex, mu::Vec3 forward, int count, float spacingMult) const
{
    std::vector<mu::Vec3> slots;
    slots.reserve(static_cast<size_t>(count));
    if (count <= 0) return slots;

    mu::Vec3 fwd   = (forward.len2() > 0.01f) ? norm3(forward) : mu::Vec3(1.f, 0.f, 0.f);
    mu::Vec3 right(-fwd.z(), 0.f, fwd.x());
    if (spacingMult <= 0.f) spacingMult = 1.f;
    float spacing    = std::max(memberSeparationRadius_ * 0.75f, 1.5f) * spacingMult;
    float rowSpacing = std::max(memberSeparationRadius_ * 0.55f, 1.25f) * spacingMult;

    int placed = 0, row = 0;
    while (placed < count) {
        int   rowCount    = std::min(row + 1, count - placed);
        float backOffset  = static_cast<float>(row) * rowSpacing;
        for (int col = 0; col < rowCount; ++col) {
            float lateral = (static_cast<float>(col) -
                             static_cast<float>(rowCount - 1) * 0.5f) * spacing;
            slots.push_back(apex - fwd * backOffset + right * lateral);
            ++placed;
        }
        ++row;
    }
    return slots;
}

// ─── pushCommandsToMembers ───────────────────────────────────────────────────

void TacticalSquad::pushCommandsToMembers(Room& room) {
    if (memberIds_.empty()) return;

    const SquadOrder& ord   = currentOrder_;
    int               count = static_cast<int>(memberIds_.size());

    switch (ord.type) {
        case SquadOrderType::Idle: {
            TacticalCommand cmd;
            cmd.type = TacticalCommandType::Idle;
            for (TacticalNpc* tnpc : memberCache_)
                if (tnpc && tnpc->hp() > 0) tnpc->receiveCommand(cmd);
            break;
        }

        case SquadOrderType::Engage: {
            TacticalCommand cmd;
            cmd.type     = TacticalCommandType::EngageTarget;
            cmd.targetId = ord.targetId;
            for (TacticalNpc* tnpc : memberCache_)
                if (tnpc && tnpc->hp() > 0) tnpc->receiveCommand(cmd);
            break;
        }

        case SquadOrderType::DistributedEngage: {
            if (ord.targetIds.empty()) return;
            for (int i = 0; i < count; ++i) {
                TacticalNpc* tnpc = memberCache_[static_cast<size_t>(i)];
                if (!tnpc || tnpc->hp() <= 0) continue;
                TacticalCommand cmd;
                cmd.type     = TacticalCommandType::EngageTarget;
                cmd.targetId = ord.targetIds[static_cast<size_t>(i) % ord.targetIds.size()];
                tnpc->receiveCommand(cmd);
            }
            break;
        }

        case SquadOrderType::Encircle: {
            GameSession* targetSession = room.findLivingSessionByPlayerId(
                static_cast<int32>(ord.targetId));
            if (!targetSession) return;

            std::vector<mu::Vec3> slots = calcEncircleSlots(
                ord.tacticCenter, ord.sectorAngle, ord.sectorSpan,
                ord.approachRadius, count);

            std::vector<bool> slotUsed(static_cast<size_t>(count), false);
            for (int i = 0; i < count; ++i) {
                TacticalNpc* tnpc = memberCache_[static_cast<size_t>(i)];
                if (!tnpc || tnpc->hp() <= 0) continue;

                int   bestSlot   = -1;
                float bestDistSq = -1.f;
                for (int j = 0; j < count; ++j) {
                    if (slotUsed[static_cast<size_t>(j)]) continue;
                    float dSq = (tnpc->pos() - slots[static_cast<size_t>(j)]).len2();
                    if (bestDistSq < 0.f || dSq < bestDistSq) {
                        bestDistSq = dSq; bestSlot = j;
                    }
                }
                if (bestSlot < 0) continue;
                slotUsed[static_cast<size_t>(bestSlot)] = true;

                TacticalCommand cmd;
                cmd.type       = TacticalCommandType::HoldSlot;
                cmd.targetId   = ord.targetId;
                cmd.slotOffset = slots[static_cast<size_t>(bestSlot)];
                tnpc->receiveCommand(cmd);
            }
            break;
        }

        case SquadOrderType::WedgeCharge: {
            GameSession* targetSession = room.findLivingSessionByPlayerId(
                static_cast<int32>(ord.targetId));
            if (!targetSession) return;

            mu::Vec3 centroid     = calcCentroid();
            mu::Vec3 targetCenter = ord.tacticCenter;
            mu::Vec3 forward      = targetCenter - centroid;
            float    flen         = forward.len();
            forward = (flen > 0.01f) ? forward * (1.f / flen) : mu::Vec3(1.f, 0.f, 0.f);

            if (!wedgePrepared_) {
                mu::Vec3 prepareApex = centroid + forward * WEDGE_PREP_APEX_DISTANCE;
                int slotCount = count + (ord.reserveWedgeApex ? 1 : 0);
                std::vector<mu::Vec3> slots = calcWedgeSlots(
                    prepareApex, forward, slotCount, ord.wedgeSpacingMult);
                mu::Vec3 exitApex = targetCenter + forward * WEDGE_EXIT_DISTANCE;

                wedgeMemberIds_.clear();
                wedgeMemberCache_.clear();
                wedgePrepareSlots_.clear();
                wedgeExitSlots_.clear();

                std::vector<bool> slotUsed(slots.size(), false);
                if (ord.reserveWedgeApex && !slotUsed.empty())
                    slotUsed[0] = true;

                for (int i = 0; i < count; ++i) {
                    TacticalNpc* tnpc = memberCache_[static_cast<size_t>(i)];
                    if (!tnpc || tnpc->hp() <= 0) continue;

                    int   bestSlot   = -1;
                    float bestDistSq = -1.f;
                    for (int j = 0; j < static_cast<int>(slots.size()); ++j) {
                        if (slotUsed[static_cast<size_t>(j)]) continue;
                        float dSq = (tnpc->pos() - slots[static_cast<size_t>(j)]).len2();
                        if (bestDistSq < 0.f || dSq < bestDistSq) {
                            bestDistSq = dSq; bestSlot = j;
                        }
                    }
                    if (bestSlot < 0) continue;
                    slotUsed[static_cast<size_t>(bestSlot)] = true;

                    mu::Vec3 prepareSlot = slots[static_cast<size_t>(bestSlot)];
                    mu::Vec3 rel         = prepareSlot - prepareApex;
                    mu::Vec3 exitSlot    = exitApex + rel;

                    wedgeMemberIds_.push_back(tnpc->getId());
                    wedgeMemberCache_.push_back(tnpc);
                    wedgePrepareSlots_.push_back(prepareSlot);
                    wedgeExitSlots_.push_back(exitSlot);

                    TacticalCommand cmd;
                    cmd.type       = TacticalCommandType::HoldSlot;
                    cmd.targetId   = ord.targetId;
                    cmd.slotOffset = prepareSlot;
                    tnpc->receiveCommand(cmd);
                }
                return;
            }

            if (activeWedgeChargeId_ == 0)
                activeWedgeChargeId_ = room.beginWedgeCharge();

            for (size_t i = 0; i < wedgeMemberCache_.size(); ++i) {
                TacticalNpc* tnpc = wedgeMemberCache_[i];
                if (!tnpc || tnpc->hp() <= 0) continue;

                TacticalNpcState st = tnpc->getState();
                if (st == TacticalNpcState::AttackWindup  ||
                    st == TacticalNpcState::AttackRecover ||
                    st == TacticalNpcState::ChargeThrough)
                    continue;

                TacticalCommand cmd;
                cmd.type         = TacticalCommandType::ChargeThrough;
                cmd.targetId     = ord.targetId;
                cmd.targetIds    = ord.targetIds;
                cmd.chargeId     = activeWedgeChargeId_;
                cmd.slotOffset   = wedgeExitSlots_[i];
                cmd.chargeDir    = forward;
                cmd.chargeCenter = targetCenter;
                cmd.impactRadius = std::max(WEDGE_IMPACT_RADIUS, memberAttackRange_);
                cmd.impactDamage = WEDGE_CHARGE_DAMAGE *
                    ((ord.wedgeDamageMult > 0.f) ? ord.wedgeDamageMult : 1.f);
                cmd.passDistance = WEDGE_PASS_DISTANCE;
                cmd.speedMult    = (ord.chargeSpeedMult > 0.f)
                    ? ord.chargeSpeedMult : WEDGE_SPEED_MULT;
                tnpc->receiveCommand(cmd);
            }
            break;
        }

        case SquadOrderType::DenseHold: {
            GameSession* targetSession = room.findLivingSessionByPlayerId(
                static_cast<int32>(ord.targetId));
            if (!targetSession) return;

            mu::Vec3 targetPos = targetSession->player()->pos();
            mu::Vec3 centroid  = calcCentroid();
            mu::Vec3 fwd       = targetPos - centroid;
            float    flen      = fwd.len();
            if (flen > 0.01f) fwd = fwd * (1.f / flen);
            else               fwd = mu::Vec3(1.f, 0.f, 0.f);

            std::vector<mu::Vec3> slots = calcDenseSlots(centroid, fwd, count);
            for (int i = 0; i < count; ++i) {
                TacticalNpc* tnpc = memberCache_[static_cast<size_t>(i)];
                if (!tnpc || tnpc->hp() <= 0) continue;
                TacticalCommand cmd;
                cmd.type       = TacticalCommandType::HoldSlot;
                cmd.targetId   = ord.targetId;
                cmd.slotOffset = slots[static_cast<size_t>(i)];
                tnpc->receiveCommand(cmd);
            }
            break;
        }

        case SquadOrderType::GuardBoss: {
            mu::Vec3 guardDir(std::cosf(ord.sectorAngle), 0.f, std::sinf(ord.sectorAngle));
            mu::Vec3 squadCenter = ord.tacticCenter + guardDir * ord.approachRadius;
            mu::Vec3 faceDir     = ord.formationTargetPos - squadCenter;
            float    fl          = faceDir.len();
            faceDir = (fl > 0.01f) ? faceDir * (1.f / fl) : guardDir * -1.f;

            std::vector<mu::Vec3> slots = calcDenseSlots(squadCenter, faceDir, count,
                ord.slotSpacingScale, ord.slotColumnScale, ord.slotColumnCount);

            for (int i = 0; i < count; ++i) {
                TacticalNpc* tnpc = memberCache_[static_cast<size_t>(i)];
                if (!tnpc || tnpc->hp() <= 0) continue;
                TacticalCommand cmd;
                cmd.type       = TacticalCommandType::GuardSlot;
                cmd.targetId   = ord.targetId;
                cmd.slotOffset = slots[static_cast<size_t>(i)];
                tnpc->receiveCommand(cmd);
            }
            break;
        }

        case SquadOrderType::FormationHold:
        case SquadOrderType::FormationGuard: {
            GameSession* targetSession = room.findLivingSessionByPlayerId(
                static_cast<int32>(ord.targetId));
            if (!targetSession) return;

            mu::Vec3 center  = ord.tacticCenter;
            mu::Vec3 faceDir = ord.formationTargetPos - center;
            float    fl      = faceDir.len();
            faceDir = (fl > 0.01f) ? faceDir * (1.f / fl) : mu::Vec3(1.f, 0.f, 0.f);

            std::vector<mu::Vec3> slots = calcDenseSlots(center, faceDir, count,
                ord.slotSpacingScale, ord.slotColumnScale, ord.slotColumnCount);

            for (int i = 0; i < count; ++i) {
                TacticalNpc* tnpc = memberCache_[static_cast<size_t>(i)];
                if (!tnpc || tnpc->hp() <= 0) continue;
                TacticalCommand cmd;
                cmd.type = (ord.type == SquadOrderType::FormationGuard)
                    ? TacticalCommandType::GuardSlot
                    : TacticalCommandType::HoldSlot;
                cmd.targetId   = ord.targetId;
                cmd.slotOffset = slots[static_cast<size_t>(i)];
                cmd.speedMult  = ord.speedMult;
                tnpc->receiveCommand(cmd);
            }
            break;
        }

        case SquadOrderType::RingGuard: {
            std::vector<TacticalNpc*> liveMembers;
            liveMembers.reserve(memberCache_.size());
            for (TacticalNpc* tnpc : memberCache_)
                if (tnpc && tnpc->hp() > 0) liveMembers.push_back(tnpc);

            int liveCount = static_cast<int>(liveMembers.size());
            if (liveCount <= 0) return;

            std::vector<mu::Vec3> slots;
            slots.reserve(liveMembers.size());

            if (ord.slotSpacingScale > 1.f && ord.approachRadius > 0.01f &&
                ord.sectorSpan > 0.01f) {
                float minArcSpacing = ord.slotSpacingScale;
                int maxPerLane = static_cast<int>(std::floorf(
                    (ord.sectorSpan * ord.approachRadius) / minArcSpacing));
                maxPerLane = std::clamp(maxPerLane, 1, liveCount);

                int   laneCount   = (liveCount + maxPerLane - 1) / maxPerLane;
                float laneSpacing = std::max(ord.slotColumnScale, 0.1f);
                if (laneCount > 1) {
                    float innerFloor = std::max(memberSeparationRadius_ * 2.f,
                                                ord.approachRadius * 0.5f);
                    float usableDepth = std::max(ord.approachRadius - innerFloor, 0.f);
                    laneSpacing = std::min(laneSpacing,
                        usableDepth / static_cast<float>(laneCount - 1));
                }

                for (int lane = 0; lane < laneCount; ++lane) {
                    int   laneStart   = lane * maxPerLane;
                    int   laneMembers = std::min(maxPerLane, liveCount - laneStart);
                    if (laneMembers <= 0) continue;

                    float radius = ord.approachRadius - laneSpacing * static_cast<float>(lane);
                    float arc    = ord.sectorSpan / static_cast<float>(laneMembers);
                    float start  = ord.sectorAngle - ord.sectorSpan * 0.5f + arc * 0.5f;
                    for (int i = 0; i < laneMembers; ++i) {
                        float a = start + arc * static_cast<float>(i);
                        slots.push_back(ord.tacticCenter +
                            mu::Vec3(std::cosf(a), 0.f, std::sinf(a)) * radius);
                    }
                }
            } else {
                slots = calcEncircleSlots(
                    ord.tacticCenter, ord.sectorAngle, ord.sectorSpan,
                    ord.approachRadius, liveCount);
            }

            std::vector<bool> slotUsed(slots.size(), false);
            for (int i = 0; i < liveCount; ++i) {
                TacticalNpc* tnpc = liveMembers[static_cast<size_t>(i)];
                if (!tnpc || tnpc->hp() <= 0) continue;

                int   bestSlot   = -1;
                float bestDistSq = -1.f;
                for (int j = 0; j < static_cast<int>(slots.size()); ++j) {
                    if (slotUsed[static_cast<size_t>(j)]) continue;
                    float dSq = (tnpc->pos() - slots[static_cast<size_t>(j)]).len2();
                    if (bestDistSq < 0.f || dSq < bestDistSq) {
                        bestDistSq = dSq; bestSlot = j;
                    }
                }
                if (bestSlot < 0) continue;
                slotUsed[static_cast<size_t>(bestSlot)] = true;

                mu::Vec3 slot    = slots[static_cast<size_t>(bestSlot)];
                mu::Vec3 outward = slot - ord.tacticCenter;
                outward = (outward.len2() > 0.01f) ? norm3(outward) : mu::Vec3(1.f, 0.f, 0.f);

                TacticalCommand cmd;
                cmd.type         = TacticalCommandType::HoldSlot;
                cmd.targetId     = ord.targetId;
                cmd.slotOffset   = slot;
                cmd.useHoldFacing = true;
                cmd.holdFacing   = outward;
                tnpc->receiveCommand(cmd);
            }
            break;
        }

        case SquadOrderType::RetreatFormUp: {
            mu::Vec3 retreatDelta = ord.tacticCenter - ord.leaderPos;
            for (int i = 0; i < count; ++i) {
                TacticalNpc* tnpc = memberCache_[static_cast<size_t>(i)];
                if (!tnpc || tnpc->hp() <= 0) continue;
                TacticalCommand cmd;
                cmd.type       = TacticalCommandType::HoldSlot;
                cmd.targetId   = ord.targetId;
                cmd.slotOffset = tnpc->pos() + retreatDelta;
                cmd.speedMult  = ord.speedMult;
                tnpc->receiveCommand(cmd);
            }
            break;
        }

        case SquadOrderType::BoxAdvance: {
            GameSession* targetSession = room.findLivingSessionByPlayerId(
                static_cast<int32>(ord.targetId));
            if (!targetSession) return;

            mu::Vec3 boxCenter    = ord.tacticCenter;
            mu::Vec3 faceTargetPos = ord.formationTargetPos;
            mu::Vec3 toTarget     = faceTargetPos - boxCenter;
            float    d            = toTarget.len();
            mu::Vec3 forward      = (d > 0.01f) ? toTarget * (1.f / d) : mu::Vec3(1.f, 0.f, 0.f);
            mu::Vec3 right(-forward.z(), 0.f, forward.x());

            mu::Vec3 squadCenter = boxCenter
                + right   * ord.sectorPos.x()
                - forward * ord.sectorPos.z();

            mu::Vec3 faceDir = faceTargetPos - squadCenter;
            float    fl      = faceDir.len();
            if (fl > 0.01f) faceDir = faceDir * (1.f / fl); else faceDir = forward;

            std::vector<mu::Vec3> slots = calcDenseSlots(squadCenter, faceDir, count);
            for (int i = 0; i < count; ++i) {
                TacticalNpc* tnpc = memberCache_[static_cast<size_t>(i)];
                if (!tnpc || tnpc->hp() <= 0) continue;

                if (tnpc->getState() == TacticalNpcState::HoldSlot) {
                    float driftSq = (tnpc->getAssignedSlot() - slots[static_cast<size_t>(i)]).len2();
                    if (driftSq < 4.0f) continue;
                }

                TacticalCommand cmd;
                cmd.type       = TacticalCommandType::HoldSlot;
                cmd.targetId   = ord.targetId;
                cmd.slotOffset = slots[static_cast<size_t>(i)];
                tnpc->receiveCommand(cmd);
            }
            break;
        }
    }
}

// ─── pushConfusedToMembers ────────────────────────────────────────────────────

void TacticalSquad::pushConfusedToMembers(Room& room) {
    endActiveWedgeCharge(room);
    currentOrder_            = {};
    currentOrder_.type       = SquadOrderType::Idle;
    orderDirty_              = false;
    wedgePrepared_           = false;
    wedgeMemberIds_.clear();
    wedgeMemberCache_.clear();
    wedgePrepareSlots_.clear();
    wedgeExitSlots_.clear();
    leaderlessBrawlEnabled_  = true;
    leaderlessBrawlTimer_    = LEADERLESS_CONFUSED_DURATION;
    leaderlessRetargetTimer_ = 0.f;

    TacticalCommand cmd;
    cmd.type = TacticalCommandType::Confused;
    for (TacticalNpc* tnpc : memberCache_)
        if (tnpc && tnpc->hp() > 0) tnpc->receiveCommand(cmd);
}

#include "rspch.hpp"
#include "TacticalSquad.hpp"
#include "Room.hpp"
#include "GameSession.hpp"
#include <cassert>
#include <cmath>
#include <algorithm>

static mu::Vec3 norm3( mu::Vec3 v ) {
    float l = v.len();
    return l > 0.0001f ? v * (1.f / l) : mu::Vec3( 0.f, 0.f, 0.f );
}

// ─── 생성자 ───────────────────────────────────────────────────────────────────

TacticalSquad::TacticalSquad( int32 squadId, float memberAttackRange, float memberSeparationRadius )
    : squadId_( squadId )
    , memberAttackRange_( memberAttackRange )
    , memberSeparationRadius_( memberSeparationRadius )
{}

// ─── 멤버 관리 ────────────────────────────────────────────────────────────────

void TacticalSquad::addMember( TacticalNpc* npc ) {
    ASSERT_CRASH( npc != nullptr );
    ASSERT_CRASH( memberIds_.size() == memberCache_.size() );
    memberIds_.push_back( npc->getId() );
    memberCache_.push_back( npc );
}

void TacticalSquad::removeMember( uint32 npcId ) {
    for ( size_t i = 0; i < memberIds_.size(); ++i ) {
        if ( memberIds_[ i ] == npcId ) {
            memberIds_.erase( memberIds_.begin() + static_cast<ptrdiff_t>( i ) );
            memberCache_.erase( memberCache_.begin() + static_cast<ptrdiff_t>( i ) );
            return;
        }
    }
}

void TacticalSquad::receiveOrder( const SquadOrder& order ) {
    leaderlessBrawlEnabled_ = false;
    leaderlessBrawlTimer_ = 0s;
    leaderlessRetargetTimer_ = 0s;
    boxRefreshTimer_ = 0s;
    currentOrder_ = order;
    orderDirty_ = true;
    wedgePrepared_ = false;
    wedgeMemberIds_.clear();
    wedgeMemberCache_.clear();
    wedgePrepareSlots_.clear();
    wedgeExitSlots_.clear();
    activeWedgeChargeId_ = 0;
    wedgeChargeReleased_ = !order.waitForChargeRelease;
    wedgeChargeCommandIssued_ = false;
}

void MU_CALLCONV TacticalSquad::updateBoxLeaderPos( mu::Vec3 pos ) {
    if ( currentOrder_.type == SquadOrderType::BoxAdvance ) {
        currentOrder_.leaderPos = pos;
    }
}

// ─── update ───────────────────────────────────────────────────────────────────

void TacticalSquad::update( Seconds dt, Room& room ) {
    if ( memberIds_.empty() ) {
        return;
    }

    if ( leaderlessBrawlEnabled_ ) {
        updateLeaderlessBrawl( dt, room );
        return;
    }

    if ( orderDirty_ ) {
        pushCommandsToMembers( room );
        orderDirty_ = false;
        return;
    }
    else if ( currentOrder_.type == SquadOrderType::BoxAdvance ) {
        boxRefreshTimer_ -= dt;
        if ( boxRefreshTimer_ <= 0s ) {
            pushCommandsToMembers( room );
            boxRefreshTimer_ = 0.1s;
        }
        return;
    }

    // strictWedgeFormation이면 안착 래치로 판정한다. areMembersAtSlots()의 허용 오차
    // (sepRad×1.5 ≈ 4.5m)는 쐐기 슬롯 간격(가로 ~2.25m / 행 ~1.65m)보다 넓어 뭉친 덩어리도
    // "완성"으로 통과시켰고, 그 상태로 돌진해 V자가 화면에 나오지 않았다.
    if ( currentOrder_.type == SquadOrderType::WedgeCharge && !wedgePrepared_ ) {
        const bool prepared = currentOrder_.strictWedgeFormation
            ? areMembersSettledAtSlotsFraction()
            : areMembersAtSlots();
        if ( prepared ) {
            wedgePrepared_ = true;
        }
    }
    if ( currentOrder_.type == SquadOrderType::WedgeCharge && wedgePrepared_ &&
         wedgeChargeReleased_ && !wedgeChargeCommandIssued_ ) {
        startWedgeCharge( room );
    }
    if ( currentOrder_.type == SquadOrderType::WedgeCharge && wedgePrepared_ && activeWedgeChargeId_ != 0 && areChargeMembersComplete() ) {
        room.endWedgeCharge( activeWedgeChargeId_ );
        activeWedgeChargeId_ = 0;
    }
}

// ─── leaderless brawl ─────────────────────────────────────────────────────────

void TacticalSquad::updateLeaderlessBrawl( Seconds dt, Room& room ) {
    if ( leaderlessBrawlTimer_ > 0s ) {
        leaderlessBrawlTimer_ -= dt;

        if ( leaderlessBrawlTimer_ > 0s ) {
            return;
        }
        leaderlessRetargetTimer_ = 0s;
    }

    leaderlessRetargetTimer_ -= dt;

    if ( leaderlessRetargetTimer_ > 0s ) {
        return;
    }
    leaderlessRetargetTimer_ = LEADERLESS_RETARGET_INTERVAL;

    currentOrder_ = {};
    currentOrder_.targetId = selectNearestPlayerToSquad( room );
    currentOrder_.type = (currentOrder_.targetId != 0) ? SquadOrderType::Engage : SquadOrderType::Idle;
    orderDirty_ = false;
    wedgePrepared_ = false;
    wedgeMemberIds_.clear();
    wedgeMemberCache_.clear();
    wedgePrepareSlots_.clear();
    wedgeExitSlots_.clear();
    activeWedgeChargeId_ = 0;
    wedgeChargeReleased_ = true;
    wedgeChargeCommandIssued_ = false;

    pushCommandsToMembers( room );
}

uint32 TacticalSquad::selectNearestPlayerToSquad( Room& room ) const {
    mu::Vec3 center = calcCentroid();
    uint32 bestId = 0;
    float bestDistSq = -1.f;

    for ( GameSession* s : room.getLivingPlayers() ) {
        if ( !s ) {
            continue;
        }

        float distSq = (center - s->player()->pos()).len2();
        uint32 sid = static_cast<uint32>(s->id());

        if ( bestId == 0 || distSq < bestDistSq || (distSq == bestDistSq && sid < bestId) ) {
            bestId = sid;
            bestDistSq = distSq;
        }
    }

    return bestId;
};

// ─── removeDeadMembers ────────────────────────────────────────────────────────

void TacticalSquad::removeDeadMembers() {
    size_t w = 0;
    for ( size_t r = 0; r < memberIds_.size(); ++r ) {
        if ( memberCache_[ r ] && memberCache_[ r ]->hp() > 0 ) {
            memberIds_[ w ] = memberIds_[ r ];
            memberCache_[ w ] = memberCache_[ r ];
            ++w;
        }
    }
    memberIds_.resize( w );
    memberCache_.resize( w );
}

mu::Vec3 TacticalSquad::calcCentroid() const {
    mu::Vec3 sum{};
    int32 count{};

    for ( TacticalNpc* tnpc : memberCache_ ) {
        if ( tnpc && tnpc->hp() > 0 ) {
            sum += tnpc->pos();
            ++count;
        }
    }

    return (count > 0) ? (sum * (1.f / static_cast<float>(count))) : mu::Vec3{};
}

bool TacticalSquad::areMembersAtSlots() const {
    int32 aliveCount = 0;
    int32 atSlotCount = 0;

    for ( TacticalNpc* tnpc : memberCache_ ) {
        if ( !tnpc || tnpc->hp() <= 0 ) {
            continue;
        }

        ++aliveCount;
        if ( tnpc->isAtSlot() ) {
            ++atSlotCount;
        }
    }

    // 전원 대신 임계비율(SLOT_ARRIVE_FRACTION) 이상 도착 시 완성 인정 — 한두 명이 끼어도 단계 전환 진행
    return aliveCount > 0 &&
           static_cast<float>( atSlotCount ) >= static_cast<float>( aliveCount ) * SLOT_ARRIVE_FRACTION;
}

static mu::Vec3 findSafeFormationSlot( Room& room, mu::Vec3 desired,
                                       const TacticalNpc& npc,
                                       const std::vector<mu::Vec3>& occupied,
                                       float minSpacing ) {
    constexpr float SEARCH_STEP = 1.5f;
    constexpr int32 SEARCH_RINGS = 4;   // 최대 6m
    constexpr int32 SAMPLES_PER_RING = 12;

    auto isUsable = [&]( mu::Vec3 candidate ) {
        candidate = mu::Vec3( candidate.x(), room.groundHeightAtWorld( candidate.x(), candidate.z() ), candidate.z() );
        if ( !room.isTacticalFormationPositionOpen( candidate, npc ) ) {
            return false;
        }
        float minSpacingSq = minSpacing * minSpacing;
        for ( const mu::Vec3& used : occupied ) {
            float dx = candidate.x() - used.x();
            float dz = candidate.z() - used.z();
            if ( dx * dx + dz * dz < minSpacingSq ) {
                return false;
            }
        }
        return true;
    };

    mu::Vec3 groundDesired( desired.x(), room.groundHeightAtWorld( desired.x(), desired.z() ), desired.z() );
    if ( isUsable( groundDesired ) ) {
        return groundDesired;
    }

    constexpr float TWO_PI = 2.f * 3.14159265f;
    float angleOffset = static_cast<float>(npc.getId() % SAMPLES_PER_RING) * (TWO_PI / SAMPLES_PER_RING);
    for ( int32 ring = 1; ring <= SEARCH_RINGS; ++ring ) {
        float radius = SEARCH_STEP * static_cast<float>( ring );
        for ( int32 sample = 0; sample < SAMPLES_PER_RING; ++sample ) {
            float angle = angleOffset + TWO_PI * static_cast<float>( sample ) / static_cast<float>( SAMPLES_PER_RING );
            mu::Vec3 candidate = groundDesired + mu::Vec3( std::cosf( angle ) * radius, 0.f, std::sinf( angle ) * radius );
            candidate = mu::Vec3( candidate.x(), room.groundHeightAtWorld( candidate.x(), candidate.z() ), candidate.z() );
            if ( isUsable( candidate ) ) {
                return candidate;
            }
        }
    }

    // 1차 폴백: 이웃 간격 조건만 버리고 원래 슬롯을 쓴다. 슬롯 근처에서는 peer 분리를 일부러
    // 꺼서 밀집 패킹을 허용하는 설계이므로(TacticalNpc::updateHoldSlot), 이웃이 붐빈다는 이유로
    // 멤버를 대형에서 빼지 않는다. 예전엔 곧장 아래의 "현재 위치"로 떨어졌는데, 그러면 그 멤버는
    // 이동 거리 0짜리 슬롯을 받아 조용히 제자리에 남았다(도착 판정까지 통과해 발견도 안 됐다).
    if ( room.isTacticalFormationPositionOpen( groundDesired, npc ) ) {
        return groundDesired;
    }

    // 2차 폴백: 슬롯이 정적 장애물에 실제로 막힌 경우. 현재 위치는 이미 물리 정리된 위치이므로
    // 마지막 수단으로 쓴다.
    mu::Vec3 current = npc.pos();
    current = mu::Vec3( current.x(), room.groundHeightAtWorld( current.x(), current.z() ), current.z() );
    return room.isTacticalFormationPositionOpen( current, npc ) ? current : groundDesired;
}

bool TacticalSquad::areMembersSettledAtSlots() const {
    int32 aliveCount = 0;

    for ( TacticalNpc* tnpc : memberCache_ ) {
        if ( !tnpc || tnpc->hp() <= 0 ) {
            continue;
        }

        ++aliveCount;
        if ( !tnpc->isSettledAtSlot() ) {
            return false;
        }
    }

    return aliveCount > 0;
}

bool TacticalSquad::areMembersSettledAtSlotsFraction() const {
    int32 aliveCount = 0;
    int32 settledCount = 0;

    for ( TacticalNpc* tnpc : memberCache_ ) {
        if ( !tnpc || tnpc->hp() <= 0 ) {
            continue;
        }

        ++aliveCount;
        if ( tnpc->isSettledAtSlot() ) {
            ++settledCount;
        }
    }

    return aliveCount > 0 &&
           static_cast<float>( settledCount ) >= static_cast<float>( aliveCount ) * SLOT_ARRIVE_FRACTION;
}

bool TacticalSquad::areChargeMembersComplete() const {
    if ( !wedgeChargeCommandIssued_ ) {
        return false;
    }

    int32 aliveCount = 0;
    int32 completeCount = 0;

    for ( TacticalNpc* tnpc : memberCache_ ) {
        if ( !tnpc || tnpc->hp() <= 0 ) {
            continue;
        }

        ++aliveCount;
        TacticalNpcState st = tnpc->getState();

        // 공격 중인 멤버는 돌진 완료로 간주(애니메이션 진행 중)
        if ( st == TacticalNpcState::AttackWindup || st == TacticalNpcState::AttackRecover
            || tnpc->isChargeComplete() ) {
            ++completeCount;
        }
    }

    return aliveCount > 0 &&
           static_cast<float>( completeCount ) >= static_cast<float>( aliveCount ) * SLOT_ARRIVE_FRACTION;
}

void TacticalSquad::endActiveWedgeCharge( Room& room ) {
    if ( activeWedgeChargeId_ == 0 ) {
        return;
    }

    room.endWedgeCharge( activeWedgeChargeId_ );
    activeWedgeChargeId_ = 0;
}

void TacticalSquad::releaseWedgeCharge() {
    if ( currentOrder_.type != SquadOrderType::WedgeCharge || wedgeChargeReleased_ ) {
        return;
    }

    wedgeChargeReleased_ = true;
    if ( wedgePrepared_ ) {
        orderDirty_ = true; // 다음 tick에 pushCommandsToMembers로 돌진 명령 발행
    }
}

float TacticalSquad::estimateWedgeHalfWidth( float spacingMult, bool reserveApex ) const {
    int32 count = static_cast<int32>( memberIds_.size() ) + (reserveApex ? 1 : 0);
    if ( count <= 1 ) {
        return 0.f;
    }

    if ( spacingMult <= 0.f ) {
        spacingMult = 1.f;
    }
    float spacing = std::max( memberSeparationRadius_ * 0.75f, 1.5f ) * spacingMult;

    int32 placed = 0;
    int32 row = 0;
    int32 widestRow = 1;
    while ( placed < count ) {
        int32 rowCount = std::min( row + 1, count - placed );
        widestRow = std::max( widestRow, rowCount );
        placed += rowCount;
        ++row;
    }

    return static_cast<float>( widestRow - 1 ) * spacing * 0.5f;
}

// ─── 슬롯 계산 ────────────────────────────────────────────────────────────────

std::vector<mu::Vec3> MU_CALLCONV TacticalSquad::calcEncircleSlots( mu::Vec3 targetPos, float sectorAngle, float sectorSpan, float radius, int32 count ) const {
    std::vector<mu::Vec3> slots;
    slots.reserve( count );

    float arc = sectorSpan / static_cast<float>(count);
    float start = sectorAngle - sectorSpan * 0.5f + arc * 0.5f;

    for ( int32 i = 0; i < count; ++i ) {
        float a = start + arc * static_cast<float>( i );
        slots.push_back( targetPos + mu::Vec3( std::cosf( a ), 0.f, std::sinf( a ) ) * radius );
    }

    return slots;
}

bool TacticalSquad::forceStartWedgeCharge( Room& room ) {
    if ( currentOrder_.type != SquadOrderType::WedgeCharge ) {
        return false;
    }
    if ( wedgeChargeCommandIssued_ ) {
        return true;
    }

    if ( wedgeMemberCache_.empty() || wedgeMemberCache_.size() != wedgeExitSlots_.size() ) {
        wedgePrepared_ = false;
        pushCommandsToMembers( room );
    }
    if ( wedgeMemberCache_.empty() || wedgeMemberCache_.size() != wedgeExitSlots_.size() ) {
        return false;
    }

    wedgePrepared_ = true;
    wedgeChargeReleased_ = true;
    orderDirty_ = false;
    return startWedgeCharge( room );
}

std::vector<mu::Vec3> MU_CALLCONV TacticalSquad::calcRingSlots(
    mu::Vec3 center, float sectorAngle, float sectorSpan, float outerRadius,
    int32 count, float minSlotSpacing, float laneSpacing, int32 laneCapacityMultiple ) {
    std::vector<mu::Vec3> slots;
    if ( count <= 0 || sectorSpan <= 0.01f || outerRadius <= 0.01f || minSlotSpacing <= 0.01f ) {
        return slots;
    }

    slots.reserve( static_cast<size_t>(count) );
    laneSpacing = std::max( laneSpacing, minSlotSpacing );

    // 보스와 겹치지 않는 안쪽 절반까지만 inward lane을 만들고, 수용량이 부족하면 바깥쪽에
    // lane을 추가한다. 작은 반경에서 radius clamp로 같은 슬롯이 중복되는 것도 방지한다.
    float innerFloor = std::max( laneSpacing, outerRadius * 0.5f );
    int32 inwardLaneCount = std::max(
        static_cast<int32>(std::floorf((outerRadius - innerFloor) / laneSpacing)) + 1, 1 );

    int32 remaining = count;
    for ( int32 lane = 0; remaining > 0; ++lane ) {
        float radius = (lane < inwardLaneCount)
            ? outerRadius - laneSpacing * static_cast<float>( lane )
            : outerRadius + laneSpacing * static_cast<float>( lane - inwardLaneCount + 1 );

        // 원호 길이가 아니라 실제 두 슬롯의 직선 중심 거리(chord)가 minSlotSpacing
        // 이상이 되도록 이 반경에서 허용 가능한 최소 각도를 계산한다.
        float chordRatio = std::clamp( minSlotSpacing / (2.f * radius), 0.f, 1.f );
        float minSlotAngle = 2.f * std::asinf( chordRatio );
        int32 laneCapacity = static_cast<int32>(std::floorf(sectorSpan / minSlotAngle));
        laneCapacity = std::max( laneCapacity, 1 );
        if ( laneCapacityMultiple > 1 && laneCapacity >= laneCapacityMultiple ) {
            laneCapacity = (laneCapacity / laneCapacityMultiple) * laneCapacityMultiple;
        }

        int32 laneMembers = std::min( laneCapacity, remaining );
        float arc = sectorSpan / static_cast<float>( laneMembers );
        float start = sectorAngle - sectorSpan * 0.5f + arc * 0.5f;

        for ( int32 i = 0; i < laneMembers; ++i ) {
            float a = start + arc * static_cast<float>( i );
            slots.push_back( center + mu::Vec3( std::cosf( a ), 0.f, std::sinf( a ) ) * radius );
        }

        remaining -= laneMembers;
    }

    return slots;
}

std::vector<mu::Vec3> MU_CALLCONV TacticalSquad::calcDenseSlots( mu::Vec3 center, mu::Vec3 forward, int32 count, float spacingScale, float columnScale, int32 fixedColumnCount ) const {
    std::vector<mu::Vec3> slots;
    slots.reserve( count );

    if ( count <= 0 ) {
        return slots;
    }

    float spacing = denseSlotSpacing( spacingScale );

    mu::Vec3 right( -forward.z(), 0.f, forward.x() );

    int32 cols = fixedColumnCount;
    if ( cols <= 0 ) {
        if ( columnScale < 1.f ) {
            columnScale = 1.f;
        }

        cols = static_cast<int32>( std::ceilf( std::sqrtf( static_cast<float>( count ) ) * columnScale ) );
    }
    if ( cols < 1 ) {
        cols = 1;
    }
    if ( cols > count ) {
        cols = count;
    }

    int32 rows = (count + cols - 1) / cols;

    for ( int32 i = 0; i < count; ++i ) {
        int32 col = i % cols;
        int32 row = i / cols;
        float colOff = (static_cast<float>( col ) - static_cast<float>( cols - 1 ) * 0.5f) * spacing;
        float rowOff = (static_cast<float>( row ) - static_cast<float>( rows - 1 ) * 0.5f) * spacing;
        slots.push_back( center + right * colOff + forward * rowOff );
    }

    return slots;
}

std::vector<mu::Vec3> MU_CALLCONV TacticalSquad::calcWedgeSlots( mu::Vec3 apex, mu::Vec3 forward, int32 count, float spacingMult ) const {
    std::vector<mu::Vec3> slots;
    slots.reserve( count );

    if ( count <= 0 ) {
        return slots;
    }

    mu::Vec3 fwd = (forward.len2() > 0.01f) ? norm3( forward ) : mu::Vec3( 1.f, 0.f, 0.f );
    mu::Vec3 right( -fwd.z(), 0.f, fwd.x() );

    if ( spacingMult <= 0.f ) {
        spacingMult = 1.f;
    }

    float spacing = wedgeColSpacing( spacingMult );
    float rowSpacing = wedgeRowSpacing( spacingMult );

    int32 placed = 0;
    int32 row = 0;

    while ( placed < count ) {
        int32 rowCount = std::min( row + 1, count - placed );
        float backOffset = static_cast<float>( row ) * rowSpacing;

        for ( int32 col = 0; col < rowCount; ++col ) {
            float lateral = (static_cast<float>( col ) - static_cast<float>( rowCount - 1 ) * 0.5f) * spacing;
            slots.push_back( apex - fwd * backOffset + right * lateral );
            ++placed;
        }
        ++row;
    }

    return slots;
}

// ─── pushCommandsToMembers ───────────────────────────────────────────────────

void TacticalSquad::pushCommandsToMembers( Room& room ) {
    if ( memberIds_.empty() ) {
        return;
    }

    const SquadOrder& ord = currentOrder_;
    int32 count = static_cast<int32>(memberIds_.size());

    switch (ord.type) {
    case SquadOrderType::Idle: {
        auto cmd = TacticalCommand{
            .type = TacticalCommandType::Idle
        };

        for ( TacticalNpc* tnpc : memberCache_ ) {
            if ( tnpc && tnpc->hp() > 0 ) {
                tnpc->receiveCommand( cmd );
            }
        }
        break;
    }

    case SquadOrderType::Engage: {
        auto cmd = TacticalCommand{
            .type = TacticalCommandType::EngageTarget,
            .targetId = ord.targetId
		};

        for ( TacticalNpc* tnpc : memberCache_ ) {
            if ( tnpc && tnpc->hp() > 0 ) {
                tnpc->receiveCommand( cmd );
            }
        }
        break;
    }

    case SquadOrderType::DistributedEngage: {
        if ( ord.targetIds.empty() ) {
            return;
        }

        for ( int32 i = 0; i < count; ++i ) {
            TacticalNpc* tnpc = memberCache_[ i ];
            if ( !tnpc || tnpc->hp() <= 0 ) {
                continue;
            }

            auto cmd = TacticalCommand{
                .type = TacticalCommandType::EngageTarget,
                .targetId = ord.targetIds[ static_cast<size_t>( i ) % ord.targetIds.size() ]
            };
            tnpc->receiveCommand( cmd );
        }
        break;
    }

    case SquadOrderType::Encircle: {
        GameSession* targetSession = room.findLivingSessionByPlayerId( static_cast<int32>(ord.targetId) );
        if ( !targetSession ) {
            return;
        }

        std::vector<mu::Vec3> slots = calcEncircleSlots( ord.tacticCenter, ord.sectorAngle, ord.sectorSpan, ord.approachRadius, count );
        std::vector<bool> slotUsed( count, false );

        for ( int32 i = 0; i < count; ++i ) {
            TacticalNpc* tnpc = memberCache_[ i ];
            if ( !tnpc || tnpc->hp() <= 0 ) {
                continue;
            }

            int32 bestSlot = -1;
            float bestDistSq = -1.f;

            for ( int32 j = 0; j < count; ++j ) {
                if ( slotUsed[ j ] ) {
                    continue;
                }

                float dSq = ( tnpc->pos() - slots[ j ] ).len2();

                if ( bestDistSq < 0.f || dSq < bestDistSq ) {
                    bestDistSq = dSq; bestSlot = j;
                }
            }

            if ( bestSlot < 0 ) {
                continue;
            }

            slotUsed[ bestSlot ] = true;

            auto cmd = TacticalCommand{
				.type = TacticalCommandType::HoldSlot,
				.targetId = ord.targetId,
				.slotOffset = slots[ bestSlot ]
            };
            tnpc->receiveCommand( cmd );
        }
        break;
    }

    case SquadOrderType::WedgeCharge: {
        uint32 targetId = resolveWedgeTargetId( room );
        if ( targetId == 0 ) {
            return;
        }

        mu::Vec3 centroid = calcCentroid();
        mu::Vec3 targetCenter = ord.tacticCenter;
        mu::Vec3 forward = targetCenter - centroid;
        float flen = forward.len();
        forward = (flen > 0.01f) ? forward * (1.f / flen) : mu::Vec3( 1.f, 0.f, 0.f );

        if ( !wedgePrepared_ ) {
            mu::Vec3 prepareApex;
            if ( ord.hasWedgeApex ) {
                prepareApex = ord.wedgeApexPos;
                mu::Vec3 toTarget = targetCenter - prepareApex;
                if ( toTarget.len2() > 0.01f ) {
                    forward = norm3( toTarget );  // 회랑 입구→타겟 방향으로 전진축 재정렬
                }
            }
            else {
                prepareApex = centroid + forward * WEDGE_PREP_APEX_DISTANCE;
            }
            int32 slotCount = count + (ord.reserveWedgeApex ? 1 : 0);
            std::vector<mu::Vec3> slots = calcWedgeSlots( prepareApex, forward, slotCount, ord.wedgeSpacingMult );
            mu::Vec3 exitApex = targetCenter + forward * WEDGE_EXIT_DISTANCE;
            // 장애물 회피 보정이 요구할 최소 간격 = 이 쐐기의 최근접 이웃 간격. 이보다 크게 잡으면
            // 모든 슬롯이 서로를 탈락시켜 쐐기가 통째로 무너진다.
            const float wedgeMinSpacing = std::min( wedgeColSpacing( ord.wedgeSpacingMult ),
                                                    wedgeRowSpacing( ord.wedgeSpacingMult ) ) * 0.95f;

            wedgeMemberIds_.clear();
            wedgeMemberCache_.clear();
            wedgePrepareSlots_.clear();
            wedgeExitSlots_.clear();
            std::vector<mu::Vec3> occupiedPrepareSlots;
            std::vector<mu::Vec3> occupiedExitSlots;

            std::vector<bool> slotUsed( slots.size(), false );
            if ( ord.reserveWedgeApex && !slotUsed.empty() ) {
                slotUsed[ 0 ] = true;
            }

            for ( int32 i = 0; i < count; ++i ) {
                TacticalNpc* tnpc = memberCache_[ i ];
                if ( !tnpc || tnpc->hp() <= 0 ) {
                    continue;
                }

                int32 bestSlot = -1;
                float bestDistSq = -1.f;

                for ( int32 j = 0; j < static_cast<int32>( slots.size() ); ++j ) {
                    if ( slotUsed[ j ] ) {
                        continue;
                    }

                    float dSq = ( tnpc->pos() - slots[ j ] ).len2();

                    if ( bestDistSq < 0.f || dSq < bestDistSq ) {
                        bestDistSq = dSq;
                        bestSlot = j;
                    }
                }

                if ( bestSlot < 0 ) {
                    continue;
                }

                slotUsed[ bestSlot ] = true;

                mu::Vec3 prepareSlot = slots[ bestSlot ];
                if ( ord.avoidStaticObstacles ) {
                    prepareSlot = findSafeFormationSlot(
                        room, prepareSlot, *tnpc, occupiedPrepareSlots, wedgeMinSpacing );
                }
                mu::Vec3 rel = prepareSlot - prepareApex;
                mu::Vec3 exitSlot = exitApex + rel;
                if ( ord.avoidStaticObstacles ) {
                    exitSlot = findSafeFormationSlot(
                        room, exitSlot, *tnpc, occupiedExitSlots, wedgeMinSpacing );
                }
                occupiedPrepareSlots.push_back( prepareSlot );
                occupiedExitSlots.push_back( exitSlot );

                wedgeMemberIds_.push_back( tnpc->getId() );
                wedgeMemberCache_.push_back( tnpc );
                wedgePrepareSlots_.push_back( prepareSlot );
                wedgeExitSlots_.push_back( exitSlot );

                auto cmd = TacticalCommand{
					.type = TacticalCommandType::HoldSlot,
					.targetId = targetId,
					.slotOffset = prepareSlot
                };
                tnpc->receiveCommand( cmd );
            }
            return;
        }

        startWedgeCharge( room );
        break;
    }

    case SquadOrderType::DenseHold: {
        GameSession* targetSession = room.findLivingSessionByPlayerId( static_cast<int32>(ord.targetId) );
        if ( !targetSession ) {
            return;
        }

        mu::Vec3 targetPos = targetSession->player()->pos();
        mu::Vec3 centroid = calcCentroid();
        mu::Vec3 fwd = targetPos - centroid;
        float flen = fwd.len();

        if ( flen > 0.01f ) {
            fwd = fwd * (1.f / flen);
        }
        else {
            fwd = mu::Vec3( 1.f, 0.f, 0.f );
        }

        std::vector<mu::Vec3> slots = calcDenseSlots( centroid, fwd, count );

        for ( int32 i = 0; i < count; ++i ) {
            TacticalNpc* tnpc = memberCache_[ i ];
            if ( !tnpc || tnpc->hp() <= 0 ) {
                continue;
            }

            auto cmd = TacticalCommand{
				.type = TacticalCommandType::HoldSlot,
				.targetId = ord.targetId,
				.slotOffset = slots[ i ]
            };
            tnpc->receiveCommand( cmd );
        }
        break;
    }

    case SquadOrderType::GuardBoss: {
        mu::Vec3 guardDir( std::cosf( ord.sectorAngle ), 0.f, std::sinf( ord.sectorAngle ) );
        mu::Vec3 squadCenter = ord.tacticCenter + guardDir * ord.approachRadius;
        mu::Vec3 faceDir = ord.formationTargetPos - squadCenter;
        float fl = faceDir.len();
        faceDir = (fl > 0.01f) ? faceDir * (1.f / fl) : guardDir * -1.f;

        std::vector<mu::Vec3> slots = calcDenseSlots( squadCenter, faceDir, count, ord.slotSpacingScale, ord.slotColumnScale, ord.slotColumnCount );

        for ( int32 i = 0; i < count; ++i ) {
            TacticalNpc* tnpc = memberCache_[ i ];
            if ( !tnpc || tnpc->hp() <= 0 ) {
                continue;
            }

            auto cmd = TacticalCommand{
				.type = TacticalCommandType::GuardSlot,
				.targetId = ord.targetId,
				.slotOffset = slots[ i ]
            };
            tnpc->receiveCommand( cmd );
        }
        break;
    }

    case SquadOrderType::FormationHold:
    case SquadOrderType::FormationGuard: {
        GameSession* targetSession = room.findLivingSessionByPlayerId( static_cast<int32>(ord.targetId) );
        if ( !targetSession ) {
            return;
        }

        mu::Vec3 center = ord.tacticCenter;
        mu::Vec3 faceDir = ord.formationTargetPos - center;
        float fl = faceDir.len();
        faceDir = (fl > 0.01f) ? faceDir * (1.f / fl) : mu::Vec3( 1.f, 0.f, 0.f );

        std::vector<mu::Vec3> slots = calcDenseSlots( center, faceDir, count, ord.slotSpacingScale, ord.slotColumnScale, ord.slotColumnCount );
        std::vector<mu::Vec3> occupiedSlots;
        // 회피 보정의 최소 간격은 이 대형의 격자 간격과 같아야 한다(더 크면 전원이 탈락한다).
        const float slotMinSpacing = denseSlotSpacing( ord.slotSpacingScale ) * 0.95f;

        // FormationHold는 슬롯 유지에 타깃이 필요 없다. holdFacing을 주지 않으면 targetId 플레이어가
        // 죽는 순간 updateHoldSlot이 슬롯을 포기하고 부대 전원이 그 자리에서 Idle이 된다.
        // (FormationGuard는 Goblin 회랑 차단선이 쓰므로 기존 거동을 유지한다.)
        const bool holdFormationFacing = ( ord.type == SquadOrderType::FormationHold );

        for ( int32 i = 0; i < count; ++i ) {
            TacticalNpc* tnpc = memberCache_[ i ];
            if ( !tnpc || tnpc->hp() <= 0 ) {
                continue;
            }

            mu::Vec3 slot = slots[ i ];
            if ( ord.avoidStaticObstacles ) {
                slot = findSafeFormationSlot( room, slot, *tnpc, occupiedSlots, slotMinSpacing );
            }
            occupiedSlots.push_back( slot );

            auto cmd = TacticalCommand{
				.type = (ord.type == SquadOrderType::FormationGuard) ? TacticalCommandType::GuardSlot : TacticalCommandType::HoldSlot,
				.targetId = ord.targetId,
				.slotOffset = slot,
				.speedMult = ord.speedMult,
				.useHoldFacing = holdFormationFacing,
				.holdFacing = faceDir
            };
            tnpc->receiveCommand( cmd );
        }
        break;
    };

    case SquadOrderType::RingGuard: {
        std::vector<TacticalNpc*> liveMembers;
        liveMembers.reserve( memberCache_.size() );

        for ( TacticalNpc* tnpc : memberCache_ ) {
            if ( tnpc && tnpc->hp() > 0 ) {
                liveMembers.push_back( tnpc );
            }
        }

        int32 liveCount = static_cast<int32>(liveMembers.size());
        if ( liveCount <= 0 ) {
            return;
        }

        std::vector<mu::Vec3> slots;
        if ( static_cast<int32>(ord.explicitSlots.size()) == liveCount ) {
            slots = ord.explicitSlots;
        }
        else if ( ord.slotSpacingScale > 1.f && ord.approachRadius > 0.01f && ord.sectorSpan > 0.01f ) {
            slots = calcRingSlots( ord.tacticCenter, ord.sectorAngle, ord.sectorSpan,
                ord.approachRadius, liveCount, ord.slotSpacingScale, ord.slotColumnScale );
        }
        else {
            slots = calcEncircleSlots( ord.tacticCenter, ord.sectorAngle, ord.sectorSpan, ord.approachRadius, liveCount );
        }

        std::vector<bool> slotUsed( slots.size(), false );

        for ( int32 i = 0; i < liveCount; ++i ) {
            TacticalNpc* tnpc = liveMembers[ i ];
            if ( !tnpc || tnpc->hp() <= 0 ) {
                continue;
            }

            int32 bestSlot = -1;
            float bestDistSq = -1.f;

            for ( int32 j = 0; j < static_cast<int32>( slots.size() ); ++j ) {
                if ( slotUsed[ j ] ) {
                    continue;
                }

                float dSq = (tnpc->pos() - slots[ j ]).len2();

                if ( bestDistSq < 0.f || dSq < bestDistSq ) {
                    bestDistSq = dSq;
                    bestSlot = j;
                }
            }

            if ( bestSlot < 0 ) {
                continue;
            }

            slotUsed[ bestSlot ] = true;

            mu::Vec3 slot = slots[ bestSlot ];
            mu::Vec3 outward = slot - ord.tacticCenter;
            outward = (outward.len2() > 0.01f) ? norm3( outward ) : mu::Vec3( 1.f, 0.f, 0.f );

            auto cmd = TacticalCommand{
                .type = TacticalCommandType::HoldSlot,
				.targetId = ord.targetId,
				.slotOffset = slot,
				.speedMult = ord.speedMult,        // 방패벽 형성 가속
				.useHoldFacing = true,
				.holdFacing = outward
            };
            tnpc->receiveCommand( cmd );
        }
        break;
    }

    case SquadOrderType::RetreatFormUp: {
        mu::Vec3 retreatDelta = ord.tacticCenter - ord.leaderPos;

        for ( int32 i = 0; i < count; ++i ) {
            TacticalNpc* tnpc = memberCache_[ i ];
            if ( !tnpc || tnpc->hp() <= 0 ) {
                continue;
            }

            auto cmd = TacticalCommand{
				.type = TacticalCommandType::HoldSlot,
				.targetId = ord.targetId,
				.slotOffset = tnpc->pos() + retreatDelta,
				.speedMult = ord.speedMult
            };
            tnpc->receiveCommand( cmd );
        }
        break;
    }

    case SquadOrderType::BoxAdvance: {
        GameSession* targetSession = room.findLivingSessionByPlayerId( static_cast<int32>(ord.targetId) );
        if ( !targetSession ) {
            return;
        }

        mu::Vec3 boxCenter = ord.tacticCenter;
        mu::Vec3 faceTargetPos = ord.formationTargetPos;
        mu::Vec3 toTarget = faceTargetPos - boxCenter;
        float d = toTarget.len();
        mu::Vec3 forward = (d > 0.01f) ? toTarget * (1.f / d) : mu::Vec3( 1.f, 0.f, 0.f );
        mu::Vec3 right( -forward.z(), 0.f, forward.x() );

        mu::Vec3 squadCenter = boxCenter + right * ord.sectorPos.x() - forward * ord.sectorPos.z();

        mu::Vec3 faceDir = faceTargetPos - squadCenter;
        float fl = faceDir.len();

        if ( fl > 0.01f ) {
            faceDir = faceDir * (1.f / fl);
        }
        else {
            faceDir = forward;
        }

        std::vector<mu::Vec3> slots = calcDenseSlots( squadCenter, faceDir, count );

        for ( int32 i = 0; i < count; ++i ) {
            TacticalNpc* tnpc = memberCache_[ i ];
            if ( !tnpc || tnpc->hp() <= 0 ) {
                continue;
            }

            if ( tnpc->getState() == TacticalNpcState::HoldSlot ) {
                float driftSq = (tnpc->getAssignedSlot() - slots[ i ]).len2();

                if ( driftSq < 4.0f ) {
                    continue;
                }
            }

            auto cmd = TacticalCommand{
				.type = TacticalCommandType::HoldSlot,
				.targetId = ord.targetId,
				.slotOffset = slots[ i ],
            };
            tnpc->receiveCommand( cmd );
        }
        break;
    }
    }
}

uint32 TacticalSquad::resolveWedgeTargetId( Room& room ) const {
    if ( currentOrder_.targetId != 0 &&
         room.findLivingSessionByPlayerId( static_cast<int32>( currentOrder_.targetId ) ) ) {
        return currentOrder_.targetId;
    }

    for ( uint32 targetId : currentOrder_.targetIds ) {
        if ( targetId != 0 && room.findLivingSessionByPlayerId( static_cast<int32>( targetId ) ) ) {
            return targetId;
        }
    }

    return selectNearestPlayerToSquad( room );
}

bool TacticalSquad::startWedgeCharge( Room& room ) {
    if ( currentOrder_.type != SquadOrderType::WedgeCharge || !wedgePrepared_ ||
         !wedgeChargeReleased_ || wedgeChargeCommandIssued_ || wedgeMemberCache_.empty() ||
         wedgeMemberCache_.size() != wedgeExitSlots_.size() ) {
        return wedgeChargeCommandIssued_;
    }

    uint32 targetId = resolveWedgeTargetId( room );
    if ( targetId == 0 ) {
        return false;
    }

    bool hasLivingMember = false;
    for ( TacticalNpc* tnpc : wedgeMemberCache_ ) {
        if ( tnpc && tnpc->hp() > 0 ) {
            hasLivingMember = true;
            break;
        }
    }
    if ( !hasLivingMember ) {
        return false;
    }

    if ( activeWedgeChargeId_ == 0 ) {
        activeWedgeChargeId_ = room.beginWedgeCharge();
    }

    const SquadOrder& ord = currentOrder_;
    mu::Vec3 targetCenter = ord.tacticCenter;
    mu::Vec3 forward = targetCenter - calcCentroid();
    float flen = forward.len();
    forward = (flen > 0.01f) ? forward * (1.f / flen) : mu::Vec3( 1.f, 0.f, 0.f );

    int32 issuedCount = 0;
    for ( int32 i = 0; i < static_cast<int32>( wedgeMemberCache_.size() ); ++i ) {
        TacticalNpc* tnpc = wedgeMemberCache_[ i ];
        if ( !tnpc || tnpc->hp() <= 0 ) {
            continue;
        }

        auto cmd = TacticalCommand{
            .type = TacticalCommandType::ChargeThrough,
            .targetId = targetId,
            .slotOffset = wedgeExitSlots_[ i ],
            .speedMult = (ord.chargeSpeedMult > 0.f) ? ord.chargeSpeedMult : WEDGE_SPEED_MULT,
            .targetIds = ord.targetIds,
            .chargeId = activeWedgeChargeId_,
            .chargeDir = forward,
            .chargeCenter = targetCenter,
            .impactRadius = std::max( WEDGE_IMPACT_RADIUS, memberAttackRange_ ),
            .impactDamage = WEDGE_CHARGE_DAMAGE * ((ord.wedgeDamageMult > 0.f) ? ord.wedgeDamageMult : 1.f),
            .passDistance = WEDGE_PASS_DISTANCE,
            .chargeAcceleration = ord.chargeAcceleration
        };
        tnpc->receiveCommand( cmd );
        ++issuedCount;
    }

    wedgeChargeCommandIssued_ = issuedCount > 0;
    if ( !wedgeChargeCommandIssued_ ) {
        room.endWedgeCharge( activeWedgeChargeId_ );
        activeWedgeChargeId_ = 0;
    }
    return wedgeChargeCommandIssued_;
}

// ─── pushConfusedToMembers ────────────────────────────────────────────────────

void TacticalSquad::pushConfusedToMembers( Room& room ) {
    endActiveWedgeCharge( room );
    currentOrder_            = {};
    currentOrder_.type       = SquadOrderType::Idle;
    orderDirty_              = false;
    wedgePrepared_           = false;
    wedgeMemberIds_.clear();
    wedgeMemberCache_.clear();
    wedgePrepareSlots_.clear();
    wedgeExitSlots_.clear();
    wedgeChargeReleased_     = true;
    wedgeChargeCommandIssued_ = false;
    leaderlessBrawlEnabled_  = true;
    leaderlessBrawlTimer_    = LEADERLESS_CONFUSED_DURATION;
    leaderlessRetargetTimer_ = 0s;

    auto cmd = TacticalCommand{
		.type = TacticalCommandType::Confused
    };

    for ( TacticalNpc* tnpc : memberCache_ ) {
        if ( tnpc && tnpc->hp() > 0 ) {
            tnpc->receiveCommand( cmd );
        }
    }
}

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

    if ( currentOrder_.type == SquadOrderType::WedgeCharge && !wedgePrepared_ && areMembersAtSlots() ) {
        wedgePrepared_ = true;
        if ( wedgeChargeReleased_ ) {
            pushCommandsToMembers( room );
        }
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
    wedgePrepareSlots_.clear();
    wedgeExitSlots_.clear();
    activeWedgeChargeId_ = 0;
    wedgeChargeReleased_ = true;

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

bool TacticalSquad::areChargeMembersComplete() const {
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

std::vector<mu::Vec3> MU_CALLCONV TacticalSquad::calcDenseSlots( mu::Vec3 center, mu::Vec3 forward, int32 count, float spacingScale, float columnScale, int32 fixedColumnCount ) const {
    std::vector<mu::Vec3> slots;
    slots.reserve( count );

    if ( count <= 0 ) {
        return slots;
    }

    float spacing = memberSeparationRadius_ * spacingScale;
    if ( spacing < 1.2f ) {
        spacing = 1.2f;
    }

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

    float spacing = std::max( memberSeparationRadius_ * 0.75f, 1.5f ) * spacingMult;
    float rowSpacing = std::max( memberSeparationRadius_ * 0.55f, 1.25f ) * spacingMult;

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
        GameSession* targetSession = room.findLivingSessionByPlayerId( static_cast<int32>(ord.targetId) );
        if ( !targetSession ) {
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

            wedgeMemberIds_.clear();
            wedgeMemberCache_.clear();
            wedgePrepareSlots_.clear();
            wedgeExitSlots_.clear();

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
                mu::Vec3 rel = prepareSlot - prepareApex;
                mu::Vec3 exitSlot = exitApex + rel;

                wedgeMemberIds_.push_back( tnpc->getId() );
                wedgeMemberCache_.push_back( tnpc );
                wedgePrepareSlots_.push_back( prepareSlot );
                wedgeExitSlots_.push_back( exitSlot );

                auto cmd = TacticalCommand{
					.type = TacticalCommandType::HoldSlot,
					.targetId = ord.targetId,
					.slotOffset = prepareSlot
                };
                tnpc->receiveCommand( cmd );
            }
            return;
        }

        if ( activeWedgeChargeId_ == 0 ) {
            activeWedgeChargeId_ = room.beginWedgeCharge();
        }

        for ( int32 i = 0; i < static_cast<int32>( wedgeMemberCache_.size() ); ++i ) {
            TacticalNpc* tnpc = wedgeMemberCache_[ i ];
            if ( !tnpc || tnpc->hp() <= 0 ) {
                continue;
            }

            TacticalNpcState st = tnpc->getState();
            if ( st == TacticalNpcState::AttackWindup ||
                st == TacticalNpcState::AttackRecover ||
                st == TacticalNpcState::ChargeThrough 
            ) {
                continue;
            }

            auto cmd = TacticalCommand{
                .type = TacticalCommandType::ChargeThrough,
                .targetId = ord.targetId,
                .slotOffset = wedgeExitSlots_[ i ],
                .speedMult = (ord.chargeSpeedMult > 0.f) ? ord.chargeSpeedMult : WEDGE_SPEED_MULT,
                .targetIds = ord.targetIds,
                .chargeId = activeWedgeChargeId_,
                .chargeDir = forward,
                .chargeCenter = targetCenter,
                .impactRadius = std::max( WEDGE_IMPACT_RADIUS, memberAttackRange_ ),
                .impactDamage = WEDGE_CHARGE_DAMAGE * ((ord.wedgeDamageMult > 0.f) ? ord.wedgeDamageMult : 1.f) ,
                .passDistance = WEDGE_PASS_DISTANCE
            };
            tnpc->receiveCommand( cmd );
        }
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

        for ( int32 i = 0; i < count; ++i ) {
            TacticalNpc* tnpc = memberCache_[ i ];
            if ( !tnpc || tnpc->hp() <= 0 ) {
                continue;
            }

            auto cmd = TacticalCommand{
				.type = (ord.type == SquadOrderType::FormationGuard) ? TacticalCommandType::GuardSlot : TacticalCommandType::HoldSlot,
				.targetId = ord.targetId,
				.slotOffset = slots[ i ],
				.speedMult = ord.speedMult
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
        slots.reserve( liveMembers.size() );

        if ( ord.slotSpacingScale > 1.f && ord.approachRadius > 0.01f && ord.sectorSpan > 0.01f ) {
            float minArcSpacing = ord.slotSpacingScale;
            int32 maxPerLane = static_cast<int32>(std::floorf((ord.sectorSpan * ord.approachRadius) / minArcSpacing ));
            maxPerLane = std::clamp( maxPerLane, 1, liveCount );

            int32 laneCount = (liveCount + maxPerLane - 1) / maxPerLane;
            float laneSpacing = std::max( ord.slotColumnScale, 0.1f );

            if ( laneCount > 1 ) {
                float innerFloor = std::max( memberSeparationRadius_ * 2.f, ord.approachRadius * 0.5f );
                float usableDepth = std::max( ord.approachRadius - innerFloor, 0.f );
                laneSpacing = std::min( laneSpacing, usableDepth / static_cast<float>(laneCount - 1) );
            }

            for ( int32 lane = 0; lane < laneCount; ++lane ) {
                int32 laneStart = lane * maxPerLane;
                int32 laneMembers = std::min( maxPerLane, liveCount - laneStart );

                if ( laneMembers <= 0 ) {
                    continue;
                }

                float radius = ord.approachRadius - laneSpacing * static_cast<float>( lane );
                float arc = ord.sectorSpan / static_cast<float>( laneMembers );
                float start = ord.sectorAngle - ord.sectorSpan * 0.5f + arc * 0.5f;

                for ( int32 i = 0; i < laneMembers; ++i ) {
                    float a = start + arc * static_cast<float>( i );
                    slots.push_back( ord.tacticCenter + mu::Vec3( std::cosf( a ), 0.f, std::sinf( a ) ) * radius );
                }
            }
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

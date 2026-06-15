#include "rspch.hpp"
#include "TacticalNpc.hpp"
#include "Room.hpp"
#include "GameSession.hpp"
#include <cmath>
#include <algorithm>

/*-----------------------------
      파일 범위 헬퍼 함수
-----------------------------*/

// 0~1 사이의 균등 분포 해시값 반환. 입력이 같으면 항상 같은 값, 입력이 다르면 거의 항상 다른 값.
static float hash01( uint32 v ) {
    v ^= v >> 16;
    v *= 0x7feb352du;
    v ^= v >> 15;
    v *= 0x846ca68bu;
    v ^= v >> 16;
    return static_cast<float>(v & 0x00ffffffu) / static_cast<float>(0x01000000u);
}

// 정규화. 입력이 너무 작으면 영벡터 반환
static mu::Vec3 norm3( mu::Vec3 v ) {
    float l = v.len();
    return l > 0.0001f ? v * (1.f / l) : mu::Vec3( 0.f, 0.f, 0.f );
}

// 수평(XZ) 거리. NPC는 중력으로 지형 높이에 붙으므로 슬롯 도착 판정은 Y를 무시해야 한다
// (슬롯 Y는 평평한 명목값이라 기복 지형에서 3D 거리가 부풀려져 도착 판정이 깨짐).
static float lenXZ( mu::Vec3 v ) {
    return mu::Vec3( v.x(), 0.f, v.z() ).len();
}

// XZ 방향벡터로 Y축 회전 설정
static void setFacingDir( Object& obj, mu::Vec3 dir ) {
    if ( dir.len2() > 0.01f ) {
        obj.setOrient( mu::NQuat( mu::Radian(), mu::Radian(), mu::Radian( std::atan2( dir.x(), dir.z() ) ) ) );
    }
}

/*---------------------
      TacticalNpc
---------------------*/

TacticalNpc::TacticalNpc( Object&& base, const TacticalNpcConfig& cfg )
    : Object( std::move( base ) )
{
    spawnPos_ = pos();
    applyConfig( cfg );
    nearbyCache_.reserve( 8 );
}

void TacticalNpc::applyConfig( const TacticalNpcConfig& cfg ) {
    maxHp_ = cfg.maxHp;
    moveSpeed_ = cfg.moveSpeed;
    attackRange_ = cfg.attackRange;
    attackDamage_ = cfg.attackDamage;
    attackWindupTime_ = cfg.attackWindupTime;
    attackRecoverTime_ = cfg.attackRecoverTime;
    separationRadius_ = cfg.separationRadius;
    separationWeight_ = cfg.separationWeight;
    setHp( static_cast<int32>(cfg.maxHp) );
}

// ─── 접근자 ───────────────────────────────────────────────────────────────────

TacticalNpcState TacticalNpc::getDisplayState() const {
    if ( state_ == TacticalNpcState::PressureWait && pressureReentering_ ) {
        return TacticalNpcState::Chase;
    }
    return state_;
}

TacticalNpcConfig TacticalNpc::getConfig() const {
    auto cfg = TacticalNpcConfig{
        .maxHp = maxHp_,
        .moveSpeed = moveSpeed_,
        .attackRange = attackRange_,
        .attackDamage = attackDamage_,
        .attackWindupTime = attackWindupTime_,
        .attackRecoverTime = attackRecoverTime_,
        .separationRadius = separationRadius_,
        .separationWeight = separationWeight_
    };
    return cfg;
}

bool TacticalNpc::isAtSlot() const {
    switch ( state_ ) {
    case TacticalNpcState::HoldSlot:
        return lenXZ( pos() - assignedSlot_ ) < separationRadius_ * TACTICAL_SLOT_ARRIVE_MULT;

    case TacticalNpcState::ChargeThrough:
        return chargeComplete_;

    case TacticalNpcState::AttackWindup:
    case TacticalNpcState::AttackRecover:
    case TacticalNpcState::PressureWait:
        return true;

    default:
        return false;
    }
}

// ─── receiveCommand / reviveAt ────────────────────────────────────────────────

void TacticalNpc::receiveCommand( const TacticalCommand& cmd ) {
    pendingCmd_ = cmd;
}

void MU_CALLCONV TacticalNpc::reviveAt( mu::Vec3 p ) {
    setPos( p );
    setLinearVel( mu::Vec3{} );      // 순간이동 잔여 속도 제거
    setDesiredVel( mu::Vec3{} );     // motor 목표도 정지
    setHp( static_cast<int32>(maxHp_) );
    state_                    = TacticalNpcState::Idle;
    pendingCmd_               = {};
    targetId_                 = 0;
    assignedSlot_             = {};
    slotSettled_              = false;
    slotRefTargetPos_         = {};
    abandonDist_              = 15.f;
    chargeId_                 = 0;
    chargeDir_                = {};
    chargeCenter_             = {};
    impactRadius_             = 3.f;
    impactDamage_             = 0.f;
    passDistance_             = 6.f;
    chargeComplete_           = false;
    guardNearestPlayer_       = false;
    useHoldFacing_            = false;
    holdFacing_               = {};
    confusedAnchor_           = p;
    confusedTarget_           = p;
    confusedRetargetTimer_    = 0s;
    confusedWanderStep_       = 0;
    speedMult_                = 1.f;
    windupTimer_              = 0s;
    recoverTimer_             = 0s;
    pressureWaitTimer_        = 0s;
    resetPressureWaitTarget();
    pressureWaitAngleOffset_  = 0.f;
    pressureWaitRadiusOffset_ = 0.f;
    pressureWaitScatterSeed_  = 0;
    pressureReentering_       = false;
    reservedAttackTargetId_   = 0;
    reservedAttackStaleTimer_ = 0s;
}

// ─── transitionTo ─────────────────────────────────────────────────────────────

void TacticalNpc::transitionTo( TacticalNpcState next ) {
    if ( state_ == next ) {
        return;
    }
    if ( next == TacticalNpcState::AttackWindup ) {
        windupTimer_ = 0s;
    }
    if ( next == TacticalNpcState::AttackRecover ) {
        recoverTimer_ = 0s;
    }
    if ( next == TacticalNpcState::PressureWait ) {
        pressureWaitTimer_ = 0s;
        resetPressureWaitTarget();
        ++pressureWaitScatterSeed_;
        refreshPressureWaitScatterOffsets();
        pressureReentering_ = false;
    }

    state_ = next;
}

// ─── consumePendingCommand ────────────────────────────────────────────────────

void TacticalNpc::consumePendingCommand( Room& room ) {
    if ( pendingCmd_.type == TacticalCommandType::None ) {
        return;
    }

    switch ( pendingCmd_.type ) {
    case TacticalCommandType::EngageTarget:
        guardNearestPlayer_    = false;
        useHoldFacing_         = false;
        confusedRetargetTimer_ = 0s;
        speedMult_             = 1.f;

        if ( targetId_ == pendingCmd_.targetId &&
            (state_ == TacticalNpcState::Chase ||
                state_ == TacticalNpcState::PressureWait ||
                state_ == TacticalNpcState::AttackWindup ||
                state_ == TacticalNpcState::AttackRecover)
        ) {
            break;
        }

        if ( reservedAttackTargetId_ != 0 && reservedAttackTargetId_ != pendingCmd_.targetId ) {
            releaseAttackReservation( room );
        }

        pressureReentering_ = false;
        targetId_ = pendingCmd_.targetId;
        transitionTo( TacticalNpcState::Chase );
        break;

    case TacticalCommandType::FlankTarget:
        releaseAttackReservation( room );
        guardNearestPlayer_    = false;
        useHoldFacing_         = false;
        confusedRetargetTimer_ = 0s;
        pressureReentering_    = false;
        chargeComplete_        = false;
        targetId_         = pendingCmd_.targetId;
        assignedSlot_     = pendingCmd_.slotOffset;
        slotRefTargetPos_ = pendingCmd_.slotRefTargetPos;
        abandonDist_      = pendingCmd_.abandonDist;
        speedMult_        = pendingCmd_.speedMult;
        transitionTo( TacticalNpcState::Flank );
        break;

    case TacticalCommandType::ChargeThrough:
        releaseAttackReservation( room );
        guardNearestPlayer_    = false;
        useHoldFacing_         = false;
        confusedRetargetTimer_ = 0s;
        pressureReentering_    = false;
        targetId_       = pendingCmd_.targetId;
        assignedSlot_   = pendingCmd_.slotOffset;
        chargeId_       = pendingCmd_.chargeId;
        chargeDir_      = norm3( pendingCmd_.chargeDir );
        chargeCenter_   = pendingCmd_.chargeCenter;
        impactRadius_   = pendingCmd_.impactRadius;
        impactDamage_   = pendingCmd_.impactDamage;
        passDistance_   = pendingCmd_.passDistance;
        speedMult_      = pendingCmd_.speedMult;
        chargeComplete_ = false;
        transitionTo( TacticalNpcState::ChargeThrough );
        break;

    case TacticalCommandType::HoldSlot:
        releaseAttackReservation( room );
        guardNearestPlayer_    = false;
        useHoldFacing_         = pendingCmd_.useHoldFacing;
        holdFacing_            = pendingCmd_.holdFacing;
        confusedRetargetTimer_ = 0s;
        pressureReentering_    = false;
        speedMult_             = pendingCmd_.speedMult;
        chargeComplete_        = false;
        slotSettled_           = false;   // 새 슬롯 수령 → 안착 래치 해제(재접근)
        targetId_     = pendingCmd_.targetId;
        assignedSlot_ = pendingCmd_.slotOffset;
        transitionTo( TacticalNpcState::HoldSlot );
        break;

    case TacticalCommandType::GuardSlot:
        releaseAttackReservation( room );
        guardNearestPlayer_    = true;
        useHoldFacing_         = pendingCmd_.useHoldFacing;
        holdFacing_            = pendingCmd_.holdFacing;
        confusedRetargetTimer_ = 0s;
        pressureReentering_    = false;
        speedMult_             = pendingCmd_.speedMult;
        slotSettled_           = false;   // 새 슬롯 수령 → 안착 래치 해제(재접근)
        targetId_     = pendingCmd_.targetId;
        assignedSlot_ = pendingCmd_.slotOffset;
        transitionTo( TacticalNpcState::HoldSlot );
        break;

    case TacticalCommandType::Idle:
        releaseAttackReservation( room );
        guardNearestPlayer_    = false;
        useHoldFacing_         = false;
        chargeComplete_        = false;
        confusedRetargetTimer_ = 0s;
        pressureReentering_    = false;
        speedMult_             = 1.f;
        targetId_              = 0;
        transitionTo( TacticalNpcState::Idle );
        break;

    case TacticalCommandType::Confused:
        releaseAttackReservation( room );
        guardNearestPlayer_    = false;
        useHoldFacing_         = false;
        chargeComplete_        = false;
        pressureReentering_    = false;
        speedMult_             = 1.f;
        targetId_              = 0;
        assignedSlot_          = {};
        confusedAnchor_        = pos();
        confusedTarget_        = pos();
        confusedRetargetTimer_ = 0s;
        confusedWanderStep_    = 0;
        transitionTo( TacticalNpcState::Confused );
        break;

    default:
        break;
    }

    pendingCmd_.type = TacticalCommandType::None;
}

// ─── 메인 업데이트 분기 ──────────────────────────────────────────────────────

TacticalNpcUpdateResult TacticalNpc::update( Seconds dt, Room& room ) {
    frameHits_.clear();

    if ( hp() <= 0 ) {
        bool wasDead = ( state_ == TacticalNpcState::Dead );
        updateDead( room );
        TacticalNpcUpdateResult result;
        result.hits = std::move( frameHits_ );
        result.justDied = !wasDead;   // Dead로 막 전이된 첫 틱
        return result;
    }

    if ( pendingCmd_.type != TacticalCommandType::None ) {
        consumePendingCommand( room );
    }

    switch ( state_ ) {
    case TacticalNpcState::Idle:          updateIdle        ();           break;
    case TacticalNpcState::Chase:         updateChase       ( dt, room ); break;
    case TacticalNpcState::AttackWindup:  updateAttackWindup( dt, room ); break;
    case TacticalNpcState::AttackRecover: updateAttackRecover( dt, room ); break;
    case TacticalNpcState::Flank:         updateFlank       ( dt, room ); break;
    case TacticalNpcState::ChargeThrough: updateChargeThrough( room );    break;
    case TacticalNpcState::Confused:      updateConfused    ( dt, room ); break;
    case TacticalNpcState::HoldSlot:      updateHoldSlot    ( room );     break;
    case TacticalNpcState::PressureWait:  updatePressureWait( dt, room ); break;
    case TacticalNpcState::Dead:                                           break;
    }

    TacticalNpcUpdateResult result;
    result.hits = std::move( frameHits_ );
    return result;
}

// ─── Idle ─────────────────────────────────────────────────────────────────────

void TacticalNpc::updateIdle() {
    setDesiredVel( mu::Vec3{} );
}

// ─── 공격 슬롯 예약 헬퍼 ────────────────────────────────────────────────────

bool TacticalNpc::hasReservedAttackSlot() const {
    return reservedAttackTargetId_ != 0 && reservedAttackTargetId_ == targetId_;
}

bool MU_CALLCONV TacticalNpc::isEligibleForAttackReservation( uint32 targetId, mu::Vec3 targetPos ) const {
    float dist = (pos() - targetPos).len();
    if ( dist > TACTICAL_ATTACK_RESERVATION_MAX_DIST )
        return false;

    // 스테일로 막힌 타깃이 아니면 거리 조건만으로 자격 충족.
    if ( blockedAttackReservationTargetId_ != targetId )
        return true;

    // 막힌 타깃이면 끊긴 시점보다 PROGRESS_DIST만큼 더 접근해야 재자격(진동 차단 핵심 게이트).
    return dist <= blockedAttackReservationDist_ - TACTICAL_ATTACK_RESERVATION_PROGRESS_DIST;
}

void TacticalNpc::resetAttackReservationLease() {
    reservedAttackStaleTimer_ = 0s;
    reservedAttackProgressDist_ = 0.f;
    blockedAttackReservationTargetId_ = 0;
    blockedAttackReservationDist_ = 0.f;
}

void TacticalNpc::updateReservedAttackStaleTimer( Seconds dt, Room& room ) {
    if ( !hasReservedAttackSlot() ) {
        reservedAttackStaleTimer_ = 0s;
        reservedAttackProgressDist_ = 0.f;
        return;
    }

    GameSession* target = resolveTarget( room );
    if ( !target ) {
        reservedAttackStaleTimer_ = 0s;
        reservedAttackProgressDist_ = 0.f;
        return;
    }

    float dist = (pos() - target->player()->pos()).len();
    // 교전 중이거나 사거리 내면 리스 유지(진척 거리 기준점 갱신).
    if ( state_ == TacticalNpcState::AttackWindup || state_ == TacticalNpcState::AttackRecover || dist <= attackRange_ ) {
        reservedAttackStaleTimer_ = 0s;
        reservedAttackProgressDist_ = dist;
        return;
    }

    // 직전보다 PROGRESS_DIST 이상 접근했으면 진척으로 보고 타이머 리셋.
    if ( reservedAttackProgressDist_ <= 0.f || dist <= reservedAttackProgressDist_ - TACTICAL_ATTACK_RESERVATION_PROGRESS_DIST ) {
        reservedAttackStaleTimer_ = 0s;
        reservedAttackProgressDist_ = dist;
        return;
    }

    // 진척 없이 정체 → 스테일 타이머 누적.
    reservedAttackStaleTimer_ += dt;
}

bool TacticalNpc::canEnterAttackSlot( Room& room ) {
    if ( targetId_ == 0 ) {
        return false;
    }

    if ( hasReservedAttackSlot() ) {
        GameSession* target = resolveTarget( room );
        if ( !target ) {
            releaseAttackReservation( room );
            return false;
        }

        float dist = (pos() - target->player()->pos()).len();
        if ( dist > TACTICAL_ATTACK_RESERVATION_MAX_DIST ) {
            releaseAttackReservation( room );
            return false;
        }
        if ( reservedAttackStaleTimer_ >= TACTICAL_ATTACK_RESERVATION_STALE_TIME ) {
            releaseStaleAttackReservation( room, dist );
            return false;
        }
        return true;
    }

    GameSession* target = resolveTarget( room );
    if ( !target || !isEligibleForAttackReservation( targetId_, target->player()->pos() ) ) {
        pressureReentering_ = false;
        return false;
    }

    if ( !room.tryReserveTacticalAttackSlot( targetId_, getId() ) ) {
        reservedAttackTargetId_ = 0;
        reservedAttackStaleTimer_ = 0s;
        reservedAttackProgressDist_ = 0.f;
        pressureReentering_ = false;
        return false;
    }

    reservedAttackTargetId_ = targetId_;
    reservedAttackStaleTimer_ = 0s;
    reservedAttackProgressDist_ = (pos() - target->player()->pos()).len();
    blockedAttackReservationTargetId_ = 0;
    blockedAttackReservationDist_ = 0.f;
    return true;
}

void TacticalNpc::releaseAttackReservation( Room& room ) {
    if ( reservedAttackTargetId_ != 0 )
        room.releaseTacticalAttackSlot( reservedAttackTargetId_, getId() );
    reservedAttackTargetId_ = 0;
    resetAttackReservationLease();
    pressureReentering_ = false;
}

void TacticalNpc::releaseStaleAttackReservation( Room& room, float currentDist ) {
    uint32 targetId = reservedAttackTargetId_;
    if ( targetId != 0 )
        room.releaseTacticalAttackSlot( targetId, getId() );

    reservedAttackTargetId_ = 0;
    reservedAttackStaleTimer_ = 0s;
    reservedAttackProgressDist_ = 0.f;
    // 이 타깃은 PROGRESS_DIST만큼 더 접근하기 전까지 재예약 차단.
    blockedAttackReservationTargetId_ = targetId;
    blockedAttackReservationDist_ = currentDist;
    pressureReentering_ = false;
}

// ─── PressureWait 헬퍼 ────────────────────────────────────────────────────────

void TacticalNpc::resetPressureWaitTarget() {
    pressureWaitDesired_       = {};
    pressureWaitTargetAnchor_  = {};
    pressureWaitFacingAnchor_  = mu::Vec3( 1.f, 0.f, 0.f );
    pressureWaitRetargetTimer_ = 0s;
    pressureWaitDesiredValid_  = false;
}

void TacticalNpc::refreshPressureWaitScatterOffsets() {
    constexpr float PI          = 3.14159265f;
    constexpr float DEG_TO_RAD  = PI / 180.f;
    constexpr float FULL_CIRCLE = 360.0f;
    uint32_t base = getId() * 73856093u ^ static_cast<uint32>(squadId_ + 4096) * 19349663u ^ pressureWaitScatterSeed_ * 83492791u;
    float openArc = TACTICAL_PRESSURE_FRONT_GAP_DEGREES * 2.0f;
    float angleDeg = TACTICAL_PRESSURE_FRONT_GAP_DEGREES + hash01(base) * (FULL_CIRCLE - openArc);
    pressureWaitAngleOffset_  = angleDeg * DEG_TO_RAD;
    pressureWaitRadiusOffset_ = TACTICAL_PRESSURE_RADIUS_OFFSET_MIN + hash01(base ^ 0xa511e9b3u) * TACTICAL_PRESSURE_RADIUS_OFFSET_SPAN;
};

mu::Vec3 TacticalNpc::computePressureWaitDesired( mu::Vec3 targetPos, mu::Vec3 targetFacing ) const {
    float ringRadius = std::max( attackRange_ + TACTICAL_PRESSURE_EXTRA_RADIUS, 
        separationRadius_ * TACTICAL_PRESSURE_SEPARATION_MULT ) + pressureWaitRadiusOffset_;

    mu::Vec3 fwd = mu::Vec3( targetFacing.x(), 0.f, targetFacing.z() );
    if ( fwd.len2() <= 0.01f ) {
        fwd = mu::Vec3( (targetPos - pos()).x(), 0.f, (targetPos - pos()).z() );
    }
    if ( fwd.len2() <= 0.01f ) {
        fwd = mu::Vec3( 1.f, 0.f, 0.f );
    }
    fwd = norm3( fwd );

    float forwardAngle = std::atan2f( fwd.z(), fwd.x() );
    float angle = forwardAngle + pressureWaitAngleOffset_;
    mu::Vec3 radial( std::cosf( angle ), 0.f, std::sinf( angle ) );

    return targetPos + radial * ringRadius;
}

void TacticalNpc::moveTowardPressureWait( Seconds dt, Room& room, mu::Vec3 targetPos, mu::Vec3 targetFacing ) {
    mu::Vec3 tf = mu::Vec3( targetFacing.x(), 0.f, targetFacing.z() );
    if ( tf.len2() <= 0.01f ) {
        tf = mu::Vec3((targetPos - pos()).x(), 0.f, (targetPos - pos()).z());
    }
    if ( tf.len2() <= 0.01f ) {
        tf = mu::Vec3(1.f, 0.f, 0.f);
    }
    tf = norm3(tf);

    pressureWaitRetargetTimer_ += dt;

    bool targetMoved = (targetPos - pressureWaitTargetAnchor_).len2() > TACTICAL_PRESSURE_TARGET_MOVE_REFRESH_DIST * TACTICAL_PRESSURE_TARGET_MOVE_REFRESH_DIST;
    bool facingChanged = pressureWaitFacingAnchor_.len2() <= 0.01f || mu::dot( tf, pressureWaitFacingAnchor_ ) < TACTICAL_PRESSURE_FACING_REFRESH_DOT;

    if ( !pressureWaitDesiredValid_ ) {
        refreshPressureWaitScatterOffsets();
        pressureWaitDesired_        = computePressureWaitDesired( targetPos, tf );
        pressureWaitTargetAnchor_   = targetPos;
        pressureWaitFacingAnchor_   = tf;
        pressureWaitRetargetTimer_  = 0s;
        pressureWaitDesiredValid_   = true;
    }
    else if ( pressureWaitRetargetTimer_ >= TACTICAL_PRESSURE_RETARGET_INTERVAL && (targetMoved || facingChanged) ) {
        mu::Vec3 delta = targetPos - pressureWaitTargetAnchor_;
        delta = mu::Vec3( delta.x(), 0.f, delta.z() );
        pressureWaitDesired_ += delta;

        mu::Vec3 relative = pressureWaitDesired_ - targetPos;
        relative = mu::Vec3( relative.x(), 0.f, relative.z() );
        float relLen = relative.len();

        constexpr float PI = 3.14159265f;
        constexpr float DEG_TO_RAD = PI / 180.f;
        float frontGap = TACTICAL_PRESSURE_FRONT_GAP_DEGREES * DEG_TO_RAD;

        if ( relLen > 0.01f ) {
            mu::Vec3 dir = relative / relLen;
            float signedAngle = std::atan2f( tf.x() * dir.z() - tf.z() * dir.x(), mu::dot( tf, dir ) );

            if ( std::fabs( signedAngle ) < frontGap ) {
                float side = (std::fabs(signedAngle) > 0.001f)
                    ? ((signedAngle >= 0.f) ? 1.f : -1.f)
                    : ((std::sinf( pressureWaitAngleOffset_ ) >= 0.f) ? 1.f : -1.f);

                float fwdAngle = std::atan2f( tf.z(), tf.x() );
                float corrected = fwdAngle + side * frontGap;

                mu::Vec3 radial( std::cosf( corrected ), 0.f, std::sinf( corrected ) );
                pressureWaitDesired_ = targetPos + radial * relLen;
            }
        }

        pressureWaitTargetAnchor_  = targetPos;
        pressureWaitFacingAnchor_  = tf;
        pressureWaitRetargetTimer_ = 0s;
    }

    mu::Vec3 toDesired = pressureWaitDesired_ - pos();
    toDesired = mu::Vec3( toDesired.x(), 0.f, toDesired.z() );
    float distToDesired = toDesired.len();
    mu::Vec3 moveDir = (distToDesired > 0.1f) ? norm3( toDesired ) : mu::Vec3( 0.f, 0.f, 0.f );

    // 분리력 (접근 방향 수직 성분만)
    nearbyCache_.clear();
    float sepRad = std::max( separationRadius_, separationRadius_ * TACTICAL_PRESSURE_SEPARATION_RADIUS_MULT );
    room.findNearbyNpcPositions( pos(), sepRad, getId(), nearbyCache_ );
    mu::Vec3 sep = calcSeparationForce( nearbyCache_, sepRad );

    if ( sep.len2() > 0.01f && moveDir.len2() > 0.01f ) {
        mu::Vec3 sepPerp = sep - moveDir * mu::dot( sep, moveDir );
        float sepScale = std::min( 1.f, sepPerp.len() );
        float nearScale = std::max( TACTICAL_PRESSURE_NEAR_SEPARATION_MIN_SCALE, std::min( 1.f, distToDesired / TACTICAL_PRESSURE_SLOW_RADIUS ) );
        moveDir = norm3( moveDir + sepPerp * (separationWeight_ * TACTICAL_PRESSURE_SEPARATION_WEIGHT_MULT * sepScale * nearScale) );
    }

    // facing: 타겟 방향
    mu::Vec3 lookDir = targetPos - pos();
    setFacingDir( *this, lookDir.len() > 0.1f ? lookDir : moveDir );

    // 중첩 drift 계산
    mu::Vec3 overlapDrift( 0.f, 0.f, 0.f );
    nearbyCache_.clear();
    float overlapRad = std::max( TACTICAL_PRESSURE_OVERLAP_RADIUS_MIN, separationRadius_ * TACTICAL_PRESSURE_OVERLAP_RADIUS_MULT );
    room.findNearbyNpcPositions( pos(), overlapRad, getId(), nearbyCache_ );
    mu::Vec3 overlapSep = calcSeparationForce( nearbyCache_, overlapRad );

    if ( overlapSep.len2() > 0.0001f ) {
        mu::Vec3 radial = pos() - targetPos;
        radial = mu::Vec3( radial.x(), 0.f, radial.z() );

        if ( radial.len2() <= 0.01f ) {
            radial = mu::Vec3( (pressureWaitDesired_ - targetPos).x(), 0.f, (pressureWaitDesired_ - targetPos).z() );
        }
        if (radial.len2() > 0.01f) {
            radial              = norm3( radial );
            float radialDot     = mu::dot( overlapSep, radial );
            mu::Vec3 outward    = radial * std::max( 0.f, radialDot );
            mu::Vec3 tangent    = overlapSep - radial * radialDot;
            overlapDrift        = outward + tangent * 0.7f;
        } 
        else {
            overlapDrift = overlapSep;
        }
    }

    mu::Vec3 xzVel( 0.f, 0.f, 0.f );

    if (distToDesired <= TACTICAL_PRESSURE_STOP_RADIUS || moveDir.len2() <= 0.01f) {
        if ( overlapDrift.len2() > 0.0001f ) {
            xzVel = overlapDrift * (moveSpeed_ * TACTICAL_PRESSURE_OVERLAP_DRIFT_MULT);
        }

        setDesiredVel(mu::Vec3(xzVel.x(), 0.f, xzVel.z()));
        return;
    }

    float slowT = std::min( 1.f,
        (distToDesired - TACTICAL_PRESSURE_STOP_RADIUS) / (TACTICAL_PRESSURE_SLOW_RADIUS - TACTICAL_PRESSURE_STOP_RADIUS) );

    xzVel = moveDir * (moveSpeed_ * TACTICAL_PRESSURE_SPEED_MULT * slowT);

    if ( overlapDrift.len2() > 0.0001f ) {
        xzVel = xzVel + overlapDrift * (moveSpeed_ * TACTICAL_PRESSURE_OVERLAP_DRIFT_MULT);
    }

    setDesiredVel( mu::Vec3( xzVel.x(), 0.f, xzVel.z() ) );
}

// ─── Chase ────────────────────────────────────────────────────────────────────

void TacticalNpc::updateChase( Seconds dt, Room& room ) {
    GameSession* target = resolveTarget( room );
    if ( !target ) {
        releaseAttackReservation( room );
        targetId_ = 0;
        transitionTo( TacticalNpcState::Idle );
        setDesiredVel( mu::Vec3{} );
        return;
    }

    updateReservedAttackStaleTimer( dt, room );

    if ( !canEnterAttackSlot( room ) ) {
        transitionTo( TacticalNpcState::PressureWait );
        return;
    }

    mu::Vec3 targetPos = target->player()->pos();
    if ( (pos() - targetPos).len2() <= attackRange_ * attackRange_ ) {
        transitionTo( TacticalNpcState::AttackWindup );
        setDesiredVel( mu::Vec3{} );
        return;
    }

    mu::Vec3 moveDir = norm3( targetPos - pos() );
    applyPeerSeparation( room, moveDir, separationRadius_ );   // peer 회피 + head-on 해소

    setFacingDir( *this, moveDir );
    setDesiredVel( mu::Vec3( moveDir.x() * moveSpeed_, 0.f, moveDir.z() * moveSpeed_ ) );
}

// ─── AttackWindup ─────────────────────────────────────────────────────────────

void TacticalNpc::updateAttackWindup( Seconds dt, Room& room ) {
    GameSession* target = resolveTarget( room );
    if ( !target ) {
        releaseAttackReservation( room );
        targetId_ = 0;
        transitionTo( TacticalNpcState::Idle );
        return;
    }

    windupTimer_ += dt;
    if ( windupTimer_ >= attackWindupTime_ ) {
        mu::Vec3 targetPos = target->player()->pos();

        if ( (pos() - targetPos).len() <= attackRange_ ) {
            int32 newHp = std::max( 0, target->player()->hp() - static_cast<int32>(attackDamage_) );
            target->player()->setHp( newHp );
            recordHit( static_cast<uint16>(target->id()), newHp );

            if ( newHp <= 0 ) {
                releaseAttackReservation( room );
                targetId_ = 0;
                transitionTo( TacticalNpcState::Idle );
                return;
            }
        }

        transitionTo( TacticalNpcState::AttackRecover );
    }
}

// ─── AttackRecover ────────────────────────────────────────────────────────────

void TacticalNpc::updateAttackRecover( Seconds dt, Room& room ) {
    GameSession* target = resolveTarget( room );
    if ( !target ) {
        releaseAttackReservation( room );
        targetId_ = 0;
        transitionTo( TacticalNpcState::Idle );
        setDesiredVel( mu::Vec3{} );
        return;
    }

    constexpr float BODY_RADIUS = 0.8f;
    nearbyCache_.clear();
    room.findNearbyNpcPositions( pos(), BODY_RADIUS * 2.f, getId(), nearbyCache_ );
    mu::Vec3 push = calcSeparationForce( nearbyCache_, BODY_RADIUS * 2.f );

    mu::Vec3 driftVel( 0.f, 0.f, 0.f );
    if ( push.len2() > 0.0001f ) {
        driftVel = push * (moveSpeed_ * TACTICAL_RECOVER_SEPARATION_DRIFT_MULT);
    }

    setFacingDir( *this, target->player()->pos() - pos() );
    setDesiredVel( mu::Vec3( driftVel.x(), 0.f, driftVel.z() ) );

    recoverTimer_ += dt;

    if ( recoverTimer_ >= attackRecoverTime_ ) {
        if ( (pos() - target->player()->pos()).len2() <= attackRange_ * attackRange_ && canEnterAttackSlot( room ) ) {
            transitionTo( TacticalNpcState::AttackWindup );
        }
        else {
            transitionTo( TacticalNpcState::PressureWait );
        }
    }
}

// ─── PressureWait ─────────────────────────────────────────────────────────────

void TacticalNpc::updatePressureWait( Seconds dt, Room& room ) {
    GameSession* target = resolveTarget( room );
    if ( !target ) {
        releaseAttackReservation( room );
        targetId_ = 0;
        transitionTo( TacticalNpcState::Idle );
        setDesiredVel( mu::Vec3{} );
        return;
    }

    pressureWaitTimer_ += dt;
    updateReservedAttackStaleTimer( dt, room );

    Seconds reenterDelay = TACTICAL_PRESSURE_REENTER_MIN_TIME + static_cast<float>(getId() % 4u) * TACTICAL_PRESSURE_REENTER_STAGGER;
    bool canReenter = pressureWaitTimer_ >= reenterDelay && canEnterAttackSlot( room );

    if ( canReenter ) {
        pressureReentering_ = true;
        mu::Vec3 targetPos = target->player()->pos();

        if ( (pos() - targetPos).len2() <= attackRange_ * attackRange_ ) {
            transitionTo( TacticalNpcState::AttackWindup );
            setDesiredVel( mu::Vec3{} );
            return;
        }

        mu::Vec3 chaseDir = mu::Vec3( (targetPos - pos()).x(), 0.f, (targetPos - pos()).z() );

        if ( chaseDir.len2() > 0.01f ) {
            chaseDir = norm3( chaseDir );
            nearbyCache_.clear();
            room.findNearbyNpcPositions( pos(), separationRadius_, getId(), nearbyCache_ );

            mu::Vec3 sep = calcSeparationForce( nearbyCache_, separationRadius_ );
            mu::Vec3 sepPerp = sep - chaseDir * mu::dot( sep, chaseDir );
            mu::Vec3 moveDir = norm3( chaseDir + sepPerp * separationWeight_ );
            float spd = moveSpeed_ * TACTICAL_PRESSURE_REENTER_SPEED_MULT;

            setFacingDir( *this, moveDir );
            setDesiredVel( mu::Vec3( moveDir.x() * spd, 0.f, moveDir.z() * spd ) );
        }
        return;
    }

    pressureReentering_ = false;
    moveTowardPressureWait( dt, room, target->player()->pos(), target->player()->forward() );
}

// ─── Flank ────────────────────────────────────────────────────────────────────

void TacticalNpc::updateFlank( Seconds dt, Room& room ) {
    GameSession* target = resolveTarget( room );
    if ( !target ) {
        releaseAttackReservation( room );
        targetId_ = 0;
        transitionTo( TacticalNpcState::Idle );
        setDesiredVel( mu::Vec3{} );
        return;
    }

    if ( (target->player()->pos() - slotRefTargetPos_).len() > abandonDist_ ) {
        transitionTo( TacticalNpcState::Chase );
        return;
    }

    float distToSlot = lenXZ( pos() - assignedSlot_ );

    if ( distToSlot < 0.5f ) {
        float distToTarget = (pos() - target->player()->pos()).len();
        updateReservedAttackStaleTimer( dt, room );

        if ( distToTarget <= attackRange_ && canEnterAttackSlot( room ) ) {
            transitionTo( TacticalNpcState::AttackWindup );
        }
        else {
            transitionTo( canEnterAttackSlot( room )
                ? TacticalNpcState::Chase
                : TacticalNpcState::PressureWait );
        }

        setDesiredVel( mu::Vec3{} );
        return;
    }

    mu::Vec3 moveDir = norm3( assignedSlot_ - pos() );
    applyPeerSeparation( room, moveDir, separationRadius_ );   // peer 회피 + head-on 해소
    float spd = moveSpeed_ * TACTICAL_SPEED_MULT * speedMult_;

    setFacingDir( *this, moveDir );
    setDesiredVel( mu::Vec3( moveDir.x() * spd, 0.f, moveDir.z() * spd ) );
}

// ─── ChargeThrough ────────────────────────────────────────────────────────────

void TacticalNpc::updateChargeThrough( Room& room ) {
    GameSession* target = resolveTarget( room );

    if ( chargeComplete_ ) {
        if ( target ) {
            setFacingDir( *this, target->player()->pos() - pos() );
        }

        setDesiredVel( mu::Vec3{} );
        return;
    }

    if ( chargeDir_.len2() < 0.01f ) {
        mu::Vec3 dir = assignedSlot_ - pos();
        chargeDir_ = dir.len() > 0.01f ? norm3( dir ) : mu::Vec3( 1.f, 0.f, 0.f );
    }

    float impactRSq = impactRadius_ * impactRadius_;
    for ( GameSession* s : room.getLivingPlayers() ) {
        if ( !s ) {
            continue;
        }
        if ( (pos() - s->player()->pos()).len2() > impactRSq ) {
            continue;
        }

        int32 newHp = room.tryApplyWedgeChargeHit( chargeId_, s->id(), impactDamage_ );
        if ( newHp >= 0 ) {
            recordHit( static_cast<uint16>(s->id()), newHp );
        }
    }

    if ( lenXZ( pos() - assignedSlot_ ) < 0.75f ) {
        chargeComplete_ = true;
        setFacingDir( *this, chargeDir_ );
        setDesiredVel( mu::Vec3{} );
        return;
    }

    mu::Vec3 toExit = assignedSlot_ - pos();
    mu::Vec3 moveDir = toExit.len() > 0.01f ? norm3( toExit ) : chargeDir_;
    float spd = moveSpeed_ * TACTICAL_SPEED_MULT * speedMult_;

    setFacingDir( *this, moveDir );
    setDesiredVel( mu::Vec3( moveDir.x() * spd, 0.f, moveDir.z() * spd ) );
}

// ─── Confused ─────────────────────────────────────────────────────────────────

void TacticalNpc::updateConfused( Seconds dt, Room& room ) {
    targetId_ = 0;
    confusedRetargetTimer_ -= dt;

    float distToTarget = (pos() - confusedTarget_).len();

    if ( confusedRetargetTimer_ <= 0s || distToTarget < 0.75f ) {
        ++confusedWanderStep_;
        float seedA = static_cast<float>( (getId() * 37u + static_cast<uint32_t>( confusedWanderStep_ ) * 101u) % 997u );
        float seedB = static_cast<float>( (getId() * 53u + static_cast<uint32_t>( confusedWanderStep_ ) * 193u) % 991u );
        float r1 = std::sinf( seedA * 12.9898f ) * 43758.5453f;
        float r2 = std::sinf( seedB * 78.233f ) * 24634.6345f;
        r1 -= std::floorf( r1 );
        r2 -= std::floorf( r2 );

        constexpr float TWO_PI = 2.f * 3.14159265f;
        float angle = r1 * TWO_PI;
        float radius = CONFUSED_WANDER_RADIUS * (0.25f + 0.75f * r2);
        confusedTarget_ = confusedAnchor_ + mu::Vec3( std::cosf( angle ) * radius, 0.f, std::sinf( angle ) * radius );
        confusedRetargetTimer_ = CONFUSED_RETARGET_MIN + CONFUSED_RETARGET_SPAN * r1;
        distToTarget = (pos() - confusedTarget_).len();
    }

    mu::Vec3 toTarget = confusedTarget_ - pos();
    mu::Vec3 moveDir = toTarget.len() > 0.01f
        ? norm3( toTarget )
        : mu::Vec3( 1.f, 0.f, 0.f );

    float confSepRad = std::max( separationRadius_, CONFUSED_SEPARATION_RADIUS );
    nearbyCache_.clear();
    room.findNearbyNpcPositions( pos(), confSepRad, getId(), nearbyCache_ );

    mu::Vec3 sep = calcSeparationForce( nearbyCache_, confSepRad );
    if ( sep.len2() > 0.01f ) {
        moveDir = norm3( moveDir + norm3( sep ) * CONFUSED_SEPARATION_WEIGHT );
    }

    if ( (pos() - confusedAnchor_).len() > CONFUSED_WANDER_RADIUS * 1.25f ) {
        moveDir = norm3( confusedAnchor_ - pos() );
    }

    float spd = moveSpeed_ * CONFUSED_SPEED_MULT;
    setFacingDir( *this, moveDir );
    setDesiredVel( mu::Vec3( moveDir.x() * spd, 0.f, moveDir.z() * spd ) );
}

// ─── HoldSlot ─────────────────────────────────────────────────────────────────

void MU_CALLCONV TacticalNpc::applyBlockerAvoidance( Room& room, mu::Vec3& slotDir, float distToSlot ) {
    // 슬롯 근처에선 정밀 정렬을 위해 회피 비활성(en route에서만 우회).
    if ( distToSlot <= separationRadius_ ) {
        return;
    }

    nearbyCache_.clear();
    room.findNearbyBlockerPositions( pos(), TACTICAL_BLOCKER_AVOID_RADIUS, nearbyCache_ );   // 플레이어 + 보스
    if ( nearbyCache_.empty() ) {
        return;
    }

    mu::Vec3 sep = calcSeparationForce( nearbyCache_, TACTICAL_BLOCKER_AVOID_RADIUS );
    mu::Vec3 sepPerp = sep - slotDir * mu::dot( sep, slotDir );   // 진행 방향에 수직인 성분만(전진 유지)
    slotDir = norm3( slotDir + sepPerp * separationWeight_ );
}

void MU_CALLCONV TacticalNpc::applyPeerSeparation( Room& room, mu::Vec3& moveDir, float radius ) {
    if ( moveDir.len2() <= 0.01f ) {
        return;
    }

    nearbyCache_.clear();
    room.findNearbyNpcPositions( pos(), radius, getId(), nearbyCache_ );   // 이웃 전술 NPC(peer)
    mu::Vec3 sep = calcSeparationForce( nearbyCache_, radius );
    if ( sep.len2() < 1e-4f ) {
        return;
    }

    mu::Vec3 sepPerp = sep - moveDir * mu::dot( sep, moveDir );   // 진행 방향에 수직 성분(기존 회피)
    mu::Vec3 steer = moveDir + sepPerp * separationWeight_;

    // 정면(head-on) 감지: 이웃이 진행방향 정면이면 sep가 moveDir 반대 → 수직 성분 소실 →
    // 모든 NPC가 동일하게 '오른쪽'으로 비키는 bias를 줘 마주친 둘이 반대편으로 갈라지게 한다.
    float forwardBlock = -mu::dot( norm3( sep ), moveDir );
    if ( forwardBlock > HEADON_DOT_THRESHOLD ) {
        mu::Vec3 right( moveDir.z(), 0.f, -moveDir.x() );   // 진행방향 기준 오른쪽(top-down)
        steer = steer + right * (HEADON_BIAS * forwardBlock);
    }

    moveDir = norm3( steer );
}

bool MU_CALLCONV TacticalNpc::updateSlotSettled( float distToSlot ) {
    if ( slotSettled_ ) {
        if ( distToSlot > separationRadius_ * TACTICAL_SLOT_UNSETTLE_RADIUS_MULT ) {
            slotSettled_ = false;   // 크게 밀려나면 래치 해제 → 재접근
        }
    }
    else if ( distToSlot < separationRadius_ * TACTICAL_SLOT_SETTLE_RADIUS_MULT ) {
        slotSettled_ = true;        // 충분히 가까우면 안착
    }
    return slotSettled_;
}

void MU_CALLCONV TacticalNpc::settleAtSlot() {
    setDesiredVel( mu::Vec3{} );                 // 모터 목표 정지(제동 유지)
    mu::Vec3 v = linearVel();                    // 솔버 주입 잔여 속도 감쇠(Y는 중력 보존)
    setLinearVel( mu::Vec3( v.x() * TACTICAL_SLOT_SETTLE_VEL_DAMP, v.y(), v.z() * TACTICAL_SLOT_SETTLE_VEL_DAMP ) );
}

void TacticalNpc::updateHoldSlot( Room& room ) {
    GameSession* target = resolveTarget( room );
    if ( guardNearestPlayer_ ) {
        target = nullptr;
        float bestDistSq = -1.f;

        for ( GameSession* s : room.getLivingPlayers() ) {
            float dSq = (pos() - s->player()->pos()).len2();

            if ( bestDistSq < 0.f || dSq < bestDistSq ) {
                bestDistSq = dSq;
                target = s;
            }
        }
    }

    if ( !target ) {
        if ( useHoldFacing_ ) {
            float distToSlot = lenXZ( pos() - assignedSlot_ );

            if ( updateSlotSettled( distToSlot ) ) {
                setFacingDir( *this, holdFacing_ );
                settleAtSlot();
                return;
            }

            mu::Vec3 slotDir = norm3( assignedSlot_ - pos() );
            applyBlockerAvoidance( room, slotDir, distToSlot );   // 플레이어/보스 우회
            if ( distToSlot > separationRadius_ ) {               // en route에서만 peer 회피(슬롯 근처는 밀집 패킹 보존)
                applyPeerSeparation( room, slotDir, separationRadius_ );
            }
            float spd = moveSpeed_ * TACTICAL_SPEED_MULT * speedMult_;
            if ( distToSlot < TACTICAL_SLOT_ARRIVE_SLOW_RADIUS )   // 도착 감속(오버슈트 진동 방지)
                spd *= std::max( TACTICAL_SLOT_ARRIVE_MIN_SCALE, distToSlot / TACTICAL_SLOT_ARRIVE_SLOW_RADIUS );
            setFacingDir( *this, slotDir );
            setDesiredVel( mu::Vec3( slotDir.x() * spd, 0.f, slotDir.z() * spd ) );
            return;
        }

        targetId_ = 0;
        transitionTo( TacticalNpcState::Idle );
        setDesiredVel( mu::Vec3{} );
        return;
    }

    float distToSlot = lenXZ( pos() - assignedSlot_ );

    if ( updateSlotSettled( distToSlot ) ) {
        if ( useHoldFacing_ && holdFacing_.len2() > 0.01f ) {
            setFacingDir( *this, holdFacing_ );
        }
        else {
            setFacingDir( *this, target->player()->pos() - pos() );
        }

        settleAtSlot();
        return;
    }

    mu::Vec3 slotDir = norm3( assignedSlot_ - pos() );
    applyBlockerAvoidance( room, slotDir, distToSlot );   // 플레이어/보스 우회
    if ( distToSlot > separationRadius_ ) {               // en route에서만 peer 회피(슬롯 근처는 밀집 패킹 보존)
        applyPeerSeparation( room, slotDir, separationRadius_ );
    }
    float spd = moveSpeed_ * TACTICAL_SPEED_MULT * speedMult_;
    if ( distToSlot < TACTICAL_SLOT_ARRIVE_SLOW_RADIUS )   // 도착 감속(오버슈트 진동 방지)
        spd *= std::max( TACTICAL_SLOT_ARRIVE_MIN_SCALE, distToSlot / TACTICAL_SLOT_ARRIVE_SLOW_RADIUS );
    setFacingDir( *this, slotDir );
    setDesiredVel( mu::Vec3( slotDir.x() * spd, 0.f, slotDir.z() * spd ) );
}

// ─── Dead ─────────────────────────────────────────────────────────────────────

void TacticalNpc::updateDead( Room& room ) {
    releaseAttackReservation( room );
    targetId_ = 0;

    if ( state_ != TacticalNpcState::Dead ) {
        transitionTo( TacticalNpcState::Dead );
    }

    setDesiredVel( mu::Vec3{} );
}

// ─── resolveTarget / recordHit ────────────────────────────────────────────────

GameSession* TacticalNpc::resolveTarget( Room& room ) const {
    if ( targetId_ == 0 ) {
        return nullptr;
    }
    return room.findLivingSessionByPlayerId( static_cast<int32>(targetId_) );
}

void TacticalNpc::recordHit( uint16 targetId, int32 newHp ) {
    frameHits_.push_back( { targetId, newHp } );
}

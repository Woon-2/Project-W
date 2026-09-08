#include "rspch.hpp"
#include "IMidBossTactic.hpp"
#include "PlatoonLeader.hpp"
#include "Room.hpp"
#include "GameSession.hpp"
#include "object.hpp"
#include <random>

PlatoonLeader::PlatoonLeader( Object&& base, const TacticalNpcConfig& cfg, std::unique_ptr<IMidBossTactic> tactic )
    : TacticalNpc( std::move( base ), cfg ), tactic_( std::move( tactic ) )
{}

void PlatoonLeader::addSquad( TacticalSquad* squad ) {
    squads_.push_back( squad );
}

void PlatoonLeader::setTactic( std::unique_ptr<IMidBossTactic> tactic ) {
    tactic_ = std::move( tactic );
}

void PlatoonLeader::removeDeadMembersFromSquads() {
    for ( TacticalSquad* sq : squads_ ) {
        sq->removeDeadMembers();
    }
}

void PlatoonLeader::pushConfusedToSquads( Room& room ) {
    for ( TacticalSquad* sq : squads_ ) {
        sq->pushConfusedToMembers( room );
    }
}

void PlatoonLeader::setFacing( mu::Vec3 dir ) {
    if ( dir.len2() > 0.01f ) {
        setOrient( mu::NQuat( mu::Radian(), mu::Radian(), mu::Radian( std::atan2( dir.x(), dir.z() ) ) ) );
    }
}

void PlatoonLeader::applyHitToSession( GameSession* session, float damage ) {
    if ( !session ) {
        return;
    }

    Player* p = session->player();
    int32 newHp = std::max( 0, p->hp() - static_cast<int32>(damage) );
    p->setHp( newHp );
    recordHit( static_cast<uint16>(session->id()), newHp );
}

bool PlatoonLeader::castSkillAttack( Room& room ) {
    if ( !hasSkillAttacks() ) return false;   // no roster -> caller does legacy melee
    const TacticalNpcAttack& atk = pickAttack();
    if ( !atk.clipKey.empty() ) animController().switchClip( atk.clipKey );
    const uint32 seed = std::random_device{}();
    room.skillStartInternal( static_cast<int32>( getId() ), atk.skillId, seed, attackDamageScale_ );
    return true;
}

bool PlatoonLeader::castSkillAt( Room& room, uint32 skillId, std::string_view clipKey,
                                 mu::Vec3 anchorPos, float damageScale ) {
    if ( skillId == 0 ) return false;
    if ( !clipKey.empty() ) animController().switchClip( std::string( clipKey ) );
    const uint32 seed = std::random_device{}();
    room.skillStartInternal( static_cast<int32>( getId() ), skillId, seed, damageScale, &anchorPos );
    return true;
}

TacticalNpcUpdateResult PlatoonLeader::update( Seconds dt, Room& room ) {
    frameHits_.clear();

    if ( hp() <= 0 ) {
        bool wasDead = deathReported_;
        if ( !deathReported_ ) {
            deathReported_ = true;

            if ( tactic_ ) {
                tactic_->onLeaderDead( room, *this );
            }
            else {
                pushConfusedToSquads( room );
            }
        }

        updateDead( room );

        auto result = TacticalNpcUpdateResult{
            .hits = std::move( frameHits_ )
        };
        result.justDied = !wasDead;   // 막 사망한 첫 틱
        return result;
    }

    if ( tactic_ ) {
        tactic_->update( dt, room, *this );
    }

    auto result = TacticalNpcUpdateResult{
		.hits = std::move( frameHits_ )
    };
    return result;
}

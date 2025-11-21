#ifndef ROOM_HPP
#define ROOM_HPP

#include "physics.hpp"
#include "object.hpp"

class Level;

class Room : std::enable_shared_from_this<Room> {
public:
	Room( int32 roomId )
		: roomId_( roomId ), mtx_( ), users_( ), cubes_( ), playerStarts_( ),
		physicSystem_( ), physicUpdateAcc_( 0s ), physicUpdateInterval( 1s/60.f ) {}

	void init( const Level& levelData );
	void enter( const SPGameSession& user );
	void leave( const SPGameSession& user );
	void broadcast( const SPSendBuffer& packet );
	bool empty( );

	void setRoomId( int32 roomId ) { roomId_ = roomId; }
	int32 getRoomId( ) const { return roomId_; }
	const std::vector<SPGameSession>& getUsers( ) { 
		std::lock_guard lock( mtx_ );
		return users_; 
	}

private:
	int32 roomId_;
	std::recursive_mutex mtx_;
	std::vector<SPGameSession> users_;

	std::vector<Object> cubes_;
	std::vector<Object> playerStarts_;

	PhysicSystem physicSystem_;
	Seconds physicUpdateAcc_;	// 물리 업데이트를 위한 시간 누산기
	Seconds physicUpdateInterval;	// 물리 업데이트 주기
};

#endif // ROOM_HPP
#ifndef ROOM_HPP
#define ROOM_HPP

class Room : std::enable_shared_from_this<Room> {
public:
	Room( int32 roomId ) : roomId_( roomId ), mtx_( ), users_( ) {}

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
};

#endif // ROOM_HPP
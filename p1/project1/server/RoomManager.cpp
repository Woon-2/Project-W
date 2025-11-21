#include "pch.hpp"
#include "RoomManager.hpp"
#include "Room.hpp"

SPRoom RoomManager::findRoom( int32 roomId ) {
	std::lock_guard lock( mtx_ );

	if ( auto search = rooms_.find( roomId ); search != rooms_.end( ) ) {
		return search->second;
	}
	else {
		return nullptr;
	}
}

SPRoom RoomManager::createRoom( int32 roomId ) {
	std::lock_guard lock( mtx_ );

	auto newRoom = std::make_shared<Room>( roomId );
	rooms_[ roomId ] = newRoom;
	
	ASSERT_CRASH(pLevel_ != nullptr);
	newRoom->init(*pLevel_);

	return newRoom;
}

void RoomManager::removeRoom( int32 roomId ) {
	std::lock_guard lock( mtx_ );
	rooms_.erase( roomId );
}

std::mutex RoomManager::mtx_;
std::unordered_map<int32, SPRoom> RoomManager::rooms_;
Level* RoomManager::pLevel_;

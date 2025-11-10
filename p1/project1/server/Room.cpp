#include "pch.hpp"
#include "Room.hpp"
#include "GameSession.hpp"

void Room::enter( const SPGameSession& user ) {
	std::lock_guard lock( mtx_ );
	users_.push_back( user );
}

void Room::leave( const SPGameSession& user ) {
	std::lock_guard lock( mtx_ );
	std::erase( users_, user );
}

void Room::broadcast( const SPSendBuffer& sendBuffer ) {
	std::lock_guard lock( mtx_ );
	for ( auto& user : users_ ) {
		user->send( sendBuffer );
	}
}

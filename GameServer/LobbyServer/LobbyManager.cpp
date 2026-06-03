#include "lspch.hpp"
#include "LobbyManager.hpp"
#include "LobbyRoom.hpp"

LobbyRoom* LobbyManager::createRoom() {
	std::unique_lock lock( mutex_ );

	std::string code;
	do {
		code = generateCode();
	} while ( rooms_.count( code ) );

	auto room = new LobbyRoom( code );
	rooms_[ code ] = room;

	return room;
}

LobbyRoom* LobbyManager::findRoom( const std::string& code ) {
	std::shared_lock lock( mutex_ );
	auto it = rooms_.find( code );
	return it != rooms_.end() ? it->second : nullptr;
}

void LobbyManager::removeRoom( const std::string& code ) {
	std::unique_lock lock( mutex_ );

	auto it = rooms_.find( code );
	if ( it == rooms_.end() ) {
		return;
	}

	delete it->second;
	rooms_.erase( it );
}

std::shared_mutex LobbyManager::mutex_;
std::unordered_map<std::string, LobbyRoom*> LobbyManager::rooms_;

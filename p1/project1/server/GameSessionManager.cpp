#include "pch.hpp"
#include "GameSessionManager.hpp"
#include "GameSession.hpp"

void GameSessionManager::add( const SPGameSession& session ) {
	std::lock_guard<std::mutex> lock( mtx_ );
	sessions_.insert( session );
}

void GameSessionManager::remove( const SPGameSession& session ) {
	std::lock_guard<std::mutex> lock( mtx_ );
	sessions_.erase( session );
}

void GameSessionManager::broadcast( const SPSendBuffer& sendBuffer ) {
	std::lock_guard<std::mutex> lock( mtx_ );
	for ( auto session : sessions_ ) {
		session->send( sendBuffer );
	}
}

std::mutex GameSessionManager::mtx_;
std::set<SPGameSession> GameSessionManager::sessions_;

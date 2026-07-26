#include "lspch.hpp"
#include "GameSessionManager.hpp"
#include "GameSession.hpp"

void GameSessionManager::add( const std::shared_ptr<GameSession>& session ) {
	std::lock_guard lock( mutex_ );
	sessions_[ static_cast<uint16>( session->id() ) ] = session;
}

void GameSessionManager::remove( uint16 id ) {
	std::lock_guard lock( mutex_ );
	sessions_.erase( id );
}

std::shared_ptr<GameSession> GameSessionManager::find( uint16 id ) {
	std::lock_guard lock( mutex_ );
	auto it = sessions_.find( id );
	return it != sessions_.end() ? it->second : nullptr;
}

int32 GameSessionManager::count() {
	std::lock_guard lock( mutex_ );
	return static_cast<int32>( sessions_.size() );
}

bool GameSessionManager::bindAccount( int64 accountId, uint16 sessionId ) {
	std::lock_guard lock( mutex_ );
	return loginMap_.try_emplace( accountId, sessionId ).second;
}

void GameSessionManager::unbindAccount( int64 accountId ) {
	std::lock_guard lock( mutex_ );
	loginMap_.erase( accountId );
}

std::mutex GameSessionManager::mutex_;
std::unordered_map<uint16, std::shared_ptr<GameSession>> GameSessionManager::sessions_;
std::unordered_map<int64, uint16> GameSessionManager::loginMap_;

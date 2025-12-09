#include "pch.hpp"
#include "GameSessionManager.hpp"
#include "GameSession.hpp"

void GameSessionManager::add( const SPGameSession& session ) {
	std::lock_guard<std::mutex> lock( mtx_ );
	sessions_[session->getId()] = session;
}

void GameSessionManager::remove( const SPGameSession& session ) {
	std::lock_guard<std::mutex> lock( mtx_ );
	auto eraseCnt = sessions_.erase(session->getId());
	ASSERT_CRASH(eraseCnt == 1);
}

void GameSessionManager::broadcast( const SPSendBuffer& sendBuffer ) {
	std::lock_guard<std::mutex> lock( mtx_ );
	for (auto& [id, session] : sessions_) {
		session->send(sendBuffer);
	}
}

std::mutex GameSessionManager::mtx_;
std::unordered_map<int32, SPGameSession> GameSessionManager::sessions_;

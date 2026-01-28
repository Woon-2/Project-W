#include "pch.hpp"
#include "GameSessionManager.hpp"
#include "GameSession.hpp"
#include "IdPool.hpp"
#include "Memory.hpp"

void GameSessionManager::addSession(GameSession* session) {
	std::lock_guard<std::mutex> lock(mtx_);
	sessionMap_[session->id()] = session;
}

void GameSessionManager::removeSession(uint32 sessionId) {
	mtx_.lock();
	xdelete(sessionMap_[sessionId]);
	auto eraseCnt = sessionMap_.erase(sessionId);
	mtx_.unlock();

	ASSERT_CRASH(eraseCnt == 1);
	IdPool::push(sessionId);
}

std::mutex GameSessionManager::mtx_;
std::unordered_map<uint32, GameSession*> GameSessionManager::sessionMap_;

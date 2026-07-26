#include "rspch.hpp"
#include "AccountRegistry.hpp"

bool AccountRegistry::bind( int64 accountId, uint16 sessionId ) {
	std::lock_guard lock( mutex_ );
	return map_.try_emplace( accountId, sessionId ).second;
}

void AccountRegistry::unbind( int64 accountId ) {
	std::lock_guard lock( mutex_ );
	map_.erase( accountId );
}

std::mutex AccountRegistry::mutex_;
std::unordered_map<int64, uint16> AccountRegistry::map_;

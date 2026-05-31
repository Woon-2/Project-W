#include "lspch.hpp"
#include "LobbyManager.hpp"
#include "LobbyRoom.hpp"
#include <shared_mutex>
#include <random>

namespace {
	std::shared_mutex mutex_;
	std::unordered_map<std::string, LobbyRoom*> rooms_;

	std::string generateCode() {
		static constexpr char pool[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
		static std::mt19937 rng{ std::random_device{}() };
		static std::uniform_int_distribution<int> dist(0, 35);
		std::string code(6, '\0');
		for (auto& c : code) c = pool[dist(rng)];
		return code;
	}
}

LobbyRoom* LobbyManager::createRoom() {
	std::unique_lock lock(mutex_);

	std::string code;
	do { code = generateCode(); } while (rooms_.count(code));

	auto* room = new LobbyRoom(code);
	rooms_[code] = room;

	return room;
}

LobbyRoom* LobbyManager::findRoom(std::string_view code) {
	std::shared_lock lock(mutex_);
	auto it = rooms_.find(std::string(code));
	return it != rooms_.end() ? it->second : nullptr;
}

void LobbyManager::removeRoom(std::string_view code) {
	std::unique_lock lock(mutex_);

	auto it = rooms_.find(std::string(code));
	if (it == rooms_.end())
		return;

	delete it->second;
	rooms_.erase(it);
}

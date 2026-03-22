#include "rspch.hpp"
#include "RoomManager.hpp"
#include "Room.hpp"
#include "ObjectPool.hpp"
#include "IdPool.hpp"

Room* RoomManager::makeRoom() {
	auto roomId = RoomIdPool::pop();
	auto newRoom = ObjectPool<Room>::pop(roomId);

	rmMtx_.lock();
	rooms_.push_back(newRoom);
	roomIdMap_[roomId] = newRoom;
	rmMtx_.unlock();

	return newRoom;
}

Room* RoomManager::getRoom(int32 roomId) {
	std::lock_guard<std::mutex> lock(rmMtx_);
	return roomIdMap_[roomId];
}

void RoomManager::removeRoom(int32 roomId) {
	std::lock_guard<std::mutex> lock(rmMtx_);
	std::erase_if(rooms_, [roomId](Room* room) { return room->id() == roomId; });
	roomIdMap_.erase(roomId);
}

std::mutex RoomManager::rmMtx_;
std::vector<Room*> RoomManager::rooms_;
std::unordered_map<int32, Room*> RoomManager::roomIdMap_;

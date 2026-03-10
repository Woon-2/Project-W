#include "rspch.hpp"
#include "RoomManager.hpp"
#include "Room.hpp"

void RoomManager::addRoom(Room* room) {
	std::lock_guard<std::mutex> lock(rmMtx_);
	rooms_.push_back(room);
	roomIdMap_[room->id()] = room;
}

void RoomManager::removeRoom(int32 roomId) {
	std::lock_guard<std::mutex> lock(rmMtx_);
	std::erase_if(rooms_, [roomId](Room* room) { return room->id() == roomId; });
	roomIdMap_.erase(roomId);
}

Room* RoomManager::getRoom(int32 roomId) {
	std::lock_guard<std::mutex> lock(rmMtx_);
	return roomIdMap_[roomId];
}

std::mutex RoomManager::rmMtx_;
std::vector<Room*> RoomManager::rooms_;
std::unordered_map<int32, Room*> RoomManager::roomIdMap_;

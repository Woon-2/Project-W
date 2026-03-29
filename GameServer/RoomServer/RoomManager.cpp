#include "rspch.hpp"
#include "RoomManager.hpp"
#include "Room.hpp"
#include "ObjectPool.hpp"
#include "IdPool.hpp"

Room* RoomManager::makeRoom() {
	auto roomId = RoomIdPool::pop();
	auto newRoom = ObjectPool<Room>::pop(roomId);	// room 삭제 시 object pool에 반환해야 함. 아직 처리 안 돼있음.

	ASSERT_CRASH(pLevel_ != nullptr);
	newRoom->init(pLevel_);

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
	Room* room = roomIdMap_[roomId];

	rmMtx_.lock();
	std::erase_if(rooms_, [roomId](Room* room) { return room->id() == roomId; });
	roomIdMap_.erase(roomId);
	rmMtx_.unlock();

	ObjectPool<Room>::push(room);
}

std::mutex RoomManager::rmMtx_;
std::vector<Room*> RoomManager::rooms_;
std::unordered_map<int32, Room*> RoomManager::roomIdMap_;

const Level* RoomManager::pLevel_ = nullptr;
const Model* RoomManager::pPlayerModel_ = nullptr;

#include "rspch.hpp"
#include "RoomManager.hpp"
#include "Room.hpp"
#include "serverAnimation.hpp"
#include "IdPool.hpp"
#include "JobTimer.hpp"

Room* RoomManager::findOrCreateRoomByCode(const std::string& code) {
	// find과 create를 한 락 안에서 원자적으로 수행해, 같은 코드로 동시에 들어오는 파티원이
	// 서로 다른 방을 만드는 것을 막는다. (init/doTimer는 JobTimer/풀의 자체 락만 추가로 잡으며
	// rmMtx_→그쪽 단방향이라 사이클 없음. enter는 핫패스가 아니라 락 구간이 길어도 무방.)
	std::lock_guard<std::mutex> lock(rmMtx_);

	auto it = codeRoomMap_.find(code);
	if (it != codeRoomMap_.end()) {
		return it->second;
	}

	auto roomId = RoomIdPool::pop();
	auto newRoom = ObjectPool<Room>::pop(roomId);

	ASSERT_CRASH(pLevel_ != nullptr);
	newRoom->init(pLevel_);
	newRoom->setCode(code);

	Milliseconds delay = 1s / 60.f;
	newRoom->doTimer( delay, [newRoom] {
		newRoom->update();
	} );

	rooms_.push_back(newRoom);
	roomIdMap_[roomId] = newRoom;
	codeRoomMap_[code] = newRoom;

	return newRoom;
}

Room* RoomManager::findRoom(int32 roomId) {
	std::lock_guard<std::mutex> lock(rmMtx_);

	auto iter = roomIdMap_.find(roomId);
	if (iter == roomIdMap_.end()) {
		return nullptr;
	}

	return iter->second;
}

void RoomManager::removeRoom(int32 roomId) {
	rmMtx_.lock();
	Room* room = roomIdMap_[roomId];
	if (room) {
		codeRoomMap_.erase(room->code());
	}
	std::erase_if(rooms_, [roomId](Room* r) { return r->id() == roomId; });
	roomIdMap_.erase(roomId);
	rmMtx_.unlock();

	ObjectPool<Room>::push(room);
}

std::mutex RoomManager::rmMtx_;
std::vector<Room*> RoomManager::rooms_;
std::unordered_map<int32, Room*> RoomManager::roomIdMap_;
std::unordered_map<std::string, Room*> RoomManager::codeRoomMap_;

const Level*  RoomManager::pLevel_            = nullptr;
const Model*  RoomManager::pPlayerModel_      = nullptr;
const std::vector<ServerAnimClip>* RoomManager::pPlayerAnimations_ = nullptr;

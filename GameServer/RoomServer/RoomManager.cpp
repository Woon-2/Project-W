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
	Room* room = nullptr;
	{
		std::lock_guard<std::mutex> lock(rmMtx_);

		auto it = roomIdMap_.find(roomId);
		if (it != roomIdMap_.end()) {
			room = it->second;
			codeRoomMap_.erase(room->code());
			roomIdMap_.erase(it);
		}
		std::erase_if(rooms_, [roomId](Room* r) { return r->id() == roomId; });

		// 여기서 파괴하지 않는다. 이 함수는 방 자신의 JobQueue 잡(leave)에서 불리므로 즉시
		// push하면 ~Room()이 자기 execute() 콜스택 안에서 돌고, 같은 배치의 잔여 잡(60Hz 틱)과
		// execute 루프 자체가 해제된 메모리를 만진다(§4-P0 D — physicsWorld_.step /
		// try_dequeue_bulk 크래시로 발현). 큐 유휴 확인 후 sweepPendingRooms가 반납한다.
		if (room) {
			pendingDestroy_.push_back({ room, 0 });
		}
	}

	// room == nullptr이면 같은 룸에 대해 removeRoom이 두 번 불린 것이다(= 잡 큐 직렬화가
	// 깨져 leave 잡이 중복 실행됐다는 뜻 — docs/objectIdLifecycle.md H3). 보고만 하고 빠진다.
	if (room == nullptr) {
		std::cout << "[RoomManager] DOUBLE REMOVE id=" << roomId
			<< " (room not in map -- job queue serialization broken?)\n";
	}
}

void RoomManager::sweepPendingRooms() {
	// 반납(~Room)은 무거우므로 락 밖에서 한다. 유휴는 "연속 2회" 관측 후에만 인정한다 —
	// push가 jobCount_를 먼저 올리므로 유휴로 보이는 큐를 워커가 새로 잡을 수는 없지만,
	// 제거 직전에 Room*를 얻어둔 스레드가 뒤늦게 doAsync하는 극단 레이스에 스윕 주기만큼의
	// 유예를 더 준다.
	std::vector<Room*> reapable;
	{
		std::lock_guard<std::mutex> lock(rmMtx_);
		for (auto it = pendingDestroy_.begin(); it != pendingDestroy_.end(); ) {
			if (it->room->jobQueueIdle()) {
				if (++it->idleSweeps >= 2) {
					reapable.push_back(it->room);
					it = pendingDestroy_.erase(it);
					continue;
				}
			}
			else {
				it->idleSweeps = 0;
			}
			++it;
		}
	}

	for (Room* room : reapable) {
		std::cout << "[RoomManager] room reaped id=" << room->id() << '\n';
		ObjectPool<Room>::push(room);
	}
}

bool RoomManager::postJob(int32 roomId, Job* job) {
	// 조회와 push를 한 락 안에서 해, 조회 직후 방이 제거·반납되는 사이에 push하는
	// TOCTOU(§4-P0 D의 (b) 경로)를 막는다. 맵에 있는 방은 반납 대상이 아니므로 push가
	// 안전하다. (락 순서: findOrCreateRoomByCode와 같은 rmMtx_ → jobTimerMtx_/풀 단방향.)
	std::lock_guard<std::mutex> lock(rmMtx_);

	auto it = roomIdMap_.find(roomId);
	if (it == roomIdMap_.end()) {
		return false;
	}

	it->second->pushJob(job);
	return true;
}

std::mutex RoomManager::rmMtx_;
std::vector<Room*> RoomManager::rooms_;
std::unordered_map<int32, Room*> RoomManager::roomIdMap_;
std::unordered_map<std::string, Room*> RoomManager::codeRoomMap_;
std::vector<RoomManager::PendingRoom> RoomManager::pendingDestroy_;

const Level*  RoomManager::pLevel_            = nullptr;
const Model*  RoomManager::pPlayerModel_      = nullptr;
const std::vector<ServerAnimClip>* RoomManager::pPlayerAnimations_ = nullptr;

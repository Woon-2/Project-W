#ifndef ROOM_HPP
#define ROOM_HPP

#include "physics.hpp"
#include "object.hpp"
#include "GameLogic.hpp"

struct Level;

class Room : std::enable_shared_from_this<Room> {
public:
	Room(int32 roomId)
		: roomId_(roomId), mtx_(), users_(), idUserMap_(), msgQueue_(), cubes_(), playerStarts_(),
		physicSystem_(), physicUpdateAcc_(0s), physicUpdateInterval(1s / 60.f) {}

	void init(const Level& levelData);
	void update(Milliseconds deltaTime);

	void enqueueMessage(const LogicMessage& msg) { msgQueue_.enqueue(msg); }
	void processMessage(Milliseconds deltaTime);
	
	void enter(int32 playerId);
	void leave(int32 playerId);
	void broadcast(const SPSendBuffer& packet);
	bool empty();

	bool validateMove(mu::Vec3 clientCurrPos, mu::Vec3 clientCurrVel, uint32 clientTimeStamp,
		Milliseconds deltaTime, const std::shared_ptr<Object>& serverUserObj);

	void setRoomId(int32 roomId) { roomId_ = roomId; }
	int32 getRoomId() const { return roomId_; }

private:
	enum : uint8 {
		MoveW = 0x1,
		MoveA = 0x2,
		MoveS = 0x4,
		MoveD = 0x8
	};

	int32 roomId_;
	std::recursive_mutex mtx_;
	std::vector<std::shared_ptr<Object>> users_;
	std::unordered_map<int32, std::shared_ptr<Object>> idUserMap_;
	CCQueue<LogicMessage> msgQueue_;

	std::vector<Object> cubes_;
	std::vector<Object> playerStarts_;

	PhysicSystem physicSystem_;
	Seconds physicUpdateAcc_;	// 물리 업데이트를 위한 시간 누산기
	Seconds physicUpdateInterval;	// 물리 업데이트 주기
};

#endif // ROOM_HPP
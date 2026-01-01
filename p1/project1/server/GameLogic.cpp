#include "pch.hpp"
#include "GameLogic.hpp"
#include "Room.hpp"
#include "RoomManager.hpp"

void GameLogic::run() {
	while (running_) {
		logicTimer_.tick();
		
		auto dt = logicTimer_.deltaTime<Milliseconds>();
		accTime_ += dt;

		processMessage();

		while (accTime_ >= logicUpdateInterval_) {
			for (auto& room : rooms_) {
				room->update(logicUpdateInterval_);
			}
			accTime_ -= logicUpdateInterval_;
		}

		std::this_thread::yield();
	}
}

void GameLogic::processMessage() {
	const auto bulkSize = 1000u;
	auto messages = std::vector<LogicMessage>(bulkSize);
	auto size = msgQueue_.try_dequeue_bulk(messages.begin(), bulkSize);

	LogicMessage msg;
	while (msgQueue_.try_dequeue(msg)) {
		messages.push_back(msg);
		++size;
	}

	for (int32 i = 0; i < size; ++i) {
		auto msgType = messages[i].type;

		switch (msgType) {
		case LogicMsgType::AddRoom: {
			if (rooms_.size() >= maxRoomCount_) {
				// 최대 방 개수를 정해놓음. 방을 더 이상 만들 수 없을 때를 대비해야 함.
				break;
			}

			auto room = RoomManager::findRoom(messages[i].roomId);
			ASSERT_CRASH(room);
			
			rooms_.push_back(room);
			idRoomMap_[room->getRoomId()] = room;
			break;
		}

		case LogicMsgType::RemoveRoom: {
			auto vEraseCnt = std::erase_if(rooms_, [&](const SPRoom& room) {
				return room->getRoomId() == messages[i].roomId;
			});

			auto umEraseCnt = idRoomMap_.erase(messages[i].roomId);

			ASSERT_CRASH(vEraseCnt > 0 && umEraseCnt == 1);
			break;
		}

		case LogicMsgType::UserEnter:
		case LogicMsgType::UserLeave:
		case LogicMsgType::UserMouseMove:
		case LogicMsgType::UserMoveState:
		case LogicMsgType::UserFire:
		case LogicMsgType::UserReload:
			idRoomMap_[messages[i].roomId]->enqueueMessage(messages[i]);
			break;

		default:
			break;
		}
	}
}

const int32 GameLogic::maxRoomCount_ = 100;

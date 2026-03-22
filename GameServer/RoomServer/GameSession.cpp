#include "rspch.hpp"
#include "GameSession.hpp"
#include "PacketManager.hpp"
#include "SendBuffer.hpp"

// temporary --------------------------------
#include "IdPool.hpp"
#include "Room.hpp"
#include "RoomManager.hpp"
static std::atomic_int32_t totalSessions{0};
static const int32 maxRoomSessions = 4;
// ------------------------------------------

void GameSession::onConnected() {
	if (totalSessions.load() % maxRoomSessions == 0) {
		myRoom_ = RoomManager::makeRoom();
	}
	else {
		myRoom_ = RoomManager::getRoom(RoomIdPool::currId());
	}

	myRoom_->doAsync(&Room::enter, this);
}

void GameSession::onDisconnected() {
	myRoom_->doAsync(&Room::leave, this);
}

void GameSession::processPacket(byte* buffer, int32 len) {
	PacketManager::handlePacket(buffer, len);
}

void GameSession::onSend(int32 len) {

}

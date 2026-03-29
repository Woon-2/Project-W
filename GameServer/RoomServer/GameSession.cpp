#include "rspch.hpp"
#include "GameSession.hpp"
#include "PacketManager.hpp"
#include "SendBuffer.hpp"
#include "IdPool.hpp"
#include "ObjectPool.hpp"
#include "object.hpp"

// temporary --------------------------------
#include "IdPool.hpp"
#include "Room.hpp"
#include "RoomManager.hpp"
static std::atomic_int32_t totalSessions{0};
static const int32 maxRoomSessions = 4;
// ------------------------------------------

GameSession::~GameSession() {
	std::cout << "GameSession destroyed. ID: " << id() << '\n';
	IdPool::push(id());
	ObjectPool<Object>::push(myPlayer_);
	myPlayer_ = nullptr;
}

void GameSession::onConnected() {
	if (totalSessions.load() % maxRoomSessions == 0) {
		myRoom_ = RoomManager::makeRoom();
	}
	else {
		myRoom_ = RoomManager::getRoom(RoomIdPool::currId());
	}
	++totalSessions;

	myPlayer_ = ObjectPool<Object>::pop();
	myPlayer_->setId(id());

	myRoom_->doAsync([this]() {
		myRoom_->enter(this);
	});
}

void GameSession::onDisconnected() {
	--totalSessions;
	
	myRoom_->doAsync([this]() {
		myRoom_->leave(this);
	});
}

void GameSession::processPacket(byte* buffer, int32 len) {
	PacketManager::handlePacket(this, buffer, len);
}

void GameSession::onSend(int32 len) {

}

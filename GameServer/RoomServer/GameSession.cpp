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
	ObjectPool<Player>::push(myPlayer_);
	myPlayer_ = nullptr;
}

void GameSession::onConnected() {
	if (totalSessions.load() % maxRoomSessions == 0) {
		myRoom_ = RoomManager::makeRoom();
	}
	else {
		myRoom_ = RoomManager::findRoom(RoomIdPool::currId());
	}
	++totalSessions;

	myPlayer_ = ObjectPool<Player>::pop();
	myPlayer_->setId(id());

	// 비동기 잡이 실행될 때까지 세션이 살아 있어야 하므로 shared_ptr(self)를 캡처한다.
	// (shared_ptr 도입 후 raw this 캡처는 잡 실행 전 세션이 반환되면 use-after-free가 된다.)
	auto self = std::static_pointer_cast<GameSession>(shared_from_this());
	myRoom_->doAsync([self]() {
		self->myRoom_->enter(self.get());
	});
}

void GameSession::onDisconnected() {
	--totalSessions;

	auto self = std::static_pointer_cast<GameSession>(shared_from_this());
	myRoom_->doAsync([self]() {
		self->myRoom_->leave(self.get());
	});
}

void GameSession::processPacket(byte* buffer, int32 len) {
	PacketManager::handlePacket(this, buffer, len);
}

void GameSession::onSend(int32 len) {

}

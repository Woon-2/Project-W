#include "rspch.hpp"
#include "GameSession.hpp"
#include "PacketManager.hpp"
#include "SendBuffer.hpp"
#include "IdPool.hpp"
#include "ObjectPool.hpp"
#include "object.hpp"

#include "IdPool.hpp"
#include "Room.hpp"
#include "RoomManager.hpp"
#include "AccountRegistry.hpp"

GameSession::~GameSession() {
	std::cout << "GameSession destroyed. ID: " << id() << '\n';
	IdPool::push(id());
	ObjectPool<Player>::push(myPlayer_);
	myPlayer_ = nullptr;
}

void GameSession::onConnected() {
	// 방 배정은 접속만으로 하지 않는다. 클라가 C_Enter(lobbyCode)를 보내면 enterRoom에서 코드 기반으로 배정.
	myPlayer_ = ObjectPool<Player>::pop();
	myPlayer_->setId(id());
}

void GameSession::setAccount(int64 accountId, const wchar_t* nickname) {
	accountId_ = accountId;
	wcsncpy_s(nickname_, nickname, _TRUNCATE);

	// 계정 필드를 다 쓴 "뒤에" release로 세운다. 잡 스레드가 acquire로 true를 보면
	// 위 두 쓰기가 이미 보인다는 뜻이다.
	entryAuthorized_.store(true, std::memory_order_release);
}

void GameSession::enterRoom(const std::string& lobbyCode) {
	myRoom_ = RoomManager::findOrCreateRoomByCode(lobbyCode);

	// 비동기 잡이 실행될 때까지 세션이 살아 있어야 하므로 shared_ptr(self)를 캡처한다.
	auto self = std::static_pointer_cast<GameSession>(shared_from_this());
	myRoom_->doAsync([self]() {
		self->myRoom_->enter(self.get());
	});
}

void GameSession::onDisconnected() {
	// 방 배정 여부와 무관하게 계정 바인딩부터 푼다. 티켓 검증은 통과했지만 방에 못 들어간
	// 경우(중복 C_Enter, 코드 이상 등)에도 계정이 영원히 잠기면 안 된다.
	if (entryAuthorized_.load(std::memory_order_acquire)) {
		AccountRegistry::unbind(accountId_);
	}

	// C_Enter 전에 끊기면 myRoom_가 없을 수 있다.
	if (!myRoom_) {
		return;
	}

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

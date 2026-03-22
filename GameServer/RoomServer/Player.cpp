#include "rspch.hpp"
#include "Player.hpp"
#include "GameSession.hpp"
#include "SendBuffer.hpp"

int32 Player::id() const {
	return ownerSession_->id();
}

void Player::send(SendBuffer* sendBuffer) {
	ownerSession_->send(sendBuffer);
}

#include "lspch.hpp"
#include "LobbyRoom.hpp"
#include "LobbyManager.hpp"
#include "GameSession.hpp"
#include "SendBuffer.hpp"
#include "PacketManager.hpp"

bool LobbyRoom::enter( const std::shared_ptr<GameSession>& session ) {
	std::lock_guard lock( mutex_ );

	// 비어서 삭제 예정인 방엔 입장 불가(findRoom↔removeRoom 경쟁 차단). 클라는 "방 없음"으로 처리.
	if ( closed_ ) {
		return false;
	}

	if ( static_cast<int32>(players_.size()) >= maxPlayers ) {
		return false;
	}

	auto info = LobbyPlayerInfo{ .sessionId = static_cast<uint16>(session->id()) };
	auto pkt = PacketManager::makeSLobbyRoomPlayerJoinedPacket( info );

	for ( const auto& p : players_ ) {
		p->send( pkt );
	}

	players_.push_back( session );

	if ( players_.size() == 1 ) {
		hostId_ = static_cast<uint16>(session->id());
	}

	return true;
}

void LobbyRoom::leave( GameSession* session ) {
	bool isEmpty = false;
	std::string codeSnapshot;

	{
		std::lock_guard lock( mutex_ );

		auto it = std::find_if( players_.begin(), players_.end(),
			[session]( const std::shared_ptr<GameSession>& p ) { return p.get() == session; } );
		if ( it == players_.end() ) {
			return;
		}

		players_.erase( it );

		if ( players_.empty() ) {
			isEmpty = true;
			codeSnapshot = code_;
			closed_ = true;   // 락 안에서 표시 → 이후 enter는 거부됨(삭제 경쟁 차단).
		}
		else {
			if ( hostId_ == static_cast<uint16>(session->id()) ) {
				hostId_ = static_cast<uint16>(players_.front()->id());
			}

			broadcast( PacketManager::makeSLobbyRoomPlayerLeftPacket( static_cast<uint16>(session->id()) ) );
		}
	}

	if ( isEmpty ) {
		LobbyManager::removeRoom( codeSnapshot );
	}
}

void LobbyRoom::startGame() {
	std::lock_guard lock( mutex_ );
	broadcast( PacketManager::makeSGameStartPacket( serverIp, roomServerPort, code_ ) );
}

uint16 LobbyRoom::hostId() const {
	std::lock_guard lock( mutex_ );
	return hostId_;
}

std::vector<uint16> LobbyRoom::playerIds() const {
	std::lock_guard lock( mutex_ );

	std::vector<uint16> ids;
	ids.reserve( players_.size() );

	for ( const auto& p : players_ ) {
		ids.push_back( static_cast<uint16>(p->id()) );
	}
	return ids;
}

std::vector<LobbyPlayerInfo> LobbyRoom::playerInfos() const {
	std::lock_guard lock( mutex_ );

	std::vector<LobbyPlayerInfo> infos;
	infos.reserve( players_.size() );

	for ( const auto& p : players_ ) {
		infos.push_back( LobbyPlayerInfo{ static_cast<uint16>(p->id()) } );
	}
	return infos;
}

bool LobbyRoom::isFull() const {
	std::lock_guard lock( mutex_ );
	return static_cast<int32>(players_.size()) >= maxPlayers;
}

int32 LobbyRoom::playerCnt() const {
	std::lock_guard lock( mutex_ );
	return static_cast<int32>(players_.size());
}

void LobbyRoom::broadcast( const std::shared_ptr<SendBuffer>& buf ) {
	for ( const auto& p : players_ ) {
		p->send( buf );
	}
}

#include "lspch.hpp"
#include "LobbyRoom.hpp"
#include "LobbyManager.hpp"
#include "GameSession.hpp"
#include "SendBuffer.hpp"
#include "PacketManager.hpp"
#include "EntryTicket.hpp"

bool LobbyRoom::enter( const std::shared_ptr<GameSession>& session ) {
	std::lock_guard lock( mutex_ );

	// 비어서 삭제 예정인 방엔 입장 불가(findRoom↔removeRoom 경쟁 차단). 클라는 "방 없음"으로 처리.
	if ( closed_ ) {
		return false;
	}

	if ( static_cast<int32>(players_.size()) >= maxPlayers ) {
		return false;
	}

	auto info = LobbyPlayerInfo{
		.sessionId = static_cast<uint16>(session->id()),
		.weaponType = session->selectedWeaponType_,
	};
	wcsncpy_s( info.nickname, session->nickname_, _TRUNCATE );
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

void LobbyRoom::selectWeapon( GameSession* session, PlayerWeaponType weaponType ) {
	std::lock_guard lock( mutex_ );

	auto it = std::find_if( players_.begin(), players_.end(),
		[session]( const std::shared_ptr<GameSession>& p ) { return p.get() == session; } );
	if ( it == players_.end() ) {
		return;
	}

	session->selectedWeaponType_ = weaponType;
	broadcast( PacketManager::makeSLobbyWeaponSelectedPacket(
		static_cast<uint16>(session->id()), weaponType ) );
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

	// 티켓이 플레이어마다 다르므로 broadcast()를 쓸 수 없다. 한 명씩 발급해 각자에게 보낸다.
	// 발급은 CNG 해시 한 번(수십 µs)이고 방 정원이 4명이라, 락 안에서 돌아도 무방하다.
	for ( const auto& p : players_ ) {
		// 인증 게이트(PacketManager.cpp 인증 게이트) 덕에 정상 경로에선 항상 참이지만,
		// 티켓에 실리는 계정 정보의 유일한 근거이므로 여기서 한 번 더 확인한다.
		if ( !p->authenticated_.load( std::memory_order_acquire ) ) {
			std::cout << "[GameStart] 미인증 세션 건너뜀. id: " << p->id() << '\n';
			continue;
		}

		EntryTicket ticket{};
		if ( !EntryTicketAuthority::mint(
			p->accountId_.load( std::memory_order_relaxed ), p->nickname_, code_, ticket ) ) {
			std::cout << "[GameStart] 티켓 발급 실패. id: " << p->id() << '\n';
			continue;
		}

		p->send( PacketManager::makeSGameStartPacket(
			roomServerEndpoint_.ip, roomServerEndpoint_.port, code_, ticket ) );
	}
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
		auto info = LobbyPlayerInfo{
			.sessionId = static_cast<uint16>(p->id()),
			.weaponType = p->selectedWeaponType_,
		};
		wcsncpy_s( info.nickname, p->nickname_, _TRUNCATE );
		infos.push_back( info );
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

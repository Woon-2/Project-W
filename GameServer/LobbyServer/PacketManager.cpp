#include "lspch.hpp"
#include "PacketManager.hpp"
#include "SendBuffer.hpp"
#include "BufferWriter.hpp"
#include "GameSession.hpp"
#include "GameSessionManager.hpp"
#include "LobbyRoom.hpp"
#include "LobbyManager.hpp"
#include "DBExecutor.hpp"
#include "DBBinder.hpp"
#include "PasswordHash.hpp"

namespace {

	// loginId는 ASCII로 검증된 입력이므로 단순 확장으로 NVARCHAR 파라미터에 맞춘다.
	std::wstring toWide( const std::string& s ) {
		return std::wstring( s.begin(), s.end() );
	}

	// --- DB 스레드에서 실행되는 계정 쿼리 ---
	// DBBinder는 포인터만 저장하므로 바인딩 변수들은 execute()/fetch()까지 이 함수 스코프에 살아 있다.

	AccountResult registerAccount( const std::string& loginId, const std::string& password, const std::wstring& nickname ) {
		DBConnectionPoolGuard guard( DBExecutor::pool() );
		if ( !guard ) {
			return AccountResult::DbError;
		}

		const std::wstring wLoginId = toWide( loginId );

		// 중복 선확인 — ID 먼저, 그다음 닉네임 (둘 다 중복이면 DuplicateId 우선 규약).
		// 동시 가입 레이스는 UNIQUE 제약이 최후 방어한다
		// (그 경우 INSERT가 실패해 DbError로 응답하지만, 시연 규모에서는 사실상 발생하지 않는다).
		{
			int32 exists = 0;
			DBBinder<1, 1> binder( *guard.get(), L"SELECT 1 FROM dbo.Account WHERE loginId = ?" );
			binder.bindParam( 0, wLoginId.c_str() );
			binder.bindColumn( 0, exists );
			if ( !binder.execute() ) {
				return AccountResult::DbError;
			}
			if ( binder.fetch() ) {
				return AccountResult::DuplicateId;
			}
		}

		{
			int32 exists = 0;
			DBBinder<1, 1> binder( *guard.get(), L"SELECT 1 FROM dbo.Account WHERE nickname = ?" );
			binder.bindParam( 0, nickname.c_str() );
			binder.bindColumn( 0, exists );
			if ( !binder.execute() ) {
				return AccountResult::DbError;
			}
			if ( binder.fetch() ) {
				return AccountResult::DuplicateNickname;
			}
		}

		byte salt[ PasswordHash::kSaltSize ]{};
		byte pwHash[ PasswordHash::kHashSize ]{};
		if ( !PasswordHash::generateSalt( salt )
			|| !PasswordHash::hash( password.c_str(), salt, PasswordHash::kSaltSize, pwHash ) ) {
			return AccountResult::DbError;
		}

		{
			DBBinder<4, 0> binder( *guard.get(),
				L"INSERT INTO dbo.Account (loginId, pwHash, pwSalt, nickname) VALUES (?, ?, ?, ?)" );
			binder.bindParam( 0, wLoginId.c_str() );
			binder.bindParam( 1, pwHash );
			binder.bindParam( 2, salt );
			binder.bindParam( 3, nickname.c_str() );
			if ( !binder.execute() ) {
				return AccountResult::DbError;
			}
		}

		return AccountResult::Ok;
	}

	AccountResult loginAccount( const std::string& loginId, const std::string& password,
		int64& outAccountId, std::wstring& outNickname ) {
		DBConnectionPoolGuard guard( DBExecutor::pool() );
		if ( !guard ) {
			return AccountResult::DbError;
		}

		const std::wstring wLoginId = toWide( loginId );

		int64 accountId = 0;
		byte pwHash[ PasswordHash::kHashSize ]{};
		byte pwSalt[ PasswordHash::kSaltSize ]{};
		wchar_t nickname[ kNicknameMax ]{};

		DBBinder<1, 4> binder( *guard.get(),
			L"SELECT accountId, pwHash, pwSalt, nickname FROM dbo.Account WHERE loginId = ?" );
		binder.bindParam( 0, wLoginId.c_str() );
		binder.bindColumn( 0, accountId );
		binder.bindColumn( 1, pwHash );
		binder.bindColumn( 2, pwSalt );
		binder.bindColumn( 3, nickname );
		if ( !binder.execute() ) {
			return AccountResult::DbError;
		}
		if ( !binder.fetch() ) {
			return AccountResult::NoSuchAccount;
		}

		if ( !PasswordHash::verify( password.c_str(), pwSalt, PasswordHash::kSaltSize, pwHash ) ) {
			return AccountResult::WrongPassword;
		}

		outAccountId = accountId;
		outNickname.assign( nickname, wcsnlen_s( nickname, kNicknameMax ) );
		return AccountResult::Ok;
	}

}

void PacketManager::handlePacket( GameSession* session, byte* buffer, int32 len ) {
	auto header = reinterpret_cast<PacketHeader*>(buffer);

	// 인증 게이트: 로그인 성공 전에는 계정 패킷만 처리한다.
	const bool isAccountPacket =
		header->type == PacketType::C_Register || header->type == PacketType::C_Login;
	if ( !isAccountPacket && !session->authenticated_.load( std::memory_order_acquire ) ) {
		std::cout << "Unauthenticated packet dropped. type: " << static_cast<uint16>(header->type)
			<< " session: " << session->id() << '\n';
		return;
	}

	switch ( header->type ) {
	case PacketType::C_Register:
		handleCRegisterPacket( session, buffer, len );
		break;

	case PacketType::C_Login:
		handleCLoginPacket( session, buffer, len );
		break;

	case PacketType::C_CreateRoom:
		handleCCreateRoomPacket( session, buffer, len );
		break;

	case PacketType::C_JoinRoom:
		handleCJoinRoomPacket( session, buffer, len );
		break;

	case PacketType::C_LeaveRoom:
		handleCLeaveRoomPacket( session, buffer, len );
		break;

	case PacketType::C_SelectWeapon:
		handleCSelectWeaponPacket( session, buffer, len );
		break;

	case PacketType::C_GameStart:
		handleCGameStartPacket( session, buffer, len );
		break;

	default:
		std::cout << "Unknown packet type. type: " << static_cast<uint16>(header->type) << '\n';
		CRASH( "Unknown packet type" );
		break;
	}
}

void PacketManager::handleCRegisterPacket( GameSession* session, byte* buffer, int32 len ) {
	auto pkt = reinterpret_cast<CRegisterPacket*>(buffer);

	const size_t idLen = strnlen_s( pkt->loginId, kLoginIdMax );
	const size_t pwLen = strnlen_s( pkt->password, kPasswordMax );
	const size_t nickLen = wcsnlen_s( pkt->nickname, kNicknameMax );

	// 널 종료가 없거나(== Max) 빈 값이면 형식 오류.
	if ( idLen == 0 || idLen >= kLoginIdMax
		|| pwLen == 0 || pwLen >= kPasswordMax
		|| nickLen == 0 || nickLen >= kNicknameMax ) {
		session->send( makeSRegisterPacket( AccountResult::InvalidInput ) );
		return;
	}

	// DB 잡이 끝날 때까지 세션이 살아 있도록 shared_ptr로 잡고, 입력은 값으로 복사한다.
	auto self = std::static_pointer_cast<GameSession>( session->shared_from_this() );

	DBExecutor::post(
		[self,
		 loginId = std::string( pkt->loginId, idLen ),
		 password = std::string( pkt->password, pwLen ),
		 nickname = std::wstring( pkt->nickname, nickLen )]() {
			self->send( makeSRegisterPacket( registerAccount( loginId, password, nickname ) ) );
		} );
}

void PacketManager::handleCLoginPacket( GameSession* session, byte* buffer, int32 len ) {
	auto pkt = reinterpret_cast<CLoginPacket*>(buffer);

	const size_t idLen = strnlen_s( pkt->loginId, kLoginIdMax );
	const size_t pwLen = strnlen_s( pkt->password, kPasswordMax );

	if ( idLen == 0 || idLen >= kLoginIdMax || pwLen == 0 || pwLen >= kPasswordMax ) {
		session->send( makeSLoginPacket( AccountResult::InvalidInput, 0, L"" ) );
		return;
	}

	if ( session->authenticated_.load( std::memory_order_acquire ) ) {
		session->send( makeSLoginPacket( AccountResult::AlreadyLoggedIn, 0, L"" ) );
		return;
	}

	auto self = std::static_pointer_cast<GameSession>( session->shared_from_this() );

	DBExecutor::post(
		[self,
		 loginId = std::string( pkt->loginId, idLen ),
		 password = std::string( pkt->password, pwLen )]() {
			int64 accountId = 0;
			std::wstring nickname;
			AccountResult result = loginAccount( loginId, password, accountId, nickname );

			// 계정당 세션 하나. 실패하면 다른 세션이 이미 이 계정으로 로그인 중이다.
			if ( result == AccountResult::Ok
				&& !GameSessionManager::bindAccount( accountId, static_cast<uint16>( self->id() ) ) ) {
				result = AccountResult::AlreadyLoggedIn;
			}

			if ( result != AccountResult::Ok ) {
				self->send( makeSLoginPacket( result, 0, L"" ) );
				return;
			}

			// nickname_은 authenticated_를 release로 세우기 "전"에 써야
			// acquire 로드로 true를 본 스레드가 완성된 nickname_을 읽는다.
			wcscpy_s( self->nickname_, nickname.c_str() );
			self->accountId_.store( accountId );
			self->authenticated_.store( true, std::memory_order_release );

			// 잡 실행 중에 이미 끊겼다면 onDisconnected가 authenticated_=false 시점을 지나쳐
			// 계정이 영영 잠길 수 있으므로 여기서 직접 회수한다 (이중 해제는 무해).
			if ( !self->isConnected() ) {
				GameSessionManager::unbindAccount( accountId );
				return;
			}

			self->send( makeSLoginPacket( AccountResult::Ok, accountId, self->nickname_ ) );
		} );
}

void PacketManager::handleCCreateRoomPacket( GameSession* session, byte* buffer, int32 len ) {
	if ( session->myRoom_ ) {
		return;
	}

	auto room = LobbyManager::createRoom();
	room->enter( std::static_pointer_cast<GameSession>( session->shared_from_this() ) );
	session->myRoom_ = room.get();
	session->send( makeSCreateRoomPacket( static_cast<uint16>(session->id()), room->code() ) );
}

void PacketManager::handleCJoinRoomPacket( GameSession* session, byte* buffer, int32 len ) {
	if ( session->myRoom_ ) {
		return;
	}

	auto pkt = reinterpret_cast<CJoinRoomPacket*>(buffer);
	std::string code( pkt->code, strnlen_s( pkt->code, sizeof( pkt->code ) ) );

	const uint16 myId = static_cast<uint16>( session->id() );

	auto room = LobbyManager::findRoom( code );
	if ( !room ) {
		session->send( makeSJoinRoomPacket( false, myId, 0, "", {} ) );
		return;
	}

	if ( !room->enter( std::static_pointer_cast<GameSession>( session->shared_from_this() ) ) ) {
		session->send( makeSJoinRoomPacket( false, myId, 0, "", {} ) );
		return;
	}

	session->myRoom_ = room.get();
	session->send( makeSJoinRoomPacket( true, myId, room->hostId(), room->code(), room->playerInfos() ) );
}

void PacketManager::handleCLeaveRoomPacket( GameSession* session, byte* buffer, int32 len ) {
	if ( !session->myRoom_ ) {
		return;
	}

	session->myRoom_->leave( session );
	session->myRoom_ = nullptr;
}

void PacketManager::handleCSelectWeaponPacket( GameSession* session, byte* buffer, int32 len ) {
	auto pkt = reinterpret_cast<CSelectWeaponPacket*>(buffer);
	const auto ordinal = static_cast<uint8>(pkt->weaponType);
	if ( ordinal > static_cast<uint8>(PlayerWeaponType::HeavyArrow) ) {
		return;
	}

	session->selectedWeaponType_ = pkt->weaponType;
	if ( session->myRoom_ ) {
		session->myRoom_->selectWeapon( session, pkt->weaponType );
	}
}

void PacketManager::handleCGameStartPacket( GameSession* session, byte* buffer, int32 len ) {
	if ( !session->myRoom_ ) {
		return;
	}

	if ( static_cast<uint16>(session->id()) != session->myRoom_->hostId() ) {
		return;
	}

	session->myRoom_->startGame();
}

std::shared_ptr<SendBuffer> PacketManager::makeSRegisterPacket( AccountResult result ) {
	auto sendBuffer = SendBufferManager::open( sizeof( SRegisterPacket ) );
	auto bw = BufferWriter( sendBuffer->data(), sendBuffer->allocSize() );

	auto pkt = bw.reserve<SRegisterPacket>();
	pkt->result = result;

	pkt->size = bw.writeSize();
	pkt->type = PacketType::S_Register;

	sendBuffer->close( bw.writeSize() );
	return sendBuffer;
}

std::shared_ptr<SendBuffer> PacketManager::makeSLoginPacket( AccountResult result, int64 accountId, const wchar_t* nickname ) {
	auto sendBuffer = SendBufferManager::open( sizeof( SLoginPacket ) );
	auto bw = BufferWriter( sendBuffer->data(), sendBuffer->allocSize() );

	auto pkt = bw.reserve<SLoginPacket>();
	pkt->result = result;
	pkt->accountId = accountId;
	wcscpy_s( pkt->nickname, nickname );

	pkt->size = bw.writeSize();
	pkt->type = PacketType::S_Login;

	sendBuffer->close( bw.writeSize() );
	return sendBuffer;
}

std::shared_ptr<SendBuffer> PacketManager::makeSCreateRoomPacket( uint16 myId, const std::string& code ) {
	auto sendBuffer = SendBufferManager::open( sizeof( SCreateRoomPacket ) );
	auto bw = BufferWriter( sendBuffer->data(), sendBuffer->allocSize() );

	auto pkt = bw.reserve<SCreateRoomPacket>();
	pkt->myId = myId;
	strncpy_s( pkt->code, code.data(), _TRUNCATE );

	pkt->size = bw.writeSize();
	pkt->type = PacketType::S_CreateRoom;

	sendBuffer->close( bw.writeSize() );
	return sendBuffer;
}

std::shared_ptr<SendBuffer> PacketManager::makeSJoinRoomPacket( bool success, uint16 myId, uint16 hostId, const std::string& code, const std::vector<LobbyPlayerInfo>& playerInfos ) {
	const uint8 cnt = static_cast<uint8>(playerInfos.size());
	auto sendBuffer = SendBufferManager::open( sizeof( SJoinRoomPacket ) + sizeof( LobbyPlayerInfo ) * cnt );
	auto bw = BufferWriter( sendBuffer->data(), sendBuffer->allocSize() );

	auto pkt = bw.reserve<SJoinRoomPacket>();
	pkt->success = success;
	pkt->myId = myId;
	pkt->playerCnt = cnt;
	pkt->hostId = hostId;
	if ( !code.empty() ) {
		strncpy_s( pkt->code, code.data(), _TRUNCATE );
	}
	else {
		pkt->code[ 0 ] = '\0';
	}

	auto infos = bw.reserve<LobbyPlayerInfo>( cnt );
	for ( uint16 i = 0; i < cnt; ++i ) {
		infos[ i ] = playerInfos[ i ];
	}

	pkt->playersOffset = static_cast<uint16>( reinterpret_cast<uint64>( infos ) - reinterpret_cast<uint64>( pkt ) );
	pkt->size = bw.writeSize();
	pkt->type = PacketType::S_JoinRoom;

	sendBuffer->close( bw.writeSize() );
	return sendBuffer;
}

std::shared_ptr<SendBuffer> PacketManager::makeSLobbyRoomPlayerJoinedPacket( const LobbyPlayerInfo& info ) {
	auto sendBuffer = SendBufferManager::open( sizeof( SLobbyRoomPlayerJoinedPacket ) );
	auto bw = BufferWriter( sendBuffer->data(), sendBuffer->allocSize() );

	auto pkt = bw.reserve<SLobbyRoomPlayerJoinedPacket>();
	pkt->info = info;

	pkt->size = bw.writeSize();
	pkt->type = PacketType::S_LobbyRoomPlayerJoined;

	sendBuffer->close( bw.writeSize() );
	return sendBuffer;
}

std::shared_ptr<SendBuffer> PacketManager::makeSLobbyRoomPlayerLeftPacket( uint16 sessionId ) {
	auto sendBuffer = SendBufferManager::open( sizeof( SLobbyRoomPlayerLeftPacket ) );
	auto bw = BufferWriter( sendBuffer->data(), sendBuffer->allocSize() );

	auto pkt = bw.reserve<SLobbyRoomPlayerLeftPacket>();
	pkt->sessionId = sessionId;

	pkt->size = bw.writeSize();
	pkt->type = PacketType::S_LobbyRoomPlayerLeft;

	sendBuffer->close( bw.writeSize() );
	return sendBuffer;
}

std::shared_ptr<SendBuffer> PacketManager::makeSLobbyWeaponSelectedPacket( uint16 sessionId, PlayerWeaponType weaponType ) {
	auto sendBuffer = SendBufferManager::open( sizeof( SLobbyWeaponSelectedPacket ) );
	auto bw = BufferWriter( sendBuffer->data(), sendBuffer->allocSize() );

	auto pkt = bw.reserve<SLobbyWeaponSelectedPacket>();
	pkt->sessionId = sessionId;
	pkt->weaponType = weaponType;

	pkt->size = bw.writeSize();
	pkt->type = PacketType::S_LobbyWeaponSelected;

	sendBuffer->close( bw.writeSize() );
	return sendBuffer;
}

std::shared_ptr<SendBuffer> PacketManager::makeSGameStartPacket( const std::string& roomServerIp, uint16 port, const std::string& lobbyCode ) {
	auto sendBuffer = SendBufferManager::open( sizeof( SGameStartPacket ) );
	auto bw = BufferWriter( sendBuffer->data(), sendBuffer->allocSize() );

	auto pkt = bw.reserve<SGameStartPacket>();
	strncpy_s( pkt->roomServerIp, roomServerIp.data(), _TRUNCATE );
	pkt->roomServerPort = port;
	strncpy_s( pkt->lobbyCode, lobbyCode.data(), _TRUNCATE );

	pkt->size = bw.writeSize();
	pkt->type = PacketType::S_GameStart;

	sendBuffer->close( bw.writeSize() );
	return sendBuffer;
}

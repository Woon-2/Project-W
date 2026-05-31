#include "lspch.hpp"
#include "PacketManager.hpp"
#include "SendBuffer.hpp"
#include "BufferWriter.hpp"
#include "GameSession.hpp"
#include "LobbyRoom.hpp"
#include "LobbyManager.hpp"

void PacketManager::handlePacket(GameSession* session, byte* buffer, int32 len) {
	auto header = reinterpret_cast<PacketHeader*>(buffer);

	switch (header->type) {
	case PacketType::C_CreateRoom:
		handleCCreateRoomPacket(session, buffer, len);
		break;

	case PacketType::C_JoinRoom:
		handleCJoinRoomPacket(session, buffer, len);
		break;

	case PacketType::C_LeaveRoom:
		handleCLeaveRoomPacket(session, buffer, len);
		break;

	case PacketType::C_GameStart:
		handleCGameStartPacket(session, buffer, len);
		break;

	default:
		std::cout << "Unknown packet type. type: " << static_cast<uint16>(header->type) << '\n';
		CRASH( "Unknown packet type" );
		break;
	}
}

void PacketManager::handleCCreateRoomPacket( GameSession* session, byte* buffer, int32 len ) {
	if ( session->myRoom_ ) {
		return;
	}

	auto* room = LobbyManager::createRoom();
	room->enter( session );
	session->myRoom_ = room;
	session->send( makeSCreateRoomPacket( room->code() ) );
}

void PacketManager::handleCJoinRoomPacket( GameSession* session, byte* buffer, int32 len ) {
	if ( session->myRoom_ ) {
		return;
	}

	auto* pkt = reinterpret_cast<CJoinRoomPacket*>(buffer);
	std::string_view code( pkt->code, strnlen_s( pkt->code, sizeof( pkt->code ) ) );

	auto* room = LobbyManager::findRoom( code );
	if ( !room ) {
		session->send( makeSJoinRoomPacket( false, 0, "", {} ) );
		return;
	}

	if ( !room->enter( session ) ) {
		session->send( makeSJoinRoomPacket( false, 0, "", {} ) );
		return;
	}

	session->myRoom_ = room;
	session->send( makeSJoinRoomPacket( true, room->hostId(), room->code(), room->playerInfos() ) );
}

void PacketManager::handleCLeaveRoomPacket( GameSession* session, byte* buffer, int32 len ) {
	if ( !session->myRoom_ ) {
		return;
	}

	session->myRoom_->leave( session );
	session->myRoom_ = nullptr;
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

std::shared_ptr<SendBuffer> PacketManager::makeSCreateRoomPacket( std::string_view code ) {
	auto sendBuffer = SendBufferManager::open( sizeof( SCreateRoomPacket ) );
	auto bw = BufferWriter( sendBuffer->data(), sendBuffer->allocSize() );

	auto pkt = bw.reserve<SCreateRoomPacket>();
	strncpy_s( pkt->code, code.data(), _TRUNCATE );

	pkt->size = bw.writeSize();
	pkt->type = PacketType::S_CreateRoom;

	sendBuffer->close( bw.writeSize() );
	return sendBuffer;
}

std::shared_ptr<SendBuffer> PacketManager::makeSJoinRoomPacket( bool success, uint16 hostId, std::string_view code, const std::vector<LobbyPlayerInfo>& playerInfos ) {
	const uint16 cnt = static_cast<uint16>(playerInfos.size());
	auto sendBuffer = SendBufferManager::open( sizeof( SJoinRoomPacket ) + sizeof( LobbyPlayerInfo ) * cnt );
	auto bw = BufferWriter( sendBuffer->data(), sendBuffer->allocSize() );

	auto pkt = bw.reserve<SJoinRoomPacket>();
	pkt->success = success;
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

std::shared_ptr<SendBuffer> PacketManager::makeSGameStartPacket( std::string_view roomServerIp, uint16 port, std::string_view lobbyCode ) {
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

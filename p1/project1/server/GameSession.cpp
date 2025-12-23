#include "pch.hpp"
#include "SendBuffer.hpp"
#include "GameSessionManager.hpp"
#include "Service.hpp"
#include "GameSession.hpp"
#include "Room.hpp"
#include "RoomManager.hpp"
#include "GameLogic.hpp"
#include "GameLogicManager.hpp"

void GameSession::onConnected( ) {
	std::cout << "GameSession " << getId( ) << " connected.\n";
	GameSessionManager::add( std::static_pointer_cast<GameSession>( shared_from_this( ) ) );

	auto packet = Packet{
		.header = {
			.size = sizeof( PacketHeader ) + sizeof( SCAssignIdPacket ),
			.id = static_cast<uint16>( PacketType::scAssignId )
		},
		.scAssignId = {
			.playerId = getId( )
		}
	};

	int32 packetSize = sizeof( Packet );
	auto sendBuffer = std::make_shared<SendBuffer>( packetSize );
	sendBuffer->copyData( &packet, packetSize );
	send( sendBuffer );
}

void GameSession::onDisconnected( ) {
	if (myRoomId_ != -1) {
		auto logicMsg = LogicMessage{
			.type = LogicMsgType::UserLeave,
			.userId = getId(),
			.roomId = myRoomId_
		};
		GameLogicManager::dispatchMessage(logicMsg);
	}

	GameSessionManager::remove(std::static_pointer_cast<GameSession>(shared_from_this()));
}

int32 GameSession::onRecvPacket( uint8* buffer, int32 len ) {
	//std::cout << "GameSession " << getId( ) << " received packet of length " << len << ".\n";

	auto packet = reinterpret_cast<Packet*>( buffer );
	
	switch ( static_cast<PacketType>( packet->header.id ) ) {
	case PacketType::csSignup: {
		auto id = std::string( packet->csSignup.id.data( ) );
		auto pw = std::string( packet->csSignup.pw.data( ) );
		std::string err;

		bool isOk = signupUser( id, pw, err );

		auto sendPacket = Packet{
			.header = {
				.size = sizeof( PacketHeader ) + sizeof( SCSignupPacket ),
				.id = static_cast<uint16>( PacketType::scSignup )
			},
			.scSignup = {
				.isOk = isOk
			}
		};

		if ( !isOk ) {
			ASSERT_CRASH( err.size( ) < sendPacket.scSignup.reason.size( ) );
			::memcpy( sendPacket.scSignup.reason.data( ), err.c_str( ), err.size( ) );
		}

		int32 packetSize = sizeof( Packet );
		auto sendBuffer = std::make_shared<SendBuffer>( packetSize );
		sendBuffer->copyData( &sendPacket, packetSize );
		send( sendBuffer );
		break;
	}

	case PacketType::csLogin: {
		auto id = std::string( packet->csLogin.id.data( ) );
		auto pw = std::string( packet->csLogin.pw.data( ) );
		std::string err;

		bool isOk = loginUser( id, pw, err );

		auto sendPacket = Packet{
			.header = {
				.size = sizeof( PacketHeader ) + sizeof( SCLoginPacket ),
				.id = static_cast<uint16>( PacketType::scLogin )
			},
			.scLogin = {
				.isOk = isOk
			}
		};

		if( !isOk ) {
			ASSERT_CRASH( err.size( ) < sendPacket.scLogin.reason.size( ) );
			::memcpy( sendPacket.scLogin.reason.data( ), err.c_str( ), err.size( ) );
		}

		int32 packetSize = sizeof( Packet );
		auto sendBuffer = std::make_shared<SendBuffer>( packetSize );
		sendBuffer->copyData( &sendPacket, packetSize );
		send( sendBuffer );
		break;
	}

	case PacketType::csMouseMove: {
		if(myRoomId_ == -1) {
			break;
		}

		auto logicMsg = LogicMessage{
			.type = LogicMsgType::UserMouseMove,
			.userId = getId(),
			.roomId = myRoomId_,
			.playerYawRadian = packet->csMouseMove.playerYawRadian,
			.cameraPitchRadian = packet->csMouseMove.cameraPitchRadian
		};

		GameLogicManager::dispatchMessage(logicMsg);
		break;
	}

	case PacketType::csMoveState: {
		if(myRoomId_ == -1) {
			break;
		}

		auto logicMsg = LogicMessage{
			.type = LogicMsgType::UserMoveState,
			.userId = getId(),
			.roomId = myRoomId_,
			.position = packet->csMoveState.position,
			.velocity = packet->csMoveState.velocity,
			.forward = packet->csMoveState.forward,
			.timeStamp = packet->csMoveState.timeStamp
		};

		GameLogicManager::dispatchMessage(logicMsg);
		break;
	}

	case PacketType::csFindRoom: {
		if( myRoomId_ != -1 ) {
			break;
		}

		auto room = RoomManager::findRoom( packet->csFindRoom.roomId );
		if ( !room ) {
			room = RoomManager::createRoom( packet->csFindRoom.roomId );
			ASSERT_CRASH(room);
		}

		myRoomId_ = room->getRoomId();

		auto addRoomMsg = LogicMessage{
			.type = LogicMsgType::AddRoom,
			.roomId = myRoomId_,
		};
		GameLogicManager::dispatchMessage(addRoomMsg);

		auto enterRoomMsg = LogicMessage{
			.type = LogicMsgType::UserEnter,
			.userId = getId(),
			.roomId = myRoomId_
		};
		GameLogicManager::dispatchMessage(enterRoomMsg);
		break;
	}

	case PacketType::csFire: {
		if(myRoomId_ == -1) {
			break;
		}

		auto logicMsg = LogicMessage{
			.type = LogicMsgType::UserFire,
			.userId = getId(),
			.roomId = myRoomId_,
			.position = packet->csFire.firePos,
			.fireDir = packet->csFire.fireDir,
			.timeStamp = packet->csFire.timeStamp
		};
		GameLogicManager::dispatchMessage(logicMsg);
		break;
	}

	case PacketType::csReload: {
		if(myRoomId_ == -1) {
			break;
		}

		auto logicMsg = LogicMessage{
			.type = LogicMsgType::UserReload,
			.userId = getId(),
			.roomId = myRoomId_
		};
		GameLogicManager::dispatchMessage(logicMsg);
		break;
	}
	}

	return len;
}

bool GameSession::signupUser( const std::string& id, const std::string& pw, std::string& err ) {
	std::lock_guard lock( signupAndLoginMtx_ );

	// ID 유효성 검사
	if ( id.empty( ) || id.find( ":" ) != std::string::npos ) {
		err = "Invalid ID.";
		return false;
	}

	auto in = std::ifstream( "users.txt" );
	if ( !in ) {
		CRASH( "Failed to open users.txt for reading." );
	}

	// ID 중복 확인
	std::string line;
	while ( std::getline( in, line ) ) {
		if ( line.starts_with( id + ":" ) ) {
			err = "ID already exists.";
			return false;
		}
	}
	in.close( );

	// 새로운 user 추가
	auto out = std::ofstream( "users.txt", std::ios::app );
	if ( !out ) {
		CRASH( "Failed to open users.txt for writing." );
	}

	out << id << ":" << pw << "\n";
	return true;
}

bool GameSession::loginUser( const std::string& id, const std::string& pw, std::string& err ) {
	std::lock_guard lock( signupAndLoginMtx_ );

	// ID 유효성 검사
	if( id.empty( ) || id.find( ":" ) != std::string::npos ) {
		err = "Invalid ID.";
		return false;
	}

	auto in = std::ifstream( "users.txt" );
	if ( !in ) {
		CRASH( "Failed to open users.txt for reading." );
	}

	// ID와 PW 확인
	std::string line;
	while ( std::getline( in, line ) ) {
		if ( line == id + ":" + pw ) {
			return true;
		}
	}

	err = "Invalid ID or password.";
	return false;
}

#include "pch.hpp"
#include "SendBuffer.hpp"
#include "GameSessionManager.hpp"
#include "Service.hpp"
#include "GameSession.hpp"

void GameSession::onConnected( ) {
	std::cout << "GameSession " << getId( ) << " connected.\n";
	GameSessionManager::add( std::static_pointer_cast<GameSession>( shared_from_this( ) ) );

	/*auto packet = Packet{
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

	auto enterPacket = Packet{
		.header{
			.size = sizeof( PacketHeader ) + sizeof( SCEnterPacket ),
			.id = static_cast<uint16>( PacketType::scEnter )
		},
	};
	enterPacket.scEnter.playerCount = getService( )->getSessionCount( );

	int32 index = 0;
	for ( const auto& session : GameSessionManager::getSessions( ) ) {
		enterPacket.scEnter.pIds[ index ] = session->getId( );
		enterPacket.scEnter.x[ index ] = session->x( );
		enterPacket.scEnter.y[ index ] = session->y( );
		enterPacket.scEnter.z[ index ] = session->z( );
		++index;
	}

	sendBuffer = std::make_shared<SendBuffer>( packetSize );
	sendBuffer->copyData( &enterPacket, packetSize );
	GameSessionManager::broadcast( sendBuffer );*/
}

void GameSession::onDisconnected( ) {
	auto packet = Packet{
			.header = {
				.size = sizeof( PacketHeader ) + sizeof( SCLeavePacket ),
				.id = static_cast<uint16>( PacketType::scLeave )
			},
			.scLeave = {
				.playerId = getId( )
			}
	};

	int32 packetSize = sizeof( Packet );
	auto sendBuffer = std::make_shared<SendBuffer>( packetSize );
	sendBuffer->copyData( &packet, packetSize );
	GameSessionManager::broadcast( sendBuffer );

	GameSessionManager::remove( std::static_pointer_cast<GameSession>( shared_from_this( ) ) );
}

int32 GameSession::onRecvPacket( uint8* buffer, int32 len ) {
	std::cout << "GameSession " << getId( ) << " received packet of length " << len << ".\n";

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
	}
		break;

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
	}
		break;

	case PacketType::csEnter:
		break;

	case PacketType::csLeave:
		break;

	case PacketType::csMove: {
		Packet sendPacket{ };
		sendPacket.scMove.playerId = getId( );
		if ( packet->csMove.dir == direction::w ) {
			sendPacket.header.size = sizeof( PacketHeader ) + sizeof( SCMovePacket );
			sendPacket.header.id = static_cast<uint16>( PacketType::scMove );
			sendPacket.scMove.x = x_;
			sendPacket.scMove.y = y_;
			sendPacket.scMove.z = z_ + 0.01f;
		}
		else if ( packet->csMove.dir == direction::a ) {
			sendPacket.header.size = sizeof( PacketHeader ) + sizeof( SCMovePacket );
			sendPacket.header.id = static_cast<uint16>( PacketType::scMove );
			sendPacket.scMove.x = x_ - 0.01f;
			sendPacket.scMove.y = y_;
			sendPacket.scMove.z = z_;
		}
		else if ( packet->csMove.dir == direction::s ) {
			sendPacket.header.size = sizeof( PacketHeader ) + sizeof( SCMovePacket );
			sendPacket.header.id = static_cast<uint16>( PacketType::scMove );
			sendPacket.scMove.x = x_;
			sendPacket.scMove.y = y_;
			sendPacket.scMove.z = z_ - 0.01f;
		}
		else if ( packet->csMove.dir == direction::d ) {
			sendPacket.header.size = sizeof( PacketHeader ) + sizeof( SCMovePacket );
			sendPacket.header.id = static_cast<uint16>( PacketType::scMove );
			sendPacket.scMove.x = x_ + 0.01f;
			sendPacket.scMove.y = y_;
			sendPacket.scMove.z = z_;
		}

		x_ = sendPacket.scMove.x;
		y_ = sendPacket.scMove.y;
		z_ = sendPacket.scMove.z;

		int32 packetSize = sizeof( Packet );
		auto sendBuffer = std::make_shared<SendBuffer>( packetSize );
		sendBuffer->copyData( &sendPacket, packetSize );
		GameSessionManager::broadcast( sendBuffer );
	}
		break;
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

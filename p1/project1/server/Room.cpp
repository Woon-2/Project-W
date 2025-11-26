#include "pch.hpp"
#include "Room.hpp"
#include "GameSession.hpp"
#include "SendBuffer.hpp"
#include "RoomManager.hpp"
#include "level.hpp"
#include "GameSessionManager.hpp"
#include "GameLogicManager.hpp"

void Room::init( const Level& levelData ) {
	cubes_ = levelData.cubes;
	playerStarts_ = levelData.playerStarts;
}

void Room::update(Milliseconds deltatime) {
	processMessage();

	// 위치 갱신
	for (auto& [id, user] : idUserMap_) {
		user->oldX_ = user->x_;
		user->oldZ_ = user->z_;

		if (user->keyMask_ & MoveW) {
			user->z_ += 0.1f;
		}
		if (user->keyMask_ & MoveA) {
			user->x_ -= 0.1f;
		}
		if (user->keyMask_ & MoveS) {
			user->z_ -= 0.1f;
		}
		if (user->keyMask_ & MoveD) {
			user->x_ += 0.1f;
		}

		// 불필요한 브로드캐스트를 줄이기 위해
		// 위치가 변경되었을 때만 다른 유저들에게 알림
		if(user->oldX_ != user->x_ || user->oldZ_ != user->z_) {
			auto packet = Packet{
				.header = {
					.size = sizeof( PacketHeader ) + sizeof( SCMovePacket ),
					.id = static_cast<uint16>( PacketType::scMove )
				},
				.scMove = {
					.playerId = user->getId( ),
					.x = user->x_,
					.z = user->z_
				}
			};

			int32 packetSize = sizeof( Packet );
			auto sendBuffer = std::make_shared<SendBuffer>( packetSize );
			sendBuffer->copyData( &packet, packetSize );
			broadcast( sendBuffer );
		}
	}
}

void Room::processMessage() {
	uint32 bulkSize{};
	if (users_.size() == 0) {
		bulkSize = 1;
	}
	else {
		bulkSize = static_cast<uint32>(users_.size());
	}

	auto messages = std::vector<LogicMessage>(bulkSize);
	const auto size = msgQueue_.try_dequeue_bulk(messages.begin(), bulkSize);

	for (int32 i = 0; i < size; ++i) {
		switch (messages[i].type) {
		case LogicMsgType::UserEnter:
			enter(GameSessionManager::findGameSession(messages[i].userId));
			break;

		case LogicMsgType::UserLeave:
			leave(idUserMap_[messages[i].userId]);
			break;

		case LogicMsgType::UserMoveStart: {
			auto user = idUserMap_[messages[i].userId];

			switch (messages[i].dir) {
			case Direction::w: 
				user->keyMask_ |= MoveW;
				break;
			case Direction::a:
				user->keyMask_ |= MoveA;
				break;
			case Direction::s: 
				user->keyMask_ |= MoveS;	
				break;
			case Direction::d:
				user->keyMask_ |= MoveD;
				break;
			}
			break;
		}

		case LogicMsgType::UserMoveStop: {
			auto user = idUserMap_[messages[i].userId];

			switch (messages[i].dir) {
			case Direction::w:
				user->keyMask_ &= ~MoveW;
				break;
			case Direction::a:
				user->keyMask_ &= ~MoveA;
				break;
			case Direction::s:
				user->keyMask_ &= ~MoveS;
				break;
			case Direction::d:
				user->keyMask_ &= ~MoveD;
				break;
			}
			break;
		}
		}
	}
}

void Room::enter( const SPGameSession& user ) {
	std::lock_guard<std::recursive_mutex> lock( mtx_ );
	if ( std::ranges::find( users_, user ) != users_.end( ) ) {
		return;
	}

	users_.push_back( user );
	idUserMap_[user->getId()] = user;
	//std::cout << "user count " << users_.size( ) << '\n';

	auto packet = Packet{
		.header = {
			.size = sizeof( PacketHeader ) + sizeof( SCEnterPacket ),
			.id = static_cast<uint16>( PacketType::scEnter )
		},
		.scEnter = {
			.playerCount = static_cast<int32>( users_.size( ) )
		}
	};

	for ( auto i = 0; i < users_.size( ); ++i ) {
		packet.scEnter.pIds[ i ] = users_[ i ]->getId( );
		packet.scEnter.x[ i ] = users_[ i ]->x_;
		packet.scEnter.z[ i ] = users_[ i ]->z_;
	}

	int32 packetSize = sizeof( Packet );
	auto sendBuffer = std::make_shared<SendBuffer>( packetSize );
	sendBuffer->copyData( &packet, packetSize );
	broadcast( sendBuffer );
}

void Room::leave( const SPGameSession& user ) {
	std::lock_guard<std::recursive_mutex> lock( mtx_ );

	auto packet = Packet{
		.header = {
			.size = sizeof( PacketHeader ) + sizeof( SCLeavePacket ),
			.id = static_cast<uint16>( PacketType::scLeave )
		},
		.scLeave = {
			.playerId = user->getId( )
		}
	};

	int32 packetSize = sizeof( Packet );
	auto sendBuffer = std::make_shared<SendBuffer>( packetSize );
	sendBuffer->copyData( &packet, packetSize );
	broadcast( sendBuffer );

	std::erase_if( users_, [&]( const SPGameSession& u ) {
		return u == user;
	} );

	idUserMap_.erase(user->getId());

	if ( empty( ) ) {
		auto removeRoomMsg = LogicMessage{
			.type = LogicMsgType::RemoveRoom,
			.roomId = roomId_
		};
		GameLogicManager::dispatchMessage(removeRoomMsg);
	}
}

void Room::broadcast( const SPSendBuffer& sendBuffer ) {
	std::lock_guard<std::recursive_mutex> lock( mtx_ );
	for ( auto& user : users_ ) {
		user->send( sendBuffer );
	}
}

bool Room::empty( ) {
	std::lock_guard<std::recursive_mutex> lock( mtx_ );
	return users_.empty( );
}

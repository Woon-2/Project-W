#include "pch.hpp"
#include "Room.hpp"
#include "GameSession.hpp"
#include "SendBuffer.hpp"
#include "RoomManager.hpp"
#include "level.hpp"
#include "GameSessionManager.hpp"
#include "GameLogicManager.hpp"

extern std::atomic_int32_t gPlayerId;

void Room::init(const Level& levelData) {
	cubes_ = levelData.cubes;
	playerStarts_ = levelData.playerStarts;
}

void Room::update(Milliseconds deltaTime) {
	processMessage();

	// 위치 갱신
	for (auto& [id, user] : idUserMap_) {
		user->setOldPos(user->pos().x(), user->pos().z());
		auto keyMask = user->keyMask();

		if (keyMask & MoveW) {
			user->setPos(user->pos() + user->forward() * 0.1f);
		}
		if (keyMask & MoveA) {
			user->setPos(user->pos() - user->right() * 0.1f);
		}
		if (keyMask & MoveS) {
			user->setPos(user->pos() - user->forward() * 0.1f);
		}
		if (keyMask & MoveD) {
			user->setPos(user->pos() + user->right() * 0.1f);
		}

		// 불필요한 브로드캐스트를 줄이기 위해
		// 위치가 변경되었을 때만 다른 유저들에게 알림
		//std::cout << "User " << user->getId() << " moved to (" << user->pos().x() << ", " << user->pos().z() << ")\n";
		if (user->oldX() != user->pos().x() || user->oldZ() != user->pos().z()) {
			auto packet = Packet{
				.header = {
					.size = sizeof(PacketHeader) + sizeof(SCMovePacket),
					.id = static_cast<uint16>(PacketType::scMove)
				},
				.scMove = {
					.playerId = user->getId(),
					.pos = user->pos().getXmf()
				}
			};

			int32 packetSize = sizeof(Packet);
			auto sendBuffer = std::make_shared<SendBuffer>(packetSize);
			sendBuffer->copyData(&packet, packetSize);
			broadcast(sendBuffer);
		}
	}
}

void Room::processMessage() {
	const auto bulkSize = 4u;
	auto messages = std::vector<LogicMessage>(bulkSize);
	const auto size = msgQueue_.try_dequeue_bulk(messages.begin(), bulkSize);

	for (int32 i = 0; i < size; ++i) {
		switch (messages[i].type) {
		case LogicMsgType::UserEnter:
			enter(messages[i].userId);
			break;

		case LogicMsgType::UserLeave:
			leave(messages[i].userId);
			break;

		case LogicMsgType::UserMoveStart: {
			auto user = idUserMap_[messages[i].userId];

			auto yawf = std::atan2(messages[i].forward.x, messages[i].forward.z);
			auto yaw = mu::NQuat(mu::Radian(0.f), mu::Radian(0.f), mu::Radian(yawf));
			user->setOrient(yaw);
			//user->setPlayerYaw(messages[i].playerYaw);
			user->setCameraPitch(messages[i].cameraPitch);

			auto keyMask = user->keyMask();

			switch (messages[i].dir) {
			case Direction::w:
				user->setKeyMask(keyMask | MoveW);
				break;
			case Direction::a:
				user->setKeyMask(keyMask | MoveA);
				break;
			case Direction::s:
				user->setKeyMask(keyMask | MoveS);
				break;
			case Direction::d:
				user->setKeyMask(keyMask | MoveD);
				break;
			}
			break;
		}

		case LogicMsgType::UserMoveStop: {
			auto user = idUserMap_[messages[i].userId];
			auto keyMask = user->keyMask();

			switch (messages[i].dir) {
			case Direction::w:
				user->setKeyMask(keyMask & ~MoveW);
				break;
			case Direction::a:
				user->setKeyMask(keyMask & ~MoveA);
				break;
			case Direction::s:
				user->setKeyMask(keyMask & ~MoveS);
				break;
			case Direction::d:
				user->setKeyMask(keyMask & ~MoveD);
				break;
			}
			break;
		}
		}
	}
}

void Room::enter(int32 playerId) {
	std::lock_guard<std::recursive_mutex> lock(mtx_);

	// 방에 추가할 플레이어 오브젝트 생성
	auto player = std::make_shared<Object>();
	player->setId(playerId);

	const auto userIdx = playerId % static_cast<int32>(playerStarts_.size());
	player->setPos(playerStarts_[userIdx].pos());
	player->setOrient(playerStarts_[userIdx].orient());
	player->setScale(playerStarts_[userIdx].scale());

	// 플레이어 정보 및 오브젝트들 정보 보내기
	auto setupPacket = Packet{
		.header = {
			.id = static_cast<uint16>(PacketType::scSetup)
		},
		.scSetup = {
			.objectCount = 1 + static_cast<int32>(cubes_.size())	// 플레이어 오브젝트 + 큐브 오브젝트들
		}
	};
	setupPacket.header.size = static_cast<uint16>(sizeof(Packet) + setupPacket.scSetup.objectCount * sizeof(ObjectData));

	auto objectDatas = std::vector<ObjectData>();
	auto playerData = ObjectData{
		.type = ObjectType::Player,
		.objectId = player->getId(),
		.pos = player->pos().getXmf(),
		.orient = player->orient().getXmf(),
		.scale = player->scale().getXmf()
	};
	objectDatas.emplace_back(std::move(playerData));

	for (auto i = 0; i < cubes_.size(); ++i) {
		auto cubeData = ObjectData{
			.type = ObjectType::Cube,
			.objectId = gPlayerId.fetch_add(1),
			.materialSetIdx = cubes_[i].materialSetIdx(),
			.pos = cubes_[i].pos().getXmf(),
			.orient = cubes_[i].orient().getXmf(),
			.scale = cubes_[i].scale().getXmf()
		};
		objectDatas.emplace_back(std::move(cubeData));
	}

	auto setupSendBuffer = std::make_shared<SendBuffer>(setupPacket.header.size);
	int32 packetSize = sizeof(Packet);
	setupSendBuffer->copyData(&setupPacket, packetSize);
	setupSendBuffer->copyData(objectDatas.data(), sizeof(ObjectData) * setupPacket.scSetup.objectCount);
	GameSessionManager::findGameSession(playerId)->send(setupSendBuffer);

	idUserMap_[player->getId()] = player;
	users_.emplace_back(std::move(player));
	//std::cout << "user count " << users_.size( ) << '\n';

	// enter 패킷 브로드캐스트
	auto packet = Packet{
		.header = {
			.size = sizeof(PacketHeader) + sizeof(SCEnterPacket),
			.id = static_cast<uint16>(PacketType::scEnter)
		},
		.scEnter = {
			.playerCount = static_cast<int32>(users_.size())
		}
	};

	for (auto i = 0; i < users_.size(); ++i) {
		packet.scEnter.pIds[i] = users_[i]->getId();
		packet.scEnter.x[i] = users_[i]->pos().x();
		packet.scEnter.y[i] = users_[i]->pos().y();
		packet.scEnter.z[i] = users_[i]->pos().z();
	}

	packetSize = sizeof(Packet);
	auto sendBuffer = std::make_shared<SendBuffer>(packetSize);
	sendBuffer->copyData(&packet, packetSize);
	broadcast(sendBuffer);
}

// 현재 플레이어의 정보를 제거하는 순서는 
// GameSession::onDisconnected에서 먼저 GameSessionManager의 remove를 호출해 완전히 연결을 끊은 뒤에,
// Room에 있는 유저의 오브젝트를 제거하는 순서로 이루어짐
// 따라서 leave 함수 내에서 broadcast를 호출했을 때 해당 플레이어의 GameSession이 이미 제거된 상태이므로
// findGameSession이 nullptr을 반환할 수 있음
void Room::leave(int32 playerId) {
	std::lock_guard<std::recursive_mutex> lock(mtx_);

	auto packet = Packet{
		.header = {
			.size = sizeof(PacketHeader) + sizeof(SCLeavePacket),
			.id = static_cast<uint16>(PacketType::scLeave)
		},
		.scLeave = {
			.playerId = playerId
		}
	};

	int32 packetSize = sizeof(Packet);
	auto sendBuffer = std::make_shared<SendBuffer>(packetSize);
	sendBuffer->copyData(&packet, packetSize);
	broadcast(sendBuffer);

	std::erase_if(users_, [&](const auto& u) {
		return u->getId() == playerId;
		});

	idUserMap_.erase(playerId);

	if (empty()) {
		auto removeRoomMsg = LogicMessage{
			.type = LogicMsgType::RemoveRoom,
			.roomId = roomId_
		};
		GameLogicManager::dispatchMessage(removeRoomMsg);
	}
}

void Room::broadcast(const SPSendBuffer& sendBuffer) {
	std::lock_guard<std::recursive_mutex> lock(mtx_);
	for (auto& [id, user] : idUserMap_) {
		auto session = GameSessionManager::findGameSession(id);
		if (session) {
			session->send(sendBuffer);
		}
	}
}

bool Room::empty() {
	std::lock_guard<std::recursive_mutex> lock(mtx_);
	return users_.empty();
}

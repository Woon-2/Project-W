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
	processMessage(deltaTime);

	// 물리 시뮬레이션
	std::vector<Object*> allObjects;
	allObjects.resize(users_.size());
	std::ranges::transform(users_, allObjects.begin(),
		[](const std::shared_ptr<Object>& obj) { return obj.get(); }
	);

	physicSystem_.step(allObjects, 1s / 60.f);
}

void Room::processMessage(Milliseconds deltaTime) {
	const auto bulkSize = 1000u;
	auto messages = std::vector<LogicMessage>(bulkSize);

	// 최대 bulkSize 개수만큼 메시지 꺼내기
	auto size = msgQueue_.try_dequeue_bulk(messages.begin(), bulkSize);

	// 남은 메시지가 있으면 모두 꺼내기
	LogicMessage msg;
	while (msgQueue_.try_dequeue(msg)) {
		messages.push_back(msg);
		++size;
	}

	for (int32 i = 0; i < size; ++i) {
		switch (messages[i].type) {
		case LogicMsgType::UserEnter:
			enter(messages[i].userId);
			break;

		case LogicMsgType::UserLeave:
			leave(messages[i].userId);
			break;

		case LogicMsgType::UserMouseMove: {
			auto user = idUserMap_[messages[i].userId];
			auto yaw = mu::NQuat(mu::Radian(0.f), mu::Radian(0.f), mu::Radian(messages[i].playerYawRadian));
			user->setOrient(yaw);
			user->setCameraPitch(messages[i].cameraPitchRadian);

			auto scMouseMovePacket = Packet{
				.header = {
					.size = sizeof(PacketHeader) + sizeof(SCMouseMovePacket),
					.id = static_cast<uint16>(PacketType::scMouseMove)
				},
				.scMouseMove = {
					.playerId = user->getId(),
					.playerYawRadian = messages[i].playerYawRadian,
					.cameraPitchRadian = messages[i].cameraPitchRadian
				}
			};

			int32 packetSize = sizeof(Packet);
			auto sendBuffer = std::make_shared<SendBuffer>(packetSize);
			sendBuffer->copyData(&scMouseMovePacket, packetSize);
			broadcast(sendBuffer);
			break;
		}

		case LogicMsgType::UserMoveState: {
			auto user = idUserMap_[messages[i].userId];

			auto clientCurrPos = mu::Vec3(DirectX::XMLoadFloat3(&messages[i].position));
			auto clientCurrVel = mu::Vec3(DirectX::XMLoadFloat3(&messages[i].velocity));
			auto clientTimeStamp = messages[i].timeStamp;

			if (clientCurrPos == user->pos()) {
				// 위치 변화 없음
				break;
			}
			
			auto valid = validateMove(clientCurrPos, clientCurrVel, clientTimeStamp, deltaTime, user);
			if (valid) {
				//std::cout << "valid\n";
				user->setLastMoveTimestamp(clientTimeStamp);
				user->setPos(clientCurrPos);
				auto yawRadian = std::atan2(messages[i].forward.x, messages[i].forward.z);
				auto yaw = mu::NQuat(mu::Radian(0.f), mu::Radian(0.f), mu::Radian(yawRadian));
				user->setOrient(yaw);

				auto scMovePacket = Packet{
					.header = {
						.size = sizeof(PacketHeader) + sizeof(SCMovePacket),
						.id = static_cast<uint16>(PacketType::scMove)
					},
					.scMove = {
						.playerId = user->getId(),
						.pos = user->pos().getXmf(),
						.playerYawRadian = yawRadian,
						.cameraPitchRadian = user->cameraPitch()
					}
				};

				int32 packetSize = sizeof(Packet);
				auto sendBuffer = std::make_shared<SendBuffer>(packetSize);
				sendBuffer->copyData(&scMovePacket, packetSize);
				broadcast(sendBuffer);
			}
			else {
				std::cout << "invalid\n";
				auto scRollbackPacket = Packet{
					.header = {
						.size = sizeof(PacketHeader) + sizeof(SCRollbackPacket),
						.id = static_cast<uint16>(PacketType::scRollback)
					},
					.scRollback = {
						.playerId = user->getId(),
						.pos = user->pos().getXmf()
					}
				};

				int32 packetSize = sizeof(Packet);
				auto sendBuffer = std::make_shared<SendBuffer>(packetSize);
				sendBuffer->copyData(&scRollbackPacket, packetSize);
				broadcast(sendBuffer);
			}
			break;
		}

		case LogicMsgType::UserFire: {
			const auto shooterId = messages[i].userId;
			auto shooter = idUserMap_[shooterId];

			if(!validateFire(mu::Vec3(DirectX::XMLoadFloat3(&messages[i].position)),
				mu::Vec3(DirectX::XMLoadFloat3(&messages[i].fireDir)), shooter)
			) {
				std::cout << "invalid fire\n";
				break;
			}

			auto ray = Ray{
				.origin = mu::Vec3(DirectX::XMLoadFloat3(&messages[i].position)),
				.dir = mu::NVec3(DirectX::XMLoadFloat3(&messages[i].fireDir))
			};

			for (auto& [id, user] : idUserMap_) {
				if(id == shooterId) {
					continue;
				}

				if (!intersectRay(ray, user)) {
					continue;
				}
			}
			break;
		}

		default:
			break;
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

bool Room::validateMove(mu::Vec3 clientCurrPos, mu::Vec3 clientCurrVel, uint32 clientTimeStamp,
	Milliseconds deltaTime, const std::shared_ptr<Object>& serverUserObj
) {
	/*std::cout << "prev pos : " << serverUserObj->pos().x() << ", "
		<< serverUserObj->pos().y() << ", " << serverUserObj->pos().z() << '\n';
	std::cout << "curr pos : " << clientCurrPos.x() << ", "
		<< clientCurrPos.y() << ", " << clientCurrPos.z() << '\n';*/

	const auto posDiff = clientCurrPos - serverUserObj->pos();
	//std::cout << "posDiff len2 : " << posDiff.len2() << '\n';

	auto lastMoveTimestamp = serverUserObj->lastMoveTimestamp();
	auto timeStampDiff = static_cast<float>(clientTimeStamp - lastMoveTimestamp);
	deltaTime *= timeStampDiff;

	const auto calculatedVel = posDiff / Seconds(deltaTime).count();
	const auto maxSpeed = 10.f;

	//std::cout << "calculatedVel len2 : " << calculatedVel.len2() << '\n';

	if (calculatedVel.len2() > maxSpeed * maxSpeed) {
		return false;
	}
	return true;
}

bool Room::validateFire(mu::Vec3 firePos, mu::Vec3 fireDir, const std::shared_ptr<Object>& serverUserObj) {
	auto userPos = serverUserObj->pos();

	const auto maxFireXZOffset = 0.6f;
	const auto maxFireYOffset = 1.5f;
	const auto minFireYOffset = 0.8f;
	const float maxFireAngleRad = mu::Radian(8.f);

	const auto dx = firePos.x() - userPos.x();
	const auto dz = firePos.z() - userPos.z();
	const auto distSqXZ = dx * dx + dz * dz;

	if (distSqXZ > maxFireXZOffset * maxFireXZOffset) {
		return false;
	}
	if (firePos.y() < minFireYOffset || firePos.y() > maxFireYOffset) {
		return false;
	}

	return true;
}

bool Room::intersectRay(Ray ray, const std::shared_ptr<Object>& target, float& dist) {

}

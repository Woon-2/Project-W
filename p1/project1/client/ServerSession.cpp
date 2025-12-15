#include "pch.hpp"
#include "global.hpp"
#include "ServerSession.hpp"
#include "SendBuffer.hpp"
#include "online/onlineGame.hpp"

extern std::unique_ptr<IGame> pGame;
extern moodycamel::ConcurrentQueue<Online::Message> messageQueue;

void ServerSession::onConnected() {
	std::cout << "[Client] Connected to server.\n";

	pGame = std::make_unique<Online::Game>();
	auto onlineGame = static_cast<Online::Game*>(pGame.get());
	onlineGame->setupStage();
	onlineGame->setServerSession(std::static_pointer_cast<ServerSession>(shared_from_this()));

	player_ = onlineGame->getPlayer();

	gReady.store(true);
}

void ServerSession::onDisconnected() {
	std::cout << "[Client] Disconnected from server.\n";
}

int32 ServerSession::onRecvPacket(uint8* buffer, int32 len) {
	//std::cout << "Client " << player_->getId( ) << '\n';
	auto packet = reinterpret_cast<Packet*>(buffer);
	auto pOnlineGame = static_cast<Online::Game*>(pGame.get());

	switch (static_cast<PacketType>(packet->header.id)) {
	case PacketType::scAssignId: {
		player_->setId(packet->scAssignId.playerId);
		pOnlineGame->addPlayer(player_);
		break;
	}

	case PacketType::scSetup: {
		const auto objCnt = packet->scSetup.objectCount;
		auto size = packet->header.size;
		auto objectDatas = reinterpret_cast<ObjectData*>(buffer + sizeof(Packet));

		for (i32t i = 0; i < objCnt; ++i) {
			auto message = Online::Message{
				.objectId = objectDatas[i].objectId,
				.materialSetIdx = objectDatas[i].materialSetIdx,
				.pos = DirectX::XMLoadFloat3(&objectDatas[i].pos),
				.orient = DirectX::XMLoadFloat4(&objectDatas[i].orient),
				.scale = DirectX::XMLoadFloat3(&objectDatas[i].scale)
			};

			if (objectDatas[i].type == ObjectType::Player) {
				message.type = Online::MsgType::SetupPlayer;
			}
			else {
				message.type = Online::MsgType::SetupCube;
			}

			messageQueue.enqueue(message);
		}
		break;
	}

	case PacketType::scEnter: {
		auto playerCount = packet->scEnter.playerCount;
		for (std::int32_t i = 0; i < playerCount; ++i) {
			auto pId = packet->scEnter.pIds[i];

			if (pOnlineGame->findPlayer(pId)) {
				continue;
			}

			auto x = packet->scEnter.x[i];
			auto y = packet->scEnter.y[i];
			auto z = packet->scEnter.z[i];
			pOnlineGame->createPlayer(pId, x, y, z);
		}
		break;
	}

	case PacketType::scLeave: {
		auto pId = packet->scLeave.playerId;
		//std::lock_guard<std::mutex> lock( gMtx );
		pOnlineGame->removePlayer(pId);
		break;
	}

	case PacketType::scMouseMove: {
		auto message = Online::Message{
			.type = Online::MsgType::PlayerMouseMove,
			.objectId = packet->scMouseMove.playerId,
			.orient = mu::NQuat(mu::Radian(), mu::Radian(),	mu::Radian(packet->scMouseMove.playerYawRadian)),
			.cameraPitch = packet->scMouseMove.cameraPitchRadian
		};
		messageQueue.enqueue(message);
		break;
	}

	case PacketType::scMove: {
		auto message = Online::Message{
			.type = Online::MsgType::PlayerMove,
			.objectId = packet->scMove.playerId,
			.pos = DirectX::XMLoadFloat3(&packet->scMove.pos),
			.orient = mu::NQuat(mu::Radian(), mu::Radian(),	mu::Radian(packet->scMove.playerYawRadian)),
			.cameraPitch = packet->scMove.cameraPitchRadian
		};
		messageQueue.enqueue(message);
		break;
	}
	}
	return len;
}
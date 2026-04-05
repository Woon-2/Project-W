#include "rspch.hpp"
#include "Room.hpp"
#include "RoomManager.hpp"
#include "GameSession.hpp"
#include "MemoryManager.hpp"
#include "PacketManager.hpp"
#include "Level.hpp"
#include "JobTimer.hpp"

void Room::init(const Level* levelData) {
	cubes_ = levelData->cubes;
	playerStarts_ = levelData->playerStarts;
	goblins_ = levelData->goblins;

	for (auto& g : goblins_) {
		g.setId(IdPool::pop());
		g.setSpawnPos(g.pos());
	}
}

void Room::update() {
	updateGoblinAI(17.f / 1000.f);

	doTimer(17, [this]() {	// 60fps
		update();
	});
}

void Room::updateGoblinAI(float dt) {
	if (sessions_.empty()) return;

	for (auto& goblin : goblins_) {
		// 가장 가까운 플레이어 탐색
		GameSession* nearestSession = nullptr;
		float nearestDist = std::numeric_limits<float>::max();
		for (auto s : sessions_) {
			float d = (s->player()->pos() - goblin.pos()).len();
			if (d < nearestDist) { nearestDist = d; nearestSession = s; }
		}

		mu::Vec3 velocity{};

		switch (goblin.aiState()) {
		case GoblinAIState::Patrol: {
			if (nearestDist < goblin.aggroRange()) {
				goblin.setAIState(GoblinAIState::Chase);
				break;
			}
			auto toTarget = goblin.patrolTarget() - goblin.pos();
			if (toTarget.len2() < 0.25f) {
				static std::mt19937 rng{ std::random_device{}() };
				std::uniform_real_distribution<float> angleDist(0.f, mu::pi * 2.f);
				float angle = angleDist(rng);
				goblin.setPatrolTarget(goblin.spawnPos()
					+ mu::Vec3(std::cos(angle) * 5.f, 0.f, std::sin(angle) * 5.f));
			} else {
				auto dir = mu::NVec3(toTarget);
				velocity = mu::Vec3(dir) * goblin.moveSpeed();
				goblin.setPos(goblin.pos() + velocity * dt);
				float yaw = std::atan2(dir.x(), dir.z());
				goblin.setOrient(mu::NQuat(mu::Radian(), mu::Radian(), mu::Radian(yaw)));
			}
			break;
		}
		case GoblinAIState::Chase: {
			if (nearestDist < goblin.attackRange()) {
				goblin.setAIState(GoblinAIState::Attack); break;
			}
			if (nearestDist > goblin.deaggroRange()) {
				goblin.setAIState(GoblinAIState::Return); break;
			}
			auto toPlayer = nearestSession->player()->pos() - goblin.pos();
			auto dir = mu::NVec3(toPlayer);
			velocity = mu::Vec3(dir) * goblin.moveSpeed();
			goblin.setPos(goblin.pos() + velocity * dt);
			float yaw = std::atan2(dir.x(), dir.z());
			goblin.setOrient(mu::NQuat(mu::Radian(), mu::Radian(), mu::Radian(yaw)));
			break;
		}
		case GoblinAIState::Attack: {
			if (nearestDist > goblin.attackRange()) {
				goblin.setAIState(GoblinAIState::Chase); break;
			}
			auto toPlayer = nearestSession->player()->pos() - goblin.pos();
			float yaw = std::atan2(toPlayer.x(), toPlayer.z());
			goblin.setOrient(mu::NQuat(mu::Radian(), mu::Radian(), mu::Radian(yaw)));
			// hp 차감은 데미지 패킷 구현 후 추가 예정
			break;
		}
		case GoblinAIState::Return: {
			if (nearestDist < goblin.aggroRange()) {
				goblin.setAIState(GoblinAIState::Chase); break;
			}
			auto toSpawn = goblin.spawnPos() - goblin.pos();
			if (toSpawn.len2() < 0.25f) {
				goblin.setPos(goblin.spawnPos());
				goblin.setPatrolTarget(goblin.spawnPos());
				goblin.setAIState(GoblinAIState::Patrol); break;
			}
			auto dir = mu::NVec3(toSpawn);
			velocity = mu::Vec3(dir) * goblin.moveSpeed();
			goblin.setPos(goblin.pos() + velocity * dt);
			float yaw = std::atan2(dir.x(), dir.z());
			goblin.setOrient(mu::NQuat(mu::Radian(), mu::Radian(), mu::Radian(yaw)));
			break;
		}
		}

		auto pkt = PacketManager::makeSNpcMovePacket(
			static_cast<uint16>(goblin.getId()),
			goblin.pos().getXmf(),
			goblin.orient().getXmf(),
			velocity.getXmf()
		);
		broadcast(pkt);
	}
}

void Room::enter(GameSession* session) {
	// 서버에서 사용할 player 객체 세팅
	auto player = session->player();
	player->setModel(RoomManager::playerModelData());
	player->setPos(playerStarts_[sessions_.size() % playerStarts_.size()].pos());	// 새로 들어오는 플레이어는 playerStarts_에서 순서대로 위치를 받는다.
	player->setOrient(playerStarts_[sessions_.size() % playerStarts_.size()].orient());
	player->setScale(playerStarts_[sessions_.size() % playerStarts_.size()].scale());

	// 새로 들어오는 플레이어에 대한 snapshot 만들기
	auto newPlayerInfo = PlayerInfo{
		.playerId = static_cast<uint16>(session->id()),
		.materialSetIdx = 0,
		.pos = player->pos().getXmf(),
		.orient = player->orient().getXmf(),
		.scale = player->scale().getXmf(),
	};

	// 기존 플레이어들 및 기타 object들에 대한 snapshot 만들기
	auto objInfos = std::vector<ObjectInfo>();
	objInfos.reserve(sessions_.size() + cubes_.size());

	for (auto session : sessions_) {
		auto playerInfo = ObjectInfo{
			.type = ObjectType::Player,
			.objectId = static_cast<uint16>(session->id()),
			.materialSetIdx = 0,
			.pos = session->player()->pos().getXmf(),
			.orient = session->player()->orient().getXmf(),
			.scale = session->player()->scale().getXmf(),
		};
		objInfos.emplace_back(playerInfo);
	}

	for (const auto& g : goblins_) {
		objInfos.push_back(ObjectInfo{
			.type = ObjectType::Goblin,
			.objectId = static_cast<uint16>(g.getId()),
			.materialSetIdx = 0,
			.pos = g.pos().getXmf(),
			.orient = g.orient().getXmf(),
			.scale = g.scale().getXmf(),
		});
	}

	// 패킷 생성 후 새로 들어온 플레이어에게 전송
	auto enterPkt = PacketManager::makeSEnterPacket(newPlayerInfo, objInfos);
	session->send(enterPkt);

	// 새로 들어온 플레이어의 정보를 기존 플레이어들에게 브로드캐스트
	if (sessions_.size() > 0) {	// 기존 플레이어가 있을 때만 브로드캐스트
		auto enterOtherPkt = PacketManager::makeSEnterOtherPacket(newPlayerInfo);
		broadcast(enterOtherPkt);
	}

	// room 상태 변경
	sessions_.push_back(session);
	idSessionMap_[session->id()] = session;
}

void Room::leave(GameSession* session) {
	std::erase_if(sessions_, [session](GameSession* s) { return s == session; });
	idSessionMap_.erase(session->id());

	auto leavePkt = PacketManager::makeSLeavePacket(static_cast<uint16>(session->id()));
	broadcast(leavePkt);

	ObjectPool<GameSession>::push(session);

	if (sessions_.size() == 0) {
		RoomManager::removeRoom(id_);
	}
}

void Room::move(int32 sessionId, CMovePacket* cMvPkt) {
	// 혹시나 하는 가능성 중, sessionId로 idSessionMap_에서 session을 찾는 것이 유효하지 않을 수도 있음.
	// leave한 sessionId가 move 패킷을 보내는 경우 등. 일단은 방에 있는 session이 보낸 패킷이므로 유효하다고 가정하고 작성한다.
	auto session = idSessionMap_[sessionId];

	auto player = session->player();
	player->setPos(DirectX::XMLoadFloat3(&cMvPkt->pos));
	//player->setOrient(DirectX::XMLoadFloat4(&cMvPkt->orient));
	//player->physicState().evVelocity = DirectX::XMLoadFloat3(&cMvPkt->velocity);

	auto sMvPkt = PacketManager::makeSMovePacket(static_cast<uint16>(sessionId), player->pos().getXmf());
	broadcastExcept(session, sMvPkt);

	ObjectPool<CMovePacket>::push(cMvPkt);
}

void Room::rotate(int32 sessionId, CMouseMovePacket* cMouseMvPkt) {
	auto session = idSessionMap_[sessionId];

	auto player = session->player();
	auto yaw = mu::NQuat(mu::Radian(), mu::Radian(), mu::Radian(cMouseMvPkt->yawRadian));
	player->setOrient(yaw);

	auto sMouseMvPkt = PacketManager::makeSMouseMovePacket(static_cast<uint16>(sessionId), cMouseMvPkt->yawRadian);
	broadcastExcept(session, sMouseMvPkt);

	ObjectPool<CMouseMovePacket>::push(cMouseMvPkt);
}

void Room::broadcast(const std::shared_ptr<SendBuffer>& sendBuffer) {
	for(auto session : sessions_) {
		session->send(sendBuffer);
	}
}

void Room::broadcastExcept(GameSession* exceptSession, const std::shared_ptr<SendBuffer>& sendBuffer) {
	for (auto session : sessions_) {
		if (session == exceptSession) {
			continue;
		}
		session->send(sendBuffer);
	}
}

void Room::doTimer(uint64 delay, CallbackType&& callback) {
	auto job = ObjectPool<Job>::pop(std::move(callback));
	JobTimer::addJob(delay, id_, job);
}

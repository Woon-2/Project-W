#include "rspch.hpp"
#include "Room.hpp"
#include "RoomManager.hpp"
#include "GameSession.hpp"
#include "MemoryManager.hpp"
#include "PacketManager.hpp"
#include "Level.hpp"
#include "JobTimer.hpp"
#include "collision.hpp"
#include "skill/skillCompiler.hpp"
#include "TacticalGoblin.hpp"

void Room::init(const Level* levelData) {
	cubes_ = levelData->cubes;
	playerStarts_ = levelData->playerStarts;
	goblins_ = levelData->goblins;

	for (auto& c : cubes_) {
		c.body().setMotionType(MotionType::Static);
		c.body().snapToCurrent();
		physicsWorld_.registerBody(&c.body(), [&c]() { c.rebuildBodyBVH(); });
	}

	for (auto& g : goblins_) {
		g.setId(IdPool::pop());
		g.setSpawnPos(g.pos());
		g.applyGoblinConfig();
		g.body().setMotionType(MotionType::Dynamic);
		g.body().setMass(70.f);
		g.body().setLinearDamping(0.1f);
		g.body().setAngularDamping(25.f);
		g.body().setRestitution(0.0f);
		g.body().setUprightStiffness(4000.f);
		g.body().snapToCurrent();
		physicsWorld_.registerBody(&g.body(), [&g]() { g.rebuildBodyBVH(); });
	}

	for (const auto& spawner : levelData->goblinSpawners) {
		int groupId = static_cast<int>(npcGroups_.size());
		npcGroups_.emplace_back(
			std::make_unique<NpcGroup>(groupId, spawner.center, spawner.activityRadius)
		);
		for (int32 i = spawner.startIdx; i < spawner.startIdx + spawner.count; ++i) {
			goblins_[i].setGroupId(groupId);
			goblins_[i].setActivityZone(spawner.center, spawner.activityRadius);
		}
	}

	terrain_ = levelData->terrain;
	if (!terrain_.heightField().empty()) {
		terrain_.body().setMotionType(MotionType::Static);
		terrain_.body().snapToCurrent();
		physicsWorld_.registerTerrain(&terrain_.body(), &terrain_.heightField());
	}

	// Compile skill assets from the shared Lua skill directory.
	{
		ServerSkillCompiler compiler;
		auto assets = compiler.compileAll("../resources/skills");
		skillSystem_.registerAssets(std::move(assets));
	}
}

void Room::update() {
	static constexpr Milliseconds dt = 1s / 60.f;	// 60fps
	static constexpr Seconds dtSec   = 1s / 60.f;

	physicsWorld_.step(dtSec);
	updateGoblinAI(dt);
	updateTacticalAI(dt);
	updateSkillSystem(dt);

	doTimer(dt, [this]() {
		update();
	});
}

void Room::updateGoblinAI(Milliseconds dt) {
	if (sessions_.empty()) return;

	// 경과 시간 누적, NpcGroup 기억 만료 정리, 활동 영역 밖 플레이어 메모리 정리
	elapsedMs_ += dt;
	for (auto& grp : npcGroups_) {
		grp->update(elapsedMs_);
		grp->clearMemoryIfPlayerOutside(*this);
	}

	rebuildLivingPlayersCache();
	rebuildAggroCount();

	uint64 serverNow = static_cast<uint64>(
		std::chrono::duration_cast<std::chrono::milliseconds>(
			HighResolutionClock::now().time_since_epoch()
		).count()
	);

	// Goblin의 angular velocity 고정 (물리 solver 누적 방지)
	for (auto& goblin : goblins_)
		goblin.body().setOmega(mu::Vec3{});

	std::vector<SNpcMoveInfo> moveInfos;
	moveInfos.reserve(goblins_.size());

	for (auto& goblin : goblins_) {
		goblin.recordSnapshot(serverNow);

		auto result = goblin.update(dt, *this);

		if (goblin.hp() > 0) {
			moveInfos.push_back({
				static_cast<uint16>(goblin.getId()),
				goblin.pos().getXmf(),
				goblin.orient().getXmf(),
				goblin.linearVel().getXmf()
			});
		}

		if (result.hit) {
			broadcast(PacketManager::makeSNpcAttackPacket(static_cast<uint16>(goblin.getId())));
			broadcast(PacketManager::makeSHitPacket(result.hit->targetId, result.hit->newHp));
		}

		if (result.respawned) {
			broadcast(PacketManager::makeSNpcRespawnPacket(
				static_cast<uint16>(goblin.getId()),
				goblin.hp(),
				goblin.pos().getXmf()
			));
		}
	}

	if (!moveInfos.empty())
		broadcast(PacketManager::makeSNpcMoveBatchPacket(moveInfos));
}

void Room::rebuildLivingPlayersCache() {
	livingPlayersCache_.clear();
	for (auto* s : sessions_) {
		if (s->player()->hp() > 0)
			livingPlayersCache_.push_back(s);
	}
}

void Room::rebuildAggroCount() {
	aggroCount_.clear();
	for (const auto& g : goblins_) {
		if (g.hp() <= 0) continue;
		NpcState s = g.getState();
		if (s == NpcState::Chase        ||
		    s == NpcState::AttackWindup  ||
		    s == NpcState::AttackRecover ||
		    s == NpcState::Reposition)
			aggroCount_[g.getTargetId()]++;
	}
}

void MU_CALLCONV Room::findNearbyNpcPositions(mu::Vec3 from, float radius,
                                               uint32 excludeId,
                                               std::vector<mu::Vec3>& out) const {
	float r2 = radius * radius;
	for (const auto& g : goblins_) {
		if (g.getId() == excludeId || g.hp() <= 0) continue;
		if ((g.pos() - from).len2() < r2)
			out.push_back(g.pos());
	}
	for (const auto& npc : tacticalNpcs_) {
		if (!npc || npc->getId() == excludeId || npc->hp() <= 0) continue;
		if ((npc->pos() - from).len2() < r2)
			out.push_back(npc->pos());
	}
	if (platoonLeader_ && platoonLeader_->getId() != excludeId &&
	    platoonLeader_->hp() > 0 &&
	    (platoonLeader_->pos() - from).len2() < r2)
		out.push_back(platoonLeader_->pos());
}

int32 Room::countNpcsTargeting(int32 playerId) const {
	auto it = aggroCount_.find(playerId);
	return (it != aggroCount_.end()) ? it->second : 0;
}

NpcGroup* Room::getNpcGroup(int32 groupId) {
	if (groupId < 0 || groupId >= static_cast<int>(npcGroups_.size())) return nullptr;
	return npcGroups_[groupId].get();
}

GameSession* Room::findLivingSessionByPlayerId(int32 playerId) const {
	auto it = idSessionMap_.find(playerId);
	if (it == idSessionMap_.end()) return nullptr;
	return (it->second->player()->hp() > 0) ? it->second : nullptr;
}

void Room::enter(GameSession* session) {
	// 서버에서 사용할 player 객체 세팅
	auto player = session->player();
	player->setModel(RoomManager::playerModelData());
	player->setPos(playerStarts_[sessions_.size() % playerStarts_.size()].pos());	// 새로 들어오는 플레이어는 playerStarts_에서 순서대로 위치를 받는다.
	player->setOrient(playerStarts_[sessions_.size() % playerStarts_.size()].orient());
	player->setScale(playerStarts_[sessions_.size() % playerStarts_.size()].scale());
	player->body().setMotionType(MotionType::Kinematic);
	player->body().snapToCurrent();
	physicsWorld_.registerBody(&player->body(), [player]() { player->rebuildBodyBVH(); });

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

	// 시계 동기화 패킷 전송 (지연 보상용)
	auto timeSyncPkt = PacketManager::makeSTimeSyncPacket(
		static_cast<uint64>(
			std::chrono::duration_cast<std::chrono::milliseconds>(
				HighResolutionClock::now().time_since_epoch()
			).count()
		));
	session->send(timeSyncPkt);

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
	physicsWorld_.unregisterBody(&session->player()->body());

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

	if (session == nullptr) {
		std::cout << "[ move() ] 존재하지 않는 session을 찾고 있습니다. sessionId: " << sessionId << '\n';
		return;
	}

	auto player = session->player();
	player->setPos(DirectX::XMLoadFloat3(&cMvPkt->pos));

	auto sMvPkt = PacketManager::makeSMovePacket(
		static_cast<uint16>(sessionId),
		player->pos().getXmf(),
		cMvPkt->velocity
	);
	broadcastExcept(session, sMvPkt);

	ObjectPool<CMovePacket>::push(cMvPkt);
}

void Room::rotate(int32 sessionId, CMouseMovePacket* cMouseMvPkt) {
	auto session = idSessionMap_[sessionId];

	if(session == nullptr) {
		std::cout << "[ rotate() ] 존재하지 않는 session을 찾고 있습니다. sessionId: " << sessionId << '\n';
		return;
	}

	auto player = session->player();
	auto yaw = mu::NQuat(mu::Radian(), mu::Radian(), mu::Radian(cMouseMvPkt->yawRadian));
	player->setOrient(yaw);

	auto sMouseMvPkt = PacketManager::makeSMouseMovePacket(static_cast<uint16>(sessionId), cMouseMvPkt->yawRadian);
	broadcastExcept(session, sMouseMvPkt);

	ObjectPool<CMouseMovePacket>::push(cMouseMvPkt);
}

void Room::updateSkillSystem(Milliseconds dt) {
	if (sessions_.empty()) return;

	// Build owner states from all live player sessions
	std::vector<ServerSkillOwner> owners;
	owners.reserve(sessions_.size());
	for (auto* s : sessions_) {
		auto* p = s->player();
		if (p->hp() <= 0) continue;
		owners.push_back({ static_cast<i32t>( p->getId() ), p->pos(), p->orient() });
	}

	// Build targets from live goblins
	std::vector<ServerSkillTarget> targets;
	targets.reserve(goblins_.size());
	for (auto& g : goblins_) {
		if (g.hp() <= 0) continue;
		targets.push_back({ static_cast<i32t>( g.getId() ), AABB{ g.pos(), { 1.0f, 2.0f, 1.0f } }, g.hp() });
	}

	std::vector<SkillHitResult> hits;
	skillSystem_.update(dt, owners, targets, hits);

	for (const auto& hit : hits) {
		for (auto& g : goblins_) {
			if (g.getId() != hit.targetId) continue;
			int32 newHp = std::max(g.hp() - hit.damage, 0);
			g.setHp(newHp);
			broadcast(PacketManager::makeSSkillHitPacket(
				static_cast<uint16>(hit.attackerId),
				static_cast<uint16>(hit.targetId),
				newHp,
				hit.skillAssetId
			));
			break;
		}
	}
}

void Room::skillStart(int32 sessionId, uint32 skillAssetId, uint64 clientMs) {
	auto sessionIt = idSessionMap_.find(sessionId);
	if (sessionIt == idSessionMap_.end()) return;

	auto* player = sessionIt->second->player();
	if (player->hp() <= 0) return;

	// Compute elapsed time for lag compensation (same pattern as attack())
	uint64 serverNow = static_cast<uint64>(
		std::chrono::duration_cast<std::chrono::milliseconds>(
			HighResolutionClock::now().time_since_epoch()
		).count()
	);
	uint64 elapsedRaw = (serverNow > clientMs) ? (serverNow - clientMs) : 0u;
	uint16 elapsedMs  = static_cast<uint16>(std::min(elapsedRaw, static_cast<uint64>(65535u)));

	// Start server-side skill instance for authoritative hit detection
	skillSystem_.startSkill(skillAssetId, player->getId(),
	                        Milliseconds{ static_cast<float>(elapsedMs) });

	// Broadcast to OTHER clients so they play the visual effect
	broadcastExcept(sessionIt->second,
		PacketManager::makeSSkillStartPacket(
			skillAssetId,
			static_cast<uint16>(player->getId()),
			elapsedMs
		)
	);
}

void Room::attack(int32 sessionId, uint64 clientMs) {
	auto sessionIt = idSessionMap_.find(sessionId);

	if (sessionIt == idSessionMap_.end()) {
		std::cout << "[ attack() ] 존재하지 않는 session을 찾고 있습니다. sessionId: " << sessionId << '\n';
		return;
	}

	auto player = sessionIt->second->player();
	if (player->hp() <= 0) {
		return;
	}

	broadcastExcept(sessionIt->second,
		PacketManager::makeSPlayerAttackPacket(static_cast<uint16>(player->getId())));

	static const mu::Vec3 kHalfExtent{ 1.5f, 1.5f, 1.5f };
	static constexpr float kOffsetFwd = 1.0f;
	static constexpr int32 kDamage = 30;

	auto hitbox = buildAttackAABB(
		player->pos(), player->forward(), kHalfExtent, kOffsetFwd
	);

	// clientMs ≈ serverTime - D (단방향 지연), 되감기 타겟으로 사용
	uint64 targetMs = clientMs;

	for (auto& goblin : goblins_) {
		if (goblin.hp() <= 0) {
			continue;
		}

		AABB goblinAABB{ goblin.rewindPos(targetMs), {1.0f, 2.0f, 1.0f} };

		if (collides(hitbox, goblinAABB).hit) {
			int32 newHp = std::max(goblin.hp() - kDamage, 0);
			goblin.setHp(newHp);

			broadcast(PacketManager::makeSHitPacket(static_cast<uint16>(goblin.getId()), newHp));
		}
	}
}

void Room::broadcast(const std::shared_ptr<SendBuffer>& sendBuffer) {
	for (auto session : sessions_) {
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

void Room::doTimer(Milliseconds delay, CallbackType&& callback) {
	auto job = ObjectPool<Job>::pop(std::move(callback));
	JobTimer::addJob(delay, id_, job);
}

// ── Tactical AI ──────────────────────────────────────────────────────────────

void Room::updateTacticalAI(Milliseconds dt) {
	if (tacticalNpcs_.empty() && !platoonLeader_) return;

	Seconds dtSec = std::chrono::duration_cast<Seconds>(dt);

	std::vector<SNpcMoveInfo> moveInfos;
	moveInfos.reserve(tacticalNpcs_.size() + (platoonLeader_ ? 1 : 0));

	for (auto& npc : tacticalNpcs_) {
		if (!npc) continue;
		npc->body().setOmega(mu::Vec3{});
		auto result = npc->update(dtSec, *this);
		if (npc->hp() > 0) {
			moveInfos.push_back({
				static_cast<uint16>(npc->getId()),
				npc->pos().getXmf(),
				npc->orient().getXmf(),
				npc->linearVel().getXmf()
			});
		}
		for (const auto& hit : result.hits)
			broadcast(PacketManager::makeSHitPacket(hit.targetId, hit.newHp));
	}

	for (auto& squad : tacticalSquads_) {
		if (squad) squad->update(dtSec, *this);
	}

	if (platoonLeader_) {
		platoonLeader_->body().setOmega(mu::Vec3{});
		auto result = platoonLeader_->update(dtSec, *this);
		if (platoonLeader_->hp() > 0) {
			moveInfos.push_back({
				static_cast<uint16>(platoonLeader_->getId()),
				platoonLeader_->pos().getXmf(),
				platoonLeader_->orient().getXmf(),
				platoonLeader_->linearVel().getXmf()
			});
		}
		for (const auto& hit : result.hits)
			broadcast(PacketManager::makeSHitPacket(hit.targetId, hit.newHp));
	}

	if (!moveInfos.empty())
		broadcast(PacketManager::makeSNpcMoveBatchPacket(moveInfos));
}

bool Room::tryReserveTacticalAttackSlot(uint32_t targetId, uint32_t npcId) {
	constexpr int MAX_ATTACKERS = 5;
	auto& slots = tacticalAttackSlots_[targetId];
	if (slots.count(npcId)) return true;
	if (static_cast<int>(slots.size()) >= MAX_ATTACKERS) return false;
	slots.insert(npcId);
	return true;
}

void Room::releaseTacticalAttackSlot(uint32_t targetId, uint32_t npcId) {
	auto it = tacticalAttackSlots_.find(targetId);
	if (it == tacticalAttackSlots_.end()) return;
	it->second.erase(npcId);
	if (it->second.empty())
		tacticalAttackSlots_.erase(it);
}

uint32_t Room::beginWedgeCharge() {
	return nextWedgeChargeId_++;
}

void Room::endWedgeCharge(uint32_t chargeId) {
	wedgeHitRecord_.erase(chargeId);
}

int32 Room::tryApplyWedgeChargeHit(uint32_t chargeId, int32 playerId, float damage) {
	auto& record = wedgeHitRecord_[chargeId];
	if (record.count(static_cast<uint32_t>(playerId))) return -1;
	record.insert(static_cast<uint32_t>(playerId));

	GameSession* s = findLivingSessionByPlayerId(playerId);
	if (!s) return -1;

	Player* p = s->player();
	int32 newHp = std::max(0, p->hp() - static_cast<int32>(damage));
	p->setHp(newHp);
	return newHp;
}

void Room::spawnTacticalGoblinEncounter(mu::Vec3 spawnCenter, mu::Vec3 bossPos,
                                         int numSquads, int troopersPerSquad)
{
	auto makeBase = [](mu::Vec3 pos) {
		Object base;
		base.setPos(pos);
		return base;
	};
	auto registerBody = [&](Object& obj) {
		obj.setId(IdPool::pop());
		obj.body().setMotionType(MotionType::Kinematic);
		obj.body().snapToCurrent();
		Object* raw = &obj;
		physicsWorld_.registerBody(&obj.body(), [raw]() { raw->rebuildBodyBVH(); });
	};

	TacticalNpcConfig trooperCfg = TacticalGoblin::trooperConfig();
	TacticalNpcConfig bossCfg    = TacticalGoblin::bossConfig();

	constexpr float TWO_PI = 2.f * 3.14159265f;

	for (int s = 0; s < numSquads; ++s) {
		float squadAngleBase = TWO_PI * static_cast<float>(s) / static_cast<float>(numSquads);

		auto squad = std::make_unique<TacticalSquad>(
			s, trooperCfg.attackRange, trooperCfg.separationRadius);

		for (int t = 0; t < troopersPerSquad; ++t) {
			int   ring  = 1 + t / 5;
			float angle = squadAngleBase + TWO_PI * static_cast<float>(t % 5) /
			              5.f + static_cast<float>(ring) * 0.3f;
			float r = 10.f + static_cast<float>(ring) * trooperCfg.separationRadius * 2.f;
			mu::Vec3 trooperPos(spawnCenter.x() + r * std::cosf(angle),
			                    spawnCenter.y(),
			                    spawnCenter.z() + r * std::sinf(angle));

			auto npc = std::make_unique<TacticalNpc>(makeBase(trooperPos), trooperCfg);
			registerBody(*npc);
			npc->setSquadId(s);

			squad->addMember(npc.get());
			tacticalNpcs_.push_back(std::move(npc));
		}

		tacticalSquads_.push_back(std::move(squad));
	}

	platoonLeader_ = std::make_unique<PlatoonLeader>(makeBase(bossPos), bossCfg);
	registerBody(*platoonLeader_);

	for (auto& sq : tacticalSquads_)
		platoonLeader_->addSquad(sq.get());
}

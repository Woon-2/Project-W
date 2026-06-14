#include "rspch.hpp"
#include "Room.hpp"
#include "RoomManager.hpp"
#include "GameSession.hpp"
#include "MemoryManager.hpp"
#include "PacketManager.hpp"
#include "Level.hpp"
#include "AssetManager.hpp"
#include "JobTimer.hpp"
#include "collision.hpp"
#include "serverAnimation.hpp"
#include "TacticalGoblin.hpp"
#include "MidBossTactics.hpp"

void Room::registerObject(Object* obj) {
	const int id = static_cast<int>(obj->getId());
	if (id >= static_cast<int>(objectById_.size()))
		objectById_.resize(id + 1, nullptr);
	objectById_[id] = obj;
}

void Room::unregisterObject(Object* obj) {
	const int id = static_cast<int>(obj->getId());
	if (id >= 0 && id < static_cast<int>(objectById_.size()))
		objectById_[id] = nullptr;
}

float MU_CALLCONV Room::groundHeightAtWorld(float x, float z) const {
	return worldTerrain_ ? worldTerrain_->heightAtWorld(x, z) : 0.f;
}

void Room::bindGroundQueries(SkillDispatchContext& ctx) const {
	if (!worldTerrain_) return;
	const TerrainChunkManager* terrain = worldTerrain_;
	ctx.groundHeight = [terrain](float x, float z) { return terrain->heightAtWorld(x, z); };
	ctx.groundNormal = [terrain](float x, float z) { return terrain->normalAtWorld(x, z); };
}

mu::Vec3 MU_CALLCONV Room::randomSpawnInDisc(mu::Vec3 center, float radius) const {
	static thread_local std::mt19937 rng{ std::random_device{}() };
	std::uniform_real_distribution<float> distR(0.f, 1.f);
	std::uniform_real_distribution<float> distAngle(0.f, 2.f * DirectX::XM_PI);

	const float r     = radius * std::sqrt(distR(rng));
	const float theta = distAngle(rng);
	const float x = center.x() + r * std::cos(theta);
	const float z = center.z() + r * std::sin(theta);
	return mu::Vec3(x, groundHeightAtWorld(x, z), z);
}

void Room::setupGoblin(Goblin& g, const Level& level) {
	const auto& anims = level.assetManager->goblinAnimations();
	g.setModel(level.assetManager->modelGoblin());
	g.animController().registerClip("Idle",   findServerAnimClip(anims, "Goblin_Idle"));
	g.animController().registerClip("Walk",   findServerAnimClip(anims, "Goblin_Walk"));
	g.animController().registerClip("Attack", findServerAnimClip(anims, "Goblin_Attack"));
	g.animController().registerClip("Die",    findServerAnimClip(anims, "Goblin_Death"));
	g.animController().switchClip("Idle");
	g.applyGoblinConfig();
	g.setCanReceiveDamage(true);   // skill system targets require this (default ctor leaves it false)
	g.body().setMotionType(MotionType::Dynamic);
	g.body().setMass(70.f);
	g.body().setLinearDamping(0.1f);
	g.body().setAngularDamping(25.f);
	g.body().setRestitution(0.0f);
	g.body().setUprightStiffness(4000.f);
	g.body().enableMotor(true);
}

void Room::setupStronghold(Stronghold& sh, const StrongholdDef& sd, const Level& level) {
	// Placeholder visual: a tall vertical bar (the real stronghold model is TBD).
	// The authored sd.scale is ignored for the placeholder.
	const mu::Vec3 kPlaceholderScale{ 1.5f, 5.f, 1.5f };

	sh.setModel(level.assetManager->modelCube());   // placeholder mesh (cube) -> BVH hit target
	sh.setFaction(Faction::Monsters);     // player skills (hostile to Monsters) can damage it
	sh.setCanReceiveDamage(true);
	sh.setHp(sd.maxHp);
	sh.setOrient(sd.orient);
	sh.setScale(kPlaceholderScale);

	// The cube model's pivot is at its center, so place it on the ground by
	// lifting it half its world-space height (AABB.size is full extent). The
	// base is then sunk slightly so its flat bottom face is NOT coplanar with the
	// terrain mesh (coplanar surfaces z-fight, producing flickering stripes). The
	// raised pos + scale are sent to clients via ObjectInfo, so the client
	// visual matches without extra work.
	const float groundY = groundHeightAtWorld(sd.center.x(), sd.center.z());
	sh.setPos(mu::Vec3(sd.center.x(), groundY, sd.center.z()));
	const BVH& bvh = sh.body().worldBVH();
	const float halfH = bvh.empty() ? 0.f : bvh.nodes[0].bounds.size.y() * 0.5f;
	const float kGroundBury = 0.5f;   // bury the bottom face below the terrain surface
	sh.setPos(mu::Vec3(sd.center.x(), groundY + std::max(0.f, halfH - kGroundBury), sd.center.z()));

	sh.body().setMotionType(MotionType::Static);
}

void Room::init(const Level* levelData) {
	cubes_ = levelData->cubes;
	playerStarts_ = levelData->playerStarts;
	worldTerrain_ = &levelData->terrainChunks;   // shared, read-only (height + stronghold defs)
	assetManager_ = levelData->assetManager;     // backref: cube model for runtime barriers

	for (auto& c : cubes_) {
		c.body().setMotionType(MotionType::Static);
		c.body().snapToCurrent();
		physicsWorld_.registerBody(&c.body(), [&c]() { c.rebuildBodyBVH(); });
	}

	// ── Strongholds drive monster spawning. Build a fixed goblin pool (sized to
	//    the sum of per-stronghold target counts) plus one Stronghold structure
	//    per definition. Pools are pre-sized so registerBody/registerObject can
	//    take stable addresses (no reallocation after registration).
	const auto& sdefs = worldTerrain_->strongholds();

	int totalGoblins = 0;
	for (const auto& sd : sdefs)
		for (const auto& pop : sd.populations)
			if (pop.type == ObjectType::Goblin) totalGoblins += pop.targetCount;
	goblins_.reserve(static_cast<size_t>(totalGoblins));
	strongholds_.reserve(sdefs.size());

	for (const auto& sd : sdefs) {
		const int groupId = static_cast<int>(npcGroups_.size());
		npcGroups_.emplace_back(
			std::make_unique<NpcGroup>(groupId, sd.center, sd.activityRadius));

		int goblinCount = 0;
		for (const auto& pop : sd.populations)
			if (pop.type == ObjectType::Goblin) goblinCount += pop.targetCount;

		const int poolStart = static_cast<int>(goblins_.size());
		for (int i = 0; i < goblinCount; ++i) {
			Goblin g{};
			setupGoblin(g, *levelData);
			const mu::Vec3 pos = randomSpawnInDisc(sd.center, sd.spawnRadius);
			g.setId(IdPool::pop());
			g.setFaction(Faction::Monsters);
			g.setPos(pos);
			g.setSpawnPos(pos);                              // leash home = spawn point
			g.setActivityZone(sd.center, sd.activityRadius); // roam radius = stronghold area
			g.setGroupId(groupId);
			goblins_.push_back(std::move(g));
		}

		Stronghold sh{};
		sh.setId(IdPool::pop());                 // required: registerObject indexes objectById_ by id
		setupStronghold(sh, sd, *levelData);
		sh.configure(sd, groupId, poolStart, goblinCount);
		strongholds_.push_back(std::move(sh));
	}

	// Register all goblins after the pool is fully built (no reallocation risk).
	npcBroad_.setFatMargin(NPC_SEPARATION_FAT_MARGIN);   // separation neighbor broad phase
	for (auto& g : goblins_) {
		g.body().snapToCurrent();
		physicsWorld_.registerBody(&g.body(), [&g]() { g.rebuildBodyBVH(); });
		registerObject(&g);
		npcBodyOwner_[&g.body()] = &g;   // SAP 쌍(RigidBody*) → NPC 역참조
	}

	// Strongholds are damageable static structures (collidable obstacles).
	for (auto& sh : strongholds_) {
		sh.body().snapToCurrent();
		physicsWorld_.registerBody(&sh.body(), [&sh]() { sh.rebuildBodyBVH(); });
		registerObject(&sh);
	}

	// Register one static collider per terrain chunk. Height field data is owned
	// by the shared (boot-time) TerrainChunkManager on the Level and referenced
	// read-only. reserve() first: registerTerrain takes &chunk.body(), so the
	// vector must not reallocate after registration (same invariant as goblins_).
	const TerrainChunkManager& terrainChunks = levelData->terrainChunks;
	terrainChunks_.reserve(terrainChunks.chunkCount());
	terrainChunks.forEachChunk(
		[&](int /*col*/, int /*row*/, mu::Vec3 worldOffset, const TerrainHeightField* hf) {
			auto& t = terrainChunks_.emplace_back();
			t.body().setMotionType(MotionType::Static);
			t.setPos(worldOffset);            // collider origin = terrain body pos
			t.body().snapToCurrent();
			t.setHeightField(hf);             // shared, read-only
			physicsWorld_.registerTerrain(&t.body(), hf);
		});

	// 스킬 레지스트리는 부팅 시 AssetManager가 1회 컴파일하여 전 룸이 공유한다.
	// (사양이 방마다 달라지지 않으므로 참조만 바인딩; 컴파일/복사 없음.)
	skillSystem_.bindRegistry(&levelData->assetManager->skillAssets());

	// Trigger volumes (pure query volumes; not registered with PhysicsWorld).
	zoneSystem_.build(worldTerrain_->zones());
	bindZoneHandlers();
}

// Binds gameplay behavior to zone tags. Handlers run on the room thread each
// tick; lambdas defined here have full access to Room internals.
// [임시 디버그] 정식 Arena_Isis zone/마커가 레벨에 아직 없어, 검증용으로 기존 Arena_Hobgoblin zone
// 진입 시 홉고블린 대신 Isis 인카운터를 스폰한다. 정식 Arena_Isis 마커 저작 후 이 값을 0으로 되돌려
// 홉고블린 정상 경로를 복원할 것.
#define ISIS_DEBUG_TRIGGER_VIA_HOBGOBLIN 1

void Room::bindZoneHandlers() {
	// Mid-boss arena: entering starts the encounter. Designers author a
	// ZoneMarker tagged "Arena_Hobgoblin" (factionMask = Players).
#if ISIS_DEBUG_TRIGGER_VIA_HOBGOBLIN
	zoneSystem_.on("Arena_Hobgoblin", ZoneEvent::Enter,
		[](Room& room, Zone& zone, uint32 playerId, Object* /*obj*/) {
			room.onArenaIsisEnter(zone, playerId);   // [디버그] 홉고블린 대신 Isis 스폰
		});
#else
	zoneSystem_.on("Arena_Hobgoblin", ZoneEvent::Enter,
		[](Room& room, Zone& zone, uint32 playerId, Object* /*obj*/) {
			room.onArenaHobgoblinEnter(zone, playerId);
		});
#endif

	zoneSystem_.on("Arena_GrandBaum", ZoneEvent::Enter,
		[](Room& room, Zone& zone, uint32 playerId, Object* /*obj*/) {
			room.onArenaGrandBaumEnter(zone, playerId);
		});

	// 정식 Isis zone(레벨에 "Arena_Isis" 마커 저작 후 작동).
	zoneSystem_.on("Arena_Isis", ZoneEvent::Enter,
		[](Room& room, Zone& zone, uint32 playerId, Object* /*obj*/) {
			room.onArenaIsisEnter(zone, playerId);
		});
}

// Triggered once when a player first enters the mid-boss arena. Builds the rear
// virtual walls from named markers, logs the boss spawn point, and tells clients
// to build the walls locally (S_ZoneState). One-shot (zone is disarmed).
void Room::onArenaHobgoblinEnter(Zone& zone, uint32 playerId) {
	std::cout << "[Zone] '" << zone.tag() << "' ENTER by player " << playerId << '\n';

	if (!worldTerrain_) return;

	// Rear walls: build a Static collider from each named "Wall" marker.
	// 동시에 Wall 위치를 누적해 BossSpawn 마커가 없을 때 fallback 스폰 중점으로 쓴다.
	mu::Vec3 wallSum{};
	int      wallCount = 0;
	for (const auto& m : worldTerrain_->markers()) {
		if (m.type != "Wall") continue;
		if (m.name != "WallHobgoblin_0" && m.name != "WallHobgoblin_1") continue;
		spawnBarrierFromMarker(m);
		wallSum += m.pos;
		++wallCount;
		std::cout << "[Zone] wall built: '" << m.name << "' at ("
		          << m.pos.x() << ", " << m.pos.y() << ", " << m.pos.z() << ")\n";
	}

	// Mid-boss encounter: dynamically spawn the boss + squads, then notify clients
	// (S_NpcSpawnBatch) so they instantiate the goblins. Guarded so a re-entry
	// can't double-spawn (zone is one-shot anyway).
	if (tacticalNpcs_.empty() && !platoonLeader_) {
		// Spawn center: prefer a "BossSpawn" marker; fall back to the Wall midpoint
		// when the level has no BossSpawn marker authored.
		mu::Vec3 spawnPos{};
		bool     haveSpawnPos = false;
		for (const auto& m : worldTerrain_->markers()) {
			if (m.type != "BossSpawn") continue;
			spawnPos     = m.pos;
			haveSpawnPos = true;
			std::cout << "[Zone] Hobgoblin spawn point '" << m.name << "' at ("
			          << m.pos.x() << ", " << m.pos.y() << ", " << m.pos.z() << ")\n";
			break;   // 첫 BossSpawn 마커만 사용
		}
		if (!haveSpawnPos && wallCount > 0) {
			spawnPos     = wallSum / static_cast<float>(wallCount);
			haveSpawnPos = true;
			std::cout << "[Zone] Hobgoblin spawn point (fallback: Wall 중점) at ("
			          << spawnPos.x() << ", " << spawnPos.y() << ", " << spawnPos.z() << ")\n";
		}

		if (haveSpawnPos) {
			// Boss reuses the regular goblin model: spawnCenter == bossPos.
			spawnTacticalGoblinEncounter(spawnPos, spawnPos, /*numSquads*/3, /*troopersPerSquad*/20);

			std::vector<ObjectInfo> spawnInfos;
			spawnInfos.reserve(tacticalNpcs_.size() + 1);
			auto appendInfo = [&](const TacticalNpc& o) {
				spawnInfos.push_back(ObjectInfo{
					.type           = ObjectType::Goblin,
					.objectId       = static_cast<uint16>(o.getId()),
					.materialSetIdx = 0,
					.hp             = o.hp(),
					.maxHp          = o.maxHp(),
					.pos            = o.pos().getXmf(),
					.orient         = o.orient().getXmf(),
					.scale          = o.scale().getXmf(),
				});
			};
			for (const auto& npc : tacticalNpcs_)
				if (npc) appendInfo(*npc);
			if (platoonLeader_) appendInfo(*platoonLeader_);

			broadcast(PacketManager::makeSNpcSpawnBatchPacket(spawnInfos));
		}
	}

	// Clients build the same walls locally so the predicted player collides.
	broadcast(PacketManager::makeSZoneStatePacket(static_cast<uint16>(zone.id()), uint8(1)));

	zone.setArmed(false);   // one-shot trigger
}

// Arena_Hobgoblin과 동일 패턴: GrandBaum 마커(WallGrandBaum_0/1, BossSpawn)로 벽/스폰점을
// 구성하고 GrandBaum 인카운터를 동적 스폰 후 클라에 통지(S_NpcSpawnBatch). 일회성(zone disarm).
void Room::onArenaGrandBaumEnter(Zone& zone, uint32 playerId) {
	std::cout << "[Zone] '" << zone.tag() << "' ENTER by player " << playerId << '\n';

	if (!worldTerrain_) return;

	// 후방 벽: 명명된 "Wall" 마커로 Static collider 구성 + BossSpawn 없을 때 fallback 중점 누적.
	mu::Vec3 wallSum{};
	int      wallCount = 0;
	for (const auto& m : worldTerrain_->markers()) {
		if (m.type != "Wall") continue;
		if (m.name != "WallGrandBaum_0" && m.name != "WallGrandBaum_1") continue;
		spawnBarrierFromMarker(m);
		wallSum += m.pos;
		++wallCount;
		std::cout << "[Zone] wall built: '" << m.name << "' at ("
		          << m.pos.x() << ", " << m.pos.y() << ", " << m.pos.z() << ")\n";
	}

	if (tacticalNpcs_.empty() && !platoonLeader_) {
		mu::Vec3 spawnPos{};
		bool     haveSpawnPos = false;
		for (const auto& m : worldTerrain_->markers()) {
			if (m.type != "BossSpawn") continue;
			spawnPos     = m.pos;
			haveSpawnPos = true;
			std::cout << "[Zone] GrandBaum spawn point '" << m.name << "' at ("
			          << m.pos.x() << ", " << m.pos.y() << ", " << m.pos.z() << ")\n";
			break;
		}
		if (!haveSpawnPos && wallCount > 0) {
			spawnPos     = wallSum / static_cast<float>(wallCount);
			haveSpawnPos = true;
			std::cout << "[Zone] GrandBaum spawn point (fallback: Wall 중점) at ("
			          << spawnPos.x() << ", " << spawnPos.y() << ", " << spawnPos.z() << ")\n";
		}

		if (haveSpawnPos) {
			spawnGrandBaumEncounter(spawnPos, spawnPos);

			std::vector<ObjectInfo> spawnInfos;
			spawnInfos.reserve(tacticalNpcs_.size() + 1);
			auto appendInfo = [&](const TacticalNpc& o) {
				spawnInfos.push_back(ObjectInfo{
					.type           = ObjectType::Goblin,
					.objectId       = static_cast<uint16>(o.getId()),
					.materialSetIdx = 0,
					.hp             = o.hp(),
					.maxHp          = o.maxHp(),
					.pos            = o.pos().getXmf(),
					.orient         = o.orient().getXmf(),
					.scale          = o.scale().getXmf(),
				});
			};
			for (const auto& npc : tacticalNpcs_)
				if (npc) appendInfo(*npc);
			if (platoonLeader_) appendInfo(*platoonLeader_);

			broadcast(PacketManager::makeSNpcSpawnBatchPacket(spawnInfos));
		}
	}

	broadcast(PacketManager::makeSZoneStatePacket(static_cast<uint16>(zone.id()), uint8(1)));

	zone.setArmed(false);   // one-shot trigger
}

// Arena_GrandBaum과 동일 패턴: Isis 마커(WallIsis_0/1, BossSpawn)로 벽/스폰점을 구성하고 Isis
// 인카운터를 동적 스폰 후 클라에 통지(S_NpcSpawnBatch). 일회성(zone disarm). 디버그 트리거(홉고블린
// zone 재사용) 시에는 WallIsis 마커가 없어 벽은 생략되고, 공용 BossSpawn 마커를 스폰점으로 쓴다.
void Room::onArenaIsisEnter(Zone& zone, uint32 playerId) {
	std::cout << "[Zone] '" << zone.tag() << "' ENTER by player " << playerId << " (Isis)\n";

	if (!worldTerrain_) return;

	// 후방 벽(있으면): 명명된 "Wall" 마커로 Static collider 구성 + BossSpawn 없을 때 fallback 중점 누적.
	mu::Vec3 wallSum{};
	int      wallCount = 0;
	for (const auto& m : worldTerrain_->markers()) {
		if (m.type != "Wall") continue;
		if (m.name != "WallIsis_0" && m.name != "WallIsis_1") continue;
		spawnBarrierFromMarker(m);
		wallSum += m.pos;
		++wallCount;
		std::cout << "[Zone] wall built: '" << m.name << "' at ("
		          << m.pos.x() << ", " << m.pos.y() << ", " << m.pos.z() << ")\n";
	}

	if (tacticalNpcs_.empty() && !platoonLeader_) {
		mu::Vec3 spawnPos{};
		bool     haveSpawnPos = false;
		for (const auto& m : worldTerrain_->markers()) {
			if (m.type != "BossSpawn") continue;
			spawnPos     = m.pos;
			haveSpawnPos = true;
			std::cout << "[Zone] Isis spawn point '" << m.name << "' at ("
			          << m.pos.x() << ", " << m.pos.y() << ", " << m.pos.z() << ")\n";
			break;
		}
		if (!haveSpawnPos && wallCount > 0) {
			spawnPos     = wallSum / static_cast<float>(wallCount);
			haveSpawnPos = true;
			std::cout << "[Zone] Isis spawn point (fallback: Wall 중점) at ("
			          << spawnPos.x() << ", " << spawnPos.y() << ", " << spawnPos.z() << ")\n";
		}

		if (haveSpawnPos) {
			spawnIsisEncounter(spawnPos, spawnPos);

			std::vector<ObjectInfo> spawnInfos;
			spawnInfos.reserve(tacticalNpcs_.size() + 1);
			auto appendInfo = [&](const TacticalNpc& o) {
				spawnInfos.push_back(ObjectInfo{
					.type           = ObjectType::Goblin,
					.objectId       = static_cast<uint16>(o.getId()),
					.materialSetIdx = 0,
					.hp             = o.hp(),
					.maxHp          = o.maxHp(),
					.pos            = o.pos().getXmf(),
					.orient         = o.orient().getXmf(),
					.scale          = o.scale().getXmf(),
				});
			};
			for (const auto& npc : tacticalNpcs_)
				if (npc) appendInfo(*npc);
			if (platoonLeader_) appendInfo(*platoonLeader_);

			broadcast(PacketManager::makeSNpcSpawnBatchPacket(spawnInfos));
		}
	}

	broadcast(PacketManager::makeSZoneStatePacket(static_cast<uint16>(zone.id()), uint8(1)));

	zone.setArmed(false);   // one-shot trigger
}

// Creates a Static cube collider matching a marker's world transform. Pure
// collider: no object id, not registered in objectById_, not networked.
void Room::spawnBarrierFromMarker(const MarkerDef& m) {
	if (!assetManager_) return;

	auto bar = std::make_unique<Cube>();
	bar->setModel(assetManager_->modelCube());   // unit cube BVH -> scaled collision shape
	bar->setFaction(Faction::Neutral);
	bar->setScale(m.scale);
	bar->setOrient(m.orient);
	bar->setPos(m.pos);
	bar->body().setMotionType(MotionType::Static);
	bar->body().snapToCurrent();

	Cube* p = bar.get();
	physicsWorld_.registerBody(&p->body(), [p]() { p->rebuildBodyBVH(); });
	barriers_.push_back(std::move(bar));
}

void Room::update() {
	static constexpr Milliseconds dt = 1s / 60.f;	// 60fps
	static constexpr Seconds dtSec   = 1s / 60.f;

	physicsWorld_.step(dtSec);
	rebuildLivingPlayersCache();   // NPC 분리력 컬링이 최신 플레이어 위치를 쓰도록 AI보다 먼저
	rebuildNpcNeighbors();         // ① 플레이어 근접 컬링 → ② SAP로 이웃 인접 리스트 구축
	zoneSystem_.update(*this, dtSec);
	updateGoblinAI(dt);
	updateTacticalAI(dt);

	updatePlayerAnimations(dt);
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

	// livingPlayersCache_는 Room::update()에서 이미 이번 프레임용으로 갱신됨.
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
	}

	// ── Strongholds: structure destruction/rebuild + population maintenance ──
	const Seconds dtSec = std::chrono::duration_cast<Seconds>(dt);
	std::vector<uint32> revivedIds;
	for (auto& sh : strongholds_) {
		if (sh.updateStructure(dtSec)) {
			broadcast(PacketManager::makeSStrongholdStatePacket(
				static_cast<uint16>(sh.getId()),
				sh.hp(),
				sh.isDestroyed() ? uint8(1) : uint8(0)));
		}
		sh.updatePopulation(dtSec, goblins_, *this, revivedIds);
	}
	for (uint32 id : revivedIds) {
		Object* o = (id < objectById_.size()) ? objectById_[id] : nullptr;
		if (!o) continue;
		broadcast(PacketManager::makeSNpcRespawnPacket(
			static_cast<uint16>(id), o->hp(), o->pos().getXmf()));
	}

	if (!moveInfos.empty())
		broadcast(PacketManager::makeSNpcMoveBatchPacket(moveInfos));
}

void Room::updatePlayerAnimations(Milliseconds dt) {
	static constexpr float kWalkThreshold = 0.3f;
	for (auto* s : sessions_) {
		auto* player = s->player();
		const float speed = player->linearVel().len();
		player->animController().switchClip(speed > kWalkThreshold ? "Run_Forward" : "Idle");
		player->updateAnimBones(dt);
	}
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

bool MU_CALLCONV Room::isNearAnyPlayer(mu::Vec3 p) const {
	const float r2 = NPC_SEPARATION_RELEVANCE_RADIUS * NPC_SEPARATION_RELEVANCE_RADIUS;
	for (auto* s : livingPlayersCache_)   // P ≤ 4
		if ((s->player()->pos() - p).len2() < r2) return true;
	return false;
}

// 매 프레임: ① 플레이어 근접 컬링으로 분리력 계산이 필요한 NPC만 추리고, ② 그들만 SAP에
// 넣어 broad phase로 이웃쌍을 산출 → id별 이웃 위치 인접 리스트(npcNeighbors_) 구축.
// findNearbyNpcPositions가 이 리스트를 조회한다. (호출 시점에 NPC 위치는 불변이므로
// goblin/tactical 양쪽 AI 패스에서 동일 리스트를 안전하게 재사용.)
void Room::rebuildNpcNeighbors() {
	// ① 플레이어 근접 컬링: 관련 있는 살아있는 NPC만 SAP 입력에 넣는다.
	npcBroad_.clear();
	auto consider = [&](Object* o) {
		if (o && o->hp() > 0 && isNearAnyPlayer(o->pos()))
			npcBroad_.add(&o->body());
	};
	for (auto& g : goblins_)      consider(&g);
	for (auto& n : tacticalNpcs_) consider(n.get());
	if (platoonLeader_)           consider(platoonLeader_.get());

	// ② SAP broad phase로 이웃쌍 산출 → 인접 리스트 구축 (capacity 재사용).
	npcBroad_.update();
	const auto pairs = npcBroad_.queryPairs();
	for (auto& [id, v] : npcNeighbors_) v.clear();
	for (const auto& [a, b] : pairs) {
		Object* oa = npcBodyOwner_[a];
		Object* ob = npcBodyOwner_[b];   // 컬링 통과분이라 둘 다 생존 보장
		npcNeighbors_[oa->getId()].push_back(ob->pos());
		npcNeighbors_[ob->getId()].push_back(oa->pos());
	}
}

void MU_CALLCONV Room::findNearbyNpcPositions(mu::Vec3 from, float radius,
                                               uint32 excludeId,
                                               std::vector<mu::Vec3>& out) const {
	// 이웃 후보는 rebuildNpcNeighbors가 SAP로 미리 구축한 인접 리스트(fatMargin 반경의
	// superset). 여기서는 각 호출의 실제 radius로 정밀 거리 필터만 한다(기존과 동일 검사).
	auto it = npcNeighbors_.find(excludeId);
	if (it == npcNeighbors_.end()) return;
	const float r2 = radius * radius;
	for (const mu::Vec3& p : it->second)
		if ((p - from).len2() < r2) out.push_back(p);
}

void MU_CALLCONV Room::findNearbyBlockerPositions(mu::Vec3 from, float radius,
                                                  std::vector<mu::Vec3>& out) const {
	// 전술 NPC가 슬롯 이동 중 우회해야 할 큰 장애물: 플레이어 + 생존 중인 보스.
	const float r2 = radius * radius;
	for (GameSession* s : livingPlayersCache_)
		if (s && (s->player()->pos() - from).len2() < r2) out.push_back(s->player()->pos());
	if (platoonLeader_ && platoonLeader_->hp() > 0 &&
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
	player->setFaction(Faction::Players);
	player->setModel(RoomManager::playerModelData());
	player->setPos(playerStarts_[sessions_.size() % playerStarts_.size()].pos());	// 새로 들어오는 플레이어는 playerStarts_에서 순서대로 위치를 받는다.
	player->setOrient(playerStarts_[sessions_.size() % playerStarts_.size()].orient());
	player->setScale(playerStarts_[sessions_.size() % playerStarts_.size()].scale());
	player->body().setMotionType(MotionType::Kinematic);
	player->body().setCollisionCategory(CollisionLayer::Player);   // 전술 NPC가 통과하도록 식별
	player->body().snapToCurrent();
	player->setHp(kPlayerMaxHp);   // authoritative HP init (base Object default is unbounded)
	physicsWorld_.registerBody(&player->body(), [player]() { player->rebuildBodyBVH(); });

	if (const auto* anims = RoomManager::playerAnimations()) {
		player->animController().registerClip("Idle",         findServerAnimClip(*anims, "Player_Idle0"));
		player->animController().registerClip("Run_Forward",  findServerAnimClip(*anims, "Player_Run_Forward"));
		player->animController().registerClip("Run_Backward", findServerAnimClip(*anims, "Player_Run_Backward"));
		player->animController().registerClip("Run_Left",     findServerAnimClip(*anims, "Player_Run_Left"));
		player->animController().registerClip("Run_Right",    findServerAnimClip(*anims, "Player_Run_Right"));
		player->animController().switchClip("Idle");
	}

	// 새로 들어오는 플레이어에 대한 snapshot 만들기
	auto newPlayerInfo = PlayerInfo{
		.playerId = static_cast<uint16>(session->id()),
		.materialSetIdx = 0,
		.hp = kPlayerMaxHp,
		.maxHp = kPlayerMaxHp,
		.pos = player->pos().getXmf(),
		.orient = player->orient().getXmf(),
		.scale = player->scale().getXmf(),
	};

	// 기존 플레이어들 및 기타 object들에 대한 snapshot 만들기
	auto objInfos = std::vector<ObjectInfo>();
	objInfos.reserve(sessions_.size() + cubes_.size() + tacticalNpcs_.size() + 1);

	for (auto session : sessions_) {
		auto playerInfo = ObjectInfo{
			.type = ObjectType::Player,
			.objectId = static_cast<uint16>(session->id()),
			.materialSetIdx = 0,
			.hp = session->player()->hp(),
			.maxHp = kPlayerMaxHp,
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
			.hp = g.hp(),
			.maxHp = g.maxHp(),
			.pos = g.pos().getXmf(),
			.orient = g.orient().getXmf(),
			.scale = g.scale().getXmf(),
		});
	}

	for (const auto& sh : strongholds_) {
		objInfos.push_back(ObjectInfo{
			.type = ObjectType::Stronghold,
			.objectId = static_cast<uint16>(sh.getId()),
			.materialSetIdx = 0,
			.hp = sh.hp(),
			.maxHp = sh.strongholdMaxHp(),
			.pos = sh.pos().getXmf(),
			.orient = sh.orient().getXmf(),
			.scale = sh.scale().getXmf(),
		});
	}

	// 이미 전술 전투가 시작된 뒤 접속한 플레이어도 무리를 보도록 스냅샷에 포함.
	// 보스도 일반 고블린 모델(ObjectType::Goblin)로 전송.
	for (const auto& npc : tacticalNpcs_) {
		if (!npc) continue;
		objInfos.push_back(ObjectInfo{
			.type = ObjectType::Goblin,
			.objectId = static_cast<uint16>(npc->getId()),
			.materialSetIdx = 0,
			.hp = npc->hp(),
			.maxHp = npc->maxHp(),
			.pos = npc->pos().getXmf(),
			.orient = npc->orient().getXmf(),
			.scale = npc->scale().getXmf(),
		});
	}
	if (platoonLeader_) {
		objInfos.push_back(ObjectInfo{
			.type = ObjectType::Goblin,
			.objectId = static_cast<uint16>(platoonLeader_->getId()),
			.materialSetIdx = 0,
			.hp = platoonLeader_->hp(),
			.maxHp = platoonLeader_->maxHp(),
			.pos = platoonLeader_->pos().getXmf(),
			.orient = platoonLeader_->orient().getXmf(),
			.scale = platoonLeader_->scale().getXmf(),
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
	registerObject(player);
}

void Room::leave(GameSession* session) {
	// Terminate this player's active skills before dropping the object, so a
	// lingering instance can't rebind to a recycled object id (new player).
	{
		SkillDispatchContext ctx{ &skillEvList_, objectById_.data(), static_cast<int>(objectById_.size()) };
		skillSystem_.interruptAll(static_cast<i32t>(session->player()->getId()), ctx);
		clearEvents(skillEvList_);
	}

	unregisterObject(session->player());
	physicsWorld_.unregisterBody(&session->player()->body());

	std::erase_if(sessions_, [session](GameSession* s) { return s == session; });
	idSessionMap_.erase(session->id());

	auto leavePkt = PacketManager::makeSLeavePacket(static_cast<uint16>(session->id()));
	broadcast(leavePkt);

	// 세션을 풀로 직접 반환하지 않는다. 세션은 shared_ptr(makeShared, deleter=push)로 소유되므로,
	// 마지막 ref(이 leave 잡의 self 캡처 등)가 소멸할 때 자동으로 ~GameSession + 풀 반환된다.
	// 여기서 push하면 deleter와 겹쳐 double-free가 된다.

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

	// ── 경량 위치 검증 (안티-텔레포트/스피드핵) ─────────────────────────────
	// 클라가 reciprocal soft separation으로 자기 위치를 산출해 보내므로 서버는
	// 그 결과를 신뢰하되, 직전 위치 대비 수평 변위가 물리적으로 불가능한 수준이면
	// 허용치로 클램프한다. (클램프만; 겹침 강제 해소·보정 명령은 하지 않음)
	//   허용치 = (최대 이동속도 + 최대 분리속도) × 최대 패킷 간격(여유 포함)
	//          = (10 m/s + 4 m/s) × 0.5 s = 7 m. 클라 상수와 동기화 필요.
	static constexpr float kMaxMovePerPacket = 7.f;
	const mu::Vec3 oldPos = player->pos();
	mu::Vec3 newPos = DirectX::XMLoadFloat3(&cMvPkt->pos);
	const mu::Vec3 deltaXZ{ newPos.x() - oldPos.x(), 0.f, newPos.z() - oldPos.z() };
	const float horizDist = deltaXZ.len();
	// GrandBaum 넉백 중인 세션은 클램프 면제(클라가 넉백 속도로 빠르게 밀려나므로).
	if (!session->isMoveClampExempt() && horizDist > kMaxMovePerPacket) {
		const float s = kMaxMovePerPacket / horizDist;
		// XZ만 허용치로 클램프, Y(낙하/지형)는 그대로 둔다.
		newPos = mu::Vec3{ oldPos.x() + deltaXZ.x() * s, newPos.y(), oldPos.z() + deltaXZ.z() * s };
		std::cout << "[move() 검증] 비정상 이동 감지 (sessionId: " << sessionId
		          << ", dist: " << horizDist << "m) → 허용치로 클램프\n";
	}

	player->setPos(newPos);
	player->setLinearVel(DirectX::XMLoadFloat3(&cMvPkt->velocity));
	player->setPosUpdateMs(elapsedMs_);

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

// Debug-only: broadcast every active skill hitbox OBB to all clients each frame
// for visualization. Off by default — it allocates + serializes + fans out per
// frame, which is pure overhead in production.
static constexpr bool kBroadcastDebugHitboxes = false;

void Room::updateSkillSystem(Milliseconds dt) {
	if (sessions_.empty()) return;

	SkillDispatchContext ctx {
		&skillEvList_,
		objectById_.data(),
		static_cast<int>(objectById_.size())
	};
	bindGroundQueries(ctx);

	skillSystem_.update(dt, ctx);

	for (const auto* p : skillEvList_) {
		const auto* ev = reinterpret_cast<const BasicEvent*>(p);
		if (ev->type != EventType::SkillHit) continue;

		const auto* hit = reinterpret_cast<const EvSkillHit*>(ev);
		if (hit->targetId < 0 || hit->targetId >= static_cast<int>(objectById_.size())) continue;
		Object* tgt = objectById_[hit->targetId];
		if (!tgt) continue;

		// 받는 피해 배율 적용(기본 1.0; GrandBaum ShieldWall 중 보스/슬라임은 0.1 → 90% 경감).
		int32 dmg   = static_cast<int32>(hit->damage * tgt->damageTakenMultiplier());
		int32 newHp = std::max(tgt->hp() - dmg, 0);
		tgt->setHp(newHp);
		broadcast(PacketManager::makeSSkillHitPacket(
			static_cast<uint16>(hit->attackerId),
			static_cast<uint16>(hit->targetId),
			newHp,
			hit->skillAssetId,
			tgt->linearVel().getXmf()
		));
	}

	clearEvents(skillEvList_);

	if constexpr (kBroadcastDebugHitboxes) {
		std::vector<OBB> activeOBBs;
		skillSystem_.collectActiveOBBs(activeOBBs);
		if (!activeOBBs.empty()) {
			std::vector<OBBInfo> obbInfos;
			obbInfos.reserve(activeOBBs.size());
			for (const OBB& o : activeOBBs)
				obbInfos.push_back({ o.center.getXmf(), o.halfExtents.getXmf(), o.orient.getXmf() });
			broadcast(PacketManager::makeSDebugHitboxPacket(obbInfos.data(), static_cast<uint16>(obbInfos.size())));
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

	// Start server-side skill instance for authoritative hit detection.
	// Reuse skillEvList_ (serialized with updateSkillSystem via the room job queue).
	SkillDispatchContext startCtx{ &skillEvList_, objectById_.data(), static_cast<int>(objectById_.size()) };
	bindGroundQueries(startCtx);
	int instIdx = skillSystem_.startSkill(skillAssetId, static_cast<i32t>(player->getId()),
	                                      startCtx, Milliseconds{ static_cast<float>(elapsedMs) });
	clearEvents(skillEvList_);
	if (instIdx < 0)
		std::cout << "[Room::skillStart] WARNING: startSkill failed (asset id=" << skillAssetId << " not in registry)\n";

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

	// 전술 전투 NPC(중간보스 + 분대)도 평타 대상에 포함(스킬과 달리 레거시 평타는 별도 경로).
	// 되감기 미적용(전술 NPC는 posHistory 없음) — 현재 위치로 판정.
	auto tryMeleeTactical = [&](Object* o) {
		if (!o || o->hp() <= 0) return;
		AABB aabb{ o->pos(), { 1.0f, 2.0f, 1.0f } };
		if (collides(hitbox, aabb).hit) {
			int32 newHp = std::max(o->hp() - kDamage, 0);
			o->setHp(newHp);
			broadcast(PacketManager::makeSHitPacket(static_cast<uint16>(o->getId()), newHp));
		}
	};
	for (auto& npc : tacticalNpcs_) tryMeleeTactical(npc.get());
	tryMeleeTactical(platoonLeader_.get());
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

	// 명령은 위→아래(리더 결정 → 분대 분배 → NPC 실행)로 흐르므로 같은 순서로 업데이트한다.
	// (원본 NPCAI/sim/Room.cpp의 PlatoonLeader → TacticalSquad → TacticalNpc 순서.)
	// 순서가 뒤집히면 order 발동→push→consume가 여러 틱으로 밀려 도착 게이트가 stale 슬롯으로
	// 거짓 양성을 내고 대형(박스 등) 단계가 건너뛰어진다.
	if (platoonLeader_) {
		platoonLeader_->body().setOmega(mu::Vec3{});
		auto result = platoonLeader_->update(dtSec, *this);
		platoonLeader_->updateAnimBones(dtSec);   // 본 갱신 → 정확한 hit BVH(스킬 피격 판정에 필수)
		if (result.justDied) {
			physicsWorld_.unregisterBody(&platoonLeader_->body());
			npcBodyOwner_.erase(&platoonLeader_->body());
		}
		if (platoonLeader_->hp() > 0) {
			moveInfos.push_back({
				static_cast<uint16>(platoonLeader_->getId()),
				platoonLeader_->pos().getXmf(),
				platoonLeader_->orient().getXmf(),
				platoonLeader_->linearVel().getXmf()
			});
		}
		if (!result.hits.empty())
			broadcast(PacketManager::makeSNpcAttackPacket(static_cast<uint16>(platoonLeader_->getId())));
		for (const auto& hit : result.hits)
			broadcast(PacketManager::makeSHitPacket(hit.targetId, hit.newHp));
	}

	for (auto& squad : tacticalSquads_) {
		if (squad) squad->update(dtSec, *this);
	}

	for (auto& npc : tacticalNpcs_) {
		if (!npc) continue;
		npc->body().setOmega(mu::Vec3{});
		auto result = npc->update(dtSec, *this);
		npc->updateAnimBones(dtSec);   // 본 갱신 → 정확한 hit BVH(스킬 피격 판정에 필수)
		if (result.justDied) {
			// 시체가 산 NPC의 이동을 막지 않도록 물리 충돌에서 제거(시각적 시체는 클라가 유지).
			physicsWorld_.unregisterBody(&npc->body());
			npcBodyOwner_.erase(&npc->body());
		}
		if (npc->hp() > 0) {
			moveInfos.push_back({
				static_cast<uint16>(npc->getId()),
				npc->pos().getXmf(),
				npc->orient().getXmf(),
				npc->linearVel().getXmf()
			});
		}
		if (!result.hits.empty())
			broadcast(PacketManager::makeSNpcAttackPacket(static_cast<uint16>(npc->getId())));
		for (const auto& hit : result.hits)
			broadcast(PacketManager::makeSHitPacket(hit.targetId, hit.newHp));
	}

	if (!moveInfos.empty())
		broadcast(PacketManager::makeSNpcMoveBatchPacket(moveInfos));
}

TacticalNpc* Room::findTacticalNpcById(uint32_t id) const {
	for (const auto& npc : tacticalNpcs_)
		if (npc && npc->getId() == id)
			return npc.get();
	return nullptr;
}

void Room::pruneTacticalAttackReservations() {
	for (auto it = tacticalAttackSlots_.begin(); it != tacticalAttackSlots_.end();) {
		uint32_t playerId = it->first;
		GameSession* player = findLivingSessionByPlayerId(static_cast<int32>(playerId));
		auto& reserved = it->second;

		for (auto rit = reserved.begin(); rit != reserved.end();) {
			TacticalNpc* tnpc = findTacticalNpcById(*rit);
			bool remove = !player || !tnpc || tnpc->hp() <= 0 ||
				tnpc->getTargetId() != playerId;

			if (!remove) {
				TacticalNpcState state = tnpc->getState();
				remove = state != TacticalNpcState::PressureWait &&
					state != TacticalNpcState::AttackWindup &&
					state != TacticalNpcState::AttackRecover &&
					state != TacticalNpcState::Chase &&
					state != TacticalNpcState::Flank;
			}

			if (remove)
				rit = reserved.erase(rit);
			else
				++rit;
		}

		if (reserved.empty())
			it = tacticalAttackSlots_.erase(it);
		else
			++it;
	}
}

// 단순 선착순 대신, 플레이어와의 거리 + 접근 진척으로 5칸을 배분한다.
// 교전 중(Windup/Recover) NPC는 항상 점유, 나머지는 가까운 순으로 채우고 먼 예약은 축출.
bool Room::tryReserveTacticalAttackSlot(uint32_t targetId, uint32_t npcId) {
	constexpr int MAX_ATTACKERS = 5;
	const uint32_t playerId = targetId;
	if (playerId == 0 || npcId == 0)
		return false;

	pruneTacticalAttackReservations();

	GameSession* player = findLivingSessionByPlayerId(static_cast<int32>(playerId));
	TacticalNpc* caller = findTacticalNpcById(npcId);
	if (!player || !caller || caller->hp() <= 0 || caller->getTargetId() != playerId)
		return false;

	const mu::Vec3 playerPos = player->player()->pos();
	auto& reserved = tacticalAttackSlots_[playerId];

	// 1) 교전 중 NPC는 슬롯을 점유한 것으로 강제 포함(occupant).
	std::unordered_set<uint32_t> occupied;
	for (const auto& other : tacticalNpcs_) {
		if (!other || other->hp() <= 0) continue;
		if (other->getTargetId() != playerId) continue;
		TacticalNpcState st = other->getState();
		if (st == TacticalNpcState::AttackWindup || st == TacticalNpcState::AttackRecover)
			occupied.insert(other->getId());
	}
	for (uint32_t occupiedId : occupied)
		reserved.insert(occupiedId);

	// 호출자가 이미 교전 중이면 즉시 점유 확정.
	TacticalNpcState callerState = caller->getState();
	if (callerState == TacticalNpcState::AttackWindup || callerState == TacticalNpcState::AttackRecover) {
		reserved.insert(npcId);
		return true;
	}
	if (!caller->isEligibleForAttackReservation(playerId, playerPos))
		return false;

	// 2) Chase/PressureWait/Flank 후보를 플레이어 거리순으로 수집.
	struct ReservationCandidate { uint32_t id{ 0 }; float distSq{ 0.f }; };
	std::vector<ReservationCandidate> candidates;
	candidates.reserve(tacticalNpcs_.size());
	std::unordered_set<uint32_t> candidateIds;

	for (const auto& other : tacticalNpcs_) {
		if (!other || other->hp() <= 0) continue;
		if (other->getTargetId() != playerId) continue;
		uint32_t oid = other->getId();
		if (occupied.count(oid)) continue;
		TacticalNpcState st = other->getState();
		if (st != TacticalNpcState::Chase && st != TacticalNpcState::PressureWait && st != TacticalNpcState::Flank)
			continue;
		if (!other->isEligibleForAttackReservation(playerId, playerPos))
			continue;
		candidates.push_back({ oid, (other->pos() - playerPos).len2() });
		candidateIds.insert(oid);
	}

	std::sort(candidates.begin(), candidates.end(),
		[](const ReservationCandidate& a, const ReservationCandidate& b) {
			if (a.distSq != b.distSq) return a.distSq < b.distSq;
			return a.id < b.id;
		});

	// 3) occupant도 후보도 아닌(상태가 바뀐) 예약은 제거.
	for (auto rit = reserved.begin(); rit != reserved.end();) {
		if (!occupied.count(*rit) && !candidateIds.count(*rit))
			rit = reserved.erase(rit);
		else
			++rit;
	}

	auto candidateForId = [&candidates](uint32_t id) -> const ReservationCandidate* {
		for (const auto& c : candidates)
			if (c.id == id) return &c;
		return nullptr;
	};
	auto findWorstReserved = [&]() {
		uint32_t worstId = 0;
		float worstDistSq = -1.f;
		for (uint32_t rid : reserved) {
			if (occupied.count(rid)) continue;       // occupant은 축출 대상 아님
			const ReservationCandidate* c = candidateForId(rid);
			if (!c) continue;
			if (c->distSq > worstDistSq || (c->distSq == worstDistSq && rid > worstId)) {
				worstDistSq = c->distSq;
				worstId = rid;
			}
		}
		return std::pair<uint32_t, float>{ worstId, worstDistSq };
	};

	// 4) 정원 초과 시 가장 먼 예약자부터 축출.
	while (reserved.size() > static_cast<size_t>(MAX_ATTACKERS)) {
		auto [worstId, worstDist] = findWorstReserved();
		(void)worstDist;
		if (worstId == 0) break;
		reserved.erase(worstId);
	}

	if (reserved.count(npcId))
		return true;

	// 5) 빈 슬롯을 가까운 후보부터 채움.
	for (const auto& c : candidates) {
		if (reserved.size() >= static_cast<size_t>(MAX_ATTACKERS)) break;
		if (reserved.count(c.id)) continue;
		reserved.insert(c.id);
	}

	return reserved.count(npcId) > 0;
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
	if (!assetManager_) return;   // 모델 없이는 충돌 BVH를 만들 수 없음

	auto makeBase = [](mu::Vec3 pos) {
		Object base;
		base.setPos(pos);
		return base;
	};
	// 일반 goblin(setupGoblin)과 동일한 모델/물리 셋업: 충돌 BVH·중력·플레이어 충돌·
	// motor 이동(setDesiredVel)에 필요. tactical NPC/보스 모두 동일하게 적용.
	const auto& anims = assetManager_->goblinAnimations();
	auto registerBody = [&](Object& obj) {
		obj.setId(IdPool::pop());
		obj.setFaction(Faction::Monsters);   // 플레이어 공격의 적대 대상이 되도록(미설정 시 Neutral → 스킬 필터 제외)

		obj.setModel(assetManager_->modelGoblin());
		obj.animController().registerClip("Idle",   findServerAnimClip(anims, "Goblin_Idle"));
		obj.animController().registerClip("Walk",   findServerAnimClip(anims, "Goblin_Walk"));
		obj.animController().registerClip("Attack", findServerAnimClip(anims, "Goblin_Attack"));
		obj.animController().registerClip("Die",    findServerAnimClip(anims, "Goblin_Death"));
		obj.animController().switchClip("Idle");
		obj.setCanReceiveDamage(true);

		obj.body().setMotionType(MotionType::Dynamic);
		obj.body().setMass(70.f);
		obj.body().setLinearDamping(0.1f);
		obj.body().setAngularDamping(25.f);
		obj.body().setRestitution(0.0f);
		obj.body().setUprightStiffness(4000.f);
		obj.body().enableMotor(true);
		obj.body().snapToCurrent();

		Object* raw = &obj;
		physicsWorld_.registerBody(&obj.body(), [raw]() { raw->rebuildBodyBVH(); });
		registerObject(raw);                // objectById_ 등록 → 스킬 히트 판정 타깃 후보에 포함
		npcBodyOwner_[&obj.body()] = raw;   // SAP 쌍(RigidBody*) → NPC 역참조
	};

	TacticalNpcConfig trooperCfg = TacticalGoblin::trooperConfig();
	TacticalNpcConfig bossCfg    = TacticalGoblin::bossConfig();

	constexpr float TACTICAL_SPAWN_RADIUS = 30.f;   // BossSpawn 주변 trooper 산개 반경

	for (int s = 0; s < numSquads; ++s) {
		auto squad = std::make_unique<TacticalSquad>(
			s, trooperCfg.attackRange, trooperCfg.separationRadius);

		for (int t = 0; t < troopersPerSquad; ++t) {
			// 랜덤 배치: spawnCenter 주변 disc 내 무작위 위치(지형 높이 반영).
			mu::Vec3 trooperPos = randomSpawnInDisc(spawnCenter, TACTICAL_SPAWN_RADIUS);

			auto npc = std::make_unique<TacticalNpc>(makeBase(trooperPos), trooperCfg);
			registerBody(*npc);
			// 전술 trooper는 플레이어/보스를 물리적으로 통과(경로 차단 방지). 나머지 충돌은 유지.
			npc->body().setCollisionMask(~(CollisionLayer::Player | CollisionLayer::Boss));
			npc->setSquadId(s);

			squad->addMember(npc.get());
			tacticalNpcs_.push_back(std::move(npc));
		}

		tacticalSquads_.push_back(std::move(squad));
	}

	// 보스는 spawnPos(Wall 마커 등 지형보다 높을 수 있음)를 그대로 받으므로 지형 높이로 보정.
	bossPos = mu::Vec3(bossPos.x(), groundHeightAtWorld(bossPos.x(), bossPos.z()), bossPos.z());
	platoonLeader_ = std::make_unique<PlatoonLeader>(makeBase(bossPos), bossCfg);
	registerBody(*platoonLeader_);
	// 보스는 Boss 카테고리로 식별 → trooper가 보스를 통과(박스 대형 경로 차단 방지). 플레이어와는 충돌 유지.
	platoonLeader_->body().setCollisionCategory(CollisionLayer::Boss);

	for (auto& sq : tacticalSquads_)
		platoonLeader_->addSquad(sq.get());
}

// GrandBaum 중간보스 인카운터 스폰. 슬라임 3부대(0,1,2) + 뱀 1부대(3)를 spawnCenter 주변에
// 산개시키고, bossPos에 GrandBaum 전술(GrandBaumMidBossTactic) 보스를 배치한다.
// NPC 모델/물리 셋업은 일반 goblin과 동일(전용 슬라임/뱀 모델 추가 전까지 goblin 재사용).
void Room::spawnGrandBaumEncounter(mu::Vec3 spawnCenter, mu::Vec3 bossPos)
{
	if (!assetManager_) return;   // 모델 없이는 충돌 BVH를 만들 수 없음

	auto makeBase = [](mu::Vec3 pos) {
		Object base;
		base.setPos(pos);
		return base;
	};
	const auto& anims = assetManager_->goblinAnimations();
	auto registerBody = [&](Object& obj) {
		obj.setId(IdPool::pop());
		obj.setFaction(Faction::Monsters);

		obj.setModel(assetManager_->modelGoblin());
		obj.animController().registerClip("Idle",   findServerAnimClip(anims, "Goblin_Idle"));
		obj.animController().registerClip("Walk",   findServerAnimClip(anims, "Goblin_Walk"));
		obj.animController().registerClip("Attack", findServerAnimClip(anims, "Goblin_Attack"));
		obj.animController().registerClip("Die",    findServerAnimClip(anims, "Goblin_Death"));
		obj.animController().switchClip("Idle");
		obj.setCanReceiveDamage(true);

		obj.body().setMotionType(MotionType::Dynamic);
		obj.body().setMass(70.f);
		obj.body().setLinearDamping(0.1f);
		obj.body().setAngularDamping(25.f);
		obj.body().setRestitution(0.0f);
		obj.body().setUprightStiffness(4000.f);
		obj.body().enableMotor(true);
		obj.body().snapToCurrent();

		Object* raw = &obj;
		physicsWorld_.registerBody(&obj.body(), [raw]() { raw->rebuildBodyBVH(); });
		registerObject(raw);
		npcBodyOwner_[&obj.body()] = raw;
	};

	// 인게임 스케일 config (시뮬값 기반, M3 튜닝 대상)
	TacticalNpcConfig slimeCfg{
		.maxHp = 60.f, .moveSpeed = 4.f, .attackRange = 2.6f, .attackDamage = 8.f,
		.attackWindupTime = 0.35s, .attackRecoverTime = 0.8s,
		.separationRadius = 3.f, .separationWeight = 1.0f
	};
	TacticalNpcConfig snakeCfg{
		.maxHp = 45.f, .moveSpeed = 8.f, .attackRange = 2.6f, .attackDamage = 12.f,
		.attackWindupTime = 0.35s, .attackRecoverTime = 0.8s,
		.separationRadius = 3.f, .separationWeight = 0.9f
	};
	TacticalNpcConfig bossCfg{
		.maxHp = 2000.f, .moveSpeed = 4.f, .attackRange = 3.5f, .attackDamage = 40.f,
		.attackWindupTime = 0.5s, .attackRecoverTime = 1.0s,
		.separationRadius = 4.f, .separationWeight = 0.3f
	};

	constexpr float TACTICAL_SPAWN_RADIUS = 30.f;

	// 부대 구성: 0,1,2 = 슬라임, 3 = 뱀. 인원은 시뮬(A12/B12/C48/D10) 기반(난이도 튜닝 가능).
	struct GrandBaumSquadDef { const TacticalNpcConfig* cfg; int count; };
	const GrandBaumSquadDef squadDefs[] = {
		{ &slimeCfg, 12 },
		{ &slimeCfg, 12 },
		{ &slimeCfg, 48 },
		{ &snakeCfg, 10 },
	};

	for (int s = 0; s < 4; ++s) {
		const TacticalNpcConfig& cfg = *squadDefs[s].cfg;
		auto squad = std::make_unique<TacticalSquad>(s, cfg.attackRange, cfg.separationRadius);

		for (int t = 0; t < squadDefs[s].count; ++t) {
			mu::Vec3 npcPos = randomSpawnInDisc(spawnCenter, TACTICAL_SPAWN_RADIUS);
			auto npc = std::make_unique<TacticalNpc>(makeBase(npcPos), cfg);
			registerBody(*npc);
			// 전술 trooper는 플레이어/보스를 물리적으로 통과(경로 차단 방지). ShieldWall 중 슬라임은
			// setShieldWallBlockers가 Player 충돌을 다시 켜 하드 블로커로 전환한다.
			npc->body().setCollisionMask(~(CollisionLayer::Player | CollisionLayer::Boss));
			npc->setSquadId(s);

			squad->addMember(npc.get());
			tacticalNpcs_.push_back(std::move(npc));
		}

		tacticalSquads_.push_back(std::move(squad));
	}

	bossPos = mu::Vec3(bossPos.x(), groundHeightAtWorld(bossPos.x(), bossPos.z()), bossPos.z());
	platoonLeader_ = std::make_unique<PlatoonLeader>(
		makeBase(bossPos), bossCfg, std::make_unique<GrandBaumMidBossTactic>());
	registerBody(*platoonLeader_);
	platoonLeader_->body().setCollisionCategory(CollisionLayer::Boss);

	for (auto& sq : tacticalSquads_)
		platoonLeader_->addSquad(sq.get());
}

// Isis 중간보스 인카운터. spawnGrandBaumEncounter 미러. 부대 계약: 0,1 = Buddy(2차 돌격 + 보스 합류),
// 2,3 = Bomber(1차 돌격). 모델/ObjectType은 전용 에셋 추가 전까지 goblin 재사용. config/인원은
// 시뮬(Buddy 12/12, Bomber 40/40) 기반 — M3 튜닝/성능 확인 대상.
void Room::spawnIsisEncounter(mu::Vec3 spawnCenter, mu::Vec3 bossPos)
{
	if (!assetManager_) return;   // 모델 없이는 충돌 BVH를 만들 수 없음

	auto makeBase = [](mu::Vec3 pos) {
		Object base;
		base.setPos(pos);
		return base;
	};
	const auto& anims = assetManager_->goblinAnimations();
	auto registerBody = [&](Object& obj) {
		obj.setId(IdPool::pop());
		obj.setFaction(Faction::Monsters);

		obj.setModel(assetManager_->modelGoblin());
		obj.animController().registerClip("Idle",   findServerAnimClip(anims, "Goblin_Idle"));
		obj.animController().registerClip("Walk",   findServerAnimClip(anims, "Goblin_Walk"));
		obj.animController().registerClip("Attack", findServerAnimClip(anims, "Goblin_Attack"));
		obj.animController().registerClip("Die",    findServerAnimClip(anims, "Goblin_Death"));
		obj.animController().switchClip("Idle");
		obj.setCanReceiveDamage(true);

		obj.body().setMotionType(MotionType::Dynamic);
		obj.body().setMass(70.f);
		obj.body().setLinearDamping(0.1f);
		obj.body().setAngularDamping(25.f);
		obj.body().setRestitution(0.0f);
		obj.body().setUprightStiffness(4000.f);
		obj.body().enableMotor(true);
		obj.body().snapToCurrent();

		Object* raw = &obj;
		physicsWorld_.registerBody(&obj.body(), [raw]() { raw->rebuildBodyBVH(); });
		registerObject(raw);
		npcBodyOwner_[&obj.body()] = raw;
	};

	// 인게임 스케일 config (시뮬값 기반, M3 튜닝 대상).
	TacticalNpcConfig buddyCfg{
		.maxHp = 80.f, .moveSpeed = 4.f, .attackRange = 2.6f, .attackDamage = 10.f,
		.attackWindupTime = 0.35s, .attackRecoverTime = 0.8s,
		.separationRadius = 3.f, .separationWeight = 1.0f
	};
	TacticalNpcConfig bomberCfg{
		.maxHp = 45.f, .moveSpeed = 5.f, .attackRange = 2.6f, .attackDamage = 8.f,
		.attackWindupTime = 0.35s, .attackRecoverTime = 0.8s,
		.separationRadius = 3.f, .separationWeight = 0.9f
	};
	TacticalNpcConfig bossCfg{
		.maxHp = 2000.f, .moveSpeed = 4.f, .attackRange = 3.5f, .attackDamage = 40.f,
		.attackWindupTime = 0.5s, .attackRecoverTime = 1.0s,
		.separationRadius = 4.f, .separationWeight = 0.3f
	};

	constexpr float TACTICAL_SPAWN_RADIUS = 30.f;

	// 부대 구성: 0,1 = Buddy, 2,3 = Bomber. 생성 순서가 IsisMidBossTactic의 squad 인덱스 계약을 보장.
	struct IsisSquadDef { const TacticalNpcConfig* cfg; int count; };
	const IsisSquadDef squadDefs[] = {
		{ &buddyCfg,  12 },
		{ &buddyCfg,  12 },
		{ &bomberCfg, 40 },
		{ &bomberCfg, 40 },
	};

	for (int s = 0; s < 4; ++s) {
		const TacticalNpcConfig& cfg = *squadDefs[s].cfg;
		auto squad = std::make_unique<TacticalSquad>(s, cfg.attackRange, cfg.separationRadius);

		for (int t = 0; t < squadDefs[s].count; ++t) {
			mu::Vec3 npcPos = randomSpawnInDisc(spawnCenter, TACTICAL_SPAWN_RADIUS);
			auto npc = std::make_unique<TacticalNpc>(makeBase(npcPos), cfg);
			registerBody(*npc);
			// 전술 trooper는 플레이어/보스를 물리적으로 통과(경로 차단 방지).
			npc->body().setCollisionMask(~(CollisionLayer::Player | CollisionLayer::Boss));
			npc->setSquadId(s);

			squad->addMember(npc.get());
			tacticalNpcs_.push_back(std::move(npc));
		}

		tacticalSquads_.push_back(std::move(squad));
	}

	bossPos = mu::Vec3(bossPos.x(), groundHeightAtWorld(bossPos.x(), bossPos.z()), bossPos.z());
	platoonLeader_ = std::make_unique<PlatoonLeader>(
		makeBase(bossPos), bossCfg, std::make_unique<IsisMidBossTactic>());
	registerBody(*platoonLeader_);
	platoonLeader_->body().setCollisionCategory(CollisionLayer::Boss);

	for (auto& sq : tacticalSquads_)
		platoonLeader_->addSquad(sq.get());
}

// ── GrandBaum ShieldWall 헬퍼 ────────────────────────────────────────────────

void Room::knockPlayersOutOfShieldWall(mu::Vec3 center, float ringRadius) {
	// 인게임 스케일 넉백(시뮬 SHIELD_WALL_KNOCKBACK_*: speed 90 × ~0.4). 이동 권한은 클라에
	// 있으므로 서버는 S_PlayerKnockback만 보내고, 클라가 로컬에서 넉백 + 이동잠금을 실행한다.
	constexpr float  SHIELD_WALL_KNOCKBACK_PADDING = 0.4f;
	constexpr float  SHIELD_WALL_KNOCKBACK_SPEED   = 36.f;
	constexpr uint16 KNOCK_MS    = 320;    // 0.32s 강제 이동
	constexpr uint16 POSTLOCK_MS = 1200;   // 1.2s 입력잠금

	const float safeRadius = ringRadius + SHIELD_WALL_KNOCKBACK_PADDING;

	for (GameSession* s : getLivingPlayers()) {
		if (!s || !s->player()) continue;

		mu::Vec3 p = s->player()->pos();
		mu::Vec3 outDir{ p.x() - center.x(), 0.f, p.z() - center.z() };
		float dist = outDir.len();
		if (dist >= safeRadius) continue;   // 링 밖이면 넉백 불필요

		mu::Vec3 dir = (dist > 0.01f) ? outDir * (1.f / dist) : mu::Vec3{ 1.f, 0.f, 0.f };
		// 안쪽 깊이 낀 플레이어는 0.32초 안에 링 밖으로 밀어내기 위해 필요 속도를 보장.
		float requiredSpeed  = (safeRadius - dist) / (static_cast<float>(KNOCK_MS) / 1000.f);
		float knockbackSpeed = std::max(SHIELD_WALL_KNOCKBACK_SPEED, requiredSpeed);

		s->grantMoveClampExemption(std::chrono::milliseconds(KNOCK_MS + POSTLOCK_MS));

		broadcast(PacketManager::makeSPlayerKnockbackPacket(
			static_cast<uint16>(s->id()), dir.x(), dir.z(), knockbackSpeed, KNOCK_MS, POSTLOCK_MS));
	}
}

// ShieldWall 중 슬라임을 플레이어가 통과 못 하는 하드 블로커로 전환한다. 평소 trooper는 Player를
// 통과(~(Player|Boss))하지만, 여기서 Player 충돌을 다시 켜(~Boss) 링이 벽처럼 막히게 한다.
void Room::setShieldWallBlockers(const std::vector<uint32_t>& blockerIds) {
	clearShieldWallBlockers();   // 이전 블로커가 남아있으면 먼저 원복(중복/매틱 호출 안전)
	for (uint32_t id : blockerIds) {
		TacticalNpc* npc = findTacticalNpcById(id);
		if (!npc) continue;
		npc->body().setCollisionMask(~CollisionLayer::Boss);
	}
	shieldWallBlockerIds_ = blockerIds;
}

void Room::clearShieldWallBlockers() {
	for (uint32_t id : shieldWallBlockerIds_) {
		TacticalNpc* npc = findTacticalNpcById(id);
		if (!npc) continue;
		npc->body().setCollisionMask(~(CollisionLayer::Player | CollisionLayer::Boss));
	}
	shieldWallBlockerIds_.clear();
}

// ── GrandBaum 뱀 증원 웨이브: 동적 소환/디스폰 ───────────────────────────────

// 일반 goblin과 동일한 모델/물리(충돌 BVH·중력·motor) 셋업 후 물리/objectById_에 등록.
// 전술 웨이브 NPC 공용(spawnTacticalGoblinEncounter/spawnGrandBaumEncounter의 registerBody와 동일 셋업).
void Room::registerTacticalNpcBody(Object& obj) {
	const auto& anims = assetManager_->goblinAnimations();
	obj.setId(IdPool::pop());
	obj.setFaction(Faction::Monsters);

	obj.setModel(assetManager_->modelGoblin());
	obj.animController().registerClip("Idle",   findServerAnimClip(anims, "Goblin_Idle"));
	obj.animController().registerClip("Walk",   findServerAnimClip(anims, "Goblin_Walk"));
	obj.animController().registerClip("Attack", findServerAnimClip(anims, "Goblin_Attack"));
	obj.animController().registerClip("Die",    findServerAnimClip(anims, "Goblin_Death"));
	obj.animController().switchClip("Idle");
	obj.setCanReceiveDamage(true);

	obj.body().setMotionType(MotionType::Dynamic);
	obj.body().setMass(70.f);
	obj.body().setLinearDamping(0.1f);
	obj.body().setAngularDamping(25.f);
	obj.body().setRestitution(0.0f);
	obj.body().setUprightStiffness(4000.f);
	obj.body().enableMotor(true);
	obj.body().snapToCurrent();

	Object* raw = &obj;
	physicsWorld_.registerBody(&obj.body(), [raw]() { raw->rebuildBodyBVH(); });
	registerObject(raw);
	npcBodyOwner_[&obj.body()] = raw;
}

TacticalNpc* Room::spawnTacticalWaveNpc(mu::Vec3 pos, const TacticalNpcConfig& cfg, int32 squadId) {
	if (!assetManager_) return nullptr;

	Object base;
	base.setPos(mu::Vec3(pos.x(), groundHeightAtWorld(pos.x(), pos.z()), pos.z()));
	auto npc = std::make_unique<TacticalNpc>(std::move(base), cfg);
	registerTacticalNpcBody(*npc);
	// 평소 trooper처럼 플레이어/보스를 통과(경로 차단 방지).
	npc->body().setCollisionMask(~(CollisionLayer::Player | CollisionLayer::Boss));
	npc->setSquadId(squadId);

	TacticalNpc* raw = npc.get();
	tacticalNpcs_.push_back(std::move(npc));
	return raw;
}

TacticalSquad* Room::addDynamicTacticalSquad(std::unique_ptr<TacticalSquad> squad) {
	TacticalSquad* raw = squad.get();
	tacticalSquads_.push_back(std::move(squad));
	return raw;
}

void Room::removeTacticalNpcById(uint32_t id) {
	for (auto it = tacticalNpcs_.begin(); it != tacticalNpcs_.end(); ++it) {
		if (*it && (*it)->getId() == id) {
			Object* raw = it->get();
			physicsWorld_.unregisterBody(&raw->body());
			npcBodyOwner_.erase(&raw->body());
			unregisterObject(raw);          // objectById_ 슬롯 정리(스킬 히트 후보에서 제외)
			IdPool::push(id);
			tacticalNpcs_.erase(it);
			return;
		}
	}
}

void Room::removeTacticalSquadById(int32 squadId) {
	for (auto it = tacticalSquads_.begin(); it != tacticalSquads_.end(); ++it) {
		if (*it && (*it)->getSquadId() == squadId) {
			tacticalSquads_.erase(it);
			return;
		}
	}
}

void Room::broadcastTacticalNpcSpawn(const std::vector<uint32_t>& npcIds) {
	std::vector<ObjectInfo> spawnInfos;
	spawnInfos.reserve(npcIds.size());
	for (uint32_t id : npcIds) {
		TacticalNpc* o = findTacticalNpcById(id);
		if (!o) continue;
		spawnInfos.push_back(ObjectInfo{
			.type           = ObjectType::Goblin,
			.objectId       = static_cast<uint16>(o->getId()),
			.materialSetIdx = 0,
			.hp             = o->hp(),
			.maxHp          = o->maxHp(),
			.pos            = o->pos().getXmf(),
			.orient         = o->orient().getXmf(),
			.scale          = o->scale().getXmf(),
		});
	}
	if (!spawnInfos.empty()) {
		broadcast(PacketManager::makeSNpcSpawnBatchPacket(spawnInfos));
	}
}

void Room::reviveTacticalNpc(uint32_t id, mu::Vec3 pos) {
	TacticalNpc* npc = findTacticalNpcById(id);
	if (!npc) return;

	pos = mu::Vec3(pos.x(), groundHeightAtWorld(pos.x(), pos.z()), pos.z());
	npc->reviveAt(pos);
	// 사망 시 물리 바디가 제거됐으므로 재등록한다(reviveAt은 객체 상태만 복구).
	physicsWorld_.registerBody(&npc->body(), [npc]() { npc->rebuildBodyBVH(); });
	npcBodyOwner_[&npc->body()] = npc;
	npc->body().snapToCurrent();

	broadcast(PacketManager::makeSNpcRespawnPacket(static_cast<uint16>(id), npc->hp(), pos.getXmf()));
}

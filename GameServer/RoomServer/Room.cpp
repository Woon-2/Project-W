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
#include "TacticalSlime.hpp"
#include "TacticalSnake.hpp"
#include "TacticalBirdy.hpp"
#include "TacticalBomber.hpp"
#include "TacticalTreant.hpp"
#include "GoblinMidBossTactic.hpp"
#include "GrandbaumMidBossTactic.hpp"
#include "IsysMidBossTactic.hpp"
#include "snake.hpp"
#include "mushroom.hpp"
#include "bomber.hpp"
#include "birdy.hpp"
#include "slime.hpp"
#include "treant.hpp"
#include <span>

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

mu::Vec3 MU_CALLCONV Room::randomSpawnInDiscAvoidingProps(
	mu::Vec3 center, float radius, const Object& footprintSource) const
{
	constexpr int kMaxSpawnAttempts = 8;   // fixed budget -> deterministic time per spawn

	const Model* model = footprintSource.model();
	mu::Vec3 candidate{};
	for (int attempt = 0; attempt < kMaxSpawnAttempts; ++attempt) {
		candidate = randomSpawnInDisc(center, radius);
		if (!model || model->bvh.empty()) return candidate;   // no collision shape to test

		const BVH worldBVH = makeWorldBVH(model->bvh, candidate,
		                                  footprintSource.body().orient(),
		                                  footprintSource.body().scale());
		if (!physicsWorld_.overlapsAnyScatterProp(candidate, worldBVH))
			return candidate;
	}
	return candidate;   // all attempts collided (rare) -> fall back to the last
	                    // sample; staticDepenetration resolves residual overlap.
}

bool MU_CALLCONV Room::isTacticalFormationPositionOpen(
	mu::Vec3 candidate, const Object& footprintSource) const
{
	const Model* model = footprintSource.model();
	if (!model || model->bvh.empty()) return true;

	const BVH worldBVH = makeWorldBVH(model->bvh, candidate,
		footprintSource.body().orient(), footprintSource.body().scale());
	return !physicsWorld_.overlapsAnyStaticObstacle(candidate, worldBVH);
}

void Room::setupGoblin(Goblin& g, const Level& level) {
	const auto& anims = level.assetManager->goblinAnimations();
	g.setModel(level.assetManager->modelGoblin());
	g.animController().registerClip("Idle",   findServerAnimClip(anims, "Goblin_Idle"));
	g.animController().registerClip("Walk",   findServerAnimClip(anims, "Goblin_Walk"));
	// 다중공격: 공격 클립을 전부 등록한다(클립 수 = 서로 다른 공격 수, characterSkillMap.hpp 참고).
	// 클라가 레거시 공격 → 스킬 시스템 기반 공격으로 전환 중이며, 완료되면 서버도 attackIndex로
	// 클립을 선택하도록 transitionTo의 switchClip("Attack")를 교체할 예정.
	// TODO(스킬전환): 현재는 임시로 항상 첫 공격 클립(Goblin_Attack1)만 재생한다.
	g.animController().registerClip("Attack1", findServerAnimClip(anims, "Goblin_Attack1"));
	g.animController().registerClip("Attack2", findServerAnimClip(anims, "Goblin_Attack2"));
	g.animController().registerClip("Attack3", findServerAnimClip(anims, "Goblin_Attack3"));
	g.animController().registerClip("Attack",  findServerAnimClip(anims, "Goblin_Attack1")); // 임시 재생용
	g.animController().registerClip("Die",    findServerAnimClip(anims, "Goblin_Death"));
	g.animController().switchClip("Idle");
	g.applyGoblinConfig();
	// Skill-based attacks: (skill asset id, server anim clip key). attackIndex in the
	// lua matches the clip order; AI picks one at random per swing.
	g.addAttack(skillIdByName("Goblin_Attack1"), "Attack1");
	g.addAttack(skillIdByName("Goblin_Attack2"), "Attack2");
	g.addAttack(skillIdByName("Goblin_Attack3"), "Attack3");
	g.setCanReceiveDamage(true);
	g.setKillChargeReward(level.assetManager->chargeConfig().monsterCharge(ObjectType::Goblin));
	g.body().setMotionType(MotionType::Dynamic);
	g.body().setMass(70.f);
	g.body().setLinearDamping(0.1f);
	g.body().setAngularDamping(25.f);
	g.body().setRestitution(0.0f);
	g.body().setUprightStiffness(4000.f);
	g.body().enableMotor(true);
}

void Room::setupSnake(Snake& s, const Level& level) {
	const auto& anims = level.assetManager->snakeAnimations();
	s.setModel(level.assetManager->modelSnake());
	s.animController().registerClip("Idle",   findServerAnimClip(anims, "Snake_Idle"));
	s.animController().registerClip("Walk",   findServerAnimClip(anims, "Snake_Walk"));
	// 다중공격: 클립 전부 등록(현재 Snake는 Attack1 하나). 임시로 첫 클립만 재생. (setupGoblin 주석 참고)
	s.animController().registerClip("Attack1", findServerAnimClip(anims, "Snake_Attack1"));
	s.animController().registerClip("Attack",  findServerAnimClip(anims, "Snake_Attack1")); // 임시 재생용
	s.animController().registerClip("Die",    findServerAnimClip(anims, "Snake_Death"));
	s.animController().switchClip("Idle");
	s.applySnakeConfig();
	s.addAttack(skillIdByName("Snake_Attack1"), "Attack1");
	s.setCanReceiveDamage(true);
	s.setKillChargeReward(level.assetManager->chargeConfig().monsterCharge(ObjectType::Snake));
	s.body().setMotionType(MotionType::Dynamic);
	s.body().setMass(50.f);
	s.body().setLinearDamping(0.1f);
	s.body().setAngularDamping(25.f);
	s.body().setRestitution(0.0f);
	s.body().setUprightStiffness(4000.f);
	s.body().enableMotor(true);
}

void Room::setupMushroom(Mushroom& m, const Level& level) {
	const auto& anims = level.assetManager->mushroomAnimations();
	m.setModel(level.assetManager->modelMushroom());
	m.animController().registerClip("Idle",   findServerAnimClip(anims, "Mushroom_Idle"));
	m.animController().registerClip("Walk",   findServerAnimClip(anims, "Mushroom_Walk"));
	// 다중공격: 클립 전부 등록(Mushroom_Attack1/2). 임시로 첫 클립만 재생. (setupGoblin 주석 참고)
	m.animController().registerClip("Attack1", findServerAnimClip(anims, "Mushroom_Attack1"));
	m.animController().registerClip("Attack2", findServerAnimClip(anims, "Mushroom_Attack2"));
	m.animController().registerClip("Attack",  findServerAnimClip(anims, "Mushroom_Attack1")); // 임시 재생용
	m.animController().registerClip("Die",    findServerAnimClip(anims, "Mushroom_Death"));
	m.animController().switchClip("Idle");
	m.applyMushroomConfig();
	m.addAttack(skillIdByName("Mushroom_Attack1"), "Attack1");
	m.addAttack(skillIdByName("Mushroom_Attack2"), "Attack2");
	m.setCanReceiveDamage(true);
	m.setKillChargeReward(level.assetManager->chargeConfig().monsterCharge(ObjectType::Mushroom));
	m.body().setMotionType(MotionType::Dynamic);
	m.body().setMass(80.f);
	m.body().setLinearDamping(0.1f);
	m.body().setAngularDamping(25.f);
	m.body().setRestitution(0.0f);
	m.body().setUprightStiffness(4000.f);
	m.body().enableMotor(true);
}

void Room::setupBomber(Bomber& b, const Level& level) {
	const auto& anims = level.assetManager->bomberAnimations();
	b.setModel(level.assetManager->modelBomber());
	b.animController().registerClip("Idle",   findServerAnimClip(anims, "Bomber_Idle"));
	b.animController().registerClip("Walk",   findServerAnimClip(anims, "Bomber_Walk"));
	b.animController().registerClip("Attack1", findServerAnimClip(anims, "Bomber_Attack1"));
	b.animController().registerClip("Attack",  findServerAnimClip(anims, "Bomber_Attack1")); // legacy default
	b.animController().registerClip("Die",    findServerAnimClip(anims, "Bomber_Death"));
	b.animController().switchClip("Idle");
	b.applyBomberConfig();
	b.addAttack(skillIdByName("Bomber_Attack1"), "Attack1");
	b.setCanReceiveDamage(true);
	b.setKillChargeReward(level.assetManager->chargeConfig().monsterCharge(ObjectType::Bomber));
	b.body().setMotionType(MotionType::Dynamic);
	b.body().setMass(70.f);
	b.body().setLinearDamping(0.1f);
	b.body().setAngularDamping(25.f);
	b.body().setRestitution(0.0f);
	b.body().setUprightStiffness(4000.f);
	b.body().enableMotor(true);
}

void Room::setupBirdy(Birdy& b, const Level& level) {
	const auto& anims = level.assetManager->birdyAnimations();
	b.setModel(level.assetManager->modelBirdy());
	b.animController().registerClip("Idle",   findServerAnimClip(anims, "Birdy_Idle"));
	b.animController().registerClip("Walk",   findServerAnimClip(anims, "Birdy_Walk"));
	b.animController().registerClip("Attack1", findServerAnimClip(anims, "Birdy_Attack1"));
	b.animController().registerClip("Attack2", findServerAnimClip(anims, "Birdy_Attack2"));
	b.animController().registerClip("Attack",  findServerAnimClip(anims, "Birdy_Attack1")); // legacy default
	b.animController().registerClip("Die",    findServerAnimClip(anims, "Birdy_Death"));
	b.animController().switchClip("Idle");
	b.applyBirdyConfig();
	b.addAttack(skillIdByName("Birdy_Attack1"), "Attack1");
	b.addAttack(skillIdByName("Birdy_Attack2"), "Attack2");
	b.setCanReceiveDamage(true);
	b.setKillChargeReward(level.assetManager->chargeConfig().monsterCharge(ObjectType::Birdy));
	b.body().setMotionType(MotionType::Dynamic);
	b.body().setMass(40.f);
	b.body().setLinearDamping(0.1f);
	b.body().setAngularDamping(25.f);
	b.body().setRestitution(0.0f);
	b.body().setUprightStiffness(4000.f);
	b.body().enableMotor(true);
}

void Room::setupSlime(Slime& s, const Level& level) {
	const auto& anims = level.assetManager->slimeAnimations();
	s.setModel(level.assetManager->modelSlime());
	s.animController().registerClip("Idle",   findServerAnimClip(anims, "Slime_Idle"));
	s.animController().registerClip("Walk",   findServerAnimClip(anims, "Slime_Walk"));
	s.animController().registerClip("Attack1", findServerAnimClip(anims, "Slime_Attack1"));
	s.animController().registerClip("Attack",  findServerAnimClip(anims, "Slime_Attack1")); // legacy default
	s.animController().registerClip("Die",    findServerAnimClip(anims, "Slime_Death"));
	s.animController().switchClip("Idle");
	s.applySlimeConfig();
	s.addAttack(skillIdByName("Slime_Attack1"), "Attack1");
	s.setCanReceiveDamage(true);
	s.setKillChargeReward(level.assetManager->chargeConfig().monsterCharge(ObjectType::Slime));
	s.body().setMotionType(MotionType::Dynamic);
	s.body().setMass(90.f);
	s.body().setLinearDamping(0.1f);
	s.body().setAngularDamping(25.f);
	s.body().setRestitution(0.0f);
	s.body().setUprightStiffness(4000.f);
	s.body().enableMotor(true);
}

void Room::setupTreant(Treant& t, const Level& level) {
	const auto& anims = level.assetManager->treantAnimations();
	t.setModel(level.assetManager->modelTreant());
	t.animController().registerClip("Idle",      findServerAnimClip(anims, "Treant_Idle"));
	t.animController().registerClip("Walk",      findServerAnimClip(anims, "Treant_Walk"));
	t.animController().registerClip("SpinKick",  findServerAnimClip(anims, "Treant_SpinKick"));
	t.animController().registerClip("Clap",      findServerAnimClip(anims, "Treant_Clap"));
	t.animController().registerClip("Punch",     findServerAnimClip(anims, "Treant_Punch"));
	t.animController().registerClip("Attack",    findServerAnimClip(anims, "Treant_SpinKick")); // legacy default
	t.animController().registerClip("Die",       findServerAnimClip(anims, "Treant_Death"));
	t.animController().switchClip("Idle");
	t.applyTreantConfig();
	t.addAttack(skillIdByName("Treant_SpinKick"), "SpinKick");
	t.addAttack(skillIdByName("Treant_Clap"),     "Clap");
	t.addAttack(skillIdByName("Treant_Punch"),    "Punch");
	t.setCanReceiveDamage(true);
	t.setKillChargeReward(level.assetManager->chargeConfig().monsterCharge(ObjectType::Treant));
	t.body().setMotionType(MotionType::Dynamic);
	t.body().setMass(120.f);
	t.body().setLinearDamping(0.1f);
	t.body().setAngularDamping(25.f);
	t.body().setRestitution(0.0f);
	t.body().setUprightStiffness(4000.f);
	t.body().enableMotor(true);
}

void Room::setupStronghold(Stronghold& sh, const StrongholdDef& sd, const Level& level) {
	sh.setModel(level.assetManager->modelStronghold());   // placeholder mesh (cube) -> BVH hit target
	sh.setFaction(Faction::Monsters);     // player skills (hostile to Monsters) can damage it
	sh.setCanReceiveDamage(true);
	sh.setHp(sd.maxHp);
	sh.setOrient(sd.orient);

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
	sh.setPos(mu::Vec3(sd.center.x(), groundY + std::max(0.f, -kGroundBury), sd.center.z()));

	sh.body().setMotionType(MotionType::Static);
}

void Room::init(const Level* levelData) {
	cubes_ = levelData->cubes;
	playerStarts_ = levelData->playerStarts;
	worldTerrain_ = &levelData->terrainChunks;   // shared, read-only (height + stronghold defs)
	assetManager_ = levelData->assetManager;     // backref: cube model for runtime barriers

	// Static prop (tree/rock) colliders for monster-vs-prop authority. Baked from
	// the shared prop BVHs + per-chunk instance data; no room-local bodies needed.
	// Registered before the stronghold spawn loop below so initial spawn positions
	// can already query overlapsAnyScatterProp() (see randomSpawnInDiscAvoidingProps).
	worldTerrain_->registerScatterColliders(physicsWorld_);

	for (auto& c : cubes_) {
		c.body().setMotionType(MotionType::Static);
		c.body().snapToCurrent();
		physicsWorld_.registerBody(&c.body(), [&c]() { c.rebuildBodyBVH(); });
	}

	// ── Strongholds drive monster spawning. Build per-type monster pools (sized to
	//    the sum of per-stronghold target counts) plus one Stronghold structure
	//    per definition. Pools are pre-sized so registerBody/registerObject can
	//    take stable addresses (no reallocation after registration).
	const auto& sdefs = worldTerrain_->strongholds();

	int totalGoblins = 0, totalSnakes = 0, totalMushrooms = 0;
	int totalBombers = 0, totalBirdys = 0, totalSlimes = 0, totalTreants = 0;
	for (const auto& sd : sdefs) {
		for (const auto& pop : sd.populations) {
			switch (pop.type) {
			case ObjectType::Goblin:   totalGoblins   += pop.targetCount; break;
			case ObjectType::Snake:    totalSnakes    += pop.targetCount; break;
			case ObjectType::Mushroom: totalMushrooms += pop.targetCount; break;
			case ObjectType::Bomber:   totalBombers   += pop.targetCount; break;
			case ObjectType::Birdy:    totalBirdys    += pop.targetCount; break;
			case ObjectType::Slime:    totalSlimes    += pop.targetCount; break;
			case ObjectType::Treant:   totalTreants   += pop.targetCount; break;
			default:
				// chunks_index.bin "MonsterType" int -> ObjectType (terrain.cpp). Named
				// bosses (Hobgoblin/Grandbaum/Isys) and out-of-range values are not
				// stronghold-spawnable; warn so an extraction mistake isn't silently dropped.
				std::cout << "[Room::init] WARNING: stronghold population has unsupported "
				             "MonsterType=" << static_cast<int>(pop.type)
				          << " -> ignored (not a stronghold-spawnable monster)\n";
				break;
			}
		}
	}
	goblins_.reserve(static_cast<size_t>(totalGoblins));
	snakes_.reserve(static_cast<size_t>(totalSnakes));
	mushrooms_.reserve(static_cast<size_t>(totalMushrooms));
	bombers_.reserve(static_cast<size_t>(totalBombers));
	birdys_.reserve(static_cast<size_t>(totalBirdys));
	slimes_.reserve(static_cast<size_t>(totalSlimes));
	treants_.reserve(static_cast<size_t>(totalTreants));
	strongholds_.reserve(sdefs.size());

	for (const auto& sd : sdefs) {
		const int groupId = static_cast<int>(npcGroups_.size());
		npcGroups_.emplace_back(
			std::make_unique<NpcGroup>(groupId, sd.center, sd.activityRadius));

		int goblinCount = 0, snakeCount = 0, mushroomCount = 0;
		int bomberCount = 0, birdyCount = 0, slimeCount = 0, treantCount = 0;
		for (const auto& pop : sd.populations) {
			if (pop.type == ObjectType::Goblin)   goblinCount   += pop.targetCount;
			if (pop.type == ObjectType::Snake)     snakeCount    += pop.targetCount;
			if (pop.type == ObjectType::Mushroom)  mushroomCount += pop.targetCount;
			if (pop.type == ObjectType::Bomber)    bomberCount   += pop.targetCount;
			if (pop.type == ObjectType::Birdy)     birdyCount    += pop.targetCount;
			if (pop.type == ObjectType::Slime)     slimeCount    += pop.targetCount;
			if (pop.type == ObjectType::Treant)    treantCount   += pop.targetCount;
		}

		const int goblinStart   = static_cast<int>(goblins_.size());
		const int snakeStart    = static_cast<int>(snakes_.size());
		const int mushroomStart = static_cast<int>(mushrooms_.size());
		const int bomberStart   = static_cast<int>(bombers_.size());
		const int birdyStart    = static_cast<int>(birdys_.size());
		const int slimeStart    = static_cast<int>(slimes_.size());
		const int treantStart   = static_cast<int>(treants_.size());

		auto spawnMonster = [&](auto& pool, auto setupFn) {
			auto& m = pool.emplace_back();
			setupFn(m, *levelData);
			const mu::Vec3 pos = randomSpawnInDiscAvoidingProps(sd.center, sd.spawnRadius, m);
			m.setId(IdPool::pop());
			m.setFaction(Faction::Monsters);
			m.setPos(pos);
			m.setSpawnPos(pos);
			m.setActivityZone(sd.center, sd.activityRadius);
			m.setGroupId(groupId);
		};

		for (int i = 0; i < goblinCount;   ++i) spawnMonster(goblins_,   [this](Goblin&   g, const Level& l) { setupGoblin(g, l);   });
		for (int i = 0; i < snakeCount;    ++i) spawnMonster(snakes_,    [this](Snake&    s, const Level& l) { setupSnake(s, l);    });
		for (int i = 0; i < mushroomCount; ++i) spawnMonster(mushrooms_, [this](Mushroom& m, const Level& l) { setupMushroom(m, l); });
		for (int i = 0; i < bomberCount;   ++i) spawnMonster(bombers_,   [this](Bomber&   b, const Level& l) { setupBomber(b, l);   });
		for (int i = 0; i < birdyCount;    ++i) spawnMonster(birdys_,    [this](Birdy&    b, const Level& l) { setupBirdy(b, l);    });
		for (int i = 0; i < slimeCount;    ++i) spawnMonster(slimes_,    [this](Slime&    s, const Level& l) { setupSlime(s, l);    });
		for (int i = 0; i < treantCount;   ++i) spawnMonster(treants_,   [this](Treant&   t, const Level& l) { setupTreant(t, l);   });

		Stronghold sh{};
		sh.setId(IdPool::pop());
		setupStronghold(sh, sd, *levelData);
		sh.configure(sd, groupId,
		             goblinStart,   goblinCount,
		             snakeStart,    snakeCount,
		             mushroomStart, mushroomCount,
		             bomberStart,   bomberCount,
		             birdyStart,    birdyCount,
		             slimeStart,    slimeCount,
		             treantStart,   treantCount);
		strongholds_.push_back(std::move(sh));
	}

	// Register all monsters after pools are fully built (no reallocation risk).
	npcBroad_.setFatMargin(NPC_SEPARATION_FAT_MARGIN);
	auto registerMonster = [&](auto& m) {
		m.body().snapToCurrent();
		physicsWorld_.registerBody(&m.body(), [&m]() { m.rebuildBodyBVH(); });
		registerObject(&m);
		npcBodyOwner_[&m.body()] = &m;
	};
	for (auto& g : goblins_)   registerMonster(g);
	for (auto& s : snakes_)    registerMonster(s);
	for (auto& m : mushrooms_) registerMonster(m);
	for (auto& b : bombers_)   registerMonster(b);
	for (auto& b : birdys_)    registerMonster(b);
	for (auto& s : slimes_)    registerMonster(s);
	for (auto& t : treants_)   registerMonster(t);

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
// 각 보스는 전용 zone(Arena_Hobgoblin/Arena_Grandbaum/Arena_Isys)과 마커(Wall*/*Spawner)가
// 레벨에 저작돼 있어 해당 아레나 진입만으로 자기 전술 인카운터가 트리거된다.
void Room::bindZoneHandlers() {
	// Mid-boss arena: entering starts the encounter. Designers author a
	// ZoneMarker tagged "Arena_Hobgoblin" (factionMask = Players).
	zoneSystem_.on("Arena_Hobgoblin", ZoneEvent::Enter,
		[](Room& room, Zone& zone, uint32 playerId, Object* /*obj*/) {
			room.onArenaHobgoblinEnter(zone, playerId);
		});

	zoneSystem_.on("Arena_Grandbaum", ZoneEvent::Enter,
		[](Room& room, Zone& zone, uint32 playerId, Object* /*obj*/) {
			room.onArenaGrandbaumEnter(zone, playerId);
		});

	// 정식 Isys zone(레벨 zone tag는 "Arena_Isys" 철자).
	zoneSystem_.on("Arena_Isys", ZoneEvent::Enter,
		[](Room& room, Zone& zone, uint32 playerId, Object* /*obj*/) {
			room.onArenaIsysEnter(zone, playerId);
		});
}

// Triggered once when a player first enters the mid-boss arena. Builds the rear
// virtual walls from named markers, logs the boss spawn point, and tells clients
// to build the walls locally (S_ZoneState). One-shot (zone is disarmed).
void Room::onArenaHobgoblinEnter(Zone& zone, uint32 playerId) {
	std::cout << "[Zone] '" << zone.tag() << "' ENTER by player " << playerId << '\n';

	if (!worldTerrain_) return;

	// 다른 아레나 인카운터가 이미 진행 중이면 이 zone엔 어떤 부수효과(벽/broadcast/disarm)도
	// 남기지 않는다. 한 Room에 동시에 2개 이상의 아레나가 활성화되는 것은 금지된 상태 — 조기
	// 반환으로 zone을 armed 상태로 유지해, 현재 인카운터가 정리된 뒤 재입장 시 정상 트리거되게 한다.
	if (tacticalEncounterActive()) {
		std::cout << "[Zone] '" << zone.tag() << "' skipped - another tactical encounter is active\n";
		return;
	}

	// Wall 마커: 후퇴 차단은 양끝 Wall 일방향 슬랩이 담당(물리 벽 미생성). 마커를 모아 중점을
	// interior 기준점(+스폰 fallback)으로 쓰고, 각 마커를 일방향 슬랩으로 만들어 둔다.
	std::vector<const MarkerDef*> wallMarkers;
	mu::Vec3 wallSum{};
	int      wallCount = 0;
	for (const auto& m : worldTerrain_->markers()) {
		if (m.type != "Wall") continue;
		if (m.name != "WallHobgoblin_0" && m.name != "WallHobgoblin_1") continue;
		wallMarkers.push_back(&m);
		wallSum += m.pos;
		++wallCount;
		std::cout << "[Zone] wall marker: '" << m.name << "' at ("
		          << m.pos.x() << ", " << m.pos.y() << ", " << m.pos.z() << ")\n";
	}
	if (wallCount > 0) {
		const mu::Vec3 mid = wallSum / static_cast<float>(wallCount);
		for (const MarkerDef* wm : wallMarkers) {
			OneWayWall w = makeOneWayWall(*wm, mid);
			arenaWalls_.push_back(w);
			std::cout << "[Zone] one-way wall '" << wm->name << "' outward=("
			          << w.outward.x() << ", " << w.outward.z() << ") halfWidth=" << w.halfWidth << '\n';
		}
	}

	// Mid-boss encounter: dynamically spawn the boss + squads, then notify clients
	// (S_NpcSpawnBatch) so they instantiate the goblins. Guarded so a re-entry
	// can't double-spawn (zone is one-shot anyway).
	if (tacticalNpcs_.empty() && !platoonLeader_) {
		// Spawn center: prefer the "HobgoblinSpawner" marker; fall back to the Wall
		// midpoint when the level has no spawner marker authored.
		mu::Vec3 spawnPos{};
		bool     haveSpawnPos = false;
		for (const auto& m : worldTerrain_->markers()) {
			if (m.type != "HobgoblinSpawner") continue;
			spawnPos     = m.pos;
			haveSpawnPos = true;
			std::cout << "[Zone] Hobgoblin spawn point '" << m.name << "' at ("
			          << m.pos.x() << ", " << m.pos.y() << ", " << m.pos.z() << ")\n";
			break;   // 첫 스포너 마커만 사용
		}
		if (!haveSpawnPos && wallCount > 0) {
			spawnPos     = wallSum / static_cast<float>(wallCount);
			haveSpawnPos = true;
			std::cout << "[Zone] Hobgoblin spawn point (fallback: Wall 중점) at ("
			          << spawnPos.x() << ", " << spawnPos.y() << ", " << spawnPos.z() << ")\n";
		}

		if (haveSpawnPos) {
			// 보스는 Hobgoblin 모델로 스폰(troopers는 일반 goblin): spawnCenter == bossPos.
			spawnTacticalGoblinEncounter(spawnPos, spawnPos, /*numSquads*/3, /*troopersPerSquad*/20);
			broadcastEncounterSpawn();   // NPC별 objType()으로 모델 통지
		}
	}

	// Clients build the same walls locally so the predicted player collides.
	broadcast(PacketManager::makeSZoneStatePacket(static_cast<uint16>(zone.id()), uint8(1)));
	activeArenaZoneId_ = static_cast<uint16>(zone.id());   // 전 NPC 처치 시 이 zone에 S_ZoneState(.,0)로 벽 해제
	arenaWallsActive_  = true;

	zone.setArmed(false);   // one-shot trigger
}

// Arena_Hobgoblin과 동일 패턴: Grandbaum 마커(WallGrandbaum_0/1/2, GrandbaumSpawner)로 벽/스폰점을
// 구성하고 Grandbaum 인카운터를 동적 스폰 후 클라에 통지(S_NpcSpawnBatch). 일회성(zone disarm).
void Room::onArenaGrandbaumEnter(Zone& zone, uint32 playerId) {
	std::cout << "[Zone] '" << zone.tag() << "' ENTER by player " << playerId << '\n';

	if (!worldTerrain_) return;

	// 다른 아레나 인카운터가 이미 진행 중이면 이 zone엔 어떤 부수효과(벽/broadcast/disarm)도
	// 남기지 않는다. 한 Room에 동시에 2개 이상의 아레나가 활성화되는 것은 금지된 상태 — 조기
	// 반환으로 zone을 armed 상태로 유지해, 현재 인카운터가 정리된 뒤 재입장 시 정상 트리거되게 한다.
	if (tacticalEncounterActive()) {
		std::cout << "[Zone] '" << zone.tag() << "' skipped - another tactical encounter is active\n";
		return;
	}

	// Wall 마커: 물리 벽 미생성(후퇴 차단은 양끝 Wall 일방향 슬랩) + 중점은 interior 기준점·스폰 fallback.
	std::vector<const MarkerDef*> wallMarkers;
	mu::Vec3 wallSum{};
	int      wallCount = 0;
	for (const auto& m : worldTerrain_->markers()) {
		if (m.type != "Wall") continue;
		if (m.name != "WallGrandbaum_0" && m.name != "WallGrandbaum_1"
		    && m.name != "WallGrandbaum_2") continue;
		wallMarkers.push_back(&m);
		wallSum += m.pos;
		++wallCount;
		std::cout << "[Zone] wall marker: '" << m.name << "' at ("
		          << m.pos.x() << ", " << m.pos.y() << ", " << m.pos.z() << ")\n";
	}
	if (wallCount > 0) {
		const mu::Vec3 mid = wallSum / static_cast<float>(wallCount);
		for (const MarkerDef* wm : wallMarkers) {
			OneWayWall w = makeOneWayWall(*wm, mid);
			arenaWalls_.push_back(w);
			std::cout << "[Zone] one-way wall '" << wm->name << "' outward=("
			          << w.outward.x() << ", " << w.outward.z() << ") halfWidth=" << w.halfWidth << '\n';
		}
	}

	if (tacticalNpcs_.empty() && !platoonLeader_) {
		mu::Vec3 spawnPos{};
		bool     haveSpawnPos = false;
		for (const auto& m : worldTerrain_->markers()) {
			if (m.type != "GrandbaumSpawner") continue;
			spawnPos     = m.pos;
			haveSpawnPos = true;
			std::cout << "[Zone] Grandbaum spawn point '" << m.name << "' at ("
			          << m.pos.x() << ", " << m.pos.y() << ", " << m.pos.z() << ")\n";
			break;
		}
		if (!haveSpawnPos && wallCount > 0) {
			spawnPos     = wallSum / static_cast<float>(wallCount);
			haveSpawnPos = true;
			std::cout << "[Zone] Grandbaum spawn point (fallback: Wall 중점) at ("
			          << spawnPos.x() << ", " << spawnPos.y() << ", " << spawnPos.z() << ")\n";
		}
		if (haveSpawnPos) {
			spawnGrandbaumEncounter(spawnPos, spawnPos);
			broadcastEncounterSpawn();   // NPC별 objType()으로 모델 통지(슬라임/뱀 + Grandbaum 보스)
		}
	}

	broadcast(PacketManager::makeSZoneStatePacket(static_cast<uint16>(zone.id()), uint8(1)));
	activeArenaZoneId_ = static_cast<uint16>(zone.id());   // 전 NPC 처치 시 이 zone에 S_ZoneState(.,0)로 벽 해제
	arenaWallsActive_  = true;

	zone.setArmed(false);   // one-shot trigger
}

// Arena_Grandbaum과 동일 패턴: Isys 마커(WallIsys_0/1/2, IsysSpawner)로 벽/스폰점을 구성하고 Isys
// 인카운터를 동적 스폰 후 클라에 통지(S_NpcSpawnBatch). 일회성(zone disarm). 디버그 트리거(홉고블린
// zone 재사용) 시에는 WallIsys 마커가 매칭 안 돼 벽은 생략되고, any-Wall/플레이어 위치로 fallback한다.
void Room::onArenaIsysEnter(Zone& zone, uint32 playerId) {
	std::cout << "[Zone] '" << zone.tag() << "' ENTER by player " << playerId << " (Isys)\n";

	if (!worldTerrain_) return;

	// 다른 아레나 인카운터가 이미 진행 중이면 이 zone엔 어떤 부수효과(벽/broadcast/disarm)도
	// 남기지 않는다. 한 Room에 동시에 2개 이상의 아레나가 활성화되는 것은 금지된 상태 — 조기
	// 반환으로 zone을 armed 상태로 유지해, 현재 인카운터가 정리된 뒤 재입장 시 정상 트리거되게 한다.
	if (tacticalEncounterActive()) {
		std::cout << "[Zone] '" << zone.tag() << "' skipped - another tactical encounter is active\n";
		return;
	}

	// Wall 마커(있으면): 물리 벽 미생성(후퇴 차단은 양끝 Wall 일방향 슬랩) + 중점은 interior 기준점·스폰 fallback.
	std::vector<const MarkerDef*> wallMarkers;
	mu::Vec3 wallSum{};
	int      wallCount = 0;
	for (const auto& m : worldTerrain_->markers()) {
		if (m.type != "Wall") continue;
		if (m.name != "WallIsys_0" && m.name != "WallIsys_1"
		    && m.name != "WallIsys_2") continue;
		wallMarkers.push_back(&m);
		wallSum += m.pos;
		++wallCount;
		std::cout << "[Zone] wall marker: '" << m.name << "' at ("
		          << m.pos.x() << ", " << m.pos.y() << ", " << m.pos.z() << ")\n";
	}
	if (wallCount > 0) {
		const mu::Vec3 mid = wallSum / static_cast<float>(wallCount);
		for (const MarkerDef* wm : wallMarkers) {
			OneWayWall w = makeOneWayWall(*wm, mid);
			arenaWalls_.push_back(w);
			std::cout << "[Zone] one-way wall '" << wm->name << "' outward=("
			          << w.outward.x() << ", " << w.outward.z() << ") halfWidth=" << w.halfWidth << '\n';
		}
	}

	if (tacticalNpcs_.empty() && !platoonLeader_) {
		mu::Vec3 spawnPos{};
		bool     haveSpawnPos = false;
		for (const auto& m : worldTerrain_->markers()) {
			if (m.type != "IsysSpawner") continue;
			spawnPos     = m.pos;
			haveSpawnPos = true;
			std::cout << "[Zone] Isys spawn point '" << m.name << "' at ("
			          << m.pos.x() << ", " << m.pos.y() << ", " << m.pos.z() << ")\n";
			break;
		}
		if (!haveSpawnPos && wallCount > 0) {
			spawnPos     = wallSum / static_cast<float>(wallCount);
			haveSpawnPos = true;
			std::cout << "[Zone] Isys spawn point (fallback: Wall 중점) at ("
			          << spawnPos.x() << ", " << spawnPos.y() << ", " << spawnPos.z() << ")\n";
		}
		if (haveSpawnPos) {
			spawnIsysEncounter(spawnPos, spawnPos);
			broadcastEncounterSpawn();   // NPC별 objType()으로 모델 통지(버디/바머 + Isys 보스)
		}
	}

	broadcast(PacketManager::makeSZoneStatePacket(static_cast<uint16>(zone.id()), uint8(1)));
	activeArenaZoneId_ = static_cast<uint16>(zone.id());   // 전 NPC 처치 시 이 zone에 S_ZoneState(.,0)로 벽 해제
	arenaWallsActive_  = true;

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
	updateMonsterAI(dt);
	updateTacticalAI(dt);

	updatePlayerAnimations(dt);
	updateSkillSystem(dt);
	updatePlayerRegen(dt);

	doTimer(dt, [this]() {
		update();
	});
}

void Room::updateMonsterAI(Milliseconds dt) {
	if (sessions_.empty()) return;

	// 경과 시간 누적, NpcGroup 기억 만료 정리, 활동 영역 밖 플레이어 메모리 정리
	elapsedMs_ += dt;
	updateComboExpiry();
	for (auto& grp : npcGroups_) {
		grp->update(elapsedMs_);
		grp->clearMemoryIfPlayerOutside(*this);
	}

	rebuildAggroCount();

	uint64 serverNow = static_cast<uint64>(
		std::chrono::duration_cast<std::chrono::milliseconds>(
			HighResolutionClock::now().time_since_epoch()
		).count()
	);

	// Angular velocity 고정 (물리 solver 누적 방지)
	for (auto& g : goblins_)   g.body().setOmega(mu::Vec3{});
	for (auto& s : snakes_)    s.body().setOmega(mu::Vec3{});
	for (auto& m : mushrooms_) m.body().setOmega(mu::Vec3{});
	for (auto& b : bombers_)   b.body().setOmega(mu::Vec3{});
	for (auto& b : birdys_)    b.body().setOmega(mu::Vec3{});
	for (auto& s : slimes_)    s.body().setOmega(mu::Vec3{});
	for (auto& t : treants_)   t.body().setOmega(mu::Vec3{});

	std::vector<SNpcMoveInfo> moveInfos;
	moveInfos.reserve(goblins_.size() + snakes_.size() + mushrooms_.size()
	                  + bombers_.size() + birdys_.size() + slimes_.size() + treants_.size());

	auto tickPool = [&](auto& pool) {
		for (auto& npc : pool) {
			npc.recordSnapshot(serverNow);
			auto result = npc.update(dt, *this);
			if (npc.hp() > 0) {
				moveInfos.push_back({
					static_cast<uint16>(npc.getId()),
					npc.pos().getXmf(),
					npc.orient().getXmf(),
					npc.linearVel().getXmf()
				});
			}
			if (result.hit) {
				broadcast(PacketManager::makeSNpcAttackPacket(static_cast<uint16>(npc.getId())));
				broadcast(PacketManager::makeSHitPacket(static_cast<uint16>(npc.getId()), result.hit->targetId, result.hit->newHp));
			}
		}
	};

	tickPool(goblins_);
	tickPool(snakes_);
	tickPool(mushrooms_);
	tickPool(bombers_);
	tickPool(birdys_);
	tickPool(slimes_);
	tickPool(treants_);

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
		sh.updatePopulation(dtSec, goblins_, snakes_, mushrooms_,
		                    bombers_, birdys_, slimes_, treants_, *this, revivedIds);
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
	auto countPool = [&](const auto& pool) {
		for (const auto& npc : pool) {
			if (npc.hp() <= 0) continue;
			NpcState s = npc.getState();
			if (s == NpcState::Chase        ||
			    s == NpcState::AttackWindup  ||
			    s == NpcState::AttackRecover ||
			    s == NpcState::Reposition)
				aggroCount_[npc.getTargetId()]++;
		}
	};
	countPool(goblins_);
	countPool(snakes_);
	countPool(mushrooms_);
	countPool(bombers_);
	countPool(birdys_);
	countPool(slimes_);
	countPool(treants_);
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
	for (auto& s : snakes_)       consider(&s);
	for (auto& m : mushrooms_)    consider(&m);
	for (auto& b : bombers_)      consider(&b);
	for (auto& b : birdys_)       consider(&b);
	for (auto& s : slimes_)       consider(&s);
	for (auto& t : treants_)      consider(&t);
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
	// Must be a valid skill target: checkHitboxCollisions() skips objects with
	// canReceiveDamage()==false, so monster (Faction::Monsters) skill hitboxes would
	// otherwise miss the player entirely (only the legacy contact push remains, no
	// damage). See strongholdSystem.md "canReceiveDamage 함정".
	player->setCanReceiveDamage(true);
	player->setModel(RoomManager::playerModelData());
	player->setPos(playerStarts_[sessions_.size() % playerStarts_.size()].pos());	// 새로 들어오는 플레이어는 playerStarts_에서 순서대로 위치를 받는다.
	player->setOrient(playerStarts_[sessions_.size() % playerStarts_.size()].orient());
	player->setScale(playerStarts_[sessions_.size() % playerStarts_.size()].scale());
	player->body().setMotionType(MotionType::Kinematic);
	player->body().setCollisionCategory(CollisionLayer::Player);   // 전술 NPC가 통과하도록 식별
	player->body().snapToCurrent();
	player->setHp(kPlayerMaxHp);   // authoritative HP init (base Object default is unbounded)
	player->resetSkillState();
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
		.weaponType = player->weaponType(),
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
			.weaponType = session->player()->weaponType(),
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
	for (const auto& s : snakes_) {
		objInfos.push_back(ObjectInfo{
			.type = ObjectType::Snake,
			.objectId = static_cast<uint16>(s.getId()),
			.materialSetIdx = 0,
			.hp = s.hp(),
			.maxHp = s.maxHp(),
			.pos = s.pos().getXmf(),
			.orient = s.orient().getXmf(),
			.scale = s.scale().getXmf(),
		});
	}
	for (const auto& m : mushrooms_) {
		objInfos.push_back(ObjectInfo{
			.type = ObjectType::Mushroom,
			.objectId = static_cast<uint16>(m.getId()),
			.materialSetIdx = 0,
			.hp = m.hp(),
			.maxHp = m.maxHp(),
			.pos = m.pos().getXmf(),
			.orient = m.orient().getXmf(),
			.scale = m.scale().getXmf(),
		});
	}
	auto pushMonsterInfo = [&](const auto& pool, ObjectType type) {
		for (const auto& m : pool) {
			objInfos.push_back(ObjectInfo{
				.type = type,
				.objectId = static_cast<uint16>(m.getId()),
				.materialSetIdx = 0,
				.hp = m.hp(),
				.maxHp = m.maxHp(),
				.pos = m.pos().getXmf(),
				.orient = m.orient().getXmf(),
				.scale = m.scale().getXmf(),
			});
		}
	};
	pushMonsterInfo(bombers_, ObjectType::Bomber);
	pushMonsterInfo(birdys_,  ObjectType::Birdy);
	pushMonsterInfo(slimes_,  ObjectType::Slime);
	pushMonsterInfo(treants_, ObjectType::Treant);

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
	// 보스는 platoonLeaderObjType_(전술별로 Hobgoblin/Goblin)로 전송.
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
			.type = platoonLeaderObjType_,
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
	// Grandbaum 넉백 중인 세션은 클램프 면제(클라가 넉백 속도로 빠르게 밀려나므로).
	if (!session->isMoveClampExempt() && horizDist > kMaxMovePerPacket) {
		const float s = kMaxMovePerPacket / horizDist;
		// XZ만 허용치로 클램프, Y(낙하/지형)는 그대로 둔다.
		newPos = mu::Vec3{ oldPos.x() + deltaXZ.x() * s, newPos.y(), oldPos.z() + deltaXZ.z() * s };
		std::cout << "[move() 검증] 비정상 이동 감지 (sessionId: " << sessionId
		          << ", dist: " << horizDist << "m) → 허용치로 클램프\n";
	}

	// ── 아레나 후방 Wall 일방향 클램프(권위 미러) ────────────────────────
	// 전투 활성 중, 양끝 Wall을 바깥으로 통과하려는 플레이어만 평면으로 되돌린다. 안쪽으로
	// 들어오기·측면 이동은 통과(입장 자유). felt collision은 클라 예측(resolveArenaWallLeash)이
	// 담당하고, 여기선 치트 방지용 권위. 넉백 중(isMoveClampExempt)은 면제.
	if (arenaWallsActive_ && !session->isMoveClampExempt()) {
		constexpr float kArenaWallMargin = 0.5f;   // footprint 여유(플레이어 반경 근사)
		for (const OneWayWall& w : arenaWalls_)
			newPos = clampOneWayWall(oldPos, newPos, w, kArenaWallMargin);
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

// 디버그 전용 텔레포트. move()의 7m/패킷 클램프를 우회해 권위 위치를 즉시 옮긴다 → 다음 zoneSystem_.update에서
// 해당 아레나 Enter가 발동(미드보스 전투 테스트). 위치는 S_Move로 다른 클라에 전파한다.
void Room::debugTeleport(int32 sessionId, DirectX::XMFLOAT3 pos) {
	auto session = idSessionMap_[sessionId];
	if (session == nullptr) {
		std::cout << "[ debugTeleport() ] 존재하지 않는 session. sessionId: " << sessionId << '\n';
		return;
	}

	auto player = session->player();
	const mu::Vec3 newPos = DirectX::XMLoadFloat3(&pos);
	player->setPos(newPos);
	player->setLinearVel(mu::Vec3{});
	player->setPosUpdateMs(elapsedMs_);

	std::cout << "[debugTeleport] sessionId: " << sessionId << " -> ("
	          << newPos.x() << ", " << newPos.y() << ", " << newPos.z() << ")\n";

	DirectX::XMFLOAT3 zeroVel{ 0.f, 0.f, 0.f };
	auto sMvPkt = PacketManager::makeSMovePacket(static_cast<uint16>(sessionId), player->pos().getXmf(), zeroVel);
	broadcastExcept(session, sMvPkt);
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

		const int32 prevHp = tgt->hp();
		// 받는 피해 배율 적용(기본 1.0; Grandbaum 평상시 슬라임 0.1, ShieldWall 중 보스 0.1 → 90% 경감).
		const int32 dmg   = static_cast<int32>(hit->damage * tgt->damageTakenMultiplier());
		const int32 newHp = std::max(prevHp - dmg, 0);
		tgt->setHp(newHp);
		noteAndMaybeReward(hit->attackerId, tgt, prevHp, newHp);
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

void Room::skillStart(int32 sessionId, uint32 skillAssetId, uint64 clientMs, uint32 skillSeed) {
	auto sessionIt = idSessionMap_.find(sessionId);
	if (sessionIt == idSessionMap_.end()) return;

	auto* player = sessionIt->second->player();
	if (player->hp() <= 0) return;

	// Stack-charge gate: selectable skills need >=1 stack and an elapsed cooldown.
	// Basic attacks (isBasic) skip the gate. The client predicts the cast locally
	// from its synced charge/cooldown; a rejected cast rolls back via S_SkillUseReject.
	if (const SkillAsset* asset = findSkillAsset(skillAssetId); asset && !asset->isBasic) {
		const int  slot      = asset->loadoutSlot;
		const bool validSlot = (slot >= 0 && slot < Player::kSkillSlots);
		const bool ownsWeapon = static_cast<unsigned>(asset->weaponType)
		                        == static_cast<unsigned>(player->weaponType());
		const Milliseconds now = elapsedMs_;
		const float cost   = asset->chargeCost;
		const int   stacks = (cost > 0.f) ? static_cast<int>(player->skillCharge(slot) / cost) : 0;
		const bool  ready  = validSlot && ownsWeapon && stacks >= 1 && now >= player->cooldownEnd(slot);
		if (!ready) {
			sessionIt->second->send(PacketManager::makeSSkillUseRejectPacket(
				static_cast<uint8>(validSlot ? slot : 0)));
			return;
		}
		player->addSkillCharge(slot, -cost);
		player->setCooldownEnd(slot, now + asset->cooldown);
		broadcast(PacketManager::makeSSkillChargePacket(
			static_cast<uint16>(player->getId()), static_cast<uint8>(slot), player->skillCharge(slot)));
	}

	// Compute elapsed time for lag compensation (same pattern as attack())
	uint64 serverNow = static_cast<uint64>(
		std::chrono::duration_cast<std::chrono::milliseconds>(
			HighResolutionClock::now().time_since_epoch()
		).count()
	);
	uint64 elapsedRaw = (serverNow > clientMs) ? (serverNow - clientMs) : 0u;
	uint16 elapsedMs  = static_cast<uint16>(std::min(elapsedRaw, static_cast<uint64>(65535u)));

	// Start server-side skill instance for authoritative hit detection.
	// skillSeed: caster-generated per-cast seed; drives the deterministic
	// VFXParticle hitbox sampler so server hits match the caster's visuals.
	// Reuse skillEvList_ (serialized with updateSkillSystem via the room job queue).
	SkillDispatchContext startCtx{ &skillEvList_, objectById_.data(), static_cast<int>(objectById_.size()) };
	bindGroundQueries(startCtx);
	int instIdx = skillSystem_.startSkill(skillAssetId, static_cast<i32t>(player->getId()),
	                                      startCtx, Milliseconds{ static_cast<float>(elapsedMs) },
	                                      skillSeed);
	clearEvents(skillEvList_);
	if (instIdx < 0)
		std::cout << "[Room::skillStart] WARNING: startSkill failed (asset id=" << skillAssetId << " not in registry)\n";

	// Broadcast to OTHER clients so they play the visual effect (with the
	// same seed so their layout matches the server's hitboxes).
	broadcastExcept(sessionIt->second,
		PacketManager::makeSSkillStartPacket(
			skillAssetId,
			static_cast<uint16>(player->getId()),
			elapsedMs,
			skillSeed
		)
	);
}

uint32 Room::skillIdByName(std::string_view name) const {
	if (!assetManager_) return 0u;
	for (const SkillAsset& a : assetManager_->skillAssets())
		if (a.name == name) return a.id;
	return 0u;
}

bool Room::npcSkillActive(int32 ownerObjectId) const {
	return skillSystem_.hasActiveSkill(static_cast<i32t>(ownerObjectId));
}

void Room::skillStartInternal(int32 ownerObjectId, uint32 skillAssetId, uint32 skillSeed, float damageScale) {
	if (skillAssetId == 0) return;
	Object* owner = (ownerObjectId >= 0 && ownerObjectId < static_cast<int32>(objectById_.size()))
		? objectById_[ownerObjectId] : nullptr;
	if (!owner || owner->hp() <= 0) return;
	if (skillSystem_.hasActiveSkill(static_cast<i32t>(ownerObjectId))) return;  // don't stack casts

	// Authoritative skill instance (elapsedMs = 0: server casts in real time, no lag comp).
	SkillDispatchContext ctx{ &skillEvList_, objectById_.data(), static_cast<int>(objectById_.size()) };
	bindGroundQueries(ctx);
	int instIdx = skillSystem_.startSkill(skillAssetId, static_cast<i32t>(ownerObjectId),
	                                      ctx, Milliseconds{ 0.f }, skillSeed, damageScale);
	clearEvents(skillEvList_);
	if (instIdx < 0) return;

	// Broadcast to ALL clients (NPC has no session to exclude) so they play the
	// matching VFX + drive the NPC's AnimBlender via the skill's PlayAnimation.
	broadcast(PacketManager::makeSSkillStartPacket(
		skillAssetId,
		static_cast<uint16>(ownerObjectId),
		uint16(0),
		skillSeed));
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

	// Legacy player melee (lag-comp rewind). New monster pools are damageable via the
	// skill system (objectById_), so they are not added to this vestigial path.
	for (auto& goblin : goblins_) {
		if (goblin.hp() <= 0) {
			continue;
		}

		AABB goblinAABB{ goblin.rewindPos(targetMs), {1.0f, 2.0f, 1.0f} };

		if (collides(hitbox, goblinAABB).hit) {
			const int32 prevHp = goblin.hp();
			int32 newHp = std::max(prevHp - kDamage, 0);
			goblin.setHp(newHp);
			noteAndMaybeReward(static_cast<int32>(player->getId()), &goblin, prevHp, newHp);

			broadcast(PacketManager::makeSHitPacket(static_cast<uint16>(player->getId()), static_cast<uint16>(goblin.getId()), newHp));
		}
	}

	// 전술 전투 NPC(중간보스 + 분대)도 평타 대상에 포함(스킬과 달리 레거시 평타는 별도 경로).
	// 되감기 미적용(전술 NPC는 posHistory 없음) — 현재 위치로 판정.
	auto tryMeleeTactical = [&](Object* o) {
		if (!o || o->hp() <= 0) return;
		AABB aabb{ o->pos(), { 1.0f, 2.0f, 1.0f } };
		if (collides(hitbox, aabb).hit) {
			const int32 prevHp = o->hp();
			// 받는 피해 배율 적용(스킬 히트와 동일; Grandbaum 평상시 슬라임/ShieldWall 중 보스 0.1).
			const int32 dmg    = static_cast<int32>(kDamage * o->damageTakenMultiplier());
			const int32 newHp  = std::max(prevHp - dmg, 0);
			o->setHp(newHp);
			noteAndMaybeReward(static_cast<int32>(player->getId()), o, prevHp, newHp);
			broadcast(PacketManager::makeSHitPacket(static_cast<uint16>(player->getId()), static_cast<uint16>(o->getId()), newHp));
		}
	};
	for (auto& npc : tacticalNpcs_) tryMeleeTactical(npc.get());
	tryMeleeTactical(platoonLeader_.get());
}

void Room::selectSkill(int32 sessionId, uint8 slot) {
	auto it = idSessionMap_.find(sessionId);
	if (it == idSessionMap_.end()) return;
	Player* p = it->second->player();
	if (!p || slot >= Player::kSkillSlots) return;
	p->setSelectedSlot(slot);
	// Relay so teammate HUDs mirror the selected skill.
	broadcastExcept(it->second,
		PacketManager::makeSSkillSelectPacket(static_cast<uint16>(p->getId()), slot));
}

const SkillAsset* Room::findSkillAsset(uint32 id) const {
	if (!assetManager_) return nullptr;
	for (const SkillAsset& a : assetManager_->skillAssets())
		if (a.id == id) return &a;
	return nullptr;
}

void Room::noteAndMaybeReward(int32 attackerObjId, Object* target, int32 prevHp, int32 newHp) {
	if (!target || target->killChargeReward() <= 0.f) return;   // not a chargeable monster
	Object* atk = (attackerObjId >= 0 && attackerObjId < static_cast<int32>(objectById_.size()))
		? objectById_[attackerObjId] : nullptr;
	if (!atk || atk->faction() != Faction::Players) return;     // credit player attackers only
	target->noteDamager(attackerObjId, elapsedMs_);
	if (prevHp > 0 && newHp <= 0) distributeKillCharge(target);
}

void Room::distributeKillCharge(Object* monster) {
	if (!assetManager_) return;
	const ChargeConfig& cfg     = assetManager_->chargeConfig();
	const SkillLoadout& loadout = assetManager_->loadout();
	const Milliseconds  now     = elapsedMs_;
	const float         reward  = monster->killChargeReward();

	std::vector<int32> attackers;
	monster->collectRecentDamagers(now, cfg.damageWindow(), attackers);
	for (int32 pid : attackers) {
		GameSession* sess = findLivingSessionByPlayerId(pid);
		if (!sess) continue;
		Player* p = sess->player();
		if (!p) continue;

		// Combo: extend if within the window, else restart at 1.
		const uint16 combo = (now - p->lastCreditMs() <= cfg.comboWindow())
			? static_cast<uint16>(p->comboCount() + 1) : static_cast<uint16>(1);
		p->setComboCount(combo);
		p->setLastCreditMs(now);

		// Credit the selected slot, scaled by the soft cap only. The combo no longer
		// accelerates charge gain — it now drives the player's HP regen (updatePlayerRegen).
		const int      slot      = p->selectedSlot();
		const unsigned w         = static_cast<unsigned>(p->weaponType());
		const float    cost      = loadout.cost(w, slot);
		const int      curStacks = (cost > 0.f) ? static_cast<int>(p->skillCharge(slot) / cost) : 0;
		const float    gain      = reward * cfg.softCapFactor(curStacks);
		p->addSkillCharge(slot, gain);

		broadcast(PacketManager::makeSSkillChargePacket(
			static_cast<uint16>(p->getId()), static_cast<uint8>(slot), p->skillCharge(slot)));
		sess->send(PacketManager::makeSComboStatePacket(
			static_cast<uint16>(p->getId()), combo, cfg.comboWindow().count()));
	}
	monster->clearDamagers();
}

void Room::updateComboExpiry() {
	if (!assetManager_) return;
	const Milliseconds window = assetManager_->chargeConfig().comboWindow();
	const Milliseconds now    = elapsedMs_;
	for (GameSession* s : livingPlayersCache_) {
		Player* p = s->player();
		if (!p || p->comboCount() == 0) continue;
		if (now - p->lastCreditMs() > window) {
			p->setComboCount(0);
			s->send(PacketManager::makeSComboStatePacket(
				static_cast<uint16>(p->getId()), 0, window.count()));
		}
	}
}

void Room::updatePlayerRegen(Milliseconds dt) {
	if (!assetManager_) return;
	const ChargeConfig& cfg   = assetManager_->chargeConfig();
	const float         dtSec = std::chrono::duration<float>(dt).count();

	// Integrate combo-driven regen every tick; the accumulator carries fractional HP.
	for (GameSession* s : livingPlayersCache_) {
		Player* p = s->player();
		if (!p) continue;
		const int32 hp = p->hp();
		if (hp <= 0 || hp >= kPlayerMaxHp) { p->setHpRegenAccum(0.f); continue; }

		float accum = p->hpRegenAccum() + cfg.hpRegenPerSec(p->comboCount()) * dtSec;
		const int32 add = static_cast<int32>(accum);   // floor (accum is non-negative)
		if (add > 0) {
			accum -= static_cast<float>(add);
			p->setHp(std::min(hp + add, kPlayerMaxHp));
		}
		p->setHpRegenAccum(accum);
	}

	// Throttle the HP push to ~10 Hz, and only for players whose HP actually changed.
	regenSyncAccum_ += dt;
	if (regenSyncAccum_ < Milliseconds{ 100.f }) return;
	regenSyncAccum_ = Milliseconds{ 0.f };
	for (GameSession* s : livingPlayersCache_) {
		Player* p = s->player();
		if (!p) continue;
		const int32 hp = p->hp();
		if (hp == p->lastSyncedHp()) continue;
		p->setLastSyncedHp(hp);
		broadcast(PacketManager::makeSPlayerHpPacket(static_cast<uint16>(p->getId()), hp));
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
			broadcast(PacketManager::makeSHitPacket(static_cast<uint16>(platoonLeader_->getId()), hit.targetId, hit.newHp));
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
			broadcast(PacketManager::makeSHitPacket(static_cast<uint16>(npc->getId()), hit.targetId, hit.newHp));
	}

	if (!moveInfos.empty())
		broadcast(PacketManager::makeSNpcMoveBatchPacket(moveInfos));

	// 전 NPC(보스 + 전 부대원) 처치 시 아레나 가상 벽을 1회 해제 → 플레이어 후퇴 허용.
	if (arenaWallsActive_ && allTacticalCombatantsDead()) {
		teardownArenaWalls();
		cleanupTacticalEncounter();   // 다음 아레나가 스폰 가드를 통과하도록 컨테이너 정리
	}
}

// 보스(platoonLeader_)가 사망했고 모든 부대원(tacticalNpcs_)도 사망했는지. 보스가 먼저
// 죽어도 Confused로 살아남은 부대원이 1명이라도 있으면 false(전 NPC 처치 후에만 벽 해제).
bool Room::allTacticalCombatantsDead() const {
	if (!platoonLeader_ || platoonLeader_->hp() > 0) return false;
	for (const auto& npc : tacticalNpcs_)
		if (npc && npc->hp() > 0) return false;
	return true;
}

// 아레나 가상 벽 해제: 서버 배리어 바디를 물리에서 제거(~Room()과 동일 패턴)하고, 클라엔
// S_ZoneState(.,0)을 broadcast한다(클라 onZoneState가 로컬 벽 제거). 1회성(arenaWallsActive_ off).
void Room::teardownArenaWalls() {
	arenaWalls_.clear();
	for (const auto& b : barriers_)
		if (b) physicsWorld_.unregisterBody(&b->body());
	barriers_.clear();
	broadcast(PacketManager::makeSZoneStatePacket(activeArenaZoneId_, uint8(0)));
	arenaWallsActive_ = false;
	std::cout << "[Zone] arena cleared - walls down (zoneId=" << activeArenaZoneId_ << ")\n";
}

// 클리어된 인카운터의 tactical 컨테이너를 전부 비운다. 참조 방향(PlatoonLeader → TacticalSquad
// (raw ptr) → TacticalNpc(raw ptr))을 따라 상위부터 정리해 dangling 참조 가능성을 없앤다.
// physicsWorld_/npcBodyOwner_ 해제는 justDied 시점에 이미 끝났을 것이나, 두 호출 모두
// find-then-erase로 idempotent이므로 방어적으로 다시 호출해도 안전하다.
// nextWedgeChargeId_(단조 증가 ID)/platoonLeaderObjType_/activeArenaZoneId_는 다음 스폰 시 덮어쓰므로
// 건드리지 않는다.
void Room::cleanupTacticalEncounter() {
	if (platoonLeader_) {
		physicsWorld_.unregisterBody(&platoonLeader_->body());
		npcBodyOwner_.erase(&platoonLeader_->body());
		unregisterObject(platoonLeader_.get());
		IdPool::push(platoonLeader_->getId());
		platoonLeader_.reset();
	}

	tacticalSquads_.clear();

	for (auto& npc : tacticalNpcs_) {
		if (!npc) continue;
		physicsWorld_.unregisterBody(&npc->body());
		npcBodyOwner_.erase(&npc->body());
		unregisterObject(npc.get());
		IdPool::push(npc->getId());
	}
	tacticalNpcs_.clear();

	clearShieldWallBlockers();
	tacticalAttackSlots_.clear();
	wedgeHitRecord_.clear();

	std::cout << "[Zone] tactical encounter cleaned up - containers cleared\n";
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
	// 바디/모델/물리 셋업은 registerTacticalNpcBody(type)이 담당. trooper는 Goblin, 보스는 Hobgoblin
	// (둘은 같은 리그를 공유하므로 Goblin_* 클립 재사용).
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
			registerTacticalNpcBody(*npc, ObjectType::Goblin);
			npc->setObjType(ObjectType::Goblin);
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
	platoonLeader_ = std::make_unique<PlatoonLeader>(
		makeBase(bossPos), bossCfg, std::make_unique<GoblinMidBossTactic>());
	registerTacticalNpcBody(*platoonLeader_, ObjectType::Hobgoblin);   // 보스는 Hobgoblin 모델
	platoonLeader_->setObjType(ObjectType::Hobgoblin);
	// 보스는 Boss 카테고리로 식별 → trooper가 보스를 통과(박스 대형 경로 차단 방지). 플레이어와는 충돌 유지.
	platoonLeader_->body().setCollisionCategory(CollisionLayer::Boss);
	platoonLeaderObjType_ = ObjectType::Hobgoblin;   // 클라에 Hobgoblin 모델로 통지(ObjectInfo.type)

	for (auto& sq : tacticalSquads_)
		platoonLeader_->addSquad(sq.get());
}

// Grandbaum 중간보스 인카운터 스폰. 슬라임 3부대(0,1,2) + 뱀 1부대(3)를 spawnCenter 주변에
// 산개시키고, bossPos에 Grandbaum 전술(GrandbaumMidBossTactic) 보스를 배치한다.
// NPC 모델/물리 셋업은 일반 goblin과 동일(전용 슬라임/뱀 모델 추가 전까지 goblin 재사용).
void Room::spawnGrandbaumEncounter(mu::Vec3 spawnCenter, mu::Vec3 bossPos)
{
	if (!assetManager_) return;   // 모델 없이는 충돌 BVH를 만들 수 없음

	auto makeBase = [](mu::Vec3 pos) {
		Object base;
		base.setPos(pos);
		return base;
	};

	// 부대별 config는 몬스터별 Tactical 클래스에서(단일 출처). 보스 config는 인카운터 고유.
	const TacticalNpcConfig slimeCfg = TacticalSlime::trooperConfig();
	TacticalNpcConfig bossCfg{
		.maxHp = 2000.f, .moveSpeed = 4.f, .attackRange = 3.5f, .attackDamage = 40.f,
		.attackWindupTime = 0.5s, .attackRecoverTime = 1.0s,
		.separationRadius = 4.f, .separationWeight = 0.3f,
		.attackDamageScale = 5.0f   // 보스 공격력 레버(Treant 스킬 × scale, 주 위협). attackDamage는 레거시 폴백(미사용). 튜닝값.
	};

	constexpr float TACTICAL_SPAWN_RADIUS = 30.f;

	// 부대 구성: 0~3 모두 슬라임(DR 토글 재설계 — 뱀 부대 제거). 인원 12/12/48/10.
	struct GrandbaumSquadDef { const TacticalNpcConfig* cfg; ObjectType type; int count; };
	const GrandbaumSquadDef squadDefs[] = {
		{ &slimeCfg, ObjectType::Slime, 20 },
		{ &slimeCfg, ObjectType::Slime, 20 },
		{ &slimeCfg, ObjectType::Slime, 20 },
		{ &slimeCfg, ObjectType::Slime, 20 },
	};

	for (int s = 0; s < 4; ++s) {
		const TacticalNpcConfig& cfg = *squadDefs[s].cfg;
		const ObjectType type = squadDefs[s].type;
		auto squad = std::make_unique<TacticalSquad>(s, cfg.attackRange, cfg.separationRadius);

		for (int t = 0; t < squadDefs[s].count; ++t) {
			mu::Vec3 npcPos = randomSpawnInDisc(spawnCenter, TACTICAL_SPAWN_RADIUS);
			auto npc = std::make_unique<TacticalNpc>(makeBase(npcPos), cfg);
			registerTacticalNpcBody(*npc, type);
			npc->setObjType(type);
			// 전술 trooper는 플레이어/보스를 물리적으로 통과(경로 차단 방지). ShieldWall 중 슬라임은
			// setShieldWallBlockers가 Player 충돌을 다시 켜 하드 블로커로 전환한다.
			npc->body().setCollisionMask(~(CollisionLayer::Player | CollisionLayer::Boss));
			// 평상시 슬라임은 받는 피해 90% 경감(단단) → 플레이어가 보스를 공격하도록 유도.
			// ShieldWall 중에는 GrandbaumMidBossTactic이 1.0으로 풀어 취약하게, 종료 시 0.1로 복귀.
			npc->setDamageTakenMultiplier(0.1f);
			npc->setSquadId(s);

			squad->addMember(npc.get());
			tacticalNpcs_.push_back(std::move(npc));
		}

		tacticalSquads_.push_back(std::move(squad));
	}

	bossPos = mu::Vec3(bossPos.x(), groundHeightAtWorld(bossPos.x(), bossPos.z()), bossPos.z());
	platoonLeader_ = std::make_unique<PlatoonLeader>(
		makeBase(bossPos), bossCfg, std::make_unique<GrandbaumMidBossTactic>());
	registerTacticalNpcBody(*platoonLeader_, ObjectType::Grandbaum);   // 전용 Grandbaum 모델
	platoonLeader_->setObjType(ObjectType::Grandbaum);
	platoonLeader_->body().setCollisionCategory(CollisionLayer::Boss);
	platoonLeaderObjType_ = ObjectType::Grandbaum;

	for (auto& sq : tacticalSquads_)
		platoonLeader_->addSquad(sq.get());
}

// Isys 중간보스 인카운터. spawnGrandbaumEncounter 미러. 부대 계약: 0,1 = Buddy(2차 돌격 + 보스 합류),
// 2,3 = Bomber(1차 돌격). 모델/ObjectType은 전용 에셋 추가 전까지 goblin 재사용. config/인원은
// 시뮬(Buddy 12/12, Bomber 40/40) 기반 — M3 튜닝/성능 확인 대상.
void Room::spawnIsysEncounter(mu::Vec3 spawnCenter, mu::Vec3 bossPos)
{
	if (!assetManager_) return;   // 모델 없이는 충돌 BVH를 만들 수 없음

	auto makeBase = [](mu::Vec3 pos) {
		Object base;
		base.setPos(pos);
		return base;
	};

	// Buddy 부대는 Birdy로 렌더(Isys=Birdy 변종). 부대별 config는 몬스터별 Tactical 클래스에서.
	const TacticalNpcConfig buddyCfg  = TacticalBirdy::trooperConfig();
	const TacticalNpcConfig bomberCfg = TacticalBomber::trooperConfig();
	TacticalNpcConfig bossCfg{
		.maxHp = 2000.f, .moveSpeed = 4.f, .attackRange = 3.5f, .attackDamage = 40.f,
		.attackWindupTime = 0.5s, .attackRecoverTime = 1.0s,
		.separationRadius = 4.f, .separationWeight = 0.3f,
		.attackDamageScale = 3.0f   // boss reuses the Birdy skill roster but hits ~3x harder
	};

	constexpr float TACTICAL_SPAWN_RADIUS = 30.f;

	// 부대 구성: 0,1 = Buddy(Birdy), 2,3 = Bomber. 생성 순서가 IsysMidBossTactic의 squad 인덱스 계약을 보장.
	struct IsysSquadDef { const TacticalNpcConfig* cfg; ObjectType type; int count; };
	const IsysSquadDef squadDefs[] = {
		{ &buddyCfg,  ObjectType::Birdy,  12 },
		{ &buddyCfg,  ObjectType::Birdy,  12 },
		{ &bomberCfg, ObjectType::Bomber, 40 },
		{ &bomberCfg, ObjectType::Bomber, 40 },
	};

	for (int s = 0; s < 4; ++s) {
		const TacticalNpcConfig& cfg = *squadDefs[s].cfg;
		const ObjectType type = squadDefs[s].type;
		auto squad = std::make_unique<TacticalSquad>(s, cfg.attackRange, cfg.separationRadius);

		for (int t = 0; t < squadDefs[s].count; ++t) {
			mu::Vec3 npcPos = randomSpawnInDisc(spawnCenter, TACTICAL_SPAWN_RADIUS);
			auto npc = std::make_unique<TacticalNpc>(makeBase(npcPos), cfg);
			registerTacticalNpcBody(*npc, type);
			npc->setObjType(type);
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
		makeBase(bossPos), bossCfg, std::make_unique<IsysMidBossTactic>());
	registerTacticalNpcBody(*platoonLeader_, ObjectType::Isys);   // 전용 Isys 모델
	platoonLeader_->setObjType(ObjectType::Isys);
	platoonLeader_->body().setCollisionCategory(CollisionLayer::Boss);
	platoonLeaderObjType_ = ObjectType::Isys;

	for (auto& sq : tacticalSquads_)
		platoonLeader_->addSquad(sq.get());
}

// ── Grandbaum ShieldWall 헬퍼 ────────────────────────────────────────────────

void Room::knockPlayersOutOfShieldWall(mu::Vec3 center, float ringRadius) {
	// 인게임 스케일 넉백(시뮬 SHIELD_WALL_KNOCKBACK_*: speed 90 × ~0.4). 이동 권한은 클라에
	// 있으므로 서버는 S_PlayerKnockback만 보내고, 클라가 로컬에서 넉백 + 이동잠금을 실행한다.
	constexpr float  SHIELD_WALL_KNOCKBACK_PADDING = 0.4f;
	constexpr float  SHIELD_WALL_KNOCKBACK_SPEED   = 54.f;  // 거리 ≈ speed×0.32s (≈17m). 인게임 튜닝
	constexpr uint16 KNOCK_MS    = 320;    // 0.32s 강제 이동
	constexpr uint16 POSTLOCK_MS = 2000;   // 2.0s 입력잠금

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

// ShieldWall 중 슬라임을 "차단벽"으로 클라에 통지한다. 플레이어 차단은 클라 권위
// (resolveBarrierSeparation)가 전담하고, 서버에서는 형성 중 바깥 링이 안쪽 링의 진입을 막지 않도록
// 방패벽 슬라임끼리만 물리 충돌을 끈다. Player/Boss 통과와 지형 충돌은 기존 설정을 유지한다.
void Room::setShieldWallBlockers(const std::vector<uint32_t>& blockerIds) {
	constexpr uint32_t NORMAL_TROOPER_MASK = ~(CollisionLayer::Player | CollisionLayer::Boss);
	constexpr uint32_t SHIELD_WALL_SLIME_MASK =
		~(CollisionLayer::Player | CollisionLayer::Boss | CollisionLayer::Slime);

	// 살아있는 blocker가 갱신 목록에서 빠진 경우 ShieldWall 설정을 즉시 원복한다.
	// 죽은 NPC는 이미 물리 월드에서 빠졌지만 같은 처리는 무해하다.
	for (uint32_t id : shieldWallBlockerIds_) {
		if (std::find(blockerIds.begin(), blockerIds.end(), id) != blockerIds.end())
			continue;

		if (TacticalNpc* npc = findTacticalNpcById(id)) {
			npc->body().setCollisionCategory(0xFFFFFFFFu);
			npc->body().setCollisionMask(NORMAL_TROOPER_MASK);
		}
	}

	// 모든 방패벽 슬라임을 동일한 Slime 카테고리로 묶고 그 카테고리를 mask에서 제외한다.
	// 상호 필터가 실패하므로 동료 링을 통과할 수 있지만 다른 물리 레이어와의 충돌은 유지된다.
	for (uint32_t id : blockerIds) {
		if (TacticalNpc* npc = findTacticalNpcById(id)) {
			npc->body().setCollisionCategory(CollisionLayer::Slime);
			npc->body().setCollisionMask(SHIELD_WALL_SLIME_MASK);
		}
	}

	shieldWallBlockerIds_ = blockerIds;
	// on은 1회만(죽은 슬라임은 클라가 hp로 자동 제외 → ids 재통지 불필요).
	if (!shieldWallBarrierOn_) {
		broadcast(PacketManager::makeSNpcBarrierPacket(true, blockerIds));
		shieldWallBarrierOn_ = true;
	}
}

void Room::clearShieldWallBlockers() {
	constexpr uint32_t NORMAL_TROOPER_MASK = ~(CollisionLayer::Player | CollisionLayer::Boss);
	for (uint32_t id : shieldWallBlockerIds_) {
		if (TacticalNpc* npc = findTacticalNpcById(id)) {
			npc->body().setCollisionCategory(0xFFFFFFFFu);
			npc->body().setCollisionMask(NORMAL_TROOPER_MASK);
		}
	}

	if (shieldWallBarrierOn_) {
		broadcast(PacketManager::makeSNpcBarrierPacket(false, shieldWallBlockerIds_));
		shieldWallBarrierOn_ = false;
	}
	shieldWallBlockerIds_.clear();
}

// ── Grandbaum 뱀 증원 웨이브: 동적 소환/디스폰 ───────────────────────────────

namespace {
// One skill attack option: which skill asset to cast, which anim clip drives it, and
// the registered clip key. Shared data so tactical NPCs cast the same varied attacks
// as their field (Npc) counterparts (skill/clip names match setupGoblin/etc.).
struct AttackDef { const char* skillName; const char* clipSrc; const char* clipKey; };

// Boss variants reuse a base monster's rig + skills (Hobgoblin→Goblin, Grandbaum→Treant,
// Isys→Birdy). Map the render objType to the base whose roster it draws from.
ObjectType attackBaseType(ObjectType t) {
	switch (t) {
	case ObjectType::Hobgoblin: return ObjectType::Goblin;
	case ObjectType::Grandbaum: return ObjectType::Treant;
	case ObjectType::Isys:      return ObjectType::Birdy;
	default:                    return t;
	}
}

std::span<const AttackDef> attackRosterFor(ObjectType baseType) {
	static const AttackDef goblin[]   = { {"Goblin_Attack1","Goblin_Attack1","Attack1"},
	                                      {"Goblin_Attack2","Goblin_Attack2","Attack2"},
	                                      {"Goblin_Attack3","Goblin_Attack3","Attack3"} };
	static const AttackDef snake[]    = { {"Snake_Attack1","Snake_Attack1","Attack1"} };
	static const AttackDef mushroom[] = { {"Mushroom_Attack1","Mushroom_Attack1","Attack1"},
	                                      {"Mushroom_Attack2","Mushroom_Attack2","Attack2"} };
	static const AttackDef bomber[]   = { {"Bomber_Attack1","Bomber_Attack1","Attack1"} };
	static const AttackDef birdy[]    = { {"Birdy_Attack1","Birdy_Attack1","Attack1"},
	                                      {"Birdy_Attack2","Birdy_Attack2","Attack2"} };
	static const AttackDef slime[]    = { {"Slime_Attack1","Slime_Attack1","Attack1"} };
	static const AttackDef treant[]   = { {"Treant_SpinKick","Treant_SpinKick","SpinKick"},
	                                      {"Treant_Clap","Treant_Clap","Clap"},
	                                      {"Treant_Punch","Treant_Punch","Punch"} };
	switch (baseType) {
	case ObjectType::Snake:    return snake;
	case ObjectType::Mushroom: return mushroom;
	case ObjectType::Bomber:   return bomber;
	case ObjectType::Birdy:    return birdy;
	case ObjectType::Slime:    return slime;
	case ObjectType::Treant:   return treant;
	case ObjectType::Goblin:
	default:                   return goblin;
	}
}
} // namespace

// 전술 NPC 바디(충돌 BVH·중력·motor) 셋업 후 물리/objectById_에 등록. type으로 모델·애니셋·클립이름을
// 선택해 부대별로 다른 몬스터(슬라임/뱀/버디/바머 등)를 외형 그대로 스폰한다. 보스 변종은 같은 리그를
// 공유한다: Hobgoblin→Goblin 애니, Grandbaum→Treant 애니, Isys→Birdy 애니.
void Room::registerTacticalNpcBody(TacticalNpc& obj, ObjectType type) {
	const Model* model = nullptr;
	const std::vector<ServerAnimClip>* anims = nullptr;
	const char* prefix = "Goblin";   // 클립 이름 접두어(= 애니셋의 몬스터 이름)
	switch (type) {
	case ObjectType::Snake:     model = assetManager_->modelSnake();     anims = &assetManager_->snakeAnimations();    prefix = "Snake";   break;
	case ObjectType::Slime:     model = assetManager_->modelSlime();     anims = &assetManager_->slimeAnimations();    prefix = "Slime";   break;
	case ObjectType::Bomber:    model = assetManager_->modelBomber();    anims = &assetManager_->bomberAnimations();   prefix = "Bomber";  break;
	case ObjectType::Birdy:     model = assetManager_->modelBirdy();     anims = &assetManager_->birdyAnimations();    prefix = "Birdy";   break;
	case ObjectType::Treant:    model = assetManager_->modelTreant();    anims = &assetManager_->treantAnimations();   prefix = "Treant";  break;
	case ObjectType::Hobgoblin: model = assetManager_->modelHobgoblin(); anims = &assetManager_->goblinAnimations();   prefix = "Goblin";  break;
	case ObjectType::Grandbaum: model = assetManager_->modelGrandbaum(); anims = &assetManager_->treantAnimations();   prefix = "Treant";  break;
	case ObjectType::Isys:      model = assetManager_->modelIsys();      anims = &assetManager_->birdyAnimations();    prefix = "Birdy";   break;
	case ObjectType::Goblin:
	default:                    model = assetManager_->modelGoblin();    anims = &assetManager_->goblinAnimations();   prefix = "Goblin";  break;
	}

	obj.setId(IdPool::pop());
	obj.setFaction(Faction::Monsters);
	obj.setModel(model);

	// 라이브 클립은 "Idle"로 고정된다(전술 NPC는 switchClip을 호출하지 않고, 클라가 속도로 모션을 추론).
	// 그래도 Idle/Walk/Die/Attack 슬롯을 모두 정확한 이름으로 등록해 본-부착 피격 BVH가 null 클립으로
	// 동결되는 것을 막는다(project_server_npc_sink_clipnull). Treant는 명명된 공격 클립을 쓴다.
	const std::string p = prefix;
	obj.animController().registerClip("Idle", findServerAnimClip(*anims, p + "_Idle"));
	obj.animController().registerClip("Walk", findServerAnimClip(*anims, p + "_Walk"));
	obj.animController().registerClip("Die",  findServerAnimClip(*anims, p + "_Death"));
	// Full per-type attack clip roster + skill attacks so a tactical NPC casts the same
	// varied skills as its field counterpart (skill hitboxes are authoritative; the AI
	// picks one at random per swing via TacticalNpc::pickAttack). Boss variants reuse the
	// base monster's roster (Hobgoblin→Goblin, etc.).
	for (const AttackDef& a : attackRosterFor(attackBaseType(type))) {
		obj.animController().registerClip(a.clipKey, findServerAnimClip(*anims, a.clipSrc));
		obj.addAttack(skillIdByName(a.skillName), a.clipKey);
	}
	// Legacy generic "Attack" alias kept for the direct-damage fallback / any path that
	// switches to a non-keyed attack clip.
	if (type == ObjectType::Treant || type == ObjectType::Grandbaum)
		obj.animController().registerClip("Attack", findServerAnimClip(*anims, "Treant_SpinKick"));
	else
		obj.animController().registerClip("Attack", findServerAnimClip(*anims, p + "_Attack1"));
	obj.animController().switchClip("Idle");
	obj.setCanReceiveDamage(true);
	// Tactical NPCs (troopers and the PlatoonLeader boss) grant skill charge on death like
	// their field counterparts. Keyed by objType so each variant uses its monster's value;
	// boss objTypes (Hobgoblin/Grandbaum/Isys) read their own chargeConfig.lua entries.
	obj.setKillChargeReward(assetManager_->chargeConfig().monsterCharge(type));

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

// 현재 인카운터(tacticalNpcs_ + platoonLeader_)를 S_NpcSpawnBatch로 통지. 각 NPC의 objType()이
// 클라 렌더 모델을 결정하므로 부대별로 다른 몬스터(슬라임/뱀/버디/바머)와 전용 보스가 그대로 표시된다.
void Room::broadcastEncounterSpawn() {
	std::vector<ObjectInfo> spawnInfos;
	spawnInfos.reserve(tacticalNpcs_.size() + 1);
	auto appendInfo = [&](const TacticalNpc& o) {
		spawnInfos.push_back(ObjectInfo{
			.type           = o.objType(),
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

// (Grandbaum 뱀 증원 웨이브용 런타임 NPC/분대 소환·디스폰 인프라는 DR 토글 재설계로 제거됨.
//  spawnTacticalWaveNpc / addDynamicTacticalSquad / removeTacticalNpcById / removeTacticalSquadById /
//  broadcastTacticalNpcSpawn / reviveTacticalNpc / despawnTacticalNpcHidden — 전부 미사용이라 삭제.)

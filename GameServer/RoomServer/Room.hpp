#ifndef room_hpp
#define room_hpp

#include "IdPool.hpp"
#include "JobQueue.hpp"
#include "GameSession.hpp"
#include "object.hpp"
#include "goblin.hpp"
#include "stronghold.hpp"
#include "NpcGroup.hpp"
#include "physicsWorld.hpp"
#include "skill/skillSystem.hpp"
#include "TacticalNpc.hpp"
#include "TacticalSquad.hpp"
#include "PlatoonLeader.hpp"
#include <memory>
#include <unordered_map>
#include <unordered_set>

class SendBuffer;
struct Level;

class Room {
public:
	Room(int32 id) : id_(id), sessions_(), idSessionMap_(), jobQueue_(), cubes_(), playerStarts_() {
		std::cout << "Room created. ID: " << id_ << '\n';
	}

	~Room() {
		std::cout << "Room destroyed. ID: " << id_ << '\n';
		IdPool::push(id_);
		for (const auto& cube : cubes_) {
			IdPool::push(cube.getId());
		}
		for (const auto& g : goblins_) {
			IdPool::push(g.getId());
		}

		// Unregister tactical NPC bodies before unique_ptrs are destroyed
		if (platoonLeader_) {
			physicsWorld_.unregisterBody(&platoonLeader_->body());
			IdPool::push(platoonLeader_->getId());
		}
		for (const auto& npc : tacticalNpcs_) {
			if (npc) {
				physicsWorld_.unregisterBody(&npc->body());
				IdPool::push(npc->getId());
			}
		}

		for (const auto& sh : strongholds_) {
			IdPool::push(sh.getId());
		}
	}

	void init(const Level* levelData);
	void update();

	void enter(GameSession* session);
	void leave(GameSession* session);
	void move(int32 sessionId, CMovePacket* cMvPkt);
	void rotate(int32 sessionId, CMouseMovePacket* cMouseMvPkt);
	void attack(int32 sessionId, uint64 clientMs);
	void skillStart(int32 sessionId, uint32 skillAssetId, uint64 clientMs);

	void broadcast(const std::shared_ptr<SendBuffer>& sendBuffer);
	void broadcastExcept(GameSession* exceptSession, const std::shared_ptr<SendBuffer>& sendBuffer);

	void pushJob( Job* job ) {
		jobQueue_.push( job );
	}

	void doAsync(CallbackType&& callback) {
		jobQueue_.doAsync(std::move(callback));
	}

	template<class T, class Ret, class... Args>
	void doAsync(Ret(T::* memFn)(Args...), Args&&... args) {
		jobQueue_.doAsync(this, memFn, std::forward<Args>(args)...);
	}

	void doTimer(Milliseconds delay, CallbackType&& callback);

	int32 id() const { return id_; }

	// ── NPC AI 쿼리 ──────────────────────────────────────────────────────────
	const std::vector<GameSession*>& getLivingPlayers() const { return livingPlayersCache_; }
	void MU_CALLCONV findNearbyNpcPositions( mu::Vec3 pos, float radius, uint32 excludeId, std::vector<mu::Vec3>& out ) const;
	int32 countNpcsTargeting( int32 playerId ) const;
	NpcGroup* getNpcGroup( int32 groupId );
	Milliseconds getElapsedMs() const { return elapsedMs_; }
	GameSession* findLivingSessionByPlayerId( int32 playerId ) const;

	// World-routed terrain height (shared chunk height fields). Used by
	// strongholds to snap monster spawn positions onto the ground.
	float MU_CALLCONV groundHeightAtWorld( float x, float z ) const;
	
	// ── Tactical AI ──────────────────────────────────────────────────────────
	bool     tryReserveTacticalAttackSlot(uint32_t targetId, uint32_t npcId);
	void     releaseTacticalAttackSlot(uint32_t targetId, uint32_t npcId);
	uint32_t beginWedgeCharge();
	void     endWedgeCharge(uint32_t chargeId);
	int32    tryApplyWedgeChargeHit(uint32_t chargeId, int32 playerId, float damage);
	void MU_CALLCONV spawnTacticalGoblinEncounter(mu::Vec3 spawnCenter,
	                                               mu::Vec3 bossPos,
	                                               int numSquads        = 3,
	                                               int troopersPerSquad = 20);

private:
	int32 id_;
	std::vector<GameSession*> sessions_;
	std::unordered_map<int32, GameSession*> idSessionMap_;
	JobQueue jobQueue_;

	void registerObject(Object* obj);
	void unregisterObject(Object* obj);

	void setupGoblin(Goblin& g, const Level& level);
	void setupStronghold(Stronghold& sh, const StrongholdDef& sd, const Level& level);
	mu::Vec3 MU_CALLCONV randomSpawnInDisc(mu::Vec3 center, float radius) const;

	void updateGoblinAI(Milliseconds dt);
	void updateTacticalAI(Milliseconds dt);
	void updatePlayerAnimations(Milliseconds dt);
	void updateSkillSystem(Milliseconds dt);
	void rebuildLivingPlayersCache();
	void rebuildAggroCount();

	std::vector<Cube>   cubes_;
	std::vector<Player> playerStarts_;
	std::vector<Goblin> goblins_;
	std::vector<Object*> objectById_;  // sparse: objectById_[id] = Object*, nullptr if unused

	PhysicsWorld      physicsWorld_;
	std::vector<TerrainObject> terrainChunks_;  // one per chunk; height fields are shared (non-owning)
	std::vector<Stronghold>    strongholds_;    // monster spawner bases (damageable structures)
	const TerrainChunkManager* worldTerrain_ = nullptr;  // shared, owned by Level
	SkillSystem skillSystem_;
	EventList         skillEvList_;  // reused across frames (cleared, not reallocated)

	// ── NPC AI 상태 ──────────────────────────────────────────────────────────
	Milliseconds elapsedMs_{ 0ms };
	std::vector<std::unique_ptr<NpcGroup>> npcGroups_;
	std::vector<GameSession*> livingPlayersCache_;
	std::unordered_map<int32/* player id */, int32/* count */> aggroCount_;

	// ── Tactical AI 상태 ─────────────────────────────────────────────────────
	std::vector<std::unique_ptr<TacticalNpc>>   tacticalNpcs_;
	std::vector<std::unique_ptr<TacticalSquad>> tacticalSquads_;
	std::unique_ptr<PlatoonLeader>              platoonLeader_;
	std::unordered_map<uint32_t, std::unordered_set<uint32_t>> tacticalAttackSlots_;
	std::unordered_map<uint32_t, std::unordered_set<uint32_t>> wedgeHitRecord_;
	uint32_t nextWedgeChargeId_{ 1 };
};

#endif // room_hpp

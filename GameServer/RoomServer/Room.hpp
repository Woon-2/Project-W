#ifndef room_hpp
#define room_hpp

#include "IdPool.hpp"
#include "JobQueue.hpp"
#include "GameSession.hpp"
#include "object.hpp"
#include "goblin.hpp"
#include "stronghold.hpp"
#include "zone.hpp"
#include "NpcGroup.hpp"
#include "physicsWorld.hpp"
#include "broadPhase.hpp"
#include "skill/skillSystem.hpp"
#include "TacticalNpc.hpp"
#include "TacticalSquad.hpp"
#include "PlatoonLeader.hpp"
#include <memory>
#include <unordered_map>
#include <unordered_set>

class SendBuffer;
struct Level;
class AssetManager;

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

		// Barriers are pure colliders (no id); unregister their bodies before destroy.
		for (const auto& b : barriers_) {
			if (b) physicsWorld_.unregisterBody(&b->body());
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
	const std::string& code() const { return code_; }
	void setCode(const std::string& c) { code_ = c; }

	// ── NPC AI 쿼리 ──────────────────────────────────────────────────────────
	const std::vector<GameSession*>& getLivingPlayers() const { return livingPlayersCache_; }

	// Accessors used by ZoneSystem to gather faction-filtered candidates and to
	// resolve object ids on Leave events.
	std::vector<Goblin>& goblins() { return goblins_; }
	Object* resolveObject(uint32 id) { return (id < objectById_.size()) ? objectById_[id] : nullptr; }
	void MU_CALLCONV findNearbyNpcPositions( mu::Vec3 pos, float radius, uint32 excludeId, std::vector<mu::Vec3>& out ) const;
	// 전술 NPC 대형 이동 시 회피할 "큰 장애물"(플레이어 + 생존 중인 보스) 위치를 radius 내에서 append.
	void MU_CALLCONV findNearbyBlockerPositions( mu::Vec3 from, float radius, std::vector<mu::Vec3>& out ) const;
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
	// 죽었거나 더 이상 교전/압박 상태가 아닌 NPC의 예약을 슬롯 풀에서 정리.
	void     pruneTacticalAttackReservations();
	// id로 전술 NPC 조회(예약 재배치 시 후보 상태 확인용). 없으면 nullptr.
	TacticalNpc* findTacticalNpcById(uint32_t id) const;
	uint32_t beginWedgeCharge();
	void     endWedgeCharge(uint32_t chargeId);
	int32    tryApplyWedgeChargeHit(uint32_t chargeId, int32 playerId, float damage);
	void MU_CALLCONV spawnTacticalGoblinEncounter(mu::Vec3 spawnCenter,
	                                               mu::Vec3 bossPos,
	                                               int numSquads        = 3,
	                                               int troopersPerSquad = 20);

private:
	int32 id_;
	std::string code_;   // 이 방을 만든 로비 코드(lobbyCode). RoomManager::codeRoomMap_ 키와 동일.
	std::vector<GameSession*> sessions_;
	std::unordered_map<int32, GameSession*> idSessionMap_;
	JobQueue jobQueue_;

	void registerObject(Object* obj);
	void unregisterObject(Object* obj);

	void setupGoblin(Goblin& g, const Level& level);
	void setupStronghold(Stronghold& sh, const StrongholdDef& sd, const Level& level);
	void bindZoneHandlers();   // binds gameplay behavior to zone tags (see Room.cpp)
	void onArenaHobgoblinEnter(Zone& zone, uint32 playerId);
	void spawnBarrierFromMarker(const MarkerDef& m);   // Static collider from a marker transform
	mu::Vec3 MU_CALLCONV randomSpawnInDisc(mu::Vec3 center, float radius) const;

	void updateGoblinAI(Milliseconds dt);
	void updateTacticalAI(Milliseconds dt);
	void updatePlayerAnimations(Milliseconds dt);
	void updateSkillSystem(Milliseconds dt);
	void rebuildLivingPlayersCache();
	void rebuildAggroCount();

	// 분리력 이웃 탐색: ① 플레이어 근접 컬링 → ② SAPBroadPhase 공간분할로 매 프레임
	// 이웃 인접 리스트(npcNeighbors_)를 재구축한다. findNearbyNpcPositions가 이를 조회.
	void rebuildNpcNeighbors();
	bool MU_CALLCONV isNearAnyPlayer(mu::Vec3 p) const;

	std::vector<Cube>   cubes_;
	std::vector<Player> playerStarts_;
	std::vector<Goblin> goblins_;
	std::vector<Object*> objectById_;  // sparse: objectById_[id] = Object*, nullptr if unused

	PhysicsWorld      physicsWorld_;
	std::vector<TerrainObject> terrainChunks_;  // one per chunk; height fields are shared (non-owning)
	std::vector<Stronghold>    strongholds_;    // monster spawner bases (damageable structures)
	ZoneSystem                 zoneSystem_;     // trigger volumes (not physics bodies)
	std::vector<std::unique_ptr<Cube>> barriers_;  // virtual walls (Static colliders, no id, not networked as entities)
	const AssetManager*        assetManager_ = nullptr;  // backref (cube model for barriers); owned by Level
	const TerrainChunkManager* worldTerrain_ = nullptr;  // shared, owned by Level
	SkillSystem skillSystem_;
	EventList         skillEvList_;  // reused across frames (cleared, not reallocated)

	// ── NPC AI 상태 ──────────────────────────────────────────────────────────
	Milliseconds elapsedMs_{ 0ms };
	std::vector<std::unique_ptr<NpcGroup>> npcGroups_;
	std::vector<GameSession*> livingPlayersCache_;
	std::unordered_map<int32/* player id */, int32/* count */> aggroCount_;

	// ── 분리력 이웃 탐색 (공간분할) ────────────────────────────────────────────
	// 1단계 컬링 기준 반경: 감지/교전 사거리 + 클라가 NPC 무리를 또렷이 보는 거리보다
	// 넉넉히 크게 잡아, 교전 중 NPC가 컬링돼 분리력이 빠지거나 팝핑이 보이지 않게 한다.
	static constexpr float NPC_SEPARATION_RELEVANCE_RADIUS = 50.f;
	// SAP AABB fatten 마진. findNearbyNpcPositions에 전달되는 최대 질의 반경(현재 ≈6:
	// CONFUSED_SEPARATION_RADIUS / TACTICAL_PRESSURE_*) 이상이어야 누락이 없다. 7로 여유.
	static constexpr float NPC_SEPARATION_FAT_MARGIN = 7.f;

	SAPBroadPhase npcBroad_;                                        // 분리력 전용 broad phase
	std::unordered_map<const RigidBody*, Object*> npcBodyOwner_;    // SAP 쌍 → NPC 역참조
	std::unordered_map<uint32, std::vector<mu::Vec3>> npcNeighbors_;// id → 이웃 NPC 위치들

	// ── Tactical AI 상태 ─────────────────────────────────────────────────────
	std::vector<std::unique_ptr<TacticalNpc>>   tacticalNpcs_;
	std::vector<std::unique_ptr<TacticalSquad>> tacticalSquads_;
	std::unique_ptr<PlatoonLeader>              platoonLeader_;
	std::unordered_map<uint32_t, std::unordered_set<uint32_t>> tacticalAttackSlots_;
	std::unordered_map<uint32_t, std::unordered_set<uint32_t>> wedgeHitRecord_;
	uint32_t nextWedgeChargeId_{ 1 };
};

#endif // room_hpp

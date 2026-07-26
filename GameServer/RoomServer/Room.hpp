#ifndef room_hpp
#define room_hpp

#include "IdPool.hpp"
#include "JobQueue.hpp"
#include "GameSession.hpp"
#include "InventoryStore.hpp"
#include "object.hpp"
#include "goblin.hpp"
#include "finalBoss.hpp"
#include "snake.hpp"
#include "mushroom.hpp"
#include "bomber.hpp"
#include "birdy.hpp"
#include "slime.hpp"
#include "treant.hpp"
#include "stronghold.hpp"
#include "zone.hpp"
#include "../common/arenaWall.hpp"
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
		// 룸 id는 객체 IdPool이 아니라 RoomIdPool에서 나온다. 예전엔 여기서 IdPool::push(id_)를
		// 불러 룸 id(0,1,2,…)를 **객체 id 풀에 주입**했고, 그 값들은 실제 객체 id와 중복되거나
		// 예약 sentinel 0이라 두 객체가 한 id를 갖게 만들었다.
		// RoomIdPool로 돌려주지도 않는다: JobTimer::distribute는 죽은 룸의 잔여 틱 잡을
		// roomId 조회 실패로 버리는데, 룸 id를 재사용하면 그 잡이 **같은 id를 받은 새 룸**을
		// 때린다. 룸 id는 단조 증가로 남긴다(서버 수명당 65536룸 한도).
		// 배경: docs/objectIdLifecycle.md
		for (const auto& cube : cubes_) {
			// 레벨에서 온 cube는 setId를 받지 않는다(id_ == -1). 반납하면 풀이 오염된다.
			if (cube.hasId()) IdPool::push(cube.getId());
		}
		for (const auto& g : goblins_) {
			IdPool::push(g.getId());
		}
		for (const auto& s : snakes_) {
			IdPool::push(s.getId());
		}
		for (const auto& m : mushrooms_) {
			IdPool::push(m.getId());
		}
		for (const auto& b : bombers_)  { IdPool::push(b.getId()); }
		for (const auto& b : birdys_)   { IdPool::push(b.getId()); }
		for (const auto& s : slimes_)   { IdPool::push(s.getId()); }
		for (const auto& t : treants_)  { IdPool::push(t.getId()); }

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

		// Final boss (runtime-spawned): unregister its body before the unique_ptr dies.
		if (finalBoss_) {
			physicsWorld_.unregisterBody(&finalBoss_->body());
			IdPool::push(finalBoss_->getId());
		}
	}

	void init(const Level* levelData);
	void update();

	void enter(GameSession* session);
	void leave(GameSession* session);
	void move(int32 sessionId, CMovePacket* cMvPkt);
	// 디버그 전용: 안티치트 클램프를 우회해 플레이어를 pos로 즉시 이동(아레나 zone 트리거 테스트).
	void debugTeleport(int32 sessionId, DirectX::XMFLOAT3 pos);
	void rotate(int32 sessionId, CMouseMovePacket* cMouseMvPkt);
	void attack(int32 sessionId, uint64 actionServerMs);
	void skillStart(int32 sessionId, uint32 skillAssetId, uint64 actionServerMs, uint32 skillSeed, float aimPitchRad);
	void selectSkill(int32 sessionId, uint8 slot);   // dial selection (drives kill-charge attribution)
	void inventoryAction(int32 sessionId, uint32 revision, uint8 slotIndex,
		InventoryAction action);

	// --- 인벤토리 영속화 (전부 이 방의 JobQueue 위에서만 실행된다) ---
	// DB 잡 완료 시 콜백. DB 스레드가 아니라 doAsync로 되돌아온 잡에서 호출해야 한다.
	void onInventoryLoaded(GameSession* session, InventoryStore::LoadStatus status,
		const std::vector<ItemStack>& slots);
	// 모든 DB 잡의 마지막에 호출. 지연된 방 제거를 마무리한다.
	void onDbJobFinished();

	// Server-internal skill cast for NPCs (no session / charge gate). Starts an
	// authoritative skill instance owned by ownerObjectId and broadcasts S_SkillStart
	// (ownerId = NPC id, elapsedMs = 0) so clients play the matching VFX/animation.
	void skillStartInternal(int32 ownerObjectId, uint32 skillAssetId, uint32 skillSeed, float damageScale = 1.0f);
	// True while ownerObjectId has a live skill instance (NPC AI holds its attack).
	bool npcSkillActive(int32 ownerObjectId) const;
	// Resolve a skill asset id by name from the shared registry (0 if absent).
	uint32 skillIdByName(std::string_view name) const;

	void broadcast(const std::shared_ptr<SendBuffer>& sendBuffer);
	void broadcastExcept(GameSession* exceptSession, const std::shared_ptr<SendBuffer>& sendBuffer);
	// Sends a tactical presentation event only to living players who have entered
	// the active arena. Tactic implementations call this at cycle start.
	void notifyTacticalDialogue(TacticalDialogueId dialogueId);

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
	void doTimerAt(HighResolutionClock::time_point executionTime, CallbackType&& callback);

	int32 id() const { return id_; }
	const std::string& code() const { return code_; }
	void setCode(const std::string& c) { code_ = c; }

	// ── NPC AI 쿼리 ──────────────────────────────────────────────────────────
	const std::vector<GameSession*>& getLivingPlayers() const { return livingPlayersCache_; }

	// Accessors used by ZoneSystem to gather faction-filtered candidates and to
	// resolve object ids on Leave events.
	std::vector<Goblin>&   goblins()   { return goblins_; }
	std::vector<Snake>&    snakes()    { return snakes_; }
	std::vector<Mushroom>& mushrooms() { return mushrooms_; }
	std::vector<Bomber>&   bombers()   { return bombers_; }
	std::vector<Birdy>&    birdys()    { return birdys_; }
	std::vector<Slime>&    slimes()    { return slimes_; }
	std::vector<Treant>&   treants()   { return treants_; }
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

	// Bounded rejection-sampling variant of randomSpawnInDisc: retries a fixed
	// number of times to find a candidate whose footprint (footprintSource's
	// model/orient/scale, placed at the candidate position) does not overlap any
	// registered scatter prop. Falls back to the last sampled position if every
	// attempt collides; the existing static depenetration resolves any residual
	// overlap over the next few physics steps. footprintSource must already have
	// setModel/setScale/setOrient applied. Used by Stronghold to place/revive monsters.
	mu::Vec3 MU_CALLCONV randomSpawnInDiscAvoidingProps(
		mu::Vec3 center, float radius, const Object& footprintSource) const;
	// Ground-snapped candidate가 scatter/arena wall/Static 구조물과 겹치지 않는지 검사.
	// Isys 전술 대형 슬롯 보정용이며 동적 NPC/플레이어는 검사하지 않는다.
	bool MU_CALLCONV isTacticalFormationPositionOpen(
		mu::Vec3 candidate, const Object& footprintSource) const;

	// Bind terrain height/normal callbacks onto a skill dispatch context so
	// AttachType::Ground hitboxes snap to the same surface the client uses.
	void bindGroundQueries( SkillDispatchContext& ctx ) const;
	
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
	// Grandbaum 중간보스 인카운터: 슬라임 3부대(0,1,2) + 뱀 1부대(3) + Grandbaum 전술 보스.
	void MU_CALLCONV spawnGrandbaumEncounter(mu::Vec3 spawnCenter, mu::Vec3 bossPos);
	// Isys 중간보스 인카운터: Buddy 2부대(0,1) + Bomber 2부대(2,3) + Isys 전술 보스(2연속 쐐기 협공).
	void MU_CALLCONV spawnIsysEncounter(mu::Vec3 spawnCenter, mu::Vec3 bossPos);

	// ── Grandbaum ShieldWall ──────────────────────────────────────────────────
	// 링 형성 순간 안쪽 플레이어를 바깥으로 넉백(클라 권한 이동잠금: S_PlayerKnockback).
	void MU_CALLCONV knockPlayersOutOfShieldWall(mu::Vec3 center, float ringRadius);
	// 슬라임을 플레이어가 통과 못 하는 하드 블로커로 전환/해제(콜리전 마스크 토글).
	void setShieldWallBlockers(const std::vector<uint32_t>& blockerIds, uint32_t impulseOnlyNpcId);
	void clearShieldWallBlockers();

private:
	int32 id_;
	std::string code_;   // 이 방을 만든 로비 코드(lobbyCode). RoomManager::codeRoomMap_ 키와 동일.
	std::vector<GameSession*> sessions_;
	std::unordered_map<int32, GameSession*> idSessionMap_;
	JobQueue jobQueue_;

	// 진행 중인 DB 잡 수. 마지막 플레이어가 나가도 이게 0이 아니면 방을 즉시 지우지 않고
	// closePending_으로 미룬다 — Room*은 ObjectPool로 재활용되므로, 완료 잡이 되돌아올 방이
	// 사라져 있으면 안 된다. 둘 다 이 방의 JobQueue 전용이라 락이 필요 없다.
	int32 pendingDbJobs_ = 0;
	bool  closePending_ = false;

	// 인벤토리 로드/저장 잡을 건다. persistInventory는 변경이 없으면 아무것도 하지 않는다.
	void requestInventoryLoad(GameSession* session);
	void persistInventory(GameSession* session);

	void registerObject(Object* obj);
	void unregisterObject(Object* obj);

	void setupGoblin   (Goblin&    g, const Level& level);
	void setupSnake    (Snake&     s, const Level& level);
	void setupMushroom (Mushroom&  m, const Level& level);
	void setupBomber   (Bomber&    b, const Level& level);
	void setupBirdy    (Birdy&     b, const Level& level);
	void setupSlime    (Slime&     s, const Level& level);
	void setupTreant   (Treant&    t, const Level& level);
	void setupStronghold(Stronghold& sh, const StrongholdDef& sd, const Level& level);
	void setupFinalBoss(FinalBoss& b);   // model/anims/clips/skills/body/BT for the final boss (uses assetManager_)
	void bindZoneHandlers();   // binds gameplay behavior to zone tags (see Room.cpp)
	void onArenaHobgoblinEnter(Zone& zone, uint32 playerId);
	void onArenaGrandbaumEnter(Zone& zone, uint32 playerId);
	void onArenaIsysEnter(Zone& zone, uint32 playerId);
	void onArenaBossEnter(Zone& zone, uint32 playerId);   // ArenaZone enter -> spawn boss at BossSpawner marker
	// 전술 NPC 바디 셋업(type별 모델/애니/클립 선택) + 물리/objectById_ 등록. type이 클라 렌더 모델을 결정.
	void registerTacticalNpcBody(TacticalNpc& obj, ObjectType type);
	// 현재 인카운터(tacticalNpcs_ + platoonLeader_)를 S_NpcSpawnBatch로 통지. NPC별 objType()으로 모델 라우팅.
	void broadcastEncounterSpawn();
	void spawnBarrierFromMarker(const MarkerDef& m);   // Static collider from a marker transform
	void teardownArenaWalls();                 // 전 NPC 처치 시 아레나 가상 벽 해제(barriers_ 파괴 + S_ZoneState(.,0))
	bool allTacticalCombatantsDead() const;    // 보스 사망 AND 전 부대원 사망이면 true
	// 클리어된 인카운터의 tactical 컨테이너(tacticalNpcs_/tacticalSquads_/platoonLeader_ 등)를
	// 전부 비워 다음 아레나가 스폰 가드를 통과할 수 있게 한다. teardownArenaWalls() 직후 호출.
	void cleanupTacticalEncounter();
	// 현재 다른 아레나 인카운터가 진행 중인지(스폰 가드/동시 진입 차단 가드에서 공용으로 사용).
	bool tacticalEncounterActive() const { return !tacticalNpcs_.empty() || platoonLeader_ != nullptr; }
	mu::Vec3 MU_CALLCONV randomSpawnInDisc(mu::Vec3 center, float radius) const;

	void updateMonsterAI(Milliseconds dt);
	void updateTacticalAI(Milliseconds dt);
	void updatePlayerAnimations(Milliseconds dt);
	void updateSkillSystem(Milliseconds dt);

	// Stack-charge: skill registry lookup + kill-charge attribution/distribution.
	const SkillAsset* findSkillAsset(uint32 id) const;
	// Records a player's damage to a target; on a chargeable-monster death,
	// distributes kill-charge to all recent damagers. Call after HP is updated.
	void noteAndMaybeReward(int32 attackerObjId, Object* target, int32 prevHp, int32 newHp);
	void distributeKillCharge(Object* monster);
	void updateComboExpiry();   // resets stale combos (combo window elapsed) each tick
	void updatePlayerRegen(Milliseconds dt);   // server-authoritative combo-driven HP regen

	void rebuildLivingPlayersCache();
	void rebuildAggroCount();

	// 분리력 이웃 탐색: ① 플레이어 근접 컬링 → ② SAPBroadPhase 공간분할로 매 프레임
	// 이웃 인접 리스트(npcNeighbors_)를 재구축한다. findNearbyNpcPositions가 이를 조회.
	void rebuildNpcNeighbors();
	bool MU_CALLCONV isNearAnyPlayer(mu::Vec3 p) const;

	std::vector<Cube>     cubes_;
	std::vector<Player>   playerStarts_;
	std::vector<Goblin>   goblins_;
	std::vector<Snake>    snakes_;
	std::vector<Mushroom> mushrooms_;
	std::vector<Bomber>   bombers_;
	std::vector<Birdy>    birdys_;
	std::vector<Slime>    slimes_;
	std::vector<Treant>   treants_;
	// Final boss: runtime-spawned (on ArenaZone enter), single 1:1 combatant. unique_ptr
	// keeps a stable address; nullptr until spawned. Not part of the init monster pools.
	std::unique_ptr<FinalBoss> finalBoss_;
	std::vector<Object*> objectById_;  // sparse: objectById_[id] = Object*, nullptr if unused

	PhysicsWorld      physicsWorld_;
	std::vector<TerrainObject> terrainChunks_;  // one per chunk; height fields are shared (non-owning)
	std::vector<Stronghold>    strongholds_;    // monster spawner bases (damageable structures)
	ZoneSystem                 zoneSystem_;     // trigger volumes (not physics bodies)
	std::vector<std::unique_ptr<Cube>> barriers_;  // virtual walls (Static colliders, no id, not networked as entities)
	uint16                     activeArenaZoneId_{ 0 };     // 현재 벽이 올라간 아레나 zone id (S_ZoneState 해제 대상)
	bool                       arenaWallsActive_{ false };  // 일방향 벽이 올라가 있고 아직 해제 안 됨(1회성 가드)
	std::vector<OneWayWall>    arenaWalls_;                 // 양끝 후방 Wall 일방향 슬랩(move() 권위 클램프 대상)
	std::unordered_set<uint32> activeArenaParticipantIds_; // 현재 아레나 입장 이력이 있는 플레이어
	const AssetManager*        assetManager_ = nullptr;  // backref (cube model for barriers); owned by Level
	const TerrainChunkManager* worldTerrain_ = nullptr;  // shared, owned by Level
	SkillSystem skillSystem_;
	EventList         skillEvList_;  // reused across frames (cleared, not reallocated)

	// 다음 룸 틱의 절대 데드라인. 고정 케이던스를 유지해 시뮬 시간이 실시간에서 이탈하지 않게 한다
	// (상대 지연 재예약은 처리 시간을 매 틱 누적시킨다 — Room::update 주석 참조).
	HighResolutionClock::time_point nextTickTime_{};

	// ── NPC AI 상태 ──────────────────────────────────────────────────────────
	Milliseconds elapsedMs_{ 0ms };
	Milliseconds regenSyncAccum_{ 0ms };   // throttles S_PlayerHp regen broadcasts (~10 Hz)
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
	// platoonLeader_가 클라에 어떤 모델로 보일지(ObjectInfo.type). Goblin 전술은 Hobgoblin,
	// GrandBaum/Isis는 전용 모델 추가 전까지 Goblin placeholder.
	ObjectType                                  platoonLeaderObjType_{ ObjectType::Goblin };
	std::vector<uint32_t>                       shieldWallBlockerIds_;   // GrandBaum ShieldWall 중 하드 블로커로 전환된 슬라임 id
	uint32_t                                    shieldWallImpulseOnlyNpcId_{ SNpcBarrierPacket::INVALID_NPC_ID }; // barrier는 아니지만 impulse 면역인 보스
	bool                                        shieldWallBarrierOn_{ false };   // 클라 S_NpcBarrier on 통지 여부(매 틱 중복 송신 방지)
	std::unordered_map<uint32_t, std::unordered_set<uint32_t>> tacticalAttackSlots_;
	std::unordered_map<uint32_t, std::unordered_set<uint32_t>> wedgeHitRecord_;
	uint32_t nextWedgeChargeId_{ 1 };
};

#endif // room_hpp

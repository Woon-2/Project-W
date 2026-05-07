#ifndef room_hpp
#define room_hpp

#include "IdPool.hpp"
#include "JobQueue.hpp"
#include "GameSession.hpp"
#include "object.hpp"
#include "goblin.hpp"
#include "NpcGroup.hpp"
#include "physicsWorld.hpp"
#include <memory>

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
	}

	void init(const Level* levelData);
	void update();

	void enter(GameSession* session);
	void leave(GameSession* session);
	void move(int32 sessionId, CMovePacket* cMvPkt);
	void rotate(int32 sessionId, CMouseMovePacket* cMouseMvPkt);
	void attack(int32 sessionId, uint64 clientMs);

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

private:
	int32 id_;
	std::vector<GameSession*> sessions_;
	std::unordered_map<int32, GameSession*> idSessionMap_;
	JobQueue jobQueue_;

	void updateGoblinAI(Milliseconds dt);
	void rebuildLivingPlayersCache();
	void rebuildAggroCount();

	std::vector<Cube>   cubes_;
	std::vector<Player> playerStarts_;
	std::vector<Goblin> goblins_;

	PhysicsWorld  physicsWorld_;
	TerrainObject terrain_;

	// ── NPC AI 상태 ──────────────────────────────────────────────────────────
	Milliseconds elapsedMs_{ 0ms };
	std::vector<std::unique_ptr<NpcGroup>> npcGroups_;
	std::vector<GameSession*> livingPlayersCache_;
	std::unordered_map<int32/* player id */, int32/* count */> aggroCount_;
};

#endif // room_hpp

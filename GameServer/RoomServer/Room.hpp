#ifndef room_hpp
#define room_hpp

#include "IdPool.hpp"
#include "JobQueue.hpp"
#include "GameSession.hpp"
#include "object.hpp"

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
	}

	void init(const Level* levelData);

	void enter(GameSession* session);
	void leave(GameSession* session);
	void move(int32 sessionId, CMovePacket* cMvPkt);
	void rotate(int32 sessionId, CMouseMovePacket* cMouseMvPkt);

	void broadcast(const std::shared_ptr<SendBuffer>& sendBuffer);
	void broadcastExcept(GameSession* exceptSession, const std::shared_ptr<SendBuffer>& sendBuffer);

	void doAsync(CallbackType&& callback) {
		jobQueue_.doAsync(std::move(callback));
	}

	template<class T, class Ret, class... Args>
	void doAsync(Ret(T::* memFn)(Args...), Args&&... args) {
		jobQueue_.doAsync(this, memFn, std::forward<Args>(args)...);
	}

	int32 id() const { return id_; }

private:
	int32 id_;
	std::vector<GameSession*> sessions_;
	std::unordered_map<int32, GameSession*> idSessionMap_;
	JobQueue jobQueue_;

	std::vector<Object> cubes_;
	std::vector<Object> playerStarts_;
};

#endif // room_hpp
#ifndef room_hpp
#define room_hpp

#include "IdPool.hpp"
#include "JobQueue.hpp"
#include "GameSession.hpp"

class SendBuffer;

class Room {
public:
	Room(int32 id) : id_(id), sessions_(), sessionIdMap_(), jobQueue_() {
		std::cout << "Room created. ID: " << id_ << '\n';
	}

	~Room() {
		std::cout << "Room destroyed. ID: " << id_ << '\n';
		IdPool::push(id_);
	}

	void enter(GameSession* session);
	void leave(GameSession* session);
	void broadcast(SendBuffer* sendBuffer);

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
	std::unordered_map<int32, GameSession*> sessionIdMap_;
	JobQueue jobQueue_;
};

#endif // room_hpp
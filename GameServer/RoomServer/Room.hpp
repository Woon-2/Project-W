#ifndef room_hpp
#define room_hpp

#include "IdPool.hpp"

class Player;
class SendBuffer;

class Room {
public:
	Room(int32 id) : id_(id), players_(), playerIdMap_() {
		std::cout << "Room created. ID: " << id_ << '\n';
	}

	~Room() {
		std::cout << "Room destroyed. ID: " << id_ << '\n';
		IdPool::push(id_);
	}

	void enter(Player* player);
	void leave(Player* player);
	void broadcast(SendBuffer* sendBuffer);

	int32 id() const { return id_; }

private:
	int32 id_;
	std::vector<Player*> players_;
	std::unordered_map<int32, Player*> playerIdMap_;
};

#endif // room_hpp
#ifndef player_hpp
#define player_hpp

class GameSession;
class SendBuffer;

class Player {
public:
	Player() : ownerSession_(nullptr) {}

	void send(SendBuffer* sendBuffer);
	int32 id() const;

private:
	GameSession* ownerSession_;
};

#endif // player_hpp
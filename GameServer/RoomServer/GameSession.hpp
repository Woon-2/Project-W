#ifndef game_session_hpp
#define game_session_hpp

#include "Session.hpp"
#include <chrono>

class Room;
class Player;

class GameSession : public PacketSession {
public:
	GameSession() : myRoom_(nullptr), myPlayer_(nullptr) {}
	virtual ~GameSession();

	Room*   room()   const { return myRoom_;   }
	Player* player() const { return myPlayer_; }

	// Grandbaum 넉백 동안 Room::move의 안티치트 클램프를 면제한다(클라가 넉백 속도로 빠르게
	// 밀려나므로 7m/packet 한계에 걸려 잘리면 안 된다). 넉백+이동잠금 총 시간 동안 면제.
	void grantMoveClampExemption(std::chrono::milliseconds d) {
		moveClampExemptUntil_ = std::chrono::steady_clock::now() + d;
	}
	bool isMoveClampExempt() const {
		return std::chrono::steady_clock::now() < moveClampExemptUntil_;
	}

	// C_Enter(lobbyCode) 수신 시 호출. 코드로 방을 find-or-create 하고 입장 잡을 건다.
	void enterRoom(const std::string& lobbyCode);

protected:
	virtual void onConnected() override;
	virtual void onDisconnected() override;
	virtual void processPacket(byte* buffer, int32 len) override;
	virtual void onSend(int32 len) override;

private:
	Room* myRoom_;
	Player* myPlayer_;
	std::chrono::steady_clock::time_point moveClampExemptUntil_{};
};

#endif // game_session_hpp
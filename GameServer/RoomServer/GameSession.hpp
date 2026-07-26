#ifndef game_session_hpp
#define game_session_hpp

#include "Session.hpp"
#include "protocol.hpp"

#include <atomic>
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

	// --- 계정 상태 (입장 티켓 검증으로 확정) ---
	// setAccount는 C_Enter를 처리하는 IOCP 스레드가 딱 한 번 호출한다. release 저장이
	// accountId_/nickname_ 쓰기를 뒤따르므로, acquire로 true를 본 스레드는 그 값들을 안전하게 읽는다.
	void setAccount(int64 accountId, const wchar_t* nickname);
	bool isEntryAuthorized() const { return entryAuthorized_.load(std::memory_order_acquire); }
	int64 accountId() const { return accountId_; }
	const wchar_t* nickname() const { return nickname_; }

	// --- 인벤토리 영속화 상태 (방 JobQueue 전용 — 다른 스레드에서 만지지 말 것) ---
	// ready: DB 로드가 끝났나. 끝나기 전 조작은 거절한다(로드가 조작을 덮어쓰는 레이스 차단).
	// loaded: DB에서 실제로 읽었나(또는 신규 계정임을 확정했나). false면 퇴장 시 저장하지 않는다
	//         — DB 장애로 받은 스타터 로드아웃이 진짜 인벤토리를 덮어쓰는 것을 막는다.
	bool inventoryReady() const { return inventoryReady_; }
	void setInventoryReady(bool v) { inventoryReady_ = v; }
	bool inventoryLoaded() const { return inventoryLoaded_; }
	void setInventoryLoaded(bool v) { inventoryLoaded_ = v; }

protected:
	virtual void onConnected() override;
	virtual void onDisconnected() override;
	virtual void processPacket(byte* buffer, int32 len) override;
	virtual void onSend(int32 len) override;

private:
	Room* myRoom_;
	Player* myPlayer_;
	std::chrono::steady_clock::time_point moveClampExemptUntil_{};

	std::atomic_bool entryAuthorized_{ false };
	int64   accountId_ = 0;
	wchar_t nickname_[kNicknameMax]{};

	bool inventoryReady_ = false;
	bool inventoryLoaded_ = false;
};

#endif // game_session_hpp
#ifndef game_session_hpp
#define game_session_hpp

#include "Session.hpp"
#include "protocol.hpp"

#include <atomic>

class LobbyRoom;

class GameSession : public PacketSession {
public:
	GameSession() = default;
	virtual ~GameSession();

	LobbyRoom* myRoom_ = nullptr;
	PlayerWeaponType selectedWeaponType_ = PlayerWeaponType::Katana;

	// --- 계정 상태 ---
	// DB 스레드가 쓰고 IOCP 스레드가 읽으므로 atomic으로 둔다.
	// nickname_은 DB 스레드가 authenticated_를 release로 세우기 "전"에만 쓰고,
	// 읽는 쪽은 authenticated_의 acquire 로드가 true일 때만 읽는다.
	std::atomic_bool authenticated_{ false };
	std::atomic_int64_t accountId_{ 0 };
	wchar_t nickname_[ kNicknameMax ]{};

protected:
	virtual void onConnected() override;
	virtual void onDisconnected() override;
	virtual void processPacket( byte* buffer, int32 len ) override;
	virtual void onSend( int32 len ) override {}
};

#endif // game_session_hpp

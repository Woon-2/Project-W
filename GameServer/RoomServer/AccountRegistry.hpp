#ifndef account_registry_hpp
#define account_registry_hpp

#include <mutex>
#include <unordered_map>

// 계정당 룸서버 세션 하나를 보장한다.
//
// 왜 룸서버가 따로 들고 있어야 하나: 클라가 핸드오프로 로비 소켓을 닫는 순간
// 로비서버는 계정 바인딩을 풀어버린다(LobbyServer/GameSession.cpp onDisconnected).
// 즉 핸드오프가 시작되면 그 계정은 곧바로 다른 곳에서 다시 로그인할 수 있고,
// 룸서버가 막지 않으면 같은 accountId를 쥔 세션 둘이 dbo.Inventory를 서로 덮어쓴다.
// 서명 티켓의 유일한 약점인 재사용도 이걸로 함께 막힌다.
//
// LobbyServer/GameSessionManager의 bindAccount/unbindAccount와 같은 구조다.
class AccountRegistry {
public:
	// 이미 다른 세션이 잡고 있으면 false.
	static bool bind( int64 accountId, uint16 sessionId );
	static void unbind( int64 accountId );

private:
	static std::mutex mutex_;
	static std::unordered_map<int64, uint16> map_;
};

#endif // account_registry_hpp

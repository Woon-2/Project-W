#ifndef game_session_manager_hpp
#define game_session_manager_hpp

#include <memory>
#include <mutex>
#include <unordered_map>

class GameSession;

// 접속 중인 GameSession의 소유 참조(shared_ptr)를 id별로 보관한다.
// 연결 동안 매니저가 1개의 ref를 들고, 끊기면 remove로 놓는다. pending I/O ref까지 모두
// 사라지면 ObjectPool::makeShared의 deleter가 ~GameSession 실행 후 풀로 반환한다.
class GameSessionManager {
public:
	static void add( const std::shared_ptr<GameSession>& session );
	static void remove( uint16 id );
	static std::shared_ptr<GameSession> find( uint16 id );
	static int32 count();

	// 중복 로그인 차단: 계정당 세션 하나만 허용한다.
	// bindAccount가 false면 이미 다른 세션이 그 계정으로 로그인 중이다.
	static bool bindAccount( int64 accountId, uint16 sessionId );
	static void unbindAccount( int64 accountId );

private:
	static std::mutex mutex_;
	static std::unordered_map<uint16, std::shared_ptr<GameSession>> sessions_;
	static std::unordered_map<int64, uint16> loginMap_;
};

#endif // game_session_manager_hpp

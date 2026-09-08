#ifndef database_executor_hpp
#define database_executor_hpp

#include "DBConnectionPool.hpp"

#include <condition_variable>
#include <deque>
#include <functional>
#include <thread>

/*--------------------
     DBExecutor
--------------------*/

// DB 작업 전용 스레드 + 작업 큐.
//
// ODBC 호출은 블로킹이므로 IOCP 워커에서 직접 실행하면 그동안 dispatch 루프가 멈춘다.
// 워커는 post()로 잡을 큐에 넣기만 하고, 전용 스레드가 순서대로 실행한다.
// 응답은 잡 안에서 session->send()로 보낸다 (send는 크로스 스레드 안전).
//
// 기존 JobQueue를 재사용하지 않는 이유: JobQueue::push()는 전역 JobQueuePool에 큐를
// 자동 등록하는데, RoomServer의 잡 스레드들이 JobQueuePool에서 아무 큐나 집어가므로
// DB 잡이 다른 스레드로 새어 나가 커넥션이 동시 실행될 수 있다.
//
// 잡에 세션을 넘길 때는 shared_ptr을 값으로 캡처할 것:
//   auto self = std::static_pointer_cast<GameSession>( session->shared_from_this() );
//   DBExecutor::post( [self, ...]() { ...; self->send( ... ); } );
//
// 수명: init()은 MemoryManager::init() 이후(내부 풀이 onew 사용),
// shutdown()은 MemoryManager::release() 이전에 호출해야 한다.
class DBExecutor {
public:
	static bool init( int32 connCnt, const WCHAR* connStr );
	static void shutdown();

	// 어느 스레드에서든 호출 가능. shutdown() 이후의 post는 조용히 버려진다.
	static void post( std::function<void()> job );

	// 잡 내부에서 DBConnectionPoolGuard로 커넥션을 빌릴 때 사용.
	static DBConnectionPool& pool() { return pool_; }

private:
	static void run();

	static std::thread thread_;
	static std::mutex mutex_;
	static std::condition_variable cv_;
	static std::deque<std::function<void()>> jobs_;
	static bool running_;
	static DBConnectionPool pool_;
};

#endif // database_executor_hpp

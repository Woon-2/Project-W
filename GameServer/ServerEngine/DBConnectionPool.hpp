#ifndef database_connection_pool_hpp
#define database_connection_pool_hpp

#include "DBConnection.hpp"

#include <mutex>
#include <vector>

/*------------------------
     DBConnectionPool
------------------------*/

// 커넥션을 미리 만들어두고 pop()/push()로 빌려주는 풀.
// DBConnection 하나는 HSTMT 하나를 독점하므로 스레드 안전하지 않다.
// 반드시 한 번에 한 스레드만 쓰도록 풀에서 빌려 쓸 것.
//
// 수명 주의: connect()는 내부에서 onew<DBConnection>()을 쓰므로 MemoryManager::init() "이후"에,
// clear()는 MemoryManager::release() "이전"에 호출해야 한다.
class DBConnectionPool {
public:
	DBConnectionPool() : poolMutex_(), hEnv_( SQL_NULL_HANDLE ), conns_() {}
	~DBConnectionPool() { clear(); }

	DBConnectionPool( const DBConnectionPool& ) = delete;
	DBConnectionPool& operator=( const DBConnectionPool& ) = delete;

	bool connect( int32 connCnt, const WCHAR* connStr );
	void clear();

	void push( DBConnection* conn );
	DBConnection* pop();

private:
	// 락을 이미 잡은 상태에서 부르는 정리 루틴.
	// std::mutex는 재진입이 안 되므로, connect() 실패 경로에서 clear()를 부르면 데드락이다.
	void clearNoLock();

	std::mutex poolMutex_;
	SQLHENV hEnv_;
	std::vector<DBConnection*> conns_;
};

/*---------------------------
     DBConnectionPoolGuard
---------------------------*/

// pop()한 커넥션을 스코프 종료 시 반드시 반납한다.
// 핸들러가 중간에 return하면 커넥션이 영구 소실되고, 풀이 마르면 DB 기능이 조용히 멈춘다.
//
//   DBConnectionPoolGuard guard( pool );
//   if ( !guard ) { return; }                 // 풀 고갈
//   DBBinder<1, 2> binder( *guard.get(), query );
class DBConnectionPoolGuard {
public:
	explicit DBConnectionPoolGuard( DBConnectionPool& pool )
		: pool_( pool ), conn_( pool.pop() )
	{
	}

	~DBConnectionPoolGuard() {
		if ( conn_ != nullptr ) {
			pool_.push( conn_ );
		}
	}

	DBConnectionPoolGuard( const DBConnectionPoolGuard& ) = delete;
	DBConnectionPoolGuard& operator=( const DBConnectionPoolGuard& ) = delete;

	// 풀이 비어 있으면 nullptr이다. 쓰기 전에 반드시 확인할 것.
	DBConnection* get() const { return conn_; }
	explicit operator bool() const { return conn_ != nullptr; }

private:
	DBConnectionPool& pool_;
	DBConnection* conn_;
};

#endif // database_connection_pool_hpp

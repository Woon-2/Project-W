#include "sepch.hpp"
#include "DBConnectionPool.hpp"

/*------------------------
     DBConnectionPool
------------------------*/

bool DBConnectionPool::connect( int32 connCnt, const WCHAR* connStr ) {
	std::lock_guard lock( poolMutex_ );

	if ( !SQL_SUCCEEDED( ::SQLAllocHandle( SQL_HANDLE_ENV, SQL_NULL_HANDLE, &hEnv_ ) ) ) {
		dbLogW( L"[DB] ODBC 환경 핸들(HENV) 할당 실패" );
		return false;
	}

	if ( !SQL_SUCCEEDED( ::SQLSetEnvAttr( hEnv_, SQL_ATTR_ODBC_VERSION, reinterpret_cast<SQLPOINTER>(SQL_OV_ODBC3), 0 ) ) ) {
		dbLogW( L"[DB] ODBC 버전(SQL_OV_ODBC3) 설정 실패" );
		clearNoLock();
		return false;
	}

    for ( int32 i = 0; i < connCnt; ++i ) {
		auto conn = onew<DBConnection>();

        if ( !conn->connect( hEnv_, connStr ) ) {
			// 실패 원인은 DBConnection::connect()가 이미 찍었다.
			// 이 커넥션은 conns_에 안 들어가므로 여기서 직접 해제해야 한다.
			odelete( conn );

			// 앞서 성공한 커넥션들과 ENV까지 정리한다.
			// 락을 이미 들고 있으므로 clear()가 아니라 clearNoLock()이어야 한다.
			clearNoLock();
            return false;
        }

		conns_.push_back( conn );
    }

    return true;
}

void DBConnectionPool::clear() {
	std::lock_guard lock( poolMutex_ );
	clearNoLock();
}

void DBConnectionPool::clearNoLock() {
	// 자식(DBC)부터 정리하고 부모(ENV)를 마지막에 해제한다.
	// 순서를 어기면 SQLFreeHandle이 HY010으로 실패해서 ENV까지 샌다.
    for ( auto conn : conns_ ) {
		odelete( conn );
    }

	conns_.clear();

    if ( hEnv_ != SQL_NULL_HANDLE ) {
		::SQLFreeHandle( SQL_HANDLE_ENV, hEnv_ );
		hEnv_ = SQL_NULL_HANDLE;
    }
}

void DBConnectionPool::push( DBConnection* conn ) {
    std::lock_guard lock( poolMutex_ );
    conns_.push_back( conn );
}

DBConnection* DBConnectionPool::pop() {
	std::lock_guard lock( poolMutex_ );

    if ( conns_.empty() ) {
        return nullptr;
    }

	auto conn = conns_.back();
	conns_.pop_back();

	return conn;
}

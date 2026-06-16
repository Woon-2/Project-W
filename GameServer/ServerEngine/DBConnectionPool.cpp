#include "sepch.hpp"
#include "DBConnectionPool.hpp"

/*------------------------
     DBConnectionPool
------------------------*/

bool DBConnectionPool::connect( int32 connCnt, const std::wstring& connStr ) {
	std::lock_guard lock( poolMutex_ );

    if ( ::SQLAllocHandle( SQL_HANDLE_ENV, SQL_NULL_HANDLE, &hEnv_ ) != SQL_SUCCESS ) {
        return false;
    }

    if ( ::SQLSetEnvAttr( hEnv_, SQL_ATTR_ODBC_VERSION, reinterpret_cast<SQLPOINTER>(SQL_OV_ODBC3), 0 ) != SQL_SUCCESS ) {
		return false;
    }

    for ( int32 i = 0; i < connCnt; ++i ) {
		auto conn = onew<DBConnection>();

        if ( !conn->connect( hEnv_, connStr ) ) {
            return false;
        }

		conns_.push_back( conn );
    }
}

void DBConnectionPool::clear() {
	std::lock_guard lock( poolMutex_ );

    if ( hEnv_ != SQL_NULL_HANDLE ) {
		::SQLFreeHandle( SQL_HANDLE_ENV, hEnv_ );
		hEnv_ = SQL_NULL_HANDLE;
    }

    for ( auto conn : conns_ ) {
		odelete( conn );
    }

	conns_.clear();
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

void DBConnectionPool::push( DBConnection* conn ) {
    std::lock_guard lock( poolMutex_ );
	conns_.push_back( conn );
}

#ifndef database_connection_pool_hpp
#define database_connection_pool_hpp

#include "DBConnection.hpp"

/*------------------------
     DBConnectionPool
------------------------*/

class DBConnectionPool {
public:
	DBConnectionPool() : poolMutex_(), hEnv_( SQL_NULL_HANDLE ), conns_() {}
	~DBConnectionPool() { clear(); }

	bool connect( int32 connCnt, const WCHAR* connStr );
	void clear();

	void push( DBConnection* conn );
	DBConnection* pop();

private:
	std::mutex poolMutex_;
	SQLHENV hEnv_;
	std::vector<DBConnection*> conns_;
};

#endif // database_connection_pool_hpp
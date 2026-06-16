#ifndef database_connection_hpp
#define database_connection_hpp

#include <sql.h>
#include <sqlext.h>

/*--------------------
     DBConnection
--------------------*/

class DBConnection {
public:
	DBConnection() : hConn_( SQL_NULL_HANDLE ), hStmt_( SQL_NULL_HANDLE ) {}
	~DBConnection() { clear(); }

	bool connect( SQLHENV hEnv, const std::wstring& connStr );
	void clear();

	bool execute( const std::wstring& query );
	bool fetch();
	int32 getRowCount();
	void unbind();

	bool bindParam( SQLUSMALLINT paramIdx, SQLSMALLINT cType, SQLSMALLINT sqlType, SQLULEN len, SQLPOINTER ptr, SQLLEN* idx );
	bool bindCol( SQLUSMALLINT columnIdx, SQLSMALLINT cType, SQLULEN len, SQLPOINTER value, SQLLEN* idx );
	void handleError( SQLRETURN ret );

private:
	SQLHDBC hConn_;
	SQLHSTMT hStmt_; // statement
};

#endif // database_connection_hpp
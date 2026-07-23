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

	bool connect( SQLHENV hEnv, const WCHAR* connStr );
	void clear();

	bool execute( const WCHAR* query );
	bool fetch();
	int32 getRowCount();
	void unbind();

	// 주의: 바인딩은 포인터만 저장한다. val/str/idx는 execute()/unbind() 시점까지 살아 있어야 한다.
	bool bindParam( int32 paramIdx, bool* val, SQLLEN* idx );
	bool bindParam( int32 paramIdx, float* val, SQLLEN* idx );
	bool bindParam( int32 paramIdx, double* val, SQLLEN* idx );
	bool bindParam( int32 paramIdx, int8* val, SQLLEN* idx );
	bool bindParam( int32 paramIdx, int16* val, SQLLEN* idx );
	bool bindParam( int32 paramIdx, int32* val, SQLLEN* idx );
	bool bindParam( int32 paramIdx, int64* val, SQLLEN* idx );
	bool bindParam( int32 paramIdx, TIMESTAMP_STRUCT* val, SQLLEN* idx );
	bool bindParam( int32 paramIdx, const WCHAR* str, SQLLEN* idx );
	bool bindParam( int32 paramIdx, const byte* bin, int32 size, SQLLEN* idx );

	bool bindCol( int32 columnIdx, bool* val, SQLLEN* idx );
	bool bindCol( int32 columnIdx, float* val, SQLLEN* idx );
	bool bindCol( int32 columnIdx, double* val, SQLLEN* idx );
	bool bindCol( int32 columnIdx, int8* val, SQLLEN* idx );
	bool bindCol( int32 columnIdx, int16* val, SQLLEN* idx );
	bool bindCol( int32 columnIdx, int32* val, SQLLEN* idx );
	bool bindCol( int32 columnIdx, int64* val, SQLLEN* idx );
	bool bindCol( int32 columnIdx, TIMESTAMP_STRUCT* val, SQLLEN* idx );
	bool bindCol( int32 columnIdx, WCHAR* val, int32 size, SQLLEN* idx );
	bool bindCol( int32 columnIdx, byte* val, int32 size, SQLLEN* idx );

private:
	enum {
		WVARCHAR_MAX = 4000,
		BINARY_MAX = 8000
	};

	// SQL_PARAM_INPUT 전용이므로 드라이버는 ptr을 읽기만 한다.
	// const 버퍼를 넘기는 호출부가 const_cast를 쓰는 근거가 이것이다.
	bool bindParam( SQLUSMALLINT paramIdx, SQLSMALLINT cType, SQLSMALLINT sqlType, SQLULEN len, SQLPOINTER ptr, SQLLEN* idx );
	bool bindCol( SQLUSMALLINT columnIdx, SQLSMALLINT cType, SQLULEN len, SQLPOINTER value, SQLLEN* idx );
	void handleError( SQLRETURN ret );

	SQLHDBC hConn_;
	SQLHSTMT hStmt_; // statement
};

#endif // database_connection_hpp
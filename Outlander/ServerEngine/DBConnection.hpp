#ifndef database_connection_hpp
#define database_connection_hpp

// sql.h / sqltypes.h는 Windows 타입(WCHAR, HWND 등)에 의존하므로 반드시 먼저 와야 한다.
#include "simpleWindows.hpp"
#include "types.hpp"

#include <sql.h>
#include <sqlext.h>

// 와이드 문자열을 콘솔 코드페이지에 맞춰 변환해 stderr로 출력한다.
// wcerr + imbue 방식은 로케일과 콘솔 코드페이지가 어긋나면 한글이 깨지고,
// 좁은/넓은 스트림을 한 stderr에 섞어 쓰는 것도 문제라 DB 쪽 로그는 전부 이걸 쓴다.
// 여러 IOCP 워커가 동시에 불러도 줄이 섞이지 않도록 내부에서 직렬화한다.
void dbLogW( const WCHAR* msg );

/*--------------------
     DBConnection
--------------------*/

class DBConnection {
public:
	DBConnection() : hConn_( SQL_NULL_HANDLE ), hStmt_( SQL_NULL_HANDLE ) {}
	~DBConnection() { clear(); }

	// 핸들 소유권을 가지므로 복사하면 이중 해제가 된다.
	DBConnection( const DBConnection& ) = delete;
	DBConnection& operator=( const DBConnection& ) = delete;

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

	// size는 모두 "바이트" 단위다 (SQLBindCol의 BufferLength가 바이트이므로).
	bool bindColumn( int32 columnIdx, bool* val, SQLLEN* idx );
	bool bindColumn( int32 columnIdx, float* val, SQLLEN* idx );
	bool bindColumn( int32 columnIdx, double* val, SQLLEN* idx );
	bool bindColumn( int32 columnIdx, int8* val, SQLLEN* idx );
	bool bindColumn( int32 columnIdx, int16* val, SQLLEN* idx );
	bool bindColumn( int32 columnIdx, int32* val, SQLLEN* idx );
	bool bindColumn( int32 columnIdx, int64* val, SQLLEN* idx );
	bool bindColumn( int32 columnIdx, TIMESTAMP_STRUCT* val, SQLLEN* idx );
	bool bindColumn( int32 columnIdx, WCHAR* str, int32 size, SQLLEN* idx );
	bool bindColumn( int32 columnIdx, byte* bin, int32 size, SQLLEN* idx );

private:
	enum {
		WVARCHAR_MAX = 4000,
		BINARY_MAX = 8000
	};

	// SQL_PARAM_INPUT 전용이므로 드라이버는 ptr을 읽기만 한다.
	// const 버퍼를 넘기는 호출부가 const_cast를 쓰는 근거가 이것이다.
	bool bindParam( SQLUSMALLINT paramIdx, SQLSMALLINT cType, SQLSMALLINT sqlType, SQLULEN len, SQLPOINTER ptr, SQLLEN* idx );
	bool bindColumn( SQLUSMALLINT columnIdx, SQLSMALLINT cType, SQLULEN len, SQLPOINTER value, SQLLEN* idx );

	// 진단 레코드는 실패한 핸들에서 읽어야 한다.
	// connect() 실패 시점에는 hStmt_가 아직 없으므로 DBC 핸들을 넘겨야 원인을 볼 수 있다.
	void handleError( SQLSMALLINT handleType, SQLHANDLE handle, SQLRETURN ret );

	SQLHDBC hConn_;
	SQLHSTMT hStmt_; // statement
};

#endif // database_connection_hpp

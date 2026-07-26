#include "sepch.hpp"
#include "DBConnection.hpp"

#include <sstream>

/*--------------------
     DBConnection
--------------------*/

namespace {
	// 여러 IOCP 워커가 동시에 에러를 찍으면 출력이 뒤섞이므로 직렬화한다.
	std::mutex gDbLogMutex;
}

void dbLogW( const WCHAR* msg ) {
	if ( msg == nullptr ) {
		return;
	}

	// 콘솔이 CP949든 UTF-8이든 그 코드페이지로 직접 변환한다.
	// 리다이렉션되어 콘솔이 없으면 GetConsoleOutputCP()가 0을 주므로 UTF-8로 떨어뜨린다.
	UINT codePage = ::GetConsoleOutputCP();
	if ( codePage == 0 ) {
		codePage = CP_UTF8;
	}

	const int32 len = ::WideCharToMultiByte( codePage, 0, msg, -1, nullptr, 0, nullptr, nullptr );
	if ( len <= 1 ) {
		return;
	}

	std::string out( static_cast<size_t>( len ) - 1, '\0' );
	::WideCharToMultiByte( codePage, 0, msg, -1, out.data(), len, nullptr, nullptr );

	std::lock_guard lock( gDbLogMutex );
	std::cerr << out << std::endl;
}

bool DBConnection::connect( SQLHENV hEnv, const WCHAR* connStr ) {
	SQLRETURN ret = ::SQLAllocHandle( SQL_HANDLE_DBC, hEnv, &hConn_ );
	if ( !SQL_SUCCEEDED( ret ) ) {
		handleError( SQL_HANDLE_ENV, hEnv, ret );
		return false;
	}

	WCHAR resultStr[ MAX_PATH ]{};
	SQLSMALLINT resultStrLen{};

	// 연결 문자열을 지역 버퍼에 복사하지 않는다.
	// MAX_PATH를 넘는 문자열이 들어오면 wcscpy_s가 프로세스를 즉시 죽였다.
	// SQL_NTS를 주면 드라이버가 널 종료까지 알아서 읽는다.
	ret = ::SQLDriverConnect(
		hConn_, nullptr,
		const_cast<SQLWCHAR*>( reinterpret_cast<const SQLWCHAR*>( connStr ) ), SQL_NTS,
		reinterpret_cast<SQLWCHAR*>( resultStr ), _countof( resultStr ),
		&resultStrLen, SQL_DRIVER_NOPROMPT
	);

	// 연결 실패 원인(SQLSTATE/네이티브 에러)은 DBC 핸들에만 남는다.
	// STMT 할당을 시도하기 전에 여기서 빠져나가야 원인을 볼 수 있다.
	if ( !SQL_SUCCEEDED( ret ) ) {
		handleError( SQL_HANDLE_DBC, hConn_, ret );
		return false;
	}

	ret = ::SQLAllocHandle( SQL_HANDLE_STMT, hConn_, &hStmt_ );
	if ( !SQL_SUCCEEDED( ret ) ) {
		handleError( SQL_HANDLE_DBC, hConn_, ret );
		return false;
	}

	return true;
}

void DBConnection::clear() {
	// ODBC 규약상 자식(STMT)부터 해제하고, 연결을 끊은 뒤 DBC를 해제해야 한다.
	// 순서를 어기면 SQLFreeHandle이 HY010으로 실패해서 핸들이 그대로 샌다.
	if ( hStmt_ != SQL_NULL_HANDLE ) {
		::SQLFreeHandle( SQL_HANDLE_STMT, hStmt_ );
		hStmt_ = SQL_NULL_HANDLE;
	}

	if ( hConn_ != SQL_NULL_HANDLE ) {
		::SQLDisconnect( hConn_ );
		::SQLFreeHandle( SQL_HANDLE_DBC, hConn_ );
		hConn_ = SQL_NULL_HANDLE;
	}
}

bool DBConnection::execute( const WCHAR* query ) {
	const SQLWCHAR* sqlQuery = reinterpret_cast<const SQLWCHAR*>(query);

	SQLRETURN ret = ::SQLExecDirect( hStmt_, const_cast<SQLWCHAR*>(sqlQuery), SQL_NTSL );

	// SQL_NO_DATA는 오류가 아니다. searched DELETE/UPDATE가 0행에 영향을 주면 SQLExecDirect가
	// 이걸 돌려준다(ODBC 규약). 실패로 처리하면 "아직 행이 없는 키를 DELETE 후 INSERT"하는
	// upsert 패턴이 첫 시도부터 영구히 막힌다 — 행이 없으니 DELETE가 실패하고, 실패했으니
	// INSERT를 못 해서 영영 행이 생기지 않는다.
	// SELECT는 결과가 비어도 SQL_SUCCESS이며, 빈 결과는 fetch()의 SQL_NO_DATA로 구분된다.
	// 영향 행 수가 필요하면 getRowCount()를 쓸 것.
    if( ret == SQL_SUCCESS || ret == SQL_SUCCESS_WITH_INFO || ret == SQL_NO_DATA ) {
        return true;
	}

	handleError( SQL_HANDLE_STMT, hStmt_, ret );
	return false;
}

bool DBConnection::fetch() {
	SQLRETURN ret = ::SQLFetch( hStmt_ );

    switch ( ret ) {
    case SQL_SUCCESS:
    case SQL_SUCCESS_WITH_INFO:
        return true;

    case SQL_NO_DATA:
        return false;

    case SQL_ERROR:
		handleError( SQL_HANDLE_STMT, hStmt_, ret );
        return false;

    default:
        return true;
    }
}

int32 DBConnection::getRowCount() {
    SQLLEN cnt{};
	SQLRETURN ret = ::SQLRowCount( hStmt_, &cnt );

    if ( ret == SQL_SUCCESS || ret == SQL_SUCCESS_WITH_INFO ) {
        return static_cast<int32>(cnt);
	}

	return -1;
}

void DBConnection::unbind() {
	::SQLFreeStmt( hStmt_, SQL_UNBIND );
	::SQLFreeStmt( hStmt_, SQL_RESET_PARAMS );
	::SQLFreeStmt( hStmt_, SQL_CLOSE );
}

bool DBConnection::bindParam( int32 paramIdx, bool* val, SQLLEN* idx ) {
    return bindParam( paramIdx, SQL_C_TINYINT, SQL_TINYINT, sizeof(bool), val, idx );
}

bool DBConnection::bindParam( int32 paramIdx, float* val, SQLLEN* idx ) {
	return bindParam( paramIdx, SQL_C_FLOAT, SQL_REAL, 0, val, idx );
}

bool DBConnection::bindParam( int32 paramIdx, double* val, SQLLEN* idx ) {
	return bindParam( paramIdx, SQL_C_DOUBLE, SQL_DOUBLE, 0, val, idx );
}

bool DBConnection::bindParam( int32 paramIdx, int8* val, SQLLEN* idx ) {
    return bindParam( paramIdx, SQL_C_TINYINT, SQL_TINYINT, sizeof( int8 ), val, idx );
}

bool DBConnection::bindParam( int32 paramIdx, int16* val, SQLLEN* idx ) {
	return bindParam( paramIdx, SQL_C_SHORT, SQL_SMALLINT, sizeof( int16 ), val, idx );
}

bool DBConnection::bindParam( int32 paramIdx, int32* val, SQLLEN* idx ) {
	return bindParam( paramIdx, SQL_C_LONG, SQL_INTEGER, sizeof( int32 ), val, idx );
}

bool DBConnection::bindParam( int32 paramIdx, int64* val, SQLLEN* idx ) {
	return bindParam( paramIdx, SQL_C_SBIGINT, SQL_BIGINT, sizeof( int64 ), val, idx );
}

bool DBConnection::bindParam( int32 paramIdx, TIMESTAMP_STRUCT* val, SQLLEN* idx ) {
	return bindParam( paramIdx, SQL_C_TYPE_TIMESTAMP, SQL_TYPE_TIMESTAMP, sizeof( TIMESTAMP_STRUCT ), val, idx );
}

bool DBConnection::bindParam( int32 paramIdx, const WCHAR* str, SQLLEN* idx ) {
	SQLULEN size = wcslen( str );
    *idx = SQL_NTSL;

	// ColumnSize 0은 드라이버가 "invalid precision value"로 거부할 수 있다.
	// 빈 문자열도 정상 파라미터이므로 최소 1로 올린다.
	if ( size == 0 ) {
		size = 1;
	}

	// ODBC는 const를 모르는 C API다. SQL_PARAM_INPUT으로만 바인딩하므로 드라이버는 이 버퍼를 읽기만 한다.
	// SQLPOINTER는 void*이고 객체 포인터 -> void*는 암시적 변환이므로, 벗겨낼 것은 const뿐이다.
	if ( size > static_cast<SQLULEN>( WVARCHAR_MAX ) ) {
		return bindParam( paramIdx, SQL_C_WCHAR, SQL_WLONGVARCHAR, size, const_cast<WCHAR*>( str ), idx );
	}
    else {
        return bindParam( paramIdx, SQL_C_WCHAR, SQL_WVARCHAR, size, const_cast<WCHAR*>( str ), idx );
    }
}

bool DBConnection::bindParam( int32 paramIdx, const byte* bin, int32 size, SQLLEN* idx ) {
    if ( bin == nullptr ) {
        *idx = SQL_NULL_DATA;
        size = 1;
    }
    else {
        *idx = size;
    }

    if ( size > BINARY_MAX ) {
		return bindParam( paramIdx, SQL_C_BINARY, SQL_LONGVARBINARY, size, const_cast<byte*>(bin), idx );
    }
    else {
		return bindParam( paramIdx, SQL_C_BINARY, SQL_BINARY, size, const_cast<byte*>(bin), idx );
    }
}

bool DBConnection::bindColumn( int32 columnIdx, bool* val, SQLLEN* idx ) {
	return bindColumn( columnIdx, SQL_C_TINYINT, sizeof( bool ), val, idx );
}

bool DBConnection::bindColumn( int32 columnIdx, float* val, SQLLEN* idx ) {
	return bindColumn( columnIdx, SQL_C_FLOAT, sizeof( float ), val, idx );
}

bool DBConnection::bindColumn( int32 columnIdx, double* val, SQLLEN* idx ) {
	return bindColumn( columnIdx, SQL_C_DOUBLE, sizeof( double ), val, idx );
}

bool DBConnection::bindColumn( int32 columnIdx, int8* val, SQLLEN* idx ) {
	return bindColumn( columnIdx, SQL_C_TINYINT, sizeof( int8 ), val, idx );
}

bool DBConnection::bindColumn( int32 columnIdx, int16* val, SQLLEN* idx ) {
	return bindColumn( columnIdx, SQL_C_SHORT, sizeof( int16 ), val, idx );
}

bool DBConnection::bindColumn( int32 columnIdx, int32* val, SQLLEN* idx ) {
	return bindColumn( columnIdx, SQL_C_LONG, sizeof( int32 ), val, idx );
}

bool DBConnection::bindColumn( int32 columnIdx, int64* val, SQLLEN* idx ) {
	return bindColumn( columnIdx, SQL_C_SBIGINT, sizeof( int64 ), val, idx );
}

bool DBConnection::bindColumn( int32 columnIdx, TIMESTAMP_STRUCT* val, SQLLEN* idx ) {
	return bindColumn( columnIdx, SQL_C_TYPE_TIMESTAMP, sizeof( TIMESTAMP_STRUCT ), val, idx );
}

bool DBConnection::bindColumn( int32 columnIdx, WCHAR* str, int32 size, SQLLEN* idx ) {
	return bindColumn( columnIdx, SQL_C_WCHAR, size, str, idx );
}

bool DBConnection::bindColumn( int32 columnIdx, byte* bin, int32 size, SQLLEN* idx ) {
	return bindColumn( columnIdx, SQL_C_BINARY, size, bin, idx );
}

bool DBConnection::bindParam( SQLUSMALLINT paramIdx, SQLSMALLINT cType, SQLSMALLINT sqlType, SQLULEN len, SQLPOINTER ptr, SQLLEN* idx ) {
    SQLRETURN ret = ::SQLBindParameter( hStmt_, paramIdx, SQL_PARAM_INPUT, cType, sqlType, len, 0, ptr, 0, idx );
    if ( !SQL_SUCCEEDED( ret ) ) {
        handleError( SQL_HANDLE_STMT, hStmt_, ret );
        return false;
    }

	return true;
}

bool DBConnection::bindColumn( SQLUSMALLINT columnIdx, SQLSMALLINT cType, SQLULEN len, SQLPOINTER value, SQLLEN* idx ) {
	SQLRETURN ret = ::SQLBindCol( hStmt_, columnIdx, cType, value, len, idx );
    if ( !SQL_SUCCEEDED( ret ) ) {
        handleError( SQL_HANDLE_STMT, hStmt_, ret );
        return false;
	}

	return true;
}

void DBConnection::handleError( SQLSMALLINT handleType, SQLHANDLE handle, SQLRETURN ret ) {
    if ( SQL_SUCCEEDED( ret ) || handle == SQL_NULL_HANDLE ) {
        return;
    }

    SQLSMALLINT idx{ 1 };
    SQLWCHAR sqlState[ 6 ]{};       // SQLSTATE는 5글자 + 널
    SQLINTEGER nativeErr{};
	SQLWCHAR errMsg[ MAX_PATH ]{};
	SQLSMALLINT errMsgLen{};

    while ( true ) {
		SQLRETURN errRet = ::SQLGetDiagRec( handleType, handle, idx, sqlState,
            &nativeErr, errMsg, _countof( errMsg ), &errMsgLen );

		// SQL_NO_DATA(진단 레코드 소진)와 실제 실패를 함께 걸러낸다.
        if ( !SQL_SUCCEEDED( errRet ) ) {
            break;
		}

		std::wostringstream oss;
		oss << L"[DB] SQLSTATE=" << reinterpret_cast<const WCHAR*>( sqlState )
			<< L" native=" << nativeErr
			<< L" : " << reinterpret_cast<const WCHAR*>( errMsg );
		dbLogW( oss.str().c_str() );

		++idx;
    }
}

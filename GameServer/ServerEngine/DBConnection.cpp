#include "sepch.hpp"
#include "DBConnection.hpp"

/*--------------------
     DBConnection
--------------------*/

bool DBConnection::connect( SQLHENV hEnv, const WCHAR* connStr ) {
    if( ::SQLAllocHandle( SQL_HANDLE_DBC, hEnv, &hConn_) != SQL_SUCCESS ) {
        return false;
	}

	WCHAR str[ MAX_PATH ]{};
	wcscpy_s( str, connStr );

    WCHAR resultStr[ MAX_PATH ]{};
	SQLSMALLINT resultStrLen{};

    SQLRETURN ret = ::SQLDriverConnect(
        hConn_, nullptr,
        reinterpret_cast<SQLWCHAR*>(str), _countof( str ),
        reinterpret_cast<SQLWCHAR*>(resultStr), _countof( resultStr ),
        &resultStrLen, SQL_DRIVER_NOPROMPT
    );

    if ( ::SQLAllocHandle( SQL_HANDLE_STMT, hConn_, &hStmt_ ) != SQL_SUCCESS ) {
		return false;
    }

    return ( ret == SQL_SUCCESS || ret == SQL_SUCCESS_WITH_INFO );
}

void DBConnection::clear() {
    if ( hConn_ != SQL_NULL_HANDLE ) {
		::SQLFreeHandle( SQL_HANDLE_DBC, hConn_ );
		hConn_ = SQL_NULL_HANDLE;
    }

    if ( hStmt_ != SQL_NULL_HANDLE ) {
        ::SQLFreeHandle( SQL_HANDLE_STMT, hStmt_ );
        hStmt_ = SQL_NULL_HANDLE;
    }
}

bool DBConnection::execute( const WCHAR* query ) {
	const SQLWCHAR* sqlQuery = reinterpret_cast<const SQLWCHAR*>(query);

	SQLRETURN ret = ::SQLExecDirect( hStmt_, const_cast<SQLWCHAR*>(sqlQuery), SQL_NTSL );
    if( ret == SQL_SUCCESS || ret == SQL_SUCCESS_WITH_INFO ) {
        return true;
	}

	handleError( ret );
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
		handleError( ret );
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
    return false;
}

bool DBConnection::bindParam( SQLUSMALLINT paramIdx, SQLSMALLINT cType, SQLSMALLINT sqlType, SQLULEN len, SQLPOINTER ptr, SQLLEN* idx ) {
    SQLRETURN ret = ::SQLBindParameter( hStmt_, paramIdx, SQL_PARAM_INPUT, cType, sqlType, len, 0, ptr, 0, idx );
    if ( ret != SQL_SUCCESS && ret != SQL_SUCCESS_WITH_INFO ) {
        handleError( ret );
        return false;
    }

	return true;
}

bool DBConnection::bindCol( SQLUSMALLINT columnIdx, SQLSMALLINT cType, SQLULEN len, SQLPOINTER value, SQLLEN* idx ) {
	SQLRETURN ret = ::SQLBindCol( hStmt_, columnIdx, cType, value, len, idx );
    if ( ret != SQL_SUCCESS && ret != SQL_SUCCESS_WITH_INFO ) {
        handleError( ret );
        return false;
	}

	return true;
}

void DBConnection::handleError( SQLRETURN ret ) {
    if ( ret == SQL_SUCCESS || ret == SQL_SUCCESS_WITH_INFO ) {
        return;
    }

    SQLSMALLINT idx{ 1 };
    SQLWCHAR sqlState[ MAX_PATH ]{};
    SQLINTEGER nativeErr{};
	SQLWCHAR errMsg[ MAX_PATH ]{};
	SQLSMALLINT errMsgLen{};
	SQLRETURN errRet{};

    while ( true ) {
		errRet = ::SQLGetDiagRec( SQL_HANDLE_STMT, hStmt_, idx, sqlState,
            &nativeErr, errMsg, _countof( errMsg ), &errMsgLen );

        if ( errRet == SQL_NO_DATA ) {
            break;
        }
        if( errRet != SQL_SUCCESS && errRet != SQL_SUCCESS_WITH_INFO ) {
            break;
		}

        // Log
		std::wcout.imbue( std::locale( "kor" ) );
		std::wcout << errMsg << std::endl;

		++idx;
    }
}

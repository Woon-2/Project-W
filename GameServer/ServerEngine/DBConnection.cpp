#include "sepch.hpp"
#include "DBConnection.hpp"

/*--------------------
     DBConnection
--------------------*/

bool DBConnection::connect( SQLHENV hEnv, const std::wstring& connStr ) {
    if( ::SQLAllocHandle( SQL_HANDLE_DBC, hEnv, &hConn_) != SQL_SUCCESS ) {
        return false;
	}

	WCHAR str[ MAX_PATH ]{};
	wcscpy_s( str, connStr.c_str() );

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
    
}

bool DBConnection::execute( const std::wstring& query ) {
    
}

bool DBConnection::fetch() {
    
}

int32 DBConnection::getRowCount() {
    
}

void DBConnection::unbind() {
    
}

bool DBConnection::bindParam( SQLUSMALLINT paramIdx, SQLSMALLINT cType, SQLSMALLINT sqlType, SQLULEN len, SQLPOINTER ptr, SQLLEN* idx ) {
    
}

bool DBConnection::bindCol( SQLUSMALLINT columnIdx, SQLSMALLINT cType, SQLULEN len, SQLPOINTER value, SQLLEN* idx ) {
    
}

void DBConnection::handleError( SQLRETURN ret ) {
    
}

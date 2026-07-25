#ifndef database_binder_hpp
#define database_binder_hpp

#include "DBConnection.hpp"
#include "types.hpp"
#include "macro.hpp"
#include <Windows.h>

template<int32 N>
struct FullBits { enum { value = (1 << (N - 1)) | FullBits<N - 1>::value }; };

template<>
struct FullBits<1> { enum { value = 1 }; };

template<>
struct FullBits<0> { enum { value = 0 }; };

template<int32 paramCnt, int32 columnCnt>
class DBBinder {
public:
	DBBinder( DBConnection& dbConn, const WCHAR* query )
		: dbConn_( dbConn ), query_( query ), paramFlag_( 0ull ), columnFlag_( 0ull )
	{
		memset( paramIdxs_, 0, sizeof( paramIdxs_ ) );
		memset( columnIdxs_, 0, sizeof( columnIdxs_ ) );
		dbConn_.unbind();
	}

	bool validate() const {
		return ( paramFlag_ == FullBits<paramCnt>::value && columnFlag_ == FullBits<columnCnt>::value );
	}

	bool execute() {
		ASSERT_CRASH( validate() );
		return dbConn_.execute( query_ );
	}

	bool fetch() {
		return dbConn_.fetch();
	}

public:
	template<class T>
	void bindParam( int32 paramIdx, T& val ) {
		dbConn_.bindParam( paramIdx + 1, &val, &paramIdxs_[ paramIdx ] );
		paramFlag_ |= (1ull << paramIdx);
	}

	void bindParam( int32 paramIdx, const WCHAR* val ) {
		dbConn_.bindParam( paramIdx + 1, val, &paramIdx_[ paramIdx ] );
		paramFlag_ |= (1ull << paramIdx);
	}

	template<class T, int32 N>
	void bindParam( int32 paramIdx, T( &val )[ N ] ) {
		dbConn_.bindParam( paramIdx + 1, reinterpret_cast<const byte*>(val), sizeof( T ) * N, &paramIdx_[ paramIdx ] );
		paramFlag_ |= (1ull << paramIdx);
	}

	template<class T>
	void bindParam( int32 paramIdx, T* val, int32 N ) {
		dbConn_.bindParam( paramIdx + 1, reinterpret_cast<const byte*>(val), sizeof( T ) * N, &paramIdx_[ paramIdx ] );
		paramFlag_ |= (1ull << paramIdx);
	}

	template<class T>
	void bindColumn( int32 columnIdx, T& val ) {
		dbConn_.bindColumn( columnIdx + 1, &val, &columnIdx_[ columnIdx ] );
		columnFlag_ |= (1ull << columnIdx);
	}

	template<int32 N>
	void bindColumn( int32 columnIdx, WCHAR( &val )[ N ] ) {
		dbConn_.bindColumn( columnIdx + 1, val, N - 1, &columnIdx_[ columnIdx ] );
		columnFlag_ |= (1ull << columnIdx);
	}

	void bindColumn( int32 columnIdx, WCHAR* val, int32 len ) {
		dbConn_.bindColumn( columnIdx + 1, val, len - 1, &columnIdx_[ columnIdx ] );
		columnFlag_ |= (1ull << columnIdx);
	}

	template<class T, int32 N>
	void bindColumn( int32 columnIdx, T( &val )[ N ] ) {
		dbConn_.bindColumn( columnIdx + 1, val, sizeof( T ) * N, &columnIdx_[ columnIdx ] );
		columnFlag_ |= (1ull << columnIdx);
	}

private:
	DBConnection& dbConn_;
	const WCHAR* query_;
	SQLLEN paramIdxs_[ paramCnt > 0 ? paramCnt : 1 ];
	SQLLEN columnIdxs_[ columnCnt > 0 ? columnCnt : 1 ];
	uint64 paramFlag_;
	uint64 columnFlag_;
};

#endif // database_binder_hpp
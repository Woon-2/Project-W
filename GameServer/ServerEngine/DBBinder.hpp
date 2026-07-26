#ifndef database_binder_hpp
#define database_binder_hpp

#include "DBConnection.hpp"
#include "macro.hpp"

#include <cassert>
#include <type_traits>

/*--------------------
      DBBinder
--------------------*/

// N개가 전부 채워진 상태를 나타내는 비트마스크.
// paramFlag_/columnFlag_가 uint64이므로 마스크도 uint64로 맞춘다.
// (int로 계산하면 N=32에서 부호비트를 건드려 비교가 항상 실패한다)
//
// N==64일 때 1ull << 64는 UB다. 삼항 연산자로 쓰면 고르지 않은 쪽도 파싱되어
// C4293이 뜨므로, if constexpr로 아예 버려야 한다.
template<int32 N>
constexpr uint64 makeFullBits() {
	if constexpr ( N >= 64 ) {
		return ~0ull;
	}
	else {
		return ( 1ull << N ) - 1;
	}
}

template<int32 N>
inline constexpr uint64 fullBits = makeFullBits<N>();

// 쿼리 하나의 파라미터/컬럼 바인딩을 묶어서 관리한다.
// 인덱스는 0-based로 받아 내부에서 +1 해 ODBC의 1-based로 넘긴다.
//
// 주의: ODBC 바인딩은 버퍼의 "포인터만" 저장한다.
// 바인딩한 변수와 이 DBBinder 객체는 execute()/fetch()가 끝날 때까지 살아 있어야 한다.
template<int32 paramCnt, int32 columnCnt>
class DBBinder {
	static_assert( paramCnt >= 0 && paramCnt <= 64, "paramCnt는 0~64여야 한다 (플래그가 uint64다)" );
	static_assert( columnCnt >= 0 && columnCnt <= 64, "columnCnt는 0~64여야 한다 (플래그가 uint64다)" );

public:
	DBBinder( DBConnection& dbConn, const WCHAR* query )
		: dbConn_( dbConn ), query_( query ), paramIdxs_{}, columnIdxs_{}, paramFlag_( 0ull ), columnFlag_( 0ull )
	{
		dbConn_.unbind();
	}

	DBBinder( const DBBinder& ) = delete;
	DBBinder& operator=( const DBBinder& ) = delete;

	// 선언한 파라미터/컬럼이 빠짐없이 바인딩됐는지 검사한다.
	bool validate() const {
		return ( paramFlag_ == fullBits<paramCnt> && columnFlag_ == fullBits<columnCnt> );
	}

	bool execute() {
		// 바인딩 누락은 호출부 실수다. 개발 중에는 assert로 즉시 잡고,
		// Release(NDEBUG)에서는 프로세스를 죽이지 않고 실패로 흘린다.
		if ( !validate() ) {
			dbLogW( L"[DB] DBBinder: 바인딩되지 않은 파라미터/컬럼이 있다" );
			dbLogW( query_ );
			assert( false && "DBBinder: 선언한 파라미터/컬럼 중 바인딩되지 않은 것이 있다" );
			return false;
		}

		return dbConn_.execute( query_ );
	}

	bool fetch() {
		return dbConn_.fetch();
	}

public:
	template<class T>
	void bindParam( int32 paramIdx, T& val ) {
		ASSERT_CRASH( 0 <= paramIdx && paramIdx < paramCnt );
		dbConn_.bindParam( paramIdx + 1, &val, &paramIdxs_[ paramIdx ] );
		paramFlag_ |= ( 1ull << paramIdx );
	}

	void bindParam( int32 paramIdx, const WCHAR* val ) {
		ASSERT_CRASH( 0 <= paramIdx && paramIdx < paramCnt );
		dbConn_.bindParam( paramIdx + 1, val, &paramIdxs_[ paramIdx ] );
		paramFlag_ |= ( 1ull << paramIdx );
	}

	// 배열 인자는 이 오버로드가 exact match로 이긴다.
	// WCHAR 배열까지 바이너리로 묶으면 nvarchar 파라미터가 조용히 깨지므로 타입으로 분기한다.
	template<class T, int32 N>
	void bindParam( int32 paramIdx, T( &val )[ N ] ) {
		ASSERT_CRASH( 0 <= paramIdx && paramIdx < paramCnt );

		if constexpr ( std::is_same_v<std::remove_const_t<T>, WCHAR> ) {
			dbConn_.bindParam( paramIdx + 1, static_cast<const WCHAR*>( val ), &paramIdxs_[ paramIdx ] );
		}
		else {
			dbConn_.bindParam( paramIdx + 1, reinterpret_cast<const byte*>( val ), sizeof( T ) * N, &paramIdxs_[ paramIdx ] );
		}

		paramFlag_ |= ( 1ull << paramIdx );
	}

	// 원소 개수를 런타임으로 넘기는 바이너리 바인딩.
	template<class T>
	void bindParam( int32 paramIdx, T* val, int32 N ) {
		ASSERT_CRASH( 0 <= paramIdx && paramIdx < paramCnt );
		dbConn_.bindParam( paramIdx + 1, reinterpret_cast<const byte*>( val ), sizeof( T ) * N, &paramIdxs_[ paramIdx ] );
		paramFlag_ |= ( 1ull << paramIdx );
	}

	template<class T>
	void bindColumn( int32 columnIdx, T& val ) {
		ASSERT_CRASH( 0 <= columnIdx && columnIdx < columnCnt );
		dbConn_.bindColumn( columnIdx + 1, &val, &columnIdxs_[ columnIdx ] );
		columnFlag_ |= ( 1ull << columnIdx );
	}

	// SQLBindCol의 BufferLength는 "바이트" 단위다.
	// 문자 개수를 넘기면 드라이버가 버퍼 절반에서 문자열을 잘라버린다.
	// 널 종료 문자 자리를 남기려고 N-1개 문자 분량만 준다.
	template<int32 N>
	void bindColumn( int32 columnIdx, WCHAR( &val )[ N ] ) {
		ASSERT_CRASH( 0 <= columnIdx && columnIdx < columnCnt );
		dbConn_.bindColumn( columnIdx + 1, val, ( N - 1 ) * sizeof( WCHAR ), &columnIdxs_[ columnIdx ] );
		columnFlag_ |= ( 1ull << columnIdx );
	}

	// len은 val이 담을 수 있는 "문자 개수"다 (바이트 아님).
	void bindColumn( int32 columnIdx, WCHAR* val, int32 len ) {
		ASSERT_CRASH( 0 <= columnIdx && columnIdx < columnCnt );
		dbConn_.bindColumn( columnIdx + 1, val, ( len - 1 ) * sizeof( WCHAR ), &columnIdxs_[ columnIdx ] );
		columnFlag_ |= ( 1ull << columnIdx );
	}

	template<class T, int32 N>
	void bindColumn( int32 columnIdx, T( &val )[ N ] ) {
		ASSERT_CRASH( 0 <= columnIdx && columnIdx < columnCnt );
		dbConn_.bindColumn( columnIdx + 1, reinterpret_cast<byte*>( val ), sizeof( T ) * N, &columnIdxs_[ columnIdx ] );
		columnFlag_ |= ( 1ull << columnIdx );
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

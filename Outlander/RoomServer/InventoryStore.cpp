#include "rspch.hpp"
#include "InventoryStore.hpp"

#include "DBExecutor.hpp"
#include "DBConnectionPool.hpp"
#include "DBBinder.hpp"

#include <algorithm>

namespace InventoryStore {

LoadStatus load( int64 accountId, uint8 slotCount, std::vector<ItemStack>& out ) {
	out.assign( slotCount, ItemStack{} );

	DBConnectionPoolGuard guard( DBExecutor::pool() );
	if ( !guard ) {
		return LoadStatus::DbError;
	}

	// 바인딩은 포인터만 저장하므로 이 변수들은 fetch 루프가 끝날 때까지 살아 있어야 한다.
	int32 slotIndex = 0;
	int32 itemId = 0;
	int32 itemCount = 0;

	DBBinder<1, 3> binder( *guard.get(),
		L"SELECT slotIndex, itemId, itemCount FROM dbo.Inventory WHERE accountId = ? ORDER BY slotIndex" );
	binder.bindParam( 0, accountId );
	binder.bindColumn( 0, slotIndex );
	binder.bindColumn( 1, itemId );
	binder.bindColumn( 2, itemCount );

	if ( !binder.execute() ) {
		return LoadStatus::DbError;
	}

	int32 rows = 0;
	while ( binder.fetch() ) {
		++rows;

		// 카탈로그가 줄어든 뒤 남아 있는 행은 버린다(슬롯 수는 inventory.json이 정한다).
		if ( slotIndex < 0 || slotIndex >= static_cast<int32>( slotCount ) ) {
			continue;
		}
		if ( itemId <= 0 || itemCount <= 0 ) {
			continue;   // 빈 슬롯
		}

		out[ slotIndex ] = ItemStack{
			static_cast<ItemId>( itemId ),
			static_cast<std::uint16_t>( std::min( itemCount, 0xFFFF ) )
		};
	}

	return rows == 0 ? LoadStatus::NewAccount : LoadStatus::Ok;
}

bool save( int64 accountId, const std::vector<ItemStack>& slots ) {
	DBConnectionPoolGuard guard( DBExecutor::pool() );
	if ( !guard ) {
		return false;
	}
	DBConnection& conn = *guard.get();

	// ODBC 기본은 autocommit이다. DELETE와 INSERT 사이에서 실패해도 인벤토리가 통째로
	// 사라지지 않도록 T-SQL 명시적 트랜잭션으로 감싼다.
	// DBBinder 생성자의 unbind()는 커서(SQL_CLOSE)만 닫으므로 트랜잭션은 바인더를 넘어 유지된다.
	{
		DBBinder<0, 0> begin( conn, L"BEGIN TRANSACTION" );
		if ( !begin.execute() ) {
			return false;
		}
	}

	bool ok = false;
	{
		DBBinder<1, 0> del( conn, L"DELETE FROM dbo.Inventory WHERE accountId = ?" );
		del.bindParam( 0, accountId );
		ok = del.execute();
	}

	for ( std::size_t i = 0; ok && i < slots.size(); ++i ) {
		// 바인딩 변수는 바인더와 같은 스코프에 둔다(포인터 바인딩이라 수명이 겹쳐야 한다).
		int32 slotIndex = static_cast<int32>( i );
		int32 itemId = static_cast<int32>( slots[ i ].itemId );
		int32 itemCount = static_cast<int32>( slots[ i ].quantity );

		DBBinder<4, 0> ins( conn,
			L"INSERT INTO dbo.Inventory (accountId, slotIndex, itemId, itemCount) VALUES (?, ?, ?, ?)" );
		ins.bindParam( 0, accountId );
		ins.bindParam( 1, slotIndex );
		ins.bindParam( 2, itemId );
		ins.bindParam( 3, itemCount );
		ok = ins.execute();
	}

	{
		DBBinder<0, 0> fin( conn, ok ? L"COMMIT TRANSACTION" : L"ROLLBACK TRANSACTION" );
		fin.execute();
	}

	return ok;
}

}

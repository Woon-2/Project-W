#ifndef inventory_store_hpp
#define inventory_store_hpp

#include "inventory.hpp"

#include <vector>

// dbo.Inventory 읽기/쓰기. **DB 전용 스레드(DBExecutor)에서만** 호출한다.
// ODBC 호출은 블로킹이라 IOCP 워커나 방 잡 스레드에서 부르면 그동안 그 스레드가 멈춘다.
//
// 저장 형식은 고정 슬롯 배열 그대로다(빈 슬롯도 itemId=0으로 한 행 차지).
// 덕분에 "행 0개"가 곧 신규 계정이라는 뜻이 되어, 다 써서 비워진 인벤토리와 구분된다.
// 상세: RoomServer/docs/inventoryPersistence.md
namespace InventoryStore {

enum class LoadStatus : uint8 {
	Ok,           // 저장된 인벤토리를 읽었다
	NewAccount,   // 행이 없다 — 스타터 로드아웃을 그대로 확정하면 된다
	DbError,      // 커넥션/쿼리 실패 — 저장하면 안 된다
};

// out은 항상 slotCount 크기로 채워진다(실패 시 전부 빈 슬롯).
LoadStatus load( int64 accountId, uint8 slotCount, std::vector<ItemStack>& out );

// 트랜잭션 안에서 DELETE 후 전체 슬롯을 INSERT 한다. 실패 시 ROLLBACK.
bool save( int64 accountId, const std::vector<ItemStack>& slots );

}

#endif // inventory_store_hpp

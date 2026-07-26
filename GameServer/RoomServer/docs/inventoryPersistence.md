# 인벤토리 영속화 (룸서버 ↔ dbo.Inventory)

작성: 2026-07-26

인벤토리를 계정 단위로 `dbo.Inventory`에 저장/복원한다. 계정 정보는 입장 티켓으로 확정된다
(`ServerEngine/docs/entryTicket.md`).

## 스키마

```sql
CREATE TABLE dbo.Inventory (
    accountId  BIGINT  NOT NULL REFERENCES dbo.Account(accountId),
    slotIndex  INT     NOT NULL,
    itemId     INT     NOT NULL,
    itemCount  INT     NOT NULL,
    PRIMARY KEY (accountId, slotIndex)
);
```

**PK가 `(accountId, slotIndex)`인 이유:** 선반영돼 있던 원래 스키마는 `(accountId, itemId)`였는데,
그러면 같은 아이템이 `maxStack`을 넘어 여러 슬롯에 나뉘는 상태(`common/inventory.cpp`의
`Inventory::add`)를 저장할 수 없다. 고정 슬롯 모델이므로 슬롯 인덱스가 키여야 맞다.

**빈 슬롯도 `itemId=0, itemCount=0`으로 저장한다.** 그래야
"행 0개 = 신규 계정"과 "다 써서 텅 빈 인벤토리"가 구분된다. 이 구분이 없으면 DB 장애와
신규 계정을 서버가 혼동한다.

슬롯 개수는 `resources/data/inventory.json`의 `slotCount`가 정한다(스키마엔 강제되지 않음).
카탈로그가 줄어들면 범위를 벗어난 행은 로드 시 버려진다.

마이그레이션은 `db/schema.sql`에 멱등 스크립트로 들어 있다. 적용:

```
sqlcmd -S "(localdb)\MSSQLLocalDB" -i db\schema.sql
```

## 스레드 홉

`Room` 상태(모든 `Player`, `sessions_`, `idSessionMap_`)는 방의 `JobQueue` 소유다.
ODBC 호출은 블로킹이라 `DBExecutor`의 전용 스레드에서만 돌아야 한다. 그래서 4단계를 거친다.

```
IOCP 워커 ──C_Enter──▶ Room JobQueue (Room::enter)
                            │  ++pendingDbJobs_, DBExecutor::post
                            ▼
                       DB 스레드 (InventoryStore::load)
                            │  RoomManager::findRoom(roomId) → room->doAsync
                            ▼
                       Room JobQueue (Room::onInventoryLoaded)
                            │  applySnapshot + S_InventorySnapshot
                            ▼
                       onDbJobFinished()
```

**DB 스레드에서 `Player::inventory()`를 절대 만지지 않는다.** 로드 결과는 값으로 들고 와
`doAsync`로 방의 JobQueue에 되돌린 뒤에 적용한다.

`DBExecutor`가 `JobQueue`를 재사용하지 않는 이유는 `DBExecutor.hpp` 주석에 있다 —
`JobQueue::push`는 전역 `JobQueuePool`에 자동 등록되므로, 룸서버 잡 스레드들이
DB 잡을 집어가 커넥션을 동시에 쓰게 된다.

## `Room*`은 refcount가 없다 — 지연 방 제거

`RoomManager::removeRoom`은 `ObjectPool<Room>::push`로 방 객체를 **재활용한다.**
DB 잡이 raw `Room*`을 캡처하면 완료 시점에 그 포인터가 다른 방이 돼 있을 수 있다.

두 겹으로 막는다.

1. 잡은 `Room*`이 아니라 `roomId`(int32)를 캡처하고 `RoomManager::findRoom(roomId)`로 재조회한다.
2. `Room`이 **진행 중인 DB 잡 수**를 센다.

```cpp
int32 pendingDbJobs_ = 0;   // 둘 다 방의 JobQueue 전용 → 락 불필요
bool  closePending_ = false;
```

- DB 잡을 걸기 전에 `++pendingDbJobs_` (JobQueue 위에서).
- `Room::leave`에서 마지막 플레이어가 나가도, `pendingDbJobs_ != 0`이면 즉시 지우지 않고
  `closePending_ = true`로 미룬다.
- 모든 DB 완료 잡의 끝에서 `onDbJobFinished()` — `--pendingDbJobs_ == 0 && closePending_ &&
  sessions_.empty()`면 그때 `removeRoom`.

`removeRoom`을 부르는 경로는 이 둘뿐이고 양쪽 모두 카운터를 존중하므로, 잡이 떠 있는 동안
방이 사라질 수 없다. `findRoom`이 널을 반환하는 경우는 방어적 처리다.

> **알려진 한계:** `DBExecutor::shutdown()` 이후의 `post`는 조용히 버려진다.
> 그러면 완료 잡이 돌지 않아 `pendingDbJobs_`가 영영 0이 되지 않는다.
> 프로세스 종료 중에만 도달하는 경로라 실질적 문제는 없다.

## `inventoryReady_` vs `inventoryLoaded_` (둘 다 `GameSession`)

| 플래그 | 뜻 | 영향 |
|---|---|---|
| `inventoryReady_` | DB 로드가 끝났나 (성공·실패 무관) | `false`면 `C_InventoryAction`을 거절 |
| `inventoryLoaded_` | DB에서 **실제로 읽었나** (또는 신규 계정임을 확정했나) | `false`면 **저장하지 않는다** |

`inventoryReady_`가 없으면: 로드가 도는 사이 아이템을 쓰면, 뒤늦게 도착한 로드 스냅샷이
방금 소비한 아이템을 되살린다. 거절은 기존 `StaleRevision`을 재사용하므로 **클라 변경이 없다**
(클라는 이미 뒤따르는 `S_InventorySnapshot`으로 재동기화한다).

`inventoryLoaded_`가 없으면: DB 장애로 로드에 실패한 세션이 퇴장 시 그 빈 상태를 저장해
**진짜 인벤토리를 지워버린다.** 이게 이 설계에서 가장 중요한 안전장치다.

## 입장 시퀀스 — 스타터는 계정 최초 1회

`Room::enter`는 `Inventory::initializeEmpty`로 **빈 인벤토리**를 만들고 스냅샷을 보낸다.
스타터 지급은 DB 로드가 끝난 뒤 `onInventoryLoaded`에서 판단한다.

| `LoadStatus` | 처리 |
|---|---|
| `Ok` | `applySnapshot`으로 저장된 인벤토리 적용. `persistedRevision_`을 맞춰 재저장을 막는다 |
| `NewAccount` | **`initialize()`로 스타터 1회 지급 + 즉시 저장.** 이 순간부터 DB가 유일한 진실이 된다 |
| `DbError` | **빈 인벤토리 유지, 저장 금지.** 스타터를 주지 않는다 |

클라는 입장 직후 스냅샷을 **두 번** 받는다(빈 것 → 실제 것).

> **왜 `Room::enter`에서 스타터를 주면 안 되는가.** 처음 구현은 입장 즉시 스타터를 넣고
> 나중에 DB 값으로 덮었다. 그 결과 **DB에 근거가 없는 아이템이 화면에 먼저 보였고**,
> 저장이 완전히 깨져 있는데도(`SQL_NO_DATA` 교착, `databaseLayer.md` ⑤ 참조) 매번 포션 5개가
> 멀쩡히 나와 버그가 드러나지 않았다. 화면에 보이는 것은 DB에 있는 것과 항상 같아야 한다.
>
> `DbError`에 스타터를 주지 않는 것도 같은 이유다. 주는 순간 "저장이 망가져도 화면은 멀쩡한"
> 상태로 되돌아간다. 인벤토리만 비고 전투·진행은 정상이므로 게임은 계속할 수 있다.

`applySnapshot`은 현재 revision 이상을 요구하고 슬롯 개수가 `slotCount()`와 같아야 하므로
`inventory.revision() + 1`을 넘긴다.

**카운터 순서:** `persistInventory`는 `onInventoryLoaded` 끝의 `onDbJobFinished()`보다 **먼저**
호출해야 한다. 그래야 `pendingDbJobs_`가 1(load) → 2(load+save) → 1 → 0으로 내려가며 중간에
0을 찍지 않아, 저장 잡이 떠 있는 동안 방이 제거되지 않는다.

## 저장 전략 — 변경 시 + 퇴장 시

`Room::persistInventory`가 두 곳에서 호출된다.

- `Room::inventoryAction`에서 `InventoryCommandResult::Success`일 때
- `Room::leave` **맨 앞** — `std::erase_if(sessions_, ...)`보다 먼저여야 `session->player()`가 유효하다

`Player::persistedRevision_`으로 게이트해서 변경이 없으면 잡을 아예 걸지 않는다.
퇴장 시에만 저장하면 크래시에 전부 날아가고, 24슬롯 × 분당 몇 번은 부담이 아니라
변경 시 저장을 택했다.

```sql
BEGIN TRANSACTION
DELETE FROM dbo.Inventory WHERE accountId = ?
INSERT INTO dbo.Inventory (accountId, slotIndex, itemId, itemCount) VALUES (?, ?, ?, ?)   -- × slotCount
COMMIT TRANSACTION
```

`MERGE`나 배치 INSERT를 쓰지 않는 이유: `DBBinder`의 `paramCnt`가 **템플릿 인자**라
가변 길이 `VALUES` 목록을 바인딩할 수 없고, `DBConnection`에 TVP 지원도 없다.
한 트랜잭션 안의 작은 문장 25개는 이 규모에서 문제되지 않는다.

`DBBinder` 생성자가 부르는 `unbind()`는 커서(`SQL_CLOSE`)만 닫으므로,
명시적 트랜잭션은 바인더 여러 개를 넘어 유지된다.

## DB 장애 시 동작

| 시점 | 동작 |
|---|---|
| 룸서버 기동 | `DBExecutor::init` 실패 → 로그 + `return 1`. **리스너가 뜨기 전** |
| 로드 실패 | **빈 인벤토리**, `inventoryLoaded_=false`, `inventoryReady_=true` → 전투·진행은 정상, 아이템만 없고 저장도 안 함 |
| 저장 실패 | `ROLLBACK` + 로그. `persistedRevision_`은 이미 올라갔으므로 다음 변경 때 재시도 |
| 로드 완료 전 조작 | `StaleRevision` + 스냅샷 재전송 |

## 현재 한계

- **획득 경로가 없다.** 루팅/줍기 패킷이 아직 없어서 인벤토리는 `Use`/`DiscardOne`으로
  **줄어들기만 한다.** "내 인벤토리가 줄기만 하네"는 버그가 아니라 미구현이다.
- 룸서버 프로세스가 여러 개면 계정 중복 저장이 가능하다(`entryTicket.md`의 위협 모델 참조).
- DB 커넥션이 끊기면 재연결하지 않는다(풀은 기동 시 1회 연결).

## 관련 파일

| 파일 | 역할 |
|---|---|
| `RoomServer/InventoryStore.{hpp,cpp}` | `load`/`save` — **DB 스레드 전용** |
| `RoomServer/Room.cpp` | `requestInventoryLoad`, `onInventoryLoaded`, `persistInventory`, `onDbJobFinished` |
| `RoomServer/GameSession.hpp` | `inventoryReady_`, `inventoryLoaded_` |
| `RoomServer/object.hpp` | `Player::persistedRevision_` |
| `common/inventory.{hpp,cpp}` | 인벤토리 모델(전송 독립) |
| `db/schema.sql` | 스키마 + 마이그레이션 |

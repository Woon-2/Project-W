# DB 레이어 (ODBC)

`ServerEngine`의 ODBC 래퍼. 로비서버(계정/로그인)와 룸서버(인벤토리)가 공용으로 쓴다.
**두 서버 모두** `main`에서 `DBExecutor::init`을 호출하며, 실패하면 리스너를 띄우기 전에 종료한다.

| 파일 | 역할 |
|---|---|
| `DBConnection.hpp/.cpp` | ODBC 커넥션 1개(`HDBC` + `HSTMT` 1개)를 감싼다. 타입별 `bindParam`/`bindColumn` 오버로드 제공 |
| `DBConnectionPool.hpp/.cpp` | 커넥션을 미리 만들어두고 빌려준다. `DBConnectionPoolGuard`(RAII)가 함께 들어 있다 |
| `DBBinder.hpp` | 쿼리 하나의 파라미터/컬럼 바인딩을 묶고, 바인딩 누락을 비트 플래그로 검증한다 |

## 사용 패턴

```cpp
DBConnectionPoolGuard guard( gDbPool );
if ( !guard ) {
    return;                       // 풀 고갈. 반드시 확인할 것
}

int32 accountId = 0;
WCHAR nickname[ 32 ]{};

DBBinder<1, 2> binder( *guard.get(), L"SELECT id, nickname FROM Account WHERE id = ?" );
binder.bindParam( 0, accountId );      // 0-based
binder.bindColumn( 0, accountId );
binder.bindColumn( 1, nickname );

if ( binder.execute() && binder.fetch() ) {
    // nickname 사용
}
```

`DBBinder<paramCnt, columnCnt>`의 템플릿 인자는 **선언한 개수**다. `execute()`는 그만큼이
전부 바인딩됐는지 `validate()`로 확인하고, 빠졌으면 로그를 남기고 `false`를 돌려준다
(Debug에서는 `assert`로 즉시 잡힌다).

### 여러 행 읽기

`SQLBindCol`이 고정 주소를 바인딩하므로 `fetch()`를 반복하면 같은 변수가 행마다 갱신된다.

```cpp
int32 slotIndex = 0, itemId = 0, itemCount = 0;   // fetch 루프가 끝날 때까지 살아 있어야 한다
DBBinder<1, 3> binder( conn, L"SELECT slotIndex, itemId, itemCount FROM dbo.Inventory WHERE accountId = ?" );
...
if ( !binder.execute() ) return false;
while ( binder.fetch() ) {
    // slotIndex/itemId/itemCount가 이번 행의 값이다
}
```

`DBBinder` 생성자가 `unbind()`(= `SQL_UNBIND` + `SQL_RESET_PARAMS` + `SQL_CLOSE`)를 호출하므로,
이전 커서는 새 바인더를 만드는 순간 닫힌다.

### 트랜잭션

ODBC 기본은 autocommit이다. 여러 문장을 묶으려면 T-SQL 문장을 그대로 실행한다.

```cpp
{ DBBinder<0, 0> b( conn, L"BEGIN TRANSACTION" ); if ( !b.execute() ) return false; }
// ... DELETE / INSERT ...
{ DBBinder<0, 0> b( conn, ok ? L"COMMIT TRANSACTION" : L"ROLLBACK TRANSACTION" ); b.execute(); }
```

`unbind()`가 닫는 것은 **커서**지 트랜잭션이 아니므로, 명시적 트랜잭션은 바인더 여러 개를
넘어 유지된다. 단 **같은 커넥션**이어야 한다 — 중간에 다른 `DBConnectionPoolGuard`를 잡으면
다른 커넥션이라 트랜잭션 밖이다.

가변 개수 행을 한 문장으로 INSERT할 수는 없다. `DBBinder`의 `paramCnt`가 템플릿 인자라
`VALUES (…),(…),…`를 바인딩할 방법이 없고 TVP 지원도 없다. 트랜잭션 안에서 행 단위로 돈다.

## 반드시 지켜야 할 것

**① 바인딩은 포인터만 저장한다.**
`SQLBindParameter`/`SQLBindCol`은 버퍼 주소만 기억한다. 바인딩한 변수와 `DBBinder` 객체 모두
`execute()`/`fetch()`가 끝날 때까지 살아 있어야 한다. 지역 변수를 바인딩하고 함수를 빠져나가면 안 된다.

**② 인덱스는 0-based로 넘긴다.**
ODBC는 1-based지만 `DBBinder`가 내부에서 +1 한다. 호출부는 0부터 쓴다.

**③ `BufferLength`는 바이트 단위다.**
`SQLBindCol`에 문자 수를 넘기면 드라이버가 버퍼 절반에서 문자열을 잘라버린다.
`WCHAR name[32]`에 `31`을 넘기면 15자에서 끊긴다. `DBBinder`의 `bindColumn(WCHAR(&)[N])`이
`( N - 1 ) * sizeof( WCHAR )`로 변환해주므로 호출부는 배열을 그대로 넘기면 된다.

**④ `WCHAR` 배열을 `bindParam`에 넘기면 문자열로 바인딩된다.**
배열 오버로드가 `if constexpr`로 타입을 보고 분기한다. 그 외 타입의 배열은 `SQL_C_BINARY`로 간다.

**⑤ 커넥션은 스레드 안전하지 않다.**
`DBConnection` 하나가 `HSTMT` 하나를 독점한다. 반드시 풀에서 빌려 한 스레드에서만 쓴다.
`DBConnectionPoolGuard`를 쓰면 스코프 이탈 시 자동 반납된다 — 직접 `pop()`/`push()`를 쓰다
`push()`를 빠뜨리면 커넥션이 영구 소실되고, 풀이 마르면 DB 기능이 조용히 멈춘다.

**⑥ 핸들 해제 순서.**
ODBC는 자식부터 해제해야 한다. `STMT` → `SQLDisconnect` → `DBC` → `ENV`.
순서를 어기면 `SQLFreeHandle`이 `HY010`으로 실패해 핸들이 그대로 샌다.
`DBConnection::clear()`와 `DBConnectionPool::clearNoLock()`이 이 순서를 지킨다.

**⑦ `MemoryManager` 수명과의 순서.**
`DBConnectionPool::connect()`는 내부에서 `onew<DBConnection>()`을 쓴다.
`MemoryManager::init()` **이후**에 `connect()`, `MemoryManager::release()` **이전**에 `clear()`.

**⑧ 풀 락은 재진입 불가.**
`poolMutex_`는 `std::mutex`다. 락을 잡은 상태에서 부를 정리 루틴은 `clear()`가 아니라
`clearNoLock()`이어야 한다.

## 로깅

DB 진단은 전부 `dbLogW( const WCHAR* )`를 거친다 (`DBConnection.hpp`에 선언).
`GetConsoleOutputCP()`로 콘솔의 실제 코드페이지를 조회해 `WideCharToMultiByte`로 변환한 뒤
`std::cerr`에 쓴다. 콘솔이 CP949든 UTF-8이든 안 깨지고, 좁은/넓은 스트림을 한 stderr에
섞어 쓰는 문제도 없으며, 내부 뮤텍스로 워커 스레드 간 줄 섞임도 막는다.

`std::wcout`/`std::wcerr` 직접 사용은 피할 것. 기본 "C" 로케일에서는 비ASCII 와이드 문자를
변환하지 못해 출력이 통째로 사라진다.

연결 실패 시에는 `SQL_HANDLE_DBC`에서 진단을 읽어 SQLSTATE와 함께 찍는다.
연결 문자열 오타는 시연에서 가장 흔한 사고인데, STMT 핸들만 조회하면 아무 메시지도 안 나온다.

## DBMS 전제

Visual Studio가 설치해주는 **SQL Server Express LocalDB**를 쓴다.

```
Driver={ODBC Driver 17 for SQL Server};Server=(localdb)\MSSQLLocalDB;Database=ProjectW;Trusted_Connection=Yes;
```

연결 문자열은 솔루션 루트 `db_config.json`에서 로드한다 (`common/dbConfig.hpp` — 서버 전용,
클라이언트가 읽는 `network_config.json`과 분리).

- **LocalDB는 네트워크 접속이 안 된다.** 같은 PC의 로컬 프로세스만 붙는다.
  로비/룸 서버를 한 PC에서 돌리는 현재 구성에서는 문제없지만, 서버를 다른 PC로 나누면
  SQL Server Express(TCP 활성화)로 가야 한다.
- **DB는 코드를 따라가지 않는다.** 다른 PC에서 재현하려면 스키마 스크립트(`db/schema.sql`)를
  저장소에 커밋하고 `sqlcmd -S "(localdb)\MSSQLLocalDB" -i db/schema.sql`로 적용한다.
  `.mdf` 파일 복사는 경로가 PC마다 달라 연결 문자열이 깨지므로 쓰지 않는다.
- `odbc32.lib`는 `lspch.hpp`/`rspch.hpp`에서 `#pragma comment`로 링크한다.
  ServerEngine은 StaticLibrary라 링크 단계가 없어서, EXE 쪽에서 해결해야 한다.

## 현재 상태 (2026-07-25 갱신)

레이어 검증 완료(연결/파라미터/컬럼 왕복, 한글 nvarchar 길이, 빈 문자열, varbinary,
풀 고갈, 정리 시 에러 없음). **첫 사용처인 계정 시스템(가입/로그인)이 로비서버에 연결됐다** —
상세는 [accountSystem.md](accountSystem.md) 참조.

이 레이어 위에 추가된 것:
- `DBExecutor` — 전용 DB 스레드 + 작업 큐. ODBC 블로킹 호출을 IOCP 워커에서 격리한다.
  풀도 내부에서 소유한다 (`DBExecutor::pool()`)
- `PasswordHash` — BCrypt(CNG) 기반 SHA-256 + 솔트
- `db/schema.sql` — ProjectW DB, Account/Inventory 테이블 (멱등 스크립트)
- `common/dbConfig.hpp` + `db_config.json` — 연결 설정 로더

다음 단계:
- 룸서버 인벤토리 로드/저장 (Inventory 테이블은 선반영됨)
- 로비→룸 계정 정보 전달 (룸 입장 시 accountId 핸드셰이크)
- DB 커넥션 끊김 시 재연결 (현재는 기동 시 1회 연결, 실패 쿼리는 DbError로 응답)

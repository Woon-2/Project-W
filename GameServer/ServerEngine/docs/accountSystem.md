# 계정 시스템 (회원가입·로그인)

로비서버의 계정 가입/로그인 처리. DB 레이어([databaseLayer.md](databaseLayer.md)) 위에 올라간 첫 게임 기능이다.

## 패킷 프로토콜 (클라이언트 담당자용)

패킷 정의는 `ServerEngine/protocol.hpp` — 클라이언트가 같은 파일을 include하므로 **struct를 그대로 쓰면 된다**.
타입: `C_Register` / `S_Register` / `C_Login` / `S_Login`, 결과 코드: `AccountResult`.

```
클라                          로비서버
 │  C_Register(loginId, password, nickname)
 │ ───────────────────────────────▶ │  검증 → DB 스레드에서 중복확인·해시·INSERT
 │  S_Register(result)              │
 │ ◀─────────────────────────────── │
 │  C_Login(loginId, password)      │
 │ ───────────────────────────────▶ │  DB 스레드에서 SELECT → 해시 검증 → 중복접속 확인
 │  S_Login(result, accountId, nickname)
 │ ◀─────────────────────────────── │
 │  (이후 C_CreateRoom 등 로비 패킷 사용 가능)
```

- **인증 게이트**: `S_Login(Ok)`를 받기 전에 보낸 다른 `C_*` 패킷은 서버가 조용히 무시한다.
  클라이언트는 로그인 성공 후에 로비 UI로 진입해야 한다.
- 문자열은 전부 **널 종료 고정 배열**: `loginId`/`password`는 `char`(ASCII),
  `nickname`은 `wchar_t`(한글 가능). 크기 상수 `kLoginIdMax`(24)/`kPasswordMax`(32)/`kNicknameMax`(16)는
  널 포함이므로 실제 입력 가능 길이는 각각 23/31/15자. 클라 입력 UI가 이 길이로 제한해야 한다.
- 빈 값이나 널 종료 없는 입력은 `InvalidInput`으로 거절된다.
- **중복 확인은 가입 버튼 시점에 서버가 수행한다** (별도 "중복 확인" 버튼·패킷 없음).
  아이디 중복이면 `DuplicateId`, 닉네임 중복이면 `DuplicateNickname`,
  **둘 다 중복이면 `DuplicateId`를 먼저** 응답한다 — 클라는 한 번에 하나씩 안내하면 된다.
- `AlreadyLoggedIn`: 같은 계정으로 이미 접속 중인 세션이 있으면 로그인이 거절된다.
  기존 세션이 끊기면 다시 로그인 가능.
- 비밀번호는 TCP 평문으로 전송된다(암호화 없음) — 시연 범위의 알려진 한계.

## 스레딩 모델

```
IOCP 워커 (PacketManager::handleC*Packet)
  │  입력 검증 → shared_ptr<GameSession> self 캡처 + 입력 값복사
  ▼
DBExecutor::post(잡)                     ← ServerEngine/DBExecutor.hpp
  │  전용 DB 스레드 1개가 mutex+cv 큐에서 순서대로 실행
  ▼
DB 스레드: DBConnectionPoolGuard → DBBinder로 쿼리 → PasswordHash 검증
  │  세션 상태 기록: nickname_ 쓰기 → accountId_ → authenticated_.store(release)
  ▼
self->send( makeS*Packet(...) )          ← Session::send는 크로스 스레드 안전
```

규약:
- **ODBC 호출은 블로킹**이므로 IOCP 워커에서 직접 실행하지 않는다. 반드시 `DBExecutor::post`.
- 기존 `JobQueue`를 쓰지 않는 이유: `JobQueue::push`가 전역 `JobQueuePool`에 자동 등록되고,
  룸서버의 잡 스레드들이 그 풀에서 아무 큐나 소비하므로 DB 잡이 다른 스레드로 새어
  커넥션이 동시 실행될 수 있다.
- 세션 계정 필드(`authenticated_`/`accountId_`)는 atomic. `nickname_`은
  `authenticated_`를 release로 세우기 **전에만** 쓰고, 읽는 쪽은 acquire 로드가 true일 때만 읽는다.
- DB 잡에는 세션을 `shared_ptr`로 캡처한다(잡 실행 전 세션이 풀로 반환되는 것을 방지).
  끊긴 세션에 send하면 무해하게 버려진다.
- 로그인 잡 완료 직전에 `isConnected()`를 재확인해, 잡 도중 끊긴 세션의 계정 바인딩을 직접 회수한다
  (안 하면 그 계정이 영영 `AlreadyLoggedIn`으로 잠긴다).

## 비밀번호 저장

`ServerEngine/PasswordHash.hpp` — Windows CNG(BCrypt), 외부 의존성 없음. `bcrypt.lib`은 `lspch.hpp`에서 링크.

- 가입: `pwSalt = BCryptGenRandom(16바이트)`, `pwHash = SHA-256(salt || password)` → `Account`에 저장
- 로그인: 저장된 솔트로 다시 해시해 비교 (`PasswordHash::verify`)
- DB에는 평문이 저장되지 않는다. 같은 비밀번호라도 계정마다 솔트가 달라 해시가 다르다.

## DB 준비 (시연 PC 셋업)

```
sqllocaldb start MSSQLLocalDB
sqlcmd -S "(localdb)\MSSQLLocalDB" -i db\schema.sql     # 멱등 — 반복 실행 안전
```

- 연결 설정은 솔루션 루트 `db_config.json` (서버 전용 — 클라이언트 배포에 넣지 말 것).
  로더는 `common/dbConfig.hpp` (`network_config.json`과 같은 탐색 규칙, 폴백 없음).
- 스키마의 `NVARCHAR` 길이는 `protocol.hpp` 상수와 짝이다: `loginId NVARCHAR(23)`, `nickname NVARCHAR(15)`.
  **한쪽을 바꾸면 반드시 같이 바꿀 것.**
- `loginId`와 `nickname` 모두 UNIQUE 제약이 있다 — 가입 시 선확인 쿼리가 1차,
  UNIQUE 제약이 동시 가입 레이스의 최후 방어다.
- LocalDB는 정지 상태여도 연결 시도가 인스턴스를 자동 기동시킨다. 서버 기동 시 연결이
  아예 불가능하면(드라이버 없음 등) SQLSTATE를 출력하고 exit 1.

## 클라이언트 배선 (2026-07-26)

UI(`client/online/lobbyUI.*`)와 서버가 실제로 연결됐다. 그 전까지 `Game::lobbyLogin`/`lobbyRegister`는
빈 값만 검사하고 `isAuthenticated_`를 직접 세우는 로컬 목업이라, 로그인은 "성공"하는데 이후
`C_CreateRoom`이 인증 게이트에 걸려 방 생성이 무응답으로 멈췄다. 목업(프로세스 메모리 중복 집합,
하드코딩 닉네임 `PLAYER`)은 제거됐다.

```
LobbyUI 버튼/Enter → Game::lobbyLogin/lobbyRegister   (로컬 검사만: 빈 값·길이·ASCII)
  → PacketManager::makeCLoginPacket/makeCRegisterPacket → ClientApp::send()
  ← S_Login/S_Register
  → PacketManager::handleSLoginPacket/handleSRegisterPacket
  → Game::onLoginResult/onRegisterResult   (상태 전환 + AccountResult → 한국어 안내)
```

- 응답 처리는 `LobbyScene()`의 `SleepEx(1, true)` alertable APC, 즉 **메인 스레드**에서 돈다.
  기존 `onLobbyCreated`와 같은 경로라 UI를 직접 만져도 된다.
- `authPending_`가 응답 대기 중 재전송을 막는다 — 로그인 버튼 연타 시 두 번째가
  `AlreadyLoggedIn`으로 튕기는 것을 방지한다.
- 클라는 `loginId`/`password`에 비ASCII가 섞이면 **보내지 않고 로컬에서 거절**한다.
  wire 타입이 `char[]`라 변환에서 깨지기 때문. 닉네임은 `wchar_t[]`라 한글이 그대로 간다.
- 입력창 자체의 `setMaxLength`/ASCII 필터는 아직 없다. 길이 초과는 전송 전 로컬 메시지로 거절된다.

## 검증 이력 (2026-07-25)

TCP 루프백 스모크 테스트 14항목 전부 통과: 가입 Ok/중복 DuplicateId/빈 값 InvalidInput,
게이트(로그인 전 C_CreateRoom 무시, 로그인 후 정상 응답), WrongPassword/NoSuchAccount,
로그인 Ok + 한글 닉네임 왕복, 중복 접속 AlreadyLoggedIn, 세션 종료 후 재로그인 Ok.
DB에 32바이트 해시 + 16바이트 솔트 저장 확인. 테스트 코드는 저장소에서 제거됨.

## 검증 이력 — 클라이언트 통합 (2026-07-26)

실제 `client.exe` GUI로 왕복 확인. 위 스모크 테스트가 서버 단독이었다면 이쪽은 UI→DB 전 구간이다.

| 항목 | 결과 |
|---|---|
| 빈 값 로그인 | 전송 없이 로컬 안내 |
| 회원가입 모달(아이디/비밀번호/닉네임) | 정상 |
| 아이디·닉네임 중복 | `DuplicateId`/`DuplicateNickname` 안내 표시 |
| 정상 가입 → 로그인 | 성공 |
| **로그인 후 방 만들기** | 성공 — 인증 게이트 통과 |
| **틀린 비밀번호 / 없는 아이디** | 거부 |
| **같은 계정 2개 클라 동시 로그인** | `AlreadyLoggedIn` 거부 |

DB 확인: 클라로 가입한 계정이 `dbo.Account`에 적재되고 `pwHash` 32B / `pwSalt` 16B.

**앞의 세 항목(굵게)이 목업과의 판별점이다.** 목업은 빈 값만 아니면 무조건 로그인을
통과시켰고, 서버 인증 게이트에 걸려 방 생성이 무응답으로 멈췄다. 가입 성공·중복 안내만으로는
목업과 구분되지 않으니, 회귀 확인 시에도 이 셋을 봐야 한다.

미확인(문제 징후는 없음): 한글 닉네임의 클라 왕복, 클라 재시작 후 재로그인,
프로필 라벨 닉네임 표시 정확성.

## 로비→룸 계정 핸드오프 (2026-07-26 구현)

로그인으로 확정된 계정 정보를 룸서버까지 위조 불가능하게 전달한다.
로비/룸 사이에 소켓이 없어 클라가 중계하므로, 로비서버가 HMAC-SHA256으로 서명한
**입장 티켓**을 발급하고 룸서버가 같은 키로 검증한다.

```
C_GameStart(호스트) → LobbyRoom::startGame()
                       └─ 플레이어별로 mint(accountId, nickname, code) → S_GameStart{ip, port, code, ticket}
클라: 로비 소켓 close → 룸서버 재접속 → C_Enter{ticket, weapon}
룸서버: verify → AccountRegistry::bind → setAccount → enterRoom(ticket의 lobbyCode)
```

이로써 함께 해결된 것:

- 룸서버 **입장 게이트** — `C_Enter` 전에는 다른 패킷을 처리하지 않는다.
  (전에는 `C_Enter` 없이 `C_Move`만 보내도 `session->room()` 널 역참조로 서버가 죽었다)
- 인게임/로비 **실명 표시** — `PlayerInfo`/`LobbyPlayerInfo`에 `nickname` 추가,
  `S_Enter`에 기존 플레이어용 `PlayerNameInfo` 섹션 추가
- 룸서버 **인벤토리 영속화** — `dbo.Inventory` 로드/저장

> **주의:** 클라가 핸드오프로 로비 소켓을 닫는 순간 로비서버는 계정 바인딩을 푼다
> (`GameSession::onDisconnected` → `unbindAccount`). 즉 그 시점부터 같은 계정으로 다시
> 로그인할 수 있으므로, 중복 접속 차단은 룸서버가 자체 `AccountRegistry`로 따로 해야 한다.

상세: `ServerEngine/docs/entryTicket.md`, `RoomServer/docs/inventoryPersistence.md`

## 남은 것 (다음 단계)

- 클라 입력창 `setMaxLength`(23/31/15) + 아이디·비밀번호 ASCII 입력 필터
- 응답 대기 중 버튼 비활성/스피너 (현재는 `authPending_` 플래그로 재전송만 차단)
- DB 커넥션 끊김 시 재연결 없음 — 쿼리는 `DbError`로 응답 (풀은 기동 시 1회 연결)
- 비밀번호 평문 전송 (TLS 없음) — 데모 범위의 알려진 한계

# 입장 티켓 (로비 → 룸 계정 핸드오프)

작성: 2026-07-26

## 왜 필요한가

로비서버와 룸서버 사이에는 **소켓이 없다.** 핸드오프는 클라가 중계한다.

```
LobbyRoom::startGame()  →  S_GameStart{ip, port, code, ticket}  →  클라
클라: 로비 소켓 close → 룸서버로 재접속 → C_Enter{ticket, weapon}
```

즉 룸서버가 보는 계정 정보는 **전부 클라가 나른 값**이다. 서명하지 않으면 아무나
포트 9000에 붙어 남의 `accountId`를 주장할 수 있고, 룸서버는 그 값으로 `dbo.Inventory`를
읽고 쓰므로 남의 계정 데이터를 덮어쓸 수 있다.

그래서 로비서버가 계정 정보에 HMAC-SHA256 서명을 붙여 발급하고, 룸서버가 같은 키로 검증한다.

### DB 예약 테이블 대안을 기각한 이유

로비가 예약 행을 INSERT하고 룸서버가 SELECT로 확인하는 방식도 가능하지만:

- `C_Enter`가 DB 왕복을 기다려야 해서 `GameSession::enterRoom`이 비동기로 바뀐다.
- `startGame()`도 파티원 수만큼의 INSERT 완료를 기다려야 한다(방이 그사이 사라질 수 있다).
- **DB가 죽으면 아무도 게임에 입장하지 못한다.**

HMAC 방식은 검증이 순수 CPU(~10µs)라 `C_Enter`가 동기로 유지되고, DB는 인벤토리에만 쓰이므로
장애 시 "인벤토리는 스타터로, 게임은 정상 플레이"로 우아하게 강등된다.

## 바이트 레이아웃

`ServerEngine/protocol.hpp`의 `#pragma pack(1)` 영역 안에 정의된다.

| 오프셋 | 크기 | 필드 | 설명 |
|---:|---:|---|---|
| 0 | 2 | `version` | `kEntryTicketVersion`. 불일치 시 `BadVersion` |
| 2 | 8 | `accountId` | |
| 10 | 32 | `nickname[16]` | `wchar_t`, 널 종료. 남는 바이트는 반드시 0 |
| 42 | 7 | `lobbyCode[7]` | 널 종료 6자리. **방 배정의 권위 출처** |
| 49 | 1 | `reserved` | 항상 0 |
| 50 | 8 | `issuedAtUtcMs` | `system_clock` UTC epoch ms |
| 58 | 8 | `expiresAtUtcMs` | |
| 66 | 8 | `nonce` | 발급마다 `BCryptGenRandom` |

`EntryTicketPayload` = **74바이트**, `EntryTicket` = payload + `mac[32]` = **106바이트**.
`protocol.hpp` 하단의 `static_assert`가 이 크기를 고정한다.

**MAC 입력은 payload 74바이트 전체의 raw 이미지다.** 따라서:

1. 구조체는 반드시 `pack(1)` 영역 안에 있어야 한다. 패딩이 생기면 MAC이 비결정적이 된다.
2. 발급 시 반드시 `EntryTicket t{}`로 값 초기화해야 한다. `nickname` 뒷부분이나 `reserved`에
   쓰레기 바이트가 남으면 같은 입력이라도 매번 다른 MAC이 나와 검증이 무작위로 실패한다.

필드를 바꾸면 `kEntryTicketVersion`을 올리고 로비·룸을 **함께** 배포해야 한다.

## 비밀키 — `security_config.json`

저장소 루트에 있고, `common/securityConfig.cpp`가 exe 디렉터리에서 부모로 거슬러 올라가며 찾는다
(`db_config.json`·`network_config.json`과 같은 방식).

```json
{
  "entryTicket": {
    "secretHex": "<64 hex chars = 32 bytes>",
    "ttlSeconds": 120
  }
}
```

검증: 정확히 64개 hex 문자, 전부 0인 키는 거부, `ttlSeconds`는 5~3600.
폴백 없음 — 로드 실패면 두 서버 모두 리스너를 띄우기 전에 `return 1`로 죽는다.

> **`security_config.json`을 `client.vcxproj`에 절대 추가하지 말 것.**
> `network_config.json` 패턴을 무심코 복사하면 클라 배포에 비밀키가 실려 누구나 티켓을
> 위조할 수 있게 되고, 이 메커니즘 전체가 무의미해진다. `common/securityConfig.hpp`가
> 서버 전용인 이유가 이것이다.

**키 교체:** 두 서버를 함께 내리고 `secretHex`를 새 32바이트 난수로 바꾼 뒤 함께 올린다.
교체 중 발급된 티켓은 `BadMac`으로 거부되므로 무중단 교체는 지원하지 않는다.

## 구현

| 파일 | 역할 |
|---|---|
| `ServerEngine/HmacSha256.{hpp,cpp}` | CNG(BCrypt) HMAC-SHA256. `verify`는 **상수 시간 비교** |
| `ServerEngine/EntryTicket.{hpp,cpp}` | `EntryTicketAuthority::{init, mint, verify}` |
| `common/securityConfig.{hpp,cpp}` | 설정 로더 |
| `RoomServer/AccountRegistry.{hpp,cpp}` | 계정당 룸서버 세션 하나 |

`PasswordHash::verify`가 `memcmp`를 쓰는 것과 달리 `HmacSha256::verify`는 상수 시간 비교를 한다.
로그인은 네트워크 왕복이 지배적이라 타이밍 부채널 우려가 낮지만, 포트 9000은 공격자가
아무 제약 없이 반복 시도할 수 있어 `memcmp`의 조기 반환이 MAC 바이트를 한 개씩 맞춰나가는
단서가 된다.

`bcrypt.lib`는 static library가 아니라 EXE의 pch에서 링크한다
(`LobbyServer/lspch.hpp`, `RoomServer/rspch.hpp`). 클라는 mint/verify를 호출하지 않으므로
해당 obj가 링크되지 않아 `bcrypt.lib`가 필요 없다.

## 검증 흐름 (룸서버)

`RoomServer/PacketManager.cpp`

```
handlePacket
  └─ 입장 게이트: type != C_Enter && !isEntryAuthorized() → 로그 후 드롭
handleCEnterPacket
  ├─ len != sizeof(CEnterPacket)        → disconnect
  ├─ 이미 인증됨 / 방 있음               → 무시 (중복 C_Enter)
  ├─ verify(ticket) != Ok               → 로그 + disconnect
  ├─ AccountRegistry::bind 실패          → 로그 + disconnect (계정 중복 입장)
  ├─ setAccount(accountId, nickname)     → entryAuthorized_ = true (release)
  └─ enterRoom(ticket.payload.lobbyCode)
```

검증 결과(`EntryTicketResult`): `Ok / NotInitialized / BadVersion / BadMac / Expired /
NotYetValid / MalformedNickname / MalformedLobbyCode`.

**MAC을 가장 먼저 검사한다.** 서명이 깨진 티켓의 내용으로 만료·형식을 판정해봐야
공격자에게 정보만 준다.

거부는 전용 `S_*` 응답 없이 `disconnect`로 처리한다 — 클라에 표시할 UI가 없다.
필요해지면 `PacketType`에 append하면 된다.

### 입장 게이트는 크래시 수정이기도 하다

게이트 이전에는 `C_Enter` 없이 `C_Move` 하나만 보내도 서버가 죽었다.
`handleCMovePacket`을 비롯한 대부분의 핸들러가 `session->room()`을 널 체크 없이 역참조하는데,
`GameSession::onConnected`는 방을 배정하지 않기 때문이다.

## AccountRegistry가 필수인 이유

클라가 핸드오프로 로비 소켓을 닫는 순간 **로비서버는 계정 바인딩을 푼다**
(`LobbyServer/GameSession.cpp`의 `onDisconnected` → `unbindAccount`).
즉 핸드오프가 시작되면 그 계정은 곧바로 다른 클라에서 다시 로그인할 수 있다.

룸서버가 자체 레지스트리를 갖지 않으면 같은 `accountId`를 쥔 세션 둘이
`dbo.Inventory`를 서로 덮어쓴다. 서명 티켓의 유일한 약점인 **재사용**도 이걸로 함께 막힌다
(TTL 안에 같은 티켓을 두 번 써도 두 번째 `bind`가 실패한다).

`onDisconnected`는 방 배정 여부와 무관하게 먼저 `unbind`한다 — 티켓은 통과했지만 방에
못 들어간 경우에도 계정이 영원히 잠기면 안 된다.

## 위협 모델

**막는 것**
- 클라가 `accountId`/닉네임/방 코드를 위조하는 것
- 룸서버에 로비를 거치지 않고 바로 붙어 임의 방에 입장하는 것
- 티켓 재사용(레지스트리), 오래된 티켓 재생(TTL)
- `C_Enter` 이전 패킷으로 서버를 죽이는 것

**막지 않는 것**
- **전송 보안 아님.** 비밀번호는 여전히 평문으로 오간다(`protocol.hpp` 계정 패킷 주석 참조).
  같은 네트워크의 관찰자는 티켓을 그대로 가로채 쓸 수 있다. TLS가 없는 데모 범위의 한계다.
- 정상 로그인한 사용자의 인게임 치팅(이동/스킬 검증은 별개 문제)
- **룸서버 프로세스가 여러 개면** 각자 레지스트리를 가지므로 같은 계정이 양쪽에 동시에
  들어가 인벤토리를 덮어쓸 수 있다. 현재는 룸서버가 한 프로세스라 문제되지 않는다.

## 시계 오차

티켓은 wall clock(`system_clock`)을 쓴다 — `steady_clock`은 프로세스마다 epoch가 달라
로비가 찍은 시각을 룸서버가 해석할 수 없다.

로비/룸이 다른 머신이고 시계가 안 맞으면 엉뚱한 `Expired`/`NotYetValid`가 난다.
`issuedAtUtcMs`에 대해 **5초의 음수 스큐**를 허용한다(`EntryTicketDetail::kClockSkewToleranceMs`).
그 이상 어긋나면 두 머신의 시각 동기화를 맞춰야 한다.

## 관련 문서

- `ServerEngine/docs/accountSystem.md` — 가입/로그인
- `RoomServer/docs/inventoryPersistence.md` — 티켓으로 확정된 계정 위에 얹은 인벤토리 저장

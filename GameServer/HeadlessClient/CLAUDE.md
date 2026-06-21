### HeadlessClient

RoomServer 부하/스트레스 테스트용 봇. **렌더링·애니메이션·입력·DirectX 없음.** 여러 가짜
클라이언트(봇)를 생성해 RoomServer 에 직접 접속시키고, Room 에 입장시키고, 20Hz 로 이동 패킷을
보내며, 서버 패킷을 수신/파싱해 1초마다 통계를 출력한다.

Entry: `HeadlessClient/main.cpp`

#### 동작 원리 (서버 사실에 의존)
- RoomServer(`roomServerPort`=9000)는 로비 검증 없이 받는다. TCP 접속 직후 별도 핸드셰이크 없이
  `C_Enter{ char lobbyCode[7] }` 를 보내면 입장. `RoomManager::findOrCreateRoomByCode()` 가 코드로
  방을 그룹화하므로 **LobbyServer 를 거치지 않고 직접 접속**한다.
- 입장 성공 시그널 = `S_Enter` 수신 → `BotState::InRoom` 전이. 이후 20Hz 로 `C_Move` 송신.
- Room 은 생성 시 stronghold 기반 기본 NPC 를 스폰하고 매 tick `S_NpcMoveBatch` 를 브로드캐스트한다.
  봇은 이 수신율(`npcBatchRecv`)을 Room 실효 FPS 신호로 셀 수 있다(후속 "최대 Room 수" 탐색용).
- 모든 패킷은 `PacketHeader{ uint16 size; PacketType type }` 선두, `size` 로 프레이밍. 패킷 정의는
  `ServerEngine/protocol.hpp` 를 그대로 재사용한다(어긋나면 안 됨). ServerEngine.lib 는 링크하지 않고
  헤더만 사용(include 경로: `ServerEngine/`, `common/`). 네트워킹은 raw Winsock(non-blocking + `ws2_32`).

#### 구조
- `StressConfig` — 설정 + command-line 파서.
- `StressRunner` — 봇 수명·`WSAPoll` 이벤트 루프·ramp-up·1초 통계 출력(+옵션 CSV). 단일 스레드에서
  모든 봇 update 를 호출(샤딩 확장 여지).
- `BotSession` — 봇 1개 = 소켓 1개 = 상태머신
  (`Disconnected→Connecting→Connected→EnteringRoom→InRoom→Closed`). connect/recv/send/parse.
- `BotBehavior` — 스폰 위치(center=S_Enter myInfo.pos) 주위 원형 이동으로 pos/velocity 생성.
- `Metrics` — atomic 통계(send/recv pps·bytes, connected/inRoom 게이지, disconnect/parseError/connectFail,
  maxSendQueue).
- `RemoteObject` — (확장 슬롯) 원격 플레이어/NPC 보관 구조.

#### 결정/한계 (MVP)
- **방당 4인 고정**(`playersPerRoom`), 스케일 축은 방 개수. `roomIndex = botId / playersPerRoom` →
  결정적 6자 코드(`R%05d`)로 같은 방에 모은다.
- **C_Move 만 송신, orientation 생략.** C_Move 엔 orientation 필드가 없고(이동≠시선 분리 설계), 회전은
  별도 `C_MouseMove{yawRadian}` 채널이다. 미송신 시 서버는 입장 시 orientation 유지·velocity 로 애니메이션
  판단하므로 부하 대표성 영향 거의 없음.
- 공격(`C_Attack`)·재접속·NPC 타겟팅·`C_MouseMove`·FPS 기반 최대 Room 자동탐색은 인터페이스/플래그만
  열어둔 **후속 확장**.

#### 실행
```
HeadlessClient.exe --ip 127.0.0.1 --port 9000 --bots 400 --playersPerRoom 4 --moveHz 20 --duration 300
```
RoomServer 는 `../resources/...` 를 읽으므로 작업 디렉터리를 `GameServer/RoomServer`(또는 `../resources`
가 `GameServer/resources` 로 풀리는 위치)로 두고 먼저 띄운다. `--help` 로 전체 옵션 확인.

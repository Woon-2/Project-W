# DEVLOG

기능 작업 및 설계 결정을 날짜별로 기록한다.  
버그 픽스는 `TroubleShooting.md` 참조.

---

### 2026.05.06 ~ 05.07
## [mod] Separation Force 적용 방식 수정
## [mod] NPC 반응 딜레이 도입
## [feat] 고블린 시체 소멸 및 리스폰 구현

**커밋:** `598b990` → `67e5826` → `2712d37`  
**수정 파일:** `RoomServer/Npc.hpp/cpp`, `RoomServer/object.hpp/cpp`, `RoomServer/Room.cpp`, `ServerEngine/protocol.hpp`, `client/PacketManager`, `client/online/onlineGame`

---

### [1] Separation Force 적용 방식 수정 (`598b990`)

NPC-AI-Lab 원본(`sim/Actor.cpp`, `sim/Npc.cpp`)과 이식 코드를 비교해 4가지 차이를 발견하고 수정.

**변경 내용:**

- **`calcSeparationForce` → `Object`로 이동**  
  원본에서 `Actor`에 정의된 함수. `radius` 파라미터를 명시적으로 추가해 호출 사이트마다 반경 지정 가능하게 변경.

- **Chase / Return — 분리 힘의 수직 성분만 적용 (Gram-Schmidt)**  
  이식 코드는 분리 힘 전체를 추격 방향에 더해 역방향 성분이 전진 속도를 줄이는 버그가 있었다.  
  원본처럼 추격 방향에 수직인 성분만 사용.
  ```cpp
  mu::Vec3 sepPerp = sep - chaseDir * mu::dot(sep, chaseDir);
  mu::NVec3 nd(chaseDir + sepPerp * separationWeight_);
  ```

- **AttackWindup — 방향 보정 코드 삭제**  
  이식 과정에서 추가된 코드(Windup 중 분리 힘으로 facing 보정). 원본에 없던 동작이고 결과도 부자연스러워 삭제.

- **AttackRecover — 드리프트에 체반경(`BODY_RADIUS = 0.8f`) 사용**  
  기존 코드는 `separationRadius_` 단일 쿼리로 드리프트와 과밀 판정을 겸했다. 원본처럼 두 목적을 분리:
  - 드리프트(살짝 밀림): 체반경(`0.8f × 2 = 1.6f`)으로 검색 + 고정 속도(`moveSpeed_ * 0.15f`)
  - 과밀 판정: 타이머 만료 시 `separationRadius_`로 재쿼리

---

### [2] NPC 반응 딜레이 도입 (`67e5826`)

Idle 상태의 NPC가 모두 동시에 반응해 집단이 일제히 움직이는 기계적인 느낌을 해소.

**설계:**

| 전이 | 딜레이 범위 | 필드 |
|------|------------|------|
| Idle → Chase (직접 감지) | 0 ~ 0.3s | `maxDirectReactDelay_` |
| Idle → Investigate (그룹 메모리) | 0 ~ 2.0s | `maxGroupReactDelay_` |

`NpcConfig`에 `maxDirectReactDelay`, `maxGroupReactDelay` 추가해 외부 설정 가능.

**구현 방식:**

- `thread_local std::mt19937` RNG 사용 (스레드당 1개, `random_device`로 시드)
- 타이머 초기값 `-1s`(미초기화 센티넬). 타겟/메모리 감지 첫 틱에만 랜덤 딜레이를 부여.
- Idle에서 다른 상태로 전이할 때 `transitionTo()`에서 두 타이머를 `-1s`로 리셋.
- 딜레이 중 타겟이 사라지면 타이머를 버리고 재시도 시 새 딜레이 부여.

---

### [3] 고블린 시체 소멸 및 리스폰 구현 (`2712d37`)

Dead 상태가 종단 상태였던 것을 타이머 기반 리스폰으로 교체.

**서버 측 (`RoomServer`):**

- `NpcConfig`에 `respawnDelay(10s)` 추가.
- `Npc`에 `maxHp_`, `respawnDelay_`, `respawnTimer_` 멤버 추가.
- `updateDead(Seconds dt)`: 매 틱 `respawnTimer_` 감산. 0 이하 도달 시 `respawn()` 호출.
- `respawn()`: HP를 `maxHp_`로 회복, 스폰 위치로 이동(`setPos` + `snapToCurrent`), Idle로 전이.  
  `NpcUpdateResult.respawned = true`를 반환해 상위(Room)에 알림.
- `Room::updateGoblinAI()`: `result.respawned`가 true이면 `SNpcRespawnPacket` broadcast.

**프로토콜 (`ServerEngine/protocol.hpp`):**

```cpp
enum class PacketType : uint16 { ..., S_NpcRespawn };

struct SNpcRespawnPacket : public PacketHeader {
    uint16            npcId;
    int32             newHp;
    XMFLOAT3          spawnPos;
};
```

**클라이언트 측 (`client`):**

- `PacketManager::handleSNpcRespawnPacket()` 추가.
- `onlineGame`에서 해당 NPC 오브젝트를 spawnPos로 이동, HP 갱신, 시체 상태 해제.

---

### 2026.06.01
## [mod] NPC 공격 판정 Dead Reckoning 적용
## [mod] AttackWindup 범위 이탈 시 즉시 Chase 복귀
## [mod] AttackRecover 종료 후 플레이어 방향 보정

**수정 파일:** `RoomServer/Npc.cpp`, `RoomServer/object.hpp`, `RoomServer/Room.cpp`

---

### [1] Dead Reckoning — 플레이어 위치 예측

NPC가 `player->pos()`(마지막 CMovePacket 수신 시점 위치)로 공격 판정을 해 패킷 미수신 구간에
이동한 플레이어를 잘못 공격하는 버그를 수정.

**설계:**

CMovePacket에 이미 포함된 `velocity`를 서버 플레이어 객체에 저장하고,
공격 판정 시 `pos + velocity × elapsed`로 추정 위치를 계산(Dead Reckoning).
최대 예측 창은 300 ms로 제한.

**`object.hpp`:**

```cpp
Milliseconds posUpdateMs_{ 0ms };  // 마지막 위치 패킷 수신 시각

void         setPosUpdateMs(Milliseconds t) { posUpdateMs_ = t; }
Milliseconds posUpdateMs()            const { return posUpdateMs_; }

mu::Vec3 estimatedPos(Milliseconds serverNow,
                      Milliseconds maxWindow = Milliseconds{300.f}) const {
    float elapsed = (serverNow - posUpdateMs_).count();
    if (elapsed > maxWindow.count()) elapsed = maxWindow.count();
    if (elapsed < 0.f) elapsed = 0.f;
    return pos() + linearVel() * (elapsed / 1000.f);
}
```

**`Room.cpp` `move()`:**

```cpp
player->setPos(DirectX::XMLoadFloat3(&cMvPkt->pos));
player->setLinearVel(DirectX::XMLoadFloat3(&cMvPkt->velocity));  // 추가
player->setPosUpdateMs(elapsedMs_);                              // 추가
```

**`Npc.cpp` — 공격 판정 교체 위치:**

| 상태 | 용도 |
|---|---|
| Chase → AttackWindup 전환 | 공격 범위 진입 판정 |
| AttackWindup 피격 확정 | 실제 데미지 적용 |
| AttackRecover → 다음 전환 | 재공격/Chase 범위 판정 |
| Reposition → 다음 전환 | 과밀 해소 후 범위 판정 |

탐지(`selectBestVisibleTarget`)는 raw `pos()` 유지 — 미래 위치 기반 조기 aggro 방지.

---

### [2] AttackWindup 범위 이탈 시 즉시 Chase 복귀

Chase에서 `estimatedPos` 거리가 `attackRange` 이하가 되면 단 1틱이라도 AttackWindup으로
전환된다. 기존 코드는 플레이어가 이탈해도 `windupTime(0.4s) + recoverTime(1.5s) ≈ 2초`를
그 자리에 서서 대기했다.

**수정:** `updateAttackWindup()` 매 틱마다 범위 체크 추가.
플레이어가 사거리 밖이면 windupTimer 진행 여부와 무관하게 즉시 Chase로 복귀.

```cpp
mu::Vec3 toTarget = targetSession->player()->estimatedPos(room.getElapsedMs()) - pos();
if ( toTarget.len2() > attackRange_ * attackRange_ ) {
    mu::NVec3 nd( toTarget );
    setLinearVel( mu::Vec3( nd.x() * moveSpeed_, body().linearVel().y(), nd.z() * moveSpeed_ ) );
    setOrient( mu::NQuat( mu::Radian(), mu::Radian(), mu::Radian( std::atan2( nd.x(), nd.z() ) ) ) );
    transitionTo( NpcState::Chase );
    return {};
}
```

---

### [3] AttackRecover 종료 후 플레이어 방향 보정

AttackWindup 진입 시 `setOrient`가 호출되지 않아 공격 사이클 내내 방향이 고정되는 문제.
플레이어가 NPC 뒤로 이동하면 회복 종료 후 엉뚱한 방향을 향한 채 공격/추격했다.

**설계:** 윈드업 + 리커버리를 하나의 공격 사이클로 보고, 사이클이 완전히 끝난 뒤
(recoverTimer 만료 시) 플레이어 방향으로 1회 `setOrient`. 이후 Chase/AttackWindup이
올바른 방향에서 시작.

```cpp
// recoverTimer_ >= attackRecoverTime_ 블록 진입 직후
mu::Vec3 toTargetXZ = targetSession->player()->pos() - pos();
toTargetXZ = mu::Vec3( toTargetXZ.x(), 0.f, toTargetXZ.z() );
if ( toTargetXZ.len() > 0.001f ) {
    mu::NVec3 nd( toTargetXZ );
    setOrient( mu::NQuat( mu::Radian(), mu::Radian(), mu::Radian( std::atan2( nd.x(), nd.z() ) ) ) );
}
```

---

### 2026.06.02
## [feat] 일반 NPC Patrol(순찰) 상태 추가

**수정 파일:** `RoomServer/Npc.hpp/cpp`, `RoomServer/CLAUDE.md`

---

### [1] Patrol 상태 추가 — Idle ↔ Patrol 배회

타깃이 없을 때 Idle에서 완전히 멈춰 있던 일반 NPC(`Goblin`)를, 스폰 근처를 천천히 배회(Patrol)하다
잠시 대기(Idle)하는 순환으로 개선. 두리번거리며 순찰하는 "살아있는" 느낌을 부여.

**상태 순환:**

```
Idle ──(idleTimer 만료)──> Patrol ──(웨이포인트 도착 or patrolDuration 만료)──> Idle ──> ...
   │                          │
   └─ checkAlert: 플레이어 감지 → Chase / 그룹 메모리 → Investigate (두 상태 공통)
```

**설계:**

- `NpcState`에 `Patrol` 추가.
- **감지 로직 공용화** — 기존 `updateIdle`에 박혀 있던 직접 감지(→Chase)·그룹 메모리(→Investigate)
  판정을 `checkAlert(dt, room)`로 추출. 경계 상태(전환했거나 반응 타이머 대기 중)면 `true`를 반환해
  호출측이 배회를 멈추도록 함. `updateIdle`/`updatePatrol`이 공유 → 순찰 중에도 정상적으로 적을 감지.
- **`updatePatrol`** — 스폰 근처 웨이포인트로 `moveSpeed * patrolSpeedMult`(느린 속도)로 이동.
  웨이포인트 도착(0.5m 이내) 또는 `patrolDuration` 만료 시 Idle로 휴식. 이동 패턴은 Chase와 동일
  (separation 적용), 속도만 낮춤.
- **`pickPatrolDest()`** — 스폰 기준 랜덤 각도(0~360°) + `[patrolRadius*0.3, patrolRadius]` 반경.
  `patrolRadius`(~5) ≪ `activityZoneRadius`(28), 스폰=활동구역 중심이라 항상 구역 내.
- **`transitionTo`** — Idle/Patrol 진입 시 타이머·웨이포인트를 초기화하고, Patrol에서 이탈할 때도
  반응 타이머를 리셋하도록 조건 확장(`Idle || Patrol`).
- **동기화 방지** — idle/patrol 지속시간과 웨이포인트를 NPC마다 `thread_local mt19937`에서 독립
  추첨. 초기 `idleTimer_`는 `applyConfig`/`respawn`(transitionTo 미경유 경로)에서 직접 초기화.
  → 여러 NPC가 같은 움직임을 동시에 보이는 기계적 군무가 발생하지 않음.

**`NpcConfig` 신규 필드:**

| 필드 | 기본값 | 용도 |
|---|---|---|
| `patrolRadius` | `5.f` | 스폰 기준 배회 반경 |
| `patrolSpeedMult` | `0.4f` | `moveSpeed` 대비 순찰 속도 배율 |
| `minIdleTime` / `maxIdleTime` | `1.5s` / `4.0s` | 휴식 구간 길이 범위 |
| `minPatrolTime` / `maxPatrolTime` | `3.0s` / `6.0s` | 순찰 구간 길이 범위(안전 타임아웃) |

**`updatePatrol` 핵심:**

```cpp
if (checkAlert(dt, room)) return {};          // 경계 시 배회 중단
patrolTimer_ += dt;

mu::Vec3 toDestXZ( (patrolDest_ - pos()).x(), 0.f, (patrolDest_ - pos()).z() );
if (toDestXZ.len2() < 0.5f*0.5f || patrolTimer_ >= patrolDuration_) {
    setLinearVel(mu::Vec3(0.f, body().linearVel().y(), 0.f));
    transitionTo(NpcState::Idle);             // 도착/만료 → 휴식
    return {};
}
// dir + separation(수직 성분)으로 천천히 이동, 속도만 patrolSpeedMult_
float spd = moveSpeed_ * patrolSpeedMult_;
setLinearVel(mu::Vec3(nd.x()*spd, body().linearVel().y(), nd.z()*spd));
setOrient(mu::NQuat(mu::Radian(), mu::Radian(), mu::Radian(std::atan2(nd.x(), nd.z()))));
```

프로토콜 변경 없음 — 클라는 기존 `S_NpcMoveBatch`의 velocity로 걷기/정지 애니메이션을 추론.

---

### [2] 알려진 이슈 — Patrol 정면 교착 (미해결, 검토 중)

Patrol 중 두 NPC가 **거의 정확히 정면**으로 마주치면 서로 밀기만 하고 비켜나지 못해 멈춰 선다.

**원인:** separation 스티어링은 이동 방향(`dir`)의 수직 성분만 사용해(slide-past 설계) 옆으로 빗겨나가게
한다. 그런데 정면(degenerate) 케이스에선 `sep`이 `dir`과 거의 정반대라 수직 성분이 0에 수렴 →
빗겨날 방향 자체가 소실. 고블린은 `MotionType::Dynamic`이라 contact solver가 둘을 막아 세우고,
좌우 대칭이라 tie-break도 없어 교착.

**1차 시도(제거됨):** `updatePatrol`에 정면 감지 시 자기 기준 측면으로 트는 보정 항을 추가.
그러나 속도를 매 프레임 일정 속력으로 직접 설정하는 구조상, 접촉 상태에서 전진 성분이 여전히 커
contact·마찰이 약한 측면 미끄러짐을 죽여 교착이 풀리지 않음.

**검토 중인 근본 해결책(추후 커밋):**
- 물리 엔진에 충돌 필터링이 없고 현재 `Dynamic` 바디는 고블린뿐 → `generateContacts`에서
  "둘 다 Dynamic" 쌍의 contact를 스킵하면 NPC끼리 물리 충돌을 끄는 것으로 교착을 원천 차단 가능
  (간격은 separation 스티어링이 담당). 단, 혼잡 시 살짝 겹쳐 보일 수 있음.
- 대안: 충돌 유지 + 정면 시 전진 감속·강한 측면 회피, 또는 끼임 감지 후 리패스.

> 버그 성격이므로 해결 시 `TroubleShooting.md`에도 기록 예정.

---

### 2026.06.04
## [feat] 로비씬 ↔ LobbyServer 연동 (방 생성/참가/퇴장/게임시작 신호)

**커밋:** `7f07d0d9` (ServerEngine 미사용 Listener 제거 `7dc902c0` 동반)
**수정 파일:** `ServerEngine/protocol.hpp`, `LobbyServer/PacketManager`, `LobbyServer/GameSession`, `client/ServerSession.hpp`, `client/PacketManager`, `client/online/onlineGame`, `client/docs/lobbyScene.md`

---

### [1] 클라이언트 로비씬을 LobbyServer에 실제 연동

기존엔 방 생성/참가/슬롯이 클라 로컬 mock이었고, 클라는 startup에서 곧장 RoomServer(9000)에 접속했다.
이를 게임 시작 전 단계까지 LobbyServer(8000)와 실제 연동했다.

**연결 / 펌핑:**

- `client/ServerSession.hpp` 접속 포트 `roomServerPort`(9000) → `lobbyServerPort`(8000).
- 클라 수신은 APC 완료 루틴 기반이라 alertable 대기가 필요하다. 기존엔 `InGameScene`의 `SleepEx(1,true)`에서만
  처리됐는데, `LobbyScene()` 진입부에도 `SleepEx(1,true)` + `ClientApp::send()`를 추가해 로비에서도 패킷
  송수신을 펌핑하도록 했다(단일 스레드 모델 → 핸들러가 메인 스레드에서 실행, 락 불필요).

**자기 식별 (myId):**

- 프로토콜상 클라가 자기 `sessionId`를 알 방법이 없어, `SCreateRoomPacket`/`SJoinRoomPacket`에 `uint16 myId`
  추가. 본인 슬롯("나") 표시 + 호스트 판별/이양에 사용. 클라 `LobbyPlayer.id`(string)를 `sessionId`(uint16)로 변경.

**요청 / 응답 흐름:**

- 요청(C→S): `lobbyCreateRoom/JoinRoom/LeaveRoom/StartGame`이 상태를 직접 바꾸지 않고
  `C_CreateRoom`/`C_JoinRoom`/`C_LeaveRoom`/`C_GameStart`만 전송.
- 응답(S→C): `client/PacketManager`가 `S_CreateRoom`/`S_JoinRoom`/`S_LobbyRoomPlayerJoined`/
  `S_LobbyRoomPlayerLeft`/`S_GameStart`를 받아 `Online::Game`의 핸들러(`onLobbyCreated`/`onLobbyJoined`/
  `onLobbyPlayerJoined`/`onLobbyPlayerLeft`/`onGameStart`)로 룸 상태·슬롯·호스트를 갱신.
- 호스트 이양은 남은 목록 front 기준(서버 `LobbyRoom` 규칙과 일치).

**범위:** `S_GameStart` 수신 시 현재는 **로그만** 출력한다. RoomServer 접속·인게임 전환은 후속(별도 작업).
RoomServer가 lobbyCode가 아닌 접속 순서(`totalSessions % 4`)로 방을 묶으므로, 핸드오프 시 lobbyCode 기반
그룹화 설계가 함께 필요하다.

---

### 2026.06.05
## [perf] NPC 분리력 이웃 탐색 공간분할 최적화 (플레이어 근접 컬링 + SAPBroadPhase)

**수정 파일:** `RoomServer/broadPhase.hpp/cpp`, `RoomServer/Room.hpp/cpp`

---

### [1] 문제 — 분리력 이웃 탐색이 O(N²)

`Room::findNearbyNpcPositions`가 이동 상태 NPC(Patrol/Chase/Return/Reposition 등)마다
살아있는 전체 NPC를 선형 스캔한 뒤 `calcSeparationForce`를 호출 → 매 프레임 O(N²).
플레이어와 멀리 떨어져 화면에 보이지도 않는 NPC 무리끼리도 서로의 분리력을 계산하므로,
NPC 수가 늘수록 프레임이 떨어졌다.

> 분리력은 NPC ↔ NPC 사이에서만 계산된다(플레이어 미포함). 플레이어와의 밀침은 분리력이
> 아니라 물리 충돌 제약(`physicsWorld`)이 담당한다.

### [2] 설계 — 서로 다른 기준의 2단계 (중복 아님)

브로드페이즈(공간적으로 가까운 후보 추리기)를 같은 기준으로 두 번 거는 것은 중복이다.
그래서 두 단계가 **다른 기준**으로 거르도록 구성했다.

1. **플레이어 근접 컬링** — 어느 살아있는 플레이어와도 먼 NPC는 계산 대상에서 제외.
   "관련성" 기준. 플레이어 ≤4명이라 NPC당 최대 4번 거리 검사로 매우 싸다(O(N·P)).
2. **SAPBroadPhase 공간분할** — 살아남은 NPC들 사이에서만 이웃쌍 산출. "공간 근접" 기준.

> 클라이언트 개발자 권고대로 새 그리드 클래스를 만들지 않고, 물리에 쓰던 `SAPBroadPhase`
> (Sort-and-Sweep) 클래스를 분리력 전용 인스턴스로 재사용했다. SAP를 쓰는 것 자체가
> 공간분할이다.

참고: 분리력은 플레이어별로 계산하지 않는다. NPC는 위치가 하나뿐이고 주변 모든 NPC를
한꺼번에 피하므로, 플레이어 4명이어도 NPC를 플레이어에 "배정"하지 않는다. 비용은
플레이어 수가 아니라 NPC 수·뭉침 정도에 비례한다.

### [3] 구현

**`broadPhase` — SAP에 fatten 마진 + clear 추가 (물리 동작 불변):**

SAP는 AABB 겹침쌍을 주는데 분리 반경(~최대 6)은 바디 AABB보다 크다. 그래서 NPC AABB를
분리 반경만큼 부풀려야(fatten) 분리 거리 내 NPC가 후보쌍으로 잡힌다. 마진 기본값 0이라
물리용 인스턴스 동작은 동일.

```cpp
void setFatMargin(float m) { fatMargin_ = m; }   // 기본 0 → 물리 회귀 없음
void clear() { bodies_.clear(); endpoints_.clear(); }  // 매 프레임 멤버십 재구성
// update(): minX -= fatMargin_; maxX += fatMargin_;
// overlapYZ(a, b, margin): 양쪽 박스를 margin만큼 부풀려 겹침 판정
```

**`Room` — 매 프레임 이웃 인접 리스트 구축:**

```cpp
void Room::rebuildNpcNeighbors() {
    npcBroad_.clear();                                  // ① 플레이어 근접 컬링
    auto consider = [&](Object* o) {
        if (o && o->hp() > 0 && isNearAnyPlayer(o->pos()))
            npcBroad_.add(&o->body());
    };
    for (auto& g : goblins_)      consider(&g);
    for (auto& n : tacticalNpcs_) consider(n.get());
    if (platoonLeader_)           consider(platoonLeader_.get());

    npcBroad_.update();                                 // ② SAP 이웃쌍 → 인접 리스트
    const auto pairs = npcBroad_.queryPairs();
    for (auto& [id, v] : npcNeighbors_) v.clear();
    for (const auto& [a, b] : pairs) {
        Object* oa = npcBodyOwner_[a];   // RigidBody*→Object* 역참조(RigidBody에 id 없음)
        Object* ob = npcBodyOwner_[b];
        npcNeighbors_[oa->getId()].push_back(ob->pos());
        npcNeighbors_[ob->getId()].push_back(oa->pos());
    }
}
```

- `findNearbyNpcPositions`는 **시그니처를 그대로 유지**하고 본문만 인접 리스트 조회로
  교체 → 호출처(Npc/TacticalNpc의 모든 상태) 무수정. 각 호출의 실제 radius로 정밀 거리
  필터만 한다(인접 리스트는 fatMargin 반경의 superset).
- `Room::update()`에서 `rebuildLivingPlayersCache()`를 AI보다 앞으로 끌어올려(컬링이 최신
  플레이어 위치 사용) `rebuildNpcNeighbors()`를 호출. `updateGoblinAI` 내부의 중복 호출은
  제거.
- 신규 멤버: `SAPBroadPhase npcBroad_`(fatMargin 7), `npcBodyOwner_`, `npcNeighbors_`.
  상수 `NPC_SEPARATION_RELEVANCE_RADIUS=50`, `NPC_SEPARATION_FAT_MARGIN=7`.

### [4] 거동 보존 / 비용

- AI 틱은 모든 NPC에 대해 매 프레임 그대로 도므로 추격/순찰/그룹 협동 거동은 불변.
  컬링은 분리력(이웃 탐색) 정련만 생략한다.
- 컬링 반경 50 > 감지 사거리(10)·활동 구역(28~40)이라 교전 중 NPC가 컬링돼 분리력이
  빠지는 일은 없다.
- `fatMargin`은 `findNearbyNpcPositions` 최대 질의 반경(현재 ≈6: `CONFUSED_SEPARATION_RADIUS`,
  `TACTICAL_PRESSURE_*`) 이상이어야 한다. 새 분리 반경/멀티플라이어 추가 시 7을 재검토.
- 비용: O(N²) → 컬링 O(N·P) + SAP ~O(M + 쌍 수). 멀리 고립된 무리는 SAP 입력에서 빠져
  비용이 사라진다.

---

### 2026.06.06
## [feat] 전술 전투(중간보스 고블린) 동적 스폰 트리거 연결

**수정 파일:** `ServerEngine/protocol.hpp`, `RoomServer/PacketManager.hpp/cpp`,
`RoomServer/Room.cpp`, `client/PacketManager.hpp/cpp`

플레이어가 `Arena_Hobgoblin` 존에 진입하면 중간보스+분대가 동적으로 스폰되어 교전을 시작하도록,
이미 구현돼 있던 전술 전투 인프라를 트리거에 연결했다. 임시 구현(보스는 일반 고블린 모델 사용).

---

### [1] 트리거 → 동적 스폰 연결

기존 `Room::onArenaHobgoblinEnter`는 후방 벽만 생성하고 보스 스폰은 `actual spawn TBD` 로그만
남겼다. 서버에는 동적 스폰 함수 `spawnTacticalGoblinEncounter`(`make_unique` + `IdPool::pop()` +
`physicsWorld_.registerBody()`)와 매 틱 AI·`S_NpcMoveBatch`를 처리하는 `updateTacticalAI`,
그리고 가장 가까운 플레이어를 자동 타겟·분대 명령하는 `PlatoonLeader` + `GoblinMidBossTactic`이
이미 완비돼 있어, **트리거에서 호출만 하면** 됐다.

- 보스도 일반 고블린 모델을 쓰므로 클라엔 `ObjectType::Goblin`으로 전송(클라 `createGoblin`이
  항상 `modelGoblin()` 사용).
- 별도 AI 게이트 플래그 불필요 — `updateTacticalAI`는 `tacticalNpcs_`가 비면 즉시 return하므로
  스폰 전엔 자동 무동작, 스폰 직후부터 교전 시작.

---

### [2] 동적 스폰 통보용 신규 패킷 `S_NpcSpawnBatch`

클라 `createGoblin`은 진입 스냅샷(`S_Enter`)에서만 호출되고, `moveGoblin`은 `idGoblinMap_`에 없는
id를 무시한다. 즉 **런타임에 스폰된 NPC를 클라가 생성하는 경로가 없었다**(원래 설계 주석은
`S_NpcRespawn` 재사용을 의도했으나 모델/타입 정보가 없어 새 객체 생성 불가).

- `protocol.hpp`에 `PacketType::S_NpcSpawnBatch` + `SNpcSpawnBatchPacket` 추가. `S_Enter`의
  `ObjectInfo` 리스트 가변길이 직렬화(`dataOffset` + `objCnt`)를 그대로 재사용.
- 서버 `PacketManager::makeSNpcSpawnBatchPacket`, 클라 `handleSNpcSpawnBatchPacket`(각 `ObjectInfo`를
  `S_Enter`와 동일하게 `ObjectType` 분기 → `createGoblin`). 클라 `onlineGame`은 무변경.
- 늦은 접속자 대응: `Room::enter` 진입 스냅샷에도 `tacticalNpcs_`/`platoonLeader_`를 포함.

```cpp
struct SNpcSpawnBatchPacket : public PacketHeader {
    uint16 dataOffset;   // ObjectInfo 배열 시작 위치 (this 기준)
    uint16 objCnt;
    using ObjectList = DataList<ObjectInfo>;
    ObjectList getObjectList() { /* this + dataOffset */ }
};
```

---

### [3] trooper 랜덤 배치

`spawnTacticalGoblinEncounter`의 기존 ring(동심원) 배치를 `randomSpawnInDisc(spawnCenter, 30m)`
랜덤 배치로 교체(반경 30m, `groundHeightAtWorld`로 지형 높이 반영). 규모 3분대 × 20 = 60 + 보스.

---

### [4] BossSpawn 마커 부재 대응 — Wall 중점 fallback

**증상:** 존 진입해도 NPC가 하나도 안 보임. 서버 로그상 `ENTER`·벽 생성은 되는데
`[Zone] Hobgoblin spawn point` 로그가 없었다(일반 stronghold 고블린은 정상 → 클라 경로는 멀쩡).

**원인:** 레벨에 `BossSpawn` 타입 마커가 배치돼 있지 않아 `if (m.type != "BossSpawn") continue;`에서
전부 걸러져 `spawnTacticalGoblinEncounter`가 한 번도 호출되지 않음(스폰 0).

**해결:** `onArenaHobgoblinEnter`에서 BossSpawn 마커를 우선 찾되, 없으면 벽 생성 루프에서 누적한
`WallHobgoblin_0/1` 마커 중점(`wallSum / wallCount`)을 fallback 스폰 위치로 사용. 정석은 Unity
레벨에 `BossSpawn` 마커를 배치하는 것(그 경우 코드 변경 없이 마커 위치가 우선됨).

---

### [5] 알려진 한계 (임시 수용)

- 클라 `createGoblin`이 HP를 90으로 하드코딩 → 보스(서버 HP 2000)의 HP바 비율이 어긋남.
- tactical NPC가 `registerObject`(objectById_)에 미등록 → 스킬 타겟 조회에서 누락 가능
  (현재 분대→플레이어 공격 동작에는 영향 없음).
- 무관 이슈: `DummyClient`는 사전부터 `../LobbyServer/protocol.hpp` 잘못된 include 경로로 빌드 실패
  (실제 `protocol.hpp`는 `ServerEngine`에만 존재). 이번 작업과 무관.

---

### [6] 실행 후 수정 — tactical NPC 물리 셋업(Dynamic+motor 전환)

존 진입 후 실제로 굴려보니 세 가지 문제가 드러남: ① 중간보스 y좌표가 공중에 떠 있음,
② NPC들이 움직이지 않음, ③ 플레이어와 충돌 처리 안 됨.

**원인:** `spawnTacticalGoblinEncounter`의 NPC 생성이 기존 `setupGoblin`(`Room.cpp:44`)을 거치지
않아 **모델·물리·데미지 수신 설정이 전부 빠졌다.** `makeBase`(pos만) + `registerBody`(Kinematic)뿐.

- ① tactical NPC가 `Kinematic`이라 중력을 안 받고(중력은 Dynamic 한정), 보스는 `spawnPos`
  (Wall 마커 y≈53, 지형 위)를 그대로 써서 떠 있었다.
- ③ (a) `setModel` 미호출로 충돌 BVH가 비어 감지 자체가 0. (b) 플레이어도 Kinematic인데 NPC도
  Kinematic → 충돌 솔버가 서로 못 밀어냄(둘 다 질량 무한 취급).
- ② 일반 `Npc`는 `Dynamic + enableMotor(true)` + `setDesiredVel`(motor)로 이동하지만,
  `TacticalNpc`는 `setLinearVel`(직접 속도)로 이동 → 물리 모델 불일치.

**수정 (일반 goblin과 동일한 물리 모델로 통일):**

- `Room::spawnTacticalGoblinEncounter`의 `registerBody`를 `setupGoblin`과 동일하게 강화 —
  `setModel(modelGoblin())`, 애니메이션 클립(Idle/Walk/Attack/Die), `setCanReceiveDamage(true)`,
  `Dynamic + mass 70 + linearDamping 0.1 + angularDamping 25 + restitution 0 +
  uprightStiffness 4000 + enableMotor(true)`. trooper·보스 동일 적용. `assetManager_` 가드 추가.
- 보스 y 지형 보정: `bossPos.y = groundHeightAtWorld(x, z)` (trooper는 `randomSpawnInDisc`로 이미 보정됨).
- 이동 방식 motor 전환: `TacticalNpc.cpp`의 모든 이동/정지 `setLinearVel` → `setDesiredVel`
  (이동은 `(x, 0, z)`로 y는 중력이 담당, 정지는 `mu::Vec3{}`), `reviveAt`은 잔여 속도 제거 위해
  `setLinearVel({})` + `setDesiredVel({})` 병행. `MidBossTactics.cpp` 보스 이동 2곳
  (`TacticalRetreat`, `moveBossToward`)도 동일 전환.

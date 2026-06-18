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

---

### 2026.06.07
## [fix] 전술 전투 실전 버그 일괄 수정 (이동/피격/공격모션/미끄러짐/교전전이)

**수정 파일:** `ServerEngine/IdPool.hpp`, `RoomServer/Room.cpp`, `RoomServer/MidBossTactics.cpp/hpp`,
`RoomServer/TacticalNpc.cpp/hpp`

존 진입 후 실제 플레이로 드러난 전술 NPC 버그들을 순차 진단·수정. 각 증상의 근본 원인이 모두 달랐다.

### [1] 전원 미이동 — 플레이어 id 0 vs 전술 `targetId==0` sentinel 충돌

`IdPool::pop()`이 id를 0부터 발급 → 첫 세션/플레이어가 id 0을 받음(`Listener` `setId`,
`GameSession` `myPlayer_->setId(id())`). 그런데 전술 시스템은 `targetId==0`을 "타깃 없음"
sentinel로 쓴다(`TacticalNpc::resolveTarget` 등). 그래서 플레이어(id 0)를 타깃으로 잡아도
`resolveTarget`이 null → 보스·trooper 전원 Idle. (일반 `Npc`는 `int32 targetId_=-1` sentinel이라
id 0도 정상 타깃 → 영향 없었음.)

**수정:** `IdPool::init()` 루프를 `i=1`부터 채워 **id 0을 전역 "무효" sentinel로 예약**.
전술 코드는 무변경(0이 발급 안 되니 기존 `==0` 규약이 옳아짐). 진단으로는 `Room::updateTacticalAI`에
임시 로그를 넣어 `tgt=0`을 확인 후 제거.

### [2] 스킬 타깃 누락 — faction/registerObject 미설정

`spawnTacticalGoblinEncounter`의 `registerBody` 람다가 일반 고블린 대비 두 가지 누락:
`setFaction(Faction::Monsters)`(기본 Neutral → 플레이어 hostileMask에서 제외), `registerObject`
(`objectById_` 미등록 → 스킬 `checkHitboxCollisions` 타깃 후보 수집에서 누락). 둘 다 람다에 추가.

### [3] 스킬 피격 불가(근본) — 전술 NPC 애니메이션 본 미갱신 → hit BVH 손상

faction/registerObject를 넣어도 스킬이 안 맞았다. 스킬 narrow phase(`skillSystem.cpp`)는 타깃의
**`worldBVH()`에 직접** OBB 충돌을 거는데, 고블린 hit BVH는 **본 기반**이라 `rebuildBodyBVH`가
`boneWorldXforms_`로 노드를 배치한다. 이 배열은 `updateAnimBones`에서만 채워지는데, 일반 고블린은
`Npc::update`(`Npc.cpp:117`)에서 매 틱 호출하지만 **전술 NPC/보스는 아무도 호출 안 함** → 본이 비어
hit BVH가 엉뚱하게 생성 → OBB 미충돌.

**수정:** `Room::updateTacticalAI`에서 각 trooper·보스 `update()` 직후 `updateAnimBones(dtSec)` 호출.

### [4] 평타 피격 불가 — `Room::attack`이 `goblins_`만 순회

좌클릭 평타(`Room::attack`)는 스킬과 별개 경로로, `goblins_`만 AABB 판정했다. `tacticalNpcs_` +
`platoonLeader_`를 판정 대상에 추가(되감기 없는 현재 위치 AABB). 스킬(Q)은 [3]으로 해결됨.

### [5] 보스 공격 모션 없음 — `S_NpcAttack` 미전송

일반 고블린은 히트 시 `S_NpcAttack`(애니메이션)+`S_Hit`를 함께 broadcast하지만 전술은 `S_Hit`만 보냄.
`updateTacticalAI`에서 `result.hits` 비어있지 않으면 `makeSNpcAttackPacket(id)`도 broadcast.

### [6] 보스 미끄러짐 — 공격 상태에서 motor desiredVel 미정지

보스가 Chase로 접근 후 AttackWindup/AttackRecover/EvaluateTarget에서 `desiredVel`을 0으로 안 만들어
Chase의 마지막 속도가 motor에 남아 계속 미끄러졌다. `updateBossPersonalCombat`의 사거리 진입 전이 +
공격 윈드업/후딜 + 무타깃 Idle에서 `leader.setDesiredVel({})` 추가 → 공격 중 정지, 추격 시만 이동.

### [7] 박스 대형 후 교전(Engage) 미전이 — 밀집 Dynamic 대형의 도착 게이트 교착

`BoxAdvance→Engage`는 `allMembersArrived`(전 대원이 슬롯 `separationRadius_*0.25`=0.75m 이내)가
조건. 진단 로그(`[BoxDBG]`) 결과 대원들이 슬롯 **2~3.5m에서 물리 contact로 막혀 정지**(motor
desiredVel≠0인데 위치 불변) → "전원 0.75m"가 영구 미충족. (DEVLOG 2026.06.02 [2]의 미해결
Dynamic-Dynamic 교착이 대형 단계에서 발현. 시도: 간격 축소·도착 감속 ramp 모두 실패.)

**수정(물리엔진 불변):** 슬롯 "도착" 판정 거리를 jam 거리 이상으로 확대 —
`isAtSlot` HoldSlot 임계값 `separationRadius_ * 0.25` → `* TACTICAL_SLOT_ARRIVE_MULT(1.5)`(=4.5m).
대원이 슬롯 근처에 모인 시점에 게이트 충족 → Engage 전이(교전 단계는 separation으로 jam 자연 해소).
`BOX_SQUAD_SPACING` 35→24(분대 그리드 폭 ~12m보다 충분히 커서 14에서 발생했던 분대 겹침 jam 회피).
오버슈트 방지용 도착 감속 ramp(`TACTICAL_SLOT_ARRIVE_SLOW_RADIUS`/`MIN_SCALE`)도 `updateHoldSlot`에 유지.

> **⚠ [7] 해석 정정 (후속 발견):** 박스 미전이의 실체는 "Dynamic 물리 jam"이 **아니라 지형 Y차**였다
> (아래 [11]). 슬롯 도착거리 4.5m 완화는 그 Y차(~3.5m)를 우연히 흡수해 통과시킨 임시방편이었고,
> 더 넓은 Encircle(반경 20m, Y차>4.5m)에선 통하지 않았다. 근본 수정은 [11]의 XZ 거리 전환.

### [8] 시체가 산 NPC 이동을 막음 — 사망 시 물리 바디 미제거

죽은 전술 NPC의 Dynamic 바디가 그대로 충돌체로 남고(motor가 `desiredVel=0`을 유지해 밀어도 안 밀리는
**고정 벽**), 분리력 이웃 탐색은 `hp>0`만 보므로 산 NPC가 시체를 회피 대상으로 인식조차 못 함 →
시체 쪽으로 직진하다 끼임. (일반 고블린도 동일 구조지만 대형 이동이 없어 안 드러남.)

**수정:** `TacticalNpcUpdateResult.justDied` 신호 추가(사망 첫 틱). `Room::updateTacticalAI`에서
`justDied`면 `physicsWorld_.unregisterBody(&body)` + `npcBodyOwner_.erase`. 시체는 클라 시각만 유지,
충돌 제거. 전술 NPC는 부활이 없어 재등록 불요(엔진 불변).

### [9] 후퇴 미완료 → 전원 정지/전술 미발동 — 보스 후퇴 도착 불안정

`TacticalRetreat→BoxAdvance`는 `allMembersArrived && leaderAtRetreat(보스 ≤1.5m)` 필요. 보스가
`leaderMoveSpeed*TACTICAL_SPEED_MULT=16.5 m/s`로 감속·정지 없이 후퇴 지점을 ~3.4m 오버슈트하며 진동 →
`leaderAtRetreat` 거의 미충족 → 후퇴 미완료 → 대원은 후퇴 대형서 정지, 전술 미도달.

**수정:** 후퇴 이동에 도착 감속(`d<5m` 거리비례) + `d≤1m` 정지, `leaderAtRetreat` 1.5→2.0.
(공격 상태 정지 [6]과 동일 패턴.)

### [10] 거리 상수 인게임 스케일 축소

시뮬레이터 스케일로 포팅된 매크로 거리들이 인게임에서 과대(후퇴 70m 등). 인게임 검증값
(트루퍼 `separationRadius≈3`/`attackRange≈2.8`) 기준으로 일괄 축소:
`REGROUP_DIST` 70→25, `ENCIRCLE_RADIUS` 50→20, `ENCIRCLE_MIN_RADIUS` 18→7, `ENCIRCLE_SLOT_SPACING`
7.5→3, `CLUSTER_RADIUS` 20→8, `VIGILANCE_GUARD_RADIUS` 20→8, `BOX_FRONT_OFFSET` 15→6,
`BOX_ARC_DEPTH` 10→4, `BOX_SQUAD_SPACING` 35→15, `SCREEN_BLOCK_SPACING` 8→3.5,
`WEDGE_EXIT_DISTANCE` 35→14, `WEDGE_PREP_APEX_DISTANCE` 10→4, `TACTICAL_ATTACK_RESERVATION_MAX_DIST`
18→8, `TACTICAL_PRESSURE_EXTRA_RADIUS` 9→4, `TACTICAL_PRESSURE_RADIUS_OFFSET_SPAN` 7→3,
`CONFUSED_WANDER_RADIUS` 100→15, `CONFUSED_SEPARATION_RADIUS` 6→4.
(시간·비율·배율·각도·데미지·개수는 거리가 아니므로 불변.)

### [11] 슬롯/후퇴 도착 판정 3D→XZ — 지형 높이차 근본 수정 (★ [7]의 진짜 원인)

도착 판정이 **3D 거리**(`(pos-target).len()`)였는데, 대상 슬롯/후퇴 지점 Y는 평평(플레이어/리더 Y)이고
NPC는 중력으로 **지형 높이**에 붙는다. 기복 지형에서 Y차가 3D거리를 부풀려 도착 게이트가 깨짐.
진단 로그상 정착한 대원이 `maxXZ≈0.25m`(수평 정확 도착)인데 `max3D≈4.1m`(Y차 ~4m). Encircle(반경 20m)이
Y차>4.5m로 영구 정체한 근본이며, 과거 박스 "jam 2~3.5m"의 실체이기도 함.

**수정:** `lenXZ(v)=Vec3(v.x,0,v.z).len()` 헬퍼로 도착·슬롯 거리 판정을 **수평(XZ)** 으로 통일 —
`TacticalNpc`의 `isAtSlot`(HoldSlot)·`updateHoldSlot`(2곳)·`updateFlank`·`updateChargeThrough`,
`MidBossTactics`의 `leaderAtRetreat`·보스 후퇴 이동. (`moveTowardPressureWait`는 이미 Y를 빼고 있어 정상이었음.)
NPC는 지형에 붙으므로 도착 판정에서 Y 무시가 옳음. 이후 Box→Engage→Retreat→Box→Encircle→Cooldown
전체 순환 정상 동작 확인.

### [12] 후퇴→박스 전환 시 박스 대형 단계 생략 — 포팅 시 전술 업데이트 순서 역전

의도된 흐름 `후퇴(TacticalRetreat)→박스(BoxAdvance)→전술(Encircle/Vigilance)` 중 인게임에서 박스
단계가 생략되고 후퇴 직후 곧바로 전술로 점프.

**(정정) 실제 원인은 `MidBossTactics`의 전환 로직이 아니라 `Room`의 업데이트 순서였다.** 전술 전투는
`NPCAI` 시뮬레이터에서 포팅했는데, 원본 `NPCAI/sim/Room.cpp`는 명령 흐름과 같은
**PlatoonLeader → TacticalSquad → TacticalNpc** 순서로 업데이트한다(주석으로 명시). 그래야 리더가 발동한
order가 같은 틱에 분대 `pushCommandsToMembers` → NPC `consumePendingCommand`까지 전파돼 `assignedSlot_`이
즉시 갱신된다. 그러나 포트 `RoomServer/Room.cpp::updateTacticalAI`는 이를 **정반대(NPC → Squad → Leader)**
로 실행해, order 발동→push→consume가 2틱 지연됐다. 그 지연 틱에 리더가 `allMembersArrived`를 검사하면
대원 `assignedSlot_`이 아직 **후퇴 슬롯(stale)** 이라 `isAtSlot==true` → 박스가 형성되기 전에 전술로 점프.

원본은 박스 전환 조건(`if leaderPhase==BoxAdvance && primary && allMembersArrived`)에 `phaseOrderIssued_`
가드가 **없는데도** 정상이다 — 올바른 순서면 stale 창 자체가 없기 때문. (1차 시도로 이 조건에 가드를
추가했으나 원본에 없는 불필요한 변경이라 되돌림.)

**수정:** `updateTacticalAI`의 세 블록을 원본대로 **Leader → Squad → NPC** 순으로 복원. 위치는 AI 전
`physicsWorld_.step`에서 확정되고 NPC `update`는 위치를 직접 바꾸지 않으므로(모터 목표 속도/상태만)
리더가 먼저 돌아도 `npc->pos()`는 동일 → 역전되는 건 명령/상태 전파뿐(원하는 방향).

### [13] 대형 단계 연출 — 후퇴/박스 완성 후 체류(텀) 추가

[12] 수정으로 박스 대형이 정상 표시되었으나, 전환이 `allMembersArrived` 즉시 일어나 대형이 순간적으로
지나갔다. 후퇴/박스 대형을 잠시 과시하도록 단계 사이 체류를 추가.

**구현:** 공용 `phaseHoldTimer_`(Seconds) + `FORMATION_HOLD_DURATION{1.5s}`. `enterPhase`에서 0으로 리셋,
대형 완성(`allMembersArrived`, 후퇴는 추가로 `leaderAtRetreat`) 상태면 `dt` 누적·미완성이면 0 리셋 →
대형이 안정적으로 갖춰진 시점부터 카운트. 누적이 `FORMATION_HOLD_DURATION` 이상이어야 다음 단계로 전환.
후퇴→박스, 박스→전술 두 전환 모두 적용. 체류 중엔 기존 메커니즘이 대형 유지(후퇴=리더 정지+RetreatFormUp
HoldSlot, 박스=`boxRefreshTimer_` 0.1s 갱신). HP 임계/BossSolo 등 비상 전이는 체류와 무관하게 우선 동작.

### [14] 전술 NPC 경로 차단 해소 — 충돌 통과 필터 + HoldSlot 회피

전술 NPC가 슬롯/목표로 motor 직진할 때 (1) 경로의 플레이어(Kinematic=무한 질량)에 벽처럼 막혀 통과
불가, (2) 박스 대형 형성 경로의 보스(Dynamic mass70)와 jam → 슬롯 도착 실패 → `allMembersArrived`
영구 false → 전술 미발동. 원인: `updateHoldSlot`에 분리력이 전혀 없었고(직진만), 분리력 이웃에
플레이어 미포함, 물리에 충돌 필터 부재.

**수정 (A) 충돌 통과 필터:** `RigidBody`에 `collisionCategory_`/`collisionMask_`(기본 `0xFFFFFFFF`) 도입,
`generateContacts`에서 `(a.cat & b.mask)==0 || (b.cat & a.mask)==0`이면 접촉 생성 skip. `CollisionLayer::Player`
/`Boss` 비트로 플레이어·보스를 태그하고 trooper mask에서 둘을 제외 → trooper↔플레이어/보스 통과(나머지
충돌·솔버 수학 불변, 보스↔플레이어 충돌 유지).

**수정 (B) HoldSlot 회피:** `Room::findNearbyBlockerPositions`(플레이어+생존 보스) 추가, `updateHoldSlot`
이동에서 슬롯까지 `separationRadius_`보다 멀면(en route) 분리력 수직 성분으로 slotDir를 옆으로 보정
(`applyBlockerAvoidance`). 박스 밀집 패킹 보존 위해 peer NPC는 제외, 슬롯 근처는 비활성. 후퇴/박스/포위/
경계가 모두 HoldSlot이라 일괄 적용. Chase(공격 접근)엔 미적용(타깃 회피 방지) — 정면 충돌은 (A)가 처리.

### [15] 홉고블린 DivideAndConquer 재설계 — 쐐기 돌진 + 좌우 차단선 회랑(corridor)

NPCAI 시뮬레이터에서 각개격파 전술이 재설계되어 RoomServer로 1:1 포팅. 기존: 차단 부대가 **다른**
클러스터를 `GuardBoss`로 막고, 쐐기는 슬롯 도착 즉시 자동 돌진(차단선과 무연동). 신규: 한 부대가
**쐐기를 준비만 하고 대기**, 나머지 2개 부대가 돌진 대상 군집의 **좌우 측면에 일렬 차단선**을 세워
회랑을 형성 → 회랑 완성 시 쐐기를 **동시에 release**해 군집을 가두며 관통.

**(A) TacticalSquad 쐐기 release 게이트:** `SquadOrder.waitForChargeRelease`로 준비된 쐐기를
`wedgeChargeReleased_`까지 보류. 리더가 `releaseWedgeCharge()` 호출 시 돌진 발동. 신규
`isWedgePrepared()`/`estimateWedgeHalfWidth()`(쐐기 최대폭=회랑 반폭 산출용).

**(B) GoblinMidBossTactic 회랑 상태머신:** `DivideStage{ Preparing, Charging, Engaging }`.
회랑 좌표계(`divideCorridorForward_/Right_/Center_/HalfWidth_/HalfLength_`)를 chargeSquad→chargeCluster
방향으로 구성. Preparing 매 틱 `isCaptureClusterInsideCorridor()`로 대상 이탈 검사(이탈 시 fail-cooldown).
차단선은 `FormationGuard`+`slotColumnCount=멤버수`로 강제 일렬. **GameServer 적응:** FormationGuard
핸들러가 살아있는 플레이어 id를 요구(`findLivingSessionByPlayerId`)하므로 screen `targetId`는
`leader.getId()`(시뮬레이터) 대신 **군집 대표 플레이어 id**로 지정.

**(C) 레거시 제거:** 구 `SCREEN_*` 상수 5개, `DivideSquadTask.taskCompleted/engageIssued/engageProtectTimer`,
`DIVIDE_ENGAGE_PROTECT_DURATION` 삭제(완료-플래그 루프 → DivideStage 상태머신으로 대체). `GuardBoss`는
다른 phase에서 계속 사용하므로 유지.

**(D) 거리 상수 인게임 스케일 적용:** 설계 규약("전술 매크로 거리 상수는 인게임 스케일, 시뮬 수치
직접 이식 금지")에 따라 시뮬 절대 거리를 기존 GameServer 쐐기 비율(EXIT 14 = 시뮬 35의 ≈0.4x)에 맞춰
축소. `WEDGE_PREP_APEX_DISTANCE`/`WEDGE_EXIT_DISTANCE`는 기존값(4/14) 유지, `CAPTURE_MIN_HALF_LENGTH`
24→**10**, `CAPTURE_CORRIDOR_CLEARANCE` 6→**2.5**, `CAPTURE_ESCAPE_TOLERANCE` 2→**1**. 비율형 배율
`CAPTURE_LINE_SPACING_SCALE`(=0.65)는 separationRadius 기준 상대값이라 불변. 인게임(client) 검증 후 미세
조정 여지 있음.

### [16] 쐐기 돌진 시작점을 차단선 밖(회랑 후방 입구)으로

[15] 인게임 검증 결과 쐐기가 차단선 **안쪽**에서 돌진을 시작함. 원인: 준비 정점이 돌진 부대
현재 위치 기준(`centroid + forward*WEDGE_PREP_APEX_DISTANCE`)이라, 부대가 보스/플레이어 근처에
모여 있으면 회랑 내부에 잡힘.

**수정:** `SquadOrder`에 `wedgeApexPos`/`hasWedgeApex` 추가. WedgeCharge 준비 시 `hasWedgeApex`면
부대 위치 대신 명시 정점을 사용하고 전진축을 `정점→타겟`으로 재정렬. `issueDivideAndConquer`에서
차단선 길이(`divideCorridorHalfLength_`) 확정 **후** 돌진 명령을 발행하도록 순서를 옮기고, 정점을
`divideCorridorCenter_ - divideCorridorForward_*(divideCorridorHalfLength_ + CAPTURE_CHARGE_STANDOFF=4)`로
지정 → 쐐기가 회랑 후방 입구 밖에서 준비해 차단선 사이를 관통. 후방 입구는 돌진 부대 쪽(forward=부대→
플레이어)이라 부대가 자연스럽게 정렬됨.

### [17] 전술 NPC 정면 충돌(head-on) 교착 해소 — peer 스티어링 + 도착 게이트 보강

증상: NPC끼리 정면으로 마주쳐 막히면 박스 대형 형성이 늦어 전술 발동이 지연되고, 전술 중에도
슬롯/돌진 도착이 막혀 종료가 지연. 원인 3가지: (1) `updateHoldSlot`에 peer 회피 스티어링 부재
(플레이어/보스만 회피), (2) Chase/Flank 분리력이 진행방향 수직 성분만이라 정확한 정면(180°)에서
수직 성분≈0 → 대칭 교착, (3) 도착 게이트(`areMembersAtSlots`/`allMembersArrived`/
`areChargeMembersComplete`)가 전원 100% 요구 → 끼인 1명이 전체 단계 stall.

**수정 (A) head-on 해소 스티어링:** `TacticalNpc::applyPeerSeparation()` 신설(이웃 전술 NPC 분리력 +
정면 감지 시 일관된 'veer-right' 측면 bias 주입 → 마주친 둘이 반대편으로 갈라져 통과, 차선 통행 규칙).
`updateHoldSlot`(en route, 슬롯 근처는 비활성해 밀집 패킹 보존)·`updateChase`·`updateFlank`에 적용.
상수 `HEADON_DOT_THRESHOLD=0.6`, `HEADON_BIAS=1.0`.

**수정 (B) 임계비율 도착:** `areMembersAtSlots`/`areChargeMembersComplete`를 전원 → 살아있는 멤버 중
`SLOT_ARRIVE_FRACTION=0.85` 이상 도착/완료로 완화.

**수정 (C) 단계 타임아웃 폴백:** `GoblinMidBossTactic`에 `phaseElapsed_`(단계 누적)·`divideStageTimer_`
추가. `formationReady()=allMembersArrived || phaseElapsed_>=FORMATION_TIMEOUT(7s)`로 박스/포위/경계/후퇴
게이트 교체. DivideAndConquer Preparing/Charging에 `DIVIDE_PREP_TIMEOUT(6s)`/`DIVIDE_CHARGE_TIMEOUT(5s)`
폴백 → stall 시에도 전술 진행·종료 보장(정상 흐름엔 무영향).

### 설계 규약 메모
- **전술 trooper는 플레이어/보스와 물리 충돌하지 않음**(CollisionLayer). 경로 차단·대형 jam 방지용.
  되돌리려면 Room.cpp의 카테고리/마스크 태그만 제거(물리 코어는 기본값이라 무해)([14]).
- **전술 시스템 업데이트는 반드시 Leader → Squad → NPC 순서** — 명령이 위에서 아래로 흐르므로 같은 순서로
  돌려야 order가 한 틱에 끝까지 전파된다. 뒤집으면 도착 게이트가 stale 슬롯으로 거짓 양성을 내 대형
  단계(박스 등)가 건너뛰어진다([12]). NPCAI 원본의 순서를 유지할 것.
- **id 0은 전역 무효(invalid) sentinel로 예약**(IdPool 1부터 발급). 엔티티/세션/플레이어 id는 1 이상.
- 전술 NPC도 일반 고블린과 동일하게 **매 틱 `updateAnimBones` 호출 필수**(hit BVH 정확도).
- **전술 슬롯/도착 거리 판정은 XZ(수평)** — NPC는 지형 높이에 붙으므로 Y를 무시해야 기복 지형에서 정상.
- 전술 매크로 거리 상수는 **인게임 스케일**(트루퍼 separationRadius≈3 / attackRange≈2.8) 기준으로 잡을 것
  (시뮬레이터 수치 직접 이식 금지).
- 전술 NPC 사망 시 **물리 바디 unregister**(시체가 산 NPC 이동을 막지 않도록).

---

### 2026.06.09
## [feat] 쐐기(각개격파) 전술 차단벽 실체화 — barrier 모드(서버 토글) + 클라 position-split 충돌

**수정 파일:** `ServerEngine/protocol.hpp`, `RoomServer/PacketManager.*`, `RoomServer/MidBossTactics.cpp/.hpp`,
`client/object.hpp`, `client/PacketManager.*`, `client/online/onlineGame.cpp/.hpp`

**배경.** 쐐기 전술의 좌우 차단선(screen line)이 플레이어를 가두고 돌진하는 기믹인데, 인게임에서 플레이어가
그냥 통과했다. 추적 결과 **플레이어↔몬스터 충돌이 아예 없었다** — 클라 `createGoblin`의 고블린 물리 등록이
주석(처음부터 스텁)이고, 서버는 플레이어가 Kinematic(클라가 보낸 위치를 그대로 적용)이라 막지 못한다. 즉
플레이어를 멈추는 로직이 어디에도 없었다. (종전엔 "군집 이탈 시 전술 취소 + fail-cooldown"이라는 논리적
안전장치로 때우고 있었다.) 의도한 기믹은 *진짜 물리 벽* — 측면이 막히고, 탈출은 ① 벽 NPC 처치(구멍) 또는
② 열린 앞·뒤(특히 돌진 진입로 쪽)로 우회만 가능.

**방식 선택(A vs B).** (A) 고블린을 Kinematic 바디로 등록해 `ContactConstraint`로 막는 방식은 Dynamic↔
Kinematic에서 100% 침투 해소 + Baumgarte/warmstart 속도 bias로 **튕김/진동**이 난다 — player-player가 물리
솔버를 버리고 position-split(`resolvePlayerSeparation`)로 간 바로 그 이유다. 그래서 (B) **position 기반
split** 채택: 안정성(임펄스 없음), 성능(플레이어×barrier XZ 실린더 테스트만), 깔끔함(고블린 바디 미등록 →
서버 권위 위치 추종 그대로, 이중 물리 없음) 모두 우세. (고블린은 클라에서 Kinematic이라 등록해도 재시뮬되진
않지만, 솔버 경로의 튕김을 피하려 position-split을 택함.)

**구현.**
- **barrier 모드(일반화).** `Object`(클라 베이스)에 `barrierActive_` 플래그 + 접근자 추가 → 고블린에
  한정되지 않고 모든 몬스터 종류에 적용 가능.
- **서버 토글.** 신규 패킷 `S_NpcBarrier{active, npcId 목록}`(`protocol.hpp`, `PacketManager::makeSNpcBarrierPacket`).
  `MidBossTactics::issueDivideAndConquer`가 차단선 NPC id를 `divideBarrierNpcIds_`에 **보관만** 하고, barrier on은
  **차단선 형성 완료(돌진 발동, Preparing→Charging 전환) 시점**에 broadcast(`divideBarrierOn_`). → 형성 중
  이동하는 NPC가 대상 군집을 밀어 돌진 경로 밖으로 내보내는 걸 방지(barrier는 Charging 구간에만 활성). 
  `clearDivideBarriers()`가 **돌진 관통 완료**(Charging→Engaging) 및 모든 종료 경로(`enterTacticFailCooldown`,
  engage 완료)에 off broadcast(`divideBarrierOn_`일 때만). 비어있으면 no-op이라 타 전술 경로에서 안전.
- **클라 분리.** `Game::resolveBarrierSeparation(dt)` — `resolvePlayerSeparation`과 동형이나 (1) faction 대신
  barrier 플래그 게이팅, (2) barrier는 움직이지 않는 권위 객체라 절반이 아닌 **전체 침투**를 플레이어가 해소
  (hard wall). 위치(`setCurrPos`)만 보정 → 임펄스 튕김 없음. 물리 step 직후 `resolvePlayerSeparation` 다음 호출.
  **죽은 벽 NPC(hp≤0)는 건너뛰어 그 자리에 구멍**이 자동으로 뚫린다(기믹). `S_NpcBarrier` 수신은
  `setNpcBarrier`가 `barrierObjects_`(Object* 목록) 갱신.
  - **틈 봉합(점→선분 캡슐).** 전술 NPC는 서로 Dynamic 충돌(NPC-NPC)이라 체격(체반경 ~0.8m) 아래로 못 뭉쳐
    실제 간격이 ~1.6m+편차다. 각 NPC를 독립 원으로 막으면 편차로 벌어진 틈(>2.0m)으로 샜다. → 살아있는
    barrier를 **`kBarrierLinkDist(2.9m)` 내 모든 쌍을 선분(캡슐)으로 이어** 연속 벽으로 처리
    (`closestPointOnSegmentXZ`)해 간격과 무관하게 봉합. (최근접 이웃 하나만 잇던 초기 버전은 직선에서 가장 넓은
    틈을 구조적으로 건너뛰어 샜음 → 전체 쌍 연결로 교정.) 고립 barrier는 원으로 단독 차단. 누적 보정은 step
    상한(`kMaxBarrierPushPerStep`)으로 클램프. `linkDist`는 죽은 NPC 양옆 간격(~3.2m)·좌우 라인(회랑 폭)보다
    작아 구멍/앞·뒤 탈출구를 보존.
- **이탈 취소 제거 + 틈 없는 벽(서버).** `updateDivideAndConquer` Preparing의 `isCaptureClusterInsideCorridor`
  → 취소 블록 삭제(놓침 허용, 빈 회랑 관통). 회랑 길이를 덮으려 간격을 벌리던 `requiredSpacing` 로직(이탈의
  원인) 삭제하고 차단선 간격을 `CAPTURE_WALL_SPACING=1.2m`로 고정 → position-split이라도 NPC 사이로 못 빠짐.
  죽은 코드(`isCaptureClusterInsideCorridor`, `calcCaptureClusterCentroid`, 상수 `CAPTURE_LINE_SPACING_SCALE`/
  `CAPTURE_MIN_HALF_LENGTH`/`CAPTURE_ESCAPE_TOLERANCE`) 제거.

**범위/특성.** barrier는 전술 중 차단선 NPC에만 토글(평소 몬스터는 통과). 각 클라는 자기 플레이어만 분리하고
막힌 위치를 `C_Move`로 송신 → 서버(Kinematic) 수용 → 타 클라 전파, desync 없음. 고블린 Dynamic 물리는 서버
권위 그대로(클라는 위치만 추종, 이중 계산 없음).

**튜닝 노트.** `kBarrierRadius`(0.6)·`CAPTURE_WALL_SPACING`(1.2)·`kMaxBarrierPushPerStep`(0.5)로 차단 강도/
틈/순간이동을 조절. 현재값은 차단 보장 우선. 인게임(client) 검증 후 미세 조정.

---

### 2026.06.10
## [mod] 전술 흐름 단순화 — 경계(Vigilance) 단계 제거, 박스 → 쐐기/포위 직행

**수정 파일:** `RoomServer/MidBossTactics.cpp/.hpp`

**배경.** 전술 사이클이 `후퇴 → 박스 → 경계(Vigilance) → (쐐기/포위)`였는데, 인게임에서 박스 대형은 명확히
보이지만 **경계 대형(GuardBoss 산개)은 NPC가 뭘 하는지 시각적으로 모호**했다. 또 경계는 박스 종료 시 한 클러스터
판단을 한 번 더 반복할 뿐이고, 쐐기(`DivideAndConquer`) 단계가 진입 시 스스로 클러스터를 재검사(≤1이면
포위/실패 폴백)하는 자기완결 구조라 경계가 한 일에 의존하지 않는다 → 불필요한 중간 단계.

**변경.** 박스 종료 분기(`BoxAdvance` 완성+1s 유지 후)에서 클러스터 ≥2일 때 `Vigilance` 대신 곧장
`DivideAndConquer`로 진입. 흐름: **후퇴 → 박스 → 1개면 포위(Encircle) / 2개↑면 쐐기**. Vigilance 죽은 코드
전부 제거(열거값 `LeaderPhase::Vigilance`, 상수 `VIGILANCE_GUARD_RADIUS`, 단계 전환 판단 블록, `GuardBoss`
명령 발행 블록).

**부자연스럽지 않은 근거.** (1) 박스 대형은 이미 `FORMATION_HOLD_DURATION`(1s) 유지하며 그 시점에 클러스터를
새로 판단 → 박스 자체가 "관찰/평가 포즈" 역할(모호했던 경계 링보다 읽기 쉬움). (2) 포위/쐐기 재판단은 쐐기
명령 발행 블록의 클러스터 재검사로 **그대로 보존**(2단 판단 유지). 차이는 플레이어가 다시 뭉칠 시간 창이
경계 형성 시간만큼 짧아지는 것뿐 — 기존 "놓침 허용"과 일관.

---

## [feat] 전술 NPC 공격권 예약·squad 교전 배정 안정화 (NPCAI 시뮬 포팅)

**수정 파일:** `RoomServer/Room.cpp/.hpp`, `RoomServer/TacticalNpc.cpp/.hpp`,
`RoomServer/TacticalSquad.hpp`, `RoomServer/MidBossTactics.cpp/.hpp`,
신규 `RoomServer/docs/tacticalReservationAndEngage.md`. 상세 설계는 그 문서 참조.

**배경.** NPCAI 시뮬에서 검증된 전술 AI 4종을 포팅. 기존 RoomServer는 (1) 공격권 예약이
단순 선착순(`size>=5→거부`)이라 먼 NPC가 슬롯 선점, (2) squad 교전 타깃을 매 틱 거리로
재계산해 타깃이 흔들리고 동일 engage가 중복 발행, (3) 예약 끊긴 NPC가 즉시 재예약을 시도해
chase↔PressureWait 진동이 발생.

**변경 (4종).**
- **공격권 예약 — 거리+접근 진척 기반.** `Room::tryReserveTacticalAttackSlot` 재작성:
  교전 중(Windup/Recover) NPC는 occupant로 점유 보장, Chase/PressureWait/Flank 후보를
  거리순 정렬, 정원(5) 초과 시 최원거리 예약자 축출, 빈 슬롯을 가까운 후보부터 채움.
  `pruneTacticalAttackReservations`·`findTacticalNpcById` 추가. `TacticalNpc`에
  `isEligibleForAttackReservation`(거리 + blocked 진척 게이트), 진척 기반 lease 갱신
  (`reservedAttackProgressDist_`), 스테일로 끊긴 타깃은 `blockedAttackReservation*`로
  표시해 일정 거리 더 접근 전까지 재예약 차단.
- **chase↔PressureWait 진동 제거.** 위 진척 게이트(`PROGRESS_DIST=0.4`)로 끊긴 NPC가
  곧바로 되붙지 못하게 해 떨림 해소. 기존 타이머 스태거·표시 마스킹(`getDisplayState`)과 결합.
- **squad 타깃 균형 재배정.** `GoblinMidBossTactic::issueStableEngage(reset)` —
  배정 수→거리→id 순으로 플레이어 배정, `engageTargetBySquad_` 영속 맵으로 생존 중 고정.
  `enterPhase()`가 전술 대형/솔로 phase 진입 시 캐시를 비워 전술 종료 후 깨끗이 재배정.
  기존 `assignSquadsToPlayers`(매 틱 거리 재계산) 제거. **모든 일반 Engage 발행부**
  (evaluateTactics 폴백·enterTacticFailCooldown·update의 Encircle 완성/Box→Engage 전환·
  issueDivideEngage)를 이 함수로 일원화.
- **동일 engage 중복 방지.** `TacticalSquad::getEngageTargetId()` 추가 →
  `issueStableEngage`가 타깃이 바뀔 때만 `receiveOrder` 발행.

**스케일.** 시뮬이 2D 가독성용으로 키운 `RESERVATION_MAX_DIST`(18)·`PRESSURE_EXTRA_RADIUS`(9)는
미적용, 인게임값 8.0/4.0 유지. 신규 `TACTICAL_ATTACK_RESERVATION_PROGRESS_DIST=0.4`만 추가.

**비고.** 커밋의 `consumePendingCommand` EngageTarget→PressureWait 라우팅은 미적용(진척
게이트로 동일 효과). `issueDivideEngage`의 `selectReplacementTarget` 단일 추격은 균형 배정으로
대체됨 → 쐐기 통과 후 추격 동작은 인게임(client) 검증 필요. 빌드 Debug/Release x64 OK.

---

### 2026.06.12
## [fix] client 종료 크래시 수정 — SendBuffer 재활용 deleter의 종료 규칙 부재

**수정 파일:** `ServerEngine/SendBuffer.hpp/cpp`, `ServerEngine/MemoryManager.cpp`,
`client/ClientApp.hpp`, `client/main.cpp`, 신규 `client/docs/shutdownSequence.md`

**증상:** client 종료 시 `main.cpp`의 `return static_cast<int>(msg.wParam);` 줄에서 크래시.

### [1] 왜 "return 줄"에서 터진 것처럼 보였나

`return`이 실행되면 WinMain은 끝나지만 **프로그램은 아직 안 끝났다**. 그 뒤에 C++ 런타임이 뒷정리를 한다:

```
return 실행
 └→ WinMain 종료 (사용자 코드의 마지막 줄)
     └→ CRT가 자동 뒷정리 시작
         1) thread_local 변수들 소멸
         2) 전역/static 변수들 소멸   ← 실제 사고 지점
```

이 뒷정리 코드에는 소스 라인이 없으니, 디버거는 "마지막으로 지나간 내 코드 줄" =
`return static_cast<int>(msg.wParam);`을 가리킨다. 즉 **그 줄이 범인이 아니라, 그 줄 직후에
자동으로 도는 소멸자들이 범인**이다.

### [2] 실제 사고: "재활용함에 버리는 규칙"이 재활용함 자체를 부술 때

SendBuffer 시스템의 설계:

- 패킷을 보낼 때 8KB짜리 `SendBufferChunk`를 쓴다.
- 다 쓴 청크는 **버리지 않고 전역 재활용 큐(`sendBufferChunks_`)에 반납**한다. 이 반납이
  shared_ptr의 deleter로 구현되어 있다. 즉 *"이 청크의 마지막 참조가 사라지면 → 큐에 다시
  넣어라"*가 청크의 소멸 규칙이다.

평상시엔 완벽한 재활용 구조인데, **프로그램 종료 때** 문제가 된다:

1. 전역 재활용 큐도 static 변수라서 CRT가 자동으로 소멸시킨다.
2. 큐가 소멸하려면 안에 들어있는 청크들(shared_ptr)을 먼저 파괴해야 한다.
3. 그런데 청크의 소멸 규칙이 뭐였나? **"큐에 다시 넣어라"**.
4. 결과: **지금 부수고 있는 큐에 다시 enqueue를 시도** — 반쯤 해체된 자료구조에 쓰기를 하는
   거라 미정의 동작(UB)이다. 힙 상태에 따라 크래시가 나거나, 운 좋으면 조용히 넘어간다.

비유하면: "다 쓴 책은 반드시 책장에 꽂아라"는 규칙만 있는 도서관에서, 폐관하며 책장을
철거하는데 철거 중 나온 책들을 규칙대로 **철거 중인 그 책장에** 다시 꽂으려는 상황이다.
종료용 규칙("폐관 중엔 그냥 버려라")이 없었던 것.

여기에 메인 스레드의 `thread_local LSendBufferChunk`(현재 쓰던 청크를 가리키는 포인터)도 같은
시점에 소멸하면서 같은 큐에 반납을 시도하니, 종료 시점에 이 경로를 밟을 기회가 두 군데였다.

### [3] 왜 Online 모드에서만, 종료할 때만?

- 청크는 **패킷을 보낼 때만** 할당된다. StandAlone 모드는 송신이 없으니 큐도 TLS 포인터도
  비어 있어 무사 통과.
- 평상시엔 큐가 멀쩡히 살아 있으니 반납이 항상 성공. 오직 "큐 자신이 죽는 순간"인 프로그램
  종료 때만 규칙이 자기모순이 된다.

### [4] 동반 결함 두 가지 (같은 '종료 순서' 계열)

- `MemoryManager::release()`가 메모리 풀들을 `delete`한 뒤, 풀을 가리키는 조회 테이블
  (`poolTable_`)을 그대로 둠 → 그 이후에 해제 요청이 하나라도 오면 **이미 죽은 풀**을 건드림.
- `retiredSession_`(은퇴한 로비 세션)을 종료 시 정리하지 않음 → static 소멸 때 `WSACleanup()`
  **이후에** 소켓을 닫는 등 순서가 꼬일 여지.

### [5] 수정이 한 일

종료 직전에 `SendBufferManager::release()`를 호출해서 **규칙 자체를 바꾼다**: "지금부터
(shutdown) 반납 = 재활용함에 넣기가 아니라 진짜 해제". 그리고 TLS 포인터와 큐를 그 자리에서
비운다. 결과적으로 `return` 이후 CRT가 뒷정리할 때는 큐도 비어 있고 반납할 청크도 없어서,
자동 소멸이 건드릴 폭탄이 사라진다.

- `ServerEngine/SendBuffer`: `SendBufferManager::release()` 신설 — `shuttingDown_` 설정 후
  push deleter는 재enqueue 대신 `odelete`로 실제 해제, TLS 청크 해제 + 정적 큐 drain.
- `ServerEngine/MemoryManager`: release() 시 `poolTable_.fill(nullptr)` + `ASSERT_CRASH` 가드.
- `client/ClientApp`: `release()`에서 `retiredSession_`도 정리.
- `client/main.cpp` WM_QUIT 종료 순서 확정:
  `ClientApp::release()` → `SendBufferManager::release()` → `MemoryManager::release()` → `SocketUtils::release()`.

요약 — **"영원히 재활용만 하는 객체"와 "프로그램 종료"가 만나면, 재활용함이 먼저 철거되는
순간이 반드시 오는데 그때의 행동이 정의돼 있지 않았다**는 게 원인이다.

**검증:** 4개 프로젝트 Release x64 빌드 OK. 실제 client로 로비 접속·로딩 중 종료(3/5/8초) 포함
자동 검증 전부 exit 0, 재현 테스트 약 5회 크래시 미발생. 서버 3종은 동일한 잠재 문제 보유 —
정상 종료 경로 도입 시 같은 순서 적용 필요(후속 과제).

---

### 2026.06.14
## [feat] GrandBaum(그랜드밤) 중간보스 전술 포팅
## [refactor] squad 교전 배정 안정화(`issueStableEngage`) 공용화

**수정 파일:** `RoomServer/MidBossTactics.hpp/cpp`, `RoomServer/Room.hpp/cpp`, `RoomServer/object.hpp`, `RoomServer/GameSession.hpp`, `RoomServer/PacketManager.hpp/cpp`, `ServerEngine/protocol.hpp`, `client/PacketManager.hpp/cpp`, `client/online/onlineGame.hpp/cpp`  
**신규 문서:** `RoomServer/docs/grandBaumTactic.md`  
**커밋:** 미커밋(working tree)

---

### [1] GrandBaum 중간보스 전술 포팅

NPCAI(`sim/MidBossTactics.*`, `sim/ScenarioGrandBaum.cpp`)의 GrandBaum 중간보스 전술을 RoomServer로
포팅. 홉고블린(`GoblinMidBossTactic`) 인프라 재사용. 상세 설계는 `RoomServer/docs/grandBaumTactic.md` 참조.

**구성·흐름:**

- 슬라임 3부대(0,1,2) + 뱀 1부대(3) + 보스(`GrandBaumMidBossTactic`, Goblin 전술과 같은 파일에 동거).
- 평상시: 슬라임은 Engage, 뱀은 personal 회피 기동, 보스는 표적 우선순위(Snake>Slime>Nearest) melee.
- **단일 ShieldWall 전술** — 보스 HP **66% / 33%** 각 1회 발동.
  - 발동 게이트: 그 순간 살아있는 슬라임 ≥ 10 **AND** 원본 뱀 ≥ 1. 불충족 시 발동 스킵(→Cooldown).
  - 발동 효과: 슬라임이 보스를 **하드블로커 링**으로 감싸고(플레이어 통과 불가), **보스+슬라임 받는
    피해 90% 감소(×0.1)**.
  - 파훼는 오직 뱀: **예방**(발동 전 원본 뱀 전멸 → 스킵) / **해제**(발동 후 증원 웨이브 =
    원본뱀×10·최대 60 전멸 → 종료·재취약). 웨이브 수는 플레이어 수와 무관.
  - `Engage` ⇄ `ShieldWall` → `Cooldown(8s)` → `Engage` 루프. 한 큐에 두 단계를 건너뛰면(폭딜)
    발동은 1회로 합쳐짐.

**핵심 설계 판단(클라-서버 구조):**

- **데미지 경감 훅** — `Object::damageTakenMultiplier_`(기본 1.0)를 스킬 피격 적용부에서 곱함.
  ShieldWall이 보스/슬라임에 0.1 적용.
- **플레이어 넉백/이동잠금** — 플레이어 이동은 **클라 권한**이라 서버 `setPos`가 무효(클라가 다음
  프레임에 덮어씀). → 신규 `S_PlayerKnockback` 명령 패킷으로 클라가 로컬에서 넉백(0.32s)·입력잠금
  (1.2s)을 실행하고, 서버는 그동안 안티치트 이동 클램프를 면제(`GameSession`).
- **전투 중 동적 소환/디스폰**(증원 웨이브) — `Room::spawnTacticalWaveNpc / addDynamicTacticalSquad /
  removeTacticalNpcById / removeTacticalSquadById / broadcastTacticalNpcSpawn / reviveTacticalNpc`.
  `tacticalNpcs_`는 `unique_ptr` 벡터라 재할당돼도 객체 주소(raw 포인터)가 불변 → tactic 실행
  중(분대/NPC 순회 이전) 즉시 push/erase 안전. 디스폰 패킷이 없어 살아있는 웨이브 강제 정리는
  `setHp(0)+S_Hit`로 대체. 부활은 `reviveAt` + 물리 바디 재등록 + `S_NpcRespawn`.
- **슬라임 하드블로커** — ShieldWall 중 슬라임 콜리전 마스크를 `~Boss`로 토글
  (`Room::set/clearShieldWallBlockers`). 슬라임은 클라에도 존재하므로 클라 로컬 물리도 자연히 막힘.
- **상수 스케일** — 거리/반경/슬롯간격은 인게임 스케일 ×~0.4 적용(시뮬 원본 병기). 시간·비율·HP
  임계·카운트는 시뮬 원본 유지.
- 모델: 전용 슬라임/뱀/보스 에셋 추가 전까지 goblin 모델/`ObjectType::Goblin` 재사용(홉고블린과 동일).
- 트리거: zone `"Arena_GrandBaum"` + 마커(`WallGrandBaum_0/1`, `BossSpawn`). **레벨 마커 미저작 →
  실제 인게임 검증·밸런싱은 후속 과제.**

---

### [2] squad 교전 배정 안정화 공용화

GrandBaum 슬라임 부대 교전에 안정화 기능을 적용하기 위한 리팩토링.

- **공격권 예약**은 `TacticalNpc` 자체 기능 — 상태 핸들러가 `canEnterAttackSlot()` →
  `Room::tryReserveTacticalAttackSlot`(targetId당 최대 5슬롯, `tacticalNpcs_` 전체 후보)을 직접
  호출하므로 **전술 무관**. GrandBaum 슬라임·뱀·웨이브 모두 이미 적용돼 있었음(추가 작업 불필요).
- **squad 교전 배정 안정화**(`issueStableEngage`: 균형배정 + 생존중 고정 + 죽은 것만 재배정)는
  `GoblinMidBossTactic`의 private였음 → `engageTargetBySquad_`·`isLivingPlayerTarget`과 함께
  **`MidBossTacticBase`로 승격**. GrandBaum `issueEngage`가 슬라임 부대(0,1,2)에 이를 재사용
  (원본 뱀은 회피, 웨이브는 `DistributedEngage`). Goblin 동작은 상속으로 불변.

**검증:** RoomServer·client 모두 Debug/x64 빌드 통과. GrandBaum 전술의 예방·해제 경로 양쪽 컴파일
레벨 완성. 실제 인게임 거동 검증은 `Arena_GrandBaum` 레벨 마커 저작 후(후속 과제).

---

### 2026.06.14 ~ 06.15
## [feat] Isis(이시스) 중간보스 전술 포팅 — 2연속 쐐기 협공
## [feat] 중간보스 전술 디버그 토글 (Arena_Hobgoblin에서 Goblin/GrandBaum/Isis 선택)
## [fix] client 종료 WSACleanup 크래시 — alertable overlapped I/O 드레인 누락
## [fix] GrandBaum/Isis 디버그 스폰 좌표 fallback (마커 없을 때 인카운터 스킵)
## [mod] GrandBaum ShieldWall 튜닝 5종 + 방패벽 통과 차단(S_NpcBarrier 클라 통지)

**수정 파일:** `RoomServer/MidBossTactics.hpp/cpp`, `RoomServer/Room.hpp/cpp`,
`client/ServerSession.hpp/cpp`, `client/ClientApp.hpp`, `client/docs/shutdownSequence.md`
**신규 문서:** `RoomServer/docs/isisTactic.md`
**커밋:** 미커밋(working tree)

---

### [1] Isis 중간보스 전술 포팅

NPCAI(`sim/MidBossTactics.*`, `IsisMidBossTactic`)의 Isis 중간보스 전술을 RoomServer로 포팅. GrandBaum(수동적
생존형)과 대비되는 **능동적 섬멸형**. 상세 설계는 `RoomServer/docs/isisTactic.md` 참조.

**구성·흐름:**

- 부대 계약: `squad[0]/[1]` = Buddy(2차 돌격 + 보스 합류), `squad[2]/[3]` = Bomber(1차 돌격), 보스 본체 =
  `PlatoonLeader`. 스폰 순서로 인덱스 계약 보장.
- 평소엔 4스쿼드 분산 교전(`issueStableEngage`) + 보스 자체 근접전. **어느 한 스쿼드든 초기 인원 80% 미만**
  으로 깎이면 협공 사이클 잠금 해제(`checkUnlockCondition`).
- 7-phase: `Engage` ⇄ `Cooldown` → `RetreatForPincer`(전군 후퇴·집결) → `RegroupBombers` →
  `FirstBomberWedge`(Bomber 1차 쐐기) → `RegroupBuddies`(보스 합류 자리 예약) → `SecondBuddyWedge`
  (Buddy+보스 2차 쐐기, **피해 ×1.5**).
- 2단 타겟 분리(`selectStrikeClusters`): 점수 = 군집 인원 ×1000 − 거리. 2차는 1차 타깃과 겹치는 군집에
  `SECOND_STRIKE_REPEAT_PENALTY(350)` 차감 → 다른 군집으로 분산 협공.
- 보스 근접 FSM: Goblin `updateBossPersonalCombat`(score 타깃 + switch margin) 미러 + **피해 반응
  Backstep/Retreat**(누적 피해 60↑ 시 이탈, 3s 쿨다운). 협공 phase 동안엔 phase 가드로 정지(2차 쐐기 직접 합류).

**핵심 설계 판단 — 기존 인프라 전부 재사용(순수 서버 AI):**

- GrandBaum과 달리 **새 패킷·넉백·피해경감·동적소환이 일절 없음**. `MidBossTactics`에 동거 → 신규 파일 0.
- **보스 합류 ×1.5 피해** = `SquadOrder::wedgeDamageMult` → `TacticalSquad`의 `impactDamage *=
  wedgeDamageMult`. 새 피해 메커니즘 불필요.
- 쐐기 = `SquadOrderType::WedgeCharge`(+`reserveWedgeApex`로 보스 apex 슬롯 예약), 집결 = `FormationHold`,
  교전 = `issueStableEngage`, 군집 = `buildPlayerClusters` — 모두 Goblin/GrandBaum 경로 재사용.
- **보스 고속 이동**(후퇴/쐐기돌진/백스텝): 시뮬은 `setPosition` 직접적분(×15.5/28/20)이나 GameServer
  보스는 물리 모터(`setDesiredVel`, `moveBossToward`)라 그대로 못 씀 → 인게임값으로 캡(주석에 시뮬 원본 병기).
- **상수 스케일**: 월드 거리/오프셋은 인게임 스케일 ×~0.4, 시간/비율/카운트/점수(0.80, ×1000−d, 350, ×1.5,
  타이머)는 시뮬 원본 유지.
- config: Buddy 80HP/4spd, Bomber 45HP/5spd, 보스 2000HP/4spd. 인원 12/12/40/40(=104기 + 보스).
  모델/`ObjectType`은 전용 에셋 전까지 goblin 재사용.

### [2] 중간보스 전술 디버그 토글

정식 `Arena_Isis`/`Arena_GrandBaum` 레벨 마커가 없어, 검증용으로 기존 `Arena_Hobgoblin` 존 진입 시 띄울
전술을 고르는 컴파일 토글 `#define HOBGOBLIN_DEBUG_TACTIC`(0=Goblin / 1=GrandBaum / 2=Isis)을 `Room.cpp`에
추가. `bindZoneHandlers`에서 토글 값에 따라 `onArenaHobgoblin/GrandBaum/Isis Enter`로 분기. 정식 존 핸들러
(`Arena_GrandBaum`/`Arena_Isis`) 등록은 그대로 유지 → 추후 마커 저작 시 디버그 토글만 걷어내면 자연 분리.

### [3] GrandBaum/Isis 디버그 스폰 좌표 fallback

**문제:** `onArenaGrandBaumEnter`/`onArenaIsisEnter`가 스폰 좌표를 `BossSpawn` → 자기 전용
`WallGrandBaum`/`WallIsis` 마커 순으로만 찾는데, 디버그 트리거(Hobgoblin 존 재사용)엔 `WallHobgoblin`만
있고 `BossSpawn`도 없어 `haveSpawnPos=false` → **인카운터가 통째로 스킵돼 NPC가 0**이었다(GrandBaum 전환 시
아무 NPC도 안 나옴).
**수정:** 두 핸들러에 fallback 추가 — 전용 마커가 없으면 **아무 `Wall` 마커 중점 → 진입 플레이어 위치** 순으로
스폰점을 잡는다. 마커 구성과 무관하게 스폰됨.

### [4] client 종료 WSACleanup 크래시 수정

**원인:** client는 IOCP가 아니라 **completion-routine 기반 alertable overlapped I/O**(`ServerSession::registerRecv`
→ `WSARecv(…, &completionCallback)`)를 쓴다. 완료 APC는 메인 스레드가 `SleepEx(alertable)`일 때만 배달된다.
종료 시 `ClientApp::release()`가 세션을 `closesocket`하면 pending `WSARecv`가 취소되고 그 완료 APC가
큐잉되지만, **이를 드레인할 alertable wait가 종료 경로에 없어** 미완료 overlapped(이미 소멸된 세션의
`recvOver_`/`sendOver_`를 가리킴)가 남은 채 `WSACleanup()`이 호출되어 경합 크래시(간헐). 직전 커밋
`a7254c7a`(SendBuffer 정적 큐 UB)와 별개로 남아 있던 종료 결함.
**수정:** `ServerSession`에 `pendingIo_` 카운터 추가(`registerRecv`/`registerSend` 성공·PENDING 시 `++`,
`completionCallback`에서 **`gClose` 가드보다 먼저** `--`). 신규 `closeAndDrain()`이 `closesocket` 후
`pendingIo_`가 0이 될 때까지 `SleepEx(1, TRUE)`로 취소 완료 APC를 전부 드레인(200ms 가드). `ClientApp::release()`를
**게임 → 세션 `closeAndDrain` → 세션 파괴** 순서로 재구성.
**검증:** 로비 Online 연결 후 정상 종료(WM_CLOSE → WM_QUIT) **5회 반복 exit 0**(자동 검증). 클라 변경만, 서버
무관.

### [5] GrandBaum ShieldWall 튜닝 5종

인게임 확인 후 거동 개선. 1~4는 값 튜닝(인게임 재조정 전제), 5는 버그 수정.

1. 플레이어 넉백 거리 ↑ — `SHIELD_WALL_KNOCKBACK_SPEED` 36 → 54 (거리 ≈ speed×0.32s ≈ 17m).
2. 넉백 후 입력잠금 시간 ↑ — `POSTLOCK_MS` 1200 → 2000ms.
3. 중간보스 이동 속도 ↑ — `BOSS_CHASE_SPEED_MULT` 1.0 → 1.8 (모터 desired vel = moveSpeed 4 × 1.8).
4. 슬라임 이동 속도 ↓ — slime `moveSpeed` 4 → 2.5.
5. **방패벽 통과 차단(버그 수정)** — `Room::setShieldWallBlockers`가 **서버 물리 콜리전 마스크만** 바꾸고
   클라에 통지하지 않아, **클라 권한인 플레이어 이동을 못 막아** 슬라임 링을 통과하던 문제. → Goblin
   DivideAndConquer와 동일한 `S_NpcBarrier` 경로 재사용: `set/clearShieldWallBlockers`에서
   `broadcast(makeSNpcBarrierPacket(true/false, slimeIds))`. 클라가 슬라임을 `barrierObjects_`에 등록해
   `resolveBarrierSeparation`(로컬 물리 position push-out)으로 차단. `setShieldWallBlockers`가 매 틱
   호출되므로 `shieldWallBarrierOn_` 플래그로 on/off **각 1회만** 송신(죽은 슬라임은 클라가 hp로 자동 제외 =
   슬라임 처치로 구멍 내기, 의도된 카운터플레이). **클라 변경 0**(기존 barrier 경로 재사용).

**검증:** RoomServer·client Debug/x64 빌드 통과(오류 0). WSACleanup 수정은 자동 5회 검증 완료. Isis 전술·
ShieldWall 튜닝·방패벽 차단의 인게임 거동 검증은 디버그 토글로 진행(일부 인게임 확인됨). 정식 `Arena_*`
레벨 마커 저작·밸런싱은 후속 과제.

## [fix] 방패벽(슬롯 유지) NPC 도착 진동 제거 + 자리 찾기 가속

그랜드밤 방패벽 형성 시 두 가지 체감 문제 수정. 슬롯 유지 이동은 전부 `TacticalNpc::updateHoldSlot`이
담당하며, `HoldSlot`/`GuardSlot` 커맨드가 **모두** `TacticalNpcState::HoldSlot`로 수렴하므로 본 수정은
방패벽(`RingGuard`)뿐 아니라 회랑 차단선(`FormationGuard`) 등 **모든 슬롯 유지 대형에 공통** 적용된다.

### [1] 도착 후 진동 — 모터 vs 솔버 한계 진동(limit cycle)

**원인:** 전술 NPC는 `setCollisionMask(~(Player|Boss))`로 **NPC끼리는 물리 충돌**하는 Dynamic(mass 70)
바디다. 밀집 링 슬롯에서 바디가 겹치면 PGS+Baumgarte 솔버가 밀어내는데, 기존 도착 정지 임계가
`separationRadius_ × 0.25 ≈ 0.75m`로 **히스테리시스가 없어**, 솔버가 0.75m 밖으로 밀어내면 모터가 곧바로
다시 슬롯으로 강하게 당김 → 솔버 밀어냄 ↔ 모터 당김의 무한 반복(떨림).
**수정:** 안착 래치(히스테리시스) 도입. `updateSlotSettled(distToSlot)` — `inner`(0.25×sepRad) 진입 시
`slotSettled_=true`, `outer`(0.7×sepRad ≈ 2.1m) 이탈 시에만 해제. 안착 동안 `settleAtSlot()`이
`setDesiredVel(0)` + 잔여 XZ 선속도 감쇠(`×0.3`, Y는 중력 보존)로 솔버 주입 속도까지 흡수한다. 즉 한 번
안착하면 ~2.1m 밖으로 밀려나기 전까지 모터를 제동 상태로 묶어, 솔버 평형 변위에 재당김하지 않게 한다.
새 슬롯 수령(`HoldSlot`/`GuardSlot` 커맨드)·`reviveAt` 시 `slotSettled_=false`로 리셋.

### [2] 자리 찾기 느림 — 마지막 접근 크롤링

**원인:** `TACTICAL_SLOT_ARRIVE_SLOW_RADIUS=3.0m` 안에서 거리비례 감속 + 최저 `MIN_SCALE=0.15`(≈1.8 m/s)로
마지막 ~3m를 기어가듯 접근.
**수정:** 래치가 오버슈트를 흡수하므로 더 공격적으로 튜닝 — `SLOW_RADIUS` 3.0→1.5, `MIN_SCALE` 0.15→0.35
(최저 ≈4.2 m/s). 전속(12 m/s) 구간을 1.5m까지 연장, 감속 구간 절반으로 단축. 선두가 빨리 안착해 모터 압박을
멈추면 후속 NPC의 물리 교착도 빨리 풀려 대형 완성 시간이 전반적으로 단축된다.

**변경 파일:** `RoomServer/TacticalNpc.{hpp,cpp}` (`slotSettled_` 멤버, `updateSlotSettled`/`settleAtSlot`
헬퍼, 래치/감쇠 상수, 감속 상수 2개 튜닝, 두 도착 분기·커맨드·`reviveAt` 적용).
**보조 옵션(미적용):** 다중 열 구간에 잔여 떨림이 남으면 `MidBossTactics.hpp`의 `SHIELD_RING_LANE_SPACING`
0.9→~1.3, `SHIELD_RING_MIN_ARC_SPACING` 1.2→~1.5로 정지 시 바디 겹침 자체를 줄일 수 있음(방패벽이 약간
퍼져 보이는 트레이드오프).
**검증:** RoomServer Debug/x64 빌드 통과(경고·오류 0). 인게임 거동(빠른 안착·무진동) 확인은 후속.

### [3] 방패벽 형성 속도 추가 가속 (RingGuard 오더 speedMult)

[2]의 감속 튜닝 후에도 더 빠른 형성을 원해 추가. 슬라임 접근 = `moveSpeed(2.5) × TACTICAL_SPEED_MULT(3.0)
× speedMult(1.0) = 7.5 m/s`인데, 슬라임 `moveSpeed 2.5`는 전투 페이싱용으로 낮춘 값(`Room.cpp:1446`)이라
건드리지 않고, 전역 `TACTICAL_SPEED_MULT`는 모든 전술 NPC에 영향. → **방패벽 오더에만 speedMult를 실어**
형성만 가속(전투 속도 불변). `TacticalSlime` 서브클래스는 미도입(슬라임은 트루퍼·뱀과 수치만 다른 config 차이,
고유 행위 없음 — 이 코드베이스는 "행위는 상속(`PlatoonLeader`), 스탯은 config" 패턴).
**수정:** `SHIELD_WALL_APPROACH_SPEED_MULT=2.0`(`MidBossTactics.hpp`) 신설 → `issueShieldWall`의 RingGuard
`ord.speedMult`에 부여 → `TacticalSquad.cpp` RingGuard case의 HoldSlot 커맨드에 `.speedMult=ord.speedMult`
전달(기존 `SquadOrder`/`TacticalCommand.speedMult` 배선 재사용). 형성 접근 7.5→15 m/s. 방패벽 종료
(`issueEngage`) 시 `speedMult_` 1.0 리셋되어 boost는 형성 단계 한정.
**검증:** RoomServer Debug/x64 빌드 통과(경고·오류 0). 인게임 체감(더 빠른 형성)·과속 시 1.6 하향은 후속.

### [4] 방패벽 형성 완료 전 플레이어 재진입 차단 (형성 동안 재넉백)

**증상:** 방패벽 발동 시 넉백+2초 입력락으로 플레이어를 밀어내지만, 슬라임이 링을 완성하는 데 2초보다 더
걸려서, 락이 풀리는 시점엔 벽이 아직 형성 중(큰 틈)이라 플레이어가 다시 안으로 들어감.
**원인:** 넉백+락은 **고정 시간**인데 형성 시간은 가변. 클라 차단벽은 *개별 슬라임* barrier 연결
(`resolveBarrierSeparation`)이라 슬라임이 슬롯 미도착이면 벽 자체가 없음. 넉백은 `issueShieldWall` 첫 발령 시
**1회만** 호출. → 락 연장은 미봉책.
**수정(서버 전용, 기존 넉백 재사용):** `Phase::ShieldWall` 업데이트의 0.5s refresh 블록에 형성 게이트 추가 —
`shieldWallFormed_`가 false인 동안, 신규 `isShieldWallFormed`(슬라임 부대 0,1,2가 모두 `areMembersAtSlots`)면
플래그 set, 아니면 `room.knockPlayersOutOfShieldWall(center, radius)` 재호출(링 안 플레이어만 재넉백+재락).
형성 완료 시 슬라임 barrier가 차단 인계 → 중단(슬라임 처치로 구멍 내는 카운터플레이 유지). 안전 타임아웃
`SHIELD_WALL_FORM_KNOCK_MAX=6s`로 형성 비정상 지연 시 영구 락 방지. `enterPhase(ShieldWall/Engage)`에서
`shieldWallFormed_`/`shieldWallFormTimer_` 리셋. **클라/프로토콜 변경 0.**
**검증:** RoomServer Debug/x64 빌드 통과(경고·오류 0). 인게임(형성 전 재진입 차단·형성 후 카운터플레이 유지)은 후속.

---

### 2026.06.15 ~ 06.16
## [mod, fix] 그랜드밤 방패벽 — 슬라임 ↔ 뱀 물리 충돌 제거 (충돌 레이어 분리)
## [feat] 그랜드밤 방패벽 — 후퇴 원본 뱀 퇴장(숨김)/복귀 + `S_NpcHide`

**수정 파일:** `RoomServer/rigidBody.hpp`, `RoomServer/Room.{hpp,cpp}`, `RoomServer/MidBossTactics.{hpp,cpp}`,
`RoomServer/PacketManager.{hpp,cpp}`, `ServerEngine/protocol.hpp`, `client/object.{hpp,cpp}`,
`client/online/onlineGame.{hpp,cpp}`, `client/PacketManager.{hpp,cpp}`

---

### [1] 슬라임 ↔ 뱀 물리 충돌 제거 (충돌 레이어 분리)

방패벽 발동 중 동적 소환된 증원 뱀 웨이브가 슬라임 링을 거의 직선으로 **관통하며 진형을 밀어** 무너뜨리던
문제. 슬라임과 뱀이 서로 물리적으로 밀지 않도록 충돌 레이어(비트마스크)를 분리.

**원인:** 충돌 필터는 비트마스크 기반 — 쌍 (a,b)는 `(a.category & b.mask) != 0 && (b.category & a.mask) != 0`
일 때만 충돌(`rigidBody.hpp`). 전술 NPC는 카테고리가 기본값(전체 비트 1)이라, 한쪽 마스크에서 비트를
빼도 상대 카테고리에 다른 비트가 남아 충돌이 안 끊긴다. → **양쪽에 고유 카테고리 비트**를 부여해야 한다.

**수정:**

- `CollisionLayer`에 비트 2종 추가(기존 `Player=1<<0`, `Boss=1<<1`):
  ```cpp
  constexpr uint32_t Slime = 1u << 2;  // 방패벽 슬라임 링(블로커)
  constexpr uint32_t Snake = 1u << 3;  // 증원 뱀 웨이브
  ```
- 방패벽 슬라임(블로커): `category=Slime`, `mask=~(Boss | Snake)` (`Room::setShieldWallBlockers`).
  플레이어 차단(하드 블로커)·슬라임끼리·지형 충돌은 유지하고, 뱀만 통과시킨다.
- 웨이브 뱀: `category=Snake`, `mask=~(Player | Boss | Slime)` (`Room::spawnTacticalWaveNpc`).
  뱀끼리·지형 충돌은 유지(겹침 방지·지면 안착), 플레이어/보스/슬라임은 통과.
- 블로커 해제/원복 시(`set/clearShieldWallBlockers`) 마스크뿐 아니라 **카테고리도 기본값(`0xFFFFFFFF`)**
  으로 되돌려 일반 NPC 충돌로 복귀.

뱀 ↔ 슬라임 충돌만 제거되고 나머지 쌍(슬라임↔플레이어/슬라임/지형/보스, 뱀↔뱀/지형/플레이어/보스)과
하드 블로커·게임 로직(뱀이 플레이어 돌진·웨이브 전멸 시 방패벽 종료)은 전부 보존된다.

---

### [2] 후퇴 원본 뱀 퇴장(숨김)/복귀 + `S_NpcHide`

방패벽 발동 시 평소 `updateSnakeEvasion`으로 플레이어를 피해 산개하던 **원본 뱀 부대(squad index 3)** 가
회피를 멈추고 `issueOriginalSnakeRetreat`의 `FormationHold`로 링 외곽(`SNAKE_OUTER_RADIUS`=26m)에 정지 →
증원 웨이브가 진행되는 `WaveActive` 내내 그 자리에 서서 **플레이어에게 공격당할 수 있게 노출**되던 문제.
후퇴한 원본 뱀을 웨이브 동안 전장에서 퇴장(시체·죽는 연출 없이 숨김)시켰다가 방패벽 종료 시 복귀시킨다.

**서버 — 사망 상태로 전환하되 객체는 시체로 유지:**

후퇴 완료 시점(`updateSnakeAmbush`의 `RetreatingOriginal` 분기에서 `spawnSnakeWave` 직후)에
`despawnOriginalSnakeSquad`가 `originalSnakeRoster_`의 살아있는 뱀을 퇴장 처리. 신규
`Room::despawnTacticalNpcHidden`:

```cpp
void Room::despawnTacticalNpcHidden(uint32_t id) {
    TacticalNpc* npc = findTacticalNpcById(id);
    if (!npc || npc->hp() <= 0) return;
    npc->setHp(0);
    physicsWorld_.unregisterBody(&npc->body());   // 정상 사망과 동일 — 물리 명시 제거
    npcBodyOwner_.erase(&npc->body());
}
```

- **함정:** 정상 사망은 `npc->update()`가 `result.justDied`를 반환하는 프레임에만 물리 바디를 제거
  (`Room.cpp` tactical NPC 루프). `setHp(0)`만 하면 `justDied`가 안 켜져 **물리 바디가 남고**, 부활 시
  `reviveTacticalNpc`가 재등록하며 **이중 등록**된다 → 디스폰 시 `unregisterBody`를 **명시적으로** 호출.
- 객체(`tacticalNpcs_`)는 시체로 유지해야 roster 기반 부활이 가능하므로 `removeTacticalNpcById`
  (객체까지 제거)는 쓰지 않는다.
- 복귀는 기존 인프라 그대로: `finishShieldWall`→`reviveOriginalSnakeSquad`의 `hp<=0` 분기 →
  `reviveTacticalNpc`(reviveAt + 물리 재등록 + `S_NpcRespawn`).

**신규 패킷 `S_NpcHide` (사망 `S_Hit`와 분리):**

```cpp
struct SNpcHideInfo { uint16 npcId; };
struct SNpcHidePacket : public PacketHeader { uint16 dataOffset; uint16 npcCount; /* DataList */ };
```

`despawnOriginalSnakeSquad`가 실제로 숨긴 id들을 묶어 `broadcast(makeSNpcHidePacket(ids))`로 일괄 통지
(`S_NpcBarrier`의 id-list 직렬화 미러). 사망(시체+래그돌)이 아니라 **즉시 비표시**로 표현하기 위한 별도 경로.

**클라 — 타입 독립 `hidden_` 플래그:**

- `Object`(공통 베이스)에 `hidden_` 추가(`setHidden`/`hidden`). 사망 `isDead_`와 **별개** — 시체가 아니라
  "존재하지 않는 것처럼" 완전 제외.
- `Object::update()`/`render()` 시작에서 `if (hidden_) return;`. 고블린 HP바 루프에서도 hidden 제외.
- `Game::hideNpcs(ids)`: `S_NpcHide` 수신 핸들러가 호출, id로 NPC 조회 후 `setHidden(true)`(+활성 래그돌 해제).
- `Game::onNpcRespawn`: 기존 처리에 `setHidden(false)` 추가 → 복귀 시 재표시.
- **타입 독립 설계:** `hidden_`이 공통 베이스에 있어 향후 전용 뱀 NPC가 goblin 모델에서 분리돼도 그대로
  동작하며, 이전 비용은 `hideNpcs`의 id 조회 통합 정도로 최소화된다.

**검증:** 전체 솔루션(ServerEngine/RoomServer/LobbyServer/client) Debug/x64 빌드 통과(신규 경고·오류 0).
인게임 실검증(후퇴 뱀 즉시 사라짐·웨이브 중 외곽 노출 없음·전멸 후 재등장·회피 재개)은 `Arena_GrandBaum`
트리거 확보 후 후속.

---

## 2026-06-17 — 전술 전투 분리 & PlatoonLeader 디커플링 (branch: `temp_server`)

미드보스 전술(홉고블린/GrandBaum/Isis)을 **독립 모듈로 분리**하고, 검증용 임시 코드를 제거했다.
또한 `PlatoonLeader`가 특정 보스 전술에 결합돼 있던 부분을 끊어 **인카운터별 명시 주입**으로 통일했다.
공통 전투 엔진(`TacticalNpc`/`TacticalSquad`/`PlatoonLeader`/`MidBossTacticBase`)은 세 전술이 함께 쓰는
기반이라 **그대로 유지**했다. 두 작업 모두 `RoomServer`(Debug|x64) 빌드 통과 검증 완료.

---

### 1. 전술 전투 분리 (스캐폴딩 제거 + 보스별 파일 분리)

#### 배경
그간 GrandBaum·Isis 전술을 검증하려고 **홉고블린 아레나(`Arena_Hobgoblin`)에 다른 보스 전투를 임시로
띄워** 한 자리에서 비교 테스트해 왔다. 이제 각 보스가 전용 zone/마커(`Arena_Grandbaum` + `WallGrandbaum_*`
+ `GrandbaumSpawner`, `Arena_Isys` + `WallIsys_*` + `IsysSpawner`)를 갖췄으므로, 테스트 전용 코드를
제거하고 각 전술을 자기 아레나에서만 트리거되는 독립 모듈로 정리했다.

> 참고: "전략 계층"은 이미 분리돼 있었다 — `IMidBossTactic` 인터페이스 + 폴리모픽 구현 3개. 이번 작업은
> ① 테스트 스캐폴딩 제거 ② 한 파일에 동거하던 전술 구현을 보스별로 물리 분리 두 가지다.

#### A. 테스트 스캐폴딩 제거
- `Room.cpp`의 `HOBGOBLIN_DEBUG_TACTIC` 매크로 + `bindZoneHandlers`의 `#if/#elif/#else` 제거 →
  `Arena_Hobgoblin`이 다시 **홉고블린 전용**으로 복귀. (매크로가 `1`이라 그동안 Arena_Hobgoblin이
  GrandBaum을 스폰하던 상태였음.)
- `onArenaGrandBaumEnter`/`onArenaIsisEnter`의 `[디버그 트리거]` fallback(전용 마커가 없을 때
  any-Wall 중점 → 진입 플레이어 위치로 스폰) 제거. 보스 **자기 벽** 기반 fallback은 정식 경로라 유지.
- 사장 코드 `TacticalGoblin::spawnEncounter()`(호출부 없는 래퍼) 제거 + 불필요해진 `Room.hpp` 인클루드·
  `class Room` 전방선언 정리. `trooperConfig()`/`bossConfig()`는 `Room::spawnTacticalGoblinEncounter`에서
  사용 중이라 그대로 유지.

#### B. `MidBossTactics` 보스별 분리
단일 파일(`MidBossTactics.hpp` 551줄 / `MidBossTactics.cpp` 3688줄)을 아래 5쌍으로 분리. **로직 변경 0**
(함수 본문을 그대로 이동).

| 새 파일 | 내용 |
|---|---|
| `MidBossTacticBase.{hpp,cpp}` | 공용 유틸 (clusters, engage, centroid, `issueStableEngage`, `isLivingPlayerTarget`) |
| `GoblinMidBossTactic.{hpp,cpp}` | 홉고블린 전술 |
| `GrandBaumMidBossTactic.{hpp,cpp}` | GrandBaum 전술 (ShieldWall/뱀 매복/넉백) |
| `IsisMidBossTactic.{hpp,cpp}` | Isis 전술 (2연속 쐐기 협공) |

- 파일-로컬 `static` 헬퍼는 내부 링키지 보존을 위해 필요한 .cpp에만 복제: `norm3`(전부), `lenXZ`(Goblin/Isis).
  `isisRng()`은 Isis에 귀속.
- 인클루드 갱신: `Room.cpp` → GrandBaum + Isis 헤더(Goblin은 §2에서 추가), `PlatoonLeader.cpp` → Goblin 헤더(§2에서 재조정).
- 원본 2파일 삭제, `RoomServer.vcxproj` / `RoomServer.vcxproj.filters` 등록 교체.

> **이슈/해결 (UTF-8 BOM)**: 새로 만든 `.hpp` 4개에 UTF-8 BOM이 빠져 있어, MSVC가 한글 주석을 시스템
> 코드 페이지(cp949)로 오해석(C4819) → 헤더 파싱이 깨지면서 `constexpr` 상수가 미선언으로 처리됨
> (`CAPTURE_CHARGE_STANDOFF`, `RETREAT_TIMEOUT` C2065). 기존 소스가 모두 UTF-8 **BOM** 포함이었던 것을
> 확인하고 4개 헤더에 BOM을 추가해 해결.

#### C. 문서 갱신
`RoomServer/docs/`의 `grandBaumTactic.md`, `isisTactic.md`, `tacticalReservationAndEngage.md`에서
파일명 참조·"(Goblin 전술과 동거)" 표현·제거된 디버그 트리거 안내를 실제 zone 태그(`Arena_Grandbaum`/
`Arena_Isys`)와 현 파일 구조에 맞게 정리.

#### 검증
`RoomServer`(Debug|x64) 빌드 통과 — 오류 0, 새 파일 경고 0.

---

### 2. PlatoonLeader 기본 tactic 디커플링

#### 배경
의도는 **실행되는 전술 전투에 맞춰** 해당 `MidBossTactic`(Goblin/GrandBaum/Isis)이 주입되는 것이었다.
GrandBaum·Isis는 이미 3-arg 생성자로 명시 주입했지만, 홉고블린 인카운터만 `PlatoonLeader`의 2-arg 생성자
**기본값(`GoblinMidBossTactic`)**에 의존했다. 이 때문에 공유 인프라인 `PlatoonLeader.cpp`가 특정 보스 모듈
(`GoblinMidBossTactic.hpp`)을 하드 의존하는 잔여 결합이 있었다.

#### 변경
- `PlatoonLeader.hpp`: 2-arg 생성자(`cfg = {}`) 선언 제거 → 전술을 받는 **3-arg 생성자만** 유지
  (전술 없이는 보스를 만들 수 없게 강제).
- `PlatoonLeader.cpp`: 기본 생성자 정의 + `GoblinMidBossTactic.hpp` 의존 제거 → **`IMidBossTactic`에만 의존**.
- `Room.cpp`: `spawnTacticalGoblinEncounter`가 GrandBaum/Isis와 대칭으로
  `std::make_unique<GoblinMidBossTactic>()`를 명시 전달하고 `GoblinMidBossTactic.hpp`를 인클루드.

#### 결과 / 검증
세 인카운터 모두 자기 전술을 **동일한 방식(명시 주입)**으로 전달. 동작은 동일(홉고블린 인카운터는 여전히
`GoblinMidBossTactic` 실행). `RoomServer`(Debug|x64) 빌드 통과.

---

### 영향받은 파일

- **신규(8)**: `MidBossTacticBase.{hpp,cpp}`, `GoblinMidBossTactic.{hpp,cpp}`,
  `GrandBaumMidBossTactic.{hpp,cpp}`, `IsisMidBossTactic.{hpp,cpp}`
- **삭제(2)**: `MidBossTactics.{hpp,cpp}`
- **수정**: `Room.cpp`, `PlatoonLeader.{hpp,cpp}`, `TacticalGoblin.{hpp,cpp}`,
  `RoomServer.vcxproj`, `RoomServer.vcxproj.filters`,
  `docs/{grandBaumTactic,isisTactic,tacticalReservationAndEngage}.md`

### 남은 작업 / 주의
- 디버그 fallback을 제거했으므로 GrandBaum/Isis는 전용 마커(`WallGrandbaum_*`/`GrandbaumSpawner`,
  `WallIsys_*`/`IsysSpawner`)가 레벨에 저작돼 있어야 스폰된다(미저작 시 인카운터 스킵).
- 실행 검증은 실제 `client`로(DummyClient 아님): `Arena_Hobgoblin` → 홉고블린, `Arena_Grandbaum` →
  GrandBaum, `Arena_Isys` → Isis 가 각각 뜨는지 확인.
- 향후 `TacticalSlime`/`TacticalSnake` 등 per-type NPC 클래스 도입은 이번 작업과 독립적인 후속 가산 작업
  (`TacticalGoblin` 패턴을 템플릿으로 사용).

---

## 2026-06-18 — 보스 코드 철자 정합 (`GrandBaum`→`Grandbaum` / `Isis`→`Isys`) (branch: `server`)

레벨 바이너리는 보스 이름을 `Grandbaum`·`Isys`로 저작하는데, 코드 식별자(클래스·함수·구조체·변수·주석·
로그 문자열·파일명·문서)는 옛 철자 `GrandBaum`(대문자 B)·`Isis`를 써 와 불일치가 남아 있었다. zone 태그/
마커 문자열(`Arena_Grandbaum`/`WallGrandbaum_*`/`GrandbaumSpawner`, `Arena_Isys`/`WallIsys_*`/`IsysSpawner`)
은 앞선 작업(81ad239a)에서 이미 바이너리에 맞췄고, 이번엔 **나머지 코드 철자를 전부 바이너리에 통일**했다.
기능·런타임 거동 변화는 없는 순수 리네임(와이어/프로토콜에 보스 이름 문자열·enum 없음).

### 범위
- **GameServer 솔루션만** 변경. NPCAI 프로토타입 시뮬레이터(GameServer.sln 밖, 레벨 바이너리 무관)는 그대로 둠.
- 이 DEVLOG의 **과거 이력 항목은 당시 명칭 그대로 보존**(이 항목만 신규 추가).

### 변경
- **파일 리네임(`git mv`)**: `GrandBaumMidBossTactic.{hpp,cpp}`→`Grandbaum...`,
  `IsisMidBossTactic.{hpp,cpp}`→`Isys...`, `docs/grandBaumTactic.md`→`grandbaumTactic.md`,
  `docs/isisTactic.md`→`isysTactic.md`.
- **일괄 치환(대소문자 구분)** `GrandBaum`→`Grandbaum`, `grandBaum`→`grandbaum`, `Isis`→`Isys`를
  GameServer 내 대상 파일에 적용. 옳은 리터럴(`Grandbaum`/`Isys`)은 대소문자가 달라 미충돌 → 이중 변환 없음.
  이 치환으로 `#include`·`RoomServer.vcxproj`·`.filters`의 옛 파일명 참조까지 자동 정합.
- **영향 파일**: RoomServer 전술 4파일, `Room.{cpp,hpp}`, `GameSession.hpp`, `object.hpp`,
  `MidBossTacticBase.hpp`, `RoomServer.vcxproj(.filters)`, `docs/{grandbaumTactic,isysTactic}.md`,
  `ServerEngine/protocol.hpp`(주석), `client/online/onlineGame.{cpp,hpp}`(주석).

### 검증
- `GrandBaum`(대문자 B)·`Isis` 대소문자 구분 잔여 검색 → 대상 파일 0건(과거 DEVLOG 이력 제외).
- `RoomServer`(Debug|x64) 빌드 통과 — 리네임 파일·인클루드·프로젝트 등록 정합 확인.

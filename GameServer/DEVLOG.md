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

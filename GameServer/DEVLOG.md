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

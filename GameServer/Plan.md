# NPC AI 포팅 계획: NPC-AI-Lab → RoomServer

## 배경

기존 Goblin의 4-state AI(Patrol/Chase/Attack/Return)를  
NPC-AI-Lab에서 검증한 **8-state FSM + NpcGroup 공유 기억 시스템**으로 교체.  
원본 소스: `d:\source\repos\NPC-AI-Lab\NPCAI\NPCAI\sim\`

클래스 계층 변경: `Object → Npc → Goblin`

---

## 현재 상태

| 항목 | 현황 |
|------|------|
| `Npc.hpp/cpp` | 미존재 — 신규 작성 |
| `NpcGroup.hpp/cpp` | 미존재 — 신규 작성 |
| `Goblin` (object.hpp/cpp) | 4-state FSM 존재 — 제거 후 Npc 상속 |
| `Room` 쿼리 인터페이스 | 부재 — 추가 |
| `Level.cpp` | `setHp(90)` 직접 호출 — `applyGoblinConfig()`로 교체 |

---

## 구현 순서

### Step 1. `RoomServer/NpcGroup.hpp` (신규)

원본 `sim/NpcGroup.hpp` 이식. 변경점:
- `Vec3` → `mu::Vec3` (필드 `.x/.y/.z` → 메서드 `.x()/.y()/.z()`)
- `uint32_t` → `uint32`
- `namespace sim` 제거
- `mu::Vec3`를 값으로 받는 메서드에 `MU_CALLCONV` 추가

```cpp
struct SharedTargetMemory {
    uint32       playerId = 0;          // 0 = 빈 슬롯
    uint32       reporterNpcId = 0;
    mu::Vec3     lastKnownPosition{};
    Milliseconds lastSeenMs{ 0ms };
    Milliseconds expireMs  { 0ms };
    bool         valid = false;
};

class NpcGroup {
public:
    NpcGroup(int groupId, mu::Vec3 center, float radius,
             Milliseconds memoryDuration = 3000ms);
    void addMember(uint32 npcId);
    void removeMember(uint32 npcId);
    void MU_CALLCONV reportSight(uint32 npcId, uint32 playerId,
                                  mu::Vec3 pos, Milliseconds currentMs);
    bool                      hasValidMemory                 (Milliseconds currentMs) const;
    const SharedTargetMemory* getBestMemory                  (Milliseconds currentMs) const;
    const SharedTargetMemory* getBestMemoryInsideActivityArea(Milliseconds currentMs) const;
    bool MU_CALLCONV isInsideActivityArea(mu::Vec3 pos) const;
    void clearMemory();
    void update(Milliseconds currentMs);
    int          getGroupId() const;
    mu::Vec3     getCenter()  const;
    float        getRadius()  const;
private:
    int          groupId_;
    mu::Vec3     activityCenter_;
    float        activityRadius_;
    Milliseconds memoryDuration_;
    std::vector<uint32> members_;
    std::array<SharedTargetMemory, 4> memories_{};
};
```

### Step 2. `RoomServer/NpcGroup.cpp` (신규)

원본 `sim/NpcGroup.cpp` 이식. Vec3 API 및 시간 타입 치환:

| 원본 (sim) | RoomServer |
|-----------|-----------|
| `uint32_t currentTick` | `Milliseconds currentMs` |
| `mem.expireTick = currentTick + memoryDurationTick_` | `mem.expireMs = currentMs + memoryDuration_` |
| `mem.expireTick <= currentTick` | `mem.expireMs <= currentMs` |

### Step 3. `RoomServer/Npc.hpp` (신규)

원본 `sim/Npc.hpp` 이식. 변경점:
- 기반 클래스: `Actor` → `Object`
- `update()` 반환형: `void` → `NpcUpdateResult`
- `Player*` → `GameSession*`

```cpp
struct NpcUpdateResult {
    struct HitInfo { uint16 targetId; int32 newHp; };
    std::optional<HitInfo> hit;
};

struct NpcConfig {
    float maxHp              = 80.f;
    float moveSpeed          = 4.f;
    float detectionRange     = 10.f;
    float attackRange        = 2.f;
    float activityZoneRadius = 28.f;
    float attackDamage       = 10.f;
    float attackWindupTime   = 0.4f;
    float attackRecoverTime  = 0.6f;
    float separationRadius   = 4.f;
    float separationWeight   = 0.6f;
    bool  canReAggroOnReturn = true;
    int   overlapThreshold   = 2;
    float returnSpeedMult    = 2.5f;
};

enum class NpcState {
    Idle, Chase, AttackWindup, AttackRecover,
    Return, Reposition, Dead, Investigate
};

class Npc : public Object {
public:
    Npc() = default;
    Npc(Object&& base, const NpcConfig& cfg = {});

    NpcUpdateResult update(Seconds dt, Room& room);

    NpcState getState()    const { return state_; }
    uint32   getTargetId() const { return targetId_; }  // countNpcsTargeting용 — public 필수
    int      getGroupId()  const { return groupId_; }
    void     setGroupId(int id)  { groupId_ = id; }
    void     MU_CALLCONV setSpawnPos(mu::Vec3 p);
    void     MU_CALLCONV setActivityZone(mu::Vec3 center, float radius);

protected:
    void applyConfig(const NpcConfig& cfg);

private:
    void transitionTo(NpcState next);
    NpcUpdateResult updateIdle         (Seconds dt, Room& room);
    NpcUpdateResult updateChase        (Seconds dt, Room& room);
    NpcUpdateResult updateAttackWindup (Seconds dt, Room& room);
    NpcUpdateResult updateAttackRecover(Seconds dt, Room& room);
    NpcUpdateResult updateReturn       (Seconds dt, Room& room);
    NpcUpdateResult updateReposition   (Seconds dt, Room& room);
    NpcUpdateResult updateDead         ();
    NpcUpdateResult updateInvestigate  (Seconds dt, Room& room);

    GameSession* selectBestTarget(Room& room) const;
    mu::Vec3     MU_CALLCONV calcSeparationForce(const std::vector<mu::Vec3>& nearby) const;
    bool         isOutsideActivityZone() const;
    bool         isOvercrowded(const std::vector<mu::Vec3>& nearby) const;

    NpcState state_{ NpcState::Idle };
    mu::Vec3 spawnPos_{};
    mu::Vec3 activityZoneCenter_{};
    float    activityZoneRadius_{ 28.f };
    uint32   targetId_{ 0 };
    int      groupId_{ -1 };

    float detectionRange_, attackRange_, moveSpeed_, attackDamage_;
    float attackWindupTime_, attackRecoverTime_;
    float separationRadius_, separationWeight_;
    bool  canReAggroOnReturn_;
    int   overlapThreshold_;
    float returnSpeedMult_;

    float windupTimer_{ 0.f };
    float recoverTimer_{ 0.f };
    float targetEvalTimer_{ 0.f };

    mu::Vec3 repositionDir_{ 1.f, 0.f, 0.f };
    float    repositionTimer_{ 0.f };

    std::vector<mu::Vec3> nearbyCache_;

    static constexpr float TARGET_EVAL_INTERVAL = 0.5f;
    static constexpr float REPOSITION_TIMEOUT   = 1.5f;
};
```

### Step 4. `RoomServer/Npc.cpp` (신규)

원본 `sim/Npc.cpp` (~500줄) 이식. 핵심 치환표:

| 원본 (sim) | RoomServer |
|-----------|-----------|
| `Vec3::distance(a,b)` | `(a-b).len()` |
| `Vec3::distanceSq(a,b)` | `(a-b).len2()` |
| `v.length()` | `v.len()` |
| `v.normalized()` | `mu::NVec3(v)` (NVec3 타입; 성분 필요 시 `.x()/.y()/.z()` 추출) |
| `position_ += dir * spd * dt` | `setLinearVel(mu::Vec3(nd.x()*spd, body().linearVel().y(), nd.z()*spd))` |
| `facing_ = dir` | `float yaw = std::atan2(nd.x(), nd.z()); setOrient(mu::NQuat(Radian(), Radian(), Radian(yaw)))` |
| `position_ = spawnPos_` (순간이동) | `setPos(spawnPos_); body().snapToCurrent();` |
| `alive_` / `isAlive()` | `hp() > 0` |
| `id_` 직접 접근 | `getId()` |
| `room.findActorById(targetId_)` | `room.findLivingSessionByPlayerId(targetId_)` (신규) |
| `p->getPosition()` | `session->player()->pos()` |
| `target->takeDamage(dmg)` | `result.hit = {targetId, newHp}` |
| `room.getLivingPlayers()` → `Player*` | `GameSession*` 벡터 |
| `p->getHp()` | `session->player()->hp()` |
| `float dt` | `Seconds dt` (`.count()` 필요 시) |
| `Logger::get().log*(...)` | 제거 |

**피격 처리**: sim은 `takeDamage()` 직접 호출. RoomServer는 `NpcUpdateResult.hit`에 채워서 반환 → Room이 broadcast.

**생성자**: `nearbyCache_.reserve(16)` 호출. 매 틱 `clear()`+`push_back()` 반복 시 capacity 16 이하에서 재할당 방지.

### facing\_ / 이동 방식 결정

#### 배경: sim과 RoomServer의 위치 관리 차이

sim(`Actor`)은 `position_`(Vec3)과 `facing_`(Vec3)을 직접 멤버로 보유하고 매 틱 직접 수정한다.
물리 엔진이 없으므로 `position_ += dir * speed * dt` 한 줄로 이동이 완성된다.

RoomServer `Object`는 위치를 `body_`(RigidBody) 안에 저장한다. 위치 수정 경로가 두 가지다:
- **`Object::setPos()`**: 위치를 즉시 덮어쓴다. 내부에서 `rebuildBodyBVH()`를 호출한다.
- **`setLinearVel()` + `PhysicsWorld::step()`**: 속도를 기록해두면 물리 엔진이 다음 `step()`에서 적분해 위치를 갱신한다. `Object::setPos()`를 거치지 않는다.

#### 왜 매 틱 `setPos()` 직접 호출은 안 되는가

`Object::rebuildBodyBVH()`는 모델의 모든 BVH 노드를 순회하며 월드 행렬 변환(스케일 → 쿼터니언 회전 → 평행이동)을 재계산한다. 정적 배치 시 1회 호출하도록 설계된 함수다. NPC가 매 틱 이동할 때마다 호출되면 불필요한 행렬 연산이 반복된다.

또한 `setPos()`로 직접 이동하면 물리 충돌 처리를 우회한다. 물리 엔진의 contact solver가 관여하지 않으므로 지형을 뚫고 지나가거나 경사면에서 미끄러지는 처리가 깨진다.

#### 채택 방식: `setLinearVel` + `setOrient`

현재 `Goblin::update()`와 동일한 패턴을 Npc에서도 사용한다.

**일반 이동 (Chase / Return / Reposition / Investigate)**
```cpp
// sim
Vec3 moveDir = (chaseDir + sepForce * separationWeight_).normalized();
facing_   = moveDir;
position_ += moveDir * (moveSpeed_ * dt);

// RoomServer
mu::NVec3 nd(chaseDir + sepForce * separationWeight_);   // normalized
setLinearVel(mu::Vec3(nd.x() * moveSpeed_, body().linearVel().y(), nd.z() * moveSpeed_));
float yaw = std::atan2(nd.x(), nd.z());
setOrient(mu::NQuat(mu::Radian(), mu::Radian(), mu::Radian(yaw)));
```
- y축 linearVel을 `body().linearVel().y()`로 보존 → 중력 계속 작용
- `(chaseDir + sepForce * separationWeight_)` 합산 후 `mu::NVec3(...)` 생성자로 정규화 — sim의 `.normalized()` 대체
- 현재 facing을 읽어야 할 때는 `forward()`로 접근 (orient 기반으로 자동 계산됨)

**AttackWindup — 이동 없이 facing만 미세 조정**
```cpp
// sim
Vec3 sep = calcSeparationForce(nearbyCache_);
if (sep.length() > 0.1f)
    facing_ = (facing_ + sep * 0.3f).normalized();

// RoomServer
mu::Vec3 sep = calcSeparationForce(nearbyCache_);
if (sep.len() > 0.1f) {
    mu::NVec3 newFacing(forward() + sep * 0.3f);
    float yaw = std::atan2(newFacing.x(), newFacing.z());
    setOrient(mu::NQuat(mu::Radian(), mu::Radian(), mu::Radian(yaw)));
    // setLinearVel 호출 없음 — windup 중 이동 없음
}
```
현재 facing은 `forward()`로 읽는다. `facing_` 멤버 불필요.

**AttackRecover — 분리력에 의한 미세 밀림**
```cpp
// sim
Vec3 sep = calcSeparationForce(nearbyCache_);
if (sep.length() > 0.1f)
    position_ += sep * (separationWeight_ * 0.3f * moveSpeed_ * dt);

// RoomServer
mu::Vec3 sep = calcSeparationForce(nearbyCache_);
if (sep.len() > 0.1f) {
    float driftSpd = sep.len() * separationWeight_ * 0.3f * moveSpeed_;
    mu::NVec3 nd(sep);
    setLinearVel(mu::Vec3(nd.x() * driftSpd, body().linearVel().y(), nd.z() * driftSpd));
    // setOrient 호출 없음 — recover 중 facing 변경 없음
}
```

**스폰 복귀 스냅 (Return 완료 시)**
```cpp
// sim
position_ = spawnPos_;
transitionTo(NpcState::Idle, "reached home");

// RoomServer
setPos(spawnPos_);
body().snapToCurrent();   // double-buffered body state 동기화
setLinearVel(mu::Vec3{});
transitionTo(NpcState::Idle, "reached home");
```
스냅은 1회성 순간이동이므로 `setPos()` + `snapToCurrent()` 사용이 정당하다.

#### 결론

`facing_` Vec3 멤버를 Npc에 추가할 필요가 없다.
- 이동 방향 → `setLinearVel`의 x/z 성분으로 표현
- 현재 facing 읽기 → `forward()` (orient 기반으로 자동 갱신)
- 시각적 방향 → `setOrient`

---

**sim에서 이식 시 제외할 멤버/메서드** (Renderer/Logger 전용):
- `facing_` 멤버, `logPrefix_` 멤버
- `dump()` 메서드
- `getWindupProgress()`, `getRecoverProgress()`
- `getSpawnPos()`, `getActivityZoneCenter()`, `getActivityZoneRadius()`, `getSeparationRadius()`
- `Logger::get().log*()` 호출 전체

### Step 5. `RoomServer/object.hpp` 수정

```cpp
// 제거
enum class GoblinAIState { Patrol, Chase, Attack, Return };
struct GoblinUpdateResult { ... };

// Goblin 변경
class Goblin : public Npc {          // Object → Npc
public:
    Goblin() = default;
    Goblin(Object&& base) : Npc(std::move(base)) {}
    void applyGoblinConfig();        // NpcConfig 설정 + setHp

    // 유지 (lag-comp)
    void recordSnapshot(uint64 serverMs);
    mu::Vec3 rewindPos(uint64 targetMs) const;

private:
    // 제거: aiState_, patrolTarget_, aggroRange_, deaggroRange_, attackCooldown_
    // 유지: posHistory_
};
```

### Step 6. `RoomServer/object.cpp` 수정

- `Goblin::update()` 4-state FSM 전체 삭제
- `Goblin::applyGoblinConfig()` 추가:

```cpp
void Goblin::applyGoblinConfig() {
    NpcConfig cfg;
    cfg.maxHp         = 90.f;
    cfg.moveSpeed     = 3.f;
    cfg.attackRange   = 1.5f;
    cfg.detectionRange= 15.f;
    cfg.attackDamage  = 15.f;
    applyConfig(cfg);   // Npc::applyConfig()
    setHp(cfg.maxHp);
}
```

### Step 7. `RoomServer/Room.hpp` 수정

```cpp
// public 추가
const std::vector<GameSession*>& getLivingPlayers() const;
void MU_CALLCONV findNearbyNpcPositions(mu::Vec3 pos, float radius, uint32 excludeId,
                                         std::vector<mu::Vec3>& out) const;
int  countNpcsTargeting(uint32 playerId) const;
NpcGroup* getNpcGroup(int groupId);
Milliseconds getElapsedMs() const { return elapsedMs_; }
GameSession* findLivingSessionByPlayerId(uint32 playerId) const;

// private 추가
Milliseconds elapsedMs_{ 0ms };
std::vector<std::unique_ptr<NpcGroup>> npcGroups_{};
std::vector<GameSession*> livingPlayersCache_{};
std::unordered_map<uint32, int> aggroCount_{};
```

### Step 8. `RoomServer/Room.cpp` 수정

**`Room::init()` 수정:**
```cpp
for (auto& g : goblins_) {
    g.setId(IdPool::pop());
    g.setSpawnPos(g.pos());     // 이미 있음
    g.applyGoblinConfig();      // 추가 — setHp(90) 대체
    // ... 나머지 물리 초기화
}
```

**`Room::updateGoblinAI()` 재작성:**
```cpp
void Room::updateGoblinAI(Milliseconds dt) {
    if (sessions_.empty()) return;

    // 1. 경과 시간 누적 및 NpcGroup 메모리 만료 정리
    elapsedMs_ += dt;
    for (auto& grp : npcGroups_) grp->update(elapsedMs_);

    // 2. 캐시 재구성
    rebuildLivingPlayersCache();
    rebuildAggroCount();

    uint64 serverNow = /* 현재 ms */;
    std::vector<SNpcMoveInfo> moveInfos;

    // 3. 각 Goblin 업데이트
    for (auto& goblin : goblins_) {
        goblin.recordSnapshot(serverNow);
        auto result = goblin.update(dt, *this);   // NpcUpdateResult

        if (goblin.hp() > 0)
            moveInfos.push_back({...});

        if (result.hit) {
            broadcast(PacketManager::makeSNpcAttackPacket(goblin.getId()));
            broadcast(PacketManager::makeSHitPacket(result.hit->targetId, result.hit->newHp));
        }
    }

    if (!moveInfos.empty())
        broadcast(PacketManager::makeSNpcMoveBatchPacket(moveInfos));
}
```

**쿼리 구현:**
- `getLivingPlayers()`: 세션 순회, `session->player()->hp() > 0`인 것만 캐싱
- `findNearbyNpcPositions()`: goblins_ 순회, `(g.pos()-pos).len() < radius && g.getId() != excludeId`
- `countNpcsTargeting(id)`: `aggroCount_[id]` 조회
- `getNpcGroup(id)`: `npcGroups_[groupId]` → O(1). groupId == vector 인덱스로 설계 (sim과 동일)
- `rebuildAggroCount()`: 살아있는 goblin 중 `Chase/AttackWindup/AttackRecover/Reposition` 상태인 것만 `aggroCount_[g.getTargetId()]++`. Idle/Return/Investigate/Dead는 제외 — 포함하면 타깃 선택 점수 계산 오류
- `findLivingSessionByPlayerId(id)`: `idSessionMap_.find((int32)id)` → O(1). session->id() == playerId이므로 기존 맵 재사용. hp > 0 확인 후 반환

### Step 9. `RoomServer/Level.cpp` 수정

```cpp
// 변경 전
goblin.setHp(90);
goblin.setModel(assetManager.modelGoblin());

// 변경 후
goblin.setModel(assetManager.modelGoblin());
// applyGoblinConfig()는 Room::init()에서 호출하므로 여기선 제거
```

> `setHp(90)`는 Level::importGoblinSpawner 내에 있음. `applyGoblinConfig()`가 Room::init()에서 불리므로 Level에서 setHp를 제거하거나, applyGoblinConfig를 여기서 불러도 됨. 중복 호출되지 않게 한 곳에서만.

### Step 10. `.vcxproj` 수동 수정

`RoomServer\RoomServer.vcxproj`에 추가:
```xml
<ItemGroup>
  <ClCompile Include="Npc.cpp" />
  <ClCompile Include="NpcGroup.cpp" />
</ItemGroup>
<ItemGroup>
  <ClInclude Include="Npc.hpp" />
  <ClInclude Include="NpcGroup.hpp" />
</ItemGroup>
```

---

## 주의사항 체크리스트

- [ ] `mu::Vec3` 값 파라미터 메서드 전부 `MU_CALLCONV` 추가
- [ ] `Npc::getTargetId()` public 노출 필수 (`countNpcsTargeting`이 사용)
- [ ] `targetId_ = 0` → "타깃 없음" 센티넬. IdPool이 0을 발급하지 않는지 확인
- [ ] 시간 단위: `Seconds dt`를 float으로 쓸 때 `.count()` 호출
- [ ] `groupId_ = -1` → 독립 NPC. Level 포맷 변경 없음
- [ ] 신규 .cpp 2개 `.vcxproj`에 수동 추가

---

## 검증 순서

1. Visual Studio 빌드 — 오류 0개
2. RoomServer + client 실행
3. Goblin이 스폰되고 플레이어를 추격하는지 확인
4. 공격 Windup/Recover 타이밍 확인
5. 복수 Goblin이 동일 플레이어에 몰리지 않고 분산되는지 확인 (Reposition/Separation)
6. 활동 구역 밖으로 이동 후 Return하는지 확인

---

## 향후 최적화 메모

현재 구현은 의도적으로 단순한 O(N) 구현을 유지한다.
고블린 수가 충분히 많아질 때 아래 최적화를 별도 커밋으로 적용한다.

### 공간 분할 그리드 (`findNearbyNpcPositions` O(N²) → O(1))

현재 `findNearbyNpcPositions`는 `goblins_` 전체를 순회(O(N)).
N개 NPC가 매 틱 각각 호출하면 총 O(N²).
N ≤ 30 수준에서는 무시 가능하나, 그 이상이면 그리드 도입 검토.

구현 참고: `sim/Room.cpp` — `rebuildSpatialGrid()` / `findNearbyNpcPositions()`

핵심 아이디어:
- `GRID_CELL_SIZE = 6.f` (separationRadius=4 기준 쿼리 시 최대 3×3=9셀)
- 셀 키: `(cx + OFFSET) * RANGE + (cz + OFFSET)` → `int64_t` 하나로 인코딩
- 매 틱 `rebuildSpatialGrid()`로 NPC 위치를 그리드에 등록
- `findNearbyNpcPositions()`에서 반경에 걸치는 셀만 조회

추가 멤버 (`Room.hpp` private):
```cpp
static constexpr float GRID_CELL_SIZE = 6.f;
std::unordered_map<int64_t, std::vector<uint32>> spatialGrid_{};
static int64_t gridKey(int cx, int cz);
void rebuildSpatialGrid();
```

`updateGoblinAI()`에서 `rebuildLivingPlayersCache()` / `rebuildAggroCount()` 이후 `rebuildSpatialGrid()` 추가 호출.

---

## NPC AI 동작 설명

### 호출 구조

`Room::updateGoblinAI()` (매 틱, 60fps) → 각 `Goblin::update()` → `Npc::update()` → 현재 상태 핸들러

### 상태 전이 흐름

```
                    ┌─────────────────────────────────────────┐
                    │                                         ▼
  [Idle] ──플레이어 감지──► [Chase] ──사정거리 진입──► [AttackWindup]
    ▲                         │                              │
    │                         │ 타겟 소실/구역 이탈           │ 시간 경과
    │                   [Return] ◄──────────────────── [AttackRecover]
    │                      │                                  │
    └──── 스폰 도달 ────────┘               과밀 감지 ──► [Reposition]

  [Idle] ──그룹 메모리 있음──► [Investigate]
  [Dead] (hp ≤ 0, 종단 상태)
```

### 각 상태 세부 동작

**Idle** — 매 틱 `selectBestTarget()`으로 감지 범위(15m) 내 플레이어 탐색. 없으면 그룹 공유 메모리를 확인해서 유효한 기억이 있으면 `Investigate`로 전이.

**Chase** — 0.5초마다 타겟 재평가(더 좋은 타겟으로 교체 가능). 매 틱 분리력(separation force)을 계산해서 다른 고블린과 겹치지 않도록 방향을 보정하면서 추격. `setLinearVel` + `setOrient`로 물리 엔진에 속도를 넘기고 충돌은 물리 solver가 처리. 사정거리(1.5m) 내 진입 시 `AttackWindup`.

**AttackWindup** — 이동 없음, 0.4초 대기. 시간이 지나면 타겟이 아직 범위 내에 있는지 확인 후 맞으면 데미지(15) 적용 → `AttackRecover`. 빗나가도 `AttackRecover`로.

**AttackRecover** — 0.6초 회복. 분리력이 충분히 크면 살짝 밀려남. 회복 후 과밀이면 `Reposition`, 타겟이 가까우면 `AttackWindup`, 멀면 `Chase`.

**Reposition** — 타겟 방향에 수직인 방향(짝수 ID는 우측, 홀수는 좌측)으로 이동해서 과밀 해소. 1.5초 타임아웃 또는 과밀 해소 시 `Chase`/`AttackWindup`.

**Return** — 스폰 위치로 귀환(속도 2.5배). 도중에 활동 구역 안에서 플레이어가 보이면 즉시 `Chase`로 재전이(`canReAggroOnReturn = true`). 스폰 도달 시 `Idle`.

**Investigate** — 그룹 메모리의 마지막 목격 위치로 이동. 도착하거나 유효 메모리 소멸 시 `Return`. 도중에 실제 플레이어가 보이면 즉시 `Chase`.

**Dead** — hp ≤ 0이면 진입, 이후 아무 처리 없음. `Room`에서 hp 체크로 moveInfos에서 제외.

### NpcGroup 시야 공유

같은 `groupId`를 공유하는 고블린들은 한 마리가 플레이어를 발견했을 때 `reportSight()`로 그룹 메모리에 위치를 기록한다. 기억은 3초 후 만료. 아직 못 본 다른 고블린은 Idle 상태에서 이 메모리를 읽어 `Investigate`로 전이 — 직접 보지 않아도 반응하는 효과.

현재 `Room::init()`에서 `npcGroups_`를 생성하지 않으므로 모든 고블린의 `groupId_ = -1`이다. 그룹 기능을 활성화하려면 `init()`에서 `npcGroups_.emplace_back(...)` 후 각 고블린에 `setGroupId()`를 호출해야 한다.

---
### 최적화 해야 할 부분
### setOrient → rebuildBodyBVH 이중 호출

NPC가 이동할 때마다 매 틱 BVH 재빌드가 두 번 발생한다:

```
setOrient(yaw)       → rebuildBodyBVH()  // 구 position, 새 orientation
physicsWorld_.step() → rebuildBodyBVH()  // 새 position, 새 orientation (콜백)
```

첫 번째 rebuild는 position이 곧 바뀌므로 낭비다. N=300, 60fps 기준 틱당 600회 불필요한 재빌드.
수정 방향: `setOrient`에서 BVH rebuild를 즉시 호출하지 않고 physics step 이후 콜백에만 위임.
Object 수준 변경이 필요하므로 NPC 수가 충분히 늘어난 시점에 별도 커밋으로 적용.

### Dead NPC 조기 스킵

현재 `Room::updateGoblinAI()`에서 죽은 고블린도 루프를 통과한다:

```cpp
goblin.body().setOmega(mu::Vec3{});  // dead도 실행
goblin.recordSnapshot(serverNow);    // dead도 실행
goblin.update(Seconds{dt}, *this);   // updateDead() 즉시 return
```

`hp() <= 0`이면 세 줄 모두 건너뛰는 것으로 간단히 해결 가능. N이 작을 때는 무시 가능.

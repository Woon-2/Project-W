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
    uint32   playerId = 0;          // 0 = 빈 슬롯
    uint32   reporterNpcId = 0;
    mu::Vec3 lastKnownPosition{};
    uint32   lastSeenTick = 0;
    uint32   expireTick = 0;
    bool     valid = false;
};

class NpcGroup {
public:
    NpcGroup(int groupId, mu::Vec3 center, float radius,
             uint32 memoryDurationTick = 180);
    void addMember(uint32 npcId);
    void removeMember(uint32 npcId);
    void MU_CALLCONV reportSight(uint32 npcId, uint32 playerId,
                                  mu::Vec3 pos, uint32 currentTick);
    bool                      hasValidMemory(uint32 currentTick) const;
    const SharedTargetMemory* getBestMemory(uint32 currentTick) const;
    const SharedTargetMemory* getBestMemoryInsideActivityArea(uint32 currentTick) const;
    bool MU_CALLCONV isInsideActivityArea(mu::Vec3 pos) const;
    void clearMemory();
    void update(uint32 currentTick);
    int      getGroupId() const;
    mu::Vec3 getCenter()  const;
    float    getRadius()  const;
private:
    int      groupId_;
    mu::Vec3 activityCenter_;
    float    activityRadius_;
    uint32   memoryDurationTick_;
    std::vector<uint32> members_;
    std::array<SharedTargetMemory, 4> memories_{};
};
```

### Step 2. `RoomServer/NpcGroup.cpp` (신규)

원본 `sim/NpcGroup.cpp` 이식. Vec3 API만 치환.

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
| `p->getPosition()` | `session->player().pos()` |
| `target->takeDamage(dmg)` | `result.hit = {targetId, newHp}` |
| `room.getLivingPlayers()` → `Player*` | `GameSession*` 벡터 |
| `p->getHp()` | `session->player().hp()` |
| `float dt` | `Seconds dt` (`.count()` 필요 시) |

**피격 처리**: sim은 `takeDamage()` 직접 호출. RoomServer는 `NpcUpdateResult.hit`에 채워서 반환 → Room이 broadcast.

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
uint64 getTickCount() const { return tickCount_; }

// private 추가
uint64 tickCount_{ 0 };
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

    // 1. NpcGroup 메모리 만료 정리
    for (auto& grp : npcGroups_) grp->update(static_cast<uint32>(tickCount_));

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

    ++tickCount_;
}
```

**쿼리 구현:**
- `getLivingPlayers()`: 세션 순회, `session->player().hp() > 0`인 것만 캐싱
- `findNearbyNpcPositions()`: goblins_ 순회, `(g.pos()-pos).len() < radius && g.getId() != excludeId`
- `countNpcsTargeting(id)`: `aggroCount_[id]` 조회
- `getNpcGroup(id)`: npcGroups_ 순회, `grp->getGroupId() == id`
- `rebuildAggroCount()`: goblins_ 순회, `aggroCount_[g.getTargetId()]++`

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

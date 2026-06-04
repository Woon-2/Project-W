# Skill System Architecture Plan

## Context

현재 전투 시스템(`client/standalone/combatSystem.hpp`)은 단일 AABB 공격 판정만 지원하며, 스킬 개념이 없다. 다수의 스킬 인스턴스를 효율적으로 시뮬레이션하고, Lua로 스킬을 정의하며, 기존 BVH/애니메이션/이벤트 시스템과 자연스럽게 통합되는 타임라인 기반 스킬 시스템을 새로 설계한다.

---

## Architecture Overview

```
Lua skill files  ──[SkillCompiler, startup only]──▶  SkillAsset (immutable)
                                                            │
                                              SkillAssetRegistry (vector<SkillAsset>)
                                                            │
input (Q/E/R key) ──▶  SkillSystem::startSkill()  ──▶  SkillInstance (pool[128])
                                    │
                         SkillSystem::update(dt)
                         ├─ Pass 1: advance elapsedMs
                         ├─ Pass 2: fire timeline events → EventList
                         ├─ Pass 3: update AttachedHitbox world transforms (bone chain)
                         ├─ Pass 4: collision detection (collides(worldBVH, AABB))
                         └─ Pass 5: emit EvSkillHit into EventList
                                    │
                         existing event processing loop
                         └─ EvSkillHit handler → setHp, applyImpulse, VFX spawn
```

---

## Critical Files

### New Files
| File | Role |
|------|------|
| `client/skill/skillTypes.hpp` | `TimelineEvent`, `HitboxDef`, `OnHitDef`, `SkillAsset` |
| `client/skill/skillSystem.hpp` | `AttachedHitbox`, `SkillInstance`, `SkillInstancePool`, `SkillSystem` |
| `client/skill/skillSystem.cpp` | `SkillSystem` implementation |
| `client/skill/skillCompiler.hpp` | sol2-based Lua → SkillAsset compiler |
| `client/skill/skillCompiler.cpp` | Compiler implementation (sol2 isolated here) |
| `resources/skills/lua/skill_api.lua` | Lua-side API wrapper |
| `resources/skills/sword_slash.lua` | First skill definition (검격) |

### Modified Files
| File | Change |
|------|--------|
| `client/event.hpp` | `EvSkillHit`, `EvCameraShake`, `EvVFXSpawn` 추가 |
| `client/standalone/game.hpp` | `SkillSystem skillSystem_`, `SkillDispatchContext skillCtx_` 추가 |
| `client/standalone/game.cpp` | Q키 → startSkill, `skillSystem_.update()` 삽입, 이벤트 핸들러 |
| `client/client.vcxproj` | 새 소스/헤더 파일 등록 |

---

## Data Structures

### TimelineEvent — fixed size, cache-friendly

```cpp
enum class SkillEventType : u8t {
    SpawnHitbox, DestroyHitbox, PlayAnimation, PlayVFX,
    SendGameplayEvent, ModifyStat, SpawnProjectile, ApplyImpulse, CameraShake
};

struct TimelineEvent {
    Milliseconds      time;     // skill 시작으로부터의 경과 시간
    SkillEventType    type;
    u8t               pad[3];
    SkillEventPayload payload;  // fixed-size union (trivially copyable)
};
```

Timeline은 `time` 기준 오름차순 정렬. 이벤트 커서(`nextEventIdx`)가 매 프레임 `elapsed`와 비교하며 전진.

### Hitbox Attachment — 문자열 기반, bone/VFX node 모두 지원

```cpp
enum class HitboxAttachType : u8t {
    Bone,     // 시전자 스켈레톤의 named bone
    VFXNode,  // 스폰된 VFX 액터의 named anchor
};

struct HitboxAttachTarget {
    HitboxAttachType type       = HitboxAttachType::Bone;
    std::string      targetName;  // "RightHand" 또는 VFX anchor 이름
    u8t              vfxId       = 0;
};
```

**문자열 해결 전략**: `SpawnHitbox` 이벤트 디스패치 시 1회만 bone index로 변환해 `ResolvedAttach`에 캐싱. 이후 매 프레임은 캐시된 인덱스만 사용 (hot path에 문자열 탐색 없음).

### SkillAsset (로드 후 불변)

```cpp
struct SkillAsset {
    std::string name;
    u32t        id            = 0;
    bool        interruptible = true;
    Milliseconds totalDuration{ 0.f };

    std::vector<TimelineEvent>  timeline;    // time 오름차순 정렬
    std::vector<SkillHitboxDef> hitboxDefs;  // SpawnHitbox::defIdx로 참조
    std::vector<std::string>    vfxNames;    // vfxId로 인덱싱
};
```

`registerAssets()` 후 `shrink_to_fit()` → 포인터 안정성 보장.

### SkillInstance + Pool

```cpp
struct SkillInstance {
    const SkillAsset* asset         = nullptr;
    i32t              ownerObjectId = -1;
    Milliseconds      elapsed{ 0.f };
    i32t              nextEventIdx  = 0;
    bool              active        = false;
    bool              interrupted   = false;
    i32t hitboxHandles[kMaxHitboxSlots];  // AttachedHitbox 풀 인덱스, -1=비어있음
};

struct SkillInstancePool {
    static constexpr int kMaxInstances = 128;
    SkillInstance instances[kMaxInstances]{};  // 정적 배열, 힙 할당 없음
    int count = 0;
};
```

### AttachedHitbox

```cpp
struct AttachedHitbox {
    AABB           worldShape;     // 매 프레임 attach transform으로 재계산
    AABB           localShape;     // SkillHitboxDef의 원본 shape
    OnHitDef       onHit;
    ResolvedAttach resolvedAttach; // 캐시된 런타임 핸들
    i32t           ownerObjectId = -1;
    i32t           instanceIdx   = -1;
    u8t            slot          = 0;
    bool           active        = false;
};
// SkillSystem::hitboxPool_[64] — 정적 배열
```

---

## Timeline Execution

```cpp
void SkillSystem::tickInstance(SkillInstance& inst, Milliseconds dt, SkillDispatchContext& ctx) {
    inst.elapsed += dt;
    const auto& tl = inst.asset->timeline;
    while (inst.nextEventIdx < (int)tl.size()) {
        if (tl[inst.nextEventIdx].time > inst.elapsed) break;
        dispatchEvent(tl[inst.nextEventIdx], inst, ctx);
        ++inst.nextEventIdx;
    }
    if (inst.asset->totalDuration.count() > 0 && inst.elapsed >= inst.asset->totalDuration)
        terminateInstance(inst, ctx);
}
```

---

## Lua → C++ 컴파일 전략

### Lua 예시 (`sword_slash.lua`)

```lua
local skill = Skill()
skill.name            = "SwordSlash"
skill.totalDurationMs = 800
skill.interruptible   = true

skill:addVFX(0, "effects/blood_hit.json")
skill:addVFX(1, "effects/sword_slash_1.json")

skill:addEvent(0, "PlayAnimation", { clipName = "Player_Attack", blendTime = 0.1 })

skill:addEvent(100, "SpawnHitbox", {
    slot       = 0,
    localShape = { center = { 0.0, 0.0, 0.4 }, size = { 0.6, 0.8, 0.8 } },
    attach     = BoneAttach("RightHand"),
    onHit      = OnHit({ damage = 35, vfxId = 0, impulseStrength = 8.0,
                         impulseDir = Vec3(0.0, 0.3, 1.0) })
})
skill:addEvent(100, "PlayVFX",       { vfxId = 1, attach = BoneAttach("RightHand") })
skill:addEvent(400, "DestroyHitbox", { slot = 0 })
skill:addEvent(600, "CameraShake",   { magnitude = 0.3, durationMs = 150 })

return skill
```

### 컴파일 단계 (startup only)

```
1. SkillCompiler::compileAll(skillDir, pSkeleton)
   ├─ sol2 State 생성 (범위 종료 시 소멸)
   ├─ skill_api.lua 로드
   ├─ 각 .lua 파일 실행 → sol::table
   ├─ table → TimelineEvent 구조체 변환
   ├─ std::sort(timeline, by time.count())
   └─ vector<SkillAsset>에 추가, 순차 ID 부여

2. SkillSystem::registerAssets(std::move(assets))
   └─ shrink_to_fit() → 이후 포인터 안정
```

**격리**: `skillCompiler.hpp/.cpp`만 `<sol/sol.hpp>` include. `skillSystem.hpp`는 Lua 의존성 없음.

**Lua 비활성화 시**: `SKILL_SYSTEM_ENABLE_LUA` 미정의 → `compileAll()` 빈 벡터 반환. `buildSwordSlashAsset()`(game.cpp 내 static 함수)으로 하드코딩된 에셋 사용.

---

## 이벤트 시스템 통합

### 새 EventType (`event.hpp`)

```cpp
enum class EventType : u32t {
    Hit, Blood, Death, Attack,  // 기존
    SkillHit,    // 스킬 명중: targetId + damage (12B, gPool16)
    CameraShake, // 카메라 흔들림: magnitude + duration (12B, gPool16)
    VFXSpawn,    // VFX 스폰 (12B, gPool16)
    SIZE
};

struct EvSkillHit : BasicEvent {
    i32t targetId{-1};
    i32t damage{0};
};
struct EvCameraShake : BasicEvent {
    float        magnitude{0.f};
    Milliseconds duration{0.f};
};
struct EvVFXSpawn : BasicEvent {
    u8t  vfxId{0xFF};
    u8t  pad[3]{};
    i32t attachObjectId{-1};
};
```

모두 ≤16B → 기존 `gPool16` 재사용. `clearEvents` 매크로 수정 불필요.

---

## Hitbox World Transform 계산

```
bone transform chain: bone.toDress * finalXformData()[boneIdx] * world
```

- SpawnHitbox dispatch 시 1회 resolve (bone name → index)
- 매 프레임 `computeAttachTransform()` 호출 (캐시된 index 사용, 문자열 탐색 없음)
- 히트박스 transform은 **직전 프레임** AnimSystem 결과 사용 (1프레임 딜레이, 60fps에서 16ms — 게임플레이 허용 범위)

---

## 게임 루프 통합 (`standalone/game.cpp`)

```
update(Milliseconds deltaTime):
  0. skillCtx_.evList = &eventList_   ← 매 프레임 갱신
     skillCtx_.pTimer = pTimer_

  1. processInput()
     ├─ [기존] LButton → combatSystem_.onPlayerAttack()
     └─ [신규] Q키 → skillSystem_.startSkill("SwordSlash", playerId, skillCtx_)

  2. [기존] combatSystem_.update()

  3. [신규] skillSystem_.update(dt, skillCtx_)
     ├─ Pass 1: elapsed 전진
     ├─ Pass 2: timeline 이벤트 발생 (PlayAnimation → holdEvent(EvAttack) 등)
     ├─ Pass 3: AttachedHitbox world transform 재계산
     ├─ Pass 4: collides(target->worldBVH(), hitbox.worldShape)
     └─ Pass 5: EvSkillHit 발생

  4. [기존] 이벤트 처리 루프
     ├─ [신규] EvSkillHit  → EvHit으로 변환 (기존 HP/애니메이션 로직 재사용)
     ├─ [신규] EvCameraShake → (Camera shake 미구현, 무시)
     └─ [신규] EvVFXSpawn  → processHitResults에서 이미 처리, 추가 동작 없음

  5. PhysicsWorld::step()
  6. Object::update()
  7. AnimSystem::update()
  8. render()
```

### SkillDispatchContext 멤버

| 필드 | 타입 | 설명 |
|------|------|------|
| `evList` | `EventList*` | 매 프레임 갱신 |
| `objectById` | `Object**` | ID 인덱싱 희소 배열 (player=0, goblin=1) |
| `objectByIdSize` | `int` | 배열 크기 |
| `vfxById` | `ParticleEffect**` | vfxId 인덱싱 (0=blood_hit, 1=sword_slash_1) |
| `vfxByIdSize` | `int` | 배열 크기 |
| `camera` | `Camera*` | CameraShake 이벤트용 (미래 사용) |
| `pTimer` | `Timer*` | 타이머 참조 |

---

## 메모리 관리

| 리소스 | 전략 |
|--------|------|
| `SkillAsset` | `vector<SkillAsset>` + `shrink_to_fit()` — startup 후 할당 없음 |
| `SkillInstance` | `instances[128]` 정적 배열 — 힙 할당 없음 |
| `AttachedHitbox` | `hitboxPool_[64]` 정적 배열 — 힙 할당 없음 |
| Hit results | `pendingHits_[128]` — 매 프레임 초기화 |
| 이벤트 | 기존 `gPool16` via `holdEvent` 매크로 |

---

## 멀티플레이어 호환성 (Phase 5)

```cpp
struct CSkillStartPacket : PacketHeader {
    u32t  skillAssetId;
    i32t  ownerId;
    u64t  clientTimestampMs;
};

struct SSkillStartPacket : PacketHeader {
    u32t  skillAssetId;
    i32t  ownerId;
    float elapsedMs;  // lag compensation용
};
```

직렬화 상태: `{skillAssetId, ownerObjectId, elapsed}`. `nextEventIdx`는 `elapsed`로부터 결정론적 재계산.

**클라이언트 예측**: 키 입력 시 로컬에서 즉시 시작 → `CSkillStartPacket` 전송 → `SSkillStartPacket` 수신 시 `elapsedMs` 보정.

---

## 구현 단계

| Phase | 내용 | 상태 |
|-------|------|------|
| **Phase 1** | 핵심 데이터 구조 + 컴파일러 | ✅ 완료 |
| **Phase 2** | SkillSystem 런타임 + 게임 루프 연동 | ✅ 완료 |
| **Phase 3** | Hitbox 시스템 (VFXParticle 동적 hitbox) | ✅ 완료 |
| **Phase 4** | 나머지 이벤트 타입 완성 | ✅ 완료 |
| **Phase 5** | 멀티플레이어 통합 | 🔲 미착수 |
| **Phase 6** | 컨텐츠 추가 + 최적화 | 🔲 미착수 |

### Phase 1 완료 목록
- `client/skill/skillTypes.hpp` 생성
- `client/skill/skillSystem.hpp/.cpp` 생성
- `client/skill/skillCompiler.hpp/.cpp` 생성 (sol2 격리)
- `resources/skills/lua/skill_api.lua` 생성
- `resources/skills/sword_slash.lua` 생성
- `client/event.hpp` 수정 (EvSkillHit, EvCameraShake, EvVFXSpawn)
- `client/client.vcxproj` 수정

### Phase 2 완료 목록
- `client/standalone/game.hpp` — SkillSystem, SkillDispatchContext, objectById/vfxById 멤버 추가
- `client/standalone/game.cpp` — Q키 핸들러, skillSystem_.update() 삽입, EvSkillHit/EvCameraShake 핸들러

### Phase 3 완료 목록
- **VFXParticle 동적 hitbox 시스템**: `ParticleHitboxSource` 구조체 도입 — SpawnHitbox(VFXParticle) 시 소스 생성, `updateParticleHitboxSources()`가 매 프레임 파티클 수에 맞춰 hitbox 재생성
- **one-hit-per-target**: `hitTargets` unordered_set으로 같은 대상 중복 피격 방지 (프레임 간 유지)
- **useParticleSize 플래그**: `SkillHitboxDef`와 `ParticleHitboxSource`에 추가 — 파티클의 현재 sizeBegin→sizeEnd 보간값으로 OBB halfExtents 자동 스케일링
- **std::vector 풀 전환**: `hitboxPool_`, `particleSources_`, `pendingHits_` 모두 `std::vector` 기반으로 전환 (kMaxHitboxSlots 제한 제거)
- **SkillInstance 슬롯 구조 재설계**: 고정 배열 `hitboxHandles[8]` → `boneHitboxBySlot` + `particleSourceBySlot` 두 벡터로 분리
- **center offset 버그 수정**: 파티클 hitbox center = `parts[pi].pos + src.templateOBB.center` (이전엔 local offset 무시)
- **BV 디버그 렌더링**: `renderDebugHitboxes(DebugBVView&)` + H키 토글 (`skillDebugBV_` 플래그)
- **VFX 방향 수정**: `PlayVFX` 디스패치 시 bone-to-world에서 `worldOrient` 추출 (이전엔 identity 고정)
- **sword_slash.lua 업데이트**: bone 부착 → VFXParticle 부착 (VFX system 0 = root slash mesh 파티클)

### Phase 4 완료 목록
모든 핵심 이벤트 타입 구현 (`skillSystem.cpp::dispatchEvent()`):
- `SpawnHitbox` / `DestroyHitbox` ✅
- `PlayAnimation` → `holdEvent(*ctx.evList, EvAttack)` (EventBus 일원화) ✅
- `PlayVFX` → `ParticleEffect::play()` (bone attach + 방향 포함) ✅
- `ApplyImpulse` → `body.applyImpulse()` ✅
- `CameraShake` → `EvCameraShake` 이벤트 발생 ✅
- `ModifyStat` → `object->setHp()` ✅
- `SendGameplayEvent`, `SpawnProjectile` — 미래 확장용 stub

---

## 검증 체크리스트 (End-to-End)

- [ ] 빌드 성공 (runtime header에 sol2 없음)
- [ ] Q키 → 애니메이션 재생 → DebugBVView에 hitbox 표시
- [ ] 고블린에 접근 후 Q키 → `EvSkillHit` 발생 → HP 감소 → 피격 애니메이션
- [ ] 고블린 HP 0 → `EvDeath` → 래그돌 활성화
- [ ] t=600ms에 CameraShake 이벤트 발생
- [ ] 스킬 도중 Q키 재입력 → interrupt (`interruptible=true` 확인)
- [ ] 128개 동시 스킬 인스턴스 60fps 유지

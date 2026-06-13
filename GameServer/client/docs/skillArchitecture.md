# 스킬 시스템 아키텍처 (클라이언트)

## 개요

스킬 시스템은 타임라인 기반 이벤트 방식으로 동작한다. 스킬 정의는 Lua 스크립트로 작성하고 `SkillCompiler`가 C++ 데이터(`SkillAsset`)로 컴파일한다. 런타임에는 컴파일된 에셋만 참조하며 Lua 의존성이 없다.

> **스킬 작성 가이드(Lua API + 유형별 레시피)는 `skillCreationGuide.md` 참조.** 이 문서는 시스템 구조를, 가이드는 작성법을 다룬다.

**공유 파일:**
- `client/skill/skillTypes.hpp` — 불변 에셋 구조체 (SkillAsset, SkillHitboxDef, OnHitDef, TimelineEvent)
- `client/skill/skillCompiler.hpp/.cpp` — Lua → SkillAsset 변환
- `resources/skills/*.lua` — 스킬 정의 소스

**클라이언트 전용 파일:**
- `client/skill/skillSystem.hpp/.cpp` — 런타임 스킬 실행 엔진

---

## 핵심 데이터 구조

```
SkillAsset
  name, id           — 식별자
  timeline[]         — TimelineEvent (시간 오름차순 정렬)
  hitboxDefs[]       — SkillHitboxDef (SpawnHitbox 이벤트가 인덱스로 참조)
  vfxNames[]         — VFX 에셋 경로

SkillHitboxDef
  localOBBs[]        — 부착 좌표계 기준 OBB 목록
  attach             — AttachTarget (Bone 이름 또는 VFXParticle 인덱스)
  onHit              — OnHitDef (damage, impulse, hitVfxId)
  slot, hitGroup     — 히트박스 식별 및 중복 제거 키
  hitGroupCooldownMs — 0 = 한 번만 피격; >0 = N ms 후 재피격 허용

TimelineEvent
  time      — 발동 시간 (ms)
  type      — SpawnHitbox / DestroyHitbox / PlayAnimation / PlayVFX /
              ApplyImpulse / CameraShake / ModifyStat / ...
  payload   — 고정 크기 union (힙 할당 없음, 56 bytes)

PlayVFX payload — 시전자 기준 이펙트 배치/방향 제어 (형상 크기는 이펙트 .json 소관)
  vfxId               — ParticleEffect 레지스트리 인덱스
  localOffset         — attach-local 배치 (right, up, forward); forward.z = "전방 N미터"
  localEulerDeg       — attach-local 방향 오프셋 (yaw, pitch, roll deg, OBB와 동일 규약)
                        → 부채꼴 등을 시전자 기준 특정 방향으로 조준
  advanceForwardLocal — attach-local 파티클 진행 방향 (zero = orient에서 유도, 4-인자 play)
  flags               — bit0 kPlayVFXFlagYawOnly: 지면 평면 배치(pitch/roll 무시)
  lua 키: vfxId / offset / orient={yaw,pitch,roll} / advance=Vec3 / groundLock=bool / attach
  디스패치: aim = rotateRPYH(localEuler) * baseRot(yawOnly면 yaw만) → play(pos, orient[, advance])
```

---

## 런타임 구조

```
SkillSystem
  assetRegistry_[]   — registerAssets()로 등록, 이후 불변
  instancePool_      — SkillInstance 동적 풀 (vector + freeList + activeList)
  hitboxPool_[]      — AttachedHitbox (freeList로 O(1) 재사용)
  particleSources_[] — ParticleHitboxSource (VFXParticle attach 전용, freeList)
  skillBroadPhase_   — Object–Hitbox bipartite sweep-and-prune (후보 쌍 생성)
  pendingHits_[]     — checkHitboxCollisions → processHitResults 전달용

SkillInstance
  asset, ownerObjectId, elapsed, nextEventIdx
  boneHitboxBySlot[] — 슬롯 → hitboxPool_ 인덱스
  hitGroups{}        — hitGroup → {targetId → lastHitTime}

AttachedHitbox
  worldOBBs[]        — 매 프레임 갱신 (뼈 변환 적용)
  localOBBs[]        — 원본 shape (부착 좌표계)
  resolvedAttach     — SpawnHitbox 발동 시 한 번 해결 (boneIdx 또는 pSystem)
  onHit, ownerObjectId, hitGroup, ...
```

---

## 업데이트 루프 (매 프레임 순서)

```
SkillSystem::update(dt, ctx)
  1. tickInstance()           — elapsed 증가, 타임라인 이벤트 발동
     └─ dispatchEvent()       — SpawnHitbox → allocHitbox + resolveAttach
                              — DestroyHitbox, PlayAnimation, PlayVFX, ...
  2. updateHitboxes()         — Bone attach: boneWorldXform * localOBBs → worldOBBs (+worldAABB 캐시)
  3. updateParticleHitboxSources() — VFXParticle: 핸들 재사용으로 파티클 수만큼 증감
  4. checkHitboxCollisions()  — SkillBroadPhase(후보쌍) → worldOBBs vs target BVH → pendingHits_
  5. processHitResults()      — EvSkillHit 발행, VFX 재생, 충격량 적용
```

---

## Standalone 모드

플레이어 스킬 시작: Q 키 누름 → `skillSystem_.startSkill("SwordSlash", playerId, ctx)`

**`SkillDispatchContext` 구성:**
```cpp
skillCtx_.clientPredictionOnly = false;  // 데미지 권위 = 클라이언트
skillCtx_.evList     = &eventList_;
skillCtx_.objectById = skillObjectById_.data();
skillCtx_.vfxById    = skillVfxById_.data();
skillCtx_.camera     = &camera_;
```

`clientPredictionOnly = false`이므로 `processHitResults()`에서:
- `EvSkillHit` 즉시 발행 → 직접 데미지 적용
- VFX 재생
- 충격량 적용

**권위 소재:** 클라이언트가 데미지·히트 판정·물리 모두 처리. 서버 없음.

---

## Online 모드

> 멀티플레이 스킬 동기화 전반(키바인딩, 결정론 파티클 히트박스, 디스패치 컨텍스트 수명)은
> 본 절과 함께 `particleHitboxDeterminism.md`(VFXParticle 결정론 계약)를 함께 본다.

### 로컬 플레이어 스킬

1. 스킬 키 → `castSkillByName(name)`:
   - `findAsset(name)` + `hasActiveSkill(playerId)` 게이트 (한 번에 1스킬)
   - **per-cast 시드 생성**: `skillSeed = std::random_device{}()`
   - `skillSystem_.startSkill(assetId, playerId, ctx, skillSeed)` 로컬 실행
   - `sendSkillStartPacket(assetId, skillSeed)` → `C_SkillStart { skillAssetId, clientMs, skillSeed }`

**`SkillDispatchContext` 구성 (Online):**
```cpp
skillCtx_.clientPredictionOnly = true;   // 데미지 권위 = 서버
skillCtx_.evList     = &eventList_;
skillCtx_.objectById = skillObjectById_.data();
skillCtx_.vfxById    = skillVfxById_.data();
skillCtx_.camera     = &camera_;
skillCtx_.ground     = &groundSampler_;  // 지면 연계 프리미티브
```

`clientPredictionOnly = true`이므로 `processHitResults()`에서:
- **`EvSkillHit` 발행 안 함** → 로컬 데미지 없음 (서버 결과 대기)
- VFX 재생 ← 클라이언트가 직접 처리 (반응성 유지)
- 충격량 적용 ← 클라이언트가 직접 처리 (반응성 유지)

**시드를 캐스터가 생성하는 이유:** 서버는 `broadcastExcept`로 캐스터에게 `S_SkillStart`를
되돌려주지 않으므로 캐스터는 즉시 로컬 재생한다. 서버 생성 시드는 RTT 후에야 도달해 첫
VFX(≈150ms)에 못 맞는다. 캐스터가 시드를 만들어 패킷에 실으면 서버·원격이 같은 시드로
동일 파티클 레이아웃을 재현한다. 상세 근거·정밀도 한계: `particleHitboxDeterminism.md` §4·§5.

### `refreshSkillCtx()` — APC 재동기화 (중요)

`skillObjectById_`는 원격 플레이어 입퇴장 시 `resize`될 수 있다. 패킷 핸들러는 프레임 시작
`SleepEx(1, true)`의 alertable APC에서 메인 스레드로 실행되는데, 이 시점 `skillCtx_`의
포인터는 직전 프레임 값이라 컨테이너 리사이즈로 **dangling**일 수 있다.

`refreshSkillCtx()`가 `evList/pTimer/objectById/objectByIdSize`를 재바인딩한다. 호출 지점:
- 매 프레임 `InGameScene` 진입 시 1회
- **스킬 시스템에 진입하는 모든 패킷 핸들러 직전**(`onSkillStart`, `removePlayer`/`interruptAll` 등)

> 누락 시 APC 배치 내 앞선 패킷이 리사이즈를 유발하면 stale `data()` 역참조 → 크래시.

### 원격 플레이어 스킬 수신 (`S_SkillStart`)

```cpp
void Game::onSkillStart(uint16 ownerId, uint32 skillAssetId, uint16 elapsedMs, uint32 skillSeed) {
    holdEvent(eventList_, EvAttack(ownerId));   // 원격 공격 애니메이션 (EventBus 일원화)
    refreshSkillCtx();                          // APC 재동기화 (위 참조)
    // 비주얼 전용 스킬 시작 (clientPredictionOnly=true). skillSeed로 캐스터와 동일 파티클 재현
    skillSystem_.startSkill(skillAssetId, ownerId, ctx, Milliseconds{elapsedMs}, skillSeed);
}
```

- `elapsedMs`: 서버 기준 경과 시간 보상(lag compensation) → 원격 히트박스 타이밍 정합.
- `skillSeed`: 캐스터 생성·서버 중계 시드 → VFXParticle 결정론 히트박스가 캐스터 비주얼과 일치.

### 피격 결과 수신 (`S_SkillHit`)

```cpp
void Game::onSkillHit(uint16 attackerId, uint16 targetId, int32 newHp,
                      uint32 skillAssetId, XMFLOAT3 targetVelocity) {
    if (newHp <= 0)  // 킬 시 래그돌 초기 속도 저장 (applyHit 이전)
        goblin->setRagdollInitVelocity(targetVelocity);
    applyHit(targetId, newHp, attackerId);   // 서버 권위 HP/이벤트
    // 피격 VFX 재생 (asset->hitboxDefs[0].onHit.hitVfxId 참조, 타깃 위치에 play)
}
```

- HP 변경은 서버 값을 그대로 반영한다(로컬이 VFX·충격량을 미리 적용했어도 서버 HP가 최종값).
- `applyHit`은 `newHp>0`이면 `EvHit`, `newHp<=0`이면 `EvDeath`를 post → EventBus 핸들러가
  HP·isDead·래그돌·피격/사망 애니메이션 소유. 인라인은 거점·HP바 가시성만.
- `targetVelocity`: 서버가 impulse 적용 **직후** 읽은 타깃 선속도 → 항상 킬링 블로우 포함.
  래그돌 활성화 시 모든 뼈 초기 속도로 사용.

---

## Standalone vs Online 차이점 요약

| 항목 | Standalone | Online |
|------|-----------|--------|
| `clientPredictionOnly` | `false` | `true` |
| 데미지 권위 | 클라이언트 | **서버** |
| 히트 판정 권위 | 클라이언트 | **서버** |
| VFX 재생 | 로컬 스킬 시스템 | 로컬 스킬 시스템 (clientPrediction) |
| 충격량 적용 | 로컬 스킬 시스템 | 로컬 스킬 시스템 (clientPrediction) |
| 원격 플레이어 스킬 표시 | 해당 없음 | `S_SkillStart` 수신 후 clientPrediction 실행 |
| HP 갱신 | `EvSkillHit` 즉시 | `S_SkillHit` 수신 시 (EvHit/EvDeath) |
| per-cast 시드 | 미사용 | **캐스터 생성 → 패킷 중계 → 결정론 파티클 히트박스** |
| 네트워크 패킷 | 없음 | `C_SkillStart` 송신 / `S_SkillStart`, `S_SkillHit` 수신 |

---

## 패킷 구조 (`ServerEngine/protocol.hpp`)

```cpp
CSkillStartPacket { uint32 skillAssetId; uint64 clientMs; uint32 skillSeed; }
SSkillStartPacket { uint32 skillAssetId; uint16 ownerId; uint16 elapsedMs; uint32 skillSeed; }
SSkillHitPacket   { uint16 attackerId; uint16 targetId; int32 newHp;
                    uint32 skillAssetId; XMFLOAT3 targetVelocity; }
```

## 패킷 흐름 (Online)

```
[로컬 클라이언트]                 [서버]                  [다른 클라이언트]
 스킬 키
 ├─ seed = random_device()
 ├─ startSkill(local, ctx, seed)  ─────────────────────────────────────────
 └─ C_SkillStart{assetId, clientMs, seed} ──→ Room::skillStart()
                                          ├─ elapsedMs = now - clientMs
                                          ├─ startSkill(assetId, ownerId,
                                          │    ctx, elapsedMs, seed)  ← 서버 실행
                                          └─ broadcastExcept(sender,
                                               S_SkillStart{assetId, ownerId,
                                                 elapsedMs, seed})  ───────→ onSkillStart()
                                                                              startSkill(pred, seed)

 (매 서버 프레임)
                                        updateSkillSystem()
                                          checkHitboxCollisions()  ← seed로 결정론 파티클 히트박스
                                          EvSkillHit 발행
                                          newHp = tgt->hp() - damage
                                          velocity = tgt->onHitImpulse(...) 직후 선속도
                                          broadcast(S_SkillHit{attackerId, targetId,
                                            newHp, assetId, targetVelocity})
 onSkillHit(...)                      ←─────────────────────────── onSkillHit(...)
 applyHit + 래그돌속도                                              applyHit + 래그돌속도
```

---

## 뼈 부착 변환 체인

히트박스를 뼈에 부착할 때 사용하는 변환 체인 (Standalone 및 Online 공통):

```
bone.toDress                         — bind pose: bone-local → model space
  * animBlender->finalXformData()[i]  — 현재 포즈: model space 변환
  * renderState().world               — model → world
= boneWorldXform
```

`localOBBs`의 center를 `boneWorldXform`으로 변환해 `worldOBBs`를 구성한다.

---

## 히트 그룹과 중복 제거

같은 스킬 인스턴스에서 동일 대상을 여러 번 피격하지 않도록 `hitGroup` 키로 쿨타임을 관리한다.

- `hitGroupCooldownMs == 0`: 인스턴스 전체에서 해당 대상 한 번만 피격
- `hitGroupCooldownMs > 0`: 마지막 피격 이후 N ms가 지나면 재피격 가능

같은 `hitGroup` 번호를 가진 히트박스들은 서로 쿨타임을 공유한다.

내부 dedup은 타깃 多 AoE에서도 O(1) 조회를 위해 `unordered_map`을 유지한다. 인스턴스 풀이
파괴 없이 재사용되므로 `resetSlots()`는 맵을 `clear()`만 하여 버킷 capacity를 보존한다(할당 churn 제거).

---

## 성능 최적화 (2026-05-29)

서버와 동일한 구조 변경을 클라이언트에도 적용했다(`RoomServer/docs/skillArchitecture.md` 성능
최적화 항목과 동일). 핵심:

### Object–Hitbox broad phase (`SkillBroadPhase`)

`checkHitboxCollisions()`가 히트박스마다 전체 객체를 순회하던 O(N·M)을 제거. 히트박스 AABB와
타깃 AABB의 X축 엔드포인트를 한 번에 정렬·sweep하는 **bipartite sweep-and-prune**으로
(히트박스, 타깃) 후보 쌍만 생성(O(K log K + pairs)). 물리 broad phase와 독립이며, 각 히트박스의
합집합 AABB는 `AttachedHitbox::worldAABB`에 캐시(margin `kHitboxAABBMargin` fatten).
타깃 수집 필터는 기존과 동일하게 `!isDead()`.

### 동적 인스턴스 풀 + freeList

`SkillInstancePool`을 고정 `instances[128]` → `vector + freeList + activeList`로 교체.
`update()/hasActiveSkill()/interruptAll()`은 activeList만 순회. `hitboxPool_`/`particleSources_`도
freeList로 O(1) alloc.

### updateParticleHitboxSources 핸들 재사용

매 프레임 per-particle 히트박스를 free/realloc하던 것을, 파티클 수 변화에만 증감하고 기존
핸들의 `worldOBBs`/`worldAABB`만 갱신하도록 변경(매 프레임 alloc/free 제거).

---

## 피아 식별 (Faction, 2026-05-29)

아군(같은 진영)끼리는 타격할 수 없다. **서버와 동일 규칙**으로 적용해 클라 예측이 서버 권위와
일치한다(`RoomServer/docs/skillArchitecture.md`「피아 식별」과 동일 설계).

- `object.hpp`에 `enum class Faction { Neutral, Players, Monsters }` + `factionBit`/`hostileMask`
  + `Object::faction_`. `SkillBroadPhase`는 게임 의미를 모른 채 일반 `u32 마스크`만 다룬다.
- **진영 설정 지점**: online은 `setupPlayer`(`player_`)·`createOtherPlayer` → `Players`,
  `createGoblin` → `Monsters`. standalone은 `player_`/`goblin_`. 모두 `skillObjectById_` 등록부와
  같은 자리에서 `setFaction` 호출.
- **targetMask 캐시 + broad phase 필터**: `SpawnHitbox` 시 owner faction의 `hostileMask`를
  `AttachedHitbox::targetMask`/`ParticleHitboxSource::targetMask`에 캐시(`updateParticleHitboxSources`
  가 per-particle 히트박스에 전파). `SkillBroadPhase::build()`가 후보 emit 직전 `(mask & category)`를
  YZ overlap보다 먼저 검사 → 아군 쌍은 narrow phase 진입 전에 제거.
- 클라 예측(`clientPredictionOnly`)에서도 아군에게 히트 FX/impulse 예측이 발생하지 않는다.
- 타깃 수집 필터는 `!isDead()` 유지, narrow phase의 owner 제외 유지.

> **새 캐릭터/몬스터 추가 시:** 생성 지점에서 `setFaction` 호출 필수(누락 시 `Neutral` → 피격 불가
> 회귀). `skillObjectById_`에 등록되지 않는 객체는 애초에 스킬 타깃이 아니다.

---

## 객체 수명주기와 연결 해제 (중요 제약)

SkillSystem은 `SkillDispatchContext::objectById`(= `skillObjectById_`, id로 색인되는 raw `Object*`
배열)를 통해 owner/target을 매 프레임 해소한다. `AttachedHitbox`는 id만 저장하므로, 객체가
파괴되기 전에 슬롯을 비우지 않으면 `checkHitboxCollisions()`가 dangling 포인터를 역참조한다.

- **`Game::removePlayer()`** — 원격 플레이어 퇴장 시 `otherPlayers_`/`idPlayerMap_`에서 제거하기
  전에 ① `skillSystem_.interruptAll(playerId, skillCtx_)`로 그 플레이어 소유 스킬을 종료하고
  ② `skillObjectById_[playerId] = nullptr`로 슬롯을 비운다. (이 정리 누락이 2026-05-29
  `checkHitboxCollisions` 크래시의 원인이었다.)
- **소유자 제거는 허용** — `updateHitboxes()`가 owner null이면 히트박스를 비활성화한다.
- **재접속/씬 재진입** — 씬 셋업에서 `skillObjectById_.assign(256, nullptr)`로 전체 초기화하며,
  신규 플레이어 추가 경로(`onEnterOther` 등)가 해당 id 슬롯을 갱신한다.

> 서버 측 동일 제약은 `RoomServer/docs/skillArchitecture.md`의 「객체 수명주기와 연결 해제」 참조.

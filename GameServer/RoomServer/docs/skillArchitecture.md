# 스킬 시스템 아키텍처 (서버)

## 개요

서버 스킬 시스템은 클라이언트의 스킬 시스템과 구조가 동일하되, 렌더링·VFX·파티클 관련 요소가 모두 제거된 권위 서버(authoritative server) 버전이다. 히트 판정과 데미지 계산은 오직 서버에서만 수행되며, 결과를 클라이언트에 브로드캐스트한다.

**공유 파일 (클라이언트와 미러):**
- `client/skill/skillTypes.hpp` ↔ `RoomServer/skill/skillTypes.hpp` — SkillAsset, SkillHitboxDef, OnHitDef, TimelineEvent (서버는 별도 사본을 두고 구조를 동일하게 유지)
- `resources/skills/*.lua` — 스킬 정의 소스

> **PlayVFX 페이로드 미러 주의:** 클라이언트가 `PlayVFX`에 배치·방향 인자(`localEulerDeg`/`advanceForwardLocal`/`flags` yawOnly)를 추가하면(payload 32→56 bytes), 서버 `skillTypes.hpp`도 동일하게 미러해 union 크기·레이아웃을 맞춘다. 단 서버는 PlayVFX를 컴파일·디스패치하지 않으므로(no-op) 이 필드들은 **레이아웃 일치 목적의 미사용 필드**다.

**서버 전용 파일:**
- `RoomServer/skill/skillSystem.hpp/.cpp` — 서버 런타임 스킬 실행 엔진
- `RoomServer/skill/skillCompiler.hpp/.cpp` — 서버용 Lua 컴파일러 (ServerSkillCompiler)

---

## 클라이언트 스킬 시스템과의 차이

| 항목 | 클라이언트 | 서버 |
|------|-----------|------|
| `SkillDispatchContext::clientPredictionOnly` | 모드별 전환 | 항상 `false` (서버가 권위) |
| `ResolvedAttach::pSystem` | `ParticleSystem*` 사용 | 항상 `nullptr` |
| `SkillDispatchContext::vfxById / camera / pTimer` | 사용 | 없음 |
| `processHitResults()` VFX 재생 | 재생 | no-op |
| `PlayVFX` 이벤트 (컴파일/디스패치) | 파싱·재생 | **컴파일·디스패치 모두 no-op** |
| `checkHitboxCollisions()` 대상 필터 | `isDead()` | `canReceiveDamage() && hp() > 0` |
| `HitResult::damageCoeff` | 없음 | BVH 리프 노드에서 읽음 |
| 데미지 적용 | `EvSkillHit` → 로컬 | `EvSkillHit` → HP 갱신 + `S_SkillHit` 브로드캐스트 |
| 충격량 적용 | 클라이언트 물리 | 서버 물리 + `Npc::onHitImpulse()` 호출 |
| 히트박스 변환 | `renderState().world` | `owner->orient()` (렌더 상태 없음) |
| 뼈 변환 | `animBlender->finalXformData()` | `animController_` (서버 AnimController) |

---

## 권위와 소재

| 기능 | 권위 | 처리 위치 |
|------|------|-----------|
| 스킬 시작 트리거 | 클라이언트 (입력) | 클라이언트가 `C_SkillStart` 전송 |
| 스킬 실행 타이밍 동기화 | 서버 | `elapsedMs` 보상으로 서버가 시작 |
| 히트 판정 | **서버** | `SkillSystem::checkHitboxCollisions()` |
| 데미지 계산 | **서버** | `damageCoeff × onHit.damage` |
| HP 반영 | **서버** | `Room::update()` 이벤트 처리 |
| 결과 전파 | 서버 | `S_SkillHit` 브로드캐스트 |
| VFX 재생 | 클라이언트 | `S_SkillHit` 수신 후 클라이언트가 결정 |
| 충격량 (물리) | 서버 | `applyImpulse()` + `onHitImpulse()` |
| NPC 움직임 | **서버** | 물리 시뮬레이션 결과를 `S_NpcMove`로 전파 |

---

## 업데이트 루프 내 위치

```
Room::update() (60 Hz 고정)
  1. physicsWorld_.step()
  2. updateGoblinAI(dt)
  3. updatePlayerAnimations(dt)
  4. updateSkillSystem(dt)           ← 여기서 스킬 시스템 실행
     └─ skillSystem_.update(dt, ctx)
          tickInstance + dispatchEvent  ← activeList만 순회 (고정 128 배열 아님)
          updateHitboxes              ← 뼈 변환으로 worldOBBs + worldAABB 갱신
          checkHitboxCollisions       ← SkillBroadPhase(후보쌍) → BVHHitResult
          processHitResults           ← EvSkillHit 생성 + 충격량 적용
  5. EvSkillHit 이벤트 처리 (skillEvList_ 멤버 재사용)
     └─ HP 갱신 + S_SkillHit 브로드캐스트
  6. (선택) S_DebugHitbox 브로드캐스트 — kBroadcastDebugHitboxes 플래그, 기본 off
```

---

## 패킷 흐름 (서버 관점)

```
[클라이언트]                           [서버]
 C_SkillStart{assetId, clientMs}  ──→  handleCSkillStartPacket()
                                        Room::skillStart(sessionId, assetId, clientMs)
                                          ├─ elapsedMs = elapsedMs_ - clientMs
                                          ├─ skillSystem_.startSkill(assetId, ownerId,
                                          │    ctx, elapsedMs{elapsedMs})
                                          └─ broadcastExcept(sender, S_SkillStart)

 (매 프레임 Room::update)
                                        updateSkillSystem(dt)
                                          → EvSkillHit{targetId, damage*coeff,
                                                       attackerId, assetId}
                                          → tgt->hp() -= damage
                                          → broadcast(S_SkillHit{attackerId,
                                               targetId, newHp, assetId})

 S_SkillHit{...}                  ←──  broadcast
```

### 타이밍 보상 (`elapsedMs`)

클라이언트가 패킷을 전송한 시점(`clientMs`)과 서버가 수신·처리하는 시점 사이에 RTT/처리 지연이 있다. 서버는 아래와 같이 경과 시간을 보상한다:

```cpp
uint64 elapsedRaw = (elapsedMs_ — clientMs 기반 추정값);
uint16 elapsedMs  = std::min(elapsedRaw, 65535u);
skillSystem_.startSkill(assetId, ownerId, ctx, Milliseconds{elapsedMs});
```

`startSkill()`의 `initialElapsed` 오버로드는 스킬 인스턴스의 `elapsed`를 이미 진행된 시간으로 초기화해, 서버의 히트박스 타이밍이 클라이언트와 동기화되도록 한다.

---

## 히트박스 변환 (서버)

서버는 렌더 상태(`RenderState`)가 없으므로 변환 체인이 클라이언트와 다르다:

**클라이언트:**
```
bone.toDress * animBlender->finalXformData()[i] * renderState().world
```

**서버:**
```
bone.toDress * animController_.bakedSamples[i][sampleIdx] * orientMat(owner->orient())
```

`animController_`는 `Object` 베이스 클래스에 있으며, AI 상태 전환 시 `switchClip()`이 호출되어 클라이언트의 시각적 포즈와 근사치로 동기화된다.

---

## 피격 데미지와 damageCoeff

서버의 `checkHitboxCollisions()`는 `collides(bvh, obb)` 대신 `BVHHitResult` 를 반환하는 오버로드를 사용한다:

```cpp
BVHHitResult r = collides(bvh, obb);
if (r.hit) {
    coeff = r.damageCoeff;  // BVHNode::damageCoeff (머리=2.0 등)
    pendingHits_.push_back({ hi, targetId, coeff });
}
```

`processHitResults()`에서:
```cpp
EvSkillHit{ targetId, (int32)(oh.damage * hr.damageCoeff), attackerId, assetId }
```

클라이언트 스킬 시스템은 `damageCoeff` 없이 `oh.damage`를 그대로 사용한다.

---

## 충격량과 넉백

피격 시 충격량 적용 → AI 속도 제어 억제 흐름:

```cpp
// processHitResults()
tgt->body().applyImpulse(impulseJ, tgt->pos());
tgt->onHitImpulse();  // Npc::onHitImpulse() → knockbackTimer_ = 0.4s

// Npc::update() 다음 프레임
if (knockbackTimer_ > 0s) return {};  // AI setLinearVel() 차단
```

이는 AI의 `setLinearVel()`이 `physicsWorld_.step()` 이후에 실행되어 충격량을 덮어쓰는 문제를 방지한다. 자세한 내용은 `serverArchitecture.md` 넉백 처리 항목 참조.

---

## 스킬 에셋 로딩 — 부팅 1회 컴파일·전 룸 공유 (2026-06-06)

스킬 사양은 방마다 달라지지 않으므로, `SkillAsset` 레지스트리는 **부팅 시 단 한 번만
컴파일하여 전 룸이 공유**한다. (이전에는 `Room::init()`이 방마다 `compileAll`로 Lua를
재컴파일하고 asset vector를 방 개수만큼 복사 보관했다.)

```cpp
// AssetManager::loadAssets() — 부팅 시 1회만 실행
ServerSkillCompiler compiler;
skillAssets_ = compiler.compileAll("../resources/skills");
skillAssets_.shrink_to_fit();
for (u32t i = 0; i < skillAssets_.size(); ++i)   // id 미지정(0) → 1부터 순번 부여
    if (skillAssets_[i].id == 0) skillAssets_[i].id = i + 1;

// Room::init() — 공유 레지스트리를 참조로 바인딩(컴파일/복사 없음)
skillSystem_.bindRegistry(&levelData->assetManager->skillAssets());
```

- **소유**: `AssetManager`가 `std::vector<SkillAsset> skillAssets_`를 소유하고 `skillAssets()`로
  노출한다. 프로그램 수명 동안 주소가 안정적이므로 `SkillInstance::asset`(`const SkillAsset*`)
  포인터가 유효하다.
- **참조**: `SkillSystem::assetRegistry_`는 `const std::vector<SkillAsset>*`(비소유)이며,
  `bindRegistry()`로 공유 레지스트리를 가리킨다. (`registerAssets(&&)`는 제거됨.)
- **공유 안전성**: 부팅 완료 후 레지스트리는 `findAsset`로만 읽히는 읽기 전용이고, 룸 스레드가
  시작되기 전에 컴파일이 끝나므로 read-only 공유가 안전하다. 인스턴스/히트박스 풀 등 런타임
  상태는 그대로 per-room으로 `SkillSystem`에 남는다.
- 기존 공유 리소스 패턴(`Level::terrainChunks`, `Level::assetManager`를 `Room`이 non-owning
  포인터로 참조)과 동일하다.

`ServerSkillCompiler`는 클라이언트 `SkillCompiler`와 동일한 Lua 파일을 처리하지만, 뼈 이름 검증에 클라이언트 Skeleton 대신 `ServerSkeleton`을 사용한다.

---

## 디버그 히트박스 패킷

활성화된 모든 히트박스 OBB를 클라이언트에 전송해 시각적 디버깅을 지원한다:

```cpp
if constexpr (kBroadcastDebugHitboxes) {   // Room.cpp, 기본 false
    skillSystem_.collectActiveOBBs(activeOBBs);
    broadcast(PacketManager::makeSDebugHitboxPacket(...));
}
```

클라이언트는 `S_DebugHitbox`를 수신하면 `debugBVView_`에 100 ms 유효 기간으로 OBB를 표시한다.

> **주의:** 이 브로드캐스트는 매 프레임 OBB 벡터 할당 + 직렬화 + 전 클라이언트 fan-out이라
> 순수 오버헤드다. `Room.cpp`의 `kBroadcastDebugHitboxes`(기본 `false`)로 게이팅한다.
> 시각화가 필요할 때만 `true`로 바꿔 빌드한다.

---

## 성능 최적화 (2026-05-29)

다수 Room × 다수 몬스터 × 모든 공격이 SkillSystem 경유라는 전제([[프로젝트: 게임 장르·규모]])
하에 per-Room per-frame 비용과 힙 할당을 제거했다. **client/server 공통 구조**.

### 1. Object–Hitbox 전용 broad phase (`SkillBroadPhase`)

`checkHitboxCollisions()`가 히트박스마다 전체 객체를 순회하던 O(N·M)을 제거.
`SkillBroadPhase`는 히트박스 AABB와 타깃 AABB의 X축 엔드포인트를 한 번에 정렬·sweep하는
**bipartite sweep-and-prune**으로 (히트박스, 타깃) 후보 쌍만 생성한다. O(K log K + pairs).

- 물리의 Object-Object BroadPhase(`queryPairs`)를 재사용하지 않는다(종류가 다름). SAP sweep
  기법과 물리가 유지하는 타깃 `worldBVH` 루트 AABB만 읽는다 → 물리와 구조적 결합 없음.
- 각 히트박스는 `worldOBBs` 갱신 시 합집합 AABB를 `AttachedHitbox::worldAABB`에 캐시
  (margin `kHitboxAABBMargin=0.2m` fatten으로 1-tick stale 보정 → false negative 방지).
- 타깃은 `checkHitboxCollisions()` 시작 시 1회 수집(`canReceiveDamage() && hp()>0`).

### 2. 동적 인스턴스 풀

`SkillInstancePool`을 고정 `instances[128]` 배열 → `std::vector + freeList + activeList`로 교체.
`update()/hasActiveSkill()/interruptAll()`은 **activeList만 순회**(활성 수). 풀은 파괴하지 않고
재사용해 인스턴스 내부 dedup 맵의 버킷 capacity를 보존한다. `hitboxPool_`/`particleSources_`도
freeList로 O(1) alloc(기존 O(poolSize) 선형 스캔 제거).

### 3. 히트 중복 제거

`SkillInstance`의 그룹·타깃 dedup은 **해시 조회 유지**(타깃 多 AoE에서도 O(1)). `resetSlots()`는
맵을 파괴하지 않고 `clear()`만 호출 → per-cast 할당 churn 제거.

### 4. 할당 정리

`Room::updateSkillSystem`의 `EventList`를 멤버(`skillEvList_`)로 재사용(std::list 노드 재할당
제거), 디버그 브로드캐스트 게이팅(위 참조). 클라이언트는 `updateParticleHitboxSources()`가
per-particle 히트박스를 매 프레임 free/realloc하던 것을 핸들 재사용으로 변경.

---

## VFXParticle 히트박스 — 결정론적 샘플러 (2026-06-11)

이전까지 서버의 `updateParticleHitboxSources()`는 no-op(파티클 시스템 부재)이었다.
이제 캐스터가 생성한 per-cast 시드(`C_SkillStart::skillSeed`)와
`common/particleGameplay.hpp`의 카운터 기반 PRNG + 해석적 평가(`pg::evaluateParticles`)로
**클라이언트 비주얼과 동일한 파티클 위치에 서버 히트박스를 생성**한다.

- `PlayVFX` 이벤트는 더 이상 no-op이 아니다: 이펙트의 월드 앵커(위치+회전+이벤트 시각)를
  `SkillInstance::vfxAnchors`에 기록한다 (클라 PlayVFX 변환 수식 미러).
- `SpawnHitbox`(VFXParticle)는 Lua `addVFX(id, path, { systems = ... })` 구성으로
  effect JSON의 게임플레이 설정을 lazy-load해 소스에 바인딩한다. 구성 테이블이 없는
  스킬은 기존처럼 히트박스 비활성(경고 로그) — 점진적 마이그레이션.
- 상세 설계·제약·SYNC 계약: `client/docs/particleHitboxDeterminism.md` 참조.

---

## 피아 식별 (Faction, 2026-05-29)

아군(같은 진영)끼리는 서로 타격할 수 없어야 한다. 진영 개념을 도입해 **피격 정확성 + broad
phase 충돌 회피 최적화**를 동시에 달성한다. **client/server 동일 규칙**(클라 예측이 서버 권위와
일치해야 하므로).

- **Faction은 엔티티 속성**: `object.hpp`에 `enum class Faction { Neutral, Players, Monsters }`,
  `factionBit(f)=1<<f`, `hostileMask(f)`(Players↔Monsters 적대, Neutral은 0), `Object::faction_`
  (+접근자). 진영 의미론은 모두 object.hpp에 모았다 — `SkillBroadPhase`는 게임 의미를 모른 채
  일반 `u32 마스크`만 다룬다(디커플).
- **진영 설정 지점**: `Room::init()` 고블린 루프 → `Monsters`, `Room::enter()` 플레이어 →
  `Players`. 명시 설정 안 된 객체는 `Neutral`(공격·피격 모두 불가, 안전한 기본값).
- **히트박스 targetMask 캐시**: `SpawnHitbox` 시점에 owner faction으로부터 `hostileMask`를 계산해
  `AttachedHitbox::targetMask`(및 `ParticleHitboxSource::targetMask`)에 캐시. 충돌 시 owner를 다시
  조회하지 않는다.
- **broad phase 마스크 필터(최적화 핵심)**: `SkillBroadPhase::HitboxEntry`에 `mask`(=히트박스
  targetMask), `TargetEntry`에 `category`(=타깃 `factionBit`). `build()`의 후보 emit 직전에
  `(mask & category)`를 **YZ overlap보다 먼저** 검사 → 아군 쌍은 candidate가 되기 전에 sweep
  단계에서 제거 → narrow phase(BVH vs OBB) 미실행.
- narrow phase의 owner 제외(`targetId == ownerObjectId`)는 유지(저렴, self/미래 아군 타깃 대비).

> **새 진영/캐릭터 추가 시:** 생성 지점에서 `setFaction` 호출 필수. 빠뜨리면 `Neutral`이 되어
> 플레이어가 그 몬스터를 타격하지 못한다(회귀). `hostileMask`에 새 적대 관계를 추가할 것.

---

## 객체 수명주기와 연결 해제 (중요 제약)

SkillSystem은 **객체를 raw 포인터로 직접 들고 있지 않고**, `SkillDispatchContext::objectById`
(sparse `Object**`, id로 색인)를 통해 매 프레임 해소한다. `AttachedHitbox`는 owner/target을
**id로** 저장하고 `lookupObject(id)`로 조회한다. 따라서 객체 수명주기 관리에 다음 제약이 있다.

1. **제거 시 슬롯 null 필수** — 객체가 사라지는 모든 경로는 `objectById[id]`를 `nullptr`로 만들어야
   한다. 서버는 `Room::leave()`가 `unregisterObject()`로 슬롯을 비운다. 이를 누락하면
   `checkHitboxCollisions()`의 타깃 수집이 해제된 메모리를 역참조해 크래시한다(클라이언트에서
   `skillObjectById_` 미정리로 실제 발생했던 버그, 2026-05-29).

2. **연결 해제 시 소유 스킬 종료** — `Room::leave()`는 `skillSystem_.interruptAll(playerId, ctx)`로
   나가는 플레이어의 스킬 인스턴스를 종료한다. 이를 빠뜨리면 `IdPool`이 그 id를 신규 플레이어에게
   재할당했을 때, 잔존 인스턴스가 신규 플레이어에게 **재바인딩**되어 엉뚱한 히트박스/데미지를
   유발한다. (단 `interruptAll`은 `interruptible=true`만 종료 — 슬롯 null 윈도우 동안은
   `updateHitboxes`의 owner 비활성화로 무해.)

3. **소유자 제거는 허용** — `updateHitboxes()`는 매 프레임 owner를 `lookupObject`로 조회하고
   null이면 해당 히트박스를 비활성화한다. 즉 소유자가 먼저 사라져도 크래시하지 않는다.

> **신규 엔티티/디스폰 경로 추가 시:** objectById 슬롯 null + (필요 시) 소유 스킬 interrupt를
> 반드시 함께 처리할 것.

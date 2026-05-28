# 스킬 시스템 아키텍처 (서버)

## 개요

서버 스킬 시스템은 클라이언트의 스킬 시스템과 구조가 동일하되, 렌더링·VFX·파티클 관련 요소가 모두 제거된 권위 서버(authoritative server) 버전이다. 히트 판정과 데미지 계산은 오직 서버에서만 수행되며, 결과를 클라이언트에 브로드캐스트한다.

**공유 파일 (클라이언트와 동일):**
- `client/skill/skillTypes.hpp` — SkillAsset, SkillHitboxDef, OnHitDef, TimelineEvent
- `resources/skills/*.lua` — 스킬 정의 소스

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
          tickInstance + dispatchEvent
          updateHitboxes              ← 뼈 변환으로 worldOBBs 갱신
          checkHitboxCollisions       ← BVHHitResult (damageCoeff 포함)
          processHitResults           ← EvSkillHit 생성 + 충격량 적용
  5. EvSkillHit 이벤트 처리
     └─ HP 갱신 + S_SkillHit 브로드캐스트
  6. S_DebugHitbox 브로드캐스트 (활성 OBB 목록)
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

## 스킬 에셋 로딩

```cpp
// Room::init()
ServerSkillCompiler compiler;
auto assets = compiler.compileAll("../resources/skills");
skillSystem_.registerAssets(std::move(assets));
```

`ServerSkillCompiler`는 클라이언트 `SkillCompiler`와 동일한 Lua 파일을 처리하지만, 뼈 이름 검증에 클라이언트 Skeleton 대신 `ServerSkeleton`을 사용한다.

---

## 디버그 히트박스 패킷

매 프레임, 활성화된 모든 히트박스 OBB를 클라이언트에 전송해 시각적 디버깅을 지원한다:

```cpp
skillSystem_.collectActiveOBBs(activeOBBs);
broadcast(PacketManager::makeSDebugHitboxPacket(...));
```

클라이언트는 `S_DebugHitbox`를 수신하면 `debugBVView_`에 100 ms 유효 기간으로 OBB를 표시한다.

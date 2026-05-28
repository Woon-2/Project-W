# 스킬 시스템 아키텍처 (클라이언트)

## 개요

스킬 시스템은 타임라인 기반 이벤트 방식으로 동작한다. 스킬 정의는 Lua 스크립트로 작성하고 `SkillCompiler`가 C++ 데이터(`SkillAsset`)로 컴파일한다. 런타임에는 컴파일된 에셋만 참조하며 Lua 의존성이 없다.

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
  payload   — 고정 크기 union (힙 할당 없음)
```

---

## 런타임 구조

```
SkillSystem
  assetRegistry_[]   — registerAssets()로 등록, 이후 불변
  instancePool_[]    — SkillInstance 정적 배열 (최대 128)
  hitboxPool_[]      — AttachedHitbox (비활성 슬롯 재사용)
  particleSources_[] — ParticleHitboxSource (VFXParticle attach 전용)
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
  2. updateHitboxes()         — Bone attach: boneWorldXform * localOBBs → worldOBBs
  3. updateParticleHitboxSources() — VFXParticle: 파티클 수에 맞게 hitbox 생성/제거
  4. checkHitboxCollisions()  — worldOBBs vs target BVH → pendingHits_
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

### 로컬 플레이어 스킬

1. Q 키 → `skillSystem_.startSkill(assetId, playerId, ctx)` 로컬 실행
2. `C_SkillStart { skillAssetId, clientMs }` 서버 전송

**`SkillDispatchContext` 구성 (Online):**
```cpp
skillCtx_.clientPredictionOnly = true;   // 데미지 권위 = 서버
skillCtx_.evList     = &eventList_;
skillCtx_.objectById = skillObjectById_.data();
skillCtx_.vfxById    = skillVfxById_.data();
skillCtx_.camera     = &camera_;
```

`clientPredictionOnly = true`이므로 `processHitResults()`에서:
- **`EvSkillHit` 발행 안 함** → 로컬 데미지 없음 (서버 결과 대기)
- VFX 재생 ← 클라이언트가 직접 처리 (반응성 유지)
- 충격량 적용 ← 클라이언트가 직접 처리 (반응성 유지)

### 원격 플레이어 스킬 수신 (`S_SkillStart`)

```cpp
void Game::onSkillStart(uint16 ownerId, uint32 skillAssetId, uint16 elapsedMs) {
    // 원격 플레이어 공격 애니메이션 트리거
    idPlayerMap_[ownerId]->animBlender()->triggerAttack();
    // 비주얼 전용으로 스킬 시작 (clientPredictionOnly=true)
    skillSystem_.startSkill(assetId, ownerId, ctx, Milliseconds{elapsedMs});
}
```

`elapsedMs`로 서버 기준 경과 시간을 보상해 원격 클라이언트의 히트박스 타이밍을 맞춘다.

### 피격 결과 수신 (`S_SkillHit`)

```cpp
void Game::onSkillHit(uint16 attackerId, uint16 targetId, int32 newHp, uint32 skillAssetId) {
    applyHit(targetId, newHp);           // 서버 권위 HP 적용
    // 피격 VFX 재생 (asset의 hitVfxId 참조)
}
```

HP 변경은 서버 값을 그대로 반영한다. 로컬 클라이언트가 VFX·충격량을 미리 적용했더라도 서버 HP가 최종값이다.

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
| HP 갱신 | `EvSkillHit` 즉시 | `S_SkillHit` 수신 시 |
| 네트워크 패킷 | 없음 | `C_SkillStart` 송신 / `S_SkillStart`, `S_SkillHit` 수신 |

---

## 패킷 흐름 (Online)

```
[로컬 클라이언트]                 [서버]                  [다른 클라이언트]
 Q 누름
 ├─ startSkill(local, prediction) ─────────────────────────────────────────
 └─ C_SkillStart{assetId, clientMs} ──→ Room::skillStart()
                                          ├─ elapsedMs = now - clientMs
                                          ├─ startSkill(assetId, ownerId,  )
                                          │    ctx, elapsedMs)  ← 서버 실행
                                          └─ broadcastExcept(sender,       )
                                               S_SkillStart{assetId,       )
                                                 ownerId, elapsedMs}  ─────→ onSkillStart()
                                                                              startSkill(prediction)

 (매 서버 프레임)
                                        updateSkillSystem()
                                          checkHitboxCollisions()
                                          EvSkillHit 발행
                                          newHp = tgt->hp() - damage
                                          broadcast(S_SkillHit{attackerId,
                                            targetId, newHp, assetId})
 onSkillHit(newHp)                    ←─────────────────── onSkillHit(newHp)
 applyHit(targetId, newHp)                                  applyHit(targetId, newHp)
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

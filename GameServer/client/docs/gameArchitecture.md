### 게임 아키텍처
- `online/onlineGame.hpp` — 네트워크 모드
- `standalone/game.hpp` — 싱글 플레이어 모드

---

### 게임 루프 (StandAlone::Game::update)

매 프레임 아래 순서로 실행된다.

```
1. skillCtx_ 갱신          — evList/pTimer 포인터 갱신 (per-frame context 준비)
2. processInput(dt)        — 키보드/마우스 입력 처리
                             LButton → combatSystem_.onPlayerAttack()
                             Q 키   → skillSystem_.startSkill() + C_SkillStart 전송 (online만)
3. debugBVView_.update(dt) — TTL 감소 + 조건부 소멸
4. combatSystem_.update()  — 몬스터 AI 공격 판정 (플레이어 생존 시만)
5. skillSystem_.update()   — 스킬 타임라인 진행 + 히트박스 갱신 + 충돌 판정 + EvSkillHit 발행
                             (플레이어 생존 시만)
6. 이벤트 처리 루프        — EventList 순회: Hit / Attack / Death / SkillHit / CameraShake
   └─ EvSkillHit → EvHit  변환 후 기존 HP/애니메이션 로직 재사용
7. 물리 업데이트           — 적응형 고정 타임스텝 (아래 상세 참조)
8. BV 충돌 색상 갱신       — Terrain접촉=빨강, Object-Object=파랑 (physicsStepsDone > 0 시)
9. Object::update()        — tPhysicInterpolation으로 RigidBody 보간 → RenderState 갱신
                             viewFrustumCulled || hiZCulled_ 이면 조기 반환
10. animSystem_.updatePriorities() — 플레이어 위치 기준 애니메이션 LOD 우선순위 갱신
11. camera_.update() / dirLight_.update() / dirLight_.updateCSMCascades()
12. HP 바 위치·값 갱신 / HiZ 통계 레이블 / uiManager_.layout() + update()
13. animSystem_.update()   — 애니메이션 스케줄링 (culled 오브젝트 스킵)
14. Ragdoll 활성화/동기화  — animSystem_.update() 이후 finalXformData 확정된 시점에 실행
    └─ activateRagdollIfPending: seedFromFinalXforms → buildPassengers → activate
    └─ syncRagdollToAnim: ragdoll body 위치를 finalXformData에 덮어쓰기
15. 발 흙먼지 파티클 방출  — 달리기 중 발 접지 페이즈 감지 시 emit
16. 파티클 시스템 update() — 모든 ParticleEffect 인스턴스 갱신
17. clearEvents(eventList_)
```

---

### 게임 루프 (Online::Game::update)

```
1. SleepEx(1, true)        — IOCP 비동기 완료 처리 (네트워크 패킷 수신)
2. player_ == nullptr 가드
3. skillCtx_ 갱신          — evList/objectById 포인터 갱신
4. processInput(dt)        — 키보드/마우스 입력 + moveChange_ 판단
5. 물리 업데이트           — 적응형 고정 타임스텝 (같은 방식)
   └─ otherPlayers_ + goblins_ rebuildBodyBVH()
6. skillSystem_.update()   — clientPredictionOnly=true, 플레이어 생존 시만
                             데미지 이벤트 없음; VFX + 충격량만 클라이언트에서 처리
7. 이동 패킷 전송          — moveStateSendAcc_ >= moveStateSendInterval_(50ms)이면 sendMovePacket()
8. Object::update()        — 플레이어(tPhysic 보간), 원격 플레이어(네트워크 보간 tNet), 고블린
9. animSystem_.updatePriorities() / camera_ / dirLight_
10. animSystem_.update()
11. Ragdoll 활성화/동기화  — standalone과 동일한 패턴
12. HP 바 / HiZ 통계 / UI
13. 파티클 시스템 update() + 디버그 BV view update + 발 흙먼지 파티클
```

**Standalone vs Online 주요 루프 차이:**

| | Standalone | Online |
|---|---|---|
| 네트워크 처리 | 없음 | SleepEx (루프 최초) |
| combatSystem_.update() | 있음 (몬스터 AI 근접 공격) | 없음 (서버 권위) |
| skillSystem clientPredictionOnly | false (데미지 즉시) | true (VFX만) |
| 이벤트 처리 위치 | 물리 이전 | 없음 (서버 이벤트는 패킷으로 수신) |
| 물리 + 스킬 순서 | 스킬 → 이벤트 → 물리 | 물리 → 스킬 |
| 이동 패킷 전송 | 없음 | 50ms 주기 |

---

### 물리 렉 관리 설계 원칙

```
물리 업데이트 (두 모드 공통 구조):
  clampedDt = min(deltaTime, kMaxPhysicsDeltaTime)   // death spiral 방지
  effectiveInterval = physicUpdateInterval * physicUpdateScaleK_
  while (acc >= effectiveInterval && steps < kMaxPhysicsStepsPerFrame):
      physicsWorld_.step(effectiveInterval)
```

- `physicUpdateScaleK_`: 렉 수준에 따라 1~4 자동 조절. k=2 → 30Hz, k=4 → 15Hz
- 배율 증가 조건: 2프레임 연속 step limit 도달 (`kLagScaleUpFrames`)
- 배율 감소 조건: 120프레임(약 2초) 연속 정상 (`kLagScaleDownFrames`) — 진동 방지
- render 스킵은 깜빡임 문제로 채택하지 않음

---

### 렌더 루프 (두 모드 공통 구조)

```
render():
  skipNextRender_ 플래그 체크 — 렉 시 1프레임 스킵
  cullObjects()               — view frustum culling → setFrustumCulled
  (standalone만) cullObjectsForShadow()
  debugBVView_.render()
  Object::render() × N       — frustum culled 제외, Hi-Z culled는 DrawEvent 계속 제출
  skybox / camera.updateGFX / dirLight.render
  ParticleEffect × N .render()
  uiManager_.render()
  gfx_.addFrameData() × 파이프라인 수
  gfx_.render()               — Hi-Z 5단계 compute + indirect draw + visibleFlags readback
  applyHiZCulling()           — readback 결과로 setHiZCulled + AnimBlender::setCulled 갱신
```

**컬링 시스템 설계 원칙:**
- `viewFrustumCulled`: DrawEvent 제출 차단 전용. Hi-Z와 무관.
- `hiZCulled_`: Object::update()/AnimBlender 연산 스킵 전용. 1-frame delay.
- `AnimBlender::setCulled(frustum || !hiZVisible)`: 두 플래그 통합해 AnimSystem 스킵 조건으로 사용.
- `applyHiZCulling()` 한 곳에서만 animBlender 동기화 (`cullObjects()`에서 setCulled 직접 호출 금지).

---

### 이벤트 시스템
파일: `event.hpp`

이벤트는 크기별 오브젝트 풀에서 할당되어 `EventList(std::list<char*>)`에 저장된다. 순회 중 삽입이 안전하다 (list 반복자 무효화 없음).

**이벤트 타입:**
| 이벤트 | 구조체 | 주요 필드 | 용도 |
|--------|--------|-----------|------|
| `Hit` | `EvHit` | targetId, hp | 피격 — HP 갱신 + 피격 애니메이션 |
| `Death` | `EvDeath` | victimId | 사망 — 사망 애니메이션, playerDead_ 플래그 |
| `Attack` | `EvAttack` | attackerId | 공격 애니메이션 트리거 |
| `SkillHit` | `EvSkillHit` | targetId, damage, attackerId, skillAssetId | 스킬 피격 — EvHit로 변환해 재사용 |
| `CameraShake` | `EvCameraShake` | magnitude, duration | 카메라 흔들림 (미구현, 소비만) |

**흐름 예시 — 몬스터가 플레이어를 공격하는 경우 (standalone):**
```
combatSystem_.update()
  → EvAttack(monsterId)       : 몬스터 공격 애니메이션
  → EvHit(playerId, hp)       : 플레이어 HP 감소 + 피격 애니메이션
  → (hp == 0) EvDeath(playerId)
```

**흐름 예시 — 플레이어 스킬 피격 (standalone):**
```
skillSystem_.update()
  → EvSkillHit(targetId, damage, ...)
이벤트 루프에서:
  → EvSkillHit → EvHit(targetId, newHp)   : 기존 HP/애니메이션 로직 재사용
```

**이벤트 버스:**
- 각 Object는 `IEventBus* eventBus()`를 제공
- AnimBlender도 별도 `IEventBus`를 가져 애니메이션 상태를 갱신
- Object::EventBus → AnimBlender::EventBus 순으로 전파

---

### CombatSystem (standalone)
파일: `standalone/combatSystem.hpp`, `standalone/combatSystem.cpp`

전투 로직을 Game 클래스로부터 분리한 서브시스템. 근접 공격 AABB hitbox 기반 판정.

**주요 인터페이스:**
- `registerCombatant(Object*, CombatConfig)` — 전투 참가자 등록
- `onPlayerAttack(playerId, evList)` — 플레이어 능동 공격. forward 방향 AABB hitbox
- `update(dt, playerId, evList)` — 매 프레임. 몬스터 AI 쿨타임 + 플레이어 교차 시 EvAttack/EvHit

**CombatConfig 파라미터:**
- `attackHalfExtent` — 공격 hitbox AABB 반크기
- `attackOffsetFwd` — hitbox 중심 전방 오프셋
- `damage` — 한 번 공격 데미지
- `cooldown` — AI 공격 쿨타임

**관계:** CombatSystem은 근접 공격(AABB) 담당. 스킬 시스템(OBB + BVH 기반)과 독립적으로 공존. → 스킬 시스템 상세는 `skillArchitecture.md` 참조.

---

### Ragdoll 시스템

사망 시 물리 기반 래그돌 전환. `Goblin` 클래스가 `Ragdoll` 멤버를 보유.

**전환 패턴 (Pending 플래그):**
```
S_SkillHit / EvHit → hp <= 0
  → goblin->setDead(true)
  → goblin->setRagdollPendingActivation(true)   // 즉시 활성화 않음

같은 프레임 animSystem_.update() 후:
  → activateRagdollIfPending:
       seedFromFinalXforms(finalXformData, skeleton, worldMat)
       buildPassengers(skeleton, finalXformData)
       activate(physicsWorld_)
       physicsWorld_.unregisterBody(&g.body())   // standalone만, 키네마틱 바디 해제
```

activateRagdollIfPending을 animSystem_.update() **이후**에 호출하는 이유: finalXformData가 최신 포즈로 확정된 후 seed해야 올바른 초기 래그돌 자세가 된다.

매 프레임 `syncRagdollToAnim`: ragdoll 물리 결과를 finalXformData에 덮어써 렌더링에 반영.

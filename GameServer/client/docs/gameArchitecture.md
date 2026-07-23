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
                             스킬 키 → castSkillByName() = startSkill(seed) + C_SkillStart 전송 (online만)
                             임시 키맵: 1~0 = 스킬 1~10, Shift+1~6 = 스킬 11~16, Q = SwordSlash
                             (순서 = characterSkillMap Player 목록 - AoESlashGreen/TornadoShot 제외)
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
6.5 이벤트 디스패치 루프   — eventList_ 순회 → 대상 객체 eventBus()->receive()로 분배
                             (Hit/Attack/Death/Respawn 애니메이션 일원화; 객체 갱신 이전에 처리)
                             패킷 핸들러(SleepEx)·스킬 시스템이 post한 이벤트를 처리한다.
7. 이동 패킷 전송          — moveStateSendAcc_ >= moveStateSendInterval_(50ms)이면 sendMovePacket()
8. Object::update()        — 플레이어(tPhysic 보간), **원격 플레이어 + 몬스터(네트워크 보간 tNet)**
   - 서버 위치로 구동되는 객체(원격 플레이어·몬스터)는 모두 `tNet = min(netInterpAcc_/netInterpDuration_, 1)`
     로 보간한다. `netInterpAcc_`/`netInterpDuration_`은 `Object` 베이스 공용. S_Move(`movePlayer`/`moveGoblin`)
     수신 시 `netInterpAcc_=0` 리셋 → 한 이동 간격에 걸쳐 prev→curr 보간 후 **curr에서 정지(clamp)**.
     **주의:** 물리 step 클럭(`tPhysicInterpolation`)으로 보간하면 매 step마다 t가 0→1을 반복해, 이동이
     드물거나 멈춘 객체가 prev↔curr를 진동(땅속↔공중 깜빡임)한다 — 몬스터가 이 버그를 겪다가 원격
     플레이어와 동일한 tNet으로 통일해 해결.
9. animSystem_.updatePriorities() / camera_ / dirLight_
10. animSystem_.update()
11. Ragdoll 활성화/동기화  — standalone과 동일한 패턴
11.5 시체/에너지 오브        — 사망 시 migrateToCorpse → updateCorpses(래그돌 2.5s→오브 전환,
                             pending charge 매칭, 흡수 완료 시 풀 반환) + orbSystem_.update(추적/흡수)
12. HP 바 / HiZ 통계 / UI
13. 파티클 시스템 update() + 디버그 BV view update + 발 흙먼지 파티클
```

**Standalone vs Online 주요 루프 차이:**

| | Standalone | Online |
|---|---|---|
| 네트워크 처리 | 없음 | SleepEx (루프 최초) |
| combatSystem_.update() | 있음 (몬스터 AI 근접 공격) | 없음 (서버 권위) |
| skillSystem clientPredictionOnly | false (데미지 즉시) | true (VFX만) |
| 이벤트 처리 위치 | 물리 이전 | skillSystem.update() 직후, Object.update() 이전 |
| 물리 + 스킬 순서 | 스킬 → 이벤트 → 물리 | 물리 → 스킬 |
| 이동 패킷 전송 | 없음 | 50ms 주기 |

---

### Player Camera Follow / Right-Drag Orbit

`Camera::update()`는 타겟 `orient()`를 그대로 쓰지 않는다. 타겟 forward를 XZ 평면에 투영해 yaw를 추출하고, pitch만 저역통과로 따라가며 roll은 제거한다. `at_`도 별도 저역통과를 적용해 평지 보행/물리 접촉의 고주파 흔들림이 시선 목표점에 바로 들어가지 않게 한다.

`StandAlone::Game::processInput()`과 `Online::Game::processInputGame()`은 같은 조작 정책을 사용한다. 우클릭 드래그 중에는 마우스 X가 `cameraYaw_`에 누적되어 플레이어 orient와 카메라 회전 프레임이 분리된다. 우클릭을 놓은 뒤 이동 입력이 들어오면 누적된 `cameraYaw_`를 플레이어 yaw에 흡수하고 `cameraYaw_`를 0으로 되돌려, 이동 방향/공격 방향/애니메이션 블렌딩이 다시 플레이어 forward 기준으로 결합된다. Online 모드는 이 재결합 회전도 기존 `sendMouseMovePacket()` forward 비교 경로로 전파한다.

---

### 온라인 플레이어 간 Soft Separation (Reciprocal Client-Prediction)

다른 플레이어는 `Kinematic`(무한 질량)이므로, 로컬 `Dynamic` 플레이어가 `ContactConstraint`로 부딪치면 침투 해소량 100%가 로컬에게만 실려 "벽에 튕기는" 느낌이 난다. 이를 reciprocal soft separation으로 대체한다.

**핵심 원리:** 각 클라는 자기 플레이어만 소유한다(위치 권위). 두 플레이어가 겹치면 이 클라는 로컬 플레이어를 침투량의 **절반**만큼만 민다. 상대의 절반은 상대 클라가 동일 규칙으로 처리하며, 그 결과는 **기존 `C_Move`/`S_Move`로 전파**된다(신규 패킷 없음).
- 절반인 이유: 양쪽이 전부 밀면 합이 2×침투 → 과분리·진동. 각자 절반이면 합이 정확히 침투량이라 매끄럽게 수렴(RVO/ORCA reciprocity).
- 상대(B)가 "밀려나야 함"을 아는 방법: B의 클라도 매 step `resolvePlayerSeparation()`을 돌려 아직 남은 겹침을 직접 감지해 자기 절반을 처리한다(입력 없어도 실행). 명령 수신이 아닌 자기 관측 기반.

**구현 위치:** `Online::Game::resolvePlayerSeparation(Seconds)` (엔진 `PhysicsWorld`가 아닌 Game 레벨 — 레이어 분리 원칙). 물리 while 루프에서 `physicsWorld_.step()` 직후 매 step 호출.
- 수평(XZ) 원기둥 모델, Faction `Players`끼리만, 사망/래그돌 제외.
- `correction = clamp(0.5 × penetration × stiffness, maxSpeed × dt)`. 완전 겹침은 `getId()` 비교로 결정론적 방향(양쪽 클라 반대 방향).
- `setCurrPos`로 **curr만 갱신**(렌더 보간 prev 보존). velocity에는 분리 변위를 주입하지 않음(원격 dead-reckoning·애니메이션 안정성). 정지 중 밀려나도 전파되도록 적용 시 `moveChange_ = true`.

**hard contact 차단:** 충돌 레이어(`kLayerPlayer`/`kPlayerCollisionMask`)로 플레이어-플레이어 쌍을 `generateContacts` 필터에서 제외. 정적 환경 충돌(지형·scatter prop)은 `WorldCollider`가 Dynamic body만 순회하므로 영향 없음.

**서버 (경량 검증):** `Room::move`는 클라가 보낸 분리 결과를 신뢰하되, 직전 위치 대비 수평 변위가 허용치(≈7m)를 넘으면 클램프(안티-텔레포트). 겹침 강제 해소·보정 명령은 하지 않는다. 서버 플레이어는 `Kinematic`이라 reciprocity 불필요 — 클라 결과 저장만으로 정합.

> **향후 과제 — 플레이어-몬스터 충돌:** 권위 모델이 다르다(몬스터는 서버 `Dynamic`, 클라 `Kinematic`). 플레이어가 몬스터를 못 뚫게 하려면 별도 설계 필요(클라 예측 + 서버 권위 넉백). 현 separation은 플레이어-플레이어 한정.

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
  feedbackCullResultToAnim()  — readback 결과로 setHiZCulled + AnimBlender::setCulled 갱신 (구 applyHiZCulling)
```

**컬링 시스템 설계 원칙:**
- `viewFrustumCulled`: DrawEvent 제출 차단 전용. Hi-Z와 무관.
- `hiZCulled_`: Object::update()/AnimBlender 연산 스킵 전용. 1-frame delay.
- `AnimBlender::setCulled(frustum || !hiZVisible)`: 두 플래그 통합해 AnimSystem 스킵 조건으로 사용.
  단, `AnimBlender::hasEverUpdated() == false`(아직 한 번도 onCalcLocal이 안 불림)인 동안은
  컬링 판정과 무관하게 `setCulled(false)`를 강제해 최초 1회 갱신을 보장한다.
- `feedbackCullResultToAnim()` 한 곳에서만 animBlender 동기화 (`cullObjects()`에서 setCulled 직접 호출 금지).

---

### 애니메이션 블렌딩 규약

**모드 LOD** — `AnimBlender::updatePriority()`가 refPos(플레이어 위치)로부터의 거리로 모드를 고른다.
약 29m(`kDistScale=50` × 0.577) 이내는 `Mode::Keyframe`(다중 클립 가중 블렌딩), 그 밖은
`Mode::Baked`(가중치 최대 클립 1개의 베이크 샘플). 즉 **플레이어와 근거리 몬스터는 항상 Keyframe 경로**다.

**쿼터니언 부호(반구) 정렬은 가중합의 전제조건이다.**
- `sumWeightedAnimFrames()`(nlerp 가중합)는 **가중치 최대 프레임을 기준으로 각 항의 부호를 맞춘 뒤** 더한다.
  q와 -q는 같은 회전인데 클립마다 추출된 부호가 제각각이라, 정렬 없이 더하면 두 클립이 비슷한 가중치일 때
  항들이 상쇄된다. 실제 사례: 플레이어 `pelvis`(전신을 지배하는 본)가 `Combat_2H/Bow/Cast_Ready`에서
  `Run_*`와 반대 부호(dot ≈ -0.95)로 저장돼 있어, idle↔run이 섞이는 가감속 구간마다 합의 크기가 0.15까지
  붕괴하고 결과가 최대 180° 튀었다(= 이동 중 캐릭터가 통째로 회전). `Combat_1H_Ready`만 같은 부호라
  **창 장착 시에만 멀쩡했던 것**이 진단의 결정적 단서였다.
- `lerpAnimFrames()`(hit/death/attack 오버레이)는 `XMQuaternionSlerp`가 최단호 부호 보정을 하므로 영향 없다.
- **새로운 가중 블렌딩을 추가할 때는 반드시 부호 정렬을 포함할 것.** 추출기가 클립 간 부호를 정준화해 주리라
  기대하면 안 된다(클립 내부 키프레임 연속성과 충돌한다).

**블렌딩 가중치는 최고속에서만 순수해진다** — `tIdle = 1 - tRun`이고 `tRun`은 속력 0.05→5.1 m/s 램프다.
이동 가속도는 `최고속도 × linearDamping`이어야 선언한 최고속에 실제로 도달한다(적분기가 매 step
`(1 - linearDamping·dt)`로 감쇠하므로 종단속도 = accel/damping). 이 관계가 깨지면 캐릭터가 영원히
중간 속도에 머물러 idle이 상시 섞인 채로 재생된다.

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

사망 시 물리 기반 래그돌 전환. `Goblin` 클래스가 `Ragdoll ragdoll_`과 `ragdollInitVelocity_`를 보유.

**전환 패턴 (Pending 플래그):**
```
[Online] S_SkillHit(newHp<=0) → onSkillHit():
  → goblin->setRagdollInitVelocity(pkt->targetVelocity)  // 사망 시점 서버 속도 저장
  → applyHit() → holdEvent(eventList_, EvDeath)
  → 디스패치 루프 → Goblin::EventBus(Death): isDead_=true + setRagdollPendingActivation(true) + 사망 애니메이션

[Standalone] EvHit → hp <= 0 → holdEvent(EvDeath):
  → Goblin::EventBus(Death): isDead_=true + setRagdollPendingActivation(true) + 사망 애니메이션
  // initVel은 activate 직전 g.body().linearVel()에서 읽음

같은 프레임 animSystem_.update() 후:
  → activateRagdollIfPending:
       seedFromFinalXforms(finalXformData, skeleton, worldMat)
       buildPassengers(skeleton, finalXformData)
       activate(physicsWorld_)
       physicsWorld_.unregisterBody(&g.body())   // standalone만
       // 초기 속도: 모든 뼈에 setLinearVel(initVel)
       // noise impulse: 뼈별 BoneBoxDef::noiseImpulse 크기, velDir 방향 bias(0.6)
```

activateRagdollIfPending을 animSystem_.update() **이후**에 호출하는 이유: finalXformData가 최신 포즈로 확정된 후 seed해야 올바른 초기 래그돌 자세가 된다.

`buildPassengers`는 ragdoll body가 없는 **모든** 본을 어떤 body에 강체 바인딩한다(자손은 조상 body, 조상 없는 본은 무방향 BFS로 최근접 body). 바인딩 누락 시 그 본은 동결된 objectWorldMat 때문에 월드에 고정되어 메시가 늘어남 — 상세: `physicsArchitecture.md` "Passenger 본 커버리지 불변식".

매 프레임 `syncRagdollToAnim`: ragdoll 물리 결과를 finalXformData에 덮어써 렌더링에 반영.

**Ragdoll 물리 파라미터:** Unity Inspector에서 뼈별로 설정 → `ModelExtractor.cs` 익스포트 → `.bin` 파일에 포함 → `importRagdollConfig(mesh.cpp)`로 `BoneBoxDef`에 로드.

---

### 에너지 오브 사망 연출 (시체 이관 / 풀링 / charge 매칭)

몬스터 처치 보상 연출. 시체가 잠시 래그돌로 남았다가 서브메시별 **에너지 오브**로 변해 로컬
플레이어에게 흡수되고, 흡수 타이밍에 스킬 charge HUD가 채워지며 몸에 색 물결이 퍼진다.
렌더링(오브 셰이더, 흡수 물결 GB2 주입)은 `graphicsArchitecture.md` "몬스터 사망 에너지 오브
연출" 절을 참조. 여기서는 **게임 레벨 라이프사이클**을 다룬다. (`online/onlineGame.{hpp,cpp}`,
`energyOrbSystem.{hpp,cpp}`, `object.cpp` `BodyRipple`, `ui/skillDialHUD`.)

**왜 시체를 이관하나(client-authored Corpse):** 사망 연출 도중 서버 리스폰 패킷이 도착하면
같은 오브젝트가 부활해 연출이 끊긴다. 그래서 사망 처리 순간 `migrateToCorpse(obj, kind, npcId)`로
오브젝트를 `Corpse`(`corpses_` 컨테이너)로 옮기고, 활성 몬스터 컨테이너(`idMonsterMap_`/
`idGoblinMap_` 등 + `skillObjectById_`)에서 분리한다. 이후 서버 동기화와 무관하게 클라가 단독
관리하며, **모든 오브가 흡수된 뒤에만** 시체가 사라진다(`orbSystem_.hasActiveOrbs(corpseId)`).

**ID 처리 (꼬임·범람 방지):** 시체는 서버 동기화에서 떨어져 나온 client-authored 객체다. 시체가
서버 npc id를 그대로 유지하면 서버가 그 npcId를 리스폰에 재사용할 때 매핑이 꼬이므로, 시체는
서버 id와 겹치지 않는 **고정 sentinel `kDetachedCorpseId=0x40000000`** 로 `setId`된다(시체는
어떤 id 맵에도 없어 조회되지 않으므로 모든 시체가 sentinel을 공유해도 안전 — per-corpse id 발급
불필요 → 카운터 범람 없음). 원래 npcId는 `Corpse::origId`에 보존하고, `reinitFromPool`이 풀 복귀
시 `setId(npcId)`로 서버 id를 복원한다.
- **renderObjectId는 객체당 1회만 발급되어 평생 유지**된다(생성 시 1회; migrate/pool/reinit에서
  재발급하지 않음). 단조 증가 카운터가 동시 생존 객체 간 유일성을 이미 보장하므로 재발급은 불필요
  하고, 재발급하면 사망/리스폰 사이클마다 카운터가 치솟아 `maxRenderObjectId`(10000)를 초과한다.
- **orb 연계 corpseId = 그 객체의 renderObjectId**(동시 활성 시체 간 유일, 모든 orb 흡수 후에만
  재사용 가능) — 별도 카운터 없음.

**중복 스폰 가드 (ghost 방지):** `S_Enter`와 `S_NpcSpawnBatch`(영역 스트리밍)가 같은 npc를 중복
나열할 수 있다. 가드 없이 `createGoblin/Snake/Mushroom`이 또 생성하면, 이전 객체가 `goblins_`에
남아 **렌더는 되지만 `idMonsterMap_`은 새 객체를 가리켜**, 이전 객체가 이동·피격을 전혀 못 받는
**ghost**가 된다(제자리 동결·피격 무반응). 각 `create*`는 `idMonsterMap_`에 이미 있으면 생성을
스킵한다(시체는 맵에 없으므로 죽은 npc의 정상 리스폰은 그대로 새 객체 생성). 리스폰은 `S_NpcRespawn`
→ `onNpcRespawn`(풀 재사용)이 담당.

**사망~리스폰 윈도우의 stray 패킷:** 시체로 이관하면 npcId가 `idMonsterMap_`에서 빠지므로,
서버가 사망~리스폰 사이에 그 npcId로 보내는 in-flight 패킷이 `moveGoblin`/`onNpcAttack`에서
"NPC not found" 에러를 낸다(시체 시스템 전에는 죽은 몬스터가 맵에 남아 `isDead()`로 조용히
무시됐던 것의 회귀). 수정: `detachedNpcIds_`(현재 시체로 분리된 npcId 집합)에 migrate 시 insert,
`onNpcRespawn` 진입 시 erase. 핸들러는 not-found일 때 이 집합에 있으면 **조용히 무시**, 없으면
실제 에러로 로깅.

**풀링:** 같은 오브젝트의 ID만 바꿔 재사용하면 서버 리스폰 시 쓸 오브젝트가 부족해진다.
그래서 종류별 풀(`goblinPool_`/`snakePool_`/`mushroomPool_`)을 두고, 리스폰은 풀에서 꺼내
재초기화(`reinitFromPool`)한다. `monsterSpawnInfo_`/`respawnKind_`로 스폰 정보·종류를 보존한다.
시체→풀 반환 시 HP 바도 함께 이동.

**`updateCorpses(dt, tPhysicInterp)` 2-페이즈:**
- **Ragdoll**(`kRagdollSeconds=2.5s`): 래그돌 물리를 유지(차밍 포인트). 순서 중요 —
  `ragdoll->syncToFinalXforms` → `Object::update`(여기서 디버그 BV `worldBVs`를 **래그돌 포즈**로
  재계산) → `rebuildBodyBVH`. update를 sync보다 먼저 부르면 BV가 직전 애니메이션 포즈로 계산돼
  메시/물리(래그돌)와 어긋난다. 2.5s 경과 시 `spawnFromMonster`로 오브 생성 + 래그돌 비활성화 후
  Orb 페이즈로 전환.
- **Orb**: 오브가 모두 흡수될 때까지 대기(`hasActiveOrbs`). 끝나면 `returnMonsterToPool` 후 시체 제거.

**EnergyOrbSystem 라이프사이클:** `spawnFromMonster(model, finalXforms, objWorld, totalCharge,
slot, corpseId)` — 스키닝된 서브메시마다 1 오브, `totalCharge`를 오브 수로 N분할. 각 오브 상태머신
`Forming`(morphT 0→1) → `Tracking`(가속 추적, 접근 시 응축) → `Absorbing` → `Dead`. `update(dt,
playerPos)`가 추적/흡수를 진행하고, 흡수 순간 `onAbsorb(orb)` 콜백 호출.

**charge 매칭(신규 패킷 없음):** charge는 서버 권위(`S_SkillCharge`)지만 HUD는 흡수에 맞춰
점진적으로 채운다.
- `onSkillCharge`에서 `delta = charge - prev[slot] > 0`이면 `pendingOrbCharges_`에 push(데미지
  기여자 전원 로컬 연출).
- `updateCorpses`가 pending charge를 **가장 최근의 미충전 Ragdoll 시체**와 시간창으로 매칭해 그
  시체의 `totalCharge`로 확정(매칭 실패 시 HUD만 즉시 채우는 안전 폴백).
- HUD는 `targetCharge`(서버 확정)와 `displayCharge`(표시)를 분리(`skillDialHUD`):
  `onSkillCharge`는 target만 올리고, `onAbsorb`마다 `addDisplayCharge`로 display를 한 칸씩 채운다.

**onAbsorb 처리:** ① `skillDial_.addDisplayCharge(slot, chargePerOrb)` — HUD 점진 채움. ② `player_->
addBodyRipple(contactPoint, color)` — 흡수 물결. 단, 오브 HDR 색을 그대로 쓰면 GB2 UNORM 클램프로
풀 채도가 되어 산만하므로 **정규화+탈채도+강도 하향**으로 부드럽게 보정해 전달.

**알려진 한계:** 비-기여자(0 charge) 오브도 시각적으로 흡수됨; hobgoblin이 goblin 풀 공유(드문
모델 혼재); 풀에 든 비활성 오브젝트의 animBlender는 계속 tick.

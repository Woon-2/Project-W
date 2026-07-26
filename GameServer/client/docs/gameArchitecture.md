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
     **⚠ 과거 결함 2건 — 2026-07-27 수정 완료.** 원격 플레이어·보스가 끊겨 보이던 원인이다.

     **① `setOrient`가 위치 보간 세그먼트를 파괴했다(근본 원인).** `Object::setOrient`는
     `body_.snapToCurrent()`를 부르는데 이건 orient만이 아니라 **`BodyState` 통째(위치 포함)**
     `prev_ = curr_`다. `moveGoblin`이 `setCurrPos` **뒤에** `setOrient`를 불러, 방금 세팅한
     세그먼트가 그 자리에서 지워졌다 → `lerp(prev,curr,t)`가 항상 새 위치를 돌려줘
     **몬스터·보스는 보간이 아니라 매 패킷 순간이동**했다(`netInterpAcc_`/`tNet`은 죽은 코드였다).
     원격 플레이어도 `rotatePlayer`가 같은 함수를 썼고 `C_MouseMove`가 매 프레임 나가 상시 무효화.
     → **`Object::setCurrOrient()` 신설**(`setOrient`에서 `snapToCurrent()`만 제외). 수신 경로 2곳 전환.
     `setOrient`의 스냅은 텔레포트·초기 배치에서는 올바르므로 그대로 남겼다.

     **② 보간 창이 실제 도착 간격과 불일치했다.** 창이 길면(50ms 창 + 16.7ms 도착) `t`가 1에 못
     미친 채 리셋돼 세그먼트 시작점으로 되튀고, 짧으면(50ms 창 + 66.7ms 도착) 먼저 도착해놓고
     남은 시간 정지한다 — 양쪽 다 끊김이다. → `netInterpDuration_`을 고정 상수에서
     **실측 도착 간격의 지수 평활값**으로 바꿨다(`Object::noteNetArrival()`, clamp `[1/90s, 0.2s]`).
     서버 송신 레이트가 바뀌어도 자동으로 맞는다. 정지 판정은 `netStale()`이 100ms 하한을 둔다.
     함께 서버 `S_NpcMoveBatch`를 **20Hz로 스로틀**했다(`Room::kNpcMoveBroadcastPeriodTicks`) —
     대역폭 1/3(242마리 ≒ 10KB 패킷이 60Hz면 클라당 약 5MB/s로 MTU를 넘겨 TCP 단편화 자체가
     지터원이었다). `RoomServer/docs/roomTickCadence.md` §7-2.
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

### 로코모션 재생 배속 (발 미끄러짐 저감)

클립은 전부 제자리(in-place)다 — root motion도 foot IK도 없다. 따라서 발 접지는
**"클립이 만들어내는 보폭 속도"와 "실제 이동 속도"가 얼마나 맞는가**로만 결정된다.

#### 왜 기존 구조가 중저속에서는 그럭저럭 맞았나

이 시스템은 원래 속도를 **블렌드 가중치**로만 표현했다. idle 포즈는 발이 고정이므로
블렌딩된 포즈의 보폭은 대략 `w × (클립 기준 보속)`에 비례한다. 그리고 `w ≈ speed / 밴드끝`이다.
즉 **"기준 속도 ≈ 블렌드 밴드 끝값"이 암묵적으로 인코딩**되어 있었고, 밴드 안에서는 대충 맞았다.

깨지는 곳은 밴드가 포화한 위쪽이다.

| 대상 | 밴드 끝 | 실제 최대 속도 | 결과 |
|---|---|---|---|
| 플레이어 | 5.1 m/s | 10 (`kPlayerMaxSpeed`), F8 부스트 시 ×5 | 5.1 위로는 애니메이션이 완전히 동일한 채 최대 2배 빨리 미끄러짐 |
| 전술 NPC | 3.06 m/s | `moveSpeed × TACTICAL_SPEED_MULT(3.0)` = 최대 12 | 약 4배 미끄러짐 |
| 보스 | walk 1.56 / run 5.0 | 3.5 | walk↔run 크로스페이드 구간 |

#### 수식

```
rate = clamp( speedXZ / (refSpeed × max(locoWeight, 0.05)), 0.35, 2.0 )   // + 지수 평활 τ=0.1s
```

**`locoWeight`로 나누는 것이 핵심이다.** 블렌드 밴드는 유지하기로 했으므로(중저속 룩 보존),
가중치가 이미 깎아놓은 보폭을 배속이 되돌려줘야 한다. `speed / refSpeed`를 그대로 쓰면
2.5 m/s에서 이미 `w≈0.49`(포즈 절반이 idle)인데 배속까지 0.55배로 낮춰 **지금보다 더 미끄러진다.**

밴드 안에서는 `locoWeight ≈ speed/밴드끝`, `refSpeed ≈ 밴드끝`이라 이 값이 **1 근처로 수렴**한다
(= 기존 룩 보존, 회귀 판정 기준). 가중치가 포화한 뒤부터 `speed/refSpeed`가 그대로 붙는다.

- **중간 clamp 금지.** `strideRate`를 먼저 clamp하고 그 결과를 가중치로 나누면 clamp 오차가
  `1/locoWeight`로 증폭되어 저속 구간이 종전보다 나빠진다. 한 번에 나누고 한 번만 clamp한다.
- **`kMinWeight`는 작게(0.05).** 크게 잡으면 저속에서 보정이 모자란다. 폭주 방어는 최종 clamp가 한다.
- **범위 `[0.25, 2.0]`.** 상한은 플레이어 `10/5 = 2.0`을 딱 담는 값이고, 하한은 Treant(0.34)를
  담기 위해 내린 값이다. **클램프에 걸리면 지정한 refSpeed가 무력화되므로, 값을 조정할 때마다
  밴드 내 배속(`밴드끝/refSpeed`)이 이 범위 안인지 확인할 것.**
  전술 NPC(`moveSpeed × 3.0`)는 상한에 걸리며,
  **남는 미끄러짐은 배속이 아니라 run 클립으로 풀 문제**다(현재 walk 클립만 있음).
- **평활은 필수.** 원격 플레이어·몬스터의 속도는 물리가 아니라 20Hz 서버 패킷 값이다
  (`movePlayer`/`moveGoblin`). 평활 없이는 배속이 패킷 주기로 계단진다.

#### `locoWeight`에 상하체 마스크를 반영한다

다리가 실제로 드러내는 가중치는 `tRun`/`tWalk_`가 아니다.

| 대상 | `locoWeight` | 근거 |
|---|---|---|
| 플레이어 | `tRun × (1 - tAttack_ × tIdle_)` | 하체(mask=0)의 공격 오버레이 비중이 `tAttack_ × tIdle_` (`aimPitchUpperBodyMask.md`). 고속 이동 중(tIdle_→0)에는 이 항이 사라져 다리는 순수 run |
| 몬스터·보스 | `t이동 × (1 - tAttack_)` | 마스크가 없어 공격 오버레이가 전 본에 걸린다 |

그래서 배속 계산은 **`tAttack_`이 확정된 뒤**(공격 오버레이 처리 다음)에 와야 한다.

`tHit_`(최대 0.75)·`tDeath_`도 마스크 없이 보폭을 깎지만 **보정 대상에서 제외**한다 —
넉백·입력잠금 상태라 접지 정확도가 의미 없고, 비틀거리는 포즈 위에서 다리만 빨리 돌리면 어색하다.

#### 기준 속도 — 값은 "엔티티"가 아니라 "클립셋"의 속성이다

같은 클립셋을 쓰는 변종은 `moveSpeed`가 달라도 **같은 기준 속도**를 써야 한다.
서버 `Room::setupTacticalNpc*`의 objType 스위치가 그 대응을 정의한다:
**Hobgoblin→Goblin, Grandbaum→Treant, Isys→Birdy** (모델만 다르고 anim은 공유).

클라는 `AnimClip`에 필드로 넣지 않는다. 클립은 추출 바이너리에서 로드되어
`shared_ptr<const AnimClip>`로 공유되므로 필드 추가는 포맷 변경 + 전 에셋 재추출을 부른다.
클립 이름을 아는 쪽은 각 블렌더이므로 **각 블렌더 `update()`의 지역 `constexpr`**로 둔다.

**refSpeed는 반비례 레버다 — 값을 올리면 애니메이션이 느려진다.**
밴드 안 정상상태 배속은 대략 `밴드끝 / refSpeed`이므로, "이 캐릭터 걸음을 느리게"는 refSpeed를 **올리는** 것이다.

| 클립셋 | refSpeed | 밴드끝 | 밴드 내 배속 | 클라 (`client/object.cpp`) | 서버 |
|---|---|---|---|---|---|
| Player run | 5.0 | 5.10 | 1.02× | `AnimBlenderPlayer::kRefSpeedRun` | `Room::setupPlayerAnimClips` |
| Mushroom | **4.5** | 3.06 | **0.68×** | `AnimBlenderMushroom` | `Mushroom::applyMushroomConfig` |
| Treant (+Grandbaum) | **7.8** | 3.06 | **0.39×** | `AnimBlenderTreant` | `Treant::applyTreantConfig`, `TacticalTreant::trooperConfig`, `Room.cpp` Grandbaum bossCfg |
| Goblin(+Hobgoblin) / Snake / Slime / Birdy(+Isys) / Bomber | 3.0 (미측정) | 3.06 | 1.02× | 각 블렌더 | `apply*Config`, `Tactical*::trooper/bossConfig`, `Room.cpp` Isys bossCfg |
| FinalBoss (walk **4.8** / run **9.6**) | 4.0~7.0 | gait별 독립 | 걷기 **0.729×** / 질주 **0.911×** | `AnimBlenderBoss::kRefSpeedWalk`·`kRefSpeedRun` | `FinalBoss::applyBossConfig`(근사 9.6) |

**클라·서버 값이 어긋나면 피격 BVH가 화면과 안 맞는다. 한쪽만 바꾸지 말 것.**
서버는 `NpcConfig::animRefSpeed`+`animBandEnd` / `TacticalNpcConfig`의 같은 두 필드로 config 주도이며,
새 몬스터를 추가하면 `applyXXXConfig`에서 명시적으로 지정한다(기본값에 기대지 말 것).

**클램프가 의도한 배율을 삼키지 않는지 확인할 것.** 하한 0.25는 Treant(0.39)를 담기 위한 값이다 —
0.35였을 때 Treant는 전 속도 구간에서 상시 클램프되어 지정한 값이 무력화됐다.
상한 2.0은 플레이어(10/5)를 담는 값이며, 전술 NPC(`moveSpeed × 3.0`)는 여전히 여기 걸린다.

> **함정 — 클램프 위에서는 refSpeed의 방향이 뒤집힌다.**
> 가중치가 포화한 뒤(고속) 실제 발 속도는 `rate × refSpeed`인데, rate가 상한에 걸려 고정되면
> 발 속도 = `kMaxRate × refSpeed`가 된다. 즉 **refSpeed를 낮추면 발이 오히려 더 느려진다.**
> 전술 고블린(12 m/s)에서 refSpeed를 3.0 → 2.5로 낮췄더니 발 속도가 6.0 → 5.0 m/s로 떨어져
> 미끄러짐이 악화된 사례가 있다. 클램프 구간의 미끄러짐은 refSpeed가 아니라
> `kMaxRate`(또는 run 클립 추가)로 풀어야 한다.

#### 예외: 보스는 클립별이 아니라 **가중 블렌드된 기준 속도**를 쓴다

§1의 가중치 나눗셈은 **로코모션 클립이 idle과 섞인다**는 전제 위에 있다. idle은 발이 고정된
포즈라 블렌딩된 보폭이 정확히 `w × 클립보폭`이고, 그래서 `w`로 되나누는 것이 옳다.

보스는 walk와 run을 서로 크로스페이드한다 — **양쪽 다 보폭을 가진다.** 여기서 각 클립을
자기 가중치로 나누면 이중 보정이 되어, 50/50 구간에서 walk 클립이 4.7배를 요구하고 상한에
박혀버렸다. 보스 발놀림이 경박해 보이던 원인이 이것이다.

```cpp
// AnimBlenderBoss::update — 두 클립이 하나의 배속을 공유한다
refBlended = (1 - tRunBand)·kRefSpeedWalk + tRunBand·kRefSpeedRun
locoRate_  = solve(speedXZ, refBlended, tMove · (1 - tAttack_))
```

가중치로 나누는 대상은 **전체 로코모션 가중치(tMove)** 하나뿐이다 — 보폭 없는 파트너는 idle뿐이므로.
배속을 공유하므로 크로스페이드 구간에서 두 클립의 케이던스도 어긋나지 않는다.

수정 전후(보스, 걷기 3.5 m/s): walk 2.00× / run 0.76×(가중평균 1.38) → **0.729× 단일 배속**.

##### 보스의 걷기/질주 — 배속이 아니라 **이동 속도**로 만든다

클라는 클립을 속도로 추론하므로(run 밴드 2.0~5.0), 보스가 "달려 보이게" 하려면
재생 배속이 아니라 **실제 이동 속도**가 그 밴드를 넘어야 한다.
BT의 `BossChaseAction`이 거리로 gait를 고른다(`FinalBoss::updateChaseGait`, 히스테리시스 6.0/4.5m):

| gait | 속도 | 클라 블렌드 | 배속 |
|---|---|---|---|
| 걷기 | `moveSpeed` 3.5 | walk 1.00 | 0.729× |
| 질주 | `× RUN_SPEED_MULT(2.5)` = 8.75 | run 1.00 | 0.911× |

**run 밴드는 두 gait 사이에 놓아야 한다(4.0~7.0).** 종전 2.0~5.0은 걷기 속도 3.5를 밴드 한가운데
(`tRunBand=0.5`)에 두어, 걷기 배속의 절반이 `kRefSpeedRun`에서 나왔다 — 상수는 walk/run으로 분리돼
있어도 **블렌드를 통해 결합**되므로, run 발 속도만 낮추면 걷기까지 같이 느려진다(0.729 → 0.583).
밴드를 두 gait 사이로 올리면 각 gait가 자기 클립 하나만 읽어 완전히 독립된다.

> 종전에는 `switchClip("Run")`만 하고 속도는 계속 `moveSpeed`였다 — 서버 본만 run 포즈였고
> 클라는 3.5 m/s를 보고 walk/run을 반반 섞었다. **클립 전환은 서버 본(피격 BVH)용이고,
> 클라의 gait를 바꾸는 것은 브로드캐스트되는 속도다.** 둘을 같이 움직여야 한다.

> **서버 근사:** 보스는 3-way 블렌드라 `max(bandEnd, speed)/refSpeed` 유도가 정확하지 않다.
> `refSpeed=9.6`, `bandEnd=7.0`은 **두 gait 모두에서 정확히 일치**하도록 고른 값이다
> (3.5 → 0.729, 8.75 → 0.911, 오차 0). 그 사이 구간에서만 어긋나며,
> 공격 중에는 서버가 attack 클립으로 전환하므로 히트 판정 구간은 이 근사의 영향을 받지 않는다.

#### 서버 미러링

서버는 단일 클립을 재생하고 그 본 변환으로 피격 BVH를 만든다(`Object::updateAnimBones`).
클라만 배속을 바꾸면 걷는 몬스터의 팔·다리 피격 위치가 화면과 어긋난다.

서버가 맞춰야 하는 것은 **클립 위상**이다(양쪽이 같은 클립을 돌린다). 서버에는 블렌드 가중치가
없지만, 그 가중치는 속력만의 함수이므로 대입하면 가중치가 통째로 소거된다:

```
w = clamp((speed - bandStart) / (bandEnd - bandStart), 0, 1) ≈ min(speed / bandEnd, 1)
  ⇒ speed / (refSpeed · w) = max(bandEnd, speed) / refSpeed
```

```cpp
// RoomServer/serverAnimation.cpp
ServerAnimState::locomotionRate = clamp(max(bandEnd, speedXZ) / refSpeed, 0.25, 2.0)
```

플레이어·몬스터 블렌더에 대해 **1.5 m/s 이상에서는 클라와 정확히 일치**하고, 그 아래에서만
`bandStart`(0.03~0.05)를 생략한 만큼 최대 6% 어긋난다.
`speed/refSpeed`로 단순화하면 **안 된다** — 그건 `refSpeed ≈ bandEnd`일 때만 성립했고,
지금은 최대 3배까지 벌어져 있다(Treant refSpeed 9.0 vs bandEnd 3.06).
보스는 3-way 블렌드(idle/walk/run)라 이 유도가 정확하지 않다 — run 밴드 끝(5.0)에서만 일치하고
크로스페이드 구간(2~5 m/s)에서는 어긋난다.

- 배속은 `Object::updateAnimBones`의 **단일 진입점**에서 매 프레임 설정한다.
  `AnimController::isPlayingLocomotion()`(등록 시 캐시한 포인터 비교)이 true일 때만 적용하고,
  공격/피격/사망 클립은 항상 1x다 — 스킬 타임라인과 히트 윈도우가 실시간 재생을 전제한다.
- `AnimController::switchClip`은 같은 포인터면 early-return하므로 **배속은 반드시 `setPlaybackRate`로** 설정한다.
- 기준 속도·밴드 끝은 `NpcConfig` / `TacticalNpcConfig`의 `animRefSpeed`·`animBandEnd`(`moveSpeed` 옆),
  플레이어는 `Room::setupPlayerAnimClips`, 보스는 `FinalBoss::applyBossConfig`.

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
- **Ragdoll**(`kRagdollSeconds` = **2.0s**): 래그돌 물리를 유지(차밍 포인트). 순서 중요 —
  `ragdoll->syncToFinalXforms(..., tPhysicInterp)` → `Object::update`(여기서 디버그 BV `worldBVs`를
  **래그돌 포즈**로 재계산) → `rebuildBodyBVH`. update를 sync보다 먼저 부르면 BV가 직전 애니메이션
  포즈로 계산돼 메시/물리(래그돌)와 어긋난다. 경과 시 `spawnFromMonster`로 오브 생성 + 래그돌
  비활성화 후 Orb 페이즈로 전환.
- **Orb**: 오브가 모두 흡수될 때까지 대기(`hasActiveOrbs`). 끝나면 `returnMonsterToPool` 후 시체 제거.

**EnergyOrbSystem 라이프사이클:** `spawnFromMonster(model, finalXforms, objWorld, totalCharge,
slot, corpseId)` — 스키닝된 서브메시마다 1 오브, `totalCharge`를 오브 수로 N분할. 각 오브 상태머신
`Forming`(morphT 0→1) → `Tracking`(가속 추적, 접근 시 응축) → `Absorbing` → `Dead`. `update(dt,
playerPos)`가 추적/흡수를 진행하고, 흡수 순간 `onAbsorb(orb)` 콜백 호출.

**연출 시간 예산 (래그돌 ↔ 오브는 제로섬):** 래그돌 물리가 이 프로젝트의 시연 포인트이므로
래그돌 구간을 늘리고 오브 구간을 그만큼 줄인다 — **총 흡수 시간은 유지하면서 붕괴만 더 길게**
보인다. 플레이어까지 10m 기준:

| 구간 | 상수 | 위치 | 값 | 소요 |
|---|---|---|---|---|
| 래그돌 유지 | `kRagdollSeconds` | `onlineGame.cpp` `updateCorpses` | 2.0 | 2.00s |
| Forming(정지 모핑) | `kFormingTime` | `energyOrbSystem.cpp` | 0.90 | 0.90s |
| Tracking | `kStartSpeed` / `kAccel` / `kMaxSpeed` | 〃 | 8 / 45 / 34 | ≈0.50s |
| Absorbing | `kAbsorbTime` | 〃 | 0.18 | 0.18s |
| | | | | **≈3.58s** |

`kFormingTime`은 오브가 **완전히 정지**해 있는 구간이라 체감 지연을 지배한다 — 오브 구간을
줄일 때 여기부터 손대는 게 맞다(속도 상수를 올리는 것보다 효과가 크고 거리에 무관).
래그돌 쪽 노브 목록은 `ragdollSafety.md` "래그돌 연출 노브".

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

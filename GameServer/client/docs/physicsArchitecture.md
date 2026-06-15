### 물리 아키텍처 (Phase 1–8 완료)

연관 파일: `rigidBody.hpp/cpp`, `physicsWorld.hpp/cpp`, `constraint.hpp`,
`contactConstraint.hpp/cpp`, `staticDepenetration.hpp/cpp`, `broadPhase.hpp/cpp`,
`collision.hpp/cpp`, `object.hpp/cpp`, `jointConstraint.hpp/cpp`, `ragdollDef.hpp/cpp`,
`ragdoll.hpp/cpp`, `activeRagdoll.hpp/cpp`, `../common/slotVector.hpp`, `../common/scatterTransform.hpp`

---

## 클래스 구조

```
PhysicsWorld
 ├─ std::vector<Entry>             // { RigidBody*, onRebuildBVH 콜백, collisionGroup, collisionMask }
 ├─ SAPBroadPhase broadPhase_      // X축 Sort-and-Sweep, O(n log n) — 일반 body-body 충돌
 ├─ SAPBroadPhase cameraBroadPhase_// 카메라 장애물 전용 독립 broad phase
 ├─ SlotVector<unique_ptr<WorldCollider>> worldColliders_ // 정적 환경 충돌(지형 + scatter prop), BroadPhase 우회
 ├─ std::vector<ContactConstraint> // 매 step 재생성 (Dynamic-Dynamic + Terrain support)
 ├─ std::vector<StaticContact>     // 매 step 재생성 (Static depenetration: broad-phase static body + scatter prop)
 ├─ std::vector<unique_ptr<Constraint>> jointConstraints_  // 소유 joint (독립 사용)
 └─ std::vector<Constraint*>       jointRefs_              // 비소유 ref (Ragdoll용)

RigidBody
 ├─ BodyState curr, prev           // 더블 버퍼 (렌더 보간용)
 ├─ MotionType: Dynamic/Kinematic/Static
 ├─ BVH worldBVH_
 └─ Dynamic 전용: invMass, invInertiaLocal/World, forceAccum, torqueAccum,
                  linearDamping, angularDamping, restitution, friction

ContactConstraint : Constraint     // PGS velocity (Normal + Coulomb friction) + Baumgarte
BallSocketJoint   : Constraint     // 3 translational DOF 제거, bilateral warmstart
HingeJoint        : Constraint     // 1 rotational DOF + optional angle limits, refOrient
ConeTwistJoint    : Constraint     // swing cone + twist limits, T-pose refOrient

Ragdoll
 ├─ std::vector<RagdollBone>       // { boneIdx, body*(비소유), parentJoint*(비소유), capsuleOffset }
 ├─ std::vector<unique_ptr<RigidBody>>   bodies_  // 소유
 └─ std::vector<unique_ptr<Constraint>> joints_  // 소유

Object
 ├─ RigidBody body_                // 인라인 (항상 유효)
 └─ unique_ptr<Ragdoll> ragdoll_   // null = 비활성
```

`Object`는 `RigidBody body_`를 인라인으로, `Ragdoll` (활성 시)을 `unique_ptr`로 소유한다.
`PhysicsWorld`는 모든 포인터를 비소유로 참조 (등록/해제 패턴).

**충돌 필터링:** `Entry::collisionGroup` + `collisionMask` 비트필드.
조건: `(a.group & b.mask) != 0 && (b.group & a.mask) != 0` 일 때만 충돌 처리.
Ragdoll 뼈대: group=2, mask=0xFFFD → 뼈대끼리 self-collision 필터링.

---

## PhysicsWorld::step() 흐름

```
step(dt)
    ├─ advanceState()              — BodyState prev ← curr (렌더 보간용, 전체 step 1회만)
    └─ [sub-step × subStepCount_ (기본 2)]:
        subDt = dt / subStepCount_
        ├─ clearPseudoVelocities() — split impulse 누적값 초기화
        ├─ integrate(subDt)
        │   ├─ Kinematic: damping → vel/omega snap → pos/orient 적분
        │   ├─ Dynamic:   damping → force/torque 적분 → pos/orient 적분 → clearAccumulators()
        │   └─ 각 body: onRebuildBVH 콜백 → worldBVH 재빌드
        ├─ generateContacts()
        │   ├─ broadPhase_->update() + queryPairs()
        │   ├─ collides(bvhA, bvhB) → leaf-leaf 쌍에서만 정밀 판정
        │   ├─ static 포함 쌍(aStatic != bStatic): StaticContact로 라우팅 후 continue (ContactConstraint 미생성)
        │   │   └─ 법선을 static→movable로 정규화 (res.normal B→A; movable==b면 부호 반전, degenerate 시 center-to-center)
        │   ├─ Dynamic-Dynamic: ContactConstraint 생성 (법선 = res.normal; degenerate 시 center-to-center fallback)
        │   │   └─ cc->setExternalAccels(gravA, gravB): Dynamic=gravity_, Static=(0,0,0)
        │   └─ WorldCollider 순회: Dynamic body마다 worldColliders_(지형·scatter) 질의 → ContactSink
        │       ├─ TerrainCollider → 지형 support ContactConstraint (cc->setExternalAccels(gravity_, {0,0,0}))
        │       └─ ScatterCollider → prop push-out StaticContact (staticContacts_에 append)
        ├─ solveConstraints(subDt)
        │   ├─ prepare(subDt) : rA/rB, tangent frame, effMass, Baumgarte bias 계산
        │   │                  + joint warmstart (accImpulse 이전 값 재적용)
        │   ├─ velocity solve × solverIterations_ (기본 10, ragdoll 활성 시 20)
        │   │   └─ Contact + BallSocket/Hinge/ConeTwist 혼합 solveVelocity()
        │   ├─ joint velocity extra × jointSolverExtraIterations_
        │   │   └─ joint 전용 solveVelocity() (contact 제외) — 분기 많은 ragdoll 체인 안정화
        │   ├─ position solve × positionSolveIterations_ (기본 3)
        │   │   └─ joint solvePosition() — split impulse (pseudoBias/pseudoAccImpulse)
        │   └─ warmstart 저장 (accImpulse)
        ├─ applyPseudoVelocity(subDt) — split impulse 결과를 위치에 적분
        └─ resolveStaticPenetration(staticContacts_, moved) — static 침투 해소 (broad-phase static body + scatter prop; 마지막, 위치 최종 결정권)
            └─ 직접 이동한 body만 onRebuildBVH 재호출 (dirty set; 다음 sub-step narrow phase가 보정 위치를 봄)
```

> Phase 4(TerrainCollider) 완료로 중력이 활성화됨. (`physicsWorld.cpp` integrate)

---

## Joint Constraint 설계 원칙 (Phase 6)

**공통 규약:**
- `kJointBeta = 0.1f` (joint 전용 velocity-level Baumgarte; ContactConstraint는 `kBaumgarteBeta = 0.2f` + `kSplitImpulseBeta = 0.3f` 조합으로 침투 회복)
- kSlop 없음 — joint는 위치 오차가 0이어야 함
- 모든 joint에 warmstart (`accImpulse` 누적값 유지)
- row-vector convention: `K_ij = (invMA+invMB)*δij + dot(rA×ei, (rA×ej)*iA) + dot(rB×ei, (rB×ej)*iB)`

**BallSocketJoint**: 3×3 K matrix inversion, bilateral (clamp 없음)

**HingeJoint**:
- 3 translational rows (BallSocket 동일) + 2 angular alignment rows (hinge 수직축)
- limit row (one-sided): `refOrient_ = conj(bodyA.orient) * bodyB.orient` at build time
- limit clamp: lo 위반 → `accImp >= 0`, hi 위반 → `accImp <= 0`

**ConeTwistJoint**:
- 3 translational rows + swing cone row (one-sided, `coneAccImp >= 0`)
- twist row (bilateral, clamped to `[-twistLimit, +twistLimit]`)
- **상대 회전 `qRel` 구성 (frame 일관성 핵심):**
  - `qRest = ~refOrientA * refOrientB` — 정지 시 A body 프레임에서 본 B의 상대 orientation (상수)
  - `qCur  = ~bodyA.orient * bodyB.orient` — 현재 A body 프레임에서 본 B의 상대 orientation
  - `qRel  = ~qRest * qCur` — rest로부터의 편차 (정지 시 identity). **HingeJoint::prepare와 동일 구성**이며 Bullet `btConeTwistConstraint`처럼 rest 오프셋을 *상수*로 분리한다.
  - ⚠️ `deltaA⁻¹·deltaB`(= `~(~refA·qA) · (~refB·qB)`) 형태로 쓰면 refA≠refB일 때 두 rest 프레임이 섞여, bodyA가 dress 자세에서 회전할수록 swing/twist 위반을 허위 보고하고 실제 twist가 swing으로 새어 twist 제한이 무력화된다. 반드시 위 `~qRest * qCur` 형태를 유지할 것.
- twist/cone world 축은 `bodyA.orient.rotate(...)`로 변환 — `qRel`이 A body 프레임에 존재하므로 일관됨. twist 축 = `bodyA.orient.rotate(twistAxisLocalA)`(부모 body-local 본 방향), cone 축 = `bodyA.orient.rotate(swing.xyz)`(twist 축과 직교).
- `swingTwistDecompose`: twist = q 성분 twistAxis 방향 투영, swing = q * conj(twist)
- `coneHalfAngle` max = `pi * 0.85f` (gimbal lock 방지)

---

## CFM (Constraint Force Mixing)

수치 안정성을 위해 effective mass 행렬의 대각선에 CFM 값을 더한다.
`Constraint` 기반 클래스 static 상수로 정의:

```cpp
static constexpr float linearCFM  = 1e-3f;  // 3×3 K 행렬 대각선에 가산
static constexpr float angularCFM = 1e-2f;  // 각 angular DOF effMass 분모에 가산
```

- `linearCFM`: `build3x3EffMassInv()` 내부에서 `k00 += linearCFM`, `k11 += linearCFM`, `k22 += linearCFM`
- `angularCFM`: `cache_.angEffMass[i] = 1.f / (contribA + contribB + angularCFM)`

ContactConstraint는 별도 CFM을 사용하지 않는다. contact 침투는 velocity-level Baumgarte(`kBaumgarteBeta=0.2`, body-body 한정·부드러운 분산 보정)와 position-level split impulse(`kSplitImpulseBeta=0.3`, 에너지 중립·튐 방지)의 **조합**으로 회복한다. 외력(중력) 보상 extComp는 **두 채널에 분배**된다(`kExtCompVelFrac`로 velocity:split 비율 결정).

---

## Per-Constraint Damping

각 joint 타입이 독립적으로 damping 계수를 보유한다.
`prepare()` 호출 시 velocity 목표에서 현재 상대속도의 일부를 빼 속도 오차가 0 수렴 속도를 줄인다.

| Joint | 필드 | 기본값 |
|-------|------|--------|
| BallSocketJoint | `damping_` | 0.1f |
| HingeJoint | `linearDamping_`, `angularDamping_` | 별도 설정 |
| ConeTwistJoint | `linearDamping_`, `coneDamping_`, `twistDamping_` | 0.1f |

damping이 0이면 joint는 오차를 완전히 해소하려는 강한 impulse를 생성한다.
ragdoll 자연스러운 움직임을 위해 적절한 damping이 필요하다.

**ConeTwistJoint angular damping (always on):** `solveVelocity()`에서 상대 각속도
`ωrel = ωA - ωB`를 twist 축(`cache_.twistAxis`, 한계 활성 여부와 무관하게 매 prepare에서
설정)에 대해 분해한다.
- twist 성분 `dot(ωrel, tAxis)` → `twistDamping_`로 감쇠
- swing 성분 `ωrel - dot(ωrel,tAxis)·tAxis`(twist 축에 수직인 평면) → `coneDamping_`로 감쇠
  (순간 swing 축에 투영). **cone 한계 내부에서도 항상 적용**되어 관절이 한계까지 자유
  회전하지 않고 묵직하게 움직인다.
- effective mass는 cache가 아니라 매번 `angEff1D(axis, invInertiaWorld)`로 재계산한다.
  `cone/twistEffMass` 캐시 필드는 해당 한계가 active일 때만 채워지므로 감쇠에 쓰면 stale.

---

## SOR (Successive Over-Relaxation)

angular row에 `kAngSOR = 0.6f`를 impulse에 곱해 over-shoot을 방지한다.

```cpp
static constexpr float kAngSOR = 0.6f;
// solveVelocity() 내부:
deltaAng *= kAngSOR;  // angular impulse 일부만 적용
```

분기가 많은 ragdoll 체인에서 angular impulse의 연쇄 overshoot(폭발)을 억제한다.
linear row에는 SOR 미적용 (translational 수렴이 더 안정적).

---

## Split Impulse (Position Solve)

velocity-level Baumgarte bias만으로는 constraint 위치 오차(drift)가 에너지를 추가한다.
Split impulse는 위치 수정용 pseudo-velocity를 별도로 누적해 에너지 추가 없이 drift를 줄인다.

**각 joint Cache에 별도 필드:**
```cpp
mu::Vec3 pseudoBias;       // 위치 오차에서 계산한 목표 pseudo-velocity
float    pseudoAccImpulse; // 누적 pseudo-impulse (sub-step마다 리셋)
```

**흐름:**
1. `prepare()`: `pseudoBias` 계산 (`kSplitBeta * invDt * positionError`)
2. `solvePosition()` × `positionSolveIterations_` (기본 3): pseudo-velocity만 갱신
3. `applyPseudoVelocity(subDt)`: pseudo-velocity를 위치에 적분 (`pos += pseudoVel * subDt`)
   — 실제 velocity에는 영향 없음

**ConeTwistJoint beta 상수:**
```cpp
static constexpr float kLinBeta   = 0.1f;   // linear Baumgarte
static constexpr float kAngBeta   = 0.025f; // angular velocity-level (conservative)
static constexpr float kSplitBeta = 0.3f;   // position-level (split impulse)
```

ContactConstraint의 침투 회복은 **Baumgarte + split impulse 조합**이다.
- velocity-level Baumgarte (`bias = kBaumgarteBeta(0.2) * invDt * penetration`): 실제 분리속도를
  주입해 침투를 여러 프레임에 걸쳐 부드럽게 빼낸다(팝 없음, 약간의 과분리 가능). **body-body 한정**
  — terrain은 0(주입 속도가 지면을 잠깐 이탈시켜 진동 유발).
- position-level split impulse (`solvePosition()`, `pseudoBias = kSplitImpulseBeta(0.3) * invDt * penetration + extCompSplit`):
  pseudo-velocity로 위치만 보정, 에너지 중립. 높은 beta는 한 step에 침투를 즉시 스냅해 깊은 침투에서
  "튀어 보임" → 0.3으로 낮추고 Baumgarte가 나머지를 부드럽게 분산.
- **외력(중력) 보상 extComp는 두 채널에 분배된다**: `extCompVel = kExtCompVelFrac·extComp`(velocity bias),
  `extCompSplit = extComp − extCompVel`(split). 둘 다 외력을 고려하되 합이 정확히 extComp라 integrate의
  중력 sink와 상쇄되어 정지 body가 가라앉지도 떠오르지도 않는다. **full extComp를 양쪽에 두면 금지**(중력
  이중 보상 → body가 천천히 부양). 어떤 비율이든 sink 상쇄는 성립한다(velocity 몫은 실제 속도, split 몫은 에너지 중립).

**튜닝:** `kBaumgarteBeta`:`kSplitImpulseBeta` 비율로 부드러움↔단단함을 조절. 팝이 거슬리면 split↓,
침투가 남으면 split↑ 또는 Baumgarte↑. 과거 split-only(0/0.8)는 너무 튀었고, Baumgarte-only는 침투 잔류.

---

## Ragdoll — CLIENT ONLY

**래그돌은 클라이언트에만 존재한다.** 서버에는 `Ragdoll` 클래스가 없으며 래그돌 바디의 충돌을 인지·처리하지 않는다. 사망한 고블린은 서버에서 `Dead` 상태로 전환만 되고 물리 시뮬레이션은 멈춘다.

클라이언트에서의 사망→래그돌 전환 흐름:
1. `S_SkillHit` → `onSkillHit()`:
   - `newHp <= 0`이면 `goblin->setRagdollInitVelocity(pkt->targetVelocity)` 저장 (사망 직전 서버 속도)
   - `applyHit()` → `goblin->setRagdollPendingActivation(true)`
2. 같은 프레임 `animSystem_.update()` 후 `activateRagdollIfPending()` 호출 (finalXformData 확정 후)
3. `activate(physicsWorld_)` 이후 초기 속도 + noise impulse 적용:
   - **초기 속도**: 모든 뼈에 `setLinearVel(initVel)` — 강체이던 고블린의 운동량 보존
   - **noise impulse**: 뼈별 `BoneBoxDef::noiseImpulse` 크기, velDir 방향으로 bias된 랜덤 impulse
     ```
     dir = normalize(velDir * kNoiseBias + randUnit * (1 - kNoiseBias))  // kNoiseBias = 0.6
     body->applyImpulse(dir * noiseImpulse, body->pos())
     ```
4. 이후 매 프레임 `syncRagdollToAnim()` — ragdoll body transform → finalXformData 덮어씀

**초기 속도가 물리적으로 올바른 이유:**
- 서버 속도는 킬링 블로우 `applyImpulse()` 직후 읽힌 값
- 강체의 모든 부분이 같은 선속도를 공유 → 뼈 질량 무관하게 `setLinearVel(v)` 적용이 옳음
- 전체 래그돌 질량 ≈ 고블린 바디 질량(70 kg) → 운동량 보존

**Ragdoll 물리 파라미터 (Unity에서 설정):**
`BoneBoxDef`에 포함되어 `.bin` 파일로 익스포트:
- `linearDamping`, `angularDamping`, `friction`, `restitution` — 뼈별 물리 특성
- `noiseImpulse` — 뼈별 최대 noise impulse (N·s)

Unity 익스포트 경로: `GoblinRagdollConfig.cs` Inspector → `ModelExtractor.cs` → `.bin`

---

## Ragdoll 소유권 모델 (Phase 7)

```
Ragdoll::bodies_  ──owns──>  unique_ptr<RigidBody>
Ragdoll::joints_  ──owns──>  unique_ptr<Constraint>
PhysicsWorld::entries_       ──ref──>  RigidBody*     (registerBody/unregisterBody)
PhysicsWorld::jointRefs_     ──ref──>  Constraint*    (addJointRef/removeJointRef)
```

**destroy() 순서 보장**: joints 먼저 제거 (joint가 body raw ptr 참조), bodies 나중 제거.

**syncFromPose**: row-vector 규약 `boneWorldMat = localAnimMat * parentWorldMat`
**syncToPose**: `localMat = boneWorldMat / parentWorldMat` (= boneWorldMat * inv(parent))
**seedFromFinalXforms**: `boneWorldMat = bone.toDress * finalXformData[i] * objectWorldMat`

**Object::update() ragdoll override**:
```cpp
// ragdoll 활성 시 finalXformData를 body transform으로 직접 덮어씀
finalXforms[boneIdx] = bone.toLocal * (boneWorldMat / renderState_.world);
// boneWorldMat: orient=body->orient(), translation=bone origin (캡슐 중심 - orient.rotate(capsuleOffset))
```

---

## ActiveRagdollController 설계 (Phase 8)

**PD 토크 공식** (bone당):
```
q = target * conj(current)       // 오차 quaternion
if q.w < 0: q = -q               // 최단 경로 보장
error = q.xyz * 2                // 소각도 근사: scaled axis-angle
torque = kp * error - kd * omega
body->applyTorqueImpulse(torque * dt)
```

**onImpact**: kp만 0으로 감소 (kd 유지 → 에너지 흡수 유지), impactRecoveryTime_ 동안 선형 복원.

**목표 orientation 계산**: DFS로 `targetPose` AnimFrame에서 bone별 world orientation 계산
(`boneWorldMat = convertAnimFrameToMatrix(pose[i]) * parentWorldMat`, row-vector 규약).

---

## 충돌 normal 개선 (OBB convention 수정)

**배경:** `collision.cpp` `collides(OBB, OBB)` 에서 AABB는 B→A normal을 반환하나 OBB는
A→B 방향을 반환하는 convention 불일치가 있었다. 때문에 `generateContacts()`에서
`res.normal`을 신뢰할 수 없어 center-to-center fallback을 항상 사용했다.
center-to-center는 한 오브젝트가 조금이라도 위에 있으면 +Y 성분이 생겨 충돌 해소가 위쪽으로 쏠리는 문제가 있었다.

**수정 (`collision.cpp` line 102):**
```cpp
// B→A convention으로 통일 (dProj > 0 → B가 axis 방향으로 앞 → B→A = -axis)
const float sign = (dProj >= 0.f) ? -1.f : 1.f;
```

**결과:** `res.normal`이 신뢰 가능해져 `generateContacts()`가 기하학적 normal을 직접 사용.
두 오브젝트가 옆으로 접촉 시 normal이 수평 → 수평 해소.

### 캐릭터-캐릭터 접촉 법선 수평화 (수직 발사 방지)

**문제:** 두 직립 캐릭터의 BVH(뼈 박스)가 서로 다른 높이에서 닿으면 narrow-phase 법선에 +Y 성분이
생긴다. body-body Baumgarte(`kBaumgarteBeta`)는 그 법선을 따라 **실제 분리속도**를 주입하므로,
+Y 성분만큼 캐릭터가 **위로 발사**된다(실제 속도라 지속 → 튀어오름). split impulse beta를 낮춰
침투가 더 오래 남으면 발사 창이 길어져 악화된다.

**수정 (`physicsWorld.cpp generateContacts()` body-body 루프):** 양쪽 모두 Static이 아닌 쌍(에이전트끼리
= Dynamic/Kinematic)이면 법선을 **수평면에 투영**(`n = normalize(n.x, 0, n.z)`)하고 침투도 수평 성분으로
스케일(`depth *= horizLen`). 거의 수직인 법선(드묾)은 수평 center-to-center로 fallback. **Static(거점)이
포함된 쌍은 원래 법선 유지** → 막힘·올라타기 보존. 지형은 별도 경로(body-terrain)라 영향 없음.

**근거:** 직립 캐릭터는 측면 충돌로 수직 속도를 얻으면 안 된다. 기존 '플레이어 간 XZ-only soft
separation' 철학과 일치. (Dynamic↔Dynamic=standalone 캐릭터, Dynamic↔Kinematic=온라인 로컬 플레이어↔고블린 모두 적용.)

---

## Static-Object Depenetration (Static 충돌 전용 경로)

**파일:** `staticDepenetration.hpp/cpp` · `StaticContact` + `resolveStaticPenetration()`

**문제:** Static이 포함된 충돌(Static-Dynamic, Static-Kinematic)을 ContactConstraint(velocity-level
sequential impulse) solver로 처리하면 두 가지 결함이 있다.
1. **불필요한 계산 과다** — 무한 질량 벽과의 단순 침투 해소에 PGS velocity 반복 + warmstart + friction +
   각 impulse는 과하다.
2. **무한 질량으로 인한 잘못된 결과** — 접촉점이 COM에서 벗어나면 `r×n` 토크 항이 잘못된 회전(spin)을
   유발하고, Baumgarte bias가 실제 분리속도를 주입해 물체가 튕겨나간다.
3. **(숨은 버그) Kinematic은 invMass()==0** → Static-Kinematic 쌍은 양쪽 invMass=0 → `effMass=1/0=inf`,
   impulse 가드(`invMass>0`)에 막혀 무효. split impulse도 `applyPseudoVelocity`가 Kinematic을 건너뜀 →
   **온라인 원격 플레이어·고블린(Kinematic)이 거점(Static)을 그대로 통과**했다.

**설계 (AAA character-controller depenetration):** static 포함 쌍은 `generateContacts()`에서
`StaticContact`로 라우팅하고 즉시 `continue`(ContactConstraint 미생성). `step()` 말미(solve +
applyPseudoVelocity 이후)에 `resolveStaticPenetration()`이 일괄 해소한다.
- **위치 보정:** movable body를 normal(static→movable) 방향으로 침투분만 밀어냄.
  `d = min(kCorrectFrac * (depth - kSlop), kMaxCorrect)`. **COM 평행이동만 — 회전 미적용**(spin 제거).
  static은 절대 안 움직임. movable별로 normal 축에 **max-combine** 누적 → 코너(다중 static)에서 공유
  축 과보정 없이 양쪽 벽 밖으로 분리.
- **속도 처리:** surface로 파고드는 inward normal 성분만 제거(`vn<0`일 때 `v -= n*vn`, restitution=0).
  tangential(미끄러짐) 보존, **분리속도 주입 없음** → 정착 시 진동 없음.
- **Kinematic 직접 이동:** `setPos`/`setLinearVel`은 invMass 무관하게 `curr_`에 기록 → impulse solver를
  쓰지 않으므로 Kinematic도 정상적으로 밀려난다. (invMass 가드는 impulse solver 전용; 의미 불변.)
- **BVH 재동기화:** 직접 `curr_.pos`를 쓰므로 integrate 시 만든 BVH가 stale. 이동된 body(dirty set)만
  `onRebuildBVH` 재호출 → 다음 sub-step narrow phase가 보정 위치를 본다.

**진동 방지 파라미터 근거:**
| param | 값 | 근거 |
|---|---|---|
| `kSlop` | 0.005m | sub-slop 침투에서 정착시켜 push/낙하 재검출 진동 제거. `ContactConstraint::kSlop`과 일치 |
| `kCorrectFrac` | 0.8/substep | full 보정은 overshoot→재침투→buzz. 부분 보정으로 기하급수 수렴+oscillation 감쇠 (Baraff/Catto) |
| `kMaxCorrect` | 0.2m/substep | 깊은 터널링 시 순간이동 방지, 몇 substep에 걸쳐 완만히 해소 |

**순서 근거:** dynamic-dynamic solver 이후 마지막에 실행 → static 벽이 위치의 최종 결정권을 가져,
다른 dynamic body에 눌린 body도 벽 밖으로 나간다.

**적용 범위:** (1) broad phase에 RigidBody로 등록된 일반 static body(거점 등), (2) `ScatterCollider`가
생성하는 scatter prop(나무/바위) contact. 둘 다 `StaticContact`로 모여 같은 경로로 해소된다(회전 없음·분리속도
주입 없음 → 정적 장애물에 정확). **Terrain은 검증된 기존 `TerrainCollider` 경로(split-impulse only, Baumgarte
off)를 그대로 유지** — 지형은 height-field 정점 샘플링+강제 Y-up 법선이라 경로가 다르고, 가장 눈에 띄는 지면
동작의 회귀 위험을 피한다.

---

## ContactConstraint 외력 보상 (External Force Compensation)

**배경:** Baumgarte bias는 침투 깊이 보정 속도를 나타낸다. 중력처럼 body를 normal 반대 방향으로
계속 끌어당기는 외력이 있으면 그 다음 스텝에서 bias가 극복해야 할 속도가 더 많아진다.
보상 없이는 지형 위 오브젝트가 중력에 의해 조금씩 뚫고 내려가는 현상(creep)이 발생한다.

**인터페이스:** `ContactConstraint::setExternalAccels(mu::Vec3 aA, mu::Vec3 aB)`
→ `prepare()` 호출 전에 설정. body-body 쌍과 body-terrain 쌍 모두에서 호출.

**prepare() bias 공식:**
```
penetration = max(0, depth - kSlop)
extVelProj  = dot((extAccelA - extAccelB) * dt, n)
extComp     = max(0, -extVelProj)
extCompVel   = kExtCompVelFrac * extComp        // velocity 몫
extCompSplit = extComp - extCompVel             // split 몫 (합 = extComp)
bias       = kBaumgarteBeta * invDt * penetration + extCompVel        // velocity-level (Baumgarte는 body-body만)
pseudoBias = kSplitImpulseBeta * invDt * penetration + extCompSplit   // position-level
```

**주의:** extComp(외력/중력 보상)는 **두 채널에 나눠** 싣는다(`kExtCompVelFrac` 비율). 둘 다 외력을 고려하되
합이 정확히 extComp여야 한다 — full extComp를 velocity·split **양쪽에 두면 중력이 이중 보상**되어, integrate
단계의 중력 sink(`g·subDt²`, 한 번)가 두 번 상쇄되며 body가 천천히 부양한다. 분배 시에는 어떤 비율이든
sink와 정확히 상쇄되어 creep도 부양도 없다(velocity 몫은 실제 분리속도, split 몫은 에너지 중립 위치 보정).

**두 동적 오브젝트(같은 중력):** `extAccelA - extAccelB = 0` → `extComp = 0` (보상 없음, 올바름)
**동적 vs 지형(static):** `extAccelA = gravity, extAccelB = 0` → `extComp = max(0, -dot(gravity*dt, n))` → 양수 → 중력 보상

---

## RigidBody::userData_ — 게임 레이어 연결

`RigidBody`는 `void* userData_` 필드를 가진다. 게임 레이어(`Object*` 등)를 physics solver가
역참조할 필요 없이 contact 순회 시 오브젝트를 찾기 위해 사용한다.

- `setUserData(void*)` / `userData()` 접근자 제공
- `PhysicsWorld::registerBody()` 호출 후 `body().setUserData(this)` 패턴
- `forEachContact()` 내에서 `static_cast<Object*>(cc.bodyA->userData())` 로 역참조

---

## PhysicsWorld::forEachContact() — post-step contact 순회

```cpp
template<typename Fn>
void forEachContact(Fn&& fn) const;
```

`step()` 직후 활성 `ContactConstraint` 목록을 순회한다. 주로 게임 레이어에서
충돌 상태(충돌 여부, 대상 body)를 읽기 위해 사용한다.

**BV 충돌 색상 시각화 패턴 (`standalone/game.cpp`):**
```
Pass 1: forEachContact → terrain 접촉 body → setBVColor(red)
Pass 2: forEachContact → object-object 접촉 body → setBVColor(blue)  ← 우선순위 높음
```
두 패스를 나눔으로써 Terrain+Object-Object 동시 접촉 시 파란색이 우선 적용된다.

---

## BV 충돌 색상 시각화

BV(Bounding Volume) 렌더링 시 충돌 상태에 따라 색상을 변경한다.

| 상태 | 색상 |
|------|------|
| 기본 | 초록 (0, 1, 0, 1) |
| Terrain 접촉 | 빨강 (1, 0, 0, 1) |
| Object-Object 접촉 | 파랑 (0, 0, 1, 1) — Terrain보다 우선 |

**구현:**
- `Object::bvColor_` (mu::Vec4): 매 프레임 `setBVColor()`로 갱신, `addDrawEvent()`에서 DrawEvent.color에 전달
- `BVPipeline::DrawEvent::color`: 기본 초록. `BVPipeline.cpp`의 `PerInstanceData` 생성 시 `drawEvent.color` 사용
- `BVShader::PerInstanceData::color`: StructuredBuffer를 통해 셰이더로 전달 (기존부터 지원)

---

## Sequential Impulse Solver 부호 규약

```
normal     : B → A 방향 (A가 +normal로 분리됨)
Jv         : dot(relVel_A_contact, normal)   → 양수 = 분리, 음수 = 접근
bias       : +kBaumgarteBeta * invDt * max(0, depth - kSlop)  (양수)
jn         : -(Jv - bias) * effMassNormal                     ← 부호 주의
             (Jv=0, depth>0 → jn > 0 → 분리 impulse)
accNormal  : clamp(prev + jn, 0, ∞)   (비음수, 당기는 impulse 금지)
```

**과거 버그 (수정 완료):** `jn = -(Jv + bias)` 로 되어 있어 정지한 두 물체의 jn이
항상 음수 → clamp → 0 → 충돌 응답 없음. `+` → `-` 1자 수정으로 해결.

---

## RigidBody 관성 텐서 초기화 주의사항

`diagMat3()` 내부에서 `mu::Mat3x3 m{}` (기본 생성자 = identity)을 사용해야 한다.
`m(0.f)` (스칼라 곱 = 전체 0 행렬)로 초기화하면 4×4 내부 표현의 row 3이
`(0,0,0,0)` → det=0 → `XMMatrixInverse` → NaN → `invInertiaLocal_ = NaN`
→ `angAcc = 0 × NaN = NaN` (IEEE 754) → angular velocity 폭발.

DirectXMath 기반 `mu::Mat3x3`은 항상 4×4 XMMATRIX를 내부에 사용한다.
row 3은 `(0, 0, 0, 1)`이어야 역행렬 계산이 정상 동작한다.

---

## VelocityMotor (NPC 전용)

`RigidBody`에 내장된 속도 수렴기. AI가 `setLinearVelocity()`로 속도를 덮어쓰는 대신 `setDesiredVel()`로 목표 속도를 선언하면, `integrate()` 안에서 매 sub-step마다 보정 impulse를 계산해 실제 속도를 목표로 수렴시킨다.

```
// physicsWorld.cpp::integrate() — Dynamic branch, damping 적용 직후
velError = desiredVel - currVel  (XZ only, Y는 중력 전용)
corrAccel = gain * velError                   // 비례 제어
corrAccel = clamp(|corrAccel|, maxDecel 또는 maxAcceleration)
currVel.xz += corrAccel * subDt
```

**파라미터 (`VelocityMotor` struct in `rigidBody.hpp`):**
| 필드 | 기본값 | 의미 |
|------|--------|------|
| `desiredVel` | `{0,0,0}` | AI가 선언한 목표 수평 속도 |
| `maxAcceleration` | 20 m/s² | 목표 방향 가속 한계 |
| `maxDeceleration` | 40 m/s² | 제동 한계 (가속보다 크게) |
| `gain` | 10 | 비례 게인 (≈ 1/수렴시간_s) |
| `enabled` | false | 활성화 여부 |

**설계 원칙:**
- knockback impulse가 적용된 다음 프레임부터 motor가 수렴을 시작 → `knockbackTimer_` 기반 AI 스킵 불필요
- Y축 제외: 중력은 물리 엔진이 담당
- sub-step 구조에서 `dtf = subDt`이므로 전체 보정량은 `maxA * fullDt`로 수렴
- `NpcConfig::motorMaxAcceleration`을 낮출수록 knockback이 더 오래 유지되는 느낌

---

## 캐릭터 Dynamic Body 설정 원칙

- `MotionType::Dynamic` 사용 (Kinematic에서 전환)
- `angularDamping = 100.f`: 매 step에서 `max(0, 1 - 100/60) = 0`으로 즉시 소거
  → 충돌 impulse에 의한 캐릭터 회전/기울어짐 방지
- `linearDamping`과 최대 속도의 관계:
  ```
  terminal_vel = accelRate / linearDamping
  → accelRate = maxSpeed × linearDamping
  ```
  `kPlayerLinearDamping = 12`, `kPlayerAccelRate = kPlayerMaxSpeed × 12`

---

## 축별 velocity damping 분리 (`physicsWorld.cpp`)

Dynamic body integrate 시 linear damping을 축별로 분리 적용한다.

```
x/z (수평): linearDamping  → 지면 마찰 (캐릭터: 12)
y   (수직): kAirDamping    → 공기 저항 (physicsWorld.cpp 상단 상수, 현재 0.15f)
```

- **이유**: `linearDamping`은 수평 이동의 지면 마찰로 설계된 값(12)이다.
  이를 y축에도 적용하면 종단 속도 = 9.8 / 12 ≈ 0.82 m/s로 낙하가 거의 정지 수준이 된다.
- `kAirDamping = 0.15f` → 종단 속도 ≈ 65 m/s, 시상수 τ = 1/0.15 ≈ 6.7s.
- **낙하 체감과 substepping (중요):** y축 점화식 `v' = v·(1−c·dt) + g·dt`의 종단속도
  `g/c`는 substep 수에 **완전히 불변**이다(damping의 step당 배수 `(1−c·h/n)^n`은
  n 증가 시 `e^(−c·h)`로 수렴하므로 substep이 많을수록 오히려 damping이 *덜* 걸림).
  과거 `kAirDamping=0.5`(종단 19.6 m/s, τ=2s)는 체감 낙하 구간(0~2s)을 통째로 깎아
  "느리게 떨어진다"의 원인이었다 — substepping 누적이 아니라 드래그 계수 자체가 문제.
  0.15로 낮추면 1s에 ≈9.1 m/s(현실 9.8의 93%), 2s에 ≈16.9 m/s(86%)로 초반 낙하가
  현실 `g·t`에 근접한다. 대부분의 게임 낙하(1~3m)는 종단 근처에 도달하지 않는다.
- 값 조정: `terminal_fall_vel = gravity / kAirDamping` (합리적 밴드 0.12~0.2 → 종단 49~82 m/s)
- **서버 동기:** `RoomServer/physicsWorld.cpp`의 `kAirDamping`과 반드시 동일해야 한다
  (온라인은 서버 권위 + 클라 예측이므로 불일치 시 러버밴딩 발생).

---

## processInput 속도 처리 원칙 (`standalone/game.cpp`, `online/onlineGame.cpp`)

입력으로 인한 가속과 속도 클램프는 **x/z 수평 성분에만** 적용한다.

```cpp
// 가속: x/z에만 더함, y(중력)는 보존
float newX = fullVel.x() + accel.x();
float newZ = fullVel.z() + accel.z();
// 클램프: x/z 평면 속도만 kPlayerMaxSpeed 기준
const float hSpd2 = newX*newX + newZ*newZ;
if (hSpd2 > kPlayerMaxSpeed²) { newX *= scale; newZ *= scale; }
player_->setVelocity(mu::Vec3(newX, fullVel.y(), newZ));
```

- **이유**: y는 물리 엔진(중력·`kAirDamping`)만 담당한다. 낙하 중 y 속도가 크면
  3D 속도 크기 기준으로 클램프할 때 (1) x/z가 의도치 않게 줄어들고, (2) **물리가
  적분한 y 낙하 속도가 매 프레임 `kPlayerMaxSpeed`(수평 상한) 안으로 재스케일되어
  물리 damping과 별개로 낙하가 한 번 더 감속된다(이중 감속).**
- **standalone/online 동일 규칙(필수):** `standalone/game.cpp::processInput`과
  `online/onlineGame.cpp::processInputGame`은 반드시 같은 x/z-only 모델이어야 한다.
  과거 online은 구버전 전체-3D-속도 클램프(`vel*=kPlayerMaxSpeed/vel.len()`)가 남아
  있어, 온라인 낙하만 standalone보다 느리게 체감됐다 (2026-06-04 수정). 로컬 플레이어는
  클라 권위 예측이므로 클라 쪽 입력 처리 불일치가 곧 체감 차이로 직결된다.

---

## BVH 충돌 판정: leaf-only

`collides(BVH, BVH)`는 **leaf-leaf 쌍에서만** 정밀 shape 판정을 수행한다.
내부 노드(LOD 0, LOD 1)의 coarse bounds는 AABB fast-reject에만 사용된다.

이전에는 비-leaf 노드도 정밀 판정에 참여해, LOD 0끼리 겹치면 실제 bone shape은
충돌 안 해도 collision이 발생 → angular impulse 오적용 → 무한 회전 버그가 있었다.

---

## BVH 재빌드 시점

- `PhysicsWorld::integrate()` 끝에서 `onRebuildBVH` 콜백 호출
- 게임 로직의 `setPos()`, `setOrient()` 호출 시 Object가 직접 `rebuildBodyBVH()` 호출

---

## 충돌체 추상화: WorldCollider (정적 환경)

충돌은 세 경로로 처리한다.

- **Body-Body**: `RigidBody::worldBVH_` dual-tree DFS. `broadPhase_`로 쌍을 좁힌 뒤 narrow phase.
- **Body-정적환경**: `WorldCollider` 추상 클래스(`collision.hpp`). broad phase를 우회하고
  `generateContacts()`에서 **Dynamic body마다** 등록된 모든 collider를 순회한다(`footprintReject()`
  빠른 거부 → narrow phase). `PhysicsWorld`는 이들을 `SlotVector<unique_ptr<WorldCollider>> worldColliders_`
  하나로 소유한다(지형·scatter 공용 registry; unregister는 tombstone을 남겨 다음 register가 재사용 —
  `common/slotVector.hpp`).

**WorldCollider 가상 인터페이스:**
| 메서드 | 역할 |
|--------|------|
| `footprintReject(bodyPos, pad)` | footprint 밖 body 빠른 거부(narrow phase 스킵) |
| `generateContacts(dyn, sink)` | Dynamic body 하나의 contact를 `ContactSink`에 append |
| `queryArm(pivot, dir, armLen, spherePad)` | 카메라 스프링 암 occlusion(기본=차단 없음, 클라 전용) |

`ContactSink`는 출력 버퍼 묶음(`{ ContactConstraint 목록*, StaticContact 목록*, gravity, subDtSec }`)이라
각 collider가 **자기 의미의 contact**를 담는다: 지형=support `ContactConstraint`, scatter=push-out `StaticContact`.

**Dynamic body만 질의 → 권위 자동 분리:** WorldCollider는 Dynamic body에 대해서만 호출된다. 클라에서는
로컬 플레이어/래그돌만 Dynamic(원격 플레이어·고블린은 Kinematic), 서버에서는 몬스터만 Dynamic(플레이어는
Kinematic)이므로 분기 없이 **플레이어-prop=클라, 몬스터-prop=서버**가 성립한다. 상세: `docs/scatterSystem.md`.

### transformShapeRigid / makeWorldBVH (`collision.hpp/cpp`)
비본 강체 world-BVH 변환 공유 헬퍼. AABB shape는 orient가 항등이면 world AABB 유지(저비용), 회전이 있으면
**world OBB로 정확 변환**(축정렬 모델 박스를 yaw된 인스턴스에서도 정확히 표현). `Object::rebuildBodyBVH()`
비본 경로와 `ScatterCollider`가 공유 → 회전된 static Object(예: 회전 거점)의 충돌 박스도 정확히 회전한다.

### ScatterCollider : WorldCollider
chunk당 1개의 정적 prop(나무/바위) 콜라이더. prop은 움직이지 않으므로 collidable 인스턴스의 model-space BVH를
`makeWorldBVH()`로 **생성 시 1회 world BVH 베이크**(이후 재빌드 없음)하고, 내장 XZ uniform grid로 body/ray
근처 인스턴스만 조회한다(밀집 수목에서도 per-body 비용 억제).
- `generateContacts`: grid 후보 → `collides(dyn.worldBVH, inst.worldBVH)` → hit 시 push-out `StaticContact`
  (법선 static→movable). `step()` 말미 `resolveStaticPenetration`이 일괄 해소.
- `queryArm`: grid 후보 → `RaycastBVH(inst.worldBVH, armRay)` → 나무/바위가 카메라 암을 차단.
- **결정론(위치 동기화 불변식):** 클라/서버가 `common/scatterTransform.hpp::makeScatterWorld` + 동일 ground-snap
  + 동일 BVH 소스를 써 prop 충돌 형상이 일치한다. 서버 미러는 `RoomServer/collision.*`(+ 헤더 전용
  `RoomServer/staticDepenetration.hpp`). collidable prop은 `<name>Server.bin`(BV-only) 재추출 필요(없으면 비충돌 skip).

청크 활성/해제 시 등록 경로는 `docs/scatterSystem.md`(클라) / `docs/serverTerrainChunk.md`(서버) 참조.

---

## TerrainCollider : WorldCollider

`collision.hpp/cpp`에 정의(위 WorldCollider 추상의 height-field 구현). `collision.hpp`는 `TerrainHeightField`를 forward-declare하고,
`collision.cpp`가 `terrain.hpp`를 include해서 완전 타입을 사용한다.

**알고리즘:**
1. Dynamic body의 BVH leaf 노드에서 하단 꼭짓점들 추출
   - AABB leaf: 4 bottom corners (y = center.y - halfSize.y)
   - OBB leaf: 8 corners 전부 (관통 여부로 필터링)
2. 각 꼭짓점에 대해 지형 로컬 공간으로 변환 → `getHeightAt(x, z)` 조회
3. `depth = terrainWorldY - vertexY > 0` → ContactPoint 생성
4. depth 내림차순 최대 4개 유지 → `ContactConstraint(dynamic, terrainBody)` 생성

**Normal 부호 규약:** `normal = B → A` (B=terrain, A=dynamic). `getNormalAt()` 결과는
항상 Y > 0 (위방향)이므로 terrain → dynamic 방향 = 올바른 부호.

---

## SAPBroadPhase

`broadPhase.hpp/cpp`에 정의. Phase 5에서 `BruteForceBroadPhase`를 대체.

**알고리즘 (X축 Sort-and-Sweep):**
1. `update()`: 각 body AABB의 minX/maxX 엔드포인트 2N개 생성 → insertion sort
   - insertion sort: 프레임 간 위치 변화가 작아 거의 정렬 → 평균 O(n)
2. `queryPairs()`: active set sweep
   - min endpoint 도달: active set 내 모든 body와 Y/Z overlap 검사 → 쌍 생성
   - max endpoint 도달: active set에서 제거
   - Static-Static 쌍 제외
3. `queryAABB(const AABB& box)`: query box를 기준으로 겹치는 body 목록 반환
   - 정렬된 `endpoints_` sweep: `!ep.isMax && ep.value > boxXMax`에서 early break
   - max endpoint 발견 시: `ep.value >= boxXMin && overlapYZ()` 충족하면 result에 추가
   - break 이후 active에 남은 body: xMax > boxXMax > boxXMin → X 조건 자동 충족, YZ만 재검

**Static body 처리:** `ContactConstraint::prepare()`에서 `invMass == 0`인 body의
angular 기여를 0으로 처리 (static body의 default `invInertiaWorld_`가 identity여서
effective mass 오산 방지). `applyImpulse()`는 이미 `invMass == 0` guard 보유.

---

## ContactPoint 위치

`ContactPoint` struct는 `collision.hpp`에 정의한다 (Phase 4에서 이동).
`contactConstraint.hpp`는 `rigidBody.hpp → collision.hpp` 체인으로 이를 획득한다.

---

## 카메라 충돌 회피 (Spring Arm + BVH Raycast)

`camera.hpp/cpp`, `physicsWorld.hpp/cpp`, `broadPhase.hpp/cpp`, `collision.hpp/cpp`

### 설계 원칙

impulse 기반 충돌 해소는 진동(jitter) 유발 → arm 길이 직접 제어 방식(Spring Arm) 채택.
- **fast-in**: 장애물 감지 시 arm 길이 즉시 단축 (snap)
- **slow-out**: 장애물 소멸 시 `armReturnRate_ * dt` 속도로 천천히 복귀 (lerp)

### queryCameraArm 알고리즘

```
PhysicsWorld::queryCameraArm(pivot, desiredEye, spherePad) → allowedArmLength:
  armLen = length(desiredEye - pivot)
  armDir = normalize(desiredEye - pivot)
  allowed = armLen

  [정적 환경: worldColliders_ 순회]
    for wc in worldColliders_:
      allowed = min(allowed, wc.queryArm(pivot, armDir, armLen, spherePad))
    // TerrainCollider::queryArm — 지형 N=6 샘플:
    //   origin = terrainBody()->pos();  groundY = origin.y + hf->getHeightAt(p.x-origin.x, p.z-origin.z)
    //   (getHeightAt 내부에서 *sizeY 처리됨) p.y < groundY + kCameraMinGroundClearance
    //   (0.15f, collision.cpp 상수) 인 첫 샘플에서 암 단축 → break
    // ScatterCollider::queryArm — grid 후보 인스턴스마다 RaycastBVH(나무/바위가 암 차단)

  [장애물 broad phase]
    armAABB = AABB enclosing [pivot, desiredEye] expanded by spherePad
    candidates = cameraBroadPhase_->queryAABB(armAABB)

  [장애물 narrow phase]
    for body in candidates:
      hit = RaycastBVH(body->worldBVH(), armRay)
      if hit.hit: allowed = min(allowed, hit.t - spherePad)

  return max(0.f, allowed)
```

### Camera::update(float dt)

```
desiredLen = length(desiredEye - at_)
if currentArmLength_ <= 0: currentArmLength_ = desiredLen  // 초기화 가드
allowed = physicsWorld_ ? queryCameraArm(at_, desiredEye, cameraRadius_) : desiredLen

if allowed < currentArmLength_:
  currentArmLength_ = allowed                                 // fast-in (즉시)
else:
  currentArmLength_ += min(armReturnRate_ * dt, allowed - currentArmLength_)  // slow-out

eye_ = at_ + (desiredEye - at_) * (currentArmLength_ / desiredLen)
```

### RaycastOBB 구현 원칙

OBB를 로컬 공간 AABB로 환원해 기존 `RaycastAABB`를 재사용.
```cpp
invRot = ~obb.orient   // NQuat::operator~ = XMQuaternionConjugate (켤레 사각수)
local.origin = invRot.rotate(ray.origin - obb.center)
local.dir    = invRot.rotate(ray.dir)
box = AABB{ {0,0,0}, obb.halfExtents * 2.f }
hit = RaycastAABB(box, local)
// t 값은 회전 불변 → 별도 변환 불필요
if hit.hit: hit.normal = obb.orient.rotate(hit.normal)  // normal만 월드 복원
```

### RaycastBVH 구현 원칙

고정 크기 `int stack[64]`으로 BVH 트리 순회 (힙 할당 없음, 깊이 < 20).
```
while stack not empty:
  node = bvh.nodes[stack.pop()]
  b = RaycastAABB(node.bounds, ray)
  if !b.hit || b.t >= best.t: continue  // bounds fast-reject
  if node.isLeaf():
    s = std::visit(AABB→RaycastAABB / OBB→RaycastOBB, node.shape)
    if s.hit && s.t < best.t: best = s
  else:
    for child in node.children: stack.push(child)
```

### 카메라 장애물 등록

```cpp
// 씬 오브젝트(벽, 나무 등)를 장애물로 등록 — 일반 physicsWorld broadPhase와 독립
physicsWorld_.registerCameraObstacle(&obj.body());
// 해제
physicsWorld_.unregisterCameraObstacle(&obj.body());
// 지형/scatter prop은 WorldCollider(registerTerrain/registerScatter)로 등록되어
// queryCameraArm의 worldColliders_ 순회에서 자동으로 암을 차단(별도 카메라 등록 불필요)
```

Camera와 PhysicsWorld 연결:
```cpp
camera_.setPhysicsWorld(&physicsWorld_);
// Camera::update(dt) 에서 physicsWorld_->queryCameraArm() 호출
```

### 물리 아키텍처 (Phase 1–8 완료)

연관 파일: `rigidBody.hpp/cpp`, `physicsWorld.hpp/cpp`, `constraint.hpp`,
`contactConstraint.hpp/cpp`, `broadPhase.hpp/cpp`, `collision.hpp/cpp`, `object.hpp/cpp`,
`jointConstraint.hpp/cpp`, `ragdollDef.hpp/cpp`, `ragdoll.hpp/cpp`, `activeRagdoll.hpp/cpp`

---

## 클래스 구조

```
PhysicsWorld
 ├─ std::vector<Entry>             // { RigidBody*, onRebuildBVH 콜백, collisionGroup, collisionMask }
 ├─ SAPBroadPhase                  // X축 Sort-and-Sweep, O(n log n)
 ├─ std::optional<TerrainCollider> // 지형 충돌 (BroadPhase 우회)
 ├─ std::vector<ContactConstraint> // 매 step 재생성
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
integrate(dt)
    ├─ Kinematic: damping → vel/omega snap → pos/orient 적분
    ├─ Dynamic:   damping → force/torque 적분 → pos/orient 적분 → clearAccumulators()
    └─ 각 body: onRebuildBVH 콜백 → worldBVH 재빌드

generateContacts()
    ├─ broadPhase_->update() + queryPairs()
    ├─ collides(bvhA, bvhB) → leaf-leaf 쌍에서만 정밀 판정
    ├─ ContactConstraint 생성 (법선 = narrow-phase 기하학적 normal res.normal; degenerate 시 center-to-center fallback)
    │   └─ cc->setExternalAccels(gravA, gravB): Dynamic body = gravity_, Static = (0,0,0)
    └─ TerrainCollider::generateContacts() → Dynamic body별 지형 contact 생성
        └─ cc->setExternalAccels(gravity_, {0,0,0})

solveConstraints(dt)
    ├─ prepare(dt)  : rA/rB, tangent frame, effMass, Baumgarte bias 계산
    │               + joint warmstart (accImpulse 이전 프레임 값 재적용)
    ├─ PGS × N iter : solveVelocity() — Contact + BallSocket/Hinge/ConeTwist 혼합
    │               N = solverIterations_ (기본 10, ragdoll 활성 시 20)
    └─ solvePosition(): no-op (Baumgarte only)
```

> Phase 4(TerrainCollider) 완료로 중력이 활성화됨. (`physicsWorld.cpp` integrate)

---

## Joint Constraint 설계 원칙 (Phase 6)

**공통 규약:**
- `kJointBeta = 0.1f` (ContactConstraint의 `kBaumgarteBeta = 0.2f`보다 softer)
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
- `swingTwistDecompose`: twist = q 성분 twistAxis 방향 투영, swing = q * conj(twist)
- `coneHalfAngle` max = `pi * 0.85f` (gimbal lock 방지)

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
bias = kBaumgarteBeta * invDt * penetration + extComp
```

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
y   (수직): kAirDamping    → 공기 저항 (physicsWorld.cpp 상단 상수, 현재 0.5f)
```

- **이유**: `linearDamping`은 수평 이동의 지면 마찰로 설계된 값(12)이다.
  이를 y축에도 적용하면 종단 속도 = 9.8 / 12 ≈ 0.82 m/s로 낙하가 거의 정지 수준이 된다.
- `kAirDamping = 0.5f` → 종단 속도 ≈ 19.6 m/s (게임 스케일에서 적절)
- 값 조정: `terminal_fall_vel = gravity / kAirDamping`

---

## processInput 속도 처리 원칙 (`standalone/game.cpp`)

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

- **이유**: 낙하 중 y 속도가 크면 3D 속도 크기 기준 클램프 시 x/z가 의도치 않게 줄어듦.

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

## 충돌체 추상화 (현재)

별도 Collider 인터페이스 없이 두 경로로 충돌을 처리한다.

- **Body-Body**: `RigidBody::worldBVH_` dual-tree DFS (기존)
- **Body-Terrain**: `TerrainCollider`가 `TerrainHeightField`를 직접 조회 (Phase 4 추가)
  - Terrain body는 BroadPhase에 등록하지 않음; `PhysicsWorld::generateContacts()`에서 별도 패스로 처리

---

## TerrainCollider

`collision.hpp/cpp`에 정의. `collision.hpp`는 `TerrainHeightField`를 forward-declare하고,
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

**Static body 처리:** `ContactConstraint::prepare()`에서 `invMass == 0`인 body의
angular 기여를 0으로 처리 (static body의 default `invInertiaWorld_`가 identity여서
effective mass 오산 방지). `applyImpulse()`는 이미 `invMass == 0` guard 보유.

---

## ContactPoint 위치

`ContactPoint` struct는 `collision.hpp`에 정의한다 (Phase 4에서 이동).
`contactConstraint.hpp`는 `rigidBody.hpp → collision.hpp` 체인으로 이를 획득한다.

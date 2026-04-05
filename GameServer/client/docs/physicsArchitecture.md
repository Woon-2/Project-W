### 물리 아키텍처 (Phase 1–3 완료)

연관 파일: `rigidBody.hpp/cpp`, `physicsWorld.hpp/cpp`, `constraint.hpp`,
`contactConstraint.hpp/cpp`, `broadPhase.hpp/cpp`, `collision.hpp/cpp`, `object.hpp/cpp`

---

## 클래스 구조

```
PhysicsWorld
 ├─ std::vector<Entry>             // { RigidBody*, onRebuildBVH 콜백 }
 ├─ BruteForceBroadPhase           // O(n²), Phase 5에서 SAP로 교체 예정
 └─ std::vector<ContactConstraint> // 매 step 재생성

RigidBody
 ├─ BodyState curr, prev           // 더블 버퍼 (렌더 보간용)
 ├─ MotionType: Dynamic/Kinematic/Static
 ├─ BVH worldBVH_
 └─ Dynamic 전용: invMass, invInertiaLocal/World, forceAccum, torqueAccum,
                  linearDamping, angularDamping, restitution, friction

ContactConstraint : Constraint
 └─ PGS velocity solve (Normal impulse + Coulomb friction)
    Baumgarte bias로 위치 수정 (split impulse는 solvePosition()에서 no-op)
```

`Object`는 `RigidBody body_`를 인라인으로 소유한다. `PhysicsWorld`는 포인터만 참조 (등록/해제 패턴).

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
    └─ ContactConstraint 생성 (법선 = center-to-center 벡터, 신뢰성 우선)

solveConstraints(dt)
    ├─ prepare(dt)  : rA/rB, tangent frame, effMass, Baumgarte bias 계산
    ├─ PGS × 10 iter: solveVelocity() — Normal impulse + Coulomb friction
    └─ solvePosition(): no-op (Baumgarte only)
```

> **주의**: 지형 충돌 미구현 상태에서 중력은 주석 처리. Phase 4(TerrainCollider) 완료 후 활성화.
> (`physicsWorld.cpp` integrate의 `/* gravity_ + */` 부분)

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

현재는 별도 Collider 인터페이스 없이 `RigidBody::worldBVH_`를 직접 사용한다.
Phase 4(TerrainCollider)에서 `Collider` 인터페이스를 도입할 예정이다.

---

## Phase 로드맵

| Phase | 목표 | 상태 |
|-------|------|------|
| 1 | PhysicsWorld + RigidBody 골격, Kinematic 이동 | 완료 |
| 2 | Dynamic body + 힘/관성 적분 | 완료 |
| 3 | ContactConstraint + PGS solver | 완료 |
| 4 | TerrainCollider + 지형 충돌 (중력 활성화) | 미구현 |
| 5 | SAPBroadPhase (O(n²) → O(n log n)) | 미구현 |
| 6 | Joint Constraints (BallSocket, Hinge, ConeTwist) | 미구현 |
| 7 | Ragdoll 구조 | 미구현 |
| 8 | ActiveRagdollController | 미구현 |

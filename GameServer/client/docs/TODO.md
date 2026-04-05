- [X] 모델 메시별로 셰이더 별도 적용
- [X] 공격을 위한 충돌처리 구현 (AABB 기반, CombatSystem 서브시스템)
- [X] 몬스터 단순 AI 구현 (쿨타임 기반 AABB 교차 공격)
- [X] 공격에 해당하는 바운딩 볼륨 렌더링 가능하도록 구현
- [X] OBB 충돌처리 지원, 캐릭터 오브젝트들에 대해 기존 AABB 전부 OBB로 교체 (AABB는 특정 단순 사물에만 사용할 예정)
- [X] 유니티에서 추출한 바이너리 리소스를 로드해 Bounding Volume Hierarchy 구축 및 그를 통한 충돌처리로 업그레이드
  - 유니티에서 어떻게 추출했는지는 `unityScripts/ExtractUtil.cs`, `unityScripts/ModelExtractor.cs`, `unityScripts/MultiBoundingVolume.cs` 참조
  - BVH 노드가 bone에 종속된 경우 `bone.toDress * finalXformData()[i] * world` 체인으로 월드 변환
- [X] Height map 기반 Terrain 구현/Terrain Splat까지 (Unity에서 맵 추출)
  - TerrainExtractor.cs로 추출된 height.raw + terrain_meta.bin + terrain_manifest.bin + DDS 텍스처 로드
  - N×N 그리드 메시 생성 (중앙차분 법선, 32-bit IB), RGBA splat map 기반 레이어 블렌딩
  - terrain.hlsl: Lambertian + globalAmbient + PCF shadow, terrainPipeline.hpp/cpp: Dispatcher
- [X] level 바이너리에서 Terrain WorldTRS 읽어 월드 변환 적용
  - TerrainObject(Object 상속)와 TerrainData 분리: Object/Model 패턴과 동일
  - importNode() "Terrain" 분기 → TerrainObject 생성 → importTerrain() → update(0ms, 1.f)
  - TerrainPipeline::DrawEvent에 world 필드 추가, mainPass()에서 ev.world로 WVP 계산
- [X] Terrain Shadow 구현 (지형이 PBR 객체 위에 그림자를 드리움, PBR 객체의 그림자가 지형 위에 드리움)
  - terrainShadowMap.hlsl + TerrainShadowMapShader PSO 추가 (position-only, depth-only, NumRenderTargets=0)
  - TerrainPipeline::Dispatcher에 shadowPass/shadowPassMT/shadowUpdate/shadowDraw 추가
  - 공유 shadow map("ShadowMap") DSV에 지형 기하를 기록 → PBR mainPass에서 샘플링
- [X] Terrain roughness metallic도 unity에서 추출 및 렌더링 시 반영하도록 수정
  - 현재는 셰이더에 하드코딩되어 있음.
- [X] Cascaded Shadow Mapping 구현
- [ ] Rigid Body Physics 구현: 중력, 공기 저항, 마찰력 등 반영 (Phase 1-3 완료 / Phase 4 TerrainCollider + 중력 활성화 미구현)
- [ ] 몬스터 AI 시스템 초안 구현(주변 배회, 피격 시 어그로)
- [ ] 장비 장착: 공격 모션에 무기도 같이 움직이도록 (필요하면 IK 구현)
- [ ] Software Occlusion(Culling)을 통한 최적화
- [ ] Active Ragdoll 시뮬레이션, 몬스터들의 움직임에 적용
- [ ] 시분할 애니메이션 제대로 적용
- [ ] Deferred Shading을 위한 GBuffer 설계
- [ ] Deferred Shading 구현
- [ ] 청크 구현 및 리소스 멀티스레드 동적 로딩 구현 (Seamless Openworld가 가능하도록)
- [ ] Image Based Lighting 구현

## Rigidbody Physics 구현안

### 최종 목표

플레이어·몬스터 캐릭터들에 **Active Ragdoll** 적용.
캐릭터 각 bone을 `RigidBody`로, bone 간 연결을 `Constraint`(Joint)로 표현하고,
`ActiveRagdollController`(PD 컨트롤러)가 애니메이션 pose를 향해 토크를 가하면서
`PhysicsWorld`가 시뮬레이션을 돌린다.

### 현재 상태 (Phase 1–3 완료)

| 항목 | 현황 | 비고 |
|------|------|------|
| 물리 단위 | `RigidBody`가 `Object`에 인라인, `PhysicsWorld`가 포인터 관리 | Phase 1 완료 |
| Broad Phase | `BruteForceBroadPhase` O(n²) | Phase 5에서 SAP 교체 예정 |
| Narrow Phase | BVH dual-tree, leaf-only 정밀 판정 | Phase 1 완료 |
| Solver | PGS Sequential Impulse (Normal + Coulomb friction) | Phase 3 완료 |
| 중력 | 코드 내 비활성화 (`/* gravity_ + */`) | Phase 4 TerrainCollider 완료 후 활성화 |
| 충돌체 추상화 | BVH를 `RigidBody::worldBVH_`에 내장 | Phase 4에서 Collider 인터페이스 도입 예정 |
| 힘·질량 | `setMass()`, `applyForce()`, `applyImpulse()` | Phase 2 완료 |
| Joint/Constraint | 없음 | Phase 6 예정 |

---

### 목표 클래스 구조

```
PhysicsWorld                      ← 시뮬레이션 진입점 (기존 PhysicSystem 대체)
 ├─ BroadPhase (interface)
 │   ├─ BruteForceBroadPhase      ← 초기 구현
 │   └─ SAPBroadPhase             ← Phase 5에서 교체
 ├─ NarrowPhase                   ← Collider 타입 쌍 dispatch
 ├─ std::vector<RigidBody*>
 └─ std::vector<Constraint*>
      ├─ ContactConstraint         ← 충돌 contact 1개
      ├─ BallSocketJoint
      ├─ HingeJoint
      └─ ConeTwistJoint

RigidBody                         ← 물리 시뮬레이션 단위 (Object와 분리)
 ├─ MotionType  (Dynamic / Static / Kinematic)
 ├─ RigidBodyState  (pos, orient, linearVel, angularVel, ...)
 │   prev/curr 더블버퍼 → Object::update() 보간에 재활용
 └─ Collider* (소유 안 함, 외부 주입)

Collider (interface)              ← 충돌 형상 추상화
 ├─ BVHCollider                   ← 캐릭터·오브젝트 (기존 BVH 래핑)
 ├─ CapsuleCollider               ← ragdoll bone용
 └─ TerrainCollider               ← 높이맵 정적 충돌

Constraint (interface)            ← velocity-level + position-level PGS
 ├─ ContactConstraint
 ├─ BallSocketJoint
 ├─ HingeJoint
 └─ ConeTwistJoint

Ragdoll                           ← Object 당 0 또는 1개
 ├─ std::vector<RagdollBone>
 │   └─ { boneIdx, RigidBody*, Constraint* parentJoint }
 ├─ activate() / deactivate()
 ├─ syncFromPose()                ← 애니메이션 → body transform
 └─ syncToPose()                  ← body transform → 렌더 bone

ActiveRagdollController           ← Ragdoll 당 1개
 └─ update(targetPose)            ← joint별 PD 토크 적용
```

**`Object`와 `RigidBody`의 관계**
- 일반 동적 오브젝트: `Object`가 `RigidBody*` 1개 보유 (PhysicsWorld 소유)
- 캐릭터(ragdoll 비활성): 기존처럼 `AnimBlender`가 렌더 pose 주도, `RigidBody` 1개로 단순 충돌 처리
- 캐릭터(ragdoll 활성): `Ragdoll`의 bone별 `RigidBody`들이 렌더 pose를 override
- `PhysicState` 구조체는 제거하고, `RigidBody` 내부의 더블 버퍼 상태가 그 역할을 대신

---

### Phase 1 — `RigidBody` + `Collider` + `PhysicsWorld` 골격

**목표**: 기존 `PhysicSystem` + `PhysicState`를 새 구조로 완전히 교체.
시뮬레이션 결과가 없어도 컴파일·실행되는 빈 골격 완성.

**핵심 클래스**

```cpp
// rigid_body.hpp
enum class MotionType { Dynamic, Static, Kinematic };

struct RigidBodyState {
    mu::Vec3  pos{};
    mu::NQuat orient{};
    mu::Vec3  linearVel{};
    mu::Vec3  angularVel{};
    mu::Vec3  forceAccum{};
    mu::Vec3  torqueAccum{};
};

class RigidBody {
public:
    RigidBodyState  curr{}, prev{};     // 더블 버퍼 (Object::update() 보간용)
    float           invMass      = 0.f;
    mu::Mat3x3      invInertiaLocal{};
    mu::Mat3x3      invInertiaWorld{};  // 매 step 갱신
    float           restitution  = 0.3f;
    float           friction     = 0.5f;
    MotionType      motionType   = MotionType::Dynamic;
    Collider*       collider     = nullptr; // 외부 주입, 소유 안 함

    void applyForce(mu::Vec3 force, mu::Vec3 worldPoint);
    void applyImpulse(mu::Vec3 impulse, mu::Vec3 worldPoint);
    void applyTorqueImpulse(mu::Vec3 torque);
    void clearAccumulators();
    AABB worldAABB() const; // broad phase용
};

// collider.hpp
class Collider {
public:
    virtual ~Collider() = default;
    virtual AABB worldAABB(const RigidBody& body) const = 0;
};

class BVHCollider     : public Collider { BVH bvh_; ... };
class CapsuleCollider : public Collider { float radius_, halfHeight_; ... };
class TerrainCollider : public Collider { const TerrainData* data_; ... };

// physics_world.hpp
class PhysicsWorld {
public:
    RigidBody* createBody(MotionType, Collider*, float mass);
    void       destroyBody(RigidBody*);
    void       addConstraint(std::unique_ptr<Constraint>);
    void       setGravity(mu::Vec3 g);
    void       step(float dt);

private:
    void integrate(float dt);
    void generateContacts();        // broad → narrow → ContactConstraint 생성
    void solveConstraints(int iterations);

    std::vector<std::unique_ptr<RigidBody>>  bodies_;
    std::vector<std::unique_ptr<Constraint>> constraints_;  // joints + contacts
    std::unique_ptr<BroadPhase>              broadPhase_;
    mu::Vec3 gravity_{ 0.f, -9.8f, 0.f };
    int      solverIterations_ = 10;
};
```

**`Object` 측 변경**
- `PhysicState currPhysicState_, prevPhysicState_` 제거
- `RigidBody* body_` 포인터로 교체 (PhysicsWorld 소유)
- `Object::update()` 보간: `body_->prev / body_->curr` 에서 읽음
- `rebuildBVH()`: `BVHCollider::update(animBlender)` 로 이동

---

### Phase 2 — `integrate()`: 힘 적분, 중력·공기저항

**목표**: Dynamic body가 중력을 받아 낙하하고 공기저항으로 감속된다.

```
// Dynamic body만 처리
body.prev = body.curr                        // 더블 버퍼 advance

forceAccum  += gravity / invMass             // 중력
forceAccum  -= linearDamping  * linearVel    // 공기저항 (선형)
torqueAccum -= angularDamping * angularVel   // 공기저항 (각)

linearVel   += (forceAccum  * invMass)       * dt
// invInertiaWorld = R * invInertiaLocal * Rᵀ (매 step 갱신)
angularVel  += (invInertiaWorld * torqueAccum) * dt

pos    += linearVel * dt
orient += (orient * Quat(angularVel, 0)) * 0.5f * dt; normalize(orient)

clearAccumulators()
```

관성 텐서 초기화 헬퍼:
- `computeBoxInertia(mass, halfExtents)` → `Mat3x3`
- `computeCapsuleInertia(mass, radius, halfHeight)` → `Mat3x3`

---

### Phase 3 — `ContactConstraint` + Sequential Impulse Solver

**목표**: 충돌 시 반발·마찰 impulse가 정확히 적용된다. 여러 iteration으로 수렴한다.

```cpp
struct ContactPoint {
    mu::Vec3  worldPos;
    mu::Vec3  localA, localB;      // body local space 오프셋
    mu::NVec3 normal;              // A → B 방향
    float     depth;
    float     accNormal    = 0.f;  // warm start
    float     accTangent[2] = {};
};

class ContactConstraint : public Constraint {
public:
    RigidBody*   bodyA;
    RigidBody*   bodyB;
    ContactPoint contacts[4];      // 최대 4점
    int          count = 0;

    void prepare(float dt) override;    // effective mass, Baumgarte bias 계산
    void solveVelocity()   override;    // normal + Coulomb friction impulse (clamped)
    void solvePosition()   override;    // pseudo-velocity position correction (split impulse)
};
```

**Solver loop** (`PhysicsWorld::solveConstraints`)
```
generateContacts()
  → NarrowPhase: Collider 쌍별 충돌 → ContactPoint 생성
  → ContactConstraint 생성 후 constraints_ 앞에 삽입

for each constraint: constraint->prepare(dt)   // effective mass 캐싱
for iter in 0..solverIterations_:
    for each constraint: constraint->solveVelocity()
for each constraint: constraint->solvePosition()

contacts 임시 제거 (매 step 재생성)
```

---

### Phase 4 — `TerrainCollider` + 지형 충돌

**목표**: 캐릭터가 지형 표면 위에 서고, 경사면에서 마찰/미끄러짐이 발생한다.

- `TerrainCollider` 추가: `generateContacts(RigidBody& dynamic)` 메서드
  - dynamic body Collider AABB ∩ terrain 영역만 검사 (불필요한 셀 스킵)
  - BVH 리프 shape의 최하단 꼭짓점에 대해 `getHeightAt(x, z)` 조회
  - 관통 깊이 > 0 → `ContactPoint` 생성 (법선 = 지형 표면 법선)
  - 생성된 contact → `ContactConstraint` → Phase 3 solver가 처리
- `TerrainObject`의 `RigidBody`는 `MotionType::Static` (invMass = 0)

---

### Phase 5 — `BroadPhase` 추상화 + Sort-and-Sweep

**목표**: broad phase를 교체 가능한 인터페이스로 감싸고, SAP로 O(n²) → O(n log n).

```cpp
struct BodyPair { RigidBody* a; RigidBody* b; };

class BroadPhase {
public:
    virtual ~BroadPhase() = default;
    virtual void add(RigidBody* body)         = 0;
    virtual void remove(RigidBody* body)      = 0;
    virtual void update()                     = 0;  // AABB 갱신
    virtual std::vector<BodyPair> queryPairs() = 0;
};

class BruteForceBroadPhase : public BroadPhase { /* O(n²), 초기 구현 */ };
class SAPBroadPhase         : public BroadPhase { /* X축 sort-and-sweep */ };
```

`PhysicsWorld` 생성 시 `broadPhase_`를 주입 (기본 `BruteForce`, 이후 `SAP`로 교체).

---

### Phase 6 — `Constraint` 계층: Ball-Socket, Hinge, ConeTwist

**목표**: 두 RigidBody 사이에 Joint 제약을 걸 수 있다.

```cpp
class Constraint {
public:
    virtual ~Constraint() = default;
    virtual void prepare(float dt) = 0;    // effective mass, bias 계산
    virtual void solveVelocity()   = 0;    // PGS velocity-level 1 iter
    virtual void solvePosition()   = 0;    // position-level correction
protected:
    RigidBody* bodyA_ = nullptr;
    RigidBody* bodyB_ = nullptr;
};

class BallSocketJoint : public Constraint {
    mu::Vec3 anchorA_, anchorB_;    // body local space
    // 3 translational DOF 제거 (척추·손목·발목)
};

class HingeJoint : public Constraint {
    mu::Vec3 axisA_, axisB_;
    float    minAngle_, maxAngle_;
    // 1 rotational DOF만 허용 (무릎·팔꿈치)
};

class ConeTwistJoint : public Constraint {
    mu::NQuat refOrientA_, refOrientB_;
    float     coneHalfAngle_, twistLimit_;
    // swing cone + twist limit (어깨·고관절)
};
```

ContactConstraint와 모든 Joint가 동일한 PGS solver loop를 공유한다.

---

### Phase 7 — `Ragdoll` 구조

**목표**: 캐릭터 스켈레톤을 RigidBody 체인으로 빌드하고, 물리 ↔ 애니메이션 전환이 가능하다.

```cpp
struct RagdollBone {
    int         boneIdx;
    RigidBody*  body;           // PhysicsWorld 소유
    Constraint* parentJoint;    // PhysicsWorld 소유, root는 nullptr
    mu::Vec3    localPivot;     // body 로컬 기준 bone 피벗 오프셋
};

class Ragdoll {
public:
    // 스켈레톤 + 코드 내 RagdollDef(bone→capsule 매핑, joint 타입)로
    // body/joint를 PhysicsWorld에 일괄 등록
    void build(const Skeleton& skel, PhysicsWorld& world);
    void destroy(PhysicsWorld& world);

    // 현재 애니메이션 pose를 body transform에 반영 (Kinematic sync)
    void syncFromPose(const std::vector<AnimFrame>& pose);
    // body transform → 렌더용 bone transform 역산
    void syncToPose(std::vector<AnimFrame>& outPose) const;

    void activate();    // MotionType → Dynamic
    void deactivate();  // MotionType → Kinematic (애니메이션이 다시 주도)

    bool isActive() const { return active_; }

private:
    std::vector<RagdollBone> bones_;
    bool active_ = false;
};
```

`Object`는 `std::optional<Ragdoll> ragdoll_` 보유.
- 비활성: `AnimBlender`가 RenderState pose 주도, `body_` 1개로 단순 충돌
- 활성: `ragdoll_.syncToPose()` 결과가 렌더 bone을 override

---

### Phase 8 — `ActiveRagdollController`

**목표**: ragdoll 활성 중에도 애니메이션 pose를 향해 스스로 균형을 잡는다.

```cpp
class ActiveRagdollController {
public:
    struct Gains { float kp = 300.f; float kd = 30.f; };

    // joint별 PD 토크 적용:
    //   error  = quatDiff(bone.curr.orient, targetOrient)
    //   torque = kp * error - kd * bone.angularVel
    //   body.applyTorqueImpulse(torque * dt)
    void update(Ragdoll& ragdoll,
                const std::vector<AnimFrame>& targetPose,
                float dt);

    // 충격 수신 시 kp 일시 감소 → 흐물흐물한 반응 후 복원
    void onImpact(float impulseStrength);

    Gains defaultGains_{};

private:
    float impactRecoveryTimer_ = 0.f;
};
```




// UI(hp, inventory, login, loading), Effect, goal-based AI, clustered AI: 5월 초 게임 시작->집단 전투 컨텐츠 완성

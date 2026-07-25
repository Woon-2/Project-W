# Client Code Index

> **규칙:** 파일/기능 위치를 찾을 때는 **이 파일을 먼저 조회**한다.
> 코드를 수정한 후에는 **해당 항목의 라인 번호를 반드시 갱신**한다.
> 새 클래스/함수를 추가하면 **이 파일에 항목을 추가**한다.

---

## 목차

1. [충돌 / BVH](#1-충돌--bvh)
2. [물리 시뮬레이션](#2-물리-시뮬레이션)
3. [전투 시스템](#3-전투-시스템)
4. [이벤트 시스템](#4-이벤트-시스템)
5. [애니메이션](#5-애니메이션)
6. [메시 / 모델 / 스켈레톤](#6-메시--모델--스켈레톤)
7. [게임 오브젝트 (Object 계층)](#7-게임-오브젝트-object-계층)
8. [렌더링 (GFX / Pipeline)](#8-렌더링-gfx--pipeline)
9. [게임 루프](#9-게임-루프)
10. [디버그 시각화](#10-디버그-시각화)
11. [UI 시스템](#11-ui-시스템)
12. [스킬 에디터 (standalone)](#12-스킬-에디터-standalone)
13. [지면 연계 스킬 / 파티클](#13-지면-연계-스킬--파티클-terrain-interaction)
14. [사운드 시스템](#14-사운드-시스템)
15. [인벤토리 시스템](#15-인벤토리-시스템)

---

## 1. 충돌 / BVH

**파일:** `client/collision.hpp` / `client/collision.cpp`

| 항목 | 위치 | 설명 |
|------|------|------|
| `AABB` struct | `collision.hpp #5-8` | center + size |
| `OBB` struct | `collision.hpp #10-14` | center + halfExtents + orient(NQuat) |
| `Ray` struct | `collision.hpp #16-19` | origin + dir |
| `CollisionResult` struct | `collision.hpp #21-27` | hit, normal, mtv, depth, contactPoint |
| `BVHNode` struct | `collision.hpp #34-43` | bounds(AABB) + shape(AABB\|OBB) + children + boneName + boneIdx |
| `BVH` struct | `collision.hpp #46-50` | linearized N-ary tree, nodes[0]=root |
| `CollisionVolume` alias | `collision.hpp #52` | `using CollisionVolume = BVH` |
| `collides(AABB, AABB)` | `collision.hpp #54` | AABB-AABB 교차 |
| `collides(OBB, OBB)` | `collision.hpp #55` | 15축 SAT; normal = B→A convention |
| `collides(BVH, BVH)` | `collision.hpp #56` | BVH-BVH 재귀 교차 (leaf-leaf 쌍에서만 정밀 판정; 내부 노드는 bounds AABB fast-reject만 사용) |
| `collides(BVH, AABB)` | `collision.hpp #57` | BVH vs 공격 hitbox |
| `RayHit` / `RaycastAABB` | `collision.hpp #62-69` | 레이-AABB 교차 |
| `RaycastOBB` | `collision.hpp #70` | OBB 로컬 공간으로 ray 변환 후 RaycastAABB, normal 월드 복원 |
| `RaycastBVH` | `collision.hpp #71` | 고정 크기 스택(64) BVH 순회; leaf에서 AABB/OBB std::visit 분기 |
| `buildAttackAABB` | `collision.hpp #73` | pos + forward + halfExtent + offsetFwd → AABB |

**bone 연결 BVH 월드 변환 체인:**
```
bone.toDress  *  finalXformData()[boneIdx]  *  objWorld
```
- 적용 위치: `object.cpp` `Object::rebuildBVH()` 및 `Object::render()` 장비 소켓 렌더링

---

## 2. 물리 시뮬레이션

**파일:** `client/rigidBody.hpp` / `client/rigidBody.cpp`
**파일:** `client/physicsWorld.hpp` / `client/physicsWorld.cpp`
**파일:** `client/constraint.hpp`
**파일:** `client/contactConstraint.hpp` / `client/contactConstraint.cpp`
**파일:** `client/staticDepenetration.hpp` / `client/staticDepenetration.cpp`
**파일:** `client/broadPhase.hpp` / `client/broadPhase.cpp`
**파일:** `client/jointConstraint.hpp` / `client/jointConstraint.cpp`
**파일:** `client/ragdollDef.hpp` / `client/ragdollDef.cpp`
**파일:** `client/ragdoll.hpp` / `client/ragdoll.cpp`
**파일:** `client/activeRagdoll.hpp` / `client/activeRagdoll.cpp`

| 항목 | 위치 | 설명 |
|------|------|------|
| `MotionType` enum | `rigidBody.hpp #9` | Kinematic / Dynamic / Static |
| `BodyState` struct | `rigidBody.hpp #16-22` | pos, linearVel, omega, orient, scale (더블 버퍼 1개) |
| `RigidBody` class | `rigidBody.hpp #35` | prev/curr 더블 버퍼 + worldBVH_ + Dynamic 물리 멤버 |
| `RigidBody::advanceState()` | `rigidBody.hpp #59` | prev ← curr (step 첫 줄) |
| `RigidBody::snapToCurrent()` | `rigidBody.hpp #63` | prev = curr (텔레포트/초기화) |
| `RigidBody::worldBVH()` | `rigidBody.hpp #69` | 월드 공간 BVH |
| `RigidBody::worldAABB()` | `rigidBody.hpp` | BroadPhase용 AABB (root BVH bounds 또는 단위 박스) |
| `RigidBody::setMass()` | `rigidBody.cpp` | invMass + 기본 box 관성 초기화 |
| `RigidBody::setInertia()` | `rigidBody.cpp` | 로컬 관성 텐서 역행렬 저장 |
| `RigidBody::applyForce()` | `rigidBody.cpp` | force accumulator에 추가 |
| `RigidBody::applyImpulse()` | `rigidBody.cpp` | 즉시 vel/omega 변경 |
| `RigidBody::applyTorqueImpulse()` | `rigidBody.cpp` | 즉시 omega 변경 (joint/PD 토크용) |
| `RigidBody::setGravityScale()` / `gravityScale()` | `rigidBody.hpp #110` | per-body 중력 배율(Unity gravityScale 등가, 기본 1). integrate Dynamic 분기에서 `gravity_ * gravityScale()`; 접지 중력 게이팅이 0/1 토글 |
| `computeBoxInertia()` | `rigidBody.hpp #26` | 박스 관성 텐서 헬퍼 |
| `computeCapsuleInertia()` | `rigidBody.hpp #27` | 캡슐 관성 텐서 헬퍼 |
| `Constraint` (abstract) | `constraint.hpp #12` | prepare/solveVelocity/solvePosition 인터페이스 |
| `ContactPoint` struct | `collision.hpp` | worldPos, normal(B→A), depth, acc 누적값 |
| `ContactConstraint` class | `contactConstraint.hpp` | PGS Normal + Coulomb 마찰 impulse solver; setExternalAccels()로 외력 보상 |
| `ContactConstraint::setExternalAccels()` | `contactConstraint.hpp` | 외력 가속도 설정 (prepare() 전 호출); Baumgarte bias에 외력 보상항 추가 |
| `ContactConstraint::isTerrainContact()` | `contactConstraint.hpp #49` | terrain support 접촉 여부 getter (게임 레이어 접지 판정용) |
| `StaticContact` struct | `staticDepenetration.hpp` | Static-Dynamic/Static-Kinematic 충돌 레코드; normal은 static→movable; ContactConstraint solver 우회 |
| `resolveStaticPenetration()` | `staticDepenetration.hpp/cpp` | static 침투 positional depenetration(partial+slop+clamp) + inward normal-vel 클램프; 회전·분리속도 미주입; Kinematic도 직접 이동(invMass 무관); 이동 body는 outMoved로 반환(BVH 재빌드용) |
| `staticdepen::kSlop/kCorrectFrac/kMaxCorrect` | `staticDepenetration.hpp` | 진동 방지 파라미터 0.005m / 0.8 / 0.2m |
| `RigidBody::setUserData()` / `userData()` | `rigidBody.hpp` | void* 게임 레이어 연결 포인터 (Object* 역참조용) |
| `PhysicsWorld::forEachContact()` | `physicsWorld.hpp` | step() 후 활성 ContactConstraint 순회 (템플릿) |
| `PhysicsTestObject` struct | `physicsTestObject.hpp #51` | bodies/halfExtents/joints/ignoredPairs 소유; activate/deactivate/visualize/applyImpulseAll/freezeAll |
| `PhysicsTestObject::ignoredPairs` | `physicsTestObject.hpp` | 1-hop+2-hop 충돌 무시 쌍; factory 함수가 채우고 activate/deactivate에서 setIgnoreCollision 호출 |
| `makePendulum()` | `standalone/game.cpp #75` | PhysicsTestObject factory: BallSocket 단진자 (kind=1) |
| `makeDoublePendulum()` | `standalone/game.cpp #100` | kind=2: BallSocket 이중 진자 |
| `makeHingeDoor()` | `standalone/game.cpp #136` | kind=3: HingeJoint 문 |
| `makeConeTwistArm()` | `standalone/game.cpp #170` | kind=4: ConeTwist 단일 팔 |
| `makeConeTwistChain()` | `standalone/game.cpp #203` | kind=5: ConeTwist 5-link 체인 |
| `makeHumanoidRagdoll()` | `standalone/game.cpp #244` | kind=6: 12 bodies A-pose, 11 joints(ConeTwist×7+Hinge×4), 1-hop+2-hop 충돌 무시; 팔·다리 twist axis=(0,-1,0) (A-pose 기준) |
| `makeUpperBodyRagdoll()` | `standalone/game.cpp #~427` | kind=7: Kinematic Hips anchor + Dynamic 상체 7 bodies(척추·머리·팔), 7 joints |
| `makeLowerBodyRagdoll()` | `standalone/game.cpp #~517` | kind=8: Kinematic Hips anchor + Dynamic 하체 4 bodies(양쪽 upper/lower leg), 4 joints |
| `BodyPair` struct | `broadPhase.hpp` | broad phase 결과 쌍 |
| `BroadPhase` (abstract) | `broadPhase.hpp #36-40` | add/remove/update/queryPairs/queryAABB 인터페이스 |
| `BroadPhase::queryAABB` | `broadPhase.hpp #39` | AABB 쿼리 순수 가상 메서드 — 카메라 arm 장애물 후보 조회 |
| `BruteForceBroadPhase` | `broadPhase.hpp #50` | O(n²) 참조 구현 (후보 비교용으로 보존) |
| `SAPBroadPhase` | `broadPhase.hpp #74` | X축 Sort-and-Sweep, O(n log n) (기본 사용) |
| `SAPBroadPhase::queryAABB` | `broadPhase.hpp #74` | 정렬된 endpoints + active-set sweep으로 box 겹침 후보 반환 |
| `TerrainHeightField` struct | `terrain.hpp` | CPU-side 높이 데이터 (getHeightAt, getNormalAt) |
| `WorldCollider` (추상) / `ContactSink` | `collision.hpp` | 정적 환경 콜라이더 베이스(footprintReject/generateContacts/queryArm). terrain·scatter 공통; ContactSink로 자기 의미의 contact append |
| `TerrainCollider : WorldCollider` | `collision.hpp` | Dynamic body ↔ 지형 높이맵 support ContactConstraint 생성(+queryArm=N6 지면샘플) |
| `ScatterCollider : WorldCollider` | `collision.hpp`/`.cpp` | chunk당 1개 정적 prop 콜라이더; collidable 인스턴스 world BVH 1회 베이크+XZ uniform grid, StaticContact(push-out) 생성, queryArm=RaycastBVH(카메라) |
| `transformShapeRigid()` / `makeWorldBVH()` | `collision.hpp`/`.cpp` | 비본 강체 world-BVH 변환(회전 시 AABB→OBB). Object::rebuildBodyBVH 비본 경로 + ScatterCollider 공유 |
| `PhysicsWorld` class | `physicsWorld.hpp` | 시뮬레이션 진입점 |
| `PhysicsWorld::registerBody()` | `physicsWorld.hpp #37` | body + onRebuildBVH 콜백 + collisionGroup/Mask + broad phase 등록 |
| `PhysicsWorld::unregisterBody()` | `physicsWorld.hpp #43` | 등록 해제 |
| `PhysicsWorld::addJointConstraint()` | `physicsWorld.hpp #47` | 소유권 이전 joint 등록 |
| `PhysicsWorld::removeJointConstraint()` | `physicsWorld.hpp #48` | 소유 joint 제거 |
| `PhysicsWorld::addJointRef()` | `physicsWorld.hpp #52` | 비소유 joint ref 등록 (Ragdoll용) |
| `PhysicsWorld::removeJointRef()` | `physicsWorld.hpp #53` | 비소유 joint ref 제거 |
| `PhysicsWorld::setIgnoreCollision()` | `physicsWorld.hpp #58` | 특정 body 쌍의 충돌 완전 무시 (symmetric). Ragdoll이 joint 연결/2-hop 쌍 등록에 사용 |
| `PhysicsWorld::ignoreCollisionPairs_` | `physicsWorld.hpp #182` | normKey 정규화된 per-pair ignore set; generateContacts()에서 group/mask 이후 체크 |
| `PhysicsWorld::registerTerrain()` | `physicsWorld.hpp` | TerrainCollider를 worldColliders_에 등록 → `TerrainHandle` 반환(시그니처 불변) |
| `PhysicsWorld::unregisterTerrain(handle)` | `physicsWorld.hpp` | 핸들로 collider 해제 (SlotVector tombstone 재사용) |
| `PhysicsWorld::registerScatter()`/`unregisterScatter()` | `physicsWorld.hpp`/`.cpp` | 청크 ScatterCollider 등록/해제(terrain과 동일 registry·핸들 공간) |
| `PhysicsWorld::worldColliders_` | `physicsWorld.hpp` | `SlotVector<unique_ptr<WorldCollider>>`(terrain+scatter 통합); generateContacts/queryCameraArm가 Dynamic body마다 전 collider 순회(footprintReject) → `SlotVector<T>`는 `common/slotVector.hpp` |
| `PhysicsWorld::registerCameraObstacle()` | `physicsWorld.hpp #67` | body를 카메라 obstacle로 cameraBroadPhase_에 등록 |
| `PhysicsWorld::unregisterCameraObstacle()` | `physicsWorld.hpp #68` | 카메라 obstacle 등록 해제 |
| `PhysicsWorld::queryCameraArm()` | `physicsWorld.hpp #73` | pivot→desiredEye arm 허용 길이 반환 (worldColliders_.queryArm: 지형 N=6 샘플 + scatter RaycastBVH, 이후 obstacle broad phase) |
| `PhysicsWorld::cameraBroadPhase_` | `physicsWorld.hpp #140` | 카메라 전용 SAPBroadPhase 인스턴스 (일반 physicsWorld broadPhase와 독립) |
| `PhysicsWorld::step()` | `physicsWorld.hpp #63` | (substep) integrate → generateContacts → solveConstraints → applyPseudoVelocity → resolveStaticPenetration + moved body BVH 재빌드 |
| `PhysicsWorld::staticContacts_` / `movedByStaticDepen_` | `physicsWorld.hpp` | step별 static 충돌 레코드(broad-phase static 분기 + ScatterCollider) + depenetration으로 직접 이동된 body dirty set |
| `PhysicsWorld::generateContacts()` static 분기 | `physicsWorld.cpp` | static 포함 쌍(aStatic != bStatic)은 StaticContact로 라우팅 후 continue(ContactConstraint 미생성); normal을 static→movable로 정규화 |
| `PhysicsWorld::setGravity()` | `physicsWorld.hpp #67` | Dynamic body 중력 설정 |
| `PhysicsWorld::setSolverIterations()` | `physicsWorld.hpp #70` | velocity PGS 반복 횟수 (기본 4, ragdoll 활성 시 16) |
| `PhysicsWorld::setPositionSolveIterations()` | `physicsWorld.hpp #87` | split impulse position correction 반복 횟수 (기본 3, ragdoll 활성 시 4) |
| `PhysicsWorld::interpolatePos()` | `physicsWorld.hpp #73` | 렌더 보간 헬퍼 (prev→curr, t) |
| `PhysicsWorld::interpolateOrient()` | `physicsWorld.hpp #74` | 렌더 보간 헬퍼 (slerp) |
| `BallSocketJoint` class | `jointConstraint.hpp #16` | 3 translational DOF 제거, bilateral warmstart |
| `HingeJoint` class | `jointConstraint.hpp #50` | 1 rotational DOF, angle limits, refOrient |
| `ConeTwistJoint` class | `jointConstraint.hpp #103` | swing cone + twist limit; kLinBeta=0.1/kAngBeta=0.05/kSplitBeta=0.3; solvePosition()으로 split-impulse angular 보정 |
| `JointType` enum | `ragdollDef.hpp #8` | BallSocket / Hinge / ConeTwist |
| `BoneBoxDef` struct | `ragdollDef.hpp #11` | boneName(string), halfExtents, center, rotEuler, mass |
| `JointDef` struct | `ragdollDef.hpp #20` | parentBoneName(string), childBoneName(string), type, limits |
| `RagdollDef` struct | `ragdollDef.hpp #34` | vector<BoneBoxDef> + vector<JointDef> |
| `getHumanoidRagdollDef()` | `ragdollDef.cpp #10` | Unity Humanoid 뼈대 정의 (static 싱글턴) |
| `importRagdollConfig()` | `mesh.cpp` | binary → Model::ragdollDef 로드 |
| `Model::ragdollDef` | `mesh.hpp` | std::optional<RagdollDef>, 파일에서 로드된 ragdoll 설정 |
| `Model::baseScale` | `mesh.hpp` | Unity root localScale(`ModelScale` 필드); `Object::setModel`이 흡수해 `modelBaseScale_⊙instanceScale_`로 body scale 적용 (vertex bake 대체, graphicsArchitecture.md 참조) |
| `Object::applyCompositeScale()` | `object.cpp` | `body_.scale()=modelBaseScale_⊙instanceScale_` 합성 + rebuildBodyBVH. setModel/setScale에서 호출 |
| `RagdollBone` struct | `ragdoll.hpp #15` | boneIdx, body*(non-owning), parentJoint*(non-owning), capsuleOffset |
| `Ragdoll` class | `ragdoll.hpp #33` | bone별 RigidBody + Constraint 소유, PhysicsWorld 비소유 등록 |
| `Ragdoll::build()` | `ragdoll.cpp` | 스켈레톤 + def → body/joint 생성 + world 등록 (modelScale 인자로 halfExtents/관성에 모델 scale 반영) |
| `Ragdoll::destroy()` | `ragdoll.cpp` | joint 먼저, body 나중 제거 (dangling ptr 방지) |
| `Ragdoll::syncFromPose()` | `ragdoll.cpp` | AnimFrame pose → body pos/orient (DFS) |
| `Ragdoll::seedFromFinalXforms()` | `ragdoll.cpp` | AnimBlender finalXformData → body pos/orient |
| `Ragdoll::syncToPose()` | `ragdoll.cpp` | body pos/orient → AnimFrame pose (DFS) |
| `Ragdoll::syncToFinalXforms()` | `ragdoll.cpp` | body transform → finalXforms 덮어씀 + passenger 본 재구성 |
| `Ragdoll::buildPassengers()` | `ragdoll.cpp` | 비-body 본 → 최근접 ragdoll body에 강체 바인딩 (DFS 조상 + 고아 본 BFS 2-pass) |
| `Ragdoll::activate()` | `ragdoll.cpp` | Kinematic → Dynamic |
| `Ragdoll::deactivate()` | `ragdoll.cpp` | Dynamic → Kinematic |
| `ActiveRagdollController` class | `activeRagdoll.hpp #25` | PD 토크 컨트롤러, 피격 반응 포함 |
| `ActiveRagdollController::update()` | `activeRagdoll.cpp` | DFS로 bone별 PD 토크 적용 |
| `ActiveRagdollController::onImpact()` | `activeRagdoll.cpp` | kp 일시 감소 (limp 반응) |
| `ActiveRagdollController::computeOrientError()` | `activeRagdoll.cpp` | q.w<0 처리 포함 최단경로 오차 |

**Object 내 물리 상태 접근:**

| 항목 | 위치 | 설명 |
|------|------|------|
| `Object::body_` (protected) | `object.hpp` | 인라인 `RigidBody` (항상 유효) |
| `Object::ragdoll_` (protected) | `object.hpp` | `unique_ptr<Ragdoll>` (비활성 시 null) |
| `Object::body()` | `object.hpp` | RigidBody 참조 (PhysicsWorld 등록 시 &body() 전달) |
| `Object::worldBVH()` | `object.hpp` | `body_.worldBVH()` 위임 (CombatSystem 호환) |
| `Object::rebuildBodyBVH()` | `object.cpp` | BVH 월드 공간 재빌드 (PhysicsWorld 콜백으로도 사용) |
| `Object::enableRagdoll()` | `object.cpp` | build + seedFromFinalXforms + activate (solverIter=20) |
| `Object::disableRagdoll()` | `object.cpp` | destroy + reset (solverIter=10 복원) |
| `Object::hasActiveRagdoll()` | `object.hpp` | ragdoll 활성 여부 |
| ragdoll finalXform override | `object.cpp Object::update()` | 활성 시 finalXformData를 body 위치로 직접 덮어씀 |

---

## 3. 전투 시스템

**파일:** `client/standalone/combatSystem.hpp` / `client/standalone/combatSystem.cpp`

| 항목 | 위치 | 설명 |
|------|------|------|
| `CombatConfig` struct | `combatSystem.hpp #12-17` | attackHalfExtent, attackOffsetFwd, damage, cooldown |
| `AttackSpec` struct | `combatSystem.hpp #33-37` | 읽기 전용 공격 파라미터 (디버그용) |
| `CombatSystem` class | `combatSystem.hpp #39-73` | 등록/공격 판정/AI 쿨타임 |
| `registerCombatant()` | `combatSystem.hpp #42` | 씬 셋업 시 등록 |
| `unregister()` | `combatSystem.hpp #45` | 등록 해제 |
| `onPlayerAttack()` | `combatSystem.hpp #50` | LButton 클릭 → EvHit 발생 |
| `update()` | `combatSystem.hpp #55` | 매 프레임 몬스터 AI 쿨타임 + 공격 판정 |
| `queryAttackSpec()` | `combatSystem.hpp #60` | 공격 사양 조회 (부작용 없음) |
| `overlapsAny()` (private) | `combatSystem.hpp #72` | AABB hitbox vs target BVH |

---

## 3-B. 스킬 시스템

**파일:** `client/skill/skillSystem.hpp` / `client/skill/skillSystem.cpp`
**설계 문서:** `client/docs/skillArchitecture.md` (서버: `RoomServer/docs/skillArchitecture.md`)

| 항목 | 위치 | 설명 |
|------|------|------|
| `AttachedHitbox` struct | `skillSystem.hpp` | worldOBBs + `worldAABB`(broad phase 캐시) + `targetMask`(피아 식별) + onHit + hitGroup |
| `SkillInstancePool` struct | `skillSystem.hpp` | 동적 풀: `instances`(vector) + `freeList` + `activeList` |
| `SkillBroadPhase` class | `skillSystem.hpp` | Object–Hitbox bipartite sweep-and-prune; `HitboxEntry.mask`×`TargetEntry.category` 마스크 필터 |
| `SkillBroadPhase::build()` | `skillSystem.cpp` | 엔드포인트 정렬 → sweep, `(mask & category)` 후 hitbox×target 쌍만 emit |
| `SkillSystem::update()` | `skillSystem.cpp` | activeList 순회 → updateHitboxes → checkHitboxCollisions → processHitResults |
| `checkHitboxCollisions()` | `skillSystem.cpp` | 타깃 1회 수집(category=factionBit) → SkillBroadPhase → 후보 쌍 narrow phase(BVH vs OBB) |
| `updateParticleHitboxSources()` | `skillSystem.cpp` | VFXParticle: 핸들 재사용으로 파티클 수만큼 증감, `targetMask` 전파, 프레임별 `particleLocalIdx` 기록 |
| `processHitResults()` 비관통 처리 | `skillSystem.cpp` | `penetrate=false` 히트박스 명중 시 소스 파티클을 `killParticle`로 소멸(소스별 인덱스 내림차순); `debugStats().particlesDestroyedOnHit` 카운트. `particleHitboxDeterminism.md` §8 |
| `Faction` enum / `hostileMask()` | `object.hpp` | 피아 식별: Neutral/Players/Monsters; 히트박스 targetMask = hostileMask(owner.faction) |
| `Object::faction()`/`setFaction()` | `object.hpp` | 진영 접근자(생성 지점에서 setFaction 호출) |
| `SkillInstance::seed` / `startSkill(..., seed)` | `skillSystem.hpp/.cpp` | per-cast 결정론 시드 (C/S_SkillStart로 공유); PlayVFX에서 `fx->setDeterministicSeed(mixSeed(seed, vfxId))` |
| `SkillAsset::VfxDef/VfxSystemDef` | `skillTypes.hpp` | `addVFX(id, path, { systems = ... })` 구성 — 서버 결정론 히트박스용 (`docs/particleHitboxDeterminism.md`) |
| `SkillCompiler::compileAll` lua 정렬 | `skillCompiler.cpp` | 파일명 정렬 후 순차 id 부여 — 서버 컴파일러와 id 일치 보장 (C_SkillStart가 숫자 id 전송) |
| `buildVfxGameplayConfigs` | `skillCompiler.{hpp,cpp}` | addVFX systems(JSON+오버라이드) → `VfxSystemDef::gameplayCfg` 1회 빌드 (서버 미러: RoomServer AssetManager에서 호출) |
| `parseVfxSystemOverrides` / `pg::VfxSystemOverrides` | `skillCompiler.cpp` / `../common/particleGameplay.hpp` | lua systems 엔트리의 게임플레이 오버라이드 (speed/lifetime/shape/bursts/volLinear 등) |
| `SkillSystem::bindVfxGameplayConfigs` | `skillSystem.cpp` | 프리빌드 설정을 ParticleEffect 시스템에 주입 (이펙트 구성 완료 후 1회 호출) |
| `Online::Game::castSkillByName` | `online/onlineGame.cpp` `processInputGame` | 휠클릭=선택 스킬 사용, 좌클릭=기본 공격(둘 다 스킬 시전). seed 생성+startSkill+C_SkillStart. (구 임시 1~0/Shift 키맵은 제거됨 → 3-C 다이얼) |
| `Online::Game::skillVfxById_[1..18]` | `online/onlineGame.cpp` | standalone과 동일한 vfxId→ParticleEffect 바인딩 (PlayVFX 해상도) |
| `Online::Game::sendSkillStartPacket(assetId, seed)` | `online/onlineGame.cpp` | `C_SkillStart{assetId, clientMs, skillSeed}` 송신 (clientMs=ClientApp::clientMs) |
| `Online::Game::onSkillStart(ownerId, assetId, elapsedMs, seed)` | `online/onlineGame.cpp` | `S_SkillStart` 수신: `EvAttack` post + `refreshSkillCtx` + `startSkill(prediction, elapsedMs, seed)`. seed로 캐스터와 동일 파티클 재현 |
| `Online::Game::onSkillHit(attackerId, targetId, newHp, assetId, targetVelocity)` | `online/onlineGame.cpp` | `S_SkillHit` 수신: 킬 시 `setRagdollInitVelocity(targetVelocity)` → `applyHit`(EvHit/EvDeath) → 타깃 위치에 hit VFX |
| PlayVFX aim pitch 합성 | `client/skill/skillSystem.cpp #610` (서버 미러: `RoomServer/skill/skillSystem.cpp` PlayVFX) | `aim = eulerOff·rotateXH(aimPitch)·baseRot` — 캐스터 조준 pitch로 발사 프레임 기울임(활/완드 궤적). YawOnly/GroundSnap/본attach 제외. `C/S_MouseMove.pitchRadian`(연속)+`C/S_SkillStart.aimPitchRadian`(시전 스냅) — `docs/aimPitchUpperBodyMask.md` |
| `AttachType::Body` (애니 독립 멜리) | `client/skill/skillSystem.cpp computeAttachTransform` Body 분기 (서버 미러 `RoomServer/skill/skillSystem.cpp`) | 본 rest 프레임(`toDress`)+aim pitch(본 원점 피벗), 재생 클립 무관. OBB는 BoneAttach와 동일 본-로컬 공간. resolveAttach/dispatch/updateHitboxes/collectActiveHitboxes가 Bone과 동일 경로로 Body 처리. lua `BodyAttach("spine_01",{pitch=})`. 플레이어 근접 8종이 사용 — `docs/aimPitchUpperBodyMask.md` §6.5 |

> 서버 전용 차이(damageCoeff, ServerAnimController 변환)는 `RoomServer/skill/skillSystem.*` 및 서버 설계 문서 참조.
> VFXParticle 히트박스의 클라/서버 결정론 동기화: `docs/particleHitboxDeterminism.md`.

---

## 3-C. 스택형 스킬 충전 (Stack-Charge) — 다이얼 HUD

**설계 문서:** `docs/skillChargeSystem.md`. 몬스터 처치(최근 15초 데미지 기여자 전원) → 선택 스킬에만 charge, 스택+쿨다운 2중 게이트, 콤보 가속·소프트캡. 서버 권위(클라 즉시 시전+재검증).

| 항목 | 위치 | 설명 |
|---|---|---|
| 스킬 메타(`weaponType/loadoutSlot/isBasic/chargeCost/cooldown`) | `client|RoomServer/skill/skillTypes.hpp` `SkillAsset` | skill lua에서 파싱(`skillCompiler.cpp::tableToAsset`) |
| `SkillLoadout::build()` | `client|RoomServer/skill/skillLoadout.hpp` | 컴파일된 자산 → 무기별 {기본, 3슬롯 assetId/코스트/쿨} |
| 신규 패킷 | `ServerEngine/protocol.hpp` | `C_SelectSkill / S_SkillSelect / S_SkillCharge / S_SkillUseReject / S_ComboState / S_PlayerHp`(서버 권위 HP 회복 푸시, 이벤트·애니 없음) (사용 요청은 기존 `C_SkillStart` 재사용) |
| `ChargeConfig` | `RoomServer/chargeConfig.{hpp,cpp}` | `resources/data/chargeConfig.lua`(몬스터 charge·윈도우·콤보·소프트캡·`regen{basePerSec,capPerSec,halfCombo,exponent}`) sol2 로드, `AssetManager` 전 룸 공유. `hpRegenPerSec(combo)`=S자(Hill) `base+(cap-base)·xⁿ/(xⁿ+halfComboⁿ)`(높은 문턱: half=10,n=3) |
| `Player` 충전 상태 | `RoomServer/object.hpp` | `selectedSlot_/skillCharge_[3]/cooldownEnd_[3]/comboCount_/lastCreditMs_/hpRegenAccum_/lastSyncedHp_` |
| `Object` 데미저 로그 + reward | `RoomServer/object.hpp` | `killChargeReward_`(스폰 시 `setupGoblin`에서 주입), `noteDamager`/`collectRecentDamagers` |
| `Room::noteAndMaybeReward / distributeKillCharge` | `RoomServer/Room.cpp` | 데미지 기록 + HP 0 전이 시 최근 데미저 선택슬롯에 `reward×소프트캡`(콤보 가속 제거됨), `S_SkillCharge`/`S_ComboState` |
| `Room::updatePlayerRegen` | `RoomServer/Room.cpp` | 60fps 틱 콤보 기반 HP 회복 적분(`hpRegenAccum_` carry, `kPlayerMaxHp` 상한), 변경분만 ~10Hz `S_PlayerHp` 브로드캐스트 |
| `Room::selectSkill / skillStart`(게이트) / `updateComboExpiry` | `RoomServer/Room.cpp` | 선택 동기화·사용 게이트(스택/쿨, 실패 시 `S_SkillUseReject`)·콤보 만료 |
| `SkillDialHUD` | `client/ui/skillDialHUD.{hpp,cpp}` | 우하단 소형 120° 회전 휠(선택=꼭대기), 반투명 회색 도넛 배경(effectMode 3) 위에 3 아이콘 배치, 슬롯별 충전/스택, ×N 배지, 0→1 준비 펄스. 크기·도넛 상수는 `.cpp` 상단(`kRadius/kSelSize/kRingPad/kRingHole`) |
| 충전 fill / 도넛 셰이더 | `client/ui.hlsl` + `uiPipeline.*` | `DrawEvent.fillAmount/effectMode`, `FrameData.time`, `Material.cRoughness/cMetallic` 재활용. mode 1=충전(어두운 base+밝은 fill) / 2=준비 / 3=절차적 반투명 도넛(텍스처 미샘플, `cRoughness`=안쪽 구멍 반지름). 아래서부터 일렁이는 액체 |
| HUD z-order | `online/onlineGame.cpp::renderInGame` | 다이얼+콤보는 `uiManager_.render` **이전**에 제출 → 설정 패널(uiManager 오버레이)이 항상 위에 그려짐(UI는 제출 순서=그리기 순서) |
| 스킬 아이콘 | `client/AssetManager.*` `skillIconByAssetName()` | 12개 명시 멤버(`resources/UI/*.dds`) |
| 무기 모델 | `client/AssetManager.*` `playerWeaponModel(PlayerWeaponType)` | `modelKatana_/modelSpearHook_/modelCrystalWand_/modelHeavyArrow_` 4개 명시 멤버(`resources/models/{sword,spear,wand,bow}/*.bin`), Phase 1(`loadLobbyVisualAssets`)에서 로드. 장착은 공용 자유 함수 `equipPlayerWeapon()`(object.{hpp,cpp}) — **무기 모델의 단일 SocketOffset 키(SocketType)를 읽어 해당 손에 장착**(Bow=왼손, 나머지=오른손; 하드코딩 RightHand 제거) + 캐릭터 `AnimBlenderPlayer::setWeaponType` 호출로 무기별 클립 세트 적용. online(로비 포트레이트·인게임)과 standalone 에디터 무기 드롭다운이 공유 |
| 입력 | `online/onlineGame.cpp` `processInputGame` / `receiveWndMsg`(WM_MOUSEWHEEL) | 휠=선택+회전+`C_SelectSkill`, 휠클릭=사용(자체 게이트+예측 쿨), 좌클릭=기본. `setupSkillDial`/`sendSelectSkillPacket` |
| 수신 핸들러 | `online/onlineGame.cpp` | `onSkillCharge/onSkillSelect/onSkillUseReject/onComboState/onPlayerHp` (준비 시 `skill_ready` 사운드, 다이얼 위 콤보 카운터; `onPlayerHp`=`idPlayerMap_` 대상 `setHp`만, 매 프레임 HP UI가 반영) |

> 시안: `docs/skill_hud_mockup/radial_dial.html`. 남은 폴리시(파티원 HUD 스택·콤보 바·사운드 자산·밸런스)는 설계 문서 §7 참조.

---

## 4. 이벤트 시스템

**파일:** `client/event.hpp` / `client/event.cpp`

| 항목 | 위치 | 설명 |
|------|------|------|
| 풀 관리 (`gPool4`, `gPool16`) | `event.hpp #15-16` | 이벤트 메모리 풀 (4B / 16B) |
| `holdEvent` macro | `event.hpp #41-46` | 풀 할당 + placement new |
| `clearEvents` macro | `event.hpp #48-57` | 이벤트 리스트 전체 해제 |
| `EventList` alias | `event.hpp #62` | `std::list<char*>` |
| `EventType` enum | `event.hpp #67-78` | Hit, Blood, Death, Attack, Respawn, SkillHit, CameraShake, VFXSpawn |
| `BasicEvent` struct | `event.hpp #84-86` | 공통 base (type 필드) |
| `EvHit` struct | `event.hpp #88-100` | targetId, hp, `hitAnimIndex`(u8): 서버 권위 선택 피격 리액션 클립 인덱스(다중 hit 리그 Boss=Hit1/Hit2). 단일 hit 몬스터는 무시. `S_SkillHit`→`onSkillHit`→`applyHit`→`EvHit` 전파 |
| `EvBlood` struct | `event.hpp #96-101` | victimId |
| `EvDeath` struct | `event.hpp #102-107` | victimId |
| `EvAttack` struct | `event.hpp #110` | attackerId + `attackIndex`(u8): AnimBlender의 `attackClips_` 인덱스로 어떤 공격 클립을 재생할지 선택. `PlayAnimation.attackIndex`에서 전파(skillSystem.cpp PlayAnimation case). 플레이어도 무기별 `attackClips_`에 동일 적용 |
| `PlayAnimation` 이벤트 | `skill/skillTypes.hpp` | `clipName[32]`(클·서버 미러; 서버가 플레이어 공격 클립을 이 이름으로 `switchClip`)+`blendTime`+`attackIndex`. 클라는 attackIndex로, 서버는 clipName으로 클립 선택. 서버 핸들러는 owner가 `isPlayer()`일 때만 동작(몬스터는 AI 구동, no-op) — `RoomServer/skill/skillSystem.cpp` |
| `EvRespawn` struct | `event.hpp #114-119` | targetId (부활 애니메이션 트리거) |
| `IEventBus` interface | `event.hpp #117-134` | `receive()` 순수 가상 |
| `NullEventBus` | `event.hpp #136-139` | 아무것도 안 하는 기본 버스 |

### 트리거 존 (Zone)

**파일:** `client/zone.hpp` / `client/zone.cpp` (클라 로컬 연출 존), `common/zoneDef.hpp` (서버·클라 공유 def)

| 항목 | 위치 | 설명 |
|------|------|------|
| `ZoneShape` / `ZoneVolumeDef` / `ZoneDef` | `common/zoneDef.hpp` | Box(OBB)+Sphere 조합, factionMask(uint32), tag |
| `ZoneEvent` enum | `client/zone.hpp` | Enter / Stay / Leave |
| `Zone::contains()` | `client/zone.cpp` | volume union point-in-OBB / point-in-sphere |
| `ZoneSystem` | `client/zone.hpp/cpp` | 태그 키 콜백; `update(playerPos)`로 로컬 플레이어만 판정(연출 존) |
| `chunks_index.bin` Zone 섹션 | `client/terrain.cpp parseChunkIndex` | 실제 파싱 → `ChunkIndex::zones` → `TerrainChunkManager::zones()` |
| `Online::Game::clientZoneSystem_` | `client/online/onlineGame.cpp` | `bindZoneHandlers()`(연출 태그), update 루프 틱, `onZoneState()`(S_ZoneState) |

> 서버 권위 게임플레이 존(보스 트리거/아레나 락)은 `RoomServer/zone.{hpp,cpp}` + `Room::bindZoneHandlers()`. 신규 패킷 `S_ZoneState`(`protocol.hpp`).

### 일반 마커 (Marker)

`common/markerDef.hpp`(`MarkerDef{type,name,pos,orient,scale}`) — Zone/Stronghold가 과한 경량 배치(보스 스폰/벽 등). `chunks_index.bin` Marker 섹션 → 서버·클라 `ChunkIndex::markers` → `TerrainChunkManager::markers()`. Unity는 `LevelMarker.cs`(type+name+transform). 소비는 게임플레이 코드가 type/name 필터.

---

## 5. 애니메이션

**파일:** `client/animation.hpp` / `client/animation.cpp`

| 항목 | 위치 | 설명 |
|------|------|------|
| `AnimFrame` struct | `animation.hpp #10-15` | translation, rotation(NQuat), scale, time |
| `WeightedAnimFrame` struct | `animation.hpp #17-20` | frame + 가중치 w |
| `convertAnimFrameToMatrix()` | `animation.hpp #25` | AnimFrame → Mat4x4 |
| `lerpAnimFrames()` | `animation.hpp #30` | lerp(translation/scale) + slerp(rotation). slerp=`XMQuaternionSlerp`가 최단호 부호 보정을 하므로 반구 문제 없음 |
| `sumWeightedAnimFrames()` | `animation.hpp #33` / `animation.cpp #36` | 가중합 (nlerp). **가중치 최댓값 프레임을 기준으로 각 항의 쿼터니언 부호를 정렬한 뒤 합산**한다 — 클립별 추출 부호가 제각각이라(플레이어 pelvis: `Combat_2H/Bow/Cast_Ready`가 `Run_*`와 반대 부호, dot≈-0.95) 정렬 없이 더하면 idle↔run이 비슷한 가중치일 때 상쇄되어 최대 180° 회전 튐이 발생했다 |
| `AnimClip` struct | `animation.hpp #42-62` | 키프레임, duration, skeletonEnum, flags. **재생 배속 필드는 없다** — 클립은 추출 바이너리에서 `shared_ptr<const>`로 공유되므로 기준 속력은 각 블렌더의 지역 `constexpr`로 둔다 |
| `loadAnimClipsFromFile()` | `animation.hpp #64` | 바이너리 → AnimClip 벡터 |
| `AnimBlender` class | `animation.hpp #92` | 추상 base; 상속 필수 |
| `AnimBlender::update()` | `animation.hpp #138` | priority_ 갱신 (오브젝트가 호출) |
| `AnimBlender::setCulled()/isCulled()` | `animation.hpp #123` | culled 플래그; viewFrustumCulled || hiZCulled_ 통합 값으로 동기화 — culled면 bone matrix 계산 및 Object::update 스킵 |
| `AnimBlender::onCalcLocal()` | `animation.hpp #143` | 로컬 변환 행렬 계산 (AnimSystem이 호출) |
| `AnimBlender::onCalcDress()` | `animation.hpp #146` | dress 공간으로 환원. 누적 직후 `onPostDress()` 훅 호출 |
| `AnimBlender::onPostDress()` (virtual 훅) | `animation.hpp #185` | 드레스 누적 직후 프로시저럴 보정 주입 지점(Keyframe 한정, 기본 no-op). AnimBlenderPlayer가 스파인 조준 pitch에 사용 — `docs/aimPitchUpperBodyMask.md` |
| `AnimBlender::onCalcFinal()` | `animation.hpp #156` | toLocal 적용 → finalXformData |
| `AnimBlender::finalXformData()` | `animation.hpp #160-161` | 셰이더 입력용 최종 행렬 배열 |
| `AnimBlender::advanceClipTime()` | `animation.hpp #198` / `animation.cpp #341` | 루프 클립 시간을 `rate`배로 진행 + fmod 랩. 파생 블렌더가 복붙하던 `t += dt; while (t > dur) t -= dur;`의 단일 구현. **루프 로코모션 전용** — 공격/피격/사망은 랩하면 안 되므로 각자 clamp 방식 유지 |
| `AnimBlender::solveLocomotionRate()` | `animation.hpp #215` / `animation.cpp #367` | 이동 속력 → 로코모션 재생 배속. `clamp(speedXZ / (refSpeed·max(locoWeight, 0.05)), 0.25, 2.0)` + 지수 평활(τ=0.1s, 원격 20Hz 패킷 지터 흡수). **가중치로 나누는 것이 핵심** — 블렌드 밴드가 이미 보폭을 깎아놨으므로 그냥 speed/refSpeed를 쓰면 중속이 더 미끄러진다. refSpeed는 **클립셋별·반비례 레버**(올리면 느려짐: Mushroom 4.5 / Treant 7.8 / Boss walk 4.8·run 9.6 / 나머지 3.0)이며 **서버 `animRefSpeed`+`animBandEnd`와 반드시 일치**. 단 클램프에 걸린 고속 구간에서는 발 속도가 `kMaxRate×refSpeed`라 refSpeed 방향이 뒤집힌다(전술 NPC). 상세: `docs/gameArchitecture.md` |
| `AnimBlender::bakedFrameOf()` | `animation.hpp #220` / `animation.cpp #391` | baked 샘플 인덱스(샘플 수로 클램프). 9개 블렌더에 복붙돼 있던 **클램프 없는** `bakedSampleRate * animTime`을 대체(텍스처 범위 초과 방지) |
| `AnimBlender::updatePriority()` | `animation.cpp #429` | 거리 LOD로 `mode_` 결정: refPos에서 약 29m(`kDistScale=50`×0.577) 이내 Keyframe, 밖은 Baked. 플레이어·근거리 몬스터는 항상 Keyframe 경로 |
| `AnimSystem` class | `animation.hpp #281` | 스케줄링 / 로드밸런싱 |
| `AnimSystem::update()` | `animation.cpp #466` | culled 파티셔닝 후 visible range만 timeSlice 기반 heap 처리. batch 경계에서 힙 끝을 `cntProcessed + i`로 줄여 중복 처리/하위 starvation 방지 |

**오브젝트별 AnimBlender (object.hpp):**

| 클래스 | 위치 |
|--------|------|
| `AnimBlenderPlayer` | `object.hpp #17` / `setWeaponType`=`object.cpp #98` — **무기 인지(weapon-aware)**. `setWeaponType(PlayerWeaponType)`(무기 장착 시 자유 함수 `equipPlayerWeapon`가 호출)가 무기별 idle/hit/4방향 run 클립명(`Combat_2H_Ready`/`Run_Bow_*` 등)과 `attackClips_` 순서 목록을 재구성. Death는 공용 `Death`. 공격은 Goblin식 오버레이(`currentAttackClip_`/`tAttack_`, EvAttack.attackIndex로 선택, 클립 길이만큼 재생). 콤보/반복은 스킬 타임라인의 다중 PlayAnimation이 구동. 트리거는 `EventBus::receive`. **상하체 분리 마스크+조준 pitch**(`docs/aimPitchUpperBodyMask.md`): `buildAttackMask()`=`object.cpp #26`(spine_01 서브트리 마스크+스파인 체인, init 시 1회), 공격 lerp에 `tAttack_*(mask+(1-mask)*tIdle_)` 적용, `onPostDress()`=`object.cpp #397`(스파인 피벗-공액 pitch, 사망 페이드). **run 배속**: `runRate_`+지역 `kRefSpeedRun=5`, 가중치 `tRun*(1-tAttack_*tIdle_)`(하체 마스크 몫 차감), 공격 오버레이 처리 **다음**에 계산 |
| `AnimBlenderGoblin` | `object.hpp #112` — 5-클립(Idle/Walk/Hit/Death + 다중 Attack) 속력 블렌딩. **다중 공격 클립**: `attackClips_`(로드된 공격 클립 풀네임 순서 목록, init이 후보 매칭으로 채움) + `currentAttackClip_`(EvAttack.attackIndex로 선택). 레거시 단일 `X_Attack` 폴백. **walk 배속**: `walkRate_`+지역 `kRefSpeedWalk=3`, 가중치 `tWalk_*(1-tAttack_)`(마스크 없음 → 공격이 전 본에 걸림) |
| `AnimBlenderSnake` / `AnimBlenderMushroom` | `object.hpp #162` / `#203` — 고블린과 동일 구조·다중 공격·walk 배속(클립 접두어만 다름). Snake는 idle 슬롯에도 `Snake_Walk`를 쓰므로 배속은 walk 슬롯에만 적용 |
| `AnimBlenderBomber/Birdy/Slime/Treant` | `object.hpp #244`/`#281`/`#318`/`#355` — Mushroom 패턴 복제(클립 접두어+attackClips_만 다름), 모두 활성(가드 제거됨). 7종 캐스터 공용 |
| `AnimBlenderBoss` | `object.hpp #396`/`object.cpp #1059` — 최종보스 14클립 풀세트. Player식 4방향 walk(`Boss_Walk_*`)+속력 run(`Boss_Run`) 블렌딩 + Goblin식 다중공격(`attackClips_`=Swings/Combo/BackAttack/Smite, EvAttack.attackIndex) + Hit1/Hit2(`hitClips_`, EvHit.hitAnimIndex) + Death. Rage는 등록만(BT 트리거 대기). `class Boss : public Goblin`(object.hpp, EventBus/ragdoll 재사용, setAnimBlender만 오버라이드). **배속은 walk/run 공유 단일값**(`locoRate_`) — walk↔run은 양쪽 다 보폭이 있어 클립별 가중치 나눗셈이 이중 보정이 된다. `kRefSpeedWalk=4.8`/`kRefSpeedRun=9.6`을 `tRunBand`로 블렌드해 기준 속도를 만들고 `tMove`로 한 번만 나눈다. **run 밴드는 4.0~7.0** — 두 gait(걷기 3.5 / 질주 8.75) 사이에 놓아야 각 gait가 자기 클립만 읽어 상수가 독립된다(`docs/gameArchitecture.md` "예외: 보스") |
| `AnimBlenderAnubis` 이하 | (인덱스 라인 밀림 — Grep으로 조회) |

---

## 6. 메시 / 모델 / 스켈레톤

**파일:** `client/mesh.hpp` / `client/mesh.cpp`

| 항목 | 위치 | 설명 |
|------|------|------|
| `Material` struct | `mesh.hpp #9-22` | PBR 텍스처 + 상수 재질 |
| `MaterialSet` struct | `mesh.hpp #24-27` | 재질 그룹 |
| `SubMesh` struct | `mesh.hpp #34-37` | IB view + 이름 (드로우콜 단위) |
| `Mesh` struct | `mesh.hpp #46-72` | VB/IB/재질 컨테이너 |
| `Mesh::vbIdxMap` | `mesh.hpp #54` | `"{메시명}_VB_{속성명}"` → VB 인덱스 |
| `Mesh::vbViewsByPipeline` | `mesh.hpp #67` | 파이프라인별 VBV 캐시 (mutable) |
| `Bone` struct | `mesh.hpp #100-114` | toLocal, toDress, name, socketType |
| `Skeleton` struct | `mesh.hpp #132-142` | bones, pRoot, skeletonEnumeration |
| `MeshWithDressXform` struct | `mesh.hpp #149-152` | mesh + dress 공간 변환 |
| `Model` struct | `mesh.hpp #157-163` | meshWithDressXforms + bvh + skeleton |
| `loadModelFromFile()` | `mesh.hpp #172-176` | 바이너리 → Model (Unity 추출 포맷) |

**파이프라인별 필수 VB 슬롯:**
- PBRPipeline: Position(0), Normal(1), Tangent(2), Bitangent(3), UV(4)
- PBRSkinnedPipeline: 위 5개 + BoneIndices(5), BoneWeights(6)
- PBRDeferredPipeline (GBuffer pass): Position(0), Normal(1), Tangent(2), Bitangent(3), UV(4)
- PBRDeferredSkinnedPipeline (Shadow pass): Position(0), BoneIndices(1), BoneWeights(2)
- PBRDeferredSkinnedPipeline (GBuffer pass): Position(0), Normal(1), Tangent(2), Bitangent(3), UV(4), BoneIndices(5), BoneWeights(6)
- TerrainPipeline (mainPass / gBufferPass): Position(0), Normal(1), Tangent(2), Bitangent(3), UV(4)

**스킨드 메시 판별 조건:** `mesh.vbIdxMap.contains(mesh.name + "_VB_BoneIndices") && animBlender`

---

## 7. 게임 오브젝트 (Object 계층)

**파일:** `client/object.hpp` / `client/object.cpp`

| 항목 | 위치 | 설명 |
|------|------|------|
| `RenderState` struct | `object.hpp` | world, pos, orient, scale, worldBVs, animBlender, pModel, viewFrustumCulled, willOcclude |
| `Equipment` struct | `object.hpp` | socketType + Object (장비 소켓) |
| `Object` class | `object.hpp` | 모든 게임 오브젝트의 base |
| `Object::equip()/disequip()/getEquipment()` | `object.cpp #1583,#1591,#1609` | `equipments_`에 부속 객체 추가/제거. 무기 장착에 사용(자유 함수 `equipPlayerWeapon`) |
| `equipPlayerWeapon(Object&, const AssetManager&, PlayerWeaponType)` (자유 함수) | `object.hpp #1012` / `object.cpp #73` | 무기 모델의 SocketOffset 키로 장착 손 결정 → 양손 disequip 후 equip → `AnimBlenderPlayer::setWeaponType`으로 클립 세트 동기화. online(`onlineGame.cpp` setupPlayer/createOtherPlayer/syncLobbyCharacterWeapons)과 standalone 에디터(`Editor::Controller::applyWeaponToPlayer`) 공용 |
| `Object::render()` 부속 객체 루프 | `object.cpp #1373` | `equipments_` 순회: socketOffset\*bone.toDress\*boneXform\*offsetXform\*world 체인으로 재귀 render() |
| `Object::renderPortrait()` | `object.cpp #1410` | 로비 포트레이트 전용. 스킨드 메시만 PBRSkinnedPipeline 채널로 제출 + 끝부분에서 `equipments_`를 `renderPortraitEquipment()`로 순회 |
| `Object::renderPortraitEquipment()` | `object.cpp #1448` | 부속 객체(장착 무기) 전용. non-skinned 메시를 `addLobbyPortraitDrawEventStatic`(PBRPipeline 포트레이트 채널)로 제출 |
| `Object::update()` | `object.cpp #1152` | 방향벡터 갱신 후 viewFrustumCulled\|\|hiZCulled_ 이면 조기 반환; 아니면 RenderState 보간 + animBlender::update |
| `Object::render()` | `object.cpp #1256` | viewFrustumCulled 체크 후 GFX DrawEvent 제출 (Hi-Z culled는 제출함, renderObjectId 포함). 스킨드 deferred는 `bakedReady`(mode==Baked && hasEverUpdated && finalBakedClipId>0) 가드로 stale clipId=0(생성 직후 stretch) 방지 → boneXforms/T-pose 폴백 (graphicsArchitecture.md 참조) |
| `Object::setFrustumCulled()/isFrustumCulled()` | `object.hpp` | view frustum culling 결과 — DrawEvent 제출 차단 |
| `Object::setHiZCulled()/isHiZCulled()` | `object.hpp` | Hi-Z occlusion culling 결과 (1-frame delay) — update/anim 스킵 |
| `Object::setRenderObjectId()/renderObjectId()` | `object.hpp` | GPU→CPU Hi-Z 역매핑용 정수 쿠키 |
| `Object::body()` | `object.hpp` | 인라인 RigidBody 참조 (PhysicsWorld 등록 시 사용) |
| `Object::worldBVH()` | `object.hpp` | `body_.worldBVH()` 위임 (CombatSystem 호환) |
| `Object::worldCullBounds()` | `object.cpp` | Hi-Z cull용 월드 AABB = worldBVH 본 부착 노드 합집합(+15% 마진), 포즈/랙돌 추종. 비스킨이면 nullopt |
| `Object::rebuildBodyBVH()` | `object.cpp` | BVH 월드 공간 재빌드 (setPos/setOrient 시 호출) |
| `Object::setPos/setOrient` | `object.hpp` | body_ 위임 + rebuildBodyBVH() |
| `Object::setAimPitch()/aimPitch()` | `object.hpp #541` | 조준 pitch(+아래, 라디안) — body orient(yaw 전용)와 분리된 상체 조준 채널. 로컬=카메라 pitch, 원격=S_MouseMove/S_SkillStart. AnimBlenderPlayer 스파인 굽힘·PlayVFX aim이 소비. NPC는 0 — `docs/aimPitchUpperBodyMask.md` |
| `Object::adoptAnimBlender()` | `object.cpp` (Object::setModel 직전) | 이미 init된 `unique_ptr<AnimBlender>` 채택(소유권 이전): 기존 블렌더 `animSystem.untrackAnimBlender` 후 교체. `setAnimBlender`(클래스 고정 타입)와 달리 런타임 임의 블렌더 교체용 — 에디터 캐스터 핫스왑(`setMonsterCaster`) |
| `Object::hp()` / `setHp()` | `object.hpp` | HP 접근자 |
| `Object::updateGroundedGravityGate()` | `object.cpp #1483` | 물리 step 직후 호출. terrain 접촉으로 접지 판정(normal.y≥0.7·비상승·2 step 지속) → `body_.setGravityScale(0/1)` + 작은 하강속도 ground-snap. 미세 충돌 피드백(중력↔접촉 솔버 튐) 제거 |
| `Object::isGrounded()` | `object.hpp #232` | 접지 판정 결과 (updateGroundedGravityGate가 갱신) |

**구체 오브젝트 클래스:**

| 클래스 | 위치 |
|--------|------|
| `Cube` | `object.hpp #591` |
| `Player` | `object.hpp #600` |
| `Object` ragdoll 가상 접근자 | `object.hpp #263` 인근 — `ragdoll()`(`Ragdoll*`, 베이스 nullptr)/`ragdollPendingActivation`/`ragdollInitVelocity`; `idMonsterMap_<Object*>` 통합 순회용 |
| `Goblin` | `object.hpp #454` : `Object` — ragdoll 필드·`EventBus`·ragdoll 가상 오버라이드를 클래스마다 복제(공용 `Monster` 베이스 없음) |
| `Snake` | `object.hpp #481` : `Object` — 고블린과 동일 패턴 복제 |
| `Mushroom` | `object.hpp #508` : `Object` — 고블린과 동일 패턴 복제 |
| `Bomber/Birdy/Slime/Treant` | `object.hpp`/`object.cpp` — 몬스터 스킬 캐스터(에디터). Mushroom 패턴 복제, 모두 활성(가드 제거됨). 리소스 클라·서버 `*.bin`+`*Server.bin` 전부 존재 |
| `Anubis` | `object.hpp #648` |
| `Bat` | `object.hpp #672` |
| `Bomber` | `object.hpp #696` |
| `Demon` | `object.hpp #720` |
| `Dragon` | `object.hpp #744` |
| `Eyeball` | `object.hpp #768` |
| `Fishman` | `object.hpp #792` |
| `Gargoyle` | `object.hpp #816` |
| `TerrainObject` | `object.hpp #845` |

**TerrainObject (`object.hpp #845`):**
- Object 상속. `const TerrainData*` 보유 (TerrainData/Model 분리 패턴과 동일)
- `setTerrainData(const TerrainData*)` — 지형 데이터 연결
- `render()` override — `TerrainPipeline::DrawEvent{ terrain, renderState_.world }` 제출
- 구현: `object.cpp` 말미

---

## 8. 렌더링 (GFX / Pipeline)

**파일:** `client/gfxUtil.hpp` / `client/gfxUtil.cpp` — 버퍼 유틸리티

| 항목 | 위치 | 설명 |
|------|------|------|
| `BufferCreationType` enum | `gfxUtil.hpp #9` | VertexBuffer / IndexBuffer / UploadBuffer / DefaultBufferUAV / ReadbackBuffer |
| `createBufferResource()` | `gfxUtil.hpp #21` | 버퍼 리소스 생성 유틸 |
| `ShaderInputBuffer` class | `gfxUtil.hpp #186` | Upload Heap 기반 CPU→GPU 버퍼 베이스 클래스 (room 단위 다중 CommandList 지원) |
| `ConstantBuffer` class | `gfxUtil.hpp #240` | ShaderInputBuffer 상속 — `SetGraphicsRootConstantBufferView` 바인딩 |
| `StructuredBuffer` class | `gfxUtil.hpp #257` | ShaderInputBuffer 상속 — `SetGraphicsRootShaderResourceView` 바인딩 |
| `RWStructuredBuffer` class | `gfxUtil.hpp #283` | Default Heap + UAV — `bindCompute` / `bindGraphics` / `bindComputeAsSRV` / `uavBarrier` / `clearUint` / `gpuAddress` / `resource` 제공. opt-in readback: `initReadback` / `copyToReadback` / `readbackPtr<T>(roomIdx)` / `hasReadback`. **offset 오버로드**: `bindCompute(...,byteOffset)` / `copyToReadback(...,dstByteOffset,srcByteOffset)` / `readbackPtr<T>(roomIdx,byteOffset)` — 단일 리소스 내 다중 슬롯(Hi-Z visibility 2-slot ring) 표현용 |
| `ConstantBufferArray` struct | `gfxUtil.hpp #356` | 큰 ConstantBuffer 여러 개를 단일 리소스에서 분할해 사용 |

**파일:** `client/renderSubmitter.hpp` / `client/renderSubmitter.cpp`

| 항목 | 위치 | 설명 |
|------|------|------|
| `RenderSubmitter` class | `renderSubmitter.hpp` | 전용 제출 스레드. 모든 ECL/Present/Signal을 순서 보장 FIFO로 받아 단일 스레드에서 `cmdQ_`에 제출 → 메인 스레드 임계 경로에서 ECL 비용 제거. `start`(inline 바인딩)/`goAsync`(스레드 가동, 첫 render에서)/`submit`/`present`/`signal`/`flushBlocking`/`stop`. 자세한 설계는 `graphicsArchitecture.md` 제출 스레드 절 |

**파일:** `client/gfx.hpp` / `client/gfx.cpp`

| 항목 | 위치 | 설명 |
|------|------|------|
| `GFX` class | `gfx.hpp #63` | DX12 렌더링 총괄 |
| `GFX::submitter_` | `gfx.hpp` | `RenderSubmitter` 소유(`cmdQ_` 뒤 선언 → 큐보다 먼저 파괴). 디스패처/헬퍼/로딩 경로가 `cmdQ_` 대신 이 핸들로 제출 |
| `GFX::setupDXGI()` | `gfx.hpp #77` | DXGI Factory + Adapter 열거 |
| `GFX::init()` | `gfx.hpp #84` | Device, CmdQ, DescriptorHeap, PSO 생성 |
| `GFX::createSwapChain()` | `gfx.hpp #90` | SwapChain + BackBuffer + FrameFence |
| `GFX::addDrawEvent()` | `gfx.hpp #97-135` | 파이프라인별 오버로드 |
| `GFX::initSharedResources()` | `gfx.hpp` | 공용 GPU 리소스(그림자맵/GBuffer/HiZ/정적 메시/white tex) 생성. 실행 시 메인 스레드 1회 |
| `GFX::loadRequestedAssets()` | `gfx.hpp` | 요청된 리소스(모델/텍스처/메시 등) 로드. ThreadPool 워커에서 백그라운드 호출 가능 |
| `GFX::loadAssets()` | `gfx.hpp` | initSharedResources + loadRequestedAssets 편의 래퍼 |
| `GFX::render()` | `gfx.hpp #155` | 전체 파이프라인 실행 |
| `GFX::drainGpu()` | `gfx.hpp/cpp` | 제출된 모든 GPU 작업(FrameFence 전체 + LoadFence) 블로킹 대기. `~GFX`가 호출하지만, **Game 소멸자 본문에서도 멤버 소멸 전에 반드시 호출** — gfx_보다 뒤에 선언된 멤버가 in-flight 리소스를 해제하면 디바이스 행(TDR)으로 같은 GPU의 타 프로세스까지 디바이스 제거됨 |
| `GFX::getHiZObjectVisible()` | `gfx.cpp` | renderObjectId → Hi-Z visibility 조회 (1-frame delay; Hi-Z OFF면 true 반환) |
| `GFX::setMaxRenderObjectId()` | `gfx.cpp` | objectVisibility 배열 크기 초기화 (setupStage 이후 호출) |
| `mu::perspReversedZ()` | `mathUtil.hpp` (client + common, 동일 내용) | Reversed-Z LH 퍼스펙티브 투영(near→depth 1.0, far→depth 0.0). `Camera::setPerspective()`(`camera.cpp`)가 사용 — 메인/로비/포트레이트 카메라 전부 적용. 그림자맵(ortho)은 미적용. 상세: `docs/graphicsArchitecture.md` "Reversed-Z 깊이 버퍼" |

**파이프라인 파일 목록:**

| 파이프라인 | 헤더 파일 | 용도 |
|-----------|----------|------|
| PBRPipeline | `pbrPipeline.hpp` | 정적 메시 PBR (Forward) |
| PBRSkinnedPipeline | `pbrSkinnedPipeline.hpp` | 스킨드 메시 PBR (Forward) |
| PBRDeferredPipeline | `pbrDeferredPipeline.hpp` / `pbrDeferredPipeline.cpp` | 정적 메시 Deferred Shading (Shadow + GBuffer + Lighting). **Hi-Z 추가(2026-06-15)**: `occludeeCandidate` DrawEvent는 `occluderPass()`(근거리 prop depth)→`hiZPass()`(cull/compact/command, skinned 5 compute 셰이더+`cmdSig_` 재사용, feedback ring 없음)→`gBufferIndirectPass()`(`PBRDeferredIndirectGBufferShader` + `gVisibleIndices` remap). 비-occludee는 기존 `gBufferPass()` direct. `OccluderInfo{mesh,subMesh,world}`/`Resources::HiZPass`/`OccluderPass` |
| PBRDeferredSkinnedPipeline | `pbrDeferredSkinnedPipeline.hpp` / `pbrDeferredSkinnedPipeline.cpp` | 스킨드 메시 Deferred Shading (Shadow + GBuffer만; Lighting은 PBRDeferredPipeline 담당). Hi-Z occlusion(2-slot feedback ring, CPU readback) |
| Masked foliage shadow | `shadowMapCSMMasked.hlsl` / `shader.cpp::createShadowMapCSMMaskedShader` | alpha-cutout 캐스터(나뭇잎/풀)용 그림자 변형. `PBRDeferredPipeline::shadowDraw/MT`가 `material->constantAlphaCutoff>0` 그룹만 masked PSO(Position+UV, `CULL_NONE`, `clip(albedo.a-cutoff)` PS)로 분기. b0=`ShadowMapCSMMaskedShader::PerDrawcallData`(`perDrawcallDataMasked`), VB=`"PBRDeferredPipeline_ShadowMasked"`. 공용 DefaultRootSig의 bindless 풀 사용. 상세: `graphicsArchitecture.md` |
| frustumCull | `frustumCull.hpp` | 재사용 VFC 헬퍼: `Frustum`/`extractFrustum(viewProj)`(Gribb-Hartmann)/`intersects(Frustum,AABB)`/`intersects(Frustum,OBB)`(OBB SAT). scatter prop VFC + `Light::shadowVisible`(그림자 컬링) 공용 |
| BVPipeline | `BVPipeline.hpp` | 바운딩 볼륨 디버그 |
| BillboardPipeline | `billboardPipeline.hpp` | 빌보드 |
| SkyboxPipeline | `skyboxPipeline.hpp` | 스카이박스. **Deferred 경로는 SceneColorHDR에 합성**(`skyboxRtv` renderPath 분기, 렌더순서 6a — heat/bloom이 하늘에도 적용), Forward(로비)만 backbuffer 직접. PSO는 타깃 포맷별 2종: `SkyboxShaderHDR`(R16G16B16A16F)/`SkyboxShader`(R8G8B8A8) — `createSkyboxShaderImpl` |
| UIPipeline | `uiPipeline.hpp` | UI 요소 |
| SamplePipeline | `samplePipeline.hpp` | 샘플 렌더 |
| TerrainPipeline | `terrainPipeline.hpp` / `terrainPipeline.cpp` | Height map 지형 렌더 (Forward path: shadowPass + mainPass) |
| TerrainDeferredPipeline | `terrainDeferredPipeline.hpp` / `terrainDeferredPipeline.cpp` | Height map 지형 렌더 (Deferred path: shadowPass + GBuffer pass) |

**Terrain 관련 파일:**

> **Chunk 스트리밍 전환:** 단일 terrain → 다중 Chunk 스트리밍. 설계 문서 `docs/terrainChunkStreaming.md`.

| 파일 | 설명 |
|------|------|
| `terrain.hpp` | `TerrainLayer`/`TerrainData`(+`chunkCol/Row`)/`TerrainLayerPalette`/`ChunkIndex(Entry)`/`ChunkCpuBuild` 구조체 + scatter(`ScatterPrototype`/`ScatterInstance`, `ChunkIndex::scatterPrototypes`/`ChunkIndexEntry::scatter`), chunk streaming 함수 선언 |
| `terrain.cpp` | `genChunkGeometryCpu`(CPU, 워커 스레드 안전)/`assembleChunkMeshGpu`(메인), `parseChunkIndex`(v2: ScatterPrototypes 전역 섹션 + Chunk 내 Scatter 블록; v3: per-instance `Rot` 쿼터니언, v2는 `Yaw` 레거시→Y쿼터니언 변환)/`loadLayerPalette`/`buildChunkCpu`/`finalizeChunkGpu`, `TerrainHeightField` 메서드 |
| `terrainChunkManager.hpp` / `.cpp` | `TerrainChunkManager` — 팔레트/인덱스 소유, hop≤3 BFS 스트리밍(load/unload+grace), 워커 CPU build + 메인 GPU finalize, `heightAtWorld`/`normalAtWorld`/`chunkCoordAtWorld`/`submitDrawEvents`/`worldCenter`. **Scatter**(나무/디테일/풀): `loadScatterAssets`(prop `.bin`→`propModels_`+빌보드 cross-quad/머티리얼+`resolvedProtos_`)/`resolveChunkScatter`(인스턴스 world·AABB 상주)/`submitScatterDrawEvents`(PBR 자동 인스턴싱 + **BVH 기준 컬링**: 비-BVH=거리컬 `kDetailCullRadius`(80m), BVH=메인카메라 `frustum_` VFC와 `Light::shadowVisible` **그림자 컬링을 독립 평가**→ `viewFrustumCulled`/`shadowCulled` 분리 설정(화면 밖 나무도 그림자 유지), 둘 다 안 보이면 skip; Hi-Z occludee/occluder는 메인 가시 시에만)/`setCullCamera(Frustum,eye)`, `buildCrossQuadMesh`(양면 빌보드). **`submitDrawEvents(GFX&, const Light&)`**: chunk 메시도 `shadowVisible(AABB, expand=3)`로 그림자 컬링(`TerrainObject::setShadowCulled`→DrawEvent `shadowCulled`, terrain shadowDraw 루프에서 skip; gbuffer는 무영향). 인스턴스 버퍼 용량은 `gfx.cpp` perInstanceData(PBRDeferred 32768/forward 16384, 정적 Hi-Z 65536) |
| `docs/scatterSystem.md` | Scatter 시스템 설계: 포맷 v3(per-instance Rot 쿼터니언·Align To Ground 베이크), 이름 매핑(ModelExtractor targetName), 자동 인스턴싱, 알파 컷아웃(+albedo 맵 누락 클리핑 함정), 빌보드, 충돌(ScatterCollider 구현) |
| `docs/scatterAuthoringGuide.md` | 지형에 Tree/Rock/Flower/Bush/Plant 띄우는 실전 작성 가이드(ModelExtractor→TerrainExtractor→DDS 변환→배치→실행, 트러블슈팅) |

**미니맵 (top-down, North-up; 월드 고정 베이크 + 매 프레임 UV 스크롤):**

> 캐시 텍스처는 **플레이어 중심의 고정 크기(`kMinimapCoverageWorld`=360m) 월드 영역**에 베이크되고, HUD가 매 프레임 플레이어 위치 기준 **UV sub-rect**로 스크롤한다(플레이어 중앙 고정, 지형이 반대로 흐름). **단일 RT**(per-room 아님; 직렬 큐라 해저드 없음, 깜빡임 근본 해소). 재굽기는 청크 로드/언로드 **또는 플레이어가 베이크 중심에서 50m(`kMinimapRebakeMoveThreshold`) 이상 이동** 시 1프레임으로 수행. 커버리지는 청크 크기(200m)와 무관하게 시야 기준이라 splat·prop이 또렷(1024px). 미로드 영역은 검정→fog-of-war 블러.

| 항목 | 위치 | 설명 |
|------|------|------|
| `TerrainChunkManager::minimapDirty()`/`clearMinimapDirty()`/`markMinimapDirty()` | `terrainChunkManager.hpp` | 청크 로드/언로드 시 세팅되는 dirty 플래그(폴링 트리거) |
| `TerrainChunkManager::submitMinimapDrawEvents()` | `terrainChunkManager.cpp` | 컬링 없이 Ready/Expiring 청크 전부를 `MinimapTerrainPipeline::DrawEvent`로 제출(베이크 center/coverage는 onlineGame이 player pos + `GFX::kMinimapCoverageWorld`로 계산) |
| `MinimapTerrainPipeline` | `minimapTerrainPipeline.hpp/.cpp` + `minimapTerrain.hlsl` | splat-blend diffuse 지형 패스. PS alpha=1 고정(로드 영역 마스크) |
| `MinimapPropPipeline` | `minimapPropPipeline.hpp/.cpp` + `minimapProp.hlsl` | scatter prop(나무/바위 등 **BVH prop만**) top-down albedo + alpha-cutout 베이크(지형 texA 위에 겹쳐 그림). 풀(grass/flowers, 비-BVH)은 미니맵 도배 방지로 제외. 텍스처 위에 그려 fog 마스크(alpha=1)에도 기여 |
| `TerrainChunkManager::submitMinimapPropDrawEvents()` | `terrainChunkManager.cpp` | Ready 청크의 collidable(BVH) scatter 인스턴스 part를 `MinimapPropPipeline::DrawEvent`로 제출 |
| `MinimapFogBlurPipeline` | `minimapFogBlurPipeline.hpp/.cpp` + `minimapFogBlur.hlsl` | 2-pass separable로 **alpha(커버리지 마스크)만** 블러하고 **RGB는 중심 탭으로 선명 통과**, `finalRGB = sharpRGB × blurredAlpha`로 합성. fog는 가장자리만 페이드(지형 색은 안 뭉갬). ※RGB까지 블러하던 버그로 미니맵 전체가 흐렸던 것 수정 |
| `SharedResources::Minimap` | `sharedResources.hpp/.cpp` | **단일** texA/texB ping-pong RT(R8G8B8A8, `created` 플래그). 초기 상태 PIXEL_SHADER_RESOURCE(첫 베이크 전 샘플 시 상태 불일치 방지) |
| `GFX::requestMinimapRebake/addMinimapDrawEvent/addMinimapPropDrawEvent/setMinimapCamera/minimapTextureForThisFrame` | `gfx.hpp/.cpp` | 요청 프레임에 단일 RT를 1프레임으로 베이크(지형→prop→fog블러) |
| `GFX::kMinimapRTSize/kMinimapCoverageWorld/kMinimapRebakeMoveThreshold/kMinimapWorldRadius/kMinimapFogBlurRadiusTexels` | `gfx.hpp` | 캐시 해상도(1024)/커버리지(360m)/이동 재굽기 임계(50m)/기본 시야 반경(60m, 줌 base)/페이드 폭(48텍셀) |
| `MinimapHUD` | `client/ui/minimapHUD.hpp/.cpp` | **우상단**(제거된 Hi-Z 디버그 프린트 자리) 위젯: 프레임+배경(UV 스크롤 sub-rect)+아이콘. 크기·위치 해상도 상대화(`uiScale=min(sw/1024,sh/768)`), 줌(`zoomIn/zoomOut`, Shift+휠), 아이콘 크기 축소(몬스터 3.5/파티 5/본인 6.5/보스 8 px×uiScale). `render(...,const MinimapGuide&)`=경로 **폴리라인**(맵 내부 세그먼트만 회전 쿼드, 아이콘 아래)+범위 밖 look-ahead **가장자리 화살표**(`uiShapes::arrow`, 아이콘 위) |
| `MinimapEntityIcon` | `client/ui/minimapHUD.hpp` | `{worldPos, Kind}` — Self(초록)/Party(파랑)/Monster(빨강)/Boss(주황) |
| `MinimapGuide` | `client/ui/minimapHUD.hpp` | `{active, span<const Vec3> polyline, Vec3 target}` — 경로 안내 오버레이(비소유 폴리라인 뷰 + 가장자리 화살표 조준점). `render` 기본값 `{}`=오버레이 없음 |
| `PathGuideHUD` | `client/ui/pathGuideHUD.hpp/.cpp` | 화면 목적지 지시자. look-ahead를 clip 투영→온스크린이면 발광 **비콘**(다이아 3겹+펄스), 오프스크린/카메라 뒤면 화면 가장자리 inset에 클램프한 **회전 화살표**(뒤면 clip.xy 반전). 하단에 **거리(`"<n>m"`, m 단위)** — 소유 `TextImage`에 Font로 래스터(basicPlayerHpUI 패턴, **정수 m 변경 시에만** 재래스터, uvScaleBias 서브렉트+시안 tint). `init(gfx)`로 텍스트 타깃 1회 생성 필수. 해상도 상대, `render(gfx,view,proj,targetWorld,distMeters,active,sw,sh)` |
| `uiShapes` (namespace) | `client/ui/uiShapes.hpp/.cpp` | 회전 solid-color UI 프리미티브 공용 헬퍼(UIPipeline 쿼드 재사용, bottom-origin 픽셀). `quad(cx,cy,w,h,angle,col)`=`scale·rotateZH·translate` 합성, `diamond`=45° 쿼드, `arrow`=shaft+V 바브(±135°)로 방향 화살표. PathGuideHUD·MinimapHUD 공유 |
| `Online::Game::minimap_`/`minimapIcons_`/`minimapBakedCenter_`/`minimapBakedCoverage_`/`bossNpcIds_` | `online/onlineGame.hpp/.cpp` | 아이콘 수집(`idPlayerMap_`/`idMonsterMap_`), 재굽기 트리거(청크 dirty 또는 50m 이동), Shift+휠 줌(`processInputGame`). 보스 판별은 스폰 시 채운 `bossNpcIds_` 집합(서버 ObjectType 권위, RTTI 무의존) |
| `unityScripts/TerrainExtractor.cs` | 지형+산포 추출(Export All Chunks). chunks_index v3 작성, `ScanPrototypes`(이름=매핑키), `GatherScatter`(트리=treeInstances·디테일=`ComputeDetailInstanceTransforms`), per-instance `Rot` 쿼터니언에 Align To Ground 틸트 베이크 |
| `unityScripts/ModelExtractor.cs` | 프롭/캐릭터 `.bin` 추출. LODGroup이면 **LOD0만**(`CollectLODRenderers`/`IsRendererUsable`), `FindAlbedoTexture`(셰이더 TexEnv 폴백으로 albedo 견고 추출), `cAlbedo` 흰색 기본값 |
| `terrain.hlsl` | Terrain VS/PS (Forward path: Splat map 블렌딩 + PBR BRDF + PCF Shadow) |
| `terrainDeferred.hlsl` | Terrain VS/GBufferOutput PS (Deferred path: GBuffer 기록, 조명 없음) |
| `terrainPipeline.hpp` | `TerrainPipeline` 네임스페이스 (DrawEvent, Resources, Dispatcher) |
| `terrainPipeline.cpp` | `Dispatcher::shadowPass()` / `mainPass()` 구현 |
| `terrainDeferredPipeline.hpp` | `TerrainDeferredPipeline` 네임스페이스 (DrawEvent, Resources, Dispatcher) |
| `terrainDeferredPipeline.cpp` | `Dispatcher::shadowPass()` / `gBufferPass()` 구현 |
| `pbrLighting.hlsli` | PBR BRDF 함수 라이브러리 — terrain에서 `#define TERRAIN_SHADER` 후 include 시 `illuminate()` 제외됨 |

**TerrainData 구조 (`terrain.hpp #18-31`):**
- `heightmapResolution` (N): 그리드 N×N 정점
- `layers`: `TerrainLayer` 배열 (diffuse + normalMap + tiling + metallic + roughness)
- `splatMap`: RGBA splat 텍스처
- `mesh`: VB 5슬롯 (Position/Normal/Tangent/Bitangent/UV), IB 32-bit

**TerrainPipeline 주요 구조체 (`terrainPipeline.hpp`):**

| 구조체 | 위치 | 설명 |
|--------|------|------|
| `LightData` | `#18` | 방향광 — 독자적 Type enum, isMainDirectionalLight, cascade view/proj/splits |
| `FrameData` | `#36` | globalAmbient + lightCount |
| `DrawEvent` | `#41` | terrain 포인터 + world 행렬 |
| `Resources::ShadowPass` | `#47` | perDrawcallData(b0), perFrameData(ConstantBufferArray, cascade별 슬롯) |
| `Resources::MainPass` | `#52` | perDrawcallData(b0), perFrameData(b1), **lightData(t1)** |

**TerrainShader cbuffer 레이아웃 (`shader.hpp #310`):**
- `PerDrawcallData`: wvp / world / **wv(world-view)** / idxSplatMap / idxDiffuse[4] / idxNormal[4] / tiling[4] / **metallicRoughness[4]** / layerCount
- `PerFrameData` (`#327`): PBRShader::PerFrameData와 동일 레이아웃 — globalAmbient / lightCnt / idxShadowMap / lightVP

**terrain.hlsl 셰이딩 흐름:**
1. splat 가중치 샘플링 → 4레이어 albedo(sRGB→linear) + tangent-space normal 블렌딩
2. buildTBN(vertNormalV) → 블렌딩 법선을 view-space로 변환
3. metallicRoughness[4]를 splat weight로 블렌딩 → lightCnt 루프 → pbrLighting.hlsli의 dirLight/pointLight/spotLight 호출
4. calcCSMShadow(posV, posRel, normalW, ndotl) → 5x5 PCF 그림자 적용
5. globalAmbient + IBL(`computeIBL`, `#define IBL_ENABLED`) ambient 가산 → Reinhard tonemapping → gamma (forward inline; 인게임 지형은 `terrainDeferred.hlsl`→공용 ACES resolve 경로)

**Deferred Shading 관련 파일:**

| 파일 | 설명 |
|------|------|
| `pbrDeferredPipeline.hpp` / `.cpp` | PBRDeferredPipeline 네임스페이스 — Shadow + GBuffer + Lighting 패스 |
| `pbrDeferredSkinnedPipeline.hpp` / `.cpp` | PBRDeferredSkinnedPipeline 네임스페이스 — Shadow + GBuffer 패스 |
| `pbrDeferred.hlsl` | GBuffer Geometry Pass VS/PS (정적 메시) |
| `pbrDeferredSkinned.hlsl` | GBuffer Geometry Pass VS/PS (스킨드 메시). **흡수 물결(M5)**: `PerInstanceData`에 ripple 배열(`ripplePosAge[4]`/`rippleColorIntensity[4]`/`rippleCount`, per-instance) 추가 — VS가 `instIdx`(nointerpolation) 전달, PS가 `gInstances[instIdx]`의 ripple을 가우시안 확장 링으로 GB2 emissive에 가산(`exp(-d*d)`, pow(neg) NaN 회피). 로컬 플레이어만 `rippleCount>0` |
| `pbrDeferredLighting.hlsl` | Deferred Lighting Pass (fullscreen triangle, GBuffer SRV 읽기) |
| `energyOrbPipeline.hpp` / `.cpp` + `energyOrb.hlsl` | **몬스터 사망 에너지 오브 렌더 파이프라인**. MeshParticle 복제 + GS quad. 죽은 서브메시 정점을 사망 포즈로 스키닝(boneData t2) → 해시 구체로 모핑(`morphT`) → 카메라향 quad point-sprite, **SceneColorHDR에 가산(bloom 이전)**. PS: 서브메시 albedo→HDR 색 `lerp(_, _, morphT)` × radial falloff. `DrawEvent{world,pMesh,pSubMesh,pAlbedo,boneXforms,sphereCenter,sphereRadius,colorHDR,morphT,pointSize,vertexCount}` |
| `energyOrbSystem.hpp` / `.cpp` | **에너지 오브 라이프사이클**(모드 비종속). `Orb` 상태머신 Forming→Tracking→Absorbing→Dead. `spawnFromMonster(model,finalXforms,objWorld,totalCharge,slot,corpseId)`=서브메시당 1오브(첫 정점 LBS 스키닝을 구체 중심으로), 플레이어 추적+가속, 근접 시 `onAbsorb`. **응축 스케일**(`renderScale`: 접근 시 월드크기 축소로 원근 팽창/bloom 블롭 억제). `hasActiveOrbs(corpseId)`/`update(dt,playerPos)`/`submitDrawEvents`. 노브: kForming 0.85s, kMaxSpeed 13, kSphereRadius 0.32, kPointSize 0.04, HDR 강도 2.0~3.8, kCondenseMinScale 0.5 |
| `pathGuideSystem.hpp` / `.cpp` | **경로 안내 연출**(클라 전용, 게임 스레드 단독). `build(markers)`=`PathPt` 마커(`name="<pathId>_<index>"`) 그룹화·정렬·등호장 리샘플. `update(dt,playerPos,terrain)`=가장 가까운 path 선택→플레이어 폴리라인 투영(`sPlayer_` 저장)→**가시 윈도우만 매 프레임 `heightAtWorld` Y conform**→위습 전진(ease)+bob. `submitDrawEvents(gfx,ribbonTex,orbProxy)`=리본을 ≤31정점+1오버랩 세그먼트로 `addHDRTrailDrawEvent`(흐름+지면정렬+`patternMode=1` 흐르는 쉐브론), 위습 1개 `addDrawEvent(EnergyOrb)`(free-orb morphT=1). **강조**: Config 기본 `ribbonWidth 1.4`/`ribbonColor{0.8,3,4.5}`/`flowSpeed 0.85`. **도착 은퇴**: 플레이어가 경로 끝 XZ `arriveRadius`(4m) 내 진입 시 `Path.completed=true`→해당 경로 영구 미안내(build 시 리셋). **전술전투 억제**: `setSuppressed(bool)`=true면 update가 전부 clear+inactive(아레나 중 off). **UI 안내 데이터 접근자**: `guidanceActive()`/`guidanceTargetWorld()`(=위습 look-ahead)/`goalWorld()`/`distanceToGoal()`(=`totalLen−sPlayer_`)/`activePathPoints(out)`(활성 경로 ~2m 서브샘플, 미니맵 폴리라인용). 노브=`Config`. 상세: `docs/pathGuidance.md` |
| `mesh.cpp` `buildOrbProxyMesh(device,cmdList,fence,pointCount=128)` | free-orb(위습)용 N-포인트 프록시 메시. EnergyOrbPipeline 4-VB 슬롯(Position/BoneIndices/BoneWeights/UV, 모두 더미)+인덱스[0..N-1]+1 SubMesh. `morphT=1`에서 정점 개수만 의미(시작 pos/본 무관). `onlineGame`이 `recordTerrainResourceLoad`로 1회 생성→`orbProxyMesh_` |
| `onlineGame.cpp` 경로 안내 훅 | `pathGuide_`(멤버)+`orbProxyMesh_`+`pathGuideHUD_`+`minimapGuidePoly_`. build: `setupStageVisual`의 zone 빌드 직후 `pathGuide_.build(chunkManager_.markers())`+프록시 메시 생성. update: `orbSystem_.update` 직후 `pathGuide_.update(dt,player_->pos(),chunkManager_)`. submit: `orbSystem_.submitDrawEvents` 직후 `pathGuide_.submitDrawEvents(gfx_, assetManager_.trail62Tex(), &orbProxyMesh_)`. **전술전투 억제**: update 직전 `pathGuide_.setSuppressed(localArenaPresentationZoneId_ >= 0)`(아레나 진입~완료 중 안내 전부 off). `pathGuideHUD_.init(gfx_)`는 setup(`tacticalDialogueOverlay_.init` 인근). **UI 방향 지시**(`renderInGame`, uiManager 렌더 직전): `activePathPoints`로 `minimapGuidePoly_` 채워 `MinimapGuide`로 `minimap_.render`에 전달(폴리라인+가장자리 화살표) + `pathGuideHUD_.render(camera view/proj, guidanceTargetWorld, distanceToGoal)`(온스크린 비콘/오프스크린 화살표, m 단위 거리) |
| `object.cpp/.hpp` `Object::addBodyRipple`/`BodyRipple`/`bodyRipples_` | 흡수 물결 앵커(M5). 오브 흡수 시 `onlineGame` onAbsorb가 호출 → 본체 위치 기준 오프셋으로 저장(매 프레임 live pos에 재앵커→몸 추적), `update`에서 노화(`kBodyRippleLife=1.0s`, HLSL `RIPPLE_LIFE`와 일치), deferred-skinned DrawEvent의 `ripplePosAge/rippleColorIntensity/rippleCount`로 주입 |
| `onlineGame.cpp` 시체/풀 (`migrateToCorpse`/`updateCorpses`/`reinitFromPool`/`returnMonsterToPool`) | 사망 연출 게임 레벨 라이프사이클(client-authored Corpse). Live→Corpse 이관(맵/컨테이너/`barrierObjects_` 제거, body `snapToCurrent`, `kDetachedCorpseId` 고정 id, `corpseId=renderObjectId`), 래그돌 2.5s→오브 전환, 흡수 완료 시 per-kind 풀(`goblinPool_`/`snakePool_`/`mushroomPool_`/`bomberPool_`/`birdyPool_`/`slimePool_`/`treantPool_`) 반환·재사용. **신규 몬스터(Bomber/Birdy/Slime/Treant)**: goblins_/snakes_/mushrooms_처럼 타입별 벡터(`bombers_`/`birdys_`/`slimes_`/`treants_`)+타입별 HP바 맵으로 처리 — `justDied`/render/cull/HiZ/HP바 모두 타입별 개별 루프, migrate/return/reinit switch도 타입별 case. 공용 셋업은 `configureNetMonster`(HP바 맵 인자), 각 createX가 타입 벡터에 push. Grandbaum→treants_(Treant kind), Isys→birdys_(Birdy kind). 오브 연출은 kind-무관 자동. **renderObjectId 객체당 1회 발급·평생 유지**(범람 방지, `setMaxRenderObjectId(10000)`). **중복 스폰 ghost 가드**: `create{Goblin,Hobgoblin,Snake,Mushroom,Bomber,Birdy,Slime,Treant}`이 `idMonsterMap_`에 이미 있으면 스킵(S_Enter/S_NpcSpawnBatch 중복 대응). 상세: `gameArchitecture.md` "에너지 오브 사망 연출" |
| `sharedResources.hpp` / `.cpp` | `SharedResources::GBuffer` 네임스페이스 — GBuffer 텍스처 생성/관리 |
| `sharedResources.hpp` / `.cpp` | `SharedResources::Portrait` 네임스페이스 — 로비 슬롯 캐릭터용 오프스크린 포트레이트 RT(가로 아틀라스, room별 triple-buffer). `addPortraitRT`/`transitionToWrite`/`transitionToRead`/`clearPortraitRT`. GFX 채널: `addLobbyPortraitDrawEvent`/`setLobbyPortraitCamera`/`addLobbyPortraitLightData`/`setLobbyPortraitActive`/`lobbyPortraitTextureForThisFrame`/`lobbyPortraitCellUvScaleBias`. 제출: `Object::renderPortrait(gfx, slot)`. render() 삽입: deferred lighting 이후 → UI 이전. 상세: `docs/lobbyScene.md` 작업 B-3 |

**GBuffer 레이아웃 (`sharedResources.hpp`):**

| 슬롯 | 포맷 | 내용 |
|------|------|------|
| GB0 | R8G8B8A8_UNORM | Albedo.rgb (linear) + AO.a |
| GB1 | R16G16_FLOAT | NormalV oct-encoded (view-space, 2채널 [0,1]) |
| GB2 | R11G11B10_FLOAT | **Emissive.rgb (HDR)** (정적·스킨드·지형 모두 emissive 전용; intensity>1 보존) |
| GB3 | R8G8_UNORM | Metallic.r + Roughness.g |
| GB4 | R32_FLOAT | Linear view-space Z (posV.z) — deferred 복원에서 NDC 깊이 양자화 회피용 |
| Depth | R32_TYPELESS (DSV=D32_FLOAT, SRV=R32_FLOAT) | Scene depth |

- GB1 클리어 값: `(0.5, 0.5, 0, 0)` → octDecode 시 정면 법선 (0, 0, 1)
- Normal Oct Encoding: `pbrLighting.hlsli`의 `octEncode()` / `octDecode()` 유틸리티 사용
- GB4(linear view-Z) × invProj 뷰 광선 → posV(NDC 깊이 양자화 회피), posV × invView → posW; CSM 그림자는 posW−camPos(**camera-relative**)로 샘플 (Lighting 패스 위치 재구성)

**HDR / IBL / Bloom 관련 파일 (상세 아키텍처: `docs/graphicsArchitecture.md` "HDR + IBL + Bloom 파이프라인"):**

| 파일 | 설명 |
|------|------|
| `sharedResources.{hpp,cpp}` | `SharedResources::SceneColor` — per-room R16G16B16A16_FLOAT HDR RT(+SRV). `SharedResources::IBL` — irradiance/prefiltered 큐브 + BRDF LUT(정적). `SharedResources::Bloom` — per-room HDR 밉체인(밉별 RTV+단일밉 SRV, 서브리소스별 상태 추적, `transitionMip`/`mip0Srv`). `SharedResources::ColorGrading` — color grading용 3D LUT(`.cube` 파싱→R8G8B8A8_UNORM Texture3D, 정적, 단일) |
| `iblIrradiance.hlsl` / `iblPrefilter.hlsl` / `iblBRDFLUT.hlsl` | IBL 프리컴퓨트 컴퓨트 셰이더(코사인 컨볼루션 / GGX importance / split-sum LUT). `envIsLDR` 토글 |
| `iblPrecomputePipeline.{hpp,cpp}` | `precomputeIBL()` — 로드 타임 1회(LoadFence), 스카이박스 큐브→IBL 맵 3종 생성 |
| `pbrLighting.hlsli` | `computeIBL`/`fresnelSchlickRoughness`(상단 정의, split-sum). `#define IBL_ENABLED` 셰이더만 컴파일 |
| `tonemapResolve.hlsl` / `TonemapPipeline.{hpp,cpp}` | fullscreen resolve: SceneColorHDR(+bloom) → exposure → ACES Filmic → gamma → **3D LUT color grading** → backbuffer. debugMode≠0 패스스루. **보스 heat distortion 굴절 워프**도 여기서(SceneColorHDR 샘플 UV 오프셋, GB4 깊이 게이팅). **배경(GB4==0=하늘) 픽셀은 패스스루**(톤맵 미적용+bloom)로 SceneColorHDR에 합성된 skybox 룩 보존 — graphicsArchitecture.md 렌더순서 6a/9 |
| `bloom.hlsl` / `BloomPipeline.{hpp,cpp}` | 픽셀 기반 bloom(VS 공유 + PSPrefilter/PSDownsample/PSUpsample). `Dispatcher::render()`가 전 패스를 단일 cmdlist에 기록 |
| `heatHaze.hlsl` + `heatField.hlsli` / `HeatDistortionPipeline.{hpp,cpp}` | 보스 위압 heat distortion. 가산 글로우 패스(bloom 이전 SceneColorHDR, GB4 깊이 게이팅)+공유 `evalHeatField`(절차 noise, 굴절 워프는 tonemap이 소비). `HeatDistortionShader`(shader.hpp). 데이터: `GFX::addHeatSource`/`setHeatGlobals`. graphicsArchitecture.md "보스 Heat Distortion" 참조 |
| `Online::Game::submitBossHeatSources()` (`online/onlineGame.cpp`) | 생존 보스→화면 `HeatSource` 투영(centerUV·해석적 radiusUV·view-Z), 보스별 `BossHeatState` 틴트, 스폰/사망 페이드. `StandAlone::Game` F9 디버그 소스(goblin_) |
| `bindless.hlsli` | `gTex2Ds`/`gTex2DArrays`/`gTexCubes`/`gTex3Ds` bindless 배열(`IDX_RANGE_*`). `sampleBindless3D`: LUT half-texel 보정(`uvw = v*(N-1)/N + 0.5/N`, N은 `BindlessIndex.idxInArray`에 저장) |

- IBL/HDR/Bloom 노브(`gfx.hpp`): `tonemapExposure_`(1.0), `bloomThreshold_`(1.0), `bloomIntensity_`(0.08), `iblIntensity`(lpfd/forward FrameData).
- Forward IBL 패리티: `pbr.hlsl`·`pbrSkinned.hlsl`·`terrain.hlsl` cbuffer에 camPos+IBL 필드, 포트레이트는 `FrameData::iblIntensity=0`.
- **디스크립터 풀:** bloom RTV(밉×room) 때문에 `rtvPool_`/`rtvHeap_`=64. per-room×N RT 추가 시 풀 용량 갱신 필수. Color grading LUT는 `srvTex3DPool_`(SRVHeap `[2100,2116)`, `gfx.cpp` 생성자) — bindless Texture3D 전용 4번째 텍스처 풀(`DefaultRootSig`의 `Texture3DPool` 파라미터, `t10 space4`).

**Deferred 렌더 패스 순서 (`gfx.cpp::render()`):**
1. GBuffer + SceneColorHDR 클리어 (`clearGBuffer` + SceneColor `transitionToWrite`/clear)
2. Shadow Pass — PBRDeferredPipeline + PBRDeferredSkinnedPipeline + TerrainPipeline (CSM)
3. GBuffer Pass (정적) — MRT 5개(GB0~GB4) + DSV에 geometry 기록
4. **GBuffer Indirect Pass (스킨드)** — Hi-Z 5단계 compute(Clear→Cull→PrefixSum→Compact→Command) 후 indirect draw. Compact Pass 이후 visibleFlags → `visibilityReadback` 복사(1-frame delay). 동일 MRT + DSV.
5. GBuffer Pass (지형) — TerrainDeferredPipeline, 동일 MRT + DSV
6. GBuffer 상태 전환: RTV→SRV (`transitionToRead`)
7. Deferred Lighting Pass — fullscreen `DrawInstanced(3, 1, 0, 0)`, **SceneColorHDR(R16G16B16A16_FLOAT)에 선형 HDR 출력** (`direct + computeIBL + emissive`, fog 적용, 톤매핑 X)
8. **GBuffer depth → backbuffer DSV 복사** (`copyResource`): Lighting pass와 같은 cmdList batch에서 실행.
9. SceneColorHDR RTV→SRV (`SceneColor::transitionToRead`)
10. **Bloom** (`gBufferDebugMode_==0`일 때): SceneColorHDR → bloom 밉체인(prefilter→downsample→additive upsample) → mip0 SRV
11. **Tonemap resolve**: SceneColorHDR(+ bloom mip0 가산) → exposure → ACES Filmic → gamma → **3D LUT color grading**(고정 단일 LUT, `SharedResources::ColorGrading::lutData`) → **backbuffer(LDR)**
12. Forward-always 오버레이(backbuffer, resolve 이후): Skybox, BV debug, Billboard, 파티클류 (GBuffer/SceneColorHDR 미사용)

**GFX RenderPath 선택 (`gfx.hpp`):**
- `enum class RenderPath { Forward, Deferred }`
- `GFX::setRenderPath(RenderPath)` — 런타임 전환
- `GFX::cycleGBufferDebugMode()` — 'G' 키로 디버그 뷰 순환 (0 None→Albedo→Normal→AO→Roughness→Metallic→LightAccum(=emissive)→Depth→**8 IBL diffuse→9 IBL specular→10 BRDF LUT**)
- `gBufferDebugMode_` (uint, 0~10) — Lighting PSO의 `debugMode` cbuffer 필드로 전달. resolve는 `debugMode≠0` 시 패스스루(톤매핑 생략)

**gfx.cpp 라이트 스테이징 (`gfx.cpp`):**
- PBR Dispatcher 생성 직전에 `lightDataPBRPipeline_` → `PBRShader::Light` 변환 후 `resourcesTerrainPipeline_.mainPass.lightData` 스테이징
- `frameDataTerrainPipeline_.lightCount` 동시 갱신

**TerrainPipeline::DrawEvent (`terrainPipeline.hpp #32-35`):**
- `terrain`: `const TerrainData*`
- `world`: `mu::Mat4x4` — 월드 변환 행렬 (기본값 identity)
- mainPass()에서 `ev.world`로 WVP/WV 계산

**GFX 내부 파이프라인별 DrawEvent 벡터 위치 (`gfx.hpp`):**

| 파이프라인 | DrawEvent 벡터 위치 |
|-----------|-------------------|
| SamplePipeline | `gfx.hpp #212` |
| PBRPipeline | `gfx.hpp #216` |
| PBRSkinnedPipeline | `gfx.hpp #223` |
| SkyboxPipeline | `gfx.hpp #230` |
| BVPipeline | `gfx.hpp #234` |
| BillboardPipeline | `gfx.hpp #238` |
| UIPipeline | `gfx.hpp #243` |
| TerrainPipeline | `gfx.hpp #247` |
| PBRDeferredPipeline | `gfx.hpp` — `drawEventsPBRDeferredPipeline_` |
| PBRDeferredSkinnedPipeline | `gfx.hpp` — `drawEventsPBRDeferredSkinnedPipeline_` |

**MeshParticlePipeline:**

| 파일 | 설명 |
|------|------|
| `meshParticlePipeline.hpp` | DrawEvent (world, pMesh, pSubMesh, pTex, tint, renderOrder), Dispatcher |
| `meshParticlePipeline.cpp` | updateGPUDataSingleThreaded / drawSingleThreaded 구현 |
| `meshParticle.hlsl` | PerInstanceData(world+tint), bindless texture PS |

**SwordSlashPipeline:**

| 파일 | 설명 |
|------|------|
| `swordSlashPipeline.hpp` | DrawEvent (world, tint, t, 텍스처 4종, FX 파라미터), Dispatcher |
| `swordSlashPipeline.cpp` | updateGPUDataSingleThreaded / drawSingleThreaded 구현 |
| `swordSlash.hlsl` | Flow+Dissolve+Emission 효과 VS/PS. b0=PerDrawcallData(bindless+FX), b1=PerFrameData(VP) |

**TwoSidesPipeline:**

| 파일 | 설명 |
|------|------|
| `twoSidesPipeline.hpp` | DrawEvent (world, tint, textures, texSpeed, frontFacesColor/backFacesColor/fresnelColor 등), Dispatcher |
| `twoSidesPipeline.cpp` | updateGPUDataSingleThreaded / drawSingleThreaded 구현. CullMode=None. Slot0=Position, Slot1=Normal, Slot2=UV, Slot3=Color |
| `twoSides.hlsl` | Unity HS_Blend_TwoSides 완전 포팅. NORMAL 입력, worldNormal+worldPos 전달, N.V Fresnel, frontFacesColor/backFacesColor/backFresnelColor 적용 |

**TrailPipeline:**

Unity ParticleSystem Trails 모듈 (Mode=Particles). RendererModule과 독립된 overlay 레이어로 동작 — 파티클 본체 렌더링과 공존 가능.

| 파일 | 설명 |
|------|------|
| `trailPipeline.hpp` | DrawEvent (`std::vector<TrailVertexCPU>` + per-trail constants), Resources (system-wide perInstanceData pool + per-drawcall PDD), Dispatcher (alpha/additive 2 PSO) |
| `trailPipeline.cpp` | updateGPUDataSingleThreaded: 모든 trail vertex를 한 StructuredBuffer에 패킹 + trailStartOffsets 기록. drawSingleThreaded: VB/IB 없이 `DrawInstanced((N-1)*6, 1, 0, 0)` |
| `trail.hlsl` | VS expansion via `SV_VertexID` — kStripOffsets/kSides 룩업 테이블로 segment 당 6 vertex로 quad strip 생성. 중앙 차분 tangent × cameraDir 외적으로 side 벡터 산출. UV: `Stretch`(1-segmentT) / `Tile`(cumulativeDist/tileLength). PS: bindless sample × baseColor × (1-age/lifetime). **하위호환 확장**: `PerDrawcallData.flowSpeed`(Tile V 시간 스크롤 `−currentSystemTime*flowSpeed`) + `alignMode`(0=카메라-페이싱[기존], 1=지면정렬 `cross(tangent, worldUp)`) + `patternMode`(0=텍스처 그대로[기존], 1=절차적 흐르는 쉐브론 마스크 `frac(v*density+slope*|u−0.5|)` V자, `density=0.7`/`slope=0.58`로 간격 넓힘, 팁이 head/goal 방향, 경로 안내 리본 전용). 모두 기본 0 → 기존 파티클 트레일 불변 (구 `pad0`→`XMFLOAT2` 재사용) |

**TrailPipeline (HDR / 프리블룸 채널):** 경로 안내 리본 발광용. 기존 트레일은 톤매핑 resolve 이후 LDR 백버퍼에 그려져 블룸이 안 먹으므로, 별도 채널을 **에너지 오브 직후(SceneColorHDR, 블룸 전)** 에 draw. `gfx.hpp/cpp`: `drawEventsTrailPipelineHDR_` + `resourcesTrailPipelineHDR_`(별도 리소스셋 — StructuredBuffer 클로버 방지, per-backbuffer) + `addHDRTrailDrawEvent`. PSO: `createTrailShaderHDR`(`shader.cpp`, RTV=`R16G16B16A16_FLOAT`, additive). 카메라/프레임 데이터는 기존 `cameraDataTrailPipeline_` 재사용.

**WindRingPipeline:**

Unity UberParticles `_EDGEFADE` 기능 포팅. 링 메시 파티클에 Fresnel 엣지 소프트닝 적용. CullMode=None, ZWrite=Off, Alpha Blend.

| 파일 | 설명 |
|------|------|
| `windRingPipeline.hpp` | CameraData (view, proj, pos), DrawEvent (world, pMesh, pSubMesh, pTex, tint, edgeFadePower, edgeFadeStrength, renderOrder), Dispatcher |
| `windRingPipeline.cpp` | VB binding key `"WindRingPipeline"`, 3 views: Position(0)+Normal(1)+UV(2). PerInstanceData(80B): world(transposed 64B)+tint(16B). PerDrawcallData(48B): idxTex+instanceOffset+edgeFadePower+edgeFadeStrength+cameraPosW |
| `windRing.hlsl` | POSITION+NORMAL+UV 입력. PS: `NdotV = abs(dot(normalW, viewDir))`, `alpha *= lerp(1, pow(NdotV, power), strength)` — 링 실루엣 투명화 |

---

## 8-B. 파티클 시스템

**설계 원칙:** Unity Particle System 모듈 구조 — 공통 simulation core, `RendererModule.mode`로 렌더 백엔드 선택

| 파일 | 설명 |
|------|------|
| `particleModules.hpp` | `MainModule`, `EmissionModule`, `ShapeModule`, `VelocityOverLifetimeModule`, `ColorOverLifetimeModule`, `SizeOverLifetimeModule`, `RotationOverLifetimeModule`, `CustomDataModule`, `Material`, `RendererModule`, `TextureSheetAnimationModule`, `SubEmittersModule`, `TrailModule`, `ParticleSystemConfig` |
| `particleSystem.hpp` | `ParticleSystem`, `Particle` (`trail` ring buffer 포함, kMaxTrailSegments=32), `TrailPoint`, `SubEmitterEvent`. 결정론 모드 API: `setGameplayConfig()`, `setDeterministicSeed()`, `deterministic()` |
| `particleSystem.cpp` | `init()`, `emit()`, `emitAt()`, `startContinuous()`, `spawnParticle()`, `sampleShapeOrigin/Direction()`, `update()`, `render()`, det 모드: `emitScheduledBurstsDet()`, `emitRateDet()` |
| `particleEffect.hpp` | `ParticleEffect` — Unity 프리팹 대응 그룹 컨테이너. `PlayMode::Emit` / `Continuous`. `SubEmitterBinding`, `PendingSubEmitterBurst` |
| `particleEffect.cpp` | `addSystem()`, `play()`, `stop()`, `isAlive()`, `update()`, `render()`, `bindSubEmitter()`, `setChildSpawnCallback()`, `setDeterministicSeed()` |
| `../common/particleGameplay.hpp` | `pg::` 결정론 코어 (클라/서버 공유): `DetRng`(SplitMix64 카운터 PRNG), `GameplayConfig`, `sampleSpawn()`, `evaluateParticles()`(서버 해석적 평가), `importGameplayConfig()`. 설계: `docs/particleHitboxDeterminism.md` |
| `../common/simpleJson.{hpp,cpp}` | (client/에서 이동) flat-vector DOM JSON 파서 — 클라 임포터 + 서버 게임플레이 설정 로드 공용 |

**ParticleSystem API (`particleSystem.hpp`):**

| 메서드 | 설명 |
|--------|------|
| `init(config, maxParticles=4096)` | 모듈 config 설정 + pool 크기 결정 |
| `emit(int count)` | init() 후 수동 방출 |
| `emitAt(count, worldPos, inheritVel, inheritColor, inheritSize)` | 지정 위치에서 spawn — ParticleEffect sub-emitter 디스패치용 |
| `pendingSubEmitterEvents()` | update() 이후 수집된 Birth/Death 이벤트 목록 반환 |
| `config()` | 설정 참조 반환 — emit 전 shape.position, main.startRotation3D 등 변경에 사용 |
| `startContinuous()` | init() 기반 연속 방출 시작 |
| `startContinuous(ParticleSystemConfig)` | config 설정 + 연속 방출 편의 오버로드 |
| `stopContinuous()` | 연속 방출 정지 |
| `killParticle(int index)` | `particles()` 인덱스의 파티클 즉시 소멸(표준 사망 경로: Death 서브이미터 + swap-remove). 비관통 히트박스의 외부 통제 hook. 내부 `killParticleAt(i)` 공유 |

**Sub Emitters (`particleEffect.hpp` / `particleModules.hpp`):**
- `SubEmittersModule` — `Event::Birth` / `Event::Death`, `emitProbability`, `inheritVelocity/Color/Size`
- `SubEmitterBinding` — parentIdx + subEmitterCfgIdx + childIdx 연결 레코드
- `PendingSubEmitterBurst` — 활성 burst-sequence 인스턴스; 시간 시뮬레이션으로 Unity burst 타이밍 재현
- `ParticleEffect::bindSubEmitter(parentIdx, cfgIdx, childIdx)` — 자식 시스템 등록; 이후 play()에서 자식 자동 재생 차단, update()에서 ParentEvent → PendingBurst 변환
- `ParticleEffect::setChildSpawnCallback(cb(childIdx,pos))` — 서브이미터 자식 스폰 시 월드 위치로 콜백(부모 이벤트당 1회, burst 수와 무관). VFX↔사운드 디커플링 훅(예: 화살 발사/폭발 SFX를 실제 launch/impact 시점에 재생)

**MainModule 주요 필드:**
- `duration` — 한 사이클 길이(초); 0 = 시간 제한 없음
- `looping` — duration 후 재시작 여부
- `startDelay` — 첫 방출 전 대기 시간
- `simulationSpeed` — 전역 재생 속도 배율
- `startRotation3D` — mesh 파티클 초기 3D 방향(`mu::Mat4x4`); emit 전 `config().main.startRotation3D`로 설정

**ShapeModule::Type 지원:**
- `Point` — 단일 점, `direction` 방향으로 emit
- `Edge` — 선분 위 랜덤 위치, `direction` 방향으로 emit
- `Cone` — apex 또는 base disc에서 원뿔 내 랜덤 방향
- `Sphere` — 구면에서 outward 방향
- `Box` — 박스 내 랜덤 위치, 중심 outward 방향

**RendererModule::Mode:**
- `Billboard` — `material.mainTex`가 있으면 항상 `BillboardPipeline::DrawEvent` 제출
  - `TextureSheetAnimationModule.enabled = true` → 그리드 기반 UV 프레임 계산
  - `TextureSheetAnimationModule.enabled = false` → 전체 텍스처 (uvOffset=0, uvScale=1)
- `Mesh` + `MatUnlit` — `MeshParticlePipeline::DrawEvent` 제출 (angularAngle + startRotation3D + translate). Billboard와 동일하게 `TextureSheetAnimationModule` 프레임 UV(`uvOffset/uvScale`)를 적용 — `PerInstanceData.uvScaleOffset`로 per-particle 전달, `meshParticle.hlsl`에서 `uv = uv*scale+offset`. 기본 (1,1,0,0)=전체 텍스처. bloodEffect(3x3 시트)가 이 경로 사용
- `Mesh` + `MatSwordSlash` — `SwordSlashPipeline::DrawEvent` 제출 (동일 transform 계산, 텍스처 4종 + FX 파라미터 포함)
- `Mesh` + `MatWindRing` — `WindRingPipeline::DrawEvent` 제출 (NORMAL 입력 + Fresnel edge fade)
- `Mesh` + `MatPiercing` — `PiercingMeshPipeline::DrawEvent` 제출 (Custom1.xy→uv2.z/.w dissolve, 3중 노이즈 + distortion)
- `Mesh` + `MatPiercingSlash` — `PiercingSlashMeshPipeline::DrawEvent` 제출 (Vefects Slash: 회전 Cutout + AdditiveLerp emission)

**AnyMat / Material 타입** (`particleModules.hpp`):
- `using AnyMat = std::variant<MatUnlit, MatSwordSlash, MatSmokeBlendCG, MatTwoSides, MatWindRing, MatPiercing, MatPiercingSlash>` — per-shader 독립 구조체 + variant
- `MatUnlit` — BillboardPipeline / MeshParticlePipeline용: `mainTex`, `additive`
- `MatSwordSlash` — SwordSlashPipeline용: `mainTex`, `emissionTex`, `dissolveTex`, `flowTex`, 스크롤/Flow/디졸브/Emission FX 파라미터
- `MatSmokeBlendCG` — SmokeBlendCGPipeline용: `mainTex`, 스프라이트 시트 애니메이션
- `MatTwoSides` — TwoSidesPipeline용: `mainTex`, `maskTex`, `noiseTex`, `emission`, `backFresnel`, UV 타일링 3종
- `MatWindRing` — WindRingPipeline용: `mainTex`, `edgeFadePower(2f)`, `edgeFadeStrength(1f)`, `color` — Fresnel 엣지 소프트닝 링 메시용
- `MatPiercing` — PiercingMeshPipeline용 (Vefects Piercing): 텍스처 8종(colorNoise/piercing/piercingNoise/distortionNoise/distortionMask/emissiveNoise/emissiveMask/opacityMask), color1/2/emissive, scale·speed 4쌍, ST 2종, colorBoost/piercingNoiseIntensity/distortionIntensity/emissiveIntensity/opacityBoost. 머테리얼 로더: `piercingMaterial.cpp::loadPiercingMaterialMetadata` (`shaderProperties` 파싱)
- `MatPiercingSlash` — PiercingSlashMeshPipeline용 (Vefects Slash): 텍스처 8종(slash/slashNoise/emissiveSlash/emissiveDissolve/distortionNoise/colorNoise/mask/cutout), color1/2/emissive, scale·speed 4쌍, maskST, cutoutOffset, slashScale·slashSpeed·emissiveSlashScale·emissiveSlashSpeed·slashNoiseIntensity·distortionIntensity·colorBoost·emissiveIntensity·opacityBoost·additiveLerp·cutoutErosion·cutoutErosionSmoothness·cutoutRotation. 머테리얼 로더: `piercingSlashMaterial.cpp::loadPiercingSlashMaterialMetadata`
- `RendererModule::mat` (`AnyMat`) — `render()` 내 `std::visit`으로 파이프라인 디스패치

**SwordSlashPipeline** (`swordSlashPipeline.hpp` / `swordSlashPipeline.cpp` / `swordSlash.hlsl`):
- Flow Map UV 왜곡 + UV 스크롤 + EmissionTex 발광 + Dissolve 마스크(파티클 수명 기반)
- DrawEvent: world, tint, t(normalized age), 텍스처 4종, FX 파라미터 전체
- PerDrawcallData(b0): bindless 텍스처 인덱스 4종 + instanceOffset + FX 파라미터 + time
- PerFrameData(b1): VP 행렬 (SystemTime은 particleSystem::render()에서 FrameData로 전달)

**TextureSheetAnimationModule** (`particleModules.hpp`):
- `enabled` — 활성화 시 Billboard UV를 그리드 기반 프레임으로 교체 (비활성 시 전체 텍스처)
- `tilesX / tilesY` — 스프라이트 시트 분할 수
- `animation` — `WholeSheet`(전체 시트 순회) / `SingleRow`(한 행만, RowMode 미구현)
- `timeMode` — `Lifetime`만 구현 (Speed/FPS 미구현)
- `cycles` — 수명 동안 반복 횟수 (기본 1)
- `startFrame` — 시작 프레임 오프셋 (기본 0)

---

## 9. 게임 루프

**파일:** `client/standalone/game.hpp` / `client/standalone/game.cpp`

| 항목 | 위치 | 설명 |
|------|------|------|
| `StandAlone::Game` class | `standalone/game.hpp #28` | IGame 구현 |
| `Game::setupStage()` | `standalone/game.hpp #37` | 씬 오브젝트 생성 + CombatSystem 등록 + renderObjectId 할당 + setMaxRenderObjectId |
| `Game::spawnTestObject(int kind)` | `standalone/game.cpp` | kind 1~8 switch: 각 factory로 PhysicsTestObject 생성 후 activate |
| `Game::update()` | `standalone/game.hpp #45` | 메인 루프 (입력→이벤트→물리→오브젝트→애니메이션) |
| `Game::render()` | `standalone/game.hpp #46` | cullObjects → GFX → feedbackCullResultToAnim |
| `Game::cullObjects()` | `standalone/game.cpp #1350` | view frustum culling (plane-based) → setFrustumCulled |
| `Game::feedbackCullResultToAnim()` | `standalone/game.cpp` | Hi-Z readback → setHiZCulled + AnimBlender::setCulled, hasEverUpdated()==false면 culled 강제 해제 (gfx_.render() 이후 호출) |
| `Game::processInput()` | `standalone/game.hpp #57` | 키보드/마우스 입력 처리 |
| `importNode()` 계열 | `standalone/game.hpp #68-80` | 씬 바이너리 파일 파싱 |
| `importTerrain()` | `standalone/game.hpp #80` | Terrain 노드 처리 — `TerrainObject`에 TerrainData 연결 |

**Online::Game 전용 (`online/onlineGame.hpp` / `online/onlineGame.cpp`):**

| 항목 | 위치 | 설명 |
|------|------|------|
| `Game::resolvePlayerSeparation()` | `onlineGame.cpp` (`removePlayer` 직후) | 플레이어 간 reciprocal soft separation. 매 물리 step 후 호출. 로컬 플레이어를 XZ 침투량의 절반만큼 `setCurrPos`로 밀어냄. Faction `Players` 게이팅, `getId` 결정론적 tie-break, 적용 시 `moveChange_=true`. 상수: `kPlayerSeparationRadius`/`kMaxSeparationSpeed`/`kSeparationStiffness`, 충돌 레이어 `kLayerPlayer`/`kPlayerCollisionMask` (파일 상단) |
| `Game::resolveBarrierSeparation()` | `onlineGame.cpp` (`resolvePlayerSeparation` 직후) | 전술 차단벽 분리. 매 물리 step 후 호출. 살아있는 `barrierObjects_`(hp>0)를 인접끼리 **선분(캡슐)으로 이어 "연속 벽"**으로 처리(`closestPointOnSegmentXZ`) → NPC 간격 편차와 무관하게 틈 봉합. 캡슐 안 플레이어를 **전체** 침투량만큼 `setCurrPos`로 밀어냄(절반 아님; barrier는 서버 권위 부동 객체). 누적은 step 상한 클램프. 임펄스 없음 → 튕김 없음. 죽은 벽은 수집 제외 → 연결 끊겨 구멍. 상수: `kBarrierRadius`/`kBarrierLinkDist`/`kMaxBarrierPushPerStep` (파일 상단) |
| `Game::setNpcBarrier(active, ids)` | `onlineGame.cpp` (public) | `S_NpcBarrier` 수신 핸들러(`PacketManager`)가 호출. 대상 NPC의 `Object::setBarrierActive` 토글 + `barrierObjects_` 추가/제거. barrier 모드는 `Object` 베이스 플래그라 몬스터 종류 무관 |
| `S_NpcBarrier` 패킷 | `protocol.hpp` `SNpcBarrierPacket{active, npcId 목록}`; 서버 `MidBossTactics`(차단선 토글)→`PacketManager::makeSNpcBarrierPacket`; 클라 `PacketManager::handleSNpcBarrierPacket`→`Game::setNpcBarrier` | 전술 중 차단선 NPC를 '플레이어를 막는 벽'으로 on/off |
| `Game::hideNpcs(ids)` | `onlineGame.cpp` (public) | `S_NpcHide` 수신 핸들러가 호출. id로 NPC 조회 후 `Object::setHidden(true)`(+활성 래그돌 해제). id 기반이라 전용 NPC 타입 분리 시 이 조회만 통합하면 됨 |
| `Object::hidden_`/`setHidden`/`hidden` | `object.hpp` (공통 베이스) | 사망(`isDead_`)과 별개. `Object::update`/`render`(`object.cpp`) 조기반환 + 고블린 HP바 루프(`onlineGame.cpp`)에서 제외 → 시체 없이 완전 비표시. `Game::onNpcRespawn`이 복귀 시 해제 |
| `S_NpcHide` 패킷 | `protocol.hpp` `SNpcHidePacket{npcId 목록}`; 서버 `Room::despawnTacticalNpcHidden`+`MidBossTactics::despawnOriginalSnakeSquad`→`PacketManager::makeSNpcHidePacket`; 클라 `PacketManager::handleSNpcHidePacket`→`Game::hideNpcs` | NPC를 시체 없이 즉시 숨김(그랜드밤 후퇴 원본 뱀 퇴장). 복귀는 `S_NpcRespawn` |
| `Game::lobbyLogin/Register/CreateRoom/JoinRoom/LeaveRoom/StartGame()` | `onlineGame.cpp` (UI 버튼 콜백) | 인증 서버 도입 전 로그인은 로컬 빈 값 검사 후 `isAuthenticated_`를 갱신. 회원가입은 아이디/비밀번호/닉네임을 검사하고, 성공한 아이디·닉네임을 프로세스 메모리 집합에 기록해 동일 실행 중 중복을 차단. Create/Join은 인증 상태를 이중 검사한 뒤 LobbyServer 요청 패킷 전송. 룸 상태 변경은 응답 핸들러에서 수행하며 Leave 후 인증 상태는 유지 |
| `Game::onLobbyCreated/onLobbyJoined/onLobbyPlayerJoined/onLobbyPlayerLeft/onGameStart()` | `onlineGame.cpp` (lobby 액션 직후, public) | LobbyServer 응답 처리. `PacketManager`가 `LobbyScene`의 `SleepEx(1,true)` alertable 대기에서 호출. 룸 상태/슬롯/호스트 갱신 후 `refreshLobbyUI()`. `onGameStart`는 현재 로그만(RoomServer 핸드오프 후속) |
| `Game::createStronghold/onStrongholdState/applyHit` | `onlineGame.cpp` | 거점은 `Stronghold`(Object+EventBus, AnimBlender 없음; `object.hpp`) 클래스. enter의 hp/maxHp로 생성. `applyHit`은 EvHit/EvDeath 발행만(거점/고블린 공통), 디스패치 루프 `resolveObject`가 `strongholdHpBars_`로 거점 해소 → 데미지 넘버 생성. `onStrongholdState`: 파괴(state=1)는 setHp 없이 EvDeath, 재건(state=0)은 setHp(full)+EvRespawn. 파괴상태=`isDead()`. 상세: `RoomServer/docs/strongholdSystem.md` §10 |
| `Game::lobbyDisplayName(uint16)` | `onlineGame.cpp` | sessionId → 표시 이름(본인 `myLobbyId_`=`"나"`, 그 외 `"Player_<id>"`) |
| `Game::LobbyScene()` 진입부 | `onlineGame.cpp` | `SleepEx(1,true)` + `ClientApp::send()`로 로비 네트워크 펌핑(InGameScene와 동일 패턴) |

**Game 멤버 변수 (game.hpp #81-135):**

| 멤버 | 타입 | 역할 |
|------|------|------|
| `assetManager_` | `AssetManager` | 모델/애니메이션/텍스처 캐시 |
| `animSystem_` | `AnimSystem` | 애니메이션 스케줄러 |
| `physicsWorld_` | `PhysicsWorld` | 물리 시뮬레이션 (integrate + contact + PGS solver) |
| `combatSystem_` | `CombatSystem` | 공격 판정 / 몬스터 AI |
| `debugBVView_` | `DebugBVView` | BV 디버그 렌더링 |
| `physicUpdateInterval` | `Seconds` | `1s/60f` 기준 타임스텝 (effectiveInterval = interval * scaleK) |
| `physicUpdateScaleK_` | `int` | 적응형 물리 주기 배율 (1~4, 렉 시 자동 증가) |
| `consecutiveLagFrames_` | `int` | 연속 렉 프레임 카운터 (scale-up 판단용) |
| `consecutiveNonLagFrames_` | `int` | 연속 정상 프레임 카운터 (scale-down 판단용) |
| `eventList_` | `EventList` | 프레임별 이벤트 큐 |
| 몬스터 shared_ptr들 | `#97-107` | goblin_, anubis_, bat_ 등 |
| `terrain_` | `std::shared_ptr<TerrainObject>` | `game.hpp #110` — 지형 게임 엔티티 |

**AssetManager 주요 멤버 (`AssetManager.hpp`):**

| 항목 | 위치 | 설명 |
|------|------|------|
| `terrain()` accessor | `AssetManager.hpp #24` | `const TerrainData*` 반환 |
| `terrain_` 멤버 | `AssetManager.hpp #64` | `TerrainData` 인스턴스 |

**Light 클래스 주요 항목 (`light.hpp`):**

| 항목 | 위치 | 설명 |
|------|------|------|
| `Light::updateCSMCascades()` | `light.hpp #30` | CascadeConfig + ShadowMapConfig → Practical Split Scheme으로 cascade 계산. **카메라-상대 공간**에서 frustum corner 직접 생성(역행렬 없음: camView 열=basis·camProj=fov) + center texel snap — shadow shimmering 해결 (radius 양자화는 시도 후 제거: 이동 중 떨림 유발) |
| `Light::cascadeCameraPos()` | `light.hpp #55` | camera-relative cascade 빌드에 쓰인 카메라 eye. caster/receiver가 `posW-camPos` rebase에 사용(`shadowVisible`도 이 값으로 bounds rebase) |
| `Light::shadowVisible(AABB/OBB/variant, expand=1)` | `light.hpp #57` / `light.cpp` | **그림자(light-frustum) 컬링 단일 진입점.** `updateCSMCascades`에서 캐시한 `cascadeFrusta_`(ortho=OBB, camera-relative)에 대해 `cascadeCameraPos_` rebase + `expand`로 half-extent 확장 후 `intersects` 테스트, 어느 cascade에라도 보이면 true(cascade 0개면 항상 true=미컬). 엔티티(`cullObjectsForShadow`)·지형 chunk(expand=3)·scatter prop(expand=1) 모두 이 함수 사용. ortho z-pad는 `minZ-2·radius` 유지(radius 축소는 foliage가 near 뒤로 사라져 되돌림) |
| `Light::render()` | `light.hpp #35` | PBR, PBRSkinned, Terrain 세 파이프라인에 LightData 자기등록 |
| `Light::dir()` | `light.hpp #52` | 조명 방향 (NVec3) |

**ParticleEffect 멤버 (game.hpp):**

| 멤버 | 설명 |
|------|------|
| `flameParticleSystem_` | 불꽃 빌보드 파티클 (FlameTex) |
| `smokeParticleSystem_` | 연기 빌보드 파티클 (SmokeTex) |
| `swordSlash1Effect_` | 검기1 효과 — SwordSlash 메시 + Smoke 서브시스템 |
| `swordSlash7Effect_` | 검기7 효과 — Sword Slash 7 + Slashes 서브시스템 |
| `swordSlashComboEffect_` | 콤보 검기 효과 |
| `slashWaveEffect_` | 슬래시 웨이브 — HalfTrail 메시 + TwoSidesPipeline (MatTwoSides) |
| `piercingEffect_` | Piercing 슬래시 — `SM_VFX_Projectile_02` 메시 + PiercingMeshPipeline (MatPiercing), `PS_VFX_Piercing_ParticleSystems.json` / `M_VFX_Piercing_Fire.json` |
| `piercingSlashEffect_` | PiercingSlash — `SM_VFX_Slash_01_HD` 메시 + PiercingSlashMeshPipeline (MatPiercingSlash), `PS_VFX_Slash_ParticleSystems.json` / `M_VFX_Slash_Fire.json` |
| `dustParticleSystem_` | 발 착지 흙먼지 빌보드 파티클 |
| `aoESlashGreenEffect_` | AoE 슬래시 그린 이펙트 (Circle2 + Slash, Billboard) |
| `energyExplosionArrowEffect_` | 에너지 발사체 복합 이펙트. 4 시스템(game.cpp `bindSubEmitter` 기준): [0] Charge(16), [1] Arrow StretchedBillboard(32, Charge.Death 서브이미터), [2] Hit(16, Arrow.Death), [3] HitWhiteBG(16, Arrow.Death). Lua `addVFX` 테이블([0]Charge [1]Arrow [2]Hit)과 인덱스 일치. 화살 비관통 트리거=`VFXParticleAttach(13,1)`, 폭발 판정=`(13,2)`. **SFX 훅**: `setChildSpawnCallback`(game/onlineGame)로 child1(Arrow) 스폰=`charge_shoot`, child2(Hit) 스폰=`charge_explosion` 재생 → 명중/최대사거리 양쪽 동기. 차징음=`arrow_charge` lua PlaySound@120 |
| `tornadoEffect_` | 토네이도 연속 이펙트. 4 시스템 (`Par_TornadoContinous_ParticleSystems.json`): [0] 메인 링 (`Par_TornadoContinous`, MatWindRing), [1] 하단 링 (`/Bottom`, MatWindRing), [2] 링 라이즈 (`/RingRise`, MatWindRing), [3] 버스트 점 (`/Par_BurstParticles`, MatUnlit Billboard) |

**Camera::updateGFX() 등록 파이프라인 (`camera.cpp`):**
- PBRPipeline, PBRSkinnedPipeline, SkyboxPipeline, BVPipeline, BillboardPipeline, **TerrainPipeline**, MeshParticlePipeline, SmokeBlendCGPipeline, BlendCGMeshPipeline, **PiercingMeshPipeline**, **PiercingSlashMeshPipeline**, SwordSlashPipeline, **TwoSidesPipeline**, **TrailPipeline**, **WindRingPipeline** CameraData 자기등록

**PiercingMeshPipeline** (`piercingMeshPipeline.hpp` / `.cpp` / `piercing.hlsl`):
- Vefects `SH_VFX_Vefects_Piercing_BIRP_New` 포팅. 메시 모드 전용, CULL_NONE(양면), SrcAlpha/InvSrcAlpha, depth read-only(LEqual).
- VB: Position/UV/Color (BlendCGMeshPipeline와 동일 입력 레이아웃). 셰이더 구조체 `PiercingMeshShader` (`PerInstanceData`=BlendCGMesh 재사용, `PerFrameData`=SmokeBlendCG 재사용, `PerDrawcallData` 신규).
- Custom1.x→uv2.z(alpha reveal), Custom1.y→uv2.w(emissive reveal). PerInstanceData가 custom1을 PS까지 전달.
- 텍스처: colorNoise/emissiveNoise=T_VFX_Noises_01, piercingNoise/distortionNoise=T_VFX_Noises_02, piercing/emissiveMask=T_VFX_Piercing_Fire, distortionMask=T_VFX_Piercing_Generic_Gradient_Mask_01. `_Texture1`(opacity mask)는 머테리얼에서 null(=white) 폴백.

**PiercingSlashMeshPipeline** (`piercingSlashMeshPipeline.hpp` / `.cpp` / `piercingSlash.hlsl`):
- Vefects `SH_VFX_Vefects_Slash_BIRP_New` 포팅. 메시 모드 전용, CULL_NONE(양면), SrcAlpha/InvSrcAlpha, depth read-only(LEqual).
- Piercing과 구분되는 핵심 차이: (1) Slash/EmissiveSlash 텍스처가 각자 `scale·speed` 1D 흐름(uv * (scale, 1) + time * (speed, 0)), (2) 회전 가능 Cutout 텍스처(`_CutoutRotation`/`_CutoutOffset`/`_CutoutErosion`/`_CutoutErosionSmoothness` smoothstep erosion), (3) AdditiveLerp로 pre-multiplied emission과 blend, (4) distortion mask 없음.
- VB: Position/UV/Color. 셰이더 구조체 `PiercingSlashMeshShader` (`PerInstanceData`=BlendCGMesh 재사용, `PerFrameData`=SmokeBlendCG 재사용, `PerDrawcallData` 신규: idxSlash/idxSlashNoise/idxEmissiveSlash/idxEmissiveDissolve/idxDistortionNoise/idxColorNoise/idxMask/idxCutout + 스칼라 13 + scaleSpeed 4쌍 + maskST + cutoutOffset).
- Custom1.x→uv2.z(alpha reveal), Custom1.y→uv2.w(emissive reveal). Piercing과 동일 매핑.
- 텍스처: slash/emissiveSlash=T_VFX_Slash_Fire, slashNoise/emissiveDissolve/distortionNoise=T_VFX_Noises_02, colorNoise=T_VFX_Noises_01, mask=T_VFX_Slash_Mask_01, cutout=T_VFX_Linear_Gradient_Mirror_Vertical_01.

**Camera Spring Arm 시스템 (`camera.hpp` / `camera.cpp`):**

| 항목 | 위치 | 설명 |
|------|------|------|
| `Camera::update(float dt)` | `camera.cpp #5` | yaw-only + filtered pitch 타겟 회전, at_ 저역통과, Spring Arm 충돌 회피(queryCameraArm → fast-in/slow-out arm 길이 제어) |
| `Camera::setView(eye, at)` | `camera.cpp` | 타겟 추종과 무관하게 view 직접 설정(로비 대기실 정적 카메라) |
| `Camera::setPhysicsWorld()` | `camera.hpp #31` | PhysicsWorld 연결 (queryCameraArm 호출 경로) |
| `Camera::atSmoothingRate_` | `camera.hpp` | 시선 목표점 at_ 저역통과 rate. 평지 보행/물리 고주파 흔들림 완화 |
| `Camera::targetPitchSmoothingRate_` | `camera.hpp` | 타겟 orient에서 추출한 pitch 저역통과 rate. roll은 카메라 추종 회전에 사용하지 않음 |
| `Camera::currentArmLength_` | `camera.hpp` | 현재 arm 길이 (fast-in 즉시 단축 / slow-out dt 기반 복귀) |
| `Camera::armReturnRate_` | `camera.hpp` | slow-out 복귀 속도 (units/sec, 기본 3.f) |
| `Camera::cameraRadius_` | `camera.hpp` | BVH raycast spherePad (기본 0.15f) |

**AssetManager::loadGFXAssets (`AssetManager.hpp #12`):**
- `loadGFXAssets(...)` — 의존성 기준 2단계 로드를 순차 호출하는 래퍼. 스탠드얼론 모드에서 사용.
- `loadLobbyVisualAssets(...)` (`AssetManager.hpp #18`) — **Phase 1**: 대기실 3D에 필요한 최소(큐브·플레이어 모델, 스카이박스, 플레이어 애니 전체) 큐잉 후 `gfx.loadRequestedAssets()` + 플레이어 baked-anim id.
- `loadRemainingInGameAssets(...)` (`AssetManager.hpp #19`) — **Phase 2**: 고블린 모델·이펙트 텍스처/메시·파티클 머티리얼·고블린 애니. (Online 모드: `startInGameAssetLoad`가 Phase 1→직렬화 대기→Phase 2 순으로 ThreadPool 백그라운드 호출)
- 진행도: `GFX::assetLoadFraction()`(요청 처리 비율) + `Online::Game::particleFilesDone_/Total_`(파티클 프리페치). 통합은 `Online::Game::loadProgress01()`.

**GFX::AssetConfigs (`gfx.hpp #61`):**

| 구조체 | 필드 | 설명 |
|--------|------|------|
| `ShadowMapConfig` | `cascadeResolutions[MAX_CSM_CASCADES]` | cascade별 독립 해상도 (기본값 {2048,1024,1024,512}) |
| `ShadowMapConfig` | `cascadeCount`, `format`, `key` | CSM 설정 |
| `CascadeConfig` | `nearZ`, `farZ`, `lambda` | Practical Split Scheme 파라미터 |

**SharedResources::ShadowMap (`sharedResources.hpp`):**

| 항목 | 위치 | 설명 |
|------|------|------|
| `kDefaultKey` | `sharedResources.hpp #41` | `"ShadowMap"` — 문자열 리터럴 대신 이 상수 사용 |
| `validateRequiredKeys()` | `sharedResources.hpp #95` | Dispatcher 생성자에서 필수 키 등록 여부 검증 |
| `getCSMAllReadyAsDepthWrite()` | `sharedResources.hpp #89` | 모든 cascade를 DepthWrite로 전환 (CL 할당 + 제출 포함) |
| `getCSMAllReadyAsShaderResource()` | `sharedResources.hpp #91` | 모든 cascade를 ShaderResource로 전환 (CL 할당 + 제출 포함) |
| `clearCSMAllShadowMaps()` | `sharedResources.hpp #93` | 모든 cascade DSV 일괄 클리어 — gfx.cpp에서 한 번만. 각 파이프라인 shadowDraw 내부 호출 금지 |
| `csmShadowMapData` | `sharedResources.hpp #102` | key → per-room CSMShadowMapData 벡터 (cascade별 독립 Texture2D) |

**업데이트 순서 (game.cpp Game::update):**
1. `processInput()` → 입력/LButton → `combatSystem_.onPlayerAttack()`
2. `combatSystem_.update()` → EvAttack, EvHit 생성
3. 이벤트 처리 루프 (Hit/Death/Blood/Attack → 각 오브젝트 eventBus)
4. `physicsWorld_.step()` (고정 타임스텝 누산기 패턴)
5. 각 `Object::update()` (물리 보간, RenderState 갱신; viewFrustumCulled||hiZCulled_ 이면 스킵)
6. `animSystem_.update()` (timeSlice 기반 스케줄링)
7. `Game::render()`:
   a. `cullObjects()` — frustum culling → setFrustumCulled
   b. Object::render() 호출들 — frustum culled만 제외, Hi-Z culled는 DrawEvent 제출
   c. `gfx_.render()` — Hi-Z readback 복사 포함
   d. `feedbackCullResultToAnim()` — 이전 프레임 readback → setHiZCulled + AnimBlender::setCulled (최초 1회는 hasEverUpdated() 보정으로 강제 갱신)

---

## 10. 디버그 시각화

**파일:** `client/debugBVView.hpp` (헤더 온리)

| 항목 | 위치 | 설명 |
|------|------|------|
| `DebugBVView` class | `debugBVView.hpp #16` | 배치 BV 렌더러 |
| `push(AABB, ttl)` | `debugBVView.hpp #19` | 스냅샷 AABB 등록 |
| `push(OBB, ttl)` | `debugBVView.hpp #26` | 스냅샷 OBB 등록 |
| `pushLive(obj, halfExtent, offsetFwd, ttl)` | `debugBVView.hpp #36` | 이동 추적 공격 hitbox |
| `pushBVHNodes(obj, ttl)` | `debugBVView.hpp #44` | BVH 노드 전체 스냅샷 |
| `update(dt)` | `debugBVView.hpp #52` | TTL 감소 + 만료(hp≤0 포함) 제거 |
| `render(gfx)` | `debugBVView.hpp #68` | BVPipeline::DrawEvent 제출 |
| `StaticEntry` (private) | `debugBVView.hpp #87-91` | 사전 계산된 worldXform + ttl |
| `LiveEntry` (private) | `debugBVView.hpp #93-99` | Object* + halfExtent + offsetFwd + ttl |

---

## 11. UI 시스템

**파일:** `client/ui/` 디렉터리

설계 문서: `client/docs/UI.md`

### 공유 타입 (`ui/UITypes.hpp`)

| 항목 | 설명 |
|------|------|
| `UI::Anchor` | 부모 사각형 기준점 (0~1 정규화) |
| `UI::Pivot` | 자기 자신 기준점 (0~1 정규화) |
| `UI::DimValue` | 픽셀 또는 퍼센트 크기값 (`px()` / `pct()` 팩토리) |
| `UI::Rect` | 레이아웃 후 절대 픽셀 사각형 |
| `UI::Color` | RGBA 색상 (float, 0~1) |
| `UI::MouseButton` | Left / Right / Middle |
| `UI::Anchors::*` | 앵커 프리셋 (TopLeft, Center, BottomCenter 등) |
| `UI::Pivots::*` | 피벗 프리셋 |

### 베이스 클래스 (`ui/UIElement.hpp` / `ui/UIElement.cpp`)

| 항목 | 설명 |
|------|------|
| `UI::UpdateContext` | update 트리 순회 시 전달되는 컨텍스트 (deltaTime, GFX*, FontHandle*, 화면 크기) |
| `UI::RenderContext` | render 트리 순회 시 전달되는 컨텍스트 (GFX*, screenHeight) |
| `UI::UIElement` | 모든 위젯의 base 클래스 |
| `UIElement::addChild()` | `unique_ptr` 자식 추가; 부모 소유권 |
| `UIElement::layout(parentRect)` | anchor/pivot/offset → `resolvedRect_` 계산 후 자식 재귀 |
| `UIElement::updateTree(ctx)` | `onUpdate` 호출 후 자식 재귀 (invisible 스킵) |
| `UIElement::renderTree(rc)` | zOrder 정렬 후 `onRender` + 자식 렌더 (invisible 스킵) |
| `UIElement::buildWorldMatrix(screenHeight)` | `resolvedRect_` → UIPipeline용 scale+translate 행렬; Y축 뒤집기 포함 |

### 매니저 (`ui/UIManager.hpp` / `ui/UIManager.cpp`)

| 항목 | 설명 |
|------|------|
| `UI::UIManager` | 엘리먼트 트리 소유, 레이아웃/업데이트/렌더/입력 총괄 |
| `UIManager::setScreenSize(w, h)` | 화면 크기 갱신 (WM_SIZE 수신 시 호출) |
| `UIManager::layout()` | 전체 트리 레이아웃 재계산 |
| `UIManager::update(dt, gfx, font)` | `UpdateContext` 생성 후 `root_.updateTree()` |
| `UIManager::render(gfx)` | `UIPipeline::FrameData` 제출 후 `root_.renderTree()` |
| `UIManager::onWndMsg(msg, wp, lp)` | WM_MOUSEMOVE/LBUTTON*/RBUTTON*/KEY* 처리; true면 게임 입력 차단 |
| `UIManager::needsCursor()` | interactive + visible 엘리먼트 존재 시 true (커서 캡처 해제 신호) |
| `UIManager::hitTest(x, y)` | zOrder 역순 히트테스트; interactive 엘리먼트만 대상 |

### 위젯 (`ui/widgets/`)

| 클래스 | 파일 | 설명 |
|--------|------|------|
| `UI::Panel` | `Panel.hpp/cpp` | 컨테이너; `backgroundTex` 있으면 배경 렌더 |
| `UI::Image` | `Image.hpp/cpp` | 단일 텍스처 표시. `uvScaleBias`(아틀라스 셀 sub-rect 샘플)/`colorMul`(틴트·알파) 필드 지원 |
| `UI::emitNineSlice()` | `UIElement.hpp/cpp` | 9-slice 헬퍼; 요소 사각형을 9셀로 나눠 셀별 부분 UV `DrawEvent` emit. 코너는 화면 px 고정, 가장자리/중앙 늘어남. `DrawEvent::uvScaleBias`(+`ui.hlsl`/`UIShader::PerInstanceData`)로 부분 UV 매핑 |
| `UI::Label` | `Label.hpp/cpp` | `TextImage` 내부 소유; `resolvedRect_` 크기에 맞게 자동 재생성; dirty-check로 매 프레임 래스터화 방지 |
| `UI::Button` | `Button.hpp/cpp` | Normal/Hovered/Pressed 상태 텍스처 + 9-slice(`slice*`) + 상태별 `texTint*`; `onClick` 콜백 (`std::function<void()>`) |
| `UI::ProgressBar` | `ProgressBar.hpp/cpp` | 배경 + fill 이중 쿼드; `setProgress(0~1)` |
| `UI::Slider` | `Slider.hpp/cpp` | 트랙 + 핸들 드래그; `onValueChanged` 콜백 (`std::function<void(float)>`) |
| `UI::Dropdown` | `Dropdown.hpp/cpp` | 파란 헤더 버튼 + 확장 리스트; `setup(items)` 후 `onSelectionChanged` 콜백 (`std::function<void(int)>`) |
| `UI::TextInput` | `TextInput.hpp/cpp` | 한 줄 텍스트 입력; 포커스 시 `WM_CHAR` 수신, 내부 child `Label`로 표시 + `|` 캐럿; `uppercase`/`alnumOnly`/`maxLength`/`placeholder`, `onChange`/`onSubmit` 콜백. (`UIManager`가 `WM_CHAR`→`onChar`, 클릭 시 `onFocus`/`onBlur` 라우팅) |

### 로비 / 설정창 UI (onlineGame에서 분리)

설계 문서: `client/docs/lobbyUISeparation.md`

| 항목 | 파일 | 설명 |
|------|------|------|
| `UI::Build::addSolid/addLabel/addButton/applyRect` | `ui/uiBuild.hpp` | 위젯 빌더 inline 자유함수(공용). `addChild + applyRect + 캡션 라벨` 보일러플레이트 래핑. LobbyUI/SettingsPanel 공유 |
| `Online::LobbyUI` | `online/lobbyUI.hpp/cpp` | 로비 2D UI 레이어: 메인메뉴 + 스쿼드 스테이지(대기실) + 로딩 오버레이 + 로비 텍스처 소유. 메인메뉴는 `ViewState::isAuthenticated`에 따라 같은 패널의 `authRoot_`와 `roomSelectionRoot_` 중 하나만 표시하며 설정/종료는 공용으로 유지. 두 화면은 동일한 세로 콘텐츠 범위를 사용하고 입력창·프로필·주요 액션·하단 버튼을 크게 배치해 중앙 패널의 불필요한 여백을 줄임. 인증 후 화면 상단에는 `lobbyPanelTex_`를 재사용한 정사각형 프로필 플레이스홀더와 `ViewState::nickname` 라벨을 표시하며, 서버 연동 전 기본값은 `PLAYER`. 회원가입 모달은 아이디/비밀번호/닉네임 입력과 필드·중복 오류 메시지를 제공하고, 로컬 가입 완료 시 로그인 아이디 자동 입력·비밀번호 삭제·모달 닫기를 수행. 위젯은 `uiManager_` 트리 소유(비소유 포인터). `loadTextures(gfx)` / `build(uiManager, Callbacks)` / `refresh(ViewState)` / `updateLoading(dt, visible, progress01)`. 버튼 액션은 `Callbacks`(login/register/create/join/leave/start/copy/openSettings/quit)로 Game에 라우팅. 접근자: `slotBay(i)`(포트레이트 합성), `setRootVisible/setLoadingVisible/setFlatBackgroundVisible/setMainMenuMessage/clearRoomCodeInput/hideAllSlotBays`, `panelTexture()/secondaryButtonTexture()`. **대기실 우측 스토리 패널**: 슬롯 4개를 좌측 60%로 묶고 우측 40%에 텍스트 패널(`storyPanelBg_/storyTitleLabel_/storyContentRoot_/storyTextLabel_`, `build()`에서 구성). 텍스트는 `../resources/story/intro.txt`(UTF-8, 컬럼 폭에 맞춰 사전 줄바꿈)를 `loadStoryTextUtf8`(자체 UTF-8→UTF-16 디코더 사용 — pch `NONLS`로 `MultiByteToWideChar`/`CP_UTF8` 불가)로 로드. 스크롤 미구현(본문은 `storyContentRoot_` 하위에 격리 → 향후 스크롤뷰가 래핑). **본문은 한 줄당 Label 하나로 스택**(`storyLineLabels_`): 폰트 D2D 전역 비트맵이 1024×256이라 한 Label이 256px를 넘으면 `CreateBitmapFromText`가 실패해 아무것도 안 그려짐 → 여러 줄을 한 Label에 담지 않고 `\n` 단위로 분할해 단일 라인 라벨로 쌓음(`lineH` 피치, `maxLines`로 패널 높이 클램프). `storyContentRoot_`는 `zOrder=2`로 패널 배경(`storyPanelBg_` z0) 위에 둬야 함 — `renderTree`가 형제를 **불안정 정렬**(`std::ranges::sort`)해 같은 z면 텍스트가 크림 패널 뒤로 가려짐. 자동 워드랩은 라벨 폭이 아닌 1024 기준이라, 빌드 시 `gfx_->measureText`로 각 source 줄을 컬럼 폭에 맞춰 greedy 워드/문자 단위로 미리 줄바꿈(`wrapToWidth`, 공백 우선·없으면 문자 단위; 측정용 GFX는 `loadTextures`에서 `gfx_`로 캐시, measureText/createFont는 물리 px이라 uiScale 적용) 후 display 줄마다 라벨 생성. 스토리 폰트는 누렁 패널 대비 위해 어두운색(폴백 단색도 밝게). **대기실 풀블리드**: 대기실 패널은 세이프에어리어(1024×768)가 아니라 `uiManager.screenWidth()/uiScale()` 기반 풀스크린 레이아웃 크기로 빌드되어(슬롯/헤더/툴바/스토리 모두 `roomPanelW`에서 파생 → 좌우로 확장) 와이드 해상도에서 빈 띠 없이 채움(로딩 오버레이와 동일한 풀블리드 패턴, 하드코딩 캡 제거). `applyDisplaySettings`가 `setScreenSize` 후 `build()` 재호출하므로 해상도 변경 추종 |
| `GameSettings` | `ui/settingsPanel.hpp` | 게임플레이용 영속 설정 값 구조체(fullscreen/vsync/allyDamageVisible/resolutionIndex/monsterDamageOpacity). `Game`이 소유(`settings_`), 로비·인게임·게임플레이가 공유. vsync 기본 ON — 한 GPU 다중 클라가 vsync 없이 Present하면 DWM이 굶어 TDR 유발(`GFX::setVsync`, `Game::render`에서 매 프레임 반영) |
| `UI::SettingsPanel` | `ui/settingsPanel.hpp/cpp` | 씬 비종속 설정창. `uiManager_.root()` 직속(zOrder 50)에 빌드, `open()/close()/toggle()/isOpen()`로 토글 → 로비/인게임(ESC) 공용. `build(uiManager, panelTex, buttonTex, GameSettings&)`, `refreshPreview()`. 값 편집은 `GameSettings&`로 write-through |
| `Game` 통합 | `online/onlineGame.cpp` | `enterLobby`: `lobbyUI_.loadTextures/build` + `settingsPanel_.build`. `refreshLobbyUI()`는 씬/세션/로컬 인증 상태로 `LobbyUI::ViewState` 스냅샷을 만들어 `lobbyUI_.refresh()`에 위임(+메인메뉴 이탈 시 `settingsPanel_.close()`). `makeLobbyCallbacks()`가 로그인·회원가입(아이디/비밀번호/닉네임)·방 버튼 액션을 `lobbyLogin/lobbyRegister/lobbyCreateRoom` 등에 연결. 인증 및 회원가입 중복 집합은 프로세스 메모리에서만 유지되고 서버 인증 패킷은 없음. `LobbyScene`/`renderWaitingRoom`/`enterInGame`/`lobbyLeaveRoom`은 컴포넌트 메서드 호출 |
| 인게임 ESC 토글 + 입력 차단 | `online/onlineGame.cpp` (`processInput`/`receiveWndMsg`) | `processInput`에서 `VK_ESCAPE` 엣지→`settingsPanel_.toggle()`. 열려 있으면 `processInputGame`/Enter·Space 토글 skip + 마우스 델타 클리어(early return)로 인게임 입력 차단. 열림/닫힘 전이(`settingsOpenPrev_`)에 따라 커서 해제·표시↔게임플레이 모드(`cursorCaptureEnabled_`/`cursorShowEnabled_`) 복원. `WM_SETFOCUS`는 설정창 열림 시 커서 복원 생략 |
| 디스플레이 설정 런타임 변경 (해상도 + 전체화면) | `online/onlineGame.cpp` (`applyPendingDisplaySettings`/`applyDisplaySettings`/`rebuildAvailableResolutions`), `gfx.cpp` (`GFX::resize`), `main.cpp` (`applyDisplayMode`/`getCurrentMonitorSize`), `sharedResources.cpp` (`GBuffer::eraseGBuffer`), `ui/UIManager.cpp` (`resetInteractionState`) | `update()` 진입부에서 `settings_.resolutionIndex`·`fullscreen` 변화를 감지해 **프레임 안전 지점**에서 적용(버튼 콜백 내 재빌드 금지 → dangling 방지). 창모드 해상도는 후보 `{1024×768,1280×720,1920×1080,2560×1440}` 중 **현재 모니터에 들어가는 것만** `rebuildAvailableResolutions`로 필터(`getCurrentMonitorSize` 기준) → FHD에선 1440p 자동 숨김. `resolutionIndex`는 이 목록 인덱스(`SettingsPanel`이 목록을 받아 라벨/스텝 클램프). 전체화면은 **borderless**(`WS_POPUP`+모니터 전체, `applyDisplayMode`) — exclusive 아님, 스왑체인 windowed 유지. `GFX::resize`: GPU idle→백버퍼/깊이/GBuffer/HiZ 해제(풀 슬롯 반납)→`ResizeBuffers`→재생성(뷰포트는 `gClientRect` 자동 추종). `LobbyUI/SettingsPanel::build` 멱등(재빌드). 상세: `docs/lobbyUISeparation.md` |

### 전투 피드백 UI (Damage Number / Kill Count)

설계 문서: `client/docs/combatFeedbackUI.md`

| 항목 | 파일 | 설명 |
|------|------|------|
| `DigitAtlas::emitNumber()` | `ui/digitAtlas.hpp/cpp` | 0~9 10칸 sprite atlas(`resources/UI/damage_digits.dds`)로 정수를 자릿수별 `UIPipeline::DrawEvent`로 emit. `uvScaleBias=(0.1,1,d*0.1,0)`로 셀 선택, `colorMul` 틴트. 상태 없는 공유 헬퍼 |
| `DamageNumberSystem` | `damageNumberSystem.hpp/cpp` | 월드앵커 + 균일 화면 크기 떠오르는 데미지 숫자 풀(`kMaxActive=256`). `spawn`(동일 대상 `mergeWindow` 내 누적 병합), `update(dtSec)`, `render`(`worldToScreen`→`emitNumber`, easeOutCubic 떠오름 + 팝/페이드). 게임 스레드 단독, 락 없음 |
| `DamageNumberTuning` | `damageNumberSystem.hpp` | 연출 v1 사양 상수 묶음(lifetime/pop/floatUp/fade/scale/bigHitThreshold 등) |
| `UI::KillCountWidget` | `ui/widgets/KillCountWidget.hpp/cpp` | 상단 HUD: 스컬 아이콘(`icon_kill.dds`) + 누적 킬. 킬 팝, streak 표시, 마일스톤(10/25/50/100) 금색 플래시. `addKill()`은 게임 스레드에서 호출 |
| `KillCountTuning` | `ui/widgets/KillCountWidget.hpp` | Kill Count 연출 상수 묶음 |
| onlineGame 통합 | `online/onlineGame.cpp` | UI 셋업: `killCountWidget_` add + `damageNumberSystem_.init()`. 이벤트 디스패치 루프: `receive` 직전 `prevHp` 캡처 → `dmg` 계산 → `spawn`, 고블린 `EvDeath` 시 `addKill()`. `InGameScene`: `damageNumberSystem_.update()`. `renderInGame`: `uiManager_.render` 직전 `damageNumberSystem_.render()` |

### 파티원 HP HUD (인게임)

디버깅 노트(멀티클라 연쇄 종료/TDR): `client/docs/gpuDeviceStability.md`

| 항목 | 파일 | 설명 |
|------|------|------|
| `Game::createOtherPlayerHud()` | `online/onlineGame.cpp` | 원격 플레이어 1명분 HUD 생성: 월드 HP바(worldBar) + 좌측 파티 행(partyRoot: 하트/무기 아이콘/이름/HP바). `otherPlayerHpBars_[id]`에 등록, 재호출 시 기존 위젯 제거 후 재생성(멱등) |
| `Game::updatePartyHpHudLayout()` | `online/onlineGame.cpp` | 로스터(`inGamePartyPlayerIds_`) 순서로 파티 행 재배치 + 이름 라벨 갱신. 해상도 변경(`updatePlayerHpHudLayout`)·HUD 생성·`removePlayer`에서 호출(퇴장 시 빈 줄 제거) |
| `Game::register/unregisterInGamePartyPlayer()`, `partyDisplayName()` | `online/onlineGame.cpp` | 파티 로스터 + **등록 시점 고정 이름**(`inGamePartyNameById_`, "playerN"). 인덱스 기반 이름은 퇴장 시 번호가 밀려 클라 간 표시가 어긋나므로 등록 시 1회 부여 후 불변. `prepareInGamePartyRoster`(S_Enter)가 이름 시퀀스를 리셋 |
| `Game::refreshSkillCtx()` | `online/onlineGame.cpp` | `skillCtx_`의 프레임 단위 포인터(evList/pTimer/objectById) 재동기화. `InGameScene` 매 프레임 + APC(프레임 시작 `SleepEx`)에서 스킬 시스템에 진입하는 패킷 핸들러(`removePlayer`/`onSkillStart`)가 호출 — 같은 배치에서 `skillObjectById_` resize 시 stale 포인터 방지 |

### 사용 패턴 (game.cpp 통합 예시)

```cpp
// 멤버 추가
UI::UIManager uiManager_;

// 씬 셋업에서 트리 구성
auto hpBar = std::make_unique<UI::ProgressBar>();
hpBar->anchor = UI::Anchors::BottomCenter;
hpBar->pivot  = UI::Pivots::BottomCenter;
hpBar->width  = UI::DimValue::px(1024.f);
hpBar->height = UI::DimValue::px(64.f);
hpBar->offsetY = UI::DimValue::px(-40.f);
hpBar->fillTex = assetManager_.getTexture("hpFill");
uiManager_.root()->addChild(std::move(hpBar));

// update()에서
uiManager_.layout();
uiManager_.update(dt, gfx_, &fontHandle_);

// render()에서
uiManager_.render(gfx_);

// receiveWndMsg()에서 (입력 차단)
if (uiManager_.onWndMsg(msg, wParam, lParam)) return 0;
```

### 레이아웃 좌표계 주의사항

- `UIElement::layout()` 내부는 **top-down** 좌표 (Y=0 = 상단)
- `buildWorldMatrix()` 에서 `translateY = screenHeight - topDownCenterY` 로 뒤집음
- `ui.hlsl`은 픽셀 Y=0 → NDC -1 (하단) 매핑이므로 이 변환이 필수

---

## 12. 스킬 에디터 (standalone)

standalone 실행 모드는 스킬/몬스터 패턴 제작 툴(에디터)로 동작한다.
`StandAlone::Game`이 월드(에셋/씬/물리/gfx)를 셋업하는 호스트이고, 에디터 로직은
`client/editor/` 모듈이 담당한다. 설계 문서: `docs/skillEditor.md`.

**파일:** `client/editor/`

| 항목 | 위치 | 설명 |
|------|------|------|
| `Editor::CharacterKind` / `CharacterDef` / `kCharacterSkillMap` | `editor/characterSkillMap.hpp` | 전역 캐릭터→스킬 매핑 상수. Player(18스킬) + 몬스터 7종(Goblin/Mushroom/Snake/Birdy/Bomber/Slime/Treant) 전부 활성. 스킬명=`<Mon>_<Attack>`(lua `resources/skills/*_attackN.lua`). Player 목록은 런타임에 **선택 무기로 필터**(`SkillAsset::weaponType`; 무기 미지정 0xFF 스킬은 목록 후미에 유지) |
| `Controller::setMonsterCaster(kind)` | `editor/editorController.cpp` | 몬스터 선택 시 단일 몬스터 객체(goblin_)를 해당 모델+`AnimBlender<Name>`로 핫스왑(`setModel`+`adoptAnimBlender`). kind별 switch(전부 활성). `selectCharacter`가 비-Player에서 호출. InitRefs에 `assetManager`/`animSystem` 주입(game.cpp editorRefs) |
| `Editor::SkillDraft` | `editor/skillDraft.hpp/.cpp` | 컴파일 에셋의 original/draft 사본 + 편집 필드 목록 + diff 콘솔 덤프 |
| `SkillDraft::Field` / `FieldType` | `editor/skillDraft.hpp` | 편집 가능한 스칼라 필드(center/half/euler/onHit/time/duration) |
| `SkillDraft::load/buildFields/applyDelta/dumpDiff` | `editor/skillDraft.cpp` | 로드/필드구성/넛지/가이드 출력 |
| `Editor::Controller` | `editor/editorController.hpp/.cpp` | 드롭다운 3개(캐릭터/무기/스킬), 히트박스 피킹, nudge 편집, slow-mo/pause, free-fly 카메라 |
| `Controller::buildUI` 레이아웃 상수 | `editor/editorController.cpp` 익명 ns | `kMarginX/kCaptionY/kRowY/kColX*/kColW*/kStatusY/kPanelY/kZOrder*` — 상단 한 줄 3열(캡션+드롭다운) + 상태 라벨 + 편집 패널. `UI::Dropdown::setup()`이 zOrder를 100으로 덮으므로 **zOrder는 setup() 이후 지정** |
| `Controller::selectWeapon` / `applyWeaponToPlayer` | `editor/editorController.cpp` | 무기 드롭다운(Player 캐스터 전용, 몬스터 선택 시 캡션과 함께 `visible=false`). 항목 인덱스=`PlayerWeaponType` ordinal(`kWeaponItems`). 선택 시 `equipPlayerWeapon`(모델+클립 세트) → 스킬 목록 재필터 |
| `Controller::rebuildSkillList` | `editor/editorController.cpp` | 현재 캐스터의 스킬 목록 구성(Player는 선택 무기 스킬 + 무기 미지정 스킬 후미) → `rebuildSkillDropdown` → `selectSkill(0)` |
| `Controller::handleInput` | `editor/editorController.cpp` | 키/마우스 처리 (Game::processInput에서 위임) |
| `Controller::updateCamera` | `editor/editorController.cpp` | follow(camera_.update) / free-fly(camera_.setView) 분기 |
| `Controller::refresh` | `editor/editorController.cpp` | 선택 히트박스 하이라이트 + 패널 갱신 |
| `Controller::pickHitbox` | `editor/editorController.cpp` | screenToRay + SkillSystem::pickHitbox |

**SkillSystem 에디터 API:** `skill/skillSystem.hpp/.cpp`

| 항목 | 위치 | 설명 |
|------|------|------|
| `SkillSystem::startSkillAsset` | `skillSystem.cpp` | 레지스트리 외부의 draft 에셋(포인터)으로 재생 |
| `SkillSystem::collectActiveHitboxes` | `skillSystem.cpp` | 활성 bone 히트박스 열거(`ActiveHitboxRef`) |
| `SkillSystem::pickHitbox` | `skillSystem.cpp` | ray로 최근접 활성 히트박스 OBB 선택 |
| `SkillSystem::setHitboxLocalOBBs/setHitboxOnHit` | `skillSystem.cpp` | 활성 히트박스 live override (pause 중 즉시 반영) |
| `SkillSystem::renderDebugHitboxes(bv, selectedIdx)` | `skillSystem.cpp` | 선택 박스 하이라이트 색 |
| `AttachedHitbox::defIdx` | `skillSystem.hpp` | 활성 히트박스 → asset hitboxDef 역매핑 |
| `SkillHitboxDef::localOBBEulerDeg` | `skillTypes.hpp` | authoring euler(yaw/pitch/roll), 컴파일러가 보관(에디터 round-trip용) |
| `SkillHitboxDef::penetrate` | `skillTypes.hpp` (클라/서버) | VFXParticle 전용: false=비관통(첫 피격 시 소스 파티클 소멸). 서버: `ParticleHitboxSource::consumedKeys`/`consumeAnchor`로 권위 처리. `particleHitboxDeterminism.md` §8 |
| `SkillEventPayload::PlayVFX` (localEulerDeg/advanceForwardLocal/flags) | `skillTypes.hpp` | VFX 배치+방향 오프셋+진행방향+yawOnly; lua orient/advance/groundLock 키 |
| PlayVFX 디스패치 (aim=rotateRPYH×baseRot, yawOnly, 2/4-인자 play) | `skillSystem.cpp` | `dispatchEvent` PlayVFX case |
| PlayVFX 컴파일 (orient/advance/groundLock 파싱) | `skillCompiler.cpp` | `tableToAsset` PlayVFX case |
| `SkillEventType::PlaySound` + `SkillEventPayload::PlaySound` (soundName[24] + `maxDurationMs`/`fadeMs` u16 + `volume` float) | `skill/skillTypes.hpp` (클라 전용) | 박자별 연출 SFX. lua `PlaySound{sound=..., durationMs=, fadeMs=, volume=}`. `durationMs>0`이면 시작 후 그만큼 뒤 `fadeMs`로 페이드아웃(사운드가 짧은 이펙트보다 길게 남지 않게; 예 arrow_rain). `volume`(0..1, 기본 1)은 이벤트별 게인—카탈로그 defaultVolume과 별개라 같은 음을 다른 스킬에서 다른 크기로(예 PiercingMulti 난무 0.6) 재생 가능. 서버는 미지원 이벤트로 스킵(결정론 무영향) |
| PlaySound 디스패치 (caster `renderState().pos`에서 `ctx.playSound` 호출) | `skill/skillSystem.cpp` `dispatchEvent` PlaySound case | `SkillDispatchContext::playSound` 콜백(클라=`playSfx3D`, 서버=null no-op) |
| PlaySound 콜백 바인딩 | `standalone/game.cpp` / `online/onlineGame.cpp` skillCtx 셋업 | `skillCtx_.playSound = [](name,pos){ sound().playSfx3D }` |
| 스킬 제작 가이드 (Lua API + 유형별 레시피: 검격/화살/부채꼴/PBAoE/메테오) | `docs/skillCreationGuide.md` | 스킬 작성자용 문서 |
| `screenToRay(...)` | `camera.hpp` | 스크린 픽셀 → 월드 ray (inverse view-proj) |

**입력 맵:** Space=재생/재시작, LMB=히트박스 피킹(ray + 화면근접 폴백), Esc=히트박스 편집 종료,
↑/↓=필드 이동, ←/→=넛지(Shift=coarse), [ / ]=timeScale, 0=pause,
F=free 카메라(WASD+RMB look, Q/E 상하), P=diff 덤프, R=리셋.

**기타:** 드롭다운 2개는 좌우 배치(캐릭터|스킬), 조작법은 우상단 helpLabel, 상태는 좌상단
statusLabel(스킬/scale/cam/target HP). 타깃 더미는 reset 시 `positionDummyInFront()`가 caster
정면 3.5m·지형 높이(InitRefs::terrainHeightAt)로 배치. 기존 standalone HUD는 제거됨.

**이펙트↔스킬 1:1 기반:** 기존 이펙트 드롭다운 18종 ParticleEffect와 1:1로 짝지은 기본 스킬 lua를
`resources/skills/`에 추가(`SkillCompiler::compileAll`이 디렉터리 스캔→자동 등록). VFX는 경로가
아니라 lua `PlayVFX{vfxId}`→`StandAlone::Game::skillVfxById_[vfxId]`(`ParticleEffect*` 배열) 인덱스
바인딩(`standalone/game.cpp` + `online/onlineGame.cpp`, 0=`bloodEffect_`(칼/창/완드 피격 혈흔, `blood_hit.json`+Plane 곡면 메시+3x3 시트), 1~18=각 Effect). 칼=sword_slash·창=piercing·완드=spikes lua의 `onHit{vfxId=0}`이 피격 시 재생(활/arrow 제외). 스킬명은
`kCharacterSkillMap` Player에 등록. vfxId↔Effect↔skill/lua 매핑표는 `docs/skillEditor.md`.

| 항목 | 위치 | 설명 |
|------|------|------|
| 18종 스킬 lua (slash_wave/slash_combo/.../piercing_multi) | `resources/skills/*.lua` | 이펙트당 기본 스킬(PlayAnimation+PlayVFX+SpawnHitbox/Destroy+OnHit 시작값) |
| `skillVfxById_` 1~18 바인딩 | `standalone/game.cpp` | vfxId→ParticleEffect* 1:1, lua PlayVFX의 인덱스원 |

---

---

## 13. 지면 연계 스킬 / 파티클 (Terrain interaction)

**설계 문서:** `docs/terrainInteractingSkills.md` (얼음 기둥/화살비/낙하 마법구 등 지면 연계)

| 항목 | 위치 | 설명 |
|------|------|------|
| `GroundSampler` struct | `groundSampler.hpp` | height/normal 콜백 번들; GFX/스킬 레이어 디커플링, `operator bool()`=지면 유무 |
| `ParticleSystem::setGroundSampler` | `particleSystem.hpp` | 비소유 terrain 질의 바인딩 |
| `ShapeModule::GroundConform` + `groundOffset` | `particleModules.hpp` | 스폰 시 지면 컨폼(None/SnapY/SnapAndAlign) |
| 지면 컨폼 스폰 hook | `particleSystem.cpp` `spawnParticle` | origin.y 스냅 + SnapAndAlign 노멀 정렬 |
| `ParticleCollisionModule` | `particleModules.hpp` | 지면 충돌(GroundStop/Kill/Bounce); Kill→Death 서브이미터 |
| 지면 충돌 hook | `particleSystem.cpp` `update` 루프 | `vel.y<0` 게이트, 표면 교차 처리 |
| `alignYToNormalMat` | `particleSystem.cpp` | +Y→노멀 회전 행렬 |
| `ParticleEffect::setGroundBehavior` | `particleEffect.cpp` | lua 구동: **conform=전 시스템(서브이미터 포함, 기둥 본체가 Birth 서브이미터인 경우 대응)**, **collision=top-level만**(버스트 즉사 방지). **effect json은 지면 정보 미포함** |
| `kPlayVFXFlagGroundSnap`/`GroundAlign` + `ParticleCollision/Conform` mask·shift | `skill/skillTypes.hpp` | PlayVFX 지면 스냅/정렬 + 파티클 충돌·컨폼 모드(flags 1바이트 패킹, 56B 유지) |
| `AttachType::Ground` + `AttachTarget::groundAlign/groundAnchorRef` | `skill/skillTypes.hpp` | 지면 고정 히트박스 attach (align=분산모드 노멀정렬, anchorRef>=0=등록앵커 강체 점충돌) |
| `SetGroundAnchor` 이벤트 + 페이로드 + `kGroundAnchorFlagAlign` | `skill/skillTypes.hpp` | 점 충돌 앵커 프레임 등록 이벤트 |
| `SkillInstance::groundAnchors[kMaxGroundAnchors]` | `skill/skillSystem.hpp` | 등록된 지면 앵커 프레임(pos+orient), 여러 별도 히트박스가 공유 |
| `SetGroundAnchor` dispatch | `skill/skillSystem.cpp` `dispatchEvent` | 시전 yaw 회전+지면 스냅+옵션 align → groundAnchors[id] 등록(서버도 권위적, no-op 아님) |
| `SkillInstance::CastAnchor` | `skill/skillSystem.hpp` | 시전자 pos+yaw(시전 시점), Ground 히트박스 앵커 |
| `SkillDispatchContext::ground` | `skill/skillSystem.hpp` | `const GroundSampler*` 주입 |
| `alignQuatYToNormal`/`captureCastAnchor` | `skill/skillSystem.cpp` | 정렬 쿼터니언 / 앵커 캡처 |
| PlayVFX 지면 스냅 dispatch | `skill/skillSystem.cpp` `dispatchEvent` PlayVFX | worldPos.y 스냅 + `fx->setGroundSampler` |
| SpawnHitbox Ground 브랜치 | `skill/skillSystem.cpp` `dispatchEvent` SpawnHitbox | **anchorRef<0: OBB별 독립 스냅(분산 융기) / anchorRef>=0: 등록 앵커 프레임에 강체 배치(점 충돌, 별도 히트박스가 onHit 유지한 채 공유)** |
| lua `groundSnap/groundAlign`, `particleCollision/particleConform`, `{type="Ground", align=, anchor=}`, `SetGroundAnchor{id,offset,align}` | `skill/skillCompiler.cpp` | 플래그/파티클 모드/Ground attach·앵커 이벤트 파싱 |
| `GroundAttach{align,anchor}` / `GroundAnchor{id,offset,align}` 헬퍼 | `resources/skills/lua/skill_api.lua` | Ground attach + 앵커 등록 lua 헬퍼 |
| PlayVFX 파티클 거동 디코드+적용 | `skill/skillSystem.cpp` PlayVFX | flags 비트3-6 → `fx->setGroundBehavior` |
| `groundSampler_` 바인딩 | `standalone/game.cpp`, `online/onlineGame.cpp` | chunkManager_→skillCtx_.ground |

> 서버 미러: `RoomServer/skill/{skillTypes,skillSystem,skillCompiler}.*`, `Room::bindGroundQueries`.
> 레거시 제거: `onlineGame.cpp`의 `SwordEffect::ArrowRain/RedEnergyExplosion` 하드코딩 지면 스냅 경로 삭제.

---

## 14. 사운드 시스템

**백엔드:** miniaudio (단일 헤더, `client/sound/miniaudio.h` — 외부 참조, 수정 금지)
**엔진 추상화:** `client/sound/soundManager.hpp` / `soundManager.cpp`
**카탈로그:** `client/sound/soundCatalog.hpp` / `soundCatalog.cpp`
**자산 폴더:** `resources/audio/bgm/*.wav`(lobby / Action 5), `resources/audio/sfx/ui_click.wav` + `sfx/sword/*.mp3` + `sfx/bow/*.mp3` + `sfx/wand/*` + `sfx/spear/*.mp3`(스킬음, mp3/wav 혼용)

| 항목 | 위치 | 설명 |
|------|------|------|
| `SoundManager` 클래스 | `sound/soundManager.hpp` | 백엔드를 pimpl로 은닉. init/shutdown/update, BGM, SFX(2D/3D), 리스너, 버스 볼륨 |
| `SoundManager::Bus` enum | `sound/soundManager.hpp` | Bgm / Sfx / Ui — master 하위 sound group |
| `SoundManager::Impl` | `sound/soundManager.cpp` | `ma_engine` + 버스 그룹 + BGM 2슬롯(크로스페이드) + SFX voice 풀(32) + warnOnce |
| `playBgm/stopBgm` | `sound/soundManager.cpp` | 스트리밍 BGM, 페이드/크로스페이드 |
| `playSfx/playSfx3D` | `sound/soundManager.cpp` | voice 풀 기반 one-shot. 3D는 월드 위치 spatialization. `playSfx3D(name,pos,vol,maxDurationMs,fadeMs)` — maxDurationMs>0이면 시작 후 그만큼 뒤 fade-stop 예약(Voice `stopAtSec`/`stopFadeMs`, `update()`에서 발화→`!ma_sound_is_playing`로 회수) |
| `setListener` | `sound/soundManager.cpp` | 카메라 eye/forward/up → miniaudio 리스너 (좌-손 패닝 핸디드니스는 Stage4 튜닝 대상) |
| `miniaudio_impl.cpp` | `sound/miniaudio_impl.cpp` | `MINIAUDIO_IMPLEMENTATION` 전용 TU. **PCH NotUsing** (vcxproj), `MA_NO_ENCODING` |
| `findSound(name)` | `sound/soundCatalog.cpp` | 논리이름→{경로,버스,loop,stream,기본볼륨} 테이블 조회 |
| 소유/접근 | `ClientApp.cpp` `init/release/update`, `ClientApp::sound()` | 프로세스 전역 단일 소유, init 1회/매 프레임 update tick |
| BGM 씬 연결 | `online/onlineGame.cpp` `enterLobby`("lobby") / `enterInGame`("ingame") | 씬 전환 시 크로스페이드 |
| UI 클릭음 훅 | `ui/widgets/Button.hpp` `sClickSfx`(정적), `online/onlineGame.cpp` `enterLobby`에서 1회 바인딩 | UI 레이어 ↔ 사운드 백엔드 디커플링 |
| (제거됨) 전투/HUD 플레이스홀더 SFX | — | hit/death/attack(이벤트 루프 디스패치)·skill_hit·ui_hover·skill_ready 카탈로그+.wav+코드 일괄 제거(2026-06-19 정리). 현재 SFX = ui_click + PlaySound 스킬음만 |
| 스킬 SFX(검 4종) | `sound/soundCatalog.cpp`(`sword_slash_1`/`sword_slash_finish`/`sword_slash_7`/`slash_wave`, .mp3), 각 `resources/skills/*.lua` PlaySound | PlaySound 이벤트 경유 스윙음. SwordSlash(sword_slash@100ms)·Slash7(sword_slash_7@100ms)·SlashWave(slash_wave@120ms)·SlashCombo(slash_1 150/550/750 + finish 1250). 단일 스윙=1회, 콤보=히트박스 웨이브별 |
| 스킬 SFX(활 5종) | `sound/soundCatalog.cpp`(`arrow_default`/`arrow_rain`/`arrow_charge`/`charge_shoot`/`charge_explosion`, .mp3, `sfx/bow/`) | Arrow·ArrowVolley=`arrow_default` lua PlaySound@120(발사), ArrowRain=`arrow_rain`@120(durationMs 1200/fadeMs 200 — 레인 종료에 맞춰 페이드아웃). EnergyExplosionArrow=차징(`arrow_charge` lua@120) + 발사(`charge_shoot`)·폭발(`charge_explosion`)은 `setChildSpawnCallback` 이벤트 구동(타임라인 아닌 실제 spawn 시점). |
| 스킬 SFX(완드 4종) | `sound/soundCatalog.cpp`(`quake`/`ice_crossfade`/`ice_front_attack`/`red_energy`, `sfx/wand/`, mp3+wav), 각 `resources/skills/*.lua` PlaySound | 전부 PlayVFX 시점 @150ms lua PlaySound 단발. Spikes=`quake`, CrystalsCrossFade=`ice_crossfade`, CrystalsFrontAttack=`ice_front_attack`, RedEnergyExplosion=`red_energy`(red_energy.mp3; **PlaySound@0**=PlayVFX와 동일 캐스트 시작; durationMs/fadeMs로 페이드아웃). ⚠️PlaySound를 캐스트 시작보다 늦게 두면 interruptible 스킬에서 연속 캐스트·서버 거부 롤백(`interruptAll`) 시 종료된 인스턴스가 그 이벤트를 스킵→VFX는 떠도 소리 누락. 캐스트음은 0ms 권장.
| 스킬 SFX(창 3종) | `sound/soundCatalog.cpp`(`spear1`/`spear2`/`spear3`, `sfx/spear/`, mp3), 각 `resources/skills/piercing*.lua` PlaySound | Piercing(기본)=`spear1`@100, PiercingSlash(dial0)=`spear2`@100, PiercingCircleSlash(dial1)=`spear3`@100. **PiercingMulti(dial2 난무)=웨이브별 스탭음**: lua 루프로 10발(100+60*w ms, 파티클 버스트 동기) spear1/2/3 로테이션 + durationMs90/fadeMs50 컷 + volume0.6 → 겹침(위상간섭·음량누적·mud) 방지 | **주의: lua `sound=`는 파일명이 아니라 카탈로그 논리이름**(확장자 없이; findSound가 논리이름→경로 매핑) |
| 3D 리스너 갱신 | `online/onlineGame.cpp` `camera_.update` 직후, `standalone/game.cpp` `camera_.updateGFX` 직전 | 매 프레임 카메라 추종 |
| 볼륨 설정값 | `ui/settingsPanel.hpp` `GameSettings` masterVolume/bgmVolume/sfxVolume(%) | 영속 설정 |
| 볼륨 설정 UI | `ui/settingsPanel.cpp` "사운드" 그룹(makeStepperRow ×3) | 투명도 행과 동일 스텝퍼 패턴 |
| 볼륨 적용 | `online/onlineGame.cpp` `render()` | 매 프레임 폴링 → setMasterVolume/setBusVolume. UI 버스=SFX 볼륨 |
| 포커스 뮤트 | `main.cpp` `gWindowActive`+`WM_ACTIVATEAPP`, `onlineGame.cpp` `render()` 게이팅 | 창 비활성 시 master=0 |
| SFX 디듀프 | `sound/soundManager.cpp` `sfxAllowed` (kSfxDedupeCooldownSec=35ms) | 동일 SFX 버스트 throttle |
| SFX 프리로드(첫 재생 무지연) | `sound/soundManager.cpp` `Impl::preloadSfx`(init에서 호출)+`Impl::sfxTemplates`, `soundCatalog.cpp` `allSounds()` | 비스트리밍 카탈로그 전부 init 시 디코드→템플릿(ma_sound) 보관. `startOneShot`은 템플릿 있으면 `ma_sound_init_copy`로 클론(디스크/디코드 0), 없으면 `init_from_file` 폴백. + init 시 무음 워밍업 1회 |
| SFX 선행무음 스킵(onset 타이트) | `sound/soundManager.cpp` `Impl::detectLeadSilenceFrames`+`sfxLeadFrames`, `startOneShot`의 `ma_sound_seek_to_pcm_frame` | mp3 인코더 패딩/조용한 클립 머리 = 디코드된 PCM 앞 무음 → 프리로드 때 첫 비무음 프레임(-50dBFS) 측정, 재생 시 그만큼 seek. **첫 타격음 지연의 실제 원인**(디코드 아님; wav는 무음 없어 정상이었음) |
| 존 BGM 예시 | `online/onlineGame.cpp` `bindZoneHandlers()` | 태그 정의 시 `playBgm(...)` 1줄 연결 (데이터 의존) |

### UI 스크롤/클리핑 (설정 패널 창모드 오버플로 대응)

| 항목 | 위치 | 설명 |
|------|------|------|
| GPU 시저 클리핑 | `UIElement::clipsChildren` + `GFX::pushUIClip/popUIClip`(clip 스택, 교집합) + `gfx.cpp:addDrawEvent(UIPipeline)` stamping | clipsChildren 요소의 하위를 시저로 클립 |
| 이벤트별 시저 | `uiPipeline.cpp` 단일/멀티스레드 드로우 루프 | `DrawEvent.clip/clipRect` 따라 `RSSetScissorRects` 전환 |
| 렌더/히트 클립 | `UIElement::renderTree`(push/pop), `UIManager::hitTest`(clip 조상 밖 거부) | 스크롤 아웃된 위젯은 렌더·입력에서 제외 |
| 마우스 휠 | `UIElement::onMouseWheel`(virtual bool) + `UIManager::onWndMsg`(WM_MOUSEWHEEL → hovered 조상 체인) | 휠 입력 라우팅 |
| `ScrollView` 위젯 | `ui/widgets/ScrollView.{hpp,cpp}` | clipsChildren 뷰포트 + content() 호스트 + 휠 스크롤(contentHeight/viewportHeight) |
| 설정 패널 스크롤 | `ui/settingsPanel.cpp` | 제목/닫기 고정, 행들은 ScrollView content로. 창모드 오버플로 해결 |

> **동시성:** 모든 SoundManager 호출은 게임 스레드(update/render)에서만. 네트워크/IOCP 스레드에서 직접 호출 금지(이벤트 post로 우회).
> **디바이스 실패:** init 실패 시 enabled=false로 모든 호출이 안전한 no-op. 파일 누락 시 1회 경고만.
> **Stage 4 완료:** 설정 볼륨(마스터/BGM/SFX, 스크롤 가능한 패널), 포커스 상실 뮤트, 동일-프레임 SFX 디듀프. 존 BGM은 예시 제공(실제 존 태그 정의 시 연결).
> **남은 튜닝:** 좌-손 좌표계 패닝 핸디드니스 실측(좌/우 반대면 `setListener` 축 반전). 설정값 디스크 영속화 미구현(현재 세션 한정).

---

## 관련 문서

- `docs/terrainInteractingSkills.md` — 지면 연계 스킬/파티클 설계
- `docs/skillEditor.md` — standalone 스킬 에디터 설계
- `docs/graphicsArchitecture.md` — GFX 초기화 흐름, 파이프라인 구조
- `docs/physicsArchitecture.md` — PhysicSystem::step 단계, BVH 변환 체인
- `docs/gameArchitecture.md` — 게임 루프, 이벤트 시스템, CombatSystem 구조
- `CLAUDE.md` — 파일 인코딩, 빌드 방법, 아키텍처 문서 링크

---

## Dialogue / Monologue UI

| Item | Location | Description |
|------|----------|-------------|
| `UI::DialogueSystem` | `ui/dialogue/DialogueSystem.hpp/.cpp` | Loads event-to-window definitions, advances pages, and fades completed windows |
| `DialogueSystem::show` | `ui/dialogue/DialogueSystem.cpp` | Opens a dialogue by JSON `eventId` |
| Dialogue JSON | `../resources/UI/dialogues/dialogues.json` | Shared 1024x768 authoring data used by HTML preview and game |
| HTML authoring preview | `docs/dialogue_preview/index.html` | Live position, size, color, opacity, font, pages, and fade editor |
| Preview launcher | `docs/dialogue_preview/preview.ps1` | Serves the repository locally and opens the preview |
| Standalone integration | `standalone/game.cpp` | Input priority, per-frame fade update, and F8 sample trigger |
| Online integration | `online/onlineGame.cpp` | `init` in `setupStage`; `sample_intro` shown in `setupPlayer` (local player spawn complete); `sample_context` shown in `onZoneState` on first Hobgoblin clear (`WallHobgoblin`, state 1->0, gated by `completedArenaZoneIds_` insert result); per-frame `update` in `InGameScene`, `handleWndMsg` first in `receiveWndMsg`, gameplay input gated while `active()` |

---

## Tactical Zone Intro (arena entry title card)

| Item | Location | Description |
|------|----------|-------------|
| `UI::TacticalZoneIntro` | `ui/intro/TacticalZoneIntro.hpp/.cpp` | Self-contained overlay module: builds its own UI subtree, animates banner/emblem/title reveal; boss arena adds a glitchy WARNING phase |
| `TacticalZoneIntro::init` | `ui/intro/TacticalZoneIntro.cpp` | Builds the hidden widget tree under `UIManager::root`; textures from `AssetManager` |
| `TacticalZoneIntro::trigger` | `ui/intro/TacticalZoneIntro.cpp` | Starts the intro for an arena wall prefix (`WallHobgoblin`/`WallGrandbaum`/`WallIsys`/`WallBoss`); unknown prefixes return false |
| `TacticalZoneIntro::update` | `ui/intro/TacticalZoneIntro.cpp` | Per-frame alpha/offset/size animation; hides itself when finished |
| Online integration | `online/onlineGame.cpp` | `setupStage` init, local `ZoneSystem::Enter` triggers arena intro/BGM from `player_->pos()`, per-frame update in `InGameScene`; shared `onZoneState` never starts presentation |

---

## 15. 인벤토리 시스템

| 항목 | 위치 | 설명 |
|------|------|------|
| `ItemCatalog` / `Inventory` | `../common/inventory.hpp/.cpp` | 공유 아이템 정의·고정 슬롯·스택 모델, revision, JSON 검증 |
| `executeInventoryCommand` | `../common/inventory.cpp` | 서버와 standalone이 공유하는 사용/버리기 판정 및 수량·HP 변경 |
| 인벤토리 JSON | `../../resources/data/inventory.json` | 슬롯 수, 아이템 정의, 시작 슬롯의 단일 원본 |
| `UI::InventoryPanel` | `ui/inventoryPanel.hpp/.cpp` | E/Esc 토글, 6×4 슬롯, 호버 툴팁, 우클릭 메뉴, 입력 차단 |
| 온라인 연결 | `online/onlineGame.cpp`, `PacketManager.cpp` | 서버 스냅샷/result 수신, action 요청, HP 동기화 |
| standalone 연결 | `standalone/game.cpp` | 공용 액션 실행기의 로컬 동기 실행 및 커서 복구 |
| 서버 권한 처리 | `../../RoomServer/Room.cpp`, `PacketManager.cpp` | Player 소유 인벤토리, revision 검증, HP 브로드캐스트 |
| 프로토콜 | `../../ServerEngine/protocol.hpp` | `C_InventoryAction`, `S_InventorySnapshot`, `S_InventoryActionResult` |
| 유지보수 문서 | `inventorySystem.md` | 아이템 추가 방법, 확장 규칙, 빌드·검증 절차 |

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
| `computeBoxInertia()` | `rigidBody.hpp #26` | 박스 관성 텐서 헬퍼 |
| `computeCapsuleInertia()` | `rigidBody.hpp #27` | 캡슐 관성 텐서 헬퍼 |
| `Constraint` (abstract) | `constraint.hpp #12` | prepare/solveVelocity/solvePosition 인터페이스 |
| `ContactPoint` struct | `collision.hpp` | worldPos, normal(B→A), depth, acc 누적값 |
| `ContactConstraint` class | `contactConstraint.hpp` | PGS Normal + Coulomb 마찰 impulse solver; setExternalAccels()로 외력 보상 |
| `ContactConstraint::setExternalAccels()` | `contactConstraint.hpp` | 외력 가속도 설정 (prepare() 전 호출); Baumgarte bias에 외력 보상항 추가 |
| `RigidBody::setUserData()` / `userData()` | `rigidBody.hpp` | void* 게임 레이어 연결 포인터 (Object* 역참조용) |
| `PhysicsWorld::forEachContact()` | `physicsWorld.hpp` | step() 후 활성 ContactConstraint 순회 (템플릿) |
| `PhysicsTestObject` struct | `physicsTestObject.hpp #51` | bodies/halfExtents/joints/ignoredPairs 소유; activate/deactivate/visualize/applyImpulseAll/freezeAll |
| `PhysicsTestObject::ignoredPairs` | `physicsTestObject.hpp` | 1-hop+2-hop 충돌 무시 쌍; factory 함수가 채우고 activate/deactivate에서 setIgnoreCollision 호출 |
| `makePendulum()` | `standalone/game.cpp #75` | PhysicsTestObject factory: BallSocket 단진자 (kind=1) |
| `makeDoublePendulum()` | `standalone/game.cpp #100` | kind=2: BallSocket 이중 진자 |
| `makeHingeDoor()` | `standalone/game.cpp #136` | kind=3: HingeJoint 문 |
| `makeConeTwistArm()` | `standalone/game.cpp #170` | kind=4: ConeTwist 단일 팔 |
| `makeConeTwistChain()` | `standalone/game.cpp #203` | kind=5: ConeTwist 5-link 체인 |
| `makeHumanoidRagdoll()` | `standalone/game.cpp #243` | kind=6: 12 bodies A-pose, 11 joints(ConeTwist×7+Hinge×4), 1-hop+2-hop 충돌 무시 |
| `BodyPair` struct | `broadPhase.hpp` | broad phase 결과 쌍 |
| `BroadPhase` (abstract) | `broadPhase.hpp #36-40` | add/remove/update/queryPairs/queryAABB 인터페이스 |
| `BroadPhase::queryAABB` | `broadPhase.hpp #39` | AABB 쿼리 순수 가상 메서드 — 카메라 arm 장애물 후보 조회 |
| `BruteForceBroadPhase` | `broadPhase.hpp #50` | O(n²) 참조 구현 (후보 비교용으로 보존) |
| `SAPBroadPhase` | `broadPhase.hpp #74` | X축 Sort-and-Sweep, O(n log n) (기본 사용) |
| `SAPBroadPhase::queryAABB` | `broadPhase.hpp #74` | 정렬된 endpoints + active-set sweep으로 box 겹침 후보 반환 |
| `TerrainHeightField` struct | `terrain.hpp` | CPU-side 높이 데이터 (getHeightAt, getNormalAt) |
| `TerrainCollider` class | `collision.hpp` | Dynamic body ↔ 지형 높이맵 contact 생성 |
| `PhysicsWorld` class | `physicsWorld.hpp` | 시뮬레이션 진입점 |
| `PhysicsWorld::registerBody()` | `physicsWorld.hpp #37` | body + onRebuildBVH 콜백 + collisionGroup/Mask + broad phase 등록 |
| `PhysicsWorld::unregisterBody()` | `physicsWorld.hpp #43` | 등록 해제 |
| `PhysicsWorld::addJointConstraint()` | `physicsWorld.hpp #47` | 소유권 이전 joint 등록 |
| `PhysicsWorld::removeJointConstraint()` | `physicsWorld.hpp #48` | 소유 joint 제거 |
| `PhysicsWorld::addJointRef()` | `physicsWorld.hpp #52` | 비소유 joint ref 등록 (Ragdoll용) |
| `PhysicsWorld::removeJointRef()` | `physicsWorld.hpp #53` | 비소유 joint ref 제거 |
| `PhysicsWorld::setIgnoreCollision()` | `physicsWorld.hpp #58` | 특정 body 쌍의 충돌 완전 무시 (symmetric). Ragdoll이 joint 연결/2-hop 쌍 등록에 사용 |
| `PhysicsWorld::ignoreCollisionPairs_` | `physicsWorld.hpp #182` | normKey 정규화된 per-pair ignore set; generateContacts()에서 group/mask 이후 체크 |
| `PhysicsWorld::registerTerrain()` | `physicsWorld.hpp #57` | Static 지형 body + heightField 등록 |
| `PhysicsWorld::unregisterTerrain()` | `physicsWorld.hpp #60` | 지형 collider 해제 |
| `PhysicsWorld::registerCameraObstacle()` | `physicsWorld.hpp #67` | body를 카메라 obstacle로 cameraBroadPhase_에 등록 |
| `PhysicsWorld::unregisterCameraObstacle()` | `physicsWorld.hpp #68` | 카메라 obstacle 등록 해제 |
| `PhysicsWorld::queryCameraArm()` | `physicsWorld.hpp #73` | pivot→desiredEye arm 허용 길이 반환 (지형 N=6 샘플 + BVH raycast) |
| `PhysicsWorld::cameraBroadPhase_` | `physicsWorld.hpp #140` | 카메라 전용 SAPBroadPhase 인스턴스 (일반 physicsWorld broadPhase와 독립) |
| `PhysicsWorld::step()` | `physicsWorld.hpp #63` | integrate → generateContacts → solveConstraints |
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
| `RagdollBone` struct | `ragdoll.hpp #15` | boneIdx, body*(non-owning), parentJoint*(non-owning), capsuleOffset |
| `Ragdoll` class | `ragdoll.hpp #33` | bone별 RigidBody + Constraint 소유, PhysicsWorld 비소유 등록 |
| `Ragdoll::build()` | `ragdoll.cpp` | 스켈레톤 + def → body/joint 생성 + world 등록 |
| `Ragdoll::destroy()` | `ragdoll.cpp` | joint 먼저, body 나중 제거 (dangling ptr 방지) |
| `Ragdoll::syncFromPose()` | `ragdoll.cpp` | AnimFrame pose → body pos/orient (DFS) |
| `Ragdoll::seedFromFinalXforms()` | `ragdoll.cpp` | AnimBlender finalXformData → body pos/orient |
| `Ragdoll::syncToPose()` | `ragdoll.cpp` | body pos/orient → AnimFrame pose (DFS) |
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

## 4. 이벤트 시스템

**파일:** `client/event.hpp` / `client/event.cpp`

| 항목 | 위치 | 설명 |
|------|------|------|
| 풀 관리 (`gPool4`, `gPool16`) | `event.hpp #15-16` | 이벤트 메모리 풀 (4B / 16B) |
| `holdEvent` macro | `event.hpp #41-46` | 풀 할당 + placement new |
| `clearEvents` macro | `event.hpp #48-57` | 이벤트 리스트 전체 해제 |
| `EventList` alias | `event.hpp #62` | `std::list<char*>` |
| `EventType` enum | `event.hpp #67-73` | Hit, Blood, Death, Attack |
| `BasicEvent` struct | `event.hpp #79-81` | 공통 base (type 필드) |
| `EvHit` struct | `event.hpp #83-90` | targetId, hp |
| `EvBlood` struct | `event.hpp #91-96` | victimId |
| `EvDeath` struct | `event.hpp #97-102` | victimId |
| `EvAttack` struct | `event.hpp #103-108` | attackerId |
| `IEventBus` interface | `event.hpp #117-134` | `receive()` 순수 가상 |
| `NullEventBus` | `event.hpp #136-139` | 아무것도 안 하는 기본 버스 |

---

## 5. 애니메이션

**파일:** `client/animation.hpp` / `client/animation.cpp`

| 항목 | 위치 | 설명 |
|------|------|------|
| `AnimFrame` struct | `animation.hpp #10-15` | translation, rotation(NQuat), scale, time |
| `WeightedAnimFrame` struct | `animation.hpp #17-20` | frame + 가중치 w |
| `convertAnimFrameToMatrix()` | `animation.hpp #25` | AnimFrame → Mat4x4 |
| `lerpAnimFrames()` | `animation.hpp #30` | lerp(translation/scale) + slerp(rotation) |
| `sumWeightedAnimFrames()` | `animation.hpp #33` | 가중합 (nlerp) |
| `AnimClip` struct | `animation.hpp #42-54` | 키프레임, duration, skeletonEnum, flags |
| `loadAnimClipsFromFile()` | `animation.hpp #56` | 바이너리 → AnimClip 벡터 |
| `AnimBlender` class | `animation.hpp #84` | 추상 base; 상속 필수 |
| `AnimBlender::update()` | `animation.hpp #118` | priority_ 갱신 (오브젝트가 호출) |
| `AnimBlender::setCulled()/isCulled()` | `animation.hpp #112` | culled 플래그; viewFrustumCulled || hiZCulled_ 통합 값으로 동기화 — culled면 bone matrix 계산 및 Object::update 스킵 |
| `AnimBlender::onCalcLocal()` | `animation.hpp #123` | 로컬 변환 행렬 계산 (AnimSystem이 호출) |
| `AnimBlender::onCalcDress()` | `animation.hpp #126` | dress 공간으로 환원 |
| `AnimBlender::onCalcFinal()` | `animation.hpp #136` | toLocal 적용 → finalXformData |
| `AnimBlender::finalXformData()` | `animation.hpp #140-141` | 셰이더 입력용 최종 행렬 배열 |
| `AnimSystem` class | `animation.hpp #214` | 스케줄링 / 로드밸런싱 |
| `AnimSystem::update()` | `animation.cpp #300` | culled 파티셔닝 후 visible range만 timeSlice 기반 heap 처리 |

**오브젝트별 AnimBlender (object.hpp):**

| 클래스 | 위치 |
|--------|------|
| `AnimBlenderPlayer` | `object.hpp #13-56` |
| `AnimBlenderGoblin` | `object.hpp #58-98` |
| `AnimBlenderAnubis` | `object.hpp #100-140` |
| `AnimBlenderBat` | `object.hpp #142-182` |
| `AnimBlenderBomber` | `object.hpp #184-???` |
| `AnimBlenderDemon` 이하 | 순서대로 약 42줄 간격 |

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
| `Object::update()` | `object.cpp #408` | 방향벡터 갱신 후 viewFrustumCulled\|\|hiZCulled_ 이면 조기 반환; 아니면 RenderState 보간 + animBlender::update |
| `Object::render()` | `object.cpp #490` | viewFrustumCulled 체크 후 GFX DrawEvent 제출 (Hi-Z culled는 제출함, renderObjectId 포함) |
| `Object::setFrustumCulled()/isFrustumCulled()` | `object.hpp` | view frustum culling 결과 — DrawEvent 제출 차단 |
| `Object::setHiZCulled()/isHiZCulled()` | `object.hpp` | Hi-Z occlusion culling 결과 (1-frame delay) — update/anim 스킵 |
| `Object::setRenderObjectId()/renderObjectId()` | `object.hpp` | GPU→CPU Hi-Z 역매핑용 정수 쿠키 |
| `Object::body()` | `object.hpp` | 인라인 RigidBody 참조 (PhysicsWorld 등록 시 사용) |
| `Object::worldBVH()` | `object.hpp` | `body_.worldBVH()` 위임 (CombatSystem 호환) |
| `Object::rebuildBodyBVH()` | `object.cpp` | BVH 월드 공간 재빌드 (setPos/setOrient 시 호출) |
| `Object::setPos/setOrient` | `object.hpp` | body_ 위임 + rebuildBodyBVH() |
| `Object::hp()` / `setHp()` | `object.hpp` | HP 접근자 |

**구체 오브젝트 클래스:**

| 클래스 | 위치 |
|--------|------|
| `Cube` | `object.hpp #591` |
| `Player` | `object.hpp #600` |
| `Goblin` | `object.hpp #624` |
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
| `RWStructuredBuffer` class | `gfxUtil.hpp #275` | Default Heap + UAV — `bindCompute` / `bindGraphics` / `bindComputeAsSRV` / `uavBarrier` / `clearUint` / `gpuAddress` / `resource` 제공. opt-in readback: `initReadback` / `copyToReadback` / `readbackPtr<T>(roomIdx)` / `hasReadback` |
| `ConstantBufferArray` struct | `gfxUtil.hpp #356` | 큰 ConstantBuffer 여러 개를 단일 리소스에서 분할해 사용 |

**파일:** `client/gfx.hpp` / `client/gfx.cpp`

| 항목 | 위치 | 설명 |
|------|------|------|
| `GFX` class | `gfx.hpp #63` | DX12 렌더링 총괄 |
| `GFX::setupDXGI()` | `gfx.hpp #77` | DXGI Factory + Adapter 열거 |
| `GFX::init()` | `gfx.hpp #84` | Device, CmdQ, DescriptorHeap, PSO 생성 |
| `GFX::createSwapChain()` | `gfx.hpp #90` | SwapChain + BackBuffer + FrameFence |
| `GFX::addDrawEvent()` | `gfx.hpp #97-135` | 파이프라인별 오버로드 |
| `GFX::loadAssets()` | `gfx.hpp #152` | 요청된 리소스 로드 |
| `GFX::render()` | `gfx.hpp #155` | 전체 파이프라인 실행 |
| `GFX::getHiZObjectVisible()` | `gfx.cpp` | renderObjectId → Hi-Z visibility 조회 (1-frame delay; Hi-Z OFF면 true 반환) |
| `GFX::setMaxRenderObjectId()` | `gfx.cpp` | objectVisibility 배열 크기 초기화 (setupStage 이후 호출) |

**파이프라인 파일 목록:**

| 파이프라인 | 헤더 파일 | 용도 |
|-----------|----------|------|
| PBRPipeline | `pbrPipeline.hpp` | 정적 메시 PBR (Forward) |
| PBRSkinnedPipeline | `pbrSkinnedPipeline.hpp` | 스킨드 메시 PBR (Forward) |
| PBRDeferredPipeline | `pbrDeferredPipeline.hpp` / `pbrDeferredPipeline.cpp` | 정적 메시 Deferred Shading (Shadow + GBuffer + Lighting) |
| PBRDeferredSkinnedPipeline | `pbrDeferredSkinnedPipeline.hpp` / `pbrDeferredSkinnedPipeline.cpp` | 스킨드 메시 Deferred Shading (Shadow + GBuffer만; Lighting은 PBRDeferredPipeline 담당) |
| BVPipeline | `BVPipeline.hpp` | 바운딩 볼륨 디버그 |
| BillboardPipeline | `billboardPipeline.hpp` | 빌보드 |
| SkyboxPipeline | `skyboxPipeline.hpp` | 스카이박스 |
| UIPipeline | `uiPipeline.hpp` | UI 요소 |
| SamplePipeline | `samplePipeline.hpp` | 샘플 렌더 |
| TerrainPipeline | `terrainPipeline.hpp` / `terrainPipeline.cpp` | Height map 지형 렌더 (Forward path: shadowPass + mainPass) |
| TerrainDeferredPipeline | `terrainDeferredPipeline.hpp` / `terrainDeferredPipeline.cpp` | Height map 지형 렌더 (Deferred path: shadowPass + GBuffer pass) |

**Terrain 관련 파일:**

| 파일 | 설명 |
|------|------|
| `terrain.hpp` | `TerrainLayer`, `TerrainData` 구조체, `loadTerrainFromFiles()` 선언 |
| `terrain.cpp` | manifest/meta 파싱, height.raw → GPU 메시 생성, 텍스처 로드 |
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
4. calcSingleShadow(posV, posL) → PCF 9-tap 그림자 적용
5. globalAmbient 더하기 → Reinhard tonemapping → gamma correction

**Deferred Shading 관련 파일:**

| 파일 | 설명 |
|------|------|
| `pbrDeferredPipeline.hpp` / `.cpp` | PBRDeferredPipeline 네임스페이스 — Shadow + GBuffer + Lighting 패스 |
| `pbrDeferredSkinnedPipeline.hpp` / `.cpp` | PBRDeferredSkinnedPipeline 네임스페이스 — Shadow + GBuffer 패스 |
| `pbrDeferred.hlsl` | GBuffer Geometry Pass VS/PS (정적 메시) |
| `pbrDeferredSkinned.hlsl` | GBuffer Geometry Pass VS/PS (스킨드 메시) |
| `pbrDeferredLighting.hlsl` | Deferred Lighting Pass (fullscreen triangle, GBuffer SRV 읽기) |
| `sharedResources.hpp` / `.cpp` | `SharedResources::GBuffer` 네임스페이스 — GBuffer 텍스처 생성/관리 |

**GBuffer 레이아웃 (`sharedResources.hpp`):**

| 슬롯 | 포맷 | 내용 |
|------|------|------|
| GB0 | R8G8B8A8_UNORM | Albedo.rgb (linear) + AO.a |
| GB1 | R16G16_FLOAT | NormalV oct-encoded (view-space, 2채널 [0,1]) |
| GB2 | R8G8B8A8_UNORM | LightAccum.rgb (ambient+emissive 선계산) + Roughness.a |
| GB3 | R8_UNORM | Metallic |
| Depth | R32_TYPELESS (DSV=D32_FLOAT, SRV=R32_FLOAT) | Scene depth |

- GB1 클리어 값: `(0.5, 0.5, 0, 0)` → octDecode 시 정면 법선 (0, 0, 1)
- Normal Oct Encoding: `pbrLighting.hlsli`의 `octEncode()` / `octDecode()` 유틸리티 사용
- depth + invProj → posV, posV + invView → posW (Lighting 패스에서 위치 재구성)

**Deferred 렌더 패스 순서 (`gfx.cpp::render()`):**
1. GBuffer 클리어 (`clearGBuffer`)
2. Shadow Pass — PBRDeferredPipeline + PBRDeferredSkinnedPipeline + TerrainPipeline (CSM)
3. GBuffer Pass (정적) — MRT 4개(GB0~GB3) + DSV에 geometry 기록
4. **GBuffer Indirect Pass (스킨드)** — Hi-Z 5단계 compute(Clear→Cull→PrefixSum→Compact→Command) 후 indirect draw. Compact Pass 이후 visibleFlags → `visibilityReadback` 복사(1-frame delay). 동일 MRT + DSV.
5. GBuffer Pass (지형) — TerrainDeferredPipeline, 동일 MRT + DSV
6. GBuffer 상태 전환: RTV→SRV (`transitionToRead`)
7. Deferred Lighting Pass — fullscreen `DrawInstanced(3, 1, 0, 0)`, backbuffer에 출력
8. **GBuffer depth → backbuffer DSV 복사** (`copyResource`): Lighting pass와 같은 cmdList batch에서 실행. 이후 Forward 패스가 올바른 장면 깊이를 기준으로 렌더링할 수 있도록 GBuffer DSV 내용을 backbuffer depth buffer로 복사.
9. Forward-always 패스: Skybox, Terrain main, BV debug, Billboard (GBuffer 미사용)

**GFX RenderPath 선택 (`gfx.hpp`):**
- `enum class RenderPath { Forward, Deferred }`
- `GFX::setRenderPath(RenderPath)` — 런타임 전환
- `GFX::cycleGBufferDebugMode()` — 'G' 키로 GBuffer 채널 디버그 뷰 순환 (None→Albedo→Normal→AO→Roughness→Metallic→LightAccum→Depth)
- `gBufferDebugMode_` (uint, 0~7) — Lighting PSO의 `debugMode` cbuffer 필드로 전달

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

---

## 8-B. 파티클 시스템

**설계 원칙:** Unity Particle System 모듈 구조 — 공통 simulation core, `RendererModule.mode`로 렌더 백엔드 선택

| 파일 | 설명 |
|------|------|
| `particleModules.hpp` | `MainModule`, `EmissionModule`, `ShapeModule`, `VelocityOverLifetimeModule`, `ColorOverLifetimeModule`, `SizeOverLifetimeModule`, `RotationOverLifetimeModule`, `CustomDataModule`, `Material`, `RendererModule`, `TextureSheetAnimationModule`, `SubEmittersModule`, `ParticleSystemConfig` |
| `particleSystem.hpp` | `ParticleSystem`, `Particle`, `SubEmitterEvent` |
| `particleSystem.cpp` | `init()`, `emit()`, `emitAt()`, `startContinuous()`, `spawnParticle()`, `sampleShapeOrigin/Direction()`, `update()`, `render()` |
| `particleEffect.hpp` | `ParticleEffect` — Unity 프리팹 대응 그룹 컨테이너. `PlayMode::Emit` / `Continuous`. `SubEmitterBinding`, `PendingSubEmitterBurst` |
| `particleEffect.cpp` | `addSystem()`, `play()`, `stop()`, `isAlive()`, `update()`, `render()`, `bindSubEmitter()` |

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

**Sub Emitters (`particleEffect.hpp` / `particleModules.hpp`):**
- `SubEmittersModule` — `Event::Birth` / `Event::Death`, `emitProbability`, `inheritVelocity/Color/Size`
- `SubEmitterBinding` — parentIdx + subEmitterCfgIdx + childIdx 연결 레코드
- `PendingSubEmitterBurst` — 활성 burst-sequence 인스턴스; 시간 시뮬레이션으로 Unity burst 타이밍 재현
- `ParticleEffect::bindSubEmitter(parentIdx, cfgIdx, childIdx)` — 자식 시스템 등록; 이후 play()에서 자식 자동 재생 차단, update()에서 ParentEvent → PendingBurst 변환

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
- `Mesh` + `MatUnlit` — `MeshParticlePipeline::DrawEvent` 제출 (angularAngle + startRotation3D + translate)
- `Mesh` + `MatSwordSlash` — `SwordSlashPipeline::DrawEvent` 제출 (동일 transform 계산, 텍스처 4종 + FX 파라미터 포함)

**AnyMat / Material 타입** (`particleModules.hpp`):
- `using AnyMat = std::variant<MatUnlit, MatSwordSlash, MatSmokeBlendCG, MatTwoSides>` — per-shader 독립 구조체 + variant
- `MatUnlit` — BillboardPipeline / MeshParticlePipeline용: `mainTex`, `additive`
- `MatSwordSlash` — SwordSlashPipeline용: `mainTex`, `emissionTex`, `dissolveTex`, `flowTex`, 스크롤/Flow/디졸브/Emission FX 파라미터
- `MatSmokeBlendCG` — SmokeBlendCGPipeline용: `mainTex`, 스프라이트 시트 애니메이션
- `MatTwoSides` — TwoSidesPipeline용: `mainTex`, `maskTex`, `noiseTex`, `emission`, `backFresnel`, UV 타일링 3종
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
| `Game::spawnTestObject(int kind)` | `standalone/game.cpp` | kind 1~6 switch: 각 factory로 PhysicsTestObject 생성 후 activate |
| `Game::update()` | `standalone/game.hpp #45` | 메인 루프 (입력→이벤트→물리→오브젝트→애니메이션) |
| `Game::render()` | `standalone/game.hpp #46` | cullObjects → GFX → applyHiZCulling |
| `Game::cullObjects()` | `standalone/game.cpp #1350` | view frustum culling (plane-based) → setFrustumCulled |
| `Game::applyHiZCulling()` | `standalone/game.cpp` | Hi-Z readback → setHiZCulled + AnimBlender::setCulled (gfx_.render() 이후 호출) |
| `Game::processInput()` | `standalone/game.hpp #57` | 키보드/마우스 입력 처리 |
| `importNode()` 계열 | `standalone/game.hpp #68-80` | 씬 바이너리 파일 파싱 |
| `importTerrain()` | `standalone/game.hpp #80` | Terrain 노드 처리 — `TerrainObject`에 TerrainData 연결 |

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
| `Light::updateCSMCascades()` | `light.hpp #30` | CascadeConfig + ShadowMapConfig → Practical Split Scheme으로 cascade 계산 |
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
| `dustParticleSystem_` | 발 착지 흙먼지 빌보드 파티클 |
| `aoESlashGreenEffect_` | AoE 슬래시 그린 이펙트 (Circle2 + Slash, Billboard) |

**Camera::updateGFX() 등록 파이프라인 (`camera.cpp`):**
- PBRPipeline, PBRSkinnedPipeline, SkyboxPipeline, BVPipeline, BillboardPipeline, **TerrainPipeline**, MeshParticlePipeline, SmokeBlendCGPipeline, SwordSlashPipeline, **TwoSidesPipeline** CameraData 자기등록

**Camera Spring Arm 시스템 (`camera.hpp` / `camera.cpp`):**

| 항목 | 위치 | 설명 |
|------|------|------|
| `Camera::update(float dt)` | `camera.cpp #5` | Spring Arm 충돌 회피: queryCameraArm → fast-in/slow-out arm 길이 제어 |
| `Camera::setPhysicsWorld()` | `camera.hpp #31` | PhysicsWorld 연결 (queryCameraArm 호출 경로) |
| `Camera::currentArmLength_` | `camera.hpp #74` | 현재 arm 길이 (fast-in 즉시 단축 / slow-out dt 기반 복귀) |
| `Camera::armReturnRate_` | `camera.hpp #75` | slow-out 복귀 속도 (units/sec, 기본 3.f) |
| `Camera::cameraRadius_` | `camera.hpp #76` | BVH raycast spherePad (기본 0.2f) |

**AssetManager::loadGFXAssets (`AssetManager.hpp #9`):**
- `loadGFXAssets(GFX& gfx, const GFX::AssetConfigs& configs = {})` — configs를 `gfx.loadAssets(configs)`로 전달

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
   d. `applyHiZCulling()` — 이전 프레임 readback → setHiZCulled + AnimBlender::setCulled

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
| `UI::Image` | `Image.hpp/cpp` | 단일 텍스처 표시 |
| `UI::Label` | `Label.hpp/cpp` | `TextImage` 내부 소유; `resolvedRect_` 크기에 맞게 자동 재생성; dirty-check로 매 프레임 래스터화 방지 |
| `UI::Button` | `Button.hpp/cpp` | Normal/Hovered/Pressed 상태 텍스처; `onClick` 콜백 (`std::function<void()>`) |
| `UI::ProgressBar` | `ProgressBar.hpp/cpp` | 배경 + fill 이중 쿼드; `setProgress(0~1)` |
| `UI::Slider` | `Slider.hpp/cpp` | 트랙 + 핸들 드래그; `onValueChanged` 콜백 (`std::function<void(float)>`) |
| `UI::Dropdown` | `Dropdown.hpp/cpp` | 파란 헤더 버튼 + 확장 리스트; `setup(items)` 후 `onSelectionChanged` 콜백 (`std::function<void(int)>`) |

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

## 관련 문서

- `docs/graphicsArchitecture.md` — GFX 초기화 흐름, 파이프라인 구조
- `docs/physicsArchitecture.md` — PhysicSystem::step 단계, BVH 변환 체인
- `docs/gameArchitecture.md` — 게임 루프, 이벤트 시스템, CombatSystem 구조
- `CLAUDE.md` — 파일 인코딩, 빌드 방법, 아키텍처 문서 링크

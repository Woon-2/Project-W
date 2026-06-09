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
| `computeBoxInertia()` | `rigidBody.hpp #26` | 박스 관성 텐서 헬퍼 |
| `computeCapsuleInertia()` | `rigidBody.hpp #27` | 캡슐 관성 텐서 헬퍼 |
| `Constraint` (abstract) | `constraint.hpp #12` | prepare/solveVelocity/solvePosition 인터페이스 |
| `ContactPoint` struct | `collision.hpp` | worldPos, normal(B→A), depth, acc 누적값 |
| `ContactConstraint` class | `contactConstraint.hpp` | PGS Normal + Coulomb 마찰 impulse solver; setExternalAccels()로 외력 보상 |
| `ContactConstraint::setExternalAccels()` | `contactConstraint.hpp` | 외력 가속도 설정 (prepare() 전 호출); Baumgarte bias에 외력 보상항 추가 |
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
| `PhysicsWorld::registerTerrain()` | `physicsWorld.hpp` | **다중 지형**: Static body+heightField 등록 → `TerrainHandle` 반환. 여러 청크 동시 등록 가능(각 collider가 XZ footprint 밖 body 자체 reject) |
| `PhysicsWorld::unregisterTerrain(handle)` | `physicsWorld.hpp` | 핸들로 개별 청크 collider 해제 (slot tombstone 재사용) |
| `PhysicsWorld::terrains_` | `physicsWorld.hpp` | `vector<TerrainEntry{collider, hf}>` + `freeTerrainSlots_`; generateContacts/queryCameraArm가 전 청크 순회(XZ reject) |
| `PhysicsWorld::registerCameraObstacle()` | `physicsWorld.hpp #67` | body를 카메라 obstacle로 cameraBroadPhase_에 등록 |
| `PhysicsWorld::unregisterCameraObstacle()` | `physicsWorld.hpp #68` | 카메라 obstacle 등록 해제 |
| `PhysicsWorld::queryCameraArm()` | `physicsWorld.hpp #73` | pivot→desiredEye arm 허용 길이 반환 (지형 N=6 샘플 + BVH raycast) |
| `PhysicsWorld::cameraBroadPhase_` | `physicsWorld.hpp #140` | 카메라 전용 SAPBroadPhase 인스턴스 (일반 physicsWorld broadPhase와 독립) |
| `PhysicsWorld::step()` | `physicsWorld.hpp #63` | (substep) integrate → generateContacts → solveConstraints → applyPseudoVelocity → resolveStaticPenetration + moved body BVH 재빌드 |
| `PhysicsWorld::staticContacts_` / `movedByStaticDepen_` | `physicsWorld.hpp` | step별 static 충돌 레코드 + depenetration으로 직접 이동된 body dirty set |
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
| `updateParticleHitboxSources()` | `skillSystem.cpp` | VFXParticle: 핸들 재사용으로 파티클 수만큼 증감, `targetMask` 전파 |
| `Faction` enum / `hostileMask()` | `object.hpp` | 피아 식별: Neutral/Players/Monsters; 히트박스 targetMask = hostileMask(owner.faction) |
| `Object::faction()`/`setFaction()` | `object.hpp` | 진영 접근자(생성 지점에서 setFaction 호출) |

> 서버 전용 차이(damageCoeff, ServerAnimController 변환)는 `RoomServer/skill/skillSystem.*` 및 서버 설계 문서 참조.

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
| `EvHit` struct | `event.hpp #88-95` | targetId, hp |
| `EvBlood` struct | `event.hpp #96-101` | victimId |
| `EvDeath` struct | `event.hpp #102-107` | victimId |
| `EvAttack` struct | `event.hpp #108-113` | attackerId |
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
| `AnimBlenderPlayer` | `object.hpp #15-62` (애니메이션 트리거는 `EventBus::receive`에서; trigger* 함수 제거됨) |
| `AnimBlenderGoblin` | `object.hpp #64-108` (애니메이션 트리거는 `EventBus::receive`에서; trigger* 함수 제거됨) |
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
| `Object::worldCullBounds()` | `object.cpp` | Hi-Z cull용 월드 AABB = worldBVH 본 부착 노드 합집합(+15% 마진), 포즈/랙돌 추종. 비스킨이면 nullopt |
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
| `RWStructuredBuffer` class | `gfxUtil.hpp #283` | Default Heap + UAV — `bindCompute` / `bindGraphics` / `bindComputeAsSRV` / `uavBarrier` / `clearUint` / `gpuAddress` / `resource` 제공. opt-in readback: `initReadback` / `copyToReadback` / `readbackPtr<T>(roomIdx)` / `hasReadback`. **offset 오버로드**: `bindCompute(...,byteOffset)` / `copyToReadback(...,dstByteOffset,srcByteOffset)` / `readbackPtr<T>(roomIdx,byteOffset)` — 단일 리소스 내 다중 슬롯(Hi-Z visibility 2-slot ring) 표현용 |
| `ConstantBufferArray` struct | `gfxUtil.hpp #356` | 큰 ConstantBuffer 여러 개를 단일 리소스에서 분할해 사용 |

**파일:** `client/gfx.hpp` / `client/gfx.cpp`

| 항목 | 위치 | 설명 |
|------|------|------|
| `GFX` class | `gfx.hpp #63` | DX12 렌더링 총괄 |
| `GFX::setupDXGI()` | `gfx.hpp #77` | DXGI Factory + Adapter 열거 |
| `GFX::init()` | `gfx.hpp #84` | Device, CmdQ, DescriptorHeap, PSO 생성 |
| `GFX::createSwapChain()` | `gfx.hpp #90` | SwapChain + BackBuffer + FrameFence |
| `GFX::addDrawEvent()` | `gfx.hpp #97-135` | 파이프라인별 오버로드 |
| `GFX::initSharedResources()` | `gfx.hpp` | 공용 GPU 리소스(그림자맵/GBuffer/HiZ/정적 메시/white tex) 생성. 실행 시 메인 스레드 1회 |
| `GFX::loadRequestedAssets()` | `gfx.hpp` | 요청된 리소스(모델/텍스처/메시 등) 로드. ThreadPool 워커에서 백그라운드 호출 가능 |
| `GFX::loadAssets()` | `gfx.hpp` | initSharedResources + loadRequestedAssets 편의 래퍼 |
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

> **Chunk 스트리밍 전환:** 단일 terrain → 다중 Chunk 스트리밍. 설계 문서 `docs/terrainChunkStreaming.md`.

| 파일 | 설명 |
|------|------|
| `terrain.hpp` | `TerrainLayer`/`TerrainData`(+`chunkCol/Row`)/`TerrainLayerPalette`/`ChunkIndex(Entry)`/`ChunkCpuBuild` 구조체, chunk streaming 함수 선언 |
| `terrain.cpp` | `genChunkGeometryCpu`(CPU, 워커 스레드 안전)/`assembleChunkMeshGpu`(메인), `parseChunkIndex`/`loadLayerPalette`/`buildChunkCpu`/`finalizeChunkGpu`, `TerrainHeightField` 메서드 |
| `terrainChunkManager.hpp` / `.cpp` | `TerrainChunkManager` — 팔레트/인덱스 소유, hop≤3 BFS 스트리밍(load/unload+grace), 워커 CPU build + 메인 GPU finalize, `heightAtWorld`/`normalAtWorld`/`chunkCoordAtWorld`/`submitDrawEvents`/`worldCenter`(인덱스 청크 평균 월드 좌표, 로비 카메라 포커스) |
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
| `sharedResources.hpp` / `.cpp` | `SharedResources::Portrait` 네임스페이스 — 로비 슬롯 캐릭터용 오프스크린 포트레이트 RT(가로 아틀라스, room별 triple-buffer). `addPortraitRT`/`transitionToWrite`/`transitionToRead`/`clearPortraitRT`. GFX 채널: `addLobbyPortraitDrawEvent`/`setLobbyPortraitCamera`/`addLobbyPortraitLightData`/`setLobbyPortraitActive`/`lobbyPortraitTextureForThisFrame`/`lobbyPortraitCellUvScaleBias`. 제출: `Object::renderPortrait(gfx, slot)`. render() 삽입: deferred lighting 이후 → UI 이전. 상세: `docs/lobbyScene.md` 작업 B-3 |

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

**TrailPipeline:**

Unity ParticleSystem Trails 모듈 (Mode=Particles). RendererModule과 독립된 overlay 레이어로 동작 — 파티클 본체 렌더링과 공존 가능.

| 파일 | 설명 |
|------|------|
| `trailPipeline.hpp` | DrawEvent (`std::vector<TrailVertexCPU>` + per-trail constants), Resources (system-wide perInstanceData pool + per-drawcall PDD), Dispatcher (alpha/additive 2 PSO) |
| `trailPipeline.cpp` | updateGPUDataSingleThreaded: 모든 trail vertex를 한 StructuredBuffer에 패킹 + trailStartOffsets 기록. drawSingleThreaded: VB/IB 없이 `DrawInstanced((N-1)*6, 1, 0, 0)` |
| `trail.hlsl` | VS expansion via `SV_VertexID` — kStripOffsets/kSides 룩업 테이블로 segment 당 6 vertex로 quad strip 생성. 중앙 차분 tangent × cameraDir 외적으로 side 벡터 산출. UV: `Stretch`(1-segmentT) / `Tile`(cumulativeDist/tileLength). PS: bindless sample × baseColor × (1-age/lifetime) |

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
| `particleSystem.hpp` | `ParticleSystem`, `Particle` (`trail` ring buffer 포함, kMaxTrailSegments=32), `TrailPoint`, `SubEmitterEvent` |
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
| `Game::render()` | `standalone/game.hpp #46` | cullObjects → GFX → applyHiZCulling |
| `Game::cullObjects()` | `standalone/game.cpp #1350` | view frustum culling (plane-based) → setFrustumCulled |
| `Game::applyHiZCulling()` | `standalone/game.cpp` | Hi-Z readback → setHiZCulled + AnimBlender::setCulled (gfx_.render() 이후 호출) |
| `Game::processInput()` | `standalone/game.hpp #57` | 키보드/마우스 입력 처리 |
| `importNode()` 계열 | `standalone/game.hpp #68-80` | 씬 바이너리 파일 파싱 |
| `importTerrain()` | `standalone/game.hpp #80` | Terrain 노드 처리 — `TerrainObject`에 TerrainData 연결 |

**Online::Game 전용 (`online/onlineGame.hpp` / `online/onlineGame.cpp`):**

| 항목 | 위치 | 설명 |
|------|------|------|
| `Game::resolvePlayerSeparation()` | `onlineGame.cpp` (`removePlayer` 직후) | 플레이어 간 reciprocal soft separation. 매 물리 step 후 호출. 로컬 플레이어를 XZ 침투량의 절반만큼 `setCurrPos`로 밀어냄. Faction `Players` 게이팅, `getId` 결정론적 tie-break, 적용 시 `moveChange_=true`. 상수: `kPlayerSeparationRadius`/`kMaxSeparationSpeed`/`kSeparationStiffness`, 충돌 레이어 `kLayerPlayer`/`kPlayerCollisionMask` (파일 상단) |
| `Game::lobbyCreateRoom/JoinRoom/LeaveRoom/StartGame()` | `onlineGame.cpp` (UI 버튼 콜백) | LobbyServer로 요청 패킷 전송(C_CreateRoom/C_JoinRoom/C_LeaveRoom/C_GameStart). 상태 변경은 응답 핸들러에서 수행 |
| `Game::onLobbyCreated/onLobbyJoined/onLobbyPlayerJoined/onLobbyPlayerLeft/onGameStart()` | `onlineGame.cpp` (lobby 액션 직후, public) | LobbyServer 응답 처리. `PacketManager`가 `LobbyScene`의 `SleepEx(1,true)` alertable 대기에서 호출. 룸 상태/슬롯/호스트 갱신 후 `refreshLobbyUI()`. `onGameStart`는 현재 로그만(RoomServer 핸드오프 후속) |
| `Game::createStronghold/onStrongholdState/applyHit` | `onlineGame.cpp` | 거점은 `Stronghold`(Object+EventBus, AnimBlender 없음; `object.hpp`) 클래스. enter의 hp/maxHp로 생성. `applyHit`은 EvHit/EvDeath 발행만(거점/고블린 공통), 디스패치 루프 `resolveObject`가 `strongholdHpBars_`로 거점 해소 → 데미지 넘버 생성. `onStrongholdState`: 파괴(state=1)는 setHp 없이 EvDeath, 재건(state=0)은 setHp(full)+EvRespawn. 파괴상태=`isDead()`. 상세: `RoomServer/docs/strongholdSystem.md` §10 |
| `Game::lobbyDisplayName(uint16)` | `onlineGame.cpp` | sessionId → 표시 이름(본인 `myId_`=`"나"`, 그 외 `"Player_<id>"`) |
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
| `piercingEffect_` | Piercing 슬래시 — `SM_VFX_Projectile_02` 메시 + PiercingMeshPipeline (MatPiercing), `PS_VFX_Piercing_ParticleSystems.json` / `M_VFX_Piercing_Fire.json` |
| `piercingSlashEffect_` | PiercingSlash — `SM_VFX_Slash_01_HD` 메시 + PiercingSlashMeshPipeline (MatPiercingSlash), `PS_VFX_Slash_ParticleSystems.json` / `M_VFX_Slash_Fire.json` |
| `dustParticleSystem_` | 발 착지 흙먼지 빌보드 파티클 |
| `aoESlashGreenEffect_` | AoE 슬래시 그린 이펙트 (Circle2 + Slash, Billboard) |
| `energyExplosionArrowEffect_` | 에너지 발사체 복합 이펙트. 4 시스템: [0] Arrow StretchedBillboard (`Arrow_ParticleSystems.json` → `EnergyExplosionArrowTex`), [1] Charge (8x6 Additive, sub-emitter on Arrow Birth), [2] Hit (8x6 Additive, sub-emitter on Arrow Death), [3] HitWhiteBG (8x6 Multiply, sub-emitter on Arrow Death) |
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
| `Online::LobbyUI` | `online/lobbyUI.hpp/cpp` | 로비 2D UI 레이어: 메인메뉴 + 스쿼드 스테이지(대기실) + 로딩 오버레이 + 로비 텍스처 소유. 위젯은 `uiManager_` 트리 소유(비소유 포인터). `loadTextures(gfx)` / `build(uiManager, Callbacks)` / `refresh(ViewState)` / `updateLoading(dt, visible, progress01)`. 버튼 액션은 `Callbacks`(create/join/leave/start/copy/openSettings/quit)로 Game에 라우팅. 접근자: `slotBay(i)`(포트레이트 합성), `setRootVisible/setLoadingVisible/setFlatBackgroundVisible/setMainMenuMessage/clearRoomCodeInput/hideAllSlotBays`, `panelTexture()/secondaryButtonTexture()` |
| `GameSettings` | `ui/settingsPanel.hpp` | 게임플레이용 영속 설정 값 구조체(fullscreen/allyDamageVisible/resolutionIndex/monsterDamageOpacity). `Game`이 소유(`settings_`), 로비·인게임·게임플레이가 공유 |
| `UI::SettingsPanel` | `ui/settingsPanel.hpp/cpp` | 씬 비종속 설정창. `uiManager_.root()` 직속(zOrder 50)에 빌드, `open()/close()/toggle()/isOpen()`로 토글 → 로비/인게임(ESC) 공용. `build(uiManager, panelTex, buttonTex, GameSettings&)`, `refreshPreview()`. 값 편집은 `GameSettings&`로 write-through |
| `Game` 통합 | `online/onlineGame.cpp` | `enterLobby`: `lobbyUI_.loadTextures/build` + `settingsPanel_.build`. `refreshLobbyUI()`는 씬/세션 상태로 `LobbyUI::ViewState` 스냅샷을 만들어 `lobbyUI_.refresh()`에 위임(+메인메뉴 이탈 시 `settingsPanel_.close()`). `makeLobbyCallbacks()`가 버튼 액션을 `lobbyCreateRoom` 등에 연결. `LobbyScene`/`renderWaitingRoom`/`enterInGame`/`lobbyLeaveRoom`은 컴포넌트 메서드 호출 |
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
| `Editor::CharacterKind` / `CharacterDef` / `kCharacterSkillMap` | `editor/characterSkillMap.hpp` | 전역 캐릭터→스킬 매핑 상수 (Player 18스킬/Goblin) |
| `Editor::SkillDraft` | `editor/skillDraft.hpp/.cpp` | 컴파일 에셋의 original/draft 사본 + 편집 필드 목록 + diff 콘솔 덤프 |
| `SkillDraft::Field` / `FieldType` | `editor/skillDraft.hpp` | 편집 가능한 스칼라 필드(center/half/euler/onHit/time/duration) |
| `SkillDraft::load/buildFields/applyDelta/dumpDiff` | `editor/skillDraft.cpp` | 로드/필드구성/넛지/가이드 출력 |
| `Editor::Controller` | `editor/editorController.hpp/.cpp` | 드롭다운 2개, 히트박스 피킹, nudge 편집, slow-mo/pause, free-fly 카메라 |
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
| `SkillEventPayload::PlayVFX` (localEulerDeg/advanceForwardLocal/flags) | `skillTypes.hpp` | VFX 배치+방향 오프셋+진행방향+yawOnly; lua orient/advance/groundLock 키 |
| PlayVFX 디스패치 (aim=rotateRPYH×baseRot, yawOnly, 2/4-인자 play) | `skillSystem.cpp` | `dispatchEvent` PlayVFX case |
| PlayVFX 컴파일 (orient/advance/groundLock 파싱) | `skillCompiler.cpp` | `tableToAsset` PlayVFX case |
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
바인딩(`standalone/game.cpp`, 0=hit/blood 예약 nullptr, 1~18=각 Effect). 스킬명은
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
| `AttachType::Ground` + `AttachTarget::groundAlign` | `skill/skillTypes.hpp` | 지면 고정 히트박스 attach |
| `SkillInstance::CastAnchor` | `skill/skillSystem.hpp` | 시전자 pos+yaw(시전 시점), Ground 히트박스 앵커 |
| `SkillDispatchContext::ground` | `skill/skillSystem.hpp` | `const GroundSampler*` 주입 |
| `alignQuatYToNormal`/`captureCastAnchor` | `skill/skillSystem.cpp` | 정렬 쿼터니언 / 앵커 캡처 |
| PlayVFX 지면 스냅 dispatch | `skill/skillSystem.cpp` `dispatchEvent` PlayVFX | worldPos.y 스냅 + `fx->setGroundSampler` |
| SpawnHitbox Ground 브랜치 | `skill/skillSystem.cpp` `dispatchEvent` SpawnHitbox | castAnchor+yaw 회전 후 OBB별 지면 스냅 |
| lua `groundSnap/groundAlign`, `particleCollision/particleConform`, `{type="Ground", align=}` | `skill/skillCompiler.cpp` | 플래그/파티클 모드/Ground attach 파싱 |
| PlayVFX 파티클 거동 디코드+적용 | `skill/skillSystem.cpp` PlayVFX | flags 비트3-6 → `fx->setGroundBehavior` |
| `groundSampler_` 바인딩 | `standalone/game.cpp`, `online/onlineGame.cpp` | chunkManager_→skillCtx_.ground |

> 서버 미러: `RoomServer/skill/{skillTypes,skillSystem,skillCompiler}.*`, `Room::bindGroundQueries`.
> 레거시 제거: `onlineGame.cpp`의 `SwordEffect::ArrowRain/RedEnergyExplosion` 하드코딩 지면 스냅 경로 삭제.

---

## 관련 문서

- `docs/terrainInteractingSkills.md` — 지면 연계 스킬/파티클 설계
- `docs/skillEditor.md` — standalone 스킬 에디터 설계
- `docs/graphicsArchitecture.md` — GFX 초기화 흐름, 파이프라인 구조
- `docs/physicsArchitecture.md` — PhysicSystem::step 단계, BVH 변환 체인
- `docs/gameArchitecture.md` — 게임 루프, 이벤트 시스템, CombatSystem 구조
- `CLAUDE.md` — 파일 인코딩, 빌드 방법, 아키텍처 문서 링크

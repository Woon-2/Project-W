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
| `collides(OBB, OBB)` | `collision.hpp #55` | 15축 SAT |
| `collides(BVH, BVH)` | `collision.hpp #56` | BVH-BVH 재귀 교차 (leaf-leaf 쌍에서만 정밀 판정; 내부 노드는 bounds AABB fast-reject만 사용) |
| `collides(BVH, AABB)` | `collision.hpp #57` | BVH vs 공격 hitbox |
| `RayHit` / `RaycastAABB` | `collision.hpp #62-69` | 레이-AABB 교차 |
| `buildAttackAABB` | `collision.hpp #71` | pos + forward + halfExtent + offsetFwd → AABB |

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
| `ContactConstraint` class | `contactConstraint.hpp` | PGS Normal + Coulomb 마찰 impulse solver |
| `BodyPair` struct | `broadPhase.hpp` | broad phase 결과 쌍 |
| `BroadPhase` (abstract) | `broadPhase.hpp` | add/remove/update/queryPairs 인터페이스 |
| `BruteForceBroadPhase` | `broadPhase.hpp` | O(n²) 참조 구현 (후보 비교용으로 보존) |
| `SAPBroadPhase` | `broadPhase.hpp` | X축 Sort-and-Sweep, O(n log n) (기본 사용) |
| `TerrainHeightField` struct | `terrain.hpp` | CPU-side 높이 데이터 (getHeightAt, getNormalAt) |
| `TerrainCollider` class | `collision.hpp` | Dynamic body ↔ 지형 높이맵 contact 생성 |
| `PhysicsWorld` class | `physicsWorld.hpp` | 시뮬레이션 진입점 |
| `PhysicsWorld::registerBody()` | `physicsWorld.hpp #37` | body + onRebuildBVH 콜백 + collisionGroup/Mask + broad phase 등록 |
| `PhysicsWorld::unregisterBody()` | `physicsWorld.hpp #43` | 등록 해제 |
| `PhysicsWorld::addJointConstraint()` | `physicsWorld.hpp #47` | 소유권 이전 joint 등록 |
| `PhysicsWorld::removeJointConstraint()` | `physicsWorld.hpp #48` | 소유 joint 제거 |
| `PhysicsWorld::addJointRef()` | `physicsWorld.hpp #52` | 비소유 joint ref 등록 (Ragdoll용) |
| `PhysicsWorld::removeJointRef()` | `physicsWorld.hpp #53` | 비소유 joint ref 제거 |
| `PhysicsWorld::registerTerrain()` | `physicsWorld.hpp #57` | Static 지형 body + heightField 등록 |
| `PhysicsWorld::unregisterTerrain()` | `physicsWorld.hpp #60` | 지형 collider 해제 |
| `PhysicsWorld::step()` | `physicsWorld.hpp #63` | integrate → generateContacts → solveConstraints |
| `PhysicsWorld::setGravity()` | `physicsWorld.hpp #67` | Dynamic body 중력 설정 |
| `PhysicsWorld::setSolverIterations()` | `physicsWorld.hpp #70` | PGS 반복 횟수 (기본 10, ragdoll 활성 시 20) |
| `PhysicsWorld::interpolatePos()` | `physicsWorld.hpp #73` | 렌더 보간 헬퍼 (prev→curr, t) |
| `PhysicsWorld::interpolateOrient()` | `physicsWorld.hpp #74` | 렌더 보간 헬퍼 (slerp) |
| `BallSocketJoint` class | `jointConstraint.hpp #16` | 3 translational DOF 제거, bilateral warmstart |
| `HingeJoint` class | `jointConstraint.hpp #50` | 1 rotational DOF, angle limits, refOrient |
| `ConeTwistJoint` class | `jointConstraint.hpp #103` | swing cone + twist limit, T-pose refOrient |
| `JointType` enum | `ragdollDef.hpp #8` | BallSocket / Hinge / ConeTwist |
| `BoneCapsuleDef` struct | `ragdollDef.hpp #11` | boneName, radius, halfHeight, mass, capsuleOffset |
| `JointDef` struct | `ragdollDef.hpp #20` | parentBoneName, childBoneName, type, limits |
| `RagdollDef` struct | `ragdollDef.hpp #34` | span<BoneCapsuleDef> + span<JointDef> |
| `getHumanoidRagdollDef()` | `ragdollDef.hpp #41` | Unity Humanoid 뼈대 정의 (static 싱글턴) |
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
| `AnimBlender` class | `animation.hpp #84-193` | 추상 base; 상속 필수 |
| `AnimBlender::update()` | `animation.hpp #116` | priority_ 갱신 (오브젝트가 호출) |
| `AnimBlender::onCalcLocal()` | `animation.hpp #121` | 로컬 변환 행렬 계산 (AnimSystem이 호출) |
| `AnimBlender::onCalcDress()` | `animation.hpp #124` | dress 공간으로 환원 |
| `AnimBlender::onCalcFinal()` | `animation.hpp #134` | toLocal 적용 → finalXformData |
| `AnimBlender::finalXformData()` | `animation.hpp #138-139` | 셰이더 입력용 최종 행렬 배열 |
| `AnimSystem` class | `animation.hpp #197-228` | 스케줄링 / 로드밸런싱 |
| `AnimSystem::update()` | `animation.hpp #216` | timeSlice 기반 업데이트 |

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
- TerrainPipeline: Position(0), Normal(1), Tangent(2), Bitangent(3), UV(4)

**스킨드 메시 판별 조건:** `mesh.vbIdxMap.contains(mesh.name + "_VB_BoneIndices") && animBlender`

---

## 7. 게임 오브젝트 (Object 계층)

**파일:** `client/object.hpp` / `client/object.cpp`

| 항목 | 위치 | 설명 |
|------|------|------|
| `RenderState` struct | `object.hpp` | world, pos, orient, scale, worldBVs, animBlender, pModel |
| `Equipment` struct | `object.hpp` | socketType + Object (장비 소켓) |
| `Object` class | `object.hpp` | 모든 게임 오브젝트의 base |
| `Object::update()` | `object.hpp` | RenderState 갱신 (물리 보간: PhysicsWorld::interpolatePos/Orient) |
| `Object::render()` | `object.hpp` | GFX DrawEvent 제출 (파이프라인 선택) |
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

**파이프라인 파일 목록:**

| 파이프라인 | 헤더 파일 | 용도 |
|-----------|----------|------|
| PBRPipeline | `pbrPipeline.hpp` | 정적 메시 PBR |
| PBRSkinnedPipeline | `pbrSkinnedPipeline.hpp` | 스킨드 메시 PBR |
| BVPipeline | `BVPipeline.hpp` | 바운딩 볼륨 디버그 |
| BillboardPipeline | `billboardPipeline.hpp` | 빌보드 |
| SkyboxPipeline | `skyboxPipeline.hpp` | 스카이박스 |
| UIPipeline | `uiPipeline.hpp` | UI 요소 |
| SamplePipeline | `samplePipeline.hpp` | 샘플 렌더 |
| TerrainPipeline | `terrainPipeline.hpp` / `terrainPipeline.cpp` | Height map 지형 렌더 (Splat 블렌딩 + PBR BRDF) |

**Terrain 관련 파일:**

| 파일 | 설명 |
|------|------|
| `terrain.hpp` | `TerrainLayer`, `TerrainData` 구조체, `loadTerrainFromFiles()` 선언 |
| `terrain.cpp` | manifest/meta 파싱, height.raw → GPU 메시 생성, 텍스처 로드 |
| `terrain.hlsl` | Terrain VS/PS (Splat map 레이어 블렌딩 + PBR Cook-Torrance BRDF + PCF Shadow) |
| `terrainPipeline.hpp` | `TerrainPipeline` 네임스페이스 (DrawEvent, Resources, Dispatcher) |
| `terrainPipeline.cpp` | `Dispatcher::mainPass()` 구현 |
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

---

## 9. 게임 루프

**파일:** `client/standalone/game.hpp` / `client/standalone/game.cpp`

| 항목 | 위치 | 설명 |
|------|------|------|
| `StandAlone::Game` class | `standalone/game.hpp #28` | IGame 구현 |
| `Game::setupStage()` | `standalone/game.hpp #37` | 씬 오브젝트 생성 + CombatSystem 등록 |
| `Game::update()` | `standalone/game.hpp #45` | 메인 루프 (입력→이벤트→물리→오브젝트→애니메이션) |
| `Game::render()` | `standalone/game.hpp #46` | GFX 렌더 호출 |
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
| `physicUpdateInterval` | `Seconds` | `1s/60f` 고정 타임스텝 |
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

**Camera::updateGFX() 등록 파이프라인 (`camera.cpp`):**
- PBRPipeline, PBRSkinnedPipeline, SkyboxPipeline, BVPipeline, BillboardPipeline, **TerrainPipeline** CameraData 자기등록

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
5. 각 `Object::update()` (물리 보간, RenderState 갱신)
6. `animSystem_.update()` (timeSlice 기반 스케줄링)
7. `Game::render()` → `gfx_.render()`

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

## 관련 문서

- `docs/graphicsArchitecture.md` — GFX 초기화 흐름, 파이프라인 구조
- `docs/physicsArchitecture.md` — PhysicSystem::step 단계, BVH 변환 체인
- `docs/gameArchitecture.md` — 게임 루프, 이벤트 시스템, CombatSystem 구조
- `CLAUDE.md` — 파일 인코딩, 빌드 방법, 아키텍처 문서 링크

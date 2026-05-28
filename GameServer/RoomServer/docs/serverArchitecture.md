# RoomServer 아키텍처

## 개요

`Room`은 게임 룸의 중심 클래스로, 플레이어 세션·오브젝트·물리 상태를 보유한다. 모든 상태 변경은 `JobQueue`를 통해 처리된다.

주요 파일: `Room.hpp/.cpp`, `roomServerMain.cpp`

---

## Room 업데이트 루프 (`Room::update`)

```
1. physicsWorld_.step()       — 물리 적분 + 접촉 생성 + 제약 풀기
2. updateGoblinAI(dt)         — NPC 상태 머신 실행, AnimController 클립 전환
3. updatePlayerAnimations(dt) — 플레이어 행동 이벤트 기반 AnimController 갱신
4. updateSkillSystem(dt)      — 히트박스 생명주기, 충돌 판정, 피격 처리
```

**프레임 순서 주의사항:**
- `physicsWorld_.step()` 내 `integrate()`에서 VelocityMotor가 실행되어 AI가 선언한 `desiredVel` 방향으로 수렴 impulse를 적용한다.
- AI는 `setLinearVel()` 대신 `setDesiredVel()`를 호출하므로 실제 속도를 덮어쓰지 않는다. knockback impulse는 motor가 여러 프레임에 걸쳐 점진적으로 보정한다.

---

## 물리 시스템

파일: `physicsWorld.hpp/.cpp`, `rigidBody.hpp/.cpp`, `collision.hpp/.cpp`, `broadPhase.hpp/.cpp`, `contactConstraint.hpp/.cpp`, `terrain.hpp/.cpp`

| 구성 요소 | 역할 |
|-----------|------|
| `PhysicsWorld` | `step(dt)` = 적분 → 접촉 생성 → 순차 충격량 풀이(PGS) |
| `RigidBody` | `MotionType`: Dynamic / Kinematic / Static |
| `SAPBroadPhase` | Sweep and Prune 브로드 페이즈 |
| `ContactConstraint` | Baumgarte bias + Coulomb 마찰 |
| `TerrainCollider` | 높이 필드 기반 지형 충돌 |
| `BVH` / `BVHNode` | 충돌 볼륨(리프는 AABB 또는 OBB); 뼈 인덱스와 `damageCoeff` 보유 |

**엔티티별 MotionType:**
- 플레이어(서버): Kinematic — 클라이언트 위치 패킷을 직접 적용
- 고블린: Dynamic — AI가 `setLinearVel()` 로 이동 제어, 물리는 중력·충돌 담당
- 지형: Static (`TerrainCollider` 별도)

**래그돌(Ragdoll) — 서버에 없음:**
서버에는 `Ragdoll` 클래스가 존재하지 않으며 앞으로도 추가 예정이 없다. 고블린 사망 시 서버에서는 `Dead` 상태 전환과 물리 비활성화만 처리한다. 래그돌 바디 충돌은 클라이언트가 자체 물리로 처리하며 서버가 인지할 필요가 없다. 관련 시각적 결과(래그돌 포즈 등)는 클라이언트-only 연산이다.

상세 물리 설계(솔버 규약, CFM, split impulse 등)는 클라이언트 측 `client/docs/physicsArchitecture.md` 참조.

---

## 서버 애니메이션 시스템

파일: `serverAnimation.hpp/.cpp`, `AssetManager.hpp/.cpp`

스킬 히트 판정 및 충격량 피드백의 정확도를 높이기 위해 서버가 독립적으로 스켈레톤 애니메이션을 시뮬레이션한다. 클라이언트와 동일한 `.anim` 파일(30 fps baked samples)을 공유한다.

### 핵심 구조체

```
ServerAnimClip
  .bakedSamples[boneIdx][sampleIdx]
      = toLocal * poseTransform  (GPU skinning matrix)
  poseTransform 복원: toDress * bakedSamples[boneIdx][sampleIdx]

ServerAnimState
  .advance(dt)
  .computeBoneModelXforms(skeleton, outXforms)
      outXforms[i] = skeleton.bones[i].toDress * bakedSamples[i][sampleIdx]

AnimController  (ServerAnimState 확장)
  .registerClip(key, clip)
  .switchClip(key, startTime)
```

`Object` 베이스 클래스가 `AnimController animController_`를 보유하므로 플레이어·고블린 모두 동일한 인터페이스를 사용한다.

### 에셋 로딩

`AssetManager::loadAssets()`에서 캐릭터별 명시적 멤버로 관리한다 (map/registry 일반화 금지).

```
playerAnimations_  ← ../resources/animations/player/*.anim
goblinAnimations_  ← ../resources/animations/goblin/*.anim
```

### 변환 규약 (DirectXMath row-major)

```
A→B→C 변환 = A * B * C (행 벡터 규약)
bone model-space = toDress * bakedSamples[i][sampleIdx]   // toDress 먼저
bone world-space = boneModelXform * entityWorldMatrix      // 모델→월드 순서
```

### 적용 위치

- **스킬 시스템**: `SkillSystem::computeAttachTransform()` 에서 뼈 월드 변환으로 히트박스 OBB 배치
- **충돌 판정**: `BVHNode::boneIdx`로 대상 BVH 리프를 해당 뼈의 월드 변환으로 이동 후 OBB vs OBB 정밀 검사
- **물리 피드백**: `Npc::onHitImpulse()` 콜백 — VelocityMotor가 넉백 수렴을 처리하므로 타이머 불필요; 히트 반응 애니메이션 트리거 추가 시 여기에 작성

---

## 스킬 시스템

상세 설계 문서: `docs/skillArchitecture.md`

파일: `skill/skillSystem.hpp/.cpp`, `skill/skillTypes.hpp`, `skill/skillCompiler.hpp/.cpp`

클라이언트 스킬 시스템과 구조가 동일하되 렌더링·VFX 관련 요소가 제거된 서버 권위 버전이다. 히트 판정·데미지·충격량 모두 서버에서 처리하며 결과를 `S_SkillHit`로 브로드캐스트한다.

---

## NPC / 고블린 AI

파일: `goblin.hpp/.cpp`, `Npc.hpp/.cpp`, `NpcGroup.hpp/.cpp`

`Goblin`은 `Npc`를 상속하며, `Npc`는 `Object`를 상속한다.

### 상태 머신

| 상태 | 전환 조건 |
|------|-----------|
| `Idle` | 탐지 범위 내 플레이어 발견 → Chase |
| `Chase` | 공격 범위 진입 → AttackWindup; 활동 구역 이탈 → Return |
| `AttackWindup` | 준비 시간 경과 → AttackRecover (공격 실행) |
| `AttackRecover` | 회복 시간 경과 → Chase 또는 Idle |
| `Return` | 스폰 복귀 완료 → Idle |
| `Reposition` | 과밀 회피 이동 |
| `Investigate` | 그룹 공유 기억 위치 조사 |
| `Dead` | HP ≤ 0; 리스폰 타이머 후 → Idle |

### 넉백 처리 — VelocityMotor

AI는 이동 속도를 `setDesiredVel()`로 선언하고, `physicsWorld_.step()` 내 `VelocityMotor`가 매 sub-step마다 실제 속도를 desired 방향으로 수렴시킨다.

```
velError = desiredVel - currVel  (XZ only)
corrAccel = gain * velError,  clamped by maxAcceleration / maxDeceleration
currVel.xz += corrAccel * dt
```

넉백 충격량이 클수록 오차가 커져 최대 보정이 가해지고, 수 프레임(~0.2 s)에 걸쳐 선형 수렴한다. 오차가 작아지면 비례(지수) 수렴으로 전환된다.  
`NpcConfig`의 `motorMaxAcceleration`(기본 20 m/s²)을 낮출수록 넉백이 더 오래 유지된다.

### AnimController 연동

AI 상태 전환 시점에 `animController_.switchClip()` 호출:

| AI 상태 | 클립 |
|---------|------|
| Idle | idle |
| Chase / Return | walk |
| AttackWindup | attack |
| Dead | die (loop=false) |

---

## 오브젝트 ID 시스템

`Object::id_`는 `IdPool`에서 발급된 전역 고유 ID다. `Room::objectById_` 는 ID로 희소 인덱싱되는 포인터 배열이다.

---

## 패킷 / 네트워크

파일: `PacketManager.hpp/.cpp`, `GameSession.hpp/.cpp`, `ServerEngine/protocol.hpp`

- 모든 패킷은 `PacketHeader { uint16 size; PacketType type; }` 로 시작
- `Room::broadcast()` — 모든 세션에 팬아웃
- `Room::broadcastExcept()` — 특정 세션 제외 팬아웃
- `Room` 상태 변경은 `doAsync()` / `JobQueue`를 통해 IOCP 워커 스레드에서 직렬화

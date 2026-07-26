### RoomServer

Entry: `RoomServer/roomServerMain.cpp`

권위 게임 서버. 한 `Room`이 한 파티의 시뮬레이션 전체(물리·NPC AI·스킬·존·거점)를 소유하고,
모든 상태 변경은 그 룸의 `JobQueue`를 통과한다.

### 문서 (먼저 읽을 것)

| 문서 | 내용 |
|---|---|
| **`docs/serverHandoff.md`** | **최근 수정 요약 + 지켜야 할 규약(R1~R6) + 우선순위별 남은 과제.** 서버 작업 시작 전에 이것부터 |
| `docs/serverArchitecture.md` | Room 업데이트 루프 전체 |
| `docs/roomTickCadence.md` | 틱 케이던스 불변식(절대 데드라인)과 그 근거 |
| `docs/objectIdLifecycle.md` | 오브젝트 id 발급·반납 규약, 잠복 UAF |
| `docs/skillArchitecture.md` | 스킬 타임라인·히트박스 생명주기 |
| `docs/strongholdSystem.md` | 거점(몬스터 스포너) |
| `docs/zoneSystem.md` | 트리거 존 |
| `docs/bossCombat.md` | 최종 보스 BehaviorTree |
| `docs/tacticalNpcVariants.md`, `docs/tacticalReservationAndEngage.md`, `docs/grandbaumTactic.md`, `docs/isysTactic.md` | 전술 NPC / 미드보스 인카운터 |
| `docs/serverTerrainChunk.md` | 지형 청크 로딩 |

### 핵심 타입

- `Room` — 세션·게임 오브젝트·물리를 소유. `enter()`/`leave()`/`move()`/`skillStart()`,
  `broadcast()`. `update()`가 60fps로 전체를 틱한다
- `RoomManager` — lobbyCode → Room. `findOrCreateRoomByCode`가 파티를 같은 룸에 모은다
- `GameSession` — 접속 1건. `Player`를 소유하고 `Room`에 attach
- `PacketManager` — `S_*` 패킷 생성(`makeSEnterPacket` 등) + 수신 디스패치
- `object.hpp` — `Object` 기반 클래스. `RigidBody body_` 소유, `rebuildBodyBVH()`가
  onRebuildBVH 콜백으로 등록됨. `id_`는 -1로 시작하므로 반납 전 `hasId()` 확인
- `physicsWorld.hpp` — `step(dt)` = integrate → generateContacts → solveConstraints (PGS).
  `broadPhase.hpp`(SAP), `contactConstraint.hpp`(Sequential Impulse + Baumgarte + Coulomb),
  `collision.hpp`(AABB/OBB/BVH)
- `Level` / `binaryImport` — 부팅 시 바이너리 에셋 로드. `AssetManager`가 전 룸 공유 자산 소유
- `physics.hpp` — legacy shim (`physicsWorld.hpp`만 include)

### NPC AI (`Npc` / `Goblin` 및 파생)

`Npc`는 유한 상태 기계(`NpcState`)이며 `Room::updateMonsterAI()`가 60fps로 틱한다.
상태: `Idle` ↔ `Patrol` 루프(타깃 없을 때) / `Chase` → `AttackWindup` → `AttackRecover` /
`Return`(스폰 복귀) / `Reposition`(과밀 회피) / `Investigate`(그룹 공유 기억 조사) / `Dead`(리스폰 대기).

- `checkAlert()`가 `Idle`/`Patrol` 공용 감지 단계다. 플레이어 직접 감지 → `Chase`,
  그룹 메모리(활동 구역 내) → `Investigate`. alert이면 true를 반환해 배회를 중단시킨다
- `Patrol`은 스폰 기준 `patrolRadius` 내 임의 지점으로 `moveSpeed * patrolSpeedMult`로 이동한 뒤
  `Idle`로 쉰다. 타이밍/감각은 `NpcConfig`로 튜닝
- **상태 자체는 클라에 보내지 않는다.** 클라는 `S_NpcMoveBatch`의 속도로 애니메이션을 추론한다.
  단 표현용 상태는 같은 패킷의 `statusFlags`(`NpcStatusFlag` 비트마스크, 예: Confused)로 전달된다
- 공격은 `NpcAttack{skillId, clipKey}` 목록에서 골라 스킬 시스템으로 시전한다
  (`Room::skillStartInternal`). 히트 판정은 스킬 히트박스가 담당한다

전술 NPC(`TacticalNpc`/`TacticalSquad`/`PlatoonLeader`)와 최종 보스는 별도 AI다 — 위 문서 참조.

### 불변식 (깨면 조용히 어긋난다)

1. **룸 틱은 절대 데드라인으로 재예약한다** (`JobTimer::addJobAt`). 상대 지연(`addJob`)은
   일회성 작업 전용 — `docs/roomTickCadence.md`
2. **`IdPool`에는 이 풀이 발급한 객체 id만 반납한다.** 룸 id는 `RoomIdPool` 소관이며
   재사용하지 않는다 — `docs/objectIdLifecycle.md`
3. **`Room` 상태는 락이 없다.** 그 전제는 "한 JobQueue를 한 스레드만 실행한다"에 의존한다 —
   `docs/serverHandoff.md` §4-P0에 아직 이 전제를 깰 수 있는 경로가 하나 남아 있다

### Protocol

`ServerEngine/protocol.hpp`가 단일 진실 공급원이다(약 49종). 문서에 목록을 복제하지 말고
그 파일을 직접 볼 것. 위치는 `XMFLOAT3`, 회전은 `XMFLOAT4`(쿼터니언)로 싣는다.
새 문자열 필드는 1바이트 length prefix라 **255바이트 이하**여야 한다.

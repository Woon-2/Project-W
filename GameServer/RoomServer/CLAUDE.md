### RoomServer
서버 아키텍처 참조 - `docs/serverArchitecture.md`
스킬 시스템 아키텍처 참조 - `docs/skillArchitecture.md`
입장 티켓/계정 핸드오프 참조 - `../ServerEngine/docs/entryTicket.md`
인벤토리 영속화 참조 - `docs/inventoryPersistence.md`

Entry: `RoomServer/roomServerMain.cpp`

- `Room` — central game room: holds player sessions, game objects, and physics state. All mutations go through its `JobQueue`.
  - `enter()` / `leave()` — player join/disconnect
  - `move()` — position synchronization
  - `broadcast()` — fan-out position updates to all players
- `PacketManager` — constructs outbound `S_*` packets (`makeSEnterPacket`, `makeSMovePacket`, etc.)
- `object.hpp` — `Object` base class owning a `RigidBody body_` (position, velocity, orientation, BVH); `rebuildBodyBVH()` is registered as onRebuildBVH callback
- `rigidBody.hpp` — `RigidBody` with double-buffered `BodyState` (prev/curr), `MotionType` (Kinematic/Dynamic/Static), force/impulse API
- `physicsWorld.hpp` — `PhysicsWorld`: `step(dt)` = integrate → generateContacts → solveConstraints (PGS). `Room` holds one instance.
- `broadPhase.hpp` — `SAPBroadPhase` (default) / `BruteForceBroadPhase`
- `contactConstraint.hpp` — Sequential Impulse solver with Baumgarte bias and Coulomb friction
- `constraint.hpp` — abstract Constraint interface
- `collision.hpp` — AABB/OBB/BVH collision + `ContactPoint` struct
- `physics.hpp` — legacy shim; just includes `physicsWorld.hpp`
- `Level` / `binaryImport` — level data loaded from binary asset files at startup

### NPC AI (`Npc` / `Goblin`)

`Npc` is a finite state machine (`NpcState`) ticked from `Room::updateGoblinAI()` at 60fps.
States: `Idle` (대기) ↔ `Patrol` (스폰 근처 천천히 배회) loop while no target;
`Chase` → `AttackWindup` → `AttackRecover`, plus `Return` (스폰 복귀), `Reposition` (과밀 회피),
`Investigate` (그룹 공유 메모리 조사), `Dead` (리스폰 대기).
- `checkAlert()` is the shared detection step used by both `Idle` and `Patrol`: 플레이어 직접 감지 → `Chase`,
  그룹 메모리(활동 구역 내) → `Investigate`. Returns true when alert so the NPC stops wandering.
- `Patrol` walks to a random waypoint within `patrolRadius` of spawn at `moveSpeed * patrolSpeedMult`,
  then rests in `Idle`; timings/feel are tuned via `NpcConfig` (`min/maxIdleTime`, `min/maxPatrolTime`).
- State is not sent to clients — they infer animation from the velocity in `S_NpcMoveBatch`.

### Protocol

Defined in `ServerEngine/protocol.hpp`. RoomServer handles the room/gameplay half — join
(`C_Enter` / `S_Enter` / `S_Enter_Other` / `S_Leave`), movement (`C_Move`, `C_MouseMove`,
`C_DebugTeleport`), combat and skills (`C_Attack`, `C_SkillStart`, `C_SelectSkill`),
inventory (`C_InventoryAction`), and time sync (`C_TimeSync`). Everything it sends outward
(`S_Npc*`, `S_Skill*`, `S_PlayerHp`, `S_ZoneState`, `S_StrongholdState`, …) is built in
`PacketManager`. Account and lobby packets belong to LobbyServer.

Packet payloads use `DirectX::XMFLOAT3` for position and `XMFLOAT4` for quaternion orientation.

**입장 게이트:** `C_Enter`는 서명된 `EntryTicket`을 나른다. 검증 전에는 다른 패킷을 처리하지
않는다 — 보안 목적이자, 대부분의 핸들러가 `session->room()`을 널 체크 없이 역참조하기 때문이다.
`docs/inventoryPersistence.md`와 `../ServerEngine/docs/entryTicket.md` 참조.
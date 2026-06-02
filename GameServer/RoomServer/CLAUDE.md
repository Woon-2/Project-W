### RoomServer
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

Defined in `ServerEngine/protocol.hpp`. Packet types:

- `C_Enter` / `S_Enter` / `S_Enter_Other` — player join notifications
- `S_Leave` — player disconnect
- `C_Move` / `S_Move` — position and orientation sync

Packet payloads use `DirectX::XMFLOAT3` for position and `XMFLOAT4` for quaternion orientation.
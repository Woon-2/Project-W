### RoomServer
서버 아키텍처 참조 - `docs/serverArchitecture.md`
스킬 시스템 아키텍처 참조 - `docs/skillArchitecture.md`

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

### Protocol

Defined in `ServerEngine/protocol.hpp`. Packet types:

- `C_Enter` / `S_Enter` / `S_Enter_Other` — player join notifications
- `S_Leave` — player disconnect
- `C_Move` / `S_Move` — position and orientation sync

Packet payloads use `DirectX::XMFLOAT3` for position and `XMFLOAT4` for quaternion orientation.
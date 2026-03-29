### RoomServer
Entry: `RoomServer/roomServerMain.cpp`

- `Room` — central game room: holds player sessions, game objects, and physics state. All mutations go through its `JobQueue`.
  - `enter()` / `leave()` — player join/disconnect
  - `move()` — position synchronization
  - `broadcast()` — fan-out position updates to all players
- `PacketManager` — constructs outbound `S_*` packets (`makeSEnterPacket`, `makeSMovePacket`, etc.)
- `object.hpp` — `Object` base class with `PhysicState` (position, velocity, orientation, AABB); `update(deltaTime)` runs physics
- `physics.hpp` / `collision.hpp` — server-side physics simulation and AABB collision detection
- `Level` / `binaryImport` — level data loaded from binary asset files at startup

### Protocol

Defined in `ServerEngine/protocol.hpp`. Packet types:

- `C_Enter` / `S_Enter` / `S_Enter_Other` — player join notifications
- `S_Leave` — player disconnect
- `C_Move` / `S_Move` — position and orientation sync

Packet payloads use `DirectX::XMFLOAT3` for position and `XMFLOAT4` for quaternion orientation.
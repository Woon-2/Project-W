# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build System

This is a Visual Studio 2022 solution (`GameServer.sln`). Build via MSBuild or the VS IDE.

```bash
# Build all projects (from repo root)
msbuild GameServer.sln /p:Configuration=Debug /p:Platform=x64

# Build a specific project
msbuild RoomServer/RoomServer.vcxproj /p:Configuration=Debug /p:Platform=x64

# Release build
msbuild GameServer.sln /p:Configuration=Release /p:Platform=x64
```

- **C++ Standard:** C++26 (`/std:c++latest`)
- **Toolset:** MSVC v143
- **Output:** Executables in `x64/{Debug|Release}/`, static libs in `lib/{Debug|Release}/`

There are no automated tests — manual testing uses `DummyClient` to connect and send/receive packets against a running server.

## Architecture Overview

Multi-tier networked game server with IOCP-based async networking, server-authoritative physics, and a DirectX 12 client.

### Projects

| Project | Type | Role |
|---------|------|------|
| `ServerEngine` | Static lib | IOCP networking primitives shared by all servers |
| `LobbyServer` | Executable | Initial client connection point |
| `RoomServer` | Executable | Game room with physics simulation and state broadcast |
| `DummyClient` | Executable | Test client for manual network testing |
| `client` | Executable | Game client — DirectX 12 renderer + network layer |

### ServerEngine (Networking Layer)

IOCP-based async I/O engine used by both servers:
- `IocpReactor` — completion port wrapper; drives the worker thread dispatch loop
- `Session` / `PacketSession` — connection lifecycle and packet framing/parsing
- `Listener` — TCP accept loop
- `SendBuffer` / `RecvBuffer` — pooled send/recv buffers
- `JobQueue` — async job queue for thread-safe operations within a room/session
- `MemoryManager` — object pooling to reduce allocation overhead
- `protocol.hpp` — all packet type definitions (shared between client and servers)

**Threading model:** N-1 worker threads run the IOCP dispatch loop. Each `Room` has its own `JobQueue`; operations on room state are posted as jobs rather than accessed directly.

### RoomServer (Game Logic)

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

### Client

- `ClientApp` — top-level app; switches between `StandAlone` and `Online` modes
- `online/onlineGame` — multiplayer mode; owns the network session and interpolates server state for rendering
- DirectX 12 rendering pipeline; shaders in `.hlsl` files (`billboard.hlsl`, `boundingVolume.hlsl`)

### Common Utilities

- `common/mathUtil.hpp` — angle types (`Radian`/`Degree`), `Vec3`/`Vec4`, quaternion `NQuat`, DirectX math interop
- `common/pool.hpp` — generic object pool templates
- `common/log.hpp` — logging system

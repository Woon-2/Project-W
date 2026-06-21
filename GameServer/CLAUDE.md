# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build System

This is a **Visual Studio 2022** solution (C++20, Windows only). Open `GameServer.sln` to build all projects.

**Projects and their output types:**
- `ServerEngine` — Static library shared by all servers
- `LobbyServer` — Matchmaking server executable
- `RoomServer` — Game room server executable
- `HeadlessClient` — Headless load/stress-test bot executable (no rendering)
- `client` — DirectX 12 game client executable

There are no automated tests. `HeadlessClient` spawns many fake bots to load/stress-test the RoomServer; manual gameplay validation uses the real `client`.

## Architecture Overview

A networked multiplayer 3D game with a client-server architecture split across three server processes.

### ServerEngine (Static Library)
Refer to: `ServerEngine/CLAUDE.md`- Claude reads this on-demand

### LobbyServer
Refer to: `LobbyServer/CLAUDE.md`- Claude reads this on-demand

### RoomServer
Refer to: `RoomServer/CLAUDE.md`- Claude reads this on-demand

### Client
Refer to: `client/CLAUDE.md`- Claude reads this on-demand

### HeadlessClient
Refer to: `HeadlessClient/CLAUDE.md`- Claude reads this on-demand

### common/
Shared utilities not tied to any single project:
- `log.hpp` — Logging (supports ASCII and wide-char logger stack)
- `mathUtil.hpp` — Vectors, matrices, transformations
- `pool.hpp` — Generic object pool
- `concurrentqueue.h` — Moodycamel lock-free queue (used for inter-thread messaging)

## Networking Protocol

All packets start with `PacketHeader { uint16 size; PacketType type; }`. Packet types are defined in `ServerEngine/protocol.hpp`. Current types include `C_Enter` (client→server) and `S_Enter` (server→client).

## Concurrency Model

- IOCP worker threads call `IocpReactor::dispatch()` in a tight loop
- `Room` mutations go through `JobQueue` (`doAsync()`) to avoid locks
- `SendBuffer` allocation is thread-local (4KB chunks) — avoid sharing `SendBuffer*` across threads after construction
- Moodycamel `concurrentqueue` is used for lock-free cross-thread message passing

## Language & Platform Notes

- C++20 (`/std:c++latest`), MSVC toolset v143
- Windows-only: Winsock2, IOCP, DirectX 12, DirectXMath
- Comments and identifiers are often in **Korean**

## Note
**Major changes or design changes must be recorded in markdown documents and remembered separately.**
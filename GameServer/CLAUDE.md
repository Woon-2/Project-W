# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build System

Visual Studio solution (C++20, Windows only). Open `GameServer.sln` to build all projects.

**Projects in the solution:**
- `ServerEngine` — Static library shared by all servers (`lib/<Config>/ServerEngine.lib`)
- `LobbyServer` — Matchmaking server executable
- `RoomServer` — Game room server executable
- `client` — DirectX 12 game client executable

`LobbyServer`/`RoomServer`/`client` link `ServerEngine.lib` via `#pragma comment(lib, ...)`, **not**
a project reference — so a change to `ServerEngine` requires building it first, and building the
solution with `/m` can hit `C1090` (PDB contention on the shared `ServerEngine.pdb`). Build
`ServerEngine` alone, then the rest.

The `DummyClient/` directory still exists on disk but is **no longer part of the solution**.
There is no automated test suite; `tests/inventoryModelSelfTest.cpp` is a standalone self-test
(see `client/docs/inventorySystem.md`). Validation is manual, by running the servers + client.

## Architecture Overview

A networked multiplayer 3D game with a client-server architecture split across three server processes.

### ServerEngine (Static Library)
Refer to: `ServerEngine/CLAUDE.md`- Claude reads this on-demand

### LobbyServer
Refer to: `LobbyServer/CLAUDE.md`- Claude reads this on-demand

### RoomServer
Refer to: `RoomServer/CLAUDE.md`- Claude reads this on-demand

**틱 케이던스 불변식:** Room 틱은 절대 데드라인으로 재예약된다(`JobTimer::addJobAt`). 고정 주기
반복을 상대 지연(`addJob`)으로 잡으면 처리 시간이 매 틱 누적돼 시뮬 클럭이 실시간에서 이탈하고,
스킬 히트 판정이 클라 예측과 어긋난다. 배경·설계 근거: `RoomServer/docs/roomTickCadence.md`

### Client
Refer to: `client/CLAUDE.md`- Claude reads this on-demand

### common/
Shared by client + servers (not tied to one project):
- `log.hpp` — Logging (ASCII and wide-char logger stack)
- `mathUtil.hpp` — Vectors, matrices, transformations (`mu::` namespace)
- `pool.hpp` / `slotVector.hpp` — Generic object pool, tombstone-reusing slot vector
- `inventory.{hpp,cpp}` — Item/inventory model mirrored by client and server
- `zoneDef.hpp` / `markerDef.hpp` / `arenaWall.hpp` / `scatterTransform.hpp` — level data shared with the terrain pipeline
- `particleGameplay.hpp` — deterministic VFX-particle hitbox contract (client/server mirror)
- `networkConfig.{hpp,cpp}` / `simpleJson.{hpp,cpp}` — `network_config.json` loading
- `sol/`, `lua*.h` — Lua bindings (skill + config scripts)

> Moodycamel `concurrentqueue.h` lives in `ServerEngine/`, not here.

## Networking Protocol

All packets start with `PacketHeader { uint16 size; PacketType type; }`. Every packet type is
defined in `ServerEngine/protocol.hpp` (~49 and growing: enter/leave, movement, NPC batches,
skills, charge/combo, inventory, zones, strongholds, debug). **`protocol.hpp` is the single
source of truth — read it rather than any list in a doc.** Wire ordinals are append-only.

## Concurrency Model

- IOCP threads call `IocpReactor::dispatch()` in a tight loop; separate job threads drain
  `JobQueuePool`, and one thread spins `JobTimer::distribute()` (see `RoomServer::start`)
- `Room` mutations go through `JobQueue` (`doAsync()`) to avoid locks. **That "no locks needed"
  guarantee rests entirely on one JobQueue being executed by one thread at a time** —
  see `RoomServer/docs/serverHandoff.md` §3-R6 for the one path that can still break it
- `SendBuffer` allocation is thread-local (64KB chunks) — avoid sharing `SendBuffer*` across threads after construction
- Moodycamel `concurrentqueue` is used for lock-free cross-thread message passing.
  **It is not strictly FIFO** (per-producer subqueues) — never assume ordering across producers;
  this made an id-recycling bug reproduce only intermittently

## Object ids (server)

All object ids — sessions, monsters, strongholds — come from one global `IdPool` (1..65535;
0 is a reserved "no target" sentinel). Returning a foreign id to it makes two live objects share
one id. Rules and the failure modes: `RoomServer/docs/serverHandoff.md` §3.

## Language & Platform Notes

- C++20 (`/std:c++latest`), MSVC toolset **v145** (VS18 MSBuild required; older MSBuild fails with `MSB8020`)
- Windows-only: Winsock2, IOCP, DirectX 12, DirectXMath
- Comments and identifiers are often in **Korean**
- Existing sources are **UTF-8 with BOM**. A new file saved without a BOM is read as cp949 on a
  Korean Windows and Korean comments break the build — **no Korean comments in new files**

## Note
**Major changes or design changes must be recorded in markdown documents and remembered separately.**
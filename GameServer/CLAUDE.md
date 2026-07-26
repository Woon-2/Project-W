# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build System

A Visual Studio solution (C++20, Windows only). Open `GameServer.sln` to build all projects.
PlatformToolset is **v145** (VS 18) — building from the command line needs VS 18's MSBuild, not VS 2022's.
Only the **x64** configurations are used (the `x86` ones map to `Win32` and produce nothing).

**Projects and their output types:**
- `ServerEngine` — Static library shared by all servers (`lib\$(Configuration)\ServerEngine.lib`)
- `LobbyServer` — Matchmaking + account/login server executable (`x64\$(Configuration)\`)
- `RoomServer` — Game room server executable
- `client` — DirectX 12 game client executable

**`ServerEngine`를 항상 먼저 빌드할 것.** 솔루션에 `ProjectReference`가 없어 빌드 의존성이
등록돼 있지 않다(링크는 각 pch의 `#pragma comment(lib, ...)`로 이뤄진다). `ServerEngine.lib`는
저장소에 커밋된 바이너리라, git 체크아웃으로 파일 mtime이 obj보다 최신이 되면 MSBuild가 링크를
건너뛰고 **낡은 lib을 그대로 남긴다** — 서버가 LNK2019로 터지면 `/t:ServerEngine:Rebuild`로 강제한다.

There are no automated tests. Server validation is manual: run `LobbyServer` + `RoomServer` and
drive them with the `client`. (`DummyClient` was removed in `c96eaac0`.)

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

### common/
Shared utilities not tied to any single project:
- `log.hpp` — Logging (supports ASCII and wide-char logger stack)
- `mathUtil.hpp` — Vectors, matrices, transformations
- `pool.hpp` — Generic object pool
- `concurrentqueue.h` — Moodycamel lock-free queue (used for inter-thread messaging)

## Networking Protocol

All packets start with `PacketHeader { uint16 size; PacketType type; }`. Packet types are defined in
`ServerEngine/protocol.hpp`, which the client includes as-is — the structs *are* the wire format
(`#pragma pack(1)`, no serializer). `PacketType` and the enums on the wire are **append-only**:
the ordinal is the wire value, so never insert in the middle.

**LobbyServer requires login first.** `C_Register`/`C_Login` are the only packets accepted before
`S_Login(Ok)`; everything else from an unauthenticated session is silently dropped
(`LobbyServer/PacketManager.cpp`, 인증 게이트). See `ServerEngine/docs/accountSystem.md`.

**RoomServer requires a signed entry ticket.** `C_Enter` is the only packet accepted before the
ticket verifies; everything else is dropped (`RoomServer/PacketManager.cpp`, 입장 게이트). The
lobby mints the ticket (HMAC-SHA256) at `C_GameStart` and the client relays it — there is **no
lobby↔room socket**, so anything the client carries must be signed. The shared key lives in
`security_config.json` (server-only, next to `db_config.json`; **never add it to `client.vcxproj`**).
See `ServerEngine/docs/entryTicket.md`.

## Concurrency Model

- IOCP worker threads call `IocpReactor::dispatch()` in a tight loop
- `Room` mutations go through `JobQueue` (`doAsync()`) to avoid locks
- `SendBuffer` allocation is thread-local (4KB chunks) — avoid sharing `SendBuffer*` across threads after construction
- Moodycamel `concurrentqueue` is used for lock-free cross-thread message passing

## Language & Platform Notes

- C++20 (`/std:c++latest`), MSVC toolset v145
- Windows-only: Winsock2, IOCP, DirectX 12, DirectXMath
- Comments and identifiers are often in **Korean**

## Note
**Major changes or design changes must be recorded in markdown documents and remembered separately.**
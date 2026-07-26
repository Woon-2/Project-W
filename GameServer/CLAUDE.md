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

Building `client` is what places the runtime DLLs (`dxcompiler.dll`, `dxil.dll`, `lua54.dll`) into
`x64\$(Configuration)\` — its PostBuildEvent copies them. **Build servers only and `RoomServer`
won't start**, because `lua54.dll` is missing.

The servers and the client resolve assets through `../resources/...`, so the **working directory
must be each project's own folder**. No project sets `LocalDebuggerWorkingDirectory`, so this
relies on VS's default of `$(ProjectDir)` — launching the exe from `x64\Debug\` directly fails.

There are no automated tests. Server validation is manual: run `LobbyServer` + `RoomServer` and
drive them with the `client`. (`DummyClient` was removed in `c96eaac0`.)

**새 PC에서 시연 준비: `docs/demoSetup.md`** — 설치 목록(ODBC Driver 17 함정 포함), 스키마 적용,
실행 순서, 증상별 진단표.

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
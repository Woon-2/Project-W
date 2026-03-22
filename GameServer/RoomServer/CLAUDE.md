### RoomServer
Entry: `RoomServer/roomServerMain.cpp`

Game room server. Key classes:
- `RoomServer` — Owns `IocpReactor` + `Listener`
- `Room` — Manages a set of players; thread-safe via `doAsync()` job queue; exposes `enter()`, `leave()`, `broadcast()`
- `RoomManager` — Owns multiple `Room` instances
- `Player` — Per-player state within a room
- `PacketManager` — Packet parsing and dispatch

Initializes `SocketUtils`, `MemoryManager`, `IdPool`, `RoomIdPool`.
### ServerEngine
Core async networking library used by all servers. Built on **Windows IOCP (I/O Completion Ports)**.

Key abstractions:
- `IocpReactor` — IOCP event loop; servers call `iocpCore().dispatch()` in worker threads
- `Session` — Base class for a network connection; handles async send/recv, connect/disconnect lifecycle
- `SendBuffer` / `RecvBuffer` — Buffer management; `SendBuffer` uses 4KB chunks with thread-local pooling to avoid lock contention
- `Listener` — Server-side accept socket
- `JobQueue` — Async job dispatch; used by `Room` to serialize mutations without a global lock
- `protocol.hpp` — Binary packet definitions (`PacketHeader` = size + type, `PacketType` enum, `PlayerInfo`, etc.)
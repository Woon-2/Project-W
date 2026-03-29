### ServerEngine (Networking Layer)
Core async networking library used by all servers. Built on **Windows IOCP (I/O Completion Ports)**.

IOCP-based async I/O engine used by both servers:
- `IocpReactor` — completion port wrapper; drives the worker thread dispatch loop
- `Session` / `PacketSession` — connection lifecycle and packet framing/parsing
- `Listener` — TCP accept loop
- `SendBuffer` / `RecvBuffer` — pooled send/recv buffers
- `JobQueue` — async job queue for thread-safe operations within a room/session
- `MemoryManager` — object pooling to reduce allocation overhead
- `protocol.hpp` — all packet type definitions (shared between client and servers)

**Threading model:** N-1 worker threads run the IOCP dispatch loop. Each `Room` has its own `JobQueue`; operations on room state are posted as jobs rather than accessed directly.
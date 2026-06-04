### ServerEngine (Networking Layer)
Core async networking library used by all servers. Built on **Windows IOCP (I/O Completion Ports)**.

IOCP-based async I/O engine used by both servers:
- `IocpReactor` — completion port wrapper; drives the worker thread dispatch loop
- `Session` / `PacketSession` — connection lifecycle and packet framing/parsing
- `IocpDispatchable` — base for reactor-dispatched objects; `enable_shared_from_this` (세션 수명 관리의 토대)
- `SendBuffer` / `RecvBuffer` — pooled send/recv buffers
- `JobQueue` — async job queue for thread-safe operations within a room/session
- `MemoryManager` / `ObjectPool` — object pooling to reduce allocation overhead
- `protocol.hpp` — all packet type definitions (shared between client and servers)

> **Listener는 ServerEngine에 없다.** TCP accept 루프는 각 서버(`LobbyServer`/`RoomServer`)가 자체 `Listener`로
> 구현한다(세션 타입/포트가 서버마다 다르기 때문).

**Threading model:** N-1 worker threads run the IOCP dispatch loop. Each `Room` has its own `JobQueue`; operations on room state are posted as jobs rather than accessed directly.

**Session 수명(중요):** `IoEvent::owner_`는 `shared_ptr<IocpDispatchable>`다. `register{Recv,Send,Disconnect}`가
`shared_from_this()`로 ref를 잡고 `process*`가 놓으므로 **진행 중 I/O가 세션을 살린다**(리액터는 dispatch 동안
로컬 shared_ptr 복사를 유지). 세션은 `ObjectPool<T>::makeShared()`(deleter=`push`)로 생성되어, 마지막 ref가
사라지는 순간(= 모든 pending I/O 완료 + 소유자 해제) `~Session`이 돌고 풀로 자동 반환된다. 따라서 끊긴 세션을
직접 `delete`/`push`하면 안 된다(IOCP 완료 통지가 dequeue되기 전 해제 시 use-after-free).
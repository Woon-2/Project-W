### ServerEngine (Networking Layer)
Core async networking library used by all servers. Built on **Windows IOCP (I/O Completion Ports)**.

IOCP-based async I/O engine used by both servers:
- `IocpReactor` — completion port wrapper; drives the worker thread dispatch loop
- `Session` / `PacketSession` — connection lifecycle and packet framing/parsing
- `IocpDispatchable` — base for reactor-dispatched objects; `enable_shared_from_this` (세션 수명 관리의 토대)
- `SendBuffer` / `RecvBuffer` — pooled send/recv buffers
- `JobQueue` — async job queue for thread-safe operations within a room/session
- `MemoryManager` / `ObjectPool` / `MemoryPool` — object pooling to reduce allocation overhead
- `IdPool` — global object-id allocator (sessions, monsters, strongholds)
- `protocol.hpp` — all packet type definitions (shared between client and servers)
- `concurrentqueue.h` — Moodycamel lock-free queue (lives here, not in `common/`)

> **Listener는 ServerEngine에 없다.** TCP accept 루프는 각 서버(`LobbyServer`/`RoomServer`)가 자체 `Listener`로
> 구현한다(세션 타입/포트가 서버마다 다르기 때문).

**Threading model** (`RoomServer::start` 기준): IOCP dispatch 스레드 2개(메인 포함),
`JobTimer::distribute()`를 도는 타이머 스레드 1개, `JobQueuePool`을 비우는 잡 스레드
`max(1, cores - 3)`개. 각 `Room`은 자기 `JobQueue`를 갖고, 룸 상태 변경은 직접 접근 대신
잡으로 게시한다.

> **JobQueue 불변식(중요):** `Room`이 락 없이 동작하는 것은 **한 `JobQueue`를 한 시점에 한
> 스레드만 실행한다**는 전제에 전적으로 의존한다. 이 전제가 깨지면 룸 컨테이너 전체가 경쟁
> 상태가 된다. `JobQueue::execute()`에 상시 탐지기가 있어 위반 시
> `[JobQueue] CONCURRENT EXECUTE` / `NEGATIVE jobCount`를 출력한다 — **뜨면 무시하지 말 것.**
> 아직 남아 있는 위반 경로(Room 자기 파괴)는 `RoomServer/docs/serverHandoff.md` §4-P0.

> **IdPool 규약:** 세션·몬스터·거점 id를 전부 이 풀 하나가 발급한다(1..65535, **0은 "타깃 없음"
> sentinel로 예약**). 발급하지 않은 id(다른 id 공간, id 미할당 객체)를 반납하면 두 객체가 같은
> id를 갖게 되므로, 풀이 이를 **거부하고 보고**한다(`[IdPool] STRAY/INVALID PUSH … REJECTED`).
> 새 오브젝트 타입을 추가하면 발급/반납 경로를 짝으로 만들 것 —
> `RoomServer/docs/objectIdLifecycle.md`.

> **moodycamel `ccqueue`는 엄격한 FIFO가 아니다.** producer별 서브큐를 갖기 때문에 반납한 값이
> 곧바로 다시 나올 수도, 한참 뒤에 나올 수도 있다. `IdPool`·`JobQueuePool`·`SendBufferManager`가
> 모두 이 큐를 쓰므로 **순서를 가정한 로직을 얹지 말 것.** id 재사용 버그가 "가끔"만 재현된
> 원인이 이것이었다.

**Session 수명(중요):** `IoEvent::owner_`는 `shared_ptr<IocpDispatchable>`다. `register{Recv,Send,Disconnect}`가
`shared_from_this()`로 ref를 잡고 `process*`가 놓으므로 **진행 중 I/O가 세션을 살린다**(리액터는 dispatch 동안
로컬 shared_ptr 복사를 유지). 세션은 `ObjectPool<T>::makeShared()`(deleter=`push`)로 생성되어, 마지막 ref가
사라지는 순간(= 모든 pending I/O 완료 + 소유자 해제) `~Session`이 돌고 풀로 자동 반환된다. 따라서 끊긴 세션을
직접 `delete`/`push`하면 안 된다(IOCP 완료 통지가 dequeue되기 전 해제 시 use-after-free).
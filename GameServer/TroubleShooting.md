# TroubleShooting

---
### 2026.04.16 / 목요일
## [Bug Fix] SendBufferChunk 크기 초과로 인한 ASSERT_CRASH

### 현상

`SNpcMoveBatchPacket` 전송 시 `SendBufferChunk::open()` 내부의 `ASSERT_CRASH` 발생.

```
ASSERT_CRASH(size <= chunkSize_);  // SendBuffer.hpp
```

### 원인

`SendBufferChunk::chunkSize_`가 **4096바이트(4KB)**로 고정되어 있었다.
NPC 이동 배치 최적화로 goblin 50마리의 이동 정보를 한 패킷에 담기 시작하면서 이 한도를 초과했다.

`SNpcMoveInfo` 한 개의 크기:

| 필드 | 타입 | 크기 |
|------|------|------|
| npcId | uint16 | 2 bytes |
| pos | XMFLOAT3 | 12 bytes |
| orient | XMFLOAT4 | 16 bytes |
| velocity | XMFLOAT3 | 12 bytes |
| **합계** | | **42 bytes** |

100마리 기준: 헤더(4) + 42 × 100 = **4600 bytes**. 
goblin 수가 늘어나거나 다른 오브젝트까지 포함될 경우 쉽게 4096을 초과.

### 수정

`ServerEngine/SendBuffer.hpp:84`

```cpp
// 변경 전
static const uint32 chunkSize_{4096u};

// 변경 후
static const uint32 chunkSize_{8192u};
```

---
### 2026.04.16 / 목요일
## [Optimization] NPC 이동 배치 전송 (SNpcMoveBatch)

### 배경

기존에는 `updateGoblinAI()` 루프에서 goblin 한 마리가 이동할 때마다 `SNpcMovePacket`을 개별 broadcast했다.
goblin이 N마리라면 프레임당 N번의 broadcast가 발생해 네트워크 및 IOCP 오버헤드가 컸다.

### 최적화 내용

프레임당 모든 goblin의 이동 정보를 수집해 `SNpcMoveBatchPacket` **하나**로 묶어 전송.
broadcast 횟수: N → 1 (per frame).

### 변경 파일

| 파일 | 변경 내용 |
|------|-----------|
| `ServerEngine/protocol.hpp` | `SNpcMoveInfo` 구조체 및 `SNpcMoveBatchPacket` 추가 |
| `RoomServer/PacketManager.cpp` | `makeSNpcMoveBatchPacket(vector<SNpcMoveInfo>)` 함수 추가 |
| `RoomServer/Room.cpp` | `updateGoblinAI()`에서 moveInfos 수집 후 배치 broadcast 적용 |

### 핵심 코드 (Room.cpp)

```cpp
std::vector<SNpcMoveInfo> moveInfos;
moveInfos.reserve(goblins_.size());

for (auto& goblin : goblins_) {
    // ...
    if (goblin.hp() > 0) {
        moveInfos.push_back({ ... });
    }
}

if (!moveInfos.empty()) {
    broadcast(PacketManager::makeSNpcMoveBatchPacket(moveInfos));
}
```

---
### 2026.04.17 / 금요일
## [Bug Fix] RecvBuffer::clean() writePos 오계산으로 인한 무한 패킷 루프

### 현상

일정 시간 플레이 후 클라이언트가 정상 패킷 수신을 멈추고 아래 로그가 무한 반복됐다.

```
Received packet - Size: 0, Type: 0
Unknown packet type received. Type: 0
Received packet - Size: 0, Type: 0
Unknown packet type received. Type: 0
...
```

### 원인

`RecvBuffer::clean()` 에서 `readPos_ = 0` 으로 설정한 **뒤** `dataSize()` 를 호출해 `writePos_` 를 계산한다.

```cpp
// 버그 코드
memcpy(&buffer_[0], &buffer_[readPos_], dataSize());
readPos_ = 0;
writePos_ = dataSize();   // ← 이 시점의 dataSize() = writePos_ - 0 = old writePos_
                           //   실제 남은 바이트(old writePos_ - old readPos_)가 아님
```

이 버그로 인해 `writePos_` 가 갱신되지 않고 기존 값을 유지한다. 다음 `onRecv(buffer, dataSize())` 호출 시 실제 유효 데이터를 넘어선 **이미 처리된 stale 영역**까지 읽는다. stale 영역에 `SNpcMoveInfo.npcId = 0`, `pos.x = 0.0f` 등 4-zero 바이트 시퀀스가 존재하면 `header->size = 0` 으로 해석된다.

이 버그는 `freeSize() < bufferSize_` 일 때만 발동한다.

- `bufferSize_ = 64KB`, `capacity_ = 640KB`
- `writePos_ > 576KB` 이면 조건 충족
- `SNpcMoveBatch(2108B)` 기준 60Hz 트래픽에서 수 초 내 도달 가능

`header->size = 0` 이 되면 `PacketSession::onRecv` 에서 `recvLen += 0` 으로 루프 탈출이 불가능해진다.

```cpp
// Session.cpp — 무한루프 조건
processPacket(buffer + recvLen, header->size);  // size=0으로 반복 호출
recvLen += header->size;                         // 0을 더하므로 진행 없음
```

### 수정

**1. `ServerEngine/RecvBuffer.hpp:41`, `client/RecvBuffer.hpp:17`** — `readPos_` 갱신 전에 remaining 캡처

```cpp
// 수정 전
memcpy(&buffer_[0], &buffer_[readPos_], dataSize());
readPos_ = 0;
writePos_ = dataSize();   // 버그: readPos_=0 이후라 old writePos_ 반환

// 수정 후
int32 remaining = dataSize();   // readPos_ 갱신 전에 캡처
memcpy(&buffer_[0], &buffer_[readPos_], remaining);
readPos_ = 0;
writePos_ = remaining;          // 실제 복사된 바이트 수
```

**2. `ServerEngine/Session.cpp:208`** — `size=0` 패킷에 대한 방어 로직 추가

```cpp
// 수정 전: size < sizeof(PacketHeader) 검증 없음
if (dataSize < header->size) { break; }

// 수정 후
if (header->size < sizeof(PacketHeader)) {
    recvLen = -1;   // processRecv에서 disconnect 트리거
    break;
}
if (dataSize < header->size) { break; }
```

---
### 2026.04.17 / 금요일
## [Bug Fix] SendBufferChunk Use-After-Reset으로 인한 2번째 플레이어 garbage 패킷

### 현상

1번째 플레이어는 정상 플레이 가능하지만, 2번째 플레이어부터 접속 직후 아래와 같은 garbage 패킷을 수신했다.

```
Received packet - Size: 49693, Type: 0
Unknown packet type received. Type: 0
Received packet - Size: 52224, Type: 32577
Unknown packet type received. Type: 32577
Received packet - Size: 0, Type: 0
Unknown packet type received. Type: 0
...
```

### 원인

`broadcast()`는 모든 세션에 **동일한 `shared_ptr<SendBuffer>` 하나**를 공유한다.

```cpp
void Room::broadcast(const std::shared_ptr<SendBuffer>& sendBuffer) {
    for (auto session : sessions_) {
        session->send(sendBuffer);  // session1, session2 모두 같은 sendBuffer
    }
}
```

`SendBuffer::buffer_`는 thread-local `SendBufferChunk::buffer_[]` 내부의 raw pointer다. `SendBufferManager::open()`에서 `freeSize() < size`이면 `reset()`을 호출해 `usedSize_ = 0`으로 초기화하고 청크 앞쪽부터 재사용했다. 이때 send 큐에 아직 남아있는 이전 `SendBuffer`들의 `buffer_` 포인터가 덮어쓰인 메모리를 가리키게 된다.

```
LSendBufferChunk = chunk_A (ref=1)

open() → SendBuffer1.buffer_ = &chunk_A[0]  ← raw pointer, ref 증가 없음
broadcast → session1.sendQueue, session2.sendQueue 모두 같은 SendBuffer1 보유

session1 WSASend 완료 → sending_=false

... 패킷 계속 생성, freeSize < 2108 ...

chunk_A.reset() → usedSize_=0
새 패킷 데이터를 chunk_A[0..] 에 덮어씀

session2 registerSend → WSABUF.buf = chunk_A[0] = 덮어씌워진 데이터
                      → 클라이언트가 garbage 수신
```

garbage 크기(`49693 = 0xC21D` 등)는 `SNpcMoveInfo`의 float 위치값 바이트 패턴이 패킷 헤더로 잘못 해석된 것이다.

### 수정

**`SendBufferChunk`가 살아있는 한 메모리를 재사용하지 않는다.**
`SendBuffer`가 `shared_ptr<SendBufferChunk>`를 보유해 청크 수명을 연장하도록 변경.
청크 공간 부족 시 `reset()` 대신 **새 청크를 생성**하고 구 청크는 마지막 `SendBuffer` 소멸 시 자동 풀 반환.

**`ServerEngine/SendBuffer.hpp`**

```cpp
// SendBufferChunk: enable_shared_from_this 상속
class SendBufferChunk : public std::enable_shared_from_this<SendBufferChunk> { ... };

// SendBuffer: owner_를 shared_ptr로 변경
class SendBuffer {
public:
    SendBuffer(const std::shared_ptr<SendBufferChunk>& owner, byte* buffer, uint32 allocSize)
        : owner_(owner), ...
private:
    std::shared_ptr<SendBufferChunk> owner_;  // raw pointer → shared_ptr
};

// SendBufferChunk::open(): shared_from_this() 전달
std::shared_ptr<SendBuffer> open(uint32 size) {
    ...
    return ObjectPool<SendBuffer>::makeShared(shared_from_this(), buffer(), size);
}

// SendBufferManager: reset 대신 새 청크 pop, 청크 풀(push/pop) 도입
class SendBufferManager {
    static std::shared_ptr<SendBuffer> open(uint32 size);  // freeSize 부족 시 pop()
    static void push(SendBufferChunk* chunk);              // 풀 반환 (custom deleter)
    static std::shared_ptr<SendBufferChunk> pop();         // 풀에서 꺼내거나 신규 생성
    static ccqueue<std::shared_ptr<SendBufferChunk>> sendBufferChunks_;
};
```

**`ServerEngine/globalTLS.hpp`, `globalTLS.cpp`**

```cpp
// 변경 전
thread_local SendBufferChunk* LSendBufferChunk = nullptr;

// 변경 후
thread_local std::shared_ptr<SendBufferChunk> LSendBufferChunk;
```

수정 후 lifecycle:

```
LSendBufferChunk = shared_ptr<chunk_A>  ref=1

open() → shared_from_this() → SendBuffer.owner_ = shared_ptr<chunk_A>  ref=2
broadcast → session1, session2 모두 SendBuffer 보유

freeSize < 2108 → LSendBufferChunk = pop() (새 chunk_B)
  → chunk_A ref: 2→1, push() 호출 안 됨, 풀 미귀환  ✅

session1, session2 WSASend 완료 → SendBuffer 소멸 → ref: 1→0
  → push(chunk_A) 호출 → 풀 귀환  ✅
```

---
### 2026.04.19 / 일요일
## [Optimization] RoomServer 스레드 아키텍처 3-way 분리 (IOCP / JobTimer / Job)

### 현상

Visual Studio Concurrency Visualizer에서 RoomServer 워커 스레드들이 **99% 동기화** 상태로 관찰됨.
실제 게임 로직 실행 시간이 거의 없고 대부분의 시간을 커널 대기에 소비하고 있었다.

### 원인

기존 `DoWork()` 루프 구조:

```cpp
void DoWork(IocpReactor& reactor) {
    while (true) {
        LWorkStartTime = HighResolutionClock::now();
        reactor.dispatch(17);       // 매 루프마다 최대 17ms 커널 대기
        JobTimer::distribute();
        DoJob();
    }
}
```

모든 워커 스레드가 `dispatch(17)`으로 17ms마다 커널 대기에 진입했다.
IOCP 이벤트가 없으면 스레드 전체가 타임아웃까지 블로킹되어 CPU가 실제 게임 로직을 실행하지 못했다.

추가로 100방 목표 시 단일 스레드 순차 처리로는 `100방 × 1ms = 100ms/tick`이 되어 16.7ms 예산 초과.

### 수정 — 3-way 스레드 분리

| 스레드 | 수 | 역할 | 대기 방식 |
|---|---|---|---|
| IOCP | 2 | 패킷 수신/발신 전담 | `dispatch(INFINITE)` |
| JobTimer | 1 | 예약된 job을 room에 전달 | pure busy wait |
| Job | coreCnt-3 | 방 물리/AI 병렬 실행 | pure busy wait |

```cpp
void DoIocp(IocpReactor& reactor) {
    while (true) { reactor.dispatch(); }
}

void DoJobTimer() {
    while (true) { JobTimer::distribute(); }
}

void DoJob() {
    while (true) {
        auto* jq = JobQueuePool::pop();
        if (!jq) continue;
        jq->execute();
    }
}
```

IOCP/JobTimer 스레드는 대부분 커널 대기 또는 빈 큐 상태이므로 Job 스레드들과 코어 경쟁이 거의 없다.

### 변경 파일

| 파일 | 변경 내용 |
|---|---|
| `ServerEngine/JobQueue.hpp` | `push()` `pushOnly` 파라미터 제거 |
| `ServerEngine/JobQueue.cpp` | `push()` 항상 `JobQueuePool`로, `execute()` 64ms 타임아웃 및 `LJobQueue` 제거 |
| `ServerEngine/globalTLS.hpp` | `LJobQueue`, `LWorkStartTime` 제거 |
| `ServerEngine/globalTLS.cpp` | 위 두 정의 제거 |
| `RoomServer/RoomServer.hpp` | `workerThreads_` → `iocpThreads_`, `jobTimerThread_`, `jobThreads_` |
| `RoomServer/RoomServer.cpp` | `DoWork()` → `DoIocp()`, `DoJobTimer()`, `DoJob()`, `start()` 재편 |

### 결과

Concurrency Visualizer 재측정: **실행 75%, 동기화 23%**

남은 23% 동기화는 `JobTimer::jobTimerMtx_` 경합으로 추정.
DoJobTimer가 busy wait으로 `distribute()`를 호출하는 동시에 Job 스레드들이 `room->update()` 완료 후 `JobTimer::addJob()`으로 같은 mutex를 경쟁함.

### 보충 — 기존 구조의 99% 동기화에 대한 이해

**Q. 기존 코드에서 왜 동기화가 99%였던 거지? 플레이어 enter, leave, move 등 잘 됐고, room update도 잘 됐는데?**

99% 동기화는 고장이 아니라 설계 특성이었다. `dispatch(17)`이 루프마다 최대 17ms 커널 대기를 소비했고, I/O 이벤트가 적은 테스트 환경에서는 그 대기가 대부분 타임아웃으로 채워졌다. 실제 작업(distribute + DoJob)은 짧게 실행되고 있었으므로 게임은 정상 동작했다.

```
동기화% ≈ 17ms / (17ms + 실제작업시간)
실제작업이 0.17ms → 동기화 99%
```

**Q. 모든 스레드가 같은 작업 순서를 반복하니 병목이 없는 구조 아닌가?**

correctness 관점에서는 맞다. dispatch → distribute → DoJob 순서로 모든 스레드가 균등하게 일하고, 64ms 타임아웃으로 독점도 방지했다. 문제는 correctness가 아니라 **Job 스레드의 실행 시작 시점이 dispatch(17) 사이클에 종속**된다는 점이었다.

**Q. 방이 늘어나면 동기화%가 줄어들었을까?**

줄어들었겠지만 그게 개선이 아니다. 방이 늘수록 실제 작업 시간이 길어지고 동기화%는 낮아지지만, 그 시점은 루프 주기가 16.7ms를 초과하기 시작하는 구간이다.

```
방 10개:  [17ms 대기] + [2ms 작업]  = 19ms 루프  → room update 약간 지연
방 100개: [17ms 대기] + [17ms 작업] = 34ms 루프  → 60Hz → 30Hz로 저하
```

동기화%가 낮아지는 건 서버가 한계에 다가간다는 신호였다.

**Q. 여러 스레드가 작업을 분배하면 60Hz를 맞출 수 있지 않을까?**

이론상 스레드들이 엇갈려서 돌면 가능하지만, 실제로는 스레드들이 **같이 잠들고 같이 깨어나는 경향**이 있다. I/O 이벤트가 적으면 모든 스레드가 dispatch(17)에서 동시에 타임아웃으로 깨어나 distribute → DoJob을 같이 실행한다.

```
스레드A: [====17ms 대기====][짧은 작업]
스레드B: [====17ms 대기====][짧은 작업]
스레드C: [====17ms 대기====][짧은 작업]
```

분산이 아니라 오히려 동기화가 일어나는 구조였고, 스레드가 자연스럽게 엇갈리는 건 충분한 I/O 부하가 있을 때만 발생하는 우연한 효과였다.

---
### 2026.04.30 / 목요일
## [Bug Fix] NPC Return 상태에서 스폰 위치 도달 실패 → Idle 전환 불가

### 현상

Return 상태의 NPC가 스폰 방향으로 계속 이동하지만 Idle 상태로 전환되지 않고 영구적으로 Return 상태에 머무름.

### 원인

**`spawnPos_.y`와 실제 `pos().y`의 불일치.**

`Room::init()`에서 `setSpawnPos(g.pos())`를 호출하는 시점에는 물리 시뮬레이션이 아직 실행되지 않은 상태다. 이 시점의 `g.pos().y`는 레벨 파일 노드의 월드 Y(지형 표면 기준)이다.

이후 `physicsWorld_.step()`이 실행되면 중력이 적용되고, Dynamic 바디인 Goblin이 지형 위에 안착한다. 이때 `pos().y`는 모델 중심점 오프셋(모델 바닥 ~ 중심 높이)만큼 `spawnPos_.y`보다 높아진다.

```
spawnPos_.y  = 레벨 노드 Y (예: 0.0)
pos().y      = 지형 Y + 모델 중심 오프셋 (예: 1.0)
```

`updateReturn()`의 스폰 도달 조건은 **3D 거리** 비교였다.

```cpp
mu::Vec3 toSpawn = spawnPos_ - pos();
if (toSpawn.len2() < 0.6f * 0.6f) { ... }  // 버그: 3D 거리
```

XZ 평면 기준으로는 스폰 위치에 완전히 도달해도 Y 차이(~1.0m)만으로 3D 거리가 0.6m를 초과해 조건이 영원히 충족되지 않는다.

### 수정

**`RoomServer/Npc.cpp` — `updateReturn()`**

도달 판정과 이동 방향 계산을 모두 XZ 2D로 변경. Idle 전환 시 Y는 물리 엔진이 관리하는 현재값을 유지.

```cpp
// 변경 전
mu::Vec3 toSpawn = spawnPos_ - pos();
if (!isOutsideActivityZone() && toSpawn.len2() < 1.0f * 1.0f && ...) { ... }
if (toSpawn.len2() < 0.6f * 0.6f) {
    setPos(spawnPos_);          // Y도 spawnPos_.y로 덮어씀
    ...
}
mu::NVec3 nd(toSpawn + sep * ...);    // Y 성분 포함된 채로 정규화 → XZ 이동량 왜곡

// 변경 후
mu::Vec3 toSpawn = spawnPos_ - pos();
float xzToSpawn2 = toSpawn.x() * toSpawn.x() + toSpawn.z() * toSpawn.z();
if (!isOutsideActivityZone() && xzToSpawn2 < 1.0f * 1.0f && ...) { ... }
if (xzToSpawn2 < 0.6f * 0.6f) {
    setPos(mu::Vec3(spawnPos_.x(), pos().y(), spawnPos_.z()));  // Y 보존
    ...
}
mu::Vec3 toSpawnXZ(toSpawn.x(), 0.f, toSpawn.z());
mu::NVec3 nd(toSpawnXZ + sep * ...);  // Y 제거 후 정규화 → XZ 이동량 정확

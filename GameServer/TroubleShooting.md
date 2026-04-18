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

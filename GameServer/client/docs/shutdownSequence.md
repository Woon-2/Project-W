# client 종료 시퀀스 설계 (2026-06-12)

## 배경: WinMain return 지점 크래시

종료 시 `main.cpp`의 `return static_cast<int>(msg.wParam);`에서 크래시가 발생했다.
실제 원인은 return 문이 아니라 **return 이후 CRT exit에서 실행되는 thread_local/static 소멸자**였다.

근본 원인 (ServerEngine SendBuffer 재활용 설계):

1. `SendBufferChunk`의 shared_ptr deleter는 `SendBufferManager::push` — 청크를 해제하지 않고
   정적 큐 `sendBufferChunks_`에 재enqueue하는 영구 재활용 설계였다.
2. 프로세스 종료 시:
   - 메인 스레드 TLS `LSendBufferChunk` 소멸자가 deleter를 거쳐 정적 큐에 push.
   - 정적 큐 자신의 소멸자가 보관 원소(shared_ptr)를 파괴 → 각 deleter가 **파괴 중인 큐에
     재진입 enqueue** → UB/크래시. Online 모드에서 패킷을 보낸 뒤 종료하면 재현되는 조건.
3. 부수 문제: `MemoryManager::release()`가 `poolTable_`을 dangling으로 방치,
   `ClientApp::release()`가 `retiredSession_`(static)을 정리하지 않아 WSACleanup 이후
   closesocket·정적 풀 소멸 순서 경합 여지가 있었다.

## 확정된 종료 순서 (main.cpp WM_QUIT)

```
gClose = true
→ INet::ClientApp::release()   // 게임·워커 join → 세션 closeAndDrain(소켓 닫고 잔여 완료 APC 드레인) → 파괴
→ SendBufferManager::release() // TLS 청크 해제 + 정적 큐 drain (신규)
→ MemoryManager::release()     // 풀 delete + poolTable_ null 초기화
→ SocketUtils::release()       // WSACleanup
→ return                       // 이후 static/TLS 소멸자가 풀·큐를 건드리지 않음
```

순서 불변 조건:
- `ServerSession`은 **completion-routine 기반 alertable overlapped I/O**다. 종료 시
  `closeAndDrain()`으로 소켓을 닫아 pending recv/send를 취소시킨 뒤, 취소 완료 APC를
  `SleepEx(alertable)`로 전부 드레인(`pendingIo_ == 0`)한 다음에 세션(=`recvOver_`/`sendOver_`)을
  파괴해야 한다. 드레인 없이 파괴/`WSACleanup`하면 미완료 overlapped와 winsock teardown이 경합해 크래시한다.
- `SendBufferManager::release()`는 **메인 스레드에서**, 모든 워커 스레드 join 이후(= Game 파괴 이후),
  `MemoryManager::release()` 이전에 호출해야 한다. 호출 시점부터 push deleter는 재enqueue 대신
  `odelete`로 실제 해제한다(`shuttingDown_` 플래그).
- `MemoryManager::release()` 이후의 allocate/deallocate는 `ASSERT_CRASH`로 즉시 드러난다.

## 서버 적용 (후속 과제)

ServerEngine 공유 코드 변경이므로 LobbyServer/RoomServer/DummyClient에도 동일한 잠재 문제가 있다.
서버는 보통 강제 종료라 실제로 터지지 않았을 뿐, 정상 종료 경로를 만들 때는 동일하게
`SendBufferManager::release()` → `MemoryManager::release()` 순서를 지켜야 한다.

## 관련 파일

- `client/main.cpp` — WM_QUIT 종료 시퀀스
- `client/ClientApp.hpp` — `release()`에서 `retiredSession_` 포함 정리
- `ServerEngine/SendBuffer.{hpp,cpp}` — `SendBufferManager::release()`, `shuttingDown_`
- `ServerEngine/MemoryManager.cpp` — `poolTable_` null 초기화 + 가드

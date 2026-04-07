[x] 패킷의 유효성 검사
[o] 클라이언트 - 서버 간 이동 동기화
    1. 클라이언트의 move 패킷 전송 주기 -> 우선 간단한 이동 동기화만 구현
        - [o] 걷기 : 50ms 주기(20Hz)
        - [x] 뛰기 : 미정
    
    2. 클라이언트 -> 서버
       1. move 패킷 -> 패킷에 담길 정보의 변동이 필요함. 속도 정보가 포함되어야 다른 플레이어의 애니메이션 업데이트 및 렌더가 가능함. 그리고 무작전 orientation을 패킷에 담는 것이 맞는지도 의문임.
          - position
          - orientation
          - velocity
       2. mouse move 패킷 -> 아직 미구현. 하지만 플레이어가 움직이지 않고 마우스만 움직였을 때 다른 플레이어가 그 현상을 관측할 수 있어야 함.
          - 플레이어의 yaw
    
    3. 서버 검증 - 일단 패스
    4. 서버에서 클라이언트의 플레이어 상태 변경 후 다른 플레이어들에게 정보 브로트캐스트

    * 이동 동기화 리뷰
        - 현재 20Hz 주기로 move 패킷을 보내도록 수정함
        - 내 화면의 다른 플레이어의 위치가 진동하는 현상 수정 (2026-03-30)

          **원인**
          - 원격 플레이어의 보간 계수 `t`를 로컬 플레이서 보간 계수(`tPhysicInterpolation`)와 공유하고 있었음
          - 로컬 물리 스텝이 발생할 때마다 `t`가 줄어들지만, 원격 플레이어는 `proceedPhysicState()`
            호출 대상이 아니므로 `prevPhysicState_.pos`가 갱신되지 않음
          - 결과: 물리 스텝 주기마다 렌더 위치가 이전 네트워크 수신 위치로 되돌아가는 톱니파 진동 발생

          타임라인으로 단계별로 설명할게요.
            ---
            전제
            - 로컬 물리 스텝: 16ms마다 발생
            - 네트워크 패킷: 50ms마다 도착
            - 패킷 도착 → prev = pos_A, curr = pos_B 로 설정됨
            ---
            진동이 발생하는 흐름
            t=0.0   물리 스텝 발생 → physicUpdateAcc_ 리셋 → tPhysicInterpolation = 0
                    (해당 스텝은 예시 : tPhysicInterpolation이 0이 아닐 수 있음. 왜냐하면 physicUpdateAcc_에서 physicUpdateInterval 값만큼 줄어들기 때문.)
                    렌더 위치 = lerp(pos_A, pos_B, 0.0) = pos_A
            t=0.3   렌더 위치 = lerp(pos_A, pos_B, 0.3)  ← pos_A~pos_B 사이 30% 이동 중
            t=0.6   렌더 위치 = lerp(pos_A, pos_B, 0.6)  ← 60% 이동 중
            t=1.0   렌더 위치 = lerp(pos_A, pos_B, 1.0) = pos_B ✓
                    ───── 물리 스텝 발생 ─────
                    physicUpdateAcc_ 리셋 → tPhysicInterpolation 다시 0으로
                    proceedPhysicState() 미호출 → prev 여전히 pos_A, curr 여전히 pos_B
            t=0.0   렌더 위치 = lerp(pos_A, pos_B, 0.0) = pos_A  ← 다시 되돌아감!
            t=0.3   렌더 위치 = lerp(pos_A, pos_B, 0.3)  ← 또 같은 구간 반복
                    ...
            ---
            핵심
            물리 스텝이 발생하면 t는 0으로 리셋되는데, 원격 플레이어는 proceedPhysicState()가 호출되지 않아서 prev가 그대로 남아있습니다.
            로컬 플레이어는 물리 스텝마다 proceedPhysicState()로 prev = curr이 되기 때문에, t가 0으로 리셋돼도 lerp(curr, curr, 0) = curr이라 위치가 유지됩니다.
            원격 플레이어는 이 갱신이 없으니 t가 0으로 리셋될 때마다 위치가 pos_A로 되돌아가고, 다시 pos_B까지 이동하는 것을 반복 → 진동.

          **수정 내용** (`client/object.hpp`, `client/online/onlineGame.cpp`)
          - 이전 수정 내용 -
          - `Player`에 네트워크 보간 전용 타이머 추가 (`netInterpAcc_`, `netInterpDuration_`)
          - 네트워크 패킷 수신 시(`movePlayer`) 타이머 리셋 + `prev`를 현재 렌더 위치(`renderState_.pos`)로 설정
            - 기존에는 `currPhysicState_.pos`(마지막 수신 위치)를 `prev`로 사용했으나,
              패킷이 보간 도중(예: t=0.6)에 도착하면 렌더 위치는 이미 그 60% 지점에 있음
            - 새 보간은 t=0에서 시작하므로, `prev`를 `currPhysicState_.pos`로 쓰면
              렌더 위치가 60% 지점 → 100% 지점으로 순간 점프하는 현상 발생
            - `renderState_.pos`(현재 화면상 위치)를 `prev`로 쓰면 화면상 위치에서 그대로 연속적으로 이어짐
          - `Game::update()`에서 원격 플레이어에 한해 `tPhysicInterpolation` 대신 플레이어별 `tNet` 사용

        - 원격 플레이어가 멈춰도 idle 애니메이션으로 전환되지 않는 현상 수정 (2026-03-30)

          **원인**
          - 원격 플레이어의 애니메이션은 `physicState().evVelocity`의 크기(speed)로 idle/run을 판정함
          - `evVelocity`는 `S_Move` 패킷이 수신될 때만 갱신됨
          - 플레이어가 멈출 때 서버는 velocity=0인 패킷을 1회만 전송함 (velocity가 변할 때만 보내기 때문)
          - 이 패킷이 유실되면 `evVelocity`가 비-영(non-zero) 값을 계속 유지 → run 애니메이션에 고착

          **시도한 접근과 실패 원인**

          시도 1 — `netInterpAcc_ >= netInterpDuration_` 조건으로 `evVelocity`를 0으로 강제:
          - `netInterpDuration_` = 50ms, 패킷 주기도 50ms
          - 60fps 기준 3프레임마다 패킷 도착, acc가 50ms 경계를 반복적으로 넘나들며
            velocity가 0 ↔ non-zero를 빠르게 반복 → idle ↔ run 깜빡임 발생

          시도 2 — 매 프레임 렌더 위치 변화량으로 velocity 유도:
          - `evVelocity = (renderPos_after - renderPos_before) / dt`
          - 보간이 tNet → 1.0으로 수렴하는 마지막 구간에서 pos_delta가 점점 작아짐
          - `tRun = 0` 조건에 걸려 `animTimeRun_ = 0s`(run 애니메이션 타이머)가 리셋됨
          - 직후 다시 움직이면 run 애니메이션이 처음부터 재생 → 순간 튐(pop) 발생
          - 추가로 `evVelocity`를 `update()` 이후에 세팅하므로 애니메이션이 항상 1프레임 지연

          시도 3 — 보간 구간의 implied velocity `(currPos - prevPos) / netInterpDuration_`:
          - 프레임 간 오차는 없으나 `movePlayer()`가 여전히 `evVelocity = packet_velocity`로
            덮어쓰고 있었기 때문에 (복원된 코드 기준) 충돌 발생, 효과 없음

          **최종 수정 내용** (`client/online/onlineGame.cpp`)

          핵심 아이디어: velocity를 매 프레임이 아니라 **패킷 도착 시점에 1회만** 계산한다.
          소스는 서버가 보낸 velocity 필드가 아니라 
          **연속 패킷 간(현재 패킷에 있는 새로운 위치 - current pos(이전에 받았던 위치)) 위치 변화량 ÷ netInterpAcc_(패킷 전송 주기를 판단하는 누적 시간)**이다.

          1. `Game::movePlayer()` — velocity 계산 방식 변경

             ```
             timeSinceLastPacket = netInterpAcc_  // 직전 패킷 이후 경과 시간
             movement            = newPos - physicState().pos  // 직전 패킷 목표 위치 → 새 목표 위치
             evVelocity          = movement / timeSinceLastPacket
             ```

             - `netInterpAcc_`는 `movePlayer()`가 호출되기 직전까지 누적된 값 = 실제 패킷 간격
             - `physicState().pos` (`currPhysicState_.pos`)는 직전 패킷의 목표 위치
             - 두 값으로 서버 측 실제 이동 속도를 역산
             - `timeSinceLastPacket > 0.001f` 조건: 첫 패킷이나 재접속처럼 간격이 의미 없는 경우 0 처리
             - `timeSinceLastPacket <= maxValidInterval(100ms)` 조건: 비정상적으로 긴 공백 후 첫 패킷에서
               과도한 velocity가 계산되는 것을 방지

             Q. movement 계산 시 renderState().pos가 아닌 physicState().pos를 빼는 이유?

             목표는 "서버 기준 실제 이동 속도"를 구하는 것이다.
             - `physicState().pos` = 서버가 직전 패킷에서 알려준 위치 (서버 스냅샷)
             - `renderState().pos` = `lerp(prevPos, currPos, tNet)` 으로 계산된 클라이언트 보간 위치

             `renderState().pos`를 쓰면, 패킷이 어느 tNet 시점에 도착하느냐에 따라 값이 달라진다.

             예) 플레이어가 일정 속도로 A→B→C 이동 중, 패킷이 tNet=0.2에 도착하는 경우와 tNet=0.9에 도착하는 경우:
               - tNet=0.2:  renderPos = lerp(A, B, 0.2) ≈ A  →  movement = C - A ≈ 크다  →  velocity 과대 계산
               - tNet=0.9:  renderPos = lerp(A, B, 0.9) ≈ B  →  movement = C - B ≈ 작다  →  velocity 과소 계산

             같은 속도로 움직이는데도 패킷 도착 타이밍에 따라 velocity가 달라짐 → 애니메이션 불안정.

             `physicState().pos`(= 이전 서버 스냅샷)를 쓰면:
               - 패킷이 언제 도착하든 movement = C - B (서버 스냅샷 간 차이, 항상 동일)
               - velocity = (C - B) / 0.05 → 일관된 값

          2. `Game::update()` 원격 플레이어 루프 — timeout을 **패킷 2개 간격(100ms)**으로 설정

             ```
             if (netInterpAcc_ >= netInterpDuration_ * 2.f) {
                 evVelocity = 0
             }
             ```

             - 100ms 동안 패킷이 없으면 멈춘 것으로 판단 → evVelocity = 0 → idle 전환
             - 1개 간격(50ms)을 쓰면 정상 이동 중에도 패킷 도착 타이밍과 timeout이 겹쳐
               velocity가 0 ↔ non-zero를 반복하는 oscillation이 발생
             - 2개 간격으로 늘리면, 패킷 도착 직후 `netInterpAcc_`는 0으로 리셋되므로
               정상 이동 중에는 threshold에 절대 도달하지 않음 → oscillation 구조적으로 불가

          **동작 타임라인 (60fps / 패킷 50ms 기준)**

          전제
          - 렌더 프레임: 16ms, 패킷 주기: 50ms, netInterpDuration_ = 50ms
          - 패킷 처리는 매 프레임 시작의 SleepEx(1, true)에서 발생 (update 루프보다 먼저)

          [정상 이동 중]
          t=0ms    패킷 A→B 도착 (movePlayer 호출)
                   timeSinceLastPacket = netInterpAcc_ = 50ms
                   evVelocity = (B - A) / 0.05 = 5m/s
                   prevPos = 화면위치, currPos = B, acc = 0
          t=16ms   acc=16ms, tNet=0.32, renderPos = lerp(prev, B, 0.32)  → 애니메이션: 5m/s → run
          t=32ms   acc=32ms, tNet=0.64
          t=48ms   acc=48ms, tNet=0.96
          t=50ms   패킷 B→C 도착  ← acc=48ms, 100ms threshold 전혀 못 미침 → timeout 미발동
                   evVelocity = (C - B) / 0.05 = 5m/s (계속 이동)
                   acc = 0 리셋  (반복)

          [stop 패킷 도착한 경우]
          t=0ms    마지막 이동 패킷 도착, evVelocity = 5m/s
          t=50ms   stop 패킷 도착 (pos=C ≈ B, velocity=0)
                   movement = C - B ≈ 매우 작음
                   evVelocity ≈ 0  → 애니메이션 즉시 idle 전환
                   이후 패킷 없음 (서버는 변화 없으면 안 보냄)

          [stop 패킷 유실된 경우 — 원래 버그 상황]
          t=0ms    마지막 이동 패킷, evVelocity = 5m/s
                   서버가 stop 패킷을 보냈지만 유실
          t=16ms   acc=16ms, 애니메이션: 5m/s → run
          t=50ms   패킷 없음
          t=100ms  acc=100ms >= 50ms * 2 → evVelocity = 0 → 애니메이션: idle (최대 100ms 지연)

          [timeout이 50ms가 아닌 100ms인 이유]
          50ms로 설정하면:
            t=64ms  (패킷이 살짝 늦게 도착)  acc=64ms >= 50ms → evVelocity = 0 (idle)
                    직후 패킷 도착 → evVelocity = 5m/s (run)
                    → 1프레임 깜빡임 발생
          100ms로 설정하면:
            정상 50ms 주기 패킷은 acc가 100ms에 절대 도달하지 않음
            패킷 지터(±10ms)가 있어도 안전 → oscillation 구조적으로 불가

          **수정 파일**
          - `client/online/onlineGame.cpp` — `Game::movePlayer()`, `Game::update()` 원격 플레이어 루프

[x] 각각의 패킷마다 유효성 검사를 할 수 있는 기능 추가
[o] JobTimer 구현
    TimerItem에 들어갈 JobData에 Room의 JobQueue를 어떻게 저장할까 고민을 했음.
    Room의 JobQueue를 담자니 해당 큐를 소유한 Room이 소멸하고나서 예약된 작업을 수행하는 것이 위험함.
    그래서 그냥 Room의 id를 저장하고 RoomManager에게 유효성을 확인받도록 구현함.



[문제 상황과 해결 사례]
2026.03.31 / 화요일
클라이언트 버그
  1. 마우스 회전에 따른 플레이어 회전 패킷과 플레이어 이동 패킷을 분리하면서 send를 2번하게 되면서 문제가 발생함.
      send를 2번하게 될 때의 문제는
        - wsasend에 사용되는 wsaoverlapped(이하 over) 구조체는 send가 완료되기 전까지는 초기화돼서는 안됨.
          하지만 send를 연달아 하게 되면서 send와 send 사이에 over 구조체가 zeromemory 함수에 의해 초기화됨.
    
    해결 방법
      send를 한 번에 몰아서 하자.
      send buffer에 대한 vector를 만들고 send 하고자 할 때 vector에 넣기만 함. 그리고 send를 할 때 한꺼번에 보냄.
      하지만 여기서도 문제가 있음. send buffer vector 하나로만 한다면 네트워크로 보내지고 있는 send buffers의 데이터와 send 하고자 하는 send buffer가 하나의 vector에 공존하기 때문에 충돌이 생김.
      따라서 pending용 vector를 하나 더 만들어서 send 하고자 할 때는 pending용 vector에 담다가 send 시에는 한꺼번에 옮겨야 함.
      하지만 유의할 점은 pending용 vector가 empty임에도 불구하고 제한없이 매 프레임 빈 깡통 데이터를 send하게 되면 마치 서버가 클라로부터 패킷을 받지 못하는 것 처럼 보이는 현상을 보게 될 것임.

      추가적으로, SleepEx -> 여러 패킷 WSASend -> SleepEx -> 여러 패킷 WSASend -> ... 가 현재 클라이언트의 네트워크 통신 시퀀스인데
      완료 처리 후 send가 항상 맞물려서 동작한다면 문제가 없겠지만, 타이밍이 절묘하게 완료 처리가 없을 때 또 다시 send를 하게 되면 이전에 send 했던 상태(over 구조체)가 망가짐.
      따라서 sending이라는 bool 변수를 둬서 아직 send 중인지를 판단함.
  
  2. WSASend에 대한 완료 처리를 할 때, 네트워크로 보내진 send buffer의 메모리 해제 시 프로그램 다운.
      클라이언트 프로그래밍을 강제 종료하면서부터가 사건의 발단이었음. 
      내가 만든 MemoryManager, MemoryPool, odelete에 문제가 있는 것일까, concurrent queue를 잘못 사용한 것일까, 싱글스레드 프로그램에서 멀티스레드 환경에 사용하는 자료구조를 사용한 것이 문제일까를 고민함.
      사실 실제로 문제가 있는 것일지도 모름. 하지만 그래도 나름 제대로 만들었다고 생각하면서 프로그램이 뻗은 곳인 메모리 오염된 send buffer를 유심히 들여다 봄.
      절망적인 상황에 의해 멘탈이 완전히 나가버리고 있는 와중에
      문득 over 구조체가 망가진 게 아닐까? 하는 생각에 프로그램의 호출 스택을 들여다 봄.
      그렇게 발견하게 된 것은 SocketUtils::release 함수가 가장 먼저 호출됐다는 사실임. over 구조체가 포함된 완료 처리가 정상적으로 동작하기 전에 WSACleanup이 호출됨.

    해결 방법
      SocketUtils::release 함수를 ServerSession의 소멸자에 넣으면서 생명 주기에 의존성을 부여함.
      그러는 김에 일관성 있게 SendBufferManager::clear(), MemoryManager::release()도 같이 넣어줌.

      2026.04.02 / 목요일
      그럼에도 문제가 발생하여 gClose라는 전역 bool 변수를 두었음. SocketUtils::release()가 호출되기 이전에 gClose를 true로 바꾸도록 함.
      그리고 Overlapped I/O Callback 함수에서는 gClose가 true인지 항상 체크하도록 수정함.
      WSACleanup이 호출되고 나서는 Callback 함수로 전달되는 overlapped 구조체를 신뢰할 수 없기 때문에 전역 변수를 사용함.

  2026.04.06 / 일요일
  3. 클라이언트 종료 시 ObjectPool<SendBuffer>::push에서 프로그램 다운 (static destruction order fiasco)

    **원인**
    gClose = true 상태에서 completionCallback이 조기 리턴하면, sendOver_.sendBuffers의
    shared_ptr<SendBuffer>가 해제되지 않은 채로 남음.
    WinMain 리턴 후 static 소멸자가 실행될 때 ClientApp::serverSession_과
    ObjectPool<SendBuffer>::pool_ (내부의 ccqueue)의 소멸 순서가 서로 다른 TU(Translation Unit)에 걸쳐
    있어 undefined임. pool_이 먼저 소멸된 뒤 serverSession_ 소멸 중 sendBuffers.clear()가
    호출되어 이미 소멸된 pool_.enqueue()를 호출 → 크래시.

    **해결 방법**
    1. ClientApp::release() 추가: game_, serverSession_을 MemoryManager::release() 전에
       명시적으로 파괴해 소멸 순서를 확정. (client/ClientApp.hpp)
    2. main.cpp WM_QUIT 핸들러에서 INet::ClientApp::release()를 SendBufferManager::release()
       보다 먼저 호출. (client/main.cpp)
    3. ServerSession::~ServerSession()에서 소켓을 닫기 전에 pendingSendBuffers_,
       sendOver_.sendBuffers를 명시적으로 clear해 APC 콜백이 늦게 발생하더라도
       버퍼가 이미 비어있게 함. (client/ServerSession.hpp)

2026.04.02 / 목요일
서버 엔진 버그
  1. Session 클래스의 processSend에서 SendBuffer에 대해 할당받은 메모리 해제 시 프로그램 다운
      SendBuffer*는 SendBufferManager::open()이 반환한 단일 포인터인데, room 서버의 broadcast/broadcastExcept에서 여러 세션의 큐에 그대로 넣음.
      각 세션이 IOCP completion 후 processSend에서 odelete(sendbuffer)를 호출하므로, 두 번째 호출 시점에는 MemoryHeader의 allocSize가 이미 0으로 초기화된 상태임.
    
    해결 방법
      SendBufferManager::open()이 SendBuffer에 대한 raw pointer가 아닌 std::shared_ptr를 반환하도록 수정함으로써 모든 세션이 전송을 완료한 후 한 번만 해제되도록 함.



2026.04.06 / 일요일
클라이언트 버그 (고블린 애니메이션)
  4. 고블린 walk 애니메이션이 재생되지 않는 문제

    **원인**
    `moveGoblin()`에서 서버가 보내는 `velocity` 파라미터를 수신하지만 `physicState().evVelocity`에
    반영하지 않음. `AnimBlenderGoblin::update()`는 `evVelocity`의 크기로 idle/walk를 판정하므로,
    `evVelocity`가 항상 0 → walk 애니메이션이 절대 재생되지 않음.

    **해결 방법** (`client/online/onlineGame.cpp` — `Game::moveGoblin()`)
    ```
    goblin->physicState().evVelocity = DirectX::XMLoadFloat3(&velocity);
    ```

  5. 고블린 idle↔walk 애니메이션 전환이 부자연스러운 문제

    **원인**
    `tWalk_`를 매 프레임 현재 speed에서 직접 계산해 즉시 덮어씀.
    고블린의 `evVelocity`는 17ms 패킷 단위로 0↔3.0이 순간 점프하므로,
    `tWalk_`도 0↔~0.98을 순간 전환 → 애니메이션이 즉시 튀는 현상 발생.

    **해결 방법** (`client/object.cpp` — `AnimBlenderGoblin::update()`)
    목표값을 구한 뒤 지수 감쇠로 부드럽게 보간:
    ```
    targetTWalk = clamp((speed - rangeStart) / (rangeEnd - rangeStart), 0, 1)
    tWalk_ += (targetTWalk - tWalk_) * (1 - exp(-dt / 0.12))
    ```
    시상수 0.12s → idle↔walk 전환이 ~250ms에 걸쳐 자연스럽게 이루어짐.

** 서버 엔진쪽 코드에서 모든 register... 함수에서 send event의 setOwner를 그때마다 해줘야할까? 그냥 한 번 등록하면 되는 거 아닌가?
** 추후 room을 id + generation 정보로 관리해야 하지 않을까 라는 생각이 들었다. id를 재사용한다는 점에서 그런 생각이 들었다. 이러면 room, room manager의 전체적인 구조에 변경이 필요하다.
** BVH -> collision 책임자
** 고블린 걷기 애니메이션 안되어있는 거 처리함. 다른 몬스터도 안되어 있어서 처리해야 함.
** 몬스터 ai를 크게 전체, 그룹, 개인으로 세분화해서 구현이 목표
  - 고블린 이동할 때 진동하는 현상은 아직 미해결.
  - 애니메이션 블렌딩 부자연스러운 문제 해결됨 (항목 5 참고).
** TU : C++에서 단일 .cpp 파일과 그것이 #include하는 헤더들을 합친 컴파일 단위를 말해.
** <chrono>에 대해서 system clock, steady clock, high resolution clock의 모든 time point는 현재 시간을 알려줌.
    time point의 차는 duration임. 기본적으로 duration은 정수값임.
    하지만
    using Nanoseconds = std::chrono::duration<float, std::nano>;
    using Microseconds = std::chrono::duration<float, std::micro>;
    using Milliseconds = std::chrono::duration<float, std::milli>;
    using Seconds = std::chrono::duration<float>;
    float값으로 바꿀 수 있음. 이렇게 했을 때 duration은 정수값 2개를 분모, 분자로 저장을 해서 분자/분모로 값을 계산함.
    time_point = 시점, duration = 시간 / 시점과 시간은 비교 불가능, 시점과 시간 덧셈-뺄셈 가능 ex) 시점 + 시간 = 시점

    2026.04.07 / 화요일
    시간 정밀도 개선에서 생긴 문제와 해결
    [문제]
    JobTimer의 addJob에서 생긴 문제임.
    JobTimer에서 TimeItem 중 executionTime의 타입을 Milliseconds( std::chrono::duration<float, std::milli> )와 더하기 위해
    HighResolutionClockTimePoint( std::chrono::time_point<HighResolutionClock, Nanoseconds> )
    해당 타입으로 했음. using Milliseconds = std::chrono::duration<float, std::milli>; 해당 타입과 더하기 위해서 
  
[Room JobQueue 동작 구조]

핵심 구성요소 3가지

┌──────────────────────────┬─────────────────────────────────────────────────────────────┐
│         구성요소          │                           역할                               │
├──────────────────────────┼─────────────────────────────────────────────────────────────┤
│ Room::jobQueue_          │ Room별로 1개씩 있는 작업 큐                                    │
├──────────────────────────┼─────────────────────────────────────────────────────────────┤
│ JobQueuePool             │ 전역 lock-free 큐. 실행 대기 중인 JobQueue* 포인터들을 담음      │
├──────────────────────────┼─────────────────────────────────────────────────────────────┤
│ LJobQueue (thread-local) │ 현재 스레드가 실행 중인 JobQueue를 가리킴                       │
└──────────────────────────┴─────────────────────────────────────────────────────────────┘

---
doAsync 호출부터 실행까지

room->doAsync(&Room::move, ...)
    └─ jobQueue_.push(job)
            ├─ jobCount_.fetch_add(1)  ← atomic 카운터 증가
            ├─ queue_.enqueue(job)
            └─ prevCnt == 0 이었다면? (첫 번째 job → 실행 담당자 결정)
                  ├─ LJobQueue == nullptr (현 스레드 여유있음)
                  │       └─ execute() 직접 실행
                  └─ LJobQueue != nullptr (현 스레드 이미 다른 큐 실행 중)
                          └─ JobQueuePool::push(this)  ← 다른 스레드에게 위임

핵심 보장: prevCnt == 0인 순간 하나의 스레드만 실행 권한을 획득 → 동시에 두 스레드가 같은 Room 큐를 실행하는 일이 없음

---
Worker 스레드 루프 (DoWork)

매 64ms 슬롯마다:
  1. reactor.dispatch(10)     ← IOCP 이벤트 처리 (패킷 수신 → room->doAsync 호출)
  2. DoReservedJob()          ← 타이머 예약 job 분배
  3. DoJob()                  ← JobQueuePool에서 큐 꺼내 execute()

DoJob() 내부:
while (now < LEndTick) {
    JobQueue* jq = JobQueuePool::pop();  // 대기 중인 Room 큐 하나 꺼냄
    jq->execute();                        // 해당 Room의 job들 실행
}

---
execute() 내부의 "바통 넘기기"

execute() {
    LJobQueue = this;  ← "나 지금 이 큐 실행 중"
    loop {
        최대 100개 bulk dequeue & 실행
        jobCount 0 되면 → LJobQueue = nullptr, 종료
        LEndTick 초과 → LJobQueue = nullptr
                          JobQueuePool::push(this)  ← 남은 작업 다른 스레드에 넘김
    }
}

---
여러 Room 큐가 서로 맞물리는 시나리오

Thread A: IOCP 이벤트 처리 중 (LJobQueue = nullptr)
  → Room1.doAsync() 호출 → prevCnt==0 → 직접 execute()
  → 실행 중 Room2.doAsync() 호출 (LJobQueue = Room1의 큐)
      → prevCnt==0 → LJobQueue != nullptr → JobQueuePool에 Room2 큐 push

Thread B: DoJob() 루프
  → JobQueuePool::pop() → Room2 큐 획득 → execute()

- Room 간 격리: 각 Room 큐는 독립적으로 실행됨, Room 내부는 단일 스레드처럼 동작
- 시간 공정성: 64ms 예산 초과 시 남은 작업을 Pool에 돌려놓아 다른 Room이 굶지 않도록 함
- Lock-free: jobCount_ atomic + moodycamel concurrentqueue로 뮤텍스 없이 동작


---

[x] 서버 공격 시스템 구현 (2026.04.07)

## 개요

StandAlone 모드에만 존재하던 `CombatSystem`의 핵심 로직을 서버 권한으로 이식함.
플레이어 공격 / 고블린 공격 / 피격 결과 브로드캐스트 / 시계 동기화(지연 보상)를 포함.

---

## 추가 패킷 (ServerEngine/protocol.hpp)

| 패킷 | 방향 | 필드 | 설명 |
|------|------|------|------|
| `C_Attack` | Client→Server | `uint64 clientMs` | 플레이어 공격 발동 |
| `S_Hit` | Server→Client | `uint16 targetId`, `int32 newHp` | 피격 대상 HP 변경 알림 |
| `S_NpcAttack` | Server→Client | `uint16 npcId` | 고블린 공격 발동 (클라이언트 애니메이션 트리거용) |
| `S_TimeSync` | Server→Client | `uint64 serverMs` | 입장 직후 1회 전송, 시계 동기화 |

---

## 고블린 공격 (RoomServer/object)

- `Goblin`에 `attackCooldown_`(Seconds), `kAttackCooldownMax_(2s)`, `kAttackDamage_(15)` 추가
- `update()` 반환 타입을 `GoblinUpdateResult`로 변경
  - `velocity` + `std::optional<HitInfo>{ targetId, newHp }`
- Attack 상태에서 쿨다운 차감 → 만료 시 가장 가까운 플레이어 HP 차감 + `result.hit` 설정
- `hp() <= 0`이면 early return (사망한 고블린은 AI 비활성)

---

## 플레이어 공격 (RoomServer/Room)

- `Room::attack(sessionId, clientMs)` 추가
- `buildAttackAABB(player.pos, player.forward, {1.5, 1.5, 1.5}, 1.0f)` 로 hitbox 생성
- 고블린마다 근사 AABB `{ rewindPos(targetMs), {1.0, 2.0, 1.0} }` 와 `collides(AABB, AABB)` 로 판정
  - 고블린 서버 모델에 BVH 미적재 → 근사 AABB 사용
  - `rewindPos(targetMs)` : 지연 보상된 위치 사용 (아래 참고)
- 히트 시 HP 차감 + `S_Hit` 브로드캐스트
- 공격 파라미터 (standalone CombatConfig와 일치): halfExtent=1.5, offsetFwd=1.0, damage=30

---

## 지연 보상 (Lag Compensation)

**문제**: 클라이언트가 LButton을 누른 시점(T)과 C_Attack이 서버에 도달하는 시점(T + D) 사이에
고블린이 이동. 서버의 현재 고블린 위치로 판정하면 클라이언트 화면과 불일치.

**방법**: 클라이언트가 공격 시점의 서버 시계 추정값을 `clientMs`로 전송 → 서버가 그 시점으로 되감기.

### 시계 동기화 원리

```
서버: S_TimeSync(Ts) ─── D(단방향 지연) ───→ 클라이언트(L_recv)

클라이언트 저장: clockOffset = Ts - nowMs()
공격 시 전송:   clientMs = nowMs() + clockOffset
              = Ts + (L_atk - L_recv)
              ≈ 서버 시계 기준 공격 시점 - D

서버 수신 시각 = Ts + (L_atk - L_recv) + D
rewindTarget  = clientMs = 서버 수신 시각 - D  ← D만큼 이전 스냅샷 사용

(nowMs = high_resolution_clock::now().time_since_epoch() 를 ms uint64로 변환한 값.
 GetTickCount64() 대신 chrono 기반으로 교체됨 - 2026.04.07)
```

RTT 측정 없이 단방향 지연이 자동으로 보상됨.
S_TimeSync를 받기 전(초기값 clockOffset=0)에는 보상 없이 동작 (LAN 환경에서는 무시 가능한 오차).

### 위치 히스토리 링 버퍼 (RoomServer/object)

```cpp
struct PosSnapshot { uint64 serverMs; mu::Vec3 pos; };
static constexpr int kHistorySize_ = 16;   // 17ms × 16 ≈ 272ms
```

- `updateGoblinAI()`에서 매 틱 `recordSnapshot(nowMs())` 호출 (chrono 기반, GetTickCount64() 교체됨)
- `rewindPos(targetMs)`: 역순 탐색으로 targetMs 이하인 가장 최신 스냅샷 반환,
  전체가 최신이면 가장 오래된 항목으로 클램프

---

## 클라이언트 (online 모드)

- `processInputGame()` — LButton 클릭(edge detection) 시 `sendAttackPacket()` 호출
- `sendAttackPacket()` — `nowMs() + serverClockOffset_`을 `clientMs`로 C_Attack 전송
- `applyTimeSync(serverMs)` — `serverClockOffset_ = serverMs - nowMs()` 갱신
  (nowMs = `high_resolution_clock::now().time_since_epoch()` ms uint64. GetTickCount64() 교체됨 - 2026.04.07)
- `applyHit(targetId, newHp)` — 내 플레이어/타 플레이어/고블린 HP 업데이트, 내 HP=0이면 `playerDead_=true`
- `onNpcAttack(npcId)` — `EvAttack` 이벤트 발생 → 고블린 공격 애니메이션 트리거

---

[x] 플레이어 / 고블린 죽음 처리 구현 (2026.04.07)

## 개요

`S_Hit(newHp=0)` 수신 시 내 플레이어 사망만 처리되던 구조에서,
원격 플레이어·고블린의 사망 애니메이션 재생 및 이후 이동 패킷 무시 기능을 추가함.
서버 측에서는 죽은 고블린에 대해 `S_NpcMove`를 계속 브로드캐스트하던 문제도 수정함.

---

## 설계 결정 — 왜 EvDeath를 쓰지 않았나

standalone 모드에서는 `holdEvent(eventList_, EvDeath(id))` → 이벤트 루프에서 처리 → `AnimBlender::dead_ = true` 패턴을 사용함.
그러나 online 모드의 `Game::update()`에는 이벤트를 처리하는 루프가 없고, `clearEvents(eventList_)`로 그냥 비워버림.
→ EvDeath를 쓸 수 없으므로, `AnimBlender` 기반 클래스에 `virtual void triggerDeath()` 가상 함수를 추가하는 방식으로 해결.

---

## 변경 내용

### `client/animation.hpp` — AnimBlender 기반 클래스

```cpp
virtual void triggerDeath() {}  // 기본 구현은 아무것도 안 함
```

### `client/object.hpp` — AnimBlenderPlayer, AnimBlenderGoblin

두 클래스에 `triggerDeath()` override 추가:
```cpp
void triggerDeath() override {
    animTimeDeath_ = 0s;
    cooldownDeath_ = 200ms;
    dead_ = true;
}
```
- `animTimeDeath_ = 0s` : 사망 애니메이션 타이머 초기화
- `cooldownDeath_ = 200ms` : 300ms 페이드인 구간 시작 (기존 EventBus::receive와 동일 값)
- `dead_ = true` : 사망 상태 플래그 → 이후 `update()`에서 사망 애니메이션 블렌딩 시작

나머지 AnimBlender 서브클래스(Anubis, Bat 등) — 온라인 모드에서 사용되지 않으므로 변경 불필요.

### `client/object.hpp` — Object 기반 클래스

`isDead_` 필드 및 접근자 추가:
```cpp
void setDead(bool dead) {
    isDead_ = dead;
    if (dead && renderState_.animBlender) {
        renderState_.animBlender->triggerDeath();
    }
}
bool isDead() const { return isDead_; }

// protected:
bool isDead_ = false;
```
- `setDead(true)` 하나로 논리 상태(`isDead_`)와 애니메이션 트리거(`triggerDeath()`)가 동시에 처리됨

### `client/online/onlineGame.cpp` — applyHit

`newHp <= 0` 시 원격 플레이어·고블린에 대해 `setDead(true)` 호출:
```cpp
if ( auto it = idPlayerMap_.find( targetId ); it != idPlayerMap_.end() ) {
    it->second->setHp( newHp );
    if ( newHp <= 0 ) { it->second->setDead( true ); }
    return;
}
if ( auto it = idGoblinMap_.find( targetId ); it != idGoblinMap_.end() ) {
    it->second->setHp( newHp );
    if ( newHp <= 0 ) { it->second->setDead( true ); }
}
```

### `client/online/onlineGame.cpp` — movePlayer / rotatePlayer / moveGoblin

죽은 오브젝트로 들어오는 in-flight 이동 패킷 무시:
```cpp
if (obj->isDead()) return;
```
각 함수의 `nullptr` 체크 직후에 추가.

### `RoomServer/Room.cpp` — updateGoblinAI

죽은 고블린(`hp() <= 0`)에 대한 `S_NpcMove` 브로드캐스트 생략:
```cpp
if (goblin.hp() > 0) {
    broadcast(PacketManager::makeSNpcMovePacket(...));
}
```

---

## 미구현 (TODO)

- 내 플레이어가 죽었을 때 사망 애니메이션 재생 (현재는 `playerDead_ = true`만 세팅, triggerDeath 미호출)
  → `applyHit`에서 내 플레이어 분기에도 `player_->setDead(true)` 추가 필요
- 사망 후 일정 시간이 지나면 오브젝트 페이드아웃 또는 제거
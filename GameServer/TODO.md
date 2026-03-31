[x] 패킷의 유효성 검사
[x] 클라이언트 - 서버 간 이동 동기화
    1. 클라이언트의 move 패킷 전송 주기 -> 우선 간단한 이동 동기화만 구현
        - [o] 걷기 : 50ms 주기(20Hz)
        - [x] 뛰기 : 미정
    
    2. 클라이언트 -> 서버
       1. move 패킷 -> 패킷에 담길 정보의 변동이 필요함. 속도 정보가 포함되어야 다른 플레이어의 애니메이션 업데이트 및 렌더가 가능함. 그리고 무작전 orientation을 패킷에 담는 것이 맞는지도 의문임.
          - position
          - orientation
          - velocity
       2. mouse move 패킷 -> 아직 미구현. 하지만 플레이어가 움직이지 않고 마우스만 움직였을 때 다른 플레이어가 그 현상을 관측할 수 있어야 함.
          - 미정
    
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
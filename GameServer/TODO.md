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
        - 속도 정보가 클라이언트의 프레임에 맞춰서 보내지는 것이 아니기 때문에 내 화면의 다른 플레이어의 애니메이션이 불안정함 -> 수정해야 할 부분
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
          - `Player`에 네트워크 보간 전용 타이머 추가 (`netInterpAcc_`, `netInterpDuration_`)
          - 네트워크 패킷 수신 시(`movePlayer`) 타이머 리셋 + `prev`를 현재 렌더 위치(`renderState_.pos`)로 설정
            - 기존에는 `currPhysicState_.pos`(마지막 수신 위치)를 `prev`로 사용했으나,
              패킷이 보간 도중(예: t=0.6)에 도착하면 렌더 위치는 이미 그 60% 지점에 있음
            - 새 보간은 t=0에서 시작하므로, `prev`를 `currPhysicState_.pos`로 쓰면
              렌더 위치가 60% 지점 → 100% 지점으로 순간 점프하는 현상 발생
            - `renderState_.pos`(현재 화면상 위치)를 `prev`로 쓰면 화면상 위치에서 그대로 연속적으로 이어짐
          - `Game::update()`에서 원격 플레이어에 한해 `tPhysicInterpolation` 대신 플레이어별 `tNet` 사용

[x] 각각의 패킷마다 유효성 검사를 할 수 있는 기능 추가
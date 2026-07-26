# Room 틱 케이던스 수정 — 서버-클라 피격 판정 동기화

**작업일**: 2026-07-26 · **브랜치**: `client`
**변경 파일**: `RoomServer/JobTimer.{hpp,cpp}`, `RoomServer/Room.{hpp,cpp}`

---

## 1. 한 줄 요약

`Room::update()`가 **자신의 처리 시간을 매 틱 주기에 누적시키는 방식**으로 다음 틱을 예약하고 있어
서버 시뮬레이션이 실시간의 약 0.83배로 흐르고 있었다. 그 결과 서버 스킬 타임라인이 클라이언트보다
느리게 진행해 히트박스가 최대 300ms 늦게 생성됐고, 이것이 피격 판정 불일치의 주원인이었다.
틱 예약을 **절대 데드라인** 방식으로 바꿔 해결했다.

---

## 2. 증상

온라인 모드에서 서버(권위 판정)와 클라(예측)의 히트 결과가 양방향으로 어긋났다.

- 서버는 맞았다고 판단, 클라는 안 맞았다고 예측 / 그 반대
- **정지한 타겟에서도** 발생 (→ 위치 동기화 문제가 아님을 시사)
- 육안 관찰: **NPC 공격의 서버 히트박스가 클라보다 200~300ms 늦게 생성**됨.
  반면 플레이어 공격은 클라/서버 히트박스가 잘 맞아떨어짐

---

## 3. 진단 근거

### 3.1 측정 데이터 (Release 빌드, 교전 중)

```
[TickDrift] sim=10000ms wall=11882ms drift=18.82% | periodMs=19.8033 cpuMs=3.83226 queueMs=-0.029
[TickDrift] sim=20000ms wall=24104ms drift=20.52% | periodMs=20.0867 cpuMs=4.09700 queueMs=-0.010
```

틱 주기가 완벽히 분해된다:

| 항목 | 값 | 의미 |
|---|---|---|
| 예약 delay | 16.00ms | `duration_cast<milliseconds>(16.667ms)` 절삭 결과 |
| `update()` CPU | 3.83~4.10ms | 실제 시뮬레이션 처리 시간 |
| 큐/타이머 지연 | **≈ 0ms** | JobTimer·JobQueue는 결백 |
| **실측 주기** | **19.80~20.09ms** | 목표 16.667ms 대비 +19~20% |

→ 서버 실제 틱레이트 **약 50Hz** (목표 60Hz), 시뮬 클럭 = 실시간의 **0.83~0.84배**.

클라이언트 측 독립 교차검증: NPC 이동 패킷은 서버 틱당 1회 전송이므로 수신 간격이 곧 틱 주기다.
클라 실측 수신 간격 **19.5~20.7ms** — 서버 `periodMs`와 일치.

### 3.2 증상과의 정량적 일치

스킬 타임라인은 `inst.elapsed += 16.667ms`로 진행하는데 틱이 20ms마다 오므로,
**인스턴스 시각 t의 히트는 실제로는 t × 1.20 시점에 발생**한다. 지연 = 0.20 × t:

| 스킬 | 히트 인스턴스 시각 | 실시간 지연 | 관찰과 대조 |
|---|---|---|---|
| NPC 공격 (Goblin/Mushroom_Attack) | 780~1300ms | **150~270ms** | "200~300ms 늦음" ✓ |
| 플레이어 SwordSlash | 206~325ms | 41~65ms | 감지 불가 → "잘 맞음" ✓ |

플레이어 공격이 멀쩡해 보인 이유는 히트박스가 옳아서가 아니라 **타임라인이 짧아 절대 오차가 작았을 뿐**이다.
정지 타겟에서도 어긋난 것도 이것으로 설명된다 — 위치가 아니라 순수 시간 문제.

### 3.3 히트박스 자체는 정상이었다

같은 캐스트(seed)에 대해 클라/서버가 기록한 히트 인스턴스 시각과 타겟 집합이 거의 일치했다.
예: `SwordSlash seed=992440858`은 양측이 **동일한 12개 타겟**을 동일 시각대(211/213, 249/246,
272/263, 292/296ms)에 히트. 즉 **히트박스 기하와 타임라인은 옳고, 타임라인을 걷는 속도만 틀렸다.**

### 3.4 기각된 가설 (같은 길을 다시 가지 않도록)

| 가설 | 판정 | 근거 |
|---|---|---|
| 캐스트 fast-forward 구간에서 히트 윈도우 소실 | **기각(로컬)** | 실측 `ffwdMs` 6~13ms에 불과, 해당 구간에 히트박스 이벤트 0건. **단 고핑 환경에선 잠재 리스크로 남음** (§7) |
| 타겟 위치 지연 / 네트워크 보간 | **주범 아님** | 몬스터 이동 델타 0.0009~0.018(사실상 정지)인데도 불일치 발생 |
| 서버 애니메이션 phase 불일치 | **부차적** | 서버 애니도 같은 느린 클럭으로 재생되므로 본 수정으로 함께 개선됨 |

---

## 4. 근본 원인 (코드 2곳)

### 4.1 상대 지연 재예약 — 처리 시간이 매 틱 누적 (주원인)

```cpp
// Room::update() 말미 — 수정 전
doTimer(dt, [this]() { update(); });   // "지금부터 16.667ms 뒤"
```

`doTimer`는 **`update()`가 모두 끝난 시점**에 호출된다. 따라서

```
실제 주기 = update() 처리 시간 + dt
```

가 되어, 처리 시간(4ms)만큼 매 틱 영구히 밀린다. 부하가 커질수록 서버는 더 느려진다.

### 4.2 `duration_cast` 절삭

```cpp
// JobTimer::addJob — 수정 전
const auto executionTime = now + std::chrono::duration_cast<std::chrono::milliseconds>(delay);
```

`Milliseconds`는 `duration<float, milli>`이므로 `1s/60.f` = **16.667ms → 16ms로 절삭**됐다.
이 절삭은 반대 방향(-4%)이라 4.1의 드리프트를 **우연히 일부 상쇄**하고 있었다.
4.1만 고치고 이걸 두면 이번엔 반대 방향 오차(서버가 실시간보다 빠름)로 드러난다. **둘은 함께 고쳐야 한다.**

---

## 5. 수정 내용

### 5.1 `JobTimer` — 절대 시각 예약 API 추가

```cpp
// JobTimer.hpp
static void addJob(Milliseconds delay, uint32 roomId, Job* job);
// 절대 시각 예약. 고정 주기 반복(룸 틱)은 반드시 이쪽을 쓴다.
static void addJobAt(HighResolutionClock::time_point executionTime, uint32 roomId, Job* job);
```

```cpp
// JobTimer.cpp
void JobTimer::addJobAt(HighResolutionClock::time_point executionTime, uint32 roomId, Job* job) {
    auto jobData = ObjectPool<JobData>::pop(roomId, job);
    std::lock_guard<std::mutex> lock(jobTimerMtx_);
    timerQueue_.push({executionTime, jobData});
}

void JobTimer::addJob(Milliseconds delay, uint32 roomId, Job* job) {
    // 클럭 native duration(ns)으로 변환 — milliseconds 절삭 제거
    addJobAt(HighResolutionClock::now()
             + std::chrono::duration_cast<HighResolutionClock::duration>(delay),
             roomId, job);
}
```

기존 `addJob`은 `addJobAt` 위에 재구현했다. 절삭 제거는 **모든 반복 타이머 사용처에 이득**이며
호출부 변경은 필요 없다.

### 5.2 `Room` — 절대 데드라인 케이던스

```cpp
// Room.hpp
void doTimerAt(HighResolutionClock::time_point executionTime, CallbackType&& callback);
...
HighResolutionClock::time_point nextTickTime_{};   // 다음 룸 틱의 절대 데드라인
```

```cpp
// Room::update() 말미 — 수정 후
static constexpr auto kTickPeriod = std::chrono::duration_cast<HighResolutionClock::duration>(dt);
static constexpr auto kMaxCatchUp = kTickPeriod * 3;

const auto tickNow = HighResolutionClock::now();
if (nextTickTime_.time_since_epoch().count() == 0)
    nextTickTime_ = tickNow;              // 첫 틱: 케이던스 기준점 확립
nextTickTime_ += kTickPeriod;
if (nextTickTime_ + kMaxCatchUp < tickNow)
    nextTickTime_ = tickNow;              // 과부하: 따라잡기 포기하고 재동기화

doTimerAt(nextTickTime_, [this]() { update(); });
```

---

## 6. 설계 결정과 근거

### D1. 왜 절대 데드라인인가 — catch-up accumulator를 쓰지 않은 이유

측정에서 `cpuMs ≈ 4ms`, 즉 **16.667ms 예산의 24%만 사용**하고 있었다. 서버는 CPU 바운드가 아니다.
따라서 데드라인을 절대 시각으로 고정하기만 하면 4ms 작업이 예산 안에 그대로 들어가고 주기는 정확히
16.667ms가 된다. 별도의 accumulator + 다중 스텝 catch-up 루프는 불필요하며,
그쪽은 **한 번의 타이머 발화에서 여러 틱을 연속 실행 → 브로드캐스트 버스트 → 부하 증가 → 더 밀림**이라는
나선 위험이 있다.

### D2. catch-up은 "공짜"로 얻는다

`nextTickTime_`이 이미 과거가 되면 `JobTimer::distribute()`가 다음 폴링에서 **즉시** 디스패치한다.
즉 짧은 hitch는 별도 코드 없이 자동으로 따라잡힌다. 이 성질 덕분에 D1의 단순함과
"hitch 후 시뮬 시간 손실 없음"을 동시에 얻는다.

### D3. 왜 상한 3틱(50ms)에서 리싱크하는가

D2의 자동 catch-up에는 브레이크가 필요하다. 과부하가 지속되면 데드라인이 계속 과거로 밀려
백로그가 무한정 쌓이고, 부하가 걷히는 순간 수백 틱이 연속 실행되며 브로드캐스트 폭풍이 발생한다.
3틱을 넘게 밀리면 **백로그를 포기하고 현재 시각을 새 기준점으로 삼는다**.
트레이드오프: 그만큼의 시뮬 시간은 영구 손실이지만, 버스트로 서버 전체가 무너지는 것보다 낫다.
50ms는 "체감 가능한 hitch"의 하한이자 클라 네트워크 보간 구간(50ms)과 같은 스케일로 잡았다.

### D4. 왜 `kTickPeriod`를 `dt`에서 파생시키는가

```cpp
static constexpr auto kTickPeriod = std::chrono::duration_cast<HighResolutionClock::duration>(dt);
```

`dt`는 물리·AI·스킬 타임라인에 전달되는 시뮬레이션 스텝이다. 스케줄링 주기를 여기서 파생시키면
**"시뮬 1스텝 = 실시간 1주기"라는 불변식이 코드로 강제**된다. 별도 상수로 두면 한쪽만 바뀔 때
드리프트가 조용히 재발한다 — 이번 버그의 본질이 정확히 그것이었다.

### D5. 왜 `addJob`을 지우지 않고 남겼는가

일회성 지연 작업(리스폰 등)에는 상대 지연이 자연스러운 표현이다. `addJob`을 `addJobAt` 위에
재구현해 절삭 문제만 제거하고 API는 보존했다. **고정 주기 반복만 `addJobAt`을 쓰면 된다** —
이 규칙을 두 함수의 주석에 명시해뒀다.

---

## 7. 부작용과 후속 조치 (중요)

지금까지 **서버 전체가 0.83배 슬로모션**이었으므로, 수정 후 모든 것이 제 속도로 돌아온다.

1. **게임플레이 튜닝값 재검토 필요.** 물리·NPC 이동속도·AI 상태 타이밍·스킬 지속시간이 모두
   ~20% 빨라진다(정확히는 *저작한 값대로* 돌아온다). 느린 클럭 기준으로 감각 튜닝된 수치가 있다면
   빠르게 느껴질 수 있다.
2. **NPC 이동 브로드캐스트 대역폭 +20%.** 50Hz → 60Hz. `S_NpcMoveBatch`는 현재 매 틱 무조건
   전송하는데(스로틀·델타 압축 없음), 클라 네트워크 보간 구간은 50ms(20Hz) 기준이다.
   **20Hz 스로틀로 낮추면 대역폭 1/3 + 클라 보간 상수와 정합** — 권장 후속 과제.
3. **스킬 쿨타임은 자동 정상화.** `elapsedMs_`가 같은 틱 클럭을 누적하므로 별도 수정 불필요.
   (수정 전에는 서버 쿨타임이 실시간 기준 20% 길어 클라 예측이 통과시킨 시전을 서버가 거부할 수 있었다.)
4. **고핑 환경의 fast-forward 리스크는 미해결.** `SkillSystem::startSkill`은 `[0, elapsedMs]`
   (최대 250ms) 구간의 타임라인 이벤트를 **충돌 검사 없이** 즉시 디스패치한다. 히트박스의
   Spawn과 Destroy가 모두 이 구간에 들어가는 짧은 히트 윈도우는 서버에서 판정이 0회가 되어
   **확정 미스**가 된다. 로컬 테스트(ffwd 6~13ms)에서는 발현하지 않았으나 실측 핑 환경에서 재검증 필요.
   해결안: fast-forward 중 스폰된 히트박스에 1틱 유예를 부여하거나, 구간을 스텝 시뮬레이션.

---

## 8. 검증 방법

### 8.1 정량 검증

서버에 틱 드리프트 로그를 임시로 넣고(§9 참조) 30초 이상 교전한다. 기대값:

- `drift ≈ 0%`, 실측 주기 ≈ **16.67ms**
- 클라 측 NPC 이동 패킷 수신 간격 ≈ **16.7ms** (수정 전 20.4ms)

### 8.2 육안 검증 — 히트박스 오버레이

서버 권위 히트박스를 클라 화면에 그려 클라 예측 히트박스와 직접 비교할 수 있다.
**두 토글 모두 코드에 남아 있으며 기본 off다.**

| 토글 | 위치 | 색 |
|---|---|---|
| `kBroadcastDebugHitboxes` | `RoomServer/Room.cpp` (`updateSkillSystem` 부근) | **빨강** = 서버 권위 |
| `kDebugSkillHitboxOverlay` | `client/online/onlineGame.cpp` (`skillSystem_.update` 직후) | **초록** = 클라 예측 |

둘 다 `true`로 바꾸고 각각 리빌드하면 두 색이 겹쳐 렌더된다. 프로토콜(`S_DebugHitbox`)과
클라 수신·렌더 경로(`debugBVView_`)는 상시 존재하므로 추가 배선은 필요 없다.
**서버 플래그는 매 프레임 alloc + serialize + 전체 브로드캐스트를 하므로 프로덕션에서는 반드시 off.**

검증 시나리오: 비어그로 몬스터 앞 **근접 스킬 최대 사거리 가장자리**에서 시전 —
경계 케이스라 미세한 타이밍/포즈 차이가 히트/미스로 증폭되어 드러난다.

---

## 9. 재진단이 필요할 때

이번 진단에 사용한 로그는 원인 확정 후 전량 제거했다. 재현이 필요하면 아래를 임시로 다시 넣으면 된다.

| 로그 | 위치 | 측정 대상 |
|---|---|---|
| 틱 드리프트 | `Room::update` 말미 | 누적 틱 수 vs wall-clock, `update()` 본문 CPU 시간 |
| 캐스트 fast-forward | `Room::skillStart` | `elapsedMs`(ffwd 크기), 타임스탬프 유효성 |
| fast-forward 스킵 이벤트 | `SkillSystem::startSkill` 루프 | ffwd 구간에 삼켜진 Spawn/Destroy (§7-4 검증용) |
| 서버 히트 | `SkillSystem::processHitResults` | seed·target·인스턴스 시각 |
| 클라 히트 | `client/skill/skillSystem.cpp processHitResults` | 위와 동일 — **seed로 조인해 건별 대조** |
| NPC 이동 수신 간격 | 클라 `Game::moveGoblin` | 서버 실제 틱 주기의 클라측 관측값 |

**핵심 기법**: 캐스트별 `skillSeed`가 클라/서버에 공유되므로, 양쪽 히트 로그를 seed로 조인하면
불일치 건을 하나씩 짚어낼 수 있다. 인스턴스 시각(`inst.elapsed`)까지 함께 찍으면
"기하 문제"인지 "타이밍 문제"인지 즉시 갈린다 — 시각이 일치하는데 결과가 다르면 기하/위치,
시각 자체가 밀리면 클럭 문제다.

---

## 10. 관련 문서

- `RoomServer/docs/serverHandoff.md` — **서버 담당 전달 사항**(이 수정 + id 수명주기 수정의
  통합 요약, 새 규약, 우선순위별 남은 과제). §7의 후속 조치는 그쪽에 P1~P3으로 정리돼 있다.
- `RoomServer/docs/objectIdLifecycle.md` — 오브젝트 id 발급·반납 규약과 잠복 UAF
- `RoomServer/docs/skillArchitecture.md` — 스킬 타임라인·히트박스 생명주기
- `RoomServer/docs/serverArchitecture.md` — Room 업데이트 루프 전체
- `client/docs/particleHitboxDeterminism.md` — `skillSeed` 결정론 계약

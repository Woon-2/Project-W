# 서버 전달 사항 — 2026-07-26

**브랜치**: `client` · **수신자**: 서버 프로그래머

클라 작업 중에 **서버 측 결함 두 건**을 찾아 수정했다. 둘 다 증상은 클라에서 보였지만 원인은
서버에 있었고, 서버 코드(`ServerEngine`, `RoomServer`)를 직접 건드렸으므로 인지가 필요하다.
이 문서는 **바뀐 것 / 앞으로 지켜야 할 규약 / 남은 과제**만 다룬다.
진단 과정과 근거는 아래 두 문서에 있다.

- `RoomServer/docs/roomTickCadence.md` — Room 틱 케이던스 (피격 판정 동기화)
- `RoomServer/docs/objectIdLifecycle.md` — 오브젝트 id 수명주기 (스킬 VFX 소실)

---

## 1. 한 눈에

| # | 문제 | 원인 위치 | 상태 |
|---|---|---|---|
| A | 서버 시뮬이 실시간의 **0.83배**로 흘러 스킬 히트가 최대 300ms 늦음 | `Room::update` 재예약 방식 | **수정 완료** |
| B | 클라 스킬 VFX·SFX가 통째로 사라짐 (몬스터는 정상 피격) | 클라 `skillObjectById_` + 서버 id 발급량 | **수정 완료** |
| C | `~Room()`이 룸 id를 **객체 id 풀에** 반납 → 중복 id | `Room::~Room` | **수정 완료** |
| D | Room이 **자기 JobQueue 실행 중에 파괴**됨 (UAF) | `Room::leave` → `RoomManager::removeRoom` | **미수정 · 최우선** |
| E | `Room::enter` 스냅샷이 사망 몬스터 포함 | `Room::enter` | 미수정 · 현 설계에선 미발화 |

A·B·C는 검증까지 끝났다. **D는 서버 수명 관리 구조 문제라 서버 담당이 보는 게 맞다고 판단해
남겨뒀다** — 아래 §4에 재현 조건과 수정 방향을 정리했다.

---

## 2. 바뀐 것

### A. Room 틱 케이던스 — 절대 데드라인 재예약

`Room::update()`가 `doTimer(dt, …)`로 "지금부터 16.667ms 뒤"를 예약하고 있었다. `doTimer`는
`update()`가 **끝난 뒤** 호출되므로 실제 주기 = `처리시간 + dt`가 되어 매 틱 밀렸다.
여기에 `duration_cast<milliseconds>`가 16.667ms를 16ms로 절삭해 반대 방향 오차가 우연히
일부 상쇄하고 있었다. **실측 틱레이트 약 50Hz(목표 60Hz), 시뮬 클럭 = 실시간의 0.83배.**

스킬 타임라인이 그 비율만큼 늦게 진행해 **NPC 공격 히트박스가 클라 예측보다 200~300ms 늦게**
생성됐고, 그것이 피격 판정 불일치의 주원인이었다. 히트박스 기하 자체는 정상이었다.

변경: `RoomServer/JobTimer.{hpp,cpp}`, `RoomServer/Room.{hpp,cpp}`
- `JobTimer::addJobAt(절대시각, …)` 추가. `addJob`은 이 위에 재구현(절삭 제거, API 보존).
- `Room`에 `nextTickTime_` 추가. 다음 틱을 절대 데드라인으로 재예약하고, 3틱(50ms) 넘게
  밀리면 백로그를 포기하고 리싱크한다.

> **부작용 주의:** 지금까지 서버 전체가 0.83배 슬로모션이었다. 물리·NPC 이동속도·AI 타이밍·
> 스킬 지속시간이 모두 *저작한 값대로* 돌아왔으므로 **체감상 ~20% 빨라진다.** 느린 클럭 기준으로
> 감각 튜닝된 수치가 있으면 재검토가 필요하다.

### B. 스킬 VFX·SFX 소실 — 서버 id 발급량이 클라 배열 한계를 넘김

클라 `skillObjectById_`는 **서버 오브젝트 id로 직접 색인하는 희소 배열**이고 초기 크기가 256인데,
플레이어 등록만 바운드 검사가 없었다. id가 256을 넘으면 힙 밖에 쓰고 슬롯이 `nullptr`로 남아
스킬 owner 해석이 실패 → `PlayVFX`/`PlaySound`가 **월드 원점에서 재생**된다.
`PlayAnimation`은 id 기반이라 정상이고 데미지는 서버 권위라, **"모션 나오고 몬스터도 맞는데
이펙트만 없는"** 형태로 나타났다.

서버 쪽이 중요한 부분은 **id가 얼마나 빨리 커지는가**다.

```
Room::init 1회 = 몬스터 232 + 거점 10 = 242개 id 소비 (현재 레벨 기준)
세션 id도 같은 IdPool에서 발급 → 접속자 id가 룸 하나당 242씩 점프

실측: 1회차 세션 id 1 → 2회차 244 → 3회차 487  (3회차에서 증상 발생)
```

수정은 클라 쪽에서 했다(`Game::registerSkillObject/unregisterSkillObject` 단일 진입점).
**서버 변경 없음.** 다만 아래 §3-R5를 알아두면 좋다.

### C. 룸 id가 객체 id 풀로 반납되던 문제

`Room::~Room`이 `IdPool::push(id_)`를 불렀는데, 룸 id는 `RoomIdPool::pop()`에서 나온다.
실측 로그:

```
[IdPool] INVALID PUSH id=0   ← 룸 0. 전역 예약 sentinel(0)이 객체 풀에 주입됨
[IdPool] STRAY PUSH id=1     ← 룸 1. 실제 객체 id 1과 중복
[IdPool] STRAY PUSH id=2     ← 룸 2. 실제 객체 id 2와 중복
```

중복 id가 한 룸에 배정되면 `objectById_[D]`에 나중 것만 남아 **앞의 객체가 스킬로 피격 불가**,
클라는 멱등 가드에 걸려 한쪽 스폰을 삼킨다(유령). id 0은 별도로 위험하다 — 전술 AI가
`targetId == 0`을 "타깃 없음"으로 읽으므로 발급되면 NPC가 전원 정지한다.

변경: `RoomServer/Room.hpp`(`~Room`), `RoomServer/object.hpp`, `ServerEngine/IdPool.{hpp,cpp}`
- `~Room()`의 룸 id 반납 삭제. **`RoomIdPool`로도 되돌리지 않는다**(이유는 §3-R4).
- `Object::hasId()` 추가. 레벨에서 복사돼 온 cube는 `setId`를 받지 않아 `id_ == -1`이므로,
  반납 전에 이걸로 거른다(현재 레벨엔 Cube 노드가 0개라 잠복 상태였다).
- **`IdPool`이 스스로를 방어한다.** 발급한 적 없는 id(STRAY)나 범위 밖 id(INVALID)는 보고 후
  **거부**하고 큐에 넣지 않는다. 로그만 남기면 오염 id가 풀에 앉아 언젠가 중복 발급된다.

---

## 3. 앞으로 지켜야 할 규약

이번 두 버그는 모두 **"코드로 강제되지 않은 암묵적 규약"**이 깨져서 생겼다. 새로 명시한다.

**R1. 고정 주기 반복은 반드시 `JobTimer::addJobAt`(절대 시각)을 쓴다.**
상대 지연(`addJob`)으로 재예약하면 처리 시간이 매 주기 누적돼 시뮬 클럭이 실시간에서 이탈한다.
`addJob`은 리스폰 같은 **일회성 지연 작업 전용**이다. 두 함수 주석에 명시해뒀다.

**R2. 스케줄 주기는 시뮬 스텝 `dt`에서 파생시킨다.**
`kTickPeriod = duration_cast<...>(dt)`. 별도 상수로 두면 한쪽만 바뀔 때 드리프트가 조용히
재발한다 — 이번 버그의 본질이 정확히 그것이었다.

**R3. `IdPool`에는 이 풀이 발급한 객체 id만 반납한다.**
다른 id 공간(룸 id 등)이나 id를 받은 적 없는 객체를 넣지 않는다. 새 오브젝트 타입을 추가하면
**발급 경로와 반납 경로를 짝으로** 만들고, 반납 전에 `hasId()`로 거른다.
풀이 거부하긴 하지만 그건 마지막 방어선이지 설계가 아니다.

**R4. 룸 id는 재사용하지 않는다.**
`JobTimer::distribute`는 죽은 룸의 잔여 틱 잡을 **roomId 조회 실패로 버린다.** 룸 id를
재사용하면 그 잡이 **같은 id를 받은 새 룸**을 때린다(이중 틱). 지금은 단조 증가로 두었다.
→ **알려진 한도: 서버 수명당 65536룸.** 넘기면 `RoomIdPool::pop`의 `ASSERT_CRASH`가 터진다.
장수 서버가 필요해지면 `JobData`에 generation을 실어 재사용을 안전하게 만들 것.

**R5. 서버가 발급하는 id 값의 크기는 클라 메모리에 직접 반영된다.**
클라는 서버 id를 배열 인덱스로 쓴다(현재는 안전하게 grow). id가 커질수록 클라 배열도 커지므로,
룸당 242개씩 태우는 현재 방식은 이상적이지 않다. **룸 단위로 id 공간을 재사용하거나
(룸 파괴 시 일괄 반납은 이미 하고 있다) 몬스터 id를 룸 로컬로 분리**하는 편이 장기적으로 낫다.
당장 문제는 없다 — 클라가 grow하도록 고쳤다.

**R6. `Room` 상태는 락이 없다. 그 전제는 "잡 큐가 직렬화한다"에 전적으로 의존한다.**
이 전제가 깨지는 경로가 아직 하나 남아 있다(§4-D). 새 코드를 넣을 때 Room 상태를
잡 큐 밖에서 만지지 않도록 주의.

---

## 4. 남은 과제

우선순위 순.

### P0 — D. Room이 자기 JobQueue 실행 도중에 파괴된다 (잠복 UAF)

`Room::leave` → `RoomManager::removeRoom` → `ObjectPool<Room>::push` → `~Room()`이
**그 룸의 `jobQueue_.execute()` 콜스택 안에서** 일어난다. `JobQueue::execute`는 잡 실행 후에도
`jobCount_`를 만지므로 **해제된 메모리에 원자 연산**을 한다.

`MemoryPool`은 LIFO라 다음 `ObjectPool<Room>::pop()`이 같은 블록을 돌려준다
(실측: 룸 3회 생성/파괴가 전부 같은 주소). 새 Room이 그 자리에 오면 죽은 큐의 `fetch_sub`가
**새 룸의 `jobCount_`를 음수로** 만들고, `push()`의 `prevCnt == 0` 게이트가 어긋나
잡이 스케줄되지 않거나 `JobQueuePool`에 큐가 중복 등록되어 **두 워커가 한 Room의 잡을 동시에
실행**한다. 그 순간 R6의 전제가 깨져 Room 상태 전체가 경쟁에 놓인다.

**왜 아직 안 터졌나:** 파괴(워커 스레드)와 다음 룸 생성 사이에 사람이 클라를 다시 켜는 수 초의
간격이 있어서 죽은 큐의 `fetch_sub`가 먼저 끝난다. **파티 2팀이 붙거나 룸 종료 직후 새 파티가
시작되면 물린다.**

수정 방향: `removeRoom`은 맵에서만 제거하고, 실제 반납은 해당 JobQueue가 유휴
(`executing_ == 0 && jobCount_ <= 0`)임이 확인된 뒤 하는 **지연 파괴(reaper)**.
곁들여 `RoomManager::removeRoom`과 `Room::move`의 `unordered_map::operator[]` → `find`
(`operator[]`가 없는 키에 nullptr을 삽입하고, 그 nullptr이 `ObjectPool<Room>::push`로 가면
`nullptr->~Room()`이다. 지금은 보고 후 반환하도록 막아뒀다).

또 하나: `GameSession::enterRoom`이 `findOrCreateRoomByCode` 반환 포인터를 **락 밖에서** 쓴다.
그 사이 다른 스레드가 룸을 파괴하면 해제된 메모리에 `doAsync`한다. 같이 정리하면 좋다.

### P1 — 고핑 환경의 fast-forward 히트 윈도우 소실

`SkillSystem::startSkill`은 `[0, elapsedMs]`(최대 250ms) 구간의 타임라인 이벤트를 **충돌 검사
없이** 즉시 디스패치한다. 히트박스의 Spawn과 Destroy가 모두 이 구간에 들어가는 짧은 히트
윈도우는 서버 판정이 0회가 되어 **확정 미스**다. 로컬(ffwd 6~13ms)에서는 발현하지 않았으나
실측 핑 환경에서 재검증이 필요하다.
해결안: fast-forward 중 스폰된 히트박스에 1틱 유예를 주거나, 구간을 스텝 시뮬레이션.

### ~~P2 — `S_NpcMoveBatch` 20Hz 스로틀~~ → **2026-07-27 처리 완료**

`Room::kNpcMoveBroadcastPeriodTicks = 3`으로 20Hz 스로틀 적용(시뮬은 60Hz 유지).
`Room::update` 선두에서 `npcMoveBroadcastThisTick_`을 판정해 `updateMonsterAI`/`updateTacticalAI`의
두 송신 지점이 같은 틱에 나가도록 공유한다.

이때 **클라에서 더 큰 결함 하나가 같이 발견돼 수정됐다** — `Object::setOrient`의
`snapToCurrent()`가 위치 보간 세그먼트를 지워, 몬스터·보스가 보간 없이 매 패킷 순간이동하고
있었다(`netInterpAcc_`/`tNet` 기계 전체가 죽은 코드였다). `Object::setCurrOrient()` 신설로 해결.
상세: `client/docs/gameArchitecture.md` 게임 루프 8단계, `docs/roomTickCadence.md` §7-2.

**남은 것:** 델타 압축·관심영역 컬링 없음(거리 무관 전체 송신), `S_NpcMoveBatch`에 서버
타임스탬프가 없어 클라가 시간 정렬을 못 한다(`S_TimeSync` 오프셋은 송신 경로에서만 쓰인다).

### P3 — 게임플레이 튜닝값 재검토

§2-A 부작용 주의 참조. 모든 시간 기반 값이 ~20% 빨라졌다.

### P4 — E. `Room::enter` 스냅샷의 사망 몬스터 (현 설계에선 미발화)

`Room::enter`의 objInfos 수집에는 hp 필터가 없다. 반면 `S_NpcMoveBatch`는 `hp() > 0`으로
거른다. 전투 중에 입장한 클라는 죽은 몬스터를 **살아 보이는 오브젝트로 생성**하고, 서버가
이동을 안 보내므로 죽은 자리에 얼어붙은 채 남는다.

**현재 설계(룸 생성 → 전원 집결 → Start)에서는 전투 시작 전에 모두 입장하므로 발화하지 않는다.**
`Room::enter`에 그 취지의 주석을 달아뒀다. **재접속이나 전투 중 입장을 도입하면 즉시
재무장되므로**, 그때 `hp() > 0` 필터를 넣어야 한다.

연쇄 위험도 같이: 클라가 모르는 id에 `S_NpcRespawn`이 오면 클라 폴백이 기본값 Goblin +
scale(0,0,0)짜리 오브젝트를 만든다.

### 미해결 — "유령 몬스터" 원인은 아직 열려 있다

가끔 뜬금없는 몬스터 한 마리가 유령처럼 젠되는 증상이 보고됐다. E를 유력 후보로 봤으나
위 이유로 기각됐다. C(중복 id)가 원인의 일부였을 가능성은 있다 — 확증된 오염이었고 이제
차단됐다. **다음에 목격하면 클라에서 F12를 눌러 판정을 캡처할 것**(§5).

---

## 5. 상시 감시 장치 읽는 법

전부 **실패할 때만** 출력하므로 평시에는 조용하다. 뜨면 무시하지 말 것.

| 신호 | 위치 | 의미 |
|---|---|---|
| `[IdPool] STRAY PUSH / INVALID PUSH … REJECTED` | `ServerEngine/IdPool.cpp` | 다른 id 공간이 객체 풀로 새려다 차단됨 = **새 누수 경로**. R3 위반 |
| `[IdPool] DUPLICATE POP` | 〃 | 풀에 같은 값이 두 개 = 두 객체가 id 공유. push 방어를 우회한 경로가 있다 |
| `[JobQueue] CONCURRENT EXECUTE` | `ServerEngine/JobQueue.cpp` | 한 잡 큐를 두 스레드가 동시 실행 = **D 발화**. Room 상태 레이스 |
| `[JobQueue] NEGATIVE jobCount` | 〃 | 파괴된 큐가 후임 인스턴스의 카운터를 깎았다 = **D 발화** |
| `[RoomManager] DOUBLE REMOVE` | `RoomServer/RoomManager.cpp` | 같은 룸 removeRoom 2회 (D 2차 증상). 크래시 대신 보고 후 반환 |
| `[Skill] owner unresolved … resolved=0` | 클라 | B 회귀. VFX/SFX가 월드 원점에서 재생된다 |
| **F12** 정합성 감사 | 클라 `Game::debugAuditObjectRegistry` | 아래 판독표 참조 |

`JobQueue::execute`는 잔량이 음수가 되면 1회 보고 후 루프를 빠져나온다. 원본은 `== 0`일 때만
탈출해서 그 상태에서 빈 큐를 영원히 스핀하며 워커 하나를 통째로 점유했다.

### 유령 목격 시 판독

즉시 **F12** → 출력 캡처, 서버 콘솔의 같은 시각 로그를 캡처하고 감사에 나온 id로 grep.

| F12 결과 | 서버 로그 | 결론 |
|---|---|---|
| `STALE` (5초 이상 서버 move 없음), 맵 등록은 정상 | — | 서버가 그 개체를 이동 배치에서 뺐다 = 서버에선 이미 사망 (E 계열) |
| `ORPHAN` (렌더는 되는데 id 맵에 없음) | `DUPLICATE POP id=…` | 중복 id로 클라 멱등 가드가 스폰을 삼킴 |
| `ORPHAN` | id 로그 정상 | 클라 컨테이너 정리 경로 결함 (신규) |
| `PLAYER SLOT MISMATCH` | — | B 회귀 |
| 감사 전부 정상인데 유령이 보임 | `CONCURRENT EXECUTE` / `NEGATIVE jobCount` | **D** |

### 오탐 주의

서버 종료 시의 `INVALID PUSH id=4294967295`는 **한 번도 accept되지 않은 대기 세션**
(`Session::id_` 기본값 -1)이 소멸한 것이다. 런타임 중에 뜨는 것만 문제로 본다.

---

## 6. 재진단 도구 상자

이번에 쓴 임시 로그는 원인 확정 후 제거했다. 재현이 필요하면 아래를 임시로 다시 넣는다.

| 로그 | 위치 | 측정 대상 |
|---|---|---|
| 틱 드리프트 | `Room::update` 말미 | 누적 틱 수 vs wall-clock, `update()` CPU 시간 |
| 캐스트 fast-forward | `Room::skillStart` | `elapsedMs`(ffwd 크기), 타임스탬프 유효성 |
| ffwd 스킵 이벤트 | `SkillSystem::startSkill` 루프 | ffwd 구간에 삼켜진 Spawn/Destroy (P1 검증용) |
| 서버/클라 히트 | 양쪽 `processHitResults` | seed·target·인스턴스 시각 |
| 룸 id 소비량 | `Room::init` 말미 | 룸당 발급 개수 = 다음 접속자 id 점프폭 |
| 룸 생성/파괴 주소 | `Room` 생성자·소멸자 | 풀 재사용으로 같은 주소가 재등장하는지 (D 전제) |

**핵심 기법 두 가지.**
① 캐스트별 `skillSeed`가 클라/서버에 공유되므로 양쪽 히트 로그를 **seed로 조인**하면 불일치
건을 하나씩 짚을 수 있다. 인스턴스 시각까지 찍으면 "기하 문제"인지 "타이밍 문제"인지 즉시
갈린다 — 시각이 일치하는데 결과가 다르면 기하/위치, 시각 자체가 밀리면 클럭 문제다.
② id 문제를 볼 때 **`IdPool`이 FIFO라고 가정하지 말 것.** moodycamel `ccqueue`는 producer별
서브큐를 갖고, 반납은 워커 스레드마다 새 서브큐를 만든다. 반납분이 곧바로 재발급되기도 해서
id 결함은 "가끔"만 재현된다.

### 히트박스 육안 대조

서버 권위 히트박스를 클라 화면에 그릴 수 있다. **두 토글 모두 코드에 남아 있고 기본 off.**

| 토글 | 위치 | 색 |
|---|---|---|
| `kBroadcastDebugHitboxes` | `RoomServer/Room.cpp` (`updateSkillSystem` 부근) | **빨강** = 서버 권위 |
| `kDebugSkillHitboxOverlay` | `client/online/onlineGame.cpp` | **초록** = 클라 예측 |

**서버 플래그는 매 프레임 alloc + serialize + 전체 브로드캐스트를 하므로 프로덕션에서는 반드시 off.**

---

## 7. 관련 문서

- `RoomServer/docs/roomTickCadence.md` — A의 측정 데이터·설계 결정·기각된 가설
- `RoomServer/docs/objectIdLifecycle.md` — B·C·D·E의 전체 분석
- `RoomServer/docs/serverArchitecture.md` — Room 업데이트 루프
- `RoomServer/docs/skillArchitecture.md` — 스킬 타임라인·히트박스 생명주기
- `client/docs/particleHitboxDeterminism.md` — `skillSeed` 결정론 계약

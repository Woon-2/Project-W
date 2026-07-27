# 오브젝트 id 수명주기

**2026-07-26.** 온라인 모드 두 증상 — ① 스킬 사용 시 몬스터는 피격되는데 VFX·SFX가 안 나옴
② 뜬금없는 몬스터 한 마리가 유령처럼 젠됨 — 을 오브젝트 id 층위로 좁혀 진단하고,
그중 **H1·H2를 수정**한 기록. H3·H4·H5는 원인만 특정된 상태로 남아 있다.

| | 원인 | 상태 |
|---|---|---|
| H1 | 클라 `skillObjectById_` 256 고정 + 바운드 미검사 | **실측 확증 → 수정 완료** |
| H2 | `~Room()`이 룸 id를 객체 `IdPool`에 반납 | **실측 확증 → 수정 완료** |
| H3 | Room이 자기 JobQueue 실행 중 파괴 | 전제 확인, 미발화 — **미수정(잠복 UAF)** |
| H4 | `Room::enter` 스냅샷이 사망 몬스터 포함 | 코드상 결함이나 **현 설계에선 발화 불가** — 미수정 |
| H5 | id 미할당 객체(cube) 반납 | 잠복 → **중앙 방어로 무해화** |

> **② 유령 몬스터의 원인은 아직 열려 있다.** H4를 유력 후보로 봤으나, 현재 게임 흐름이
> "룸 생성 → 전원 집결 → Start"라 전투 중 입장 자체가 없어 기각됐다. H2(확증된 중복 id 오염,
> 이제 차단됨)가 원인의 일부였을 가능성은 있다. 다음에 목격하면 **F12 감사**로 판정을 캡처할 것.

인수인계용 요약은 `serverHandoff.md` 참조.

---

## 1. 구조적 전제

서버는 **단일 `IdPool`** 하나로 세션 id·몬스터 id·거점 id를 전부 발급한다.

| 소비 지점 | 위치 |
|---|---|
| 세션 id (접속 1건당 1개) | `RoomServer/Listener.cpp` `processAccept` |
| 몬스터 id / 거점 id (룸 생성 시) | `Room::init` |
| 전술 NPC id (아레나 인카운터) | `Room::registerTacticalNpcBody` |
| 최종 보스 id | `Room::onArenaBossEnter` 경로 |

반납은 `~GameSession`, `~Room`, `Room::cleanupTacticalEncounter`.

**`IdPool`은 moodycamel `ccqueue`라서 엄격한 FIFO가 아니다.** 초기 65535개는 메인 스레드
producer 큐에 있고, 반납은 워커 스레드마다 별도 producer 큐를 만든다. consumer가 어느
producer를 먼저 훑느냐에 따라 낮은 재활용 id가 나오기도 하고 초기 블록에서 계속 빼기도 한다.
**id 관련 결함이 "가끔"만 재현되는 이유가 이것이다 — id 문제를 볼 때 FIFO를 가정하지 말 것.**

### 룸 1개가 삼키는 id 개수 (실측)

현재 레벨(`resources/terrains/chunks_index.bin`): 거점 10개, `TargetCount` 합계 232
→ **`Room::init`이 242개**를 소비. 세션 id도 같은 풀에서 나오므로 **접속자 id가 룸 하나당
242씩 점프**한다. 서버 로그로 확인한 실측값:

| 실행 | 세션 id | 룸 몬스터 id |
|---|---|---|
| 1회차 | 1 | 2 ~ 243 |
| 2회차 | 244 | 245 ~ 486 |
| 3회차 | **487** | 488 ~ 729 |

---

## 2. H1 — 클라 `skillObjectById_`의 256 고정 한계 (수정 완료)

`skillObjectById_`는 **서버 오브젝트 id로 직접 색인하는 희소 배열**이다.
`setupPlayer`가 `assign(256, nullptr)` 후 플레이어를 등록하는데, 몬스터·원격 플레이어
등록부 8곳은 전부 `if (id >= size) resize(id+1)`로 방어하면서 **플레이어 등록만 바운드 검사가
없었다.**

`playerId >= 256`이면 (1) 힙 밖으로 8바이트를 쓰고, (2) 직후 몬스터들이 `resize`하면서 버퍼가
재할당돼 **플레이어 슬롯이 `nullptr`로 남는다.**

그러면 `SkillSystem::lookupObject`가 owner를 못 찾고 타임라인 이벤트가 이렇게 갈린다:

| 이벤트 | owner == nullptr |
|---|---|
| `PlayVFX` | **월드 원점(0,0,0)에서 재생** |
| `PlaySound` | **원점 3D 감쇠 → 안 들림** |
| `SpawnHitbox` | 로컬 OBB 그대로 → 로컬 예측 히트 0 |
| `ApplyImpulse` | 무시 |
| `PlayAnimation` | **정상** (id로 `EvAttack` post) |

→ 공격 모션은 나오고 서버 권위 데미지(`S_SkillHit`)로 몬스터는 피격되는데 **VFX·SFX·피격
blood만 사라진다.** 3회차 실행(세션 id 487)에서 정확히 재현됐다.

**재시작이 없어도 발생한다.** 아레나 전술 인카운터가 NPC 61마리를 추가 발급한 뒤 누군가
재접속하면 첫 서버 기동에서도 256을 넘는다.

### 수정

`Online::Game`에 등록 단일 진입점을 두고 복붙된 10곳을 전부 통합했다.

```cpp
void Game::registerSkillObject(i32t id, Object* obj);   // 필요한 만큼 resize 후 등록
void Game::unregisterSkillObject(i32t id);
```

- `setupPlayer`의 무검사 쓰기 제거. `assign(256, nullptr)`은 **상한이 아니라 초기 크기 힌트**로
  남기고(이전 세션 잔재 제거 역할 겸함), 직후 `idMonsterMap_`과 `idPlayerMap_`을 순회해
  **setupPlayer 이전에 도착한 오브젝트를 복구**한다(패킷 순서는 보장되지 않는다).
  기존에는 몬스터만 복구하고 원격 플레이어는 빠져 있었다.
- `createOtherPlayer` 계열의 `if (!skillObjectById_.empty())` 순서 가드 제거 — setupPlayer 전에
  도착한 원격 플레이어가 영영 미등록되던 함정이었다.
- `removeNpc` / `removePlayer`는 `unregisterSkillObject`로 통일.

**불변식:** 배열이 커지면 `data()`가 재할당되므로, 등록 후 스킬 시스템에 진입하기 전에
`refreshSkillCtx()`로 `skillCtx_`의 포인터를 다시 맞춰야 한다(프레임 시작 + `removePlayer`
/`onSkillStart` 핸들러가 이미 호출하고 있다).

---

## 3. H2 — 룸 id가 객체 id 풀로 반납되던 문제 (수정 완료)

`Room::~Room`이 `IdPool::push(id_)`를 불렀는데 룸 id는 `RoomIdPool::pop()`에서 나온다.
실측 로그:

```
[IdPool] INVALID PUSH id=0   ← 룸 0. 전역 예약 sentinel이 풀에 주입됨
[IdPool] STRAY PUSH id=1     ← 룸 1. 실제 객체 id 1과 중복
[IdPool] STRAY PUSH id=2     ← 룸 2. 실제 객체 id 2와 중복
```

중복 id가 한 룸에 배정되면: 서버 `objectById_[D]`에는 나중 것만 남아 앞의 객체가 스킬로
**피격 불가**, 클라는 `createXxx`의 멱등 가드에 걸려 한쪽이 아예 안 만들어지거나 이동 갱신을
못 받는다 → **유령**. 플레이어와 몬스터가 id를 공유하면 `skillObjectById_[D]`를 몬스터가
덮어써 **VFX가 몬스터 위치에서 재생**된다.

id 0은 별도로 위험하다 — `IdPool::init`의 주석대로 전술 AI가 `targetId == 0`을 "타깃 없음"으로
읽으므로, 발급되면 NPC가 전원 정지한다.

### 수정

1. **`~Room()`에서 룸 id 반납을 삭제**했다. `RoomIdPool`로 돌려주지도 **않는다**:
   `JobTimer::distribute`는 죽은 룸의 잔여 틱 잡을 roomId 조회 실패로 버리는데, 룸 id를
   재사용하면 그 잡이 **같은 id를 받은 새 룸**을 때린다. 룸 id는 단조 증가로 남긴다.
   → **알려진 한도: 서버 수명당 65536룸.** 넘기면 `RoomIdPool::pop`의 `ASSERT_CRASH`가 터진다.
   장수 서버가 필요해지면 JobData에 generation을 실어 재사용을 안전하게 만들 것.
2. **`IdPool`이 스스로를 방어한다.** 발급한 적 없는 id(STRAY)나 범위 밖 id(INVALID)는
   보고 후 **거부**하고 큐에 넣지 않는다. 오염된 id가 풀에 남으면 두 객체가 같은 id를 갖게
   되므로, 로그만 남기는 것으로는 부족하다.
3. **H5 동시 무해화**: `~Room()`이 cube의 id를 반납하기 전에 `Object::hasId()`로 거른다
   (레벨에서 복사돼 온 cube는 `setId`를 받지 않아 `id_ == -1`). 중앙 거부와 이중 방어.

---

## 4. 남은 원인 (미수정)

### ~~H3. Room이 자기 JobQueue 실행 도중에 파괴된다 — 잠복 UAF~~ → **2026-07-27 수정 완료**

`Room::leave` → `RoomManager::removeRoom` → `ObjectPool<Room>::push` → `~Room()`이
**그 룸의 `jobQueue_.execute()` 콜스택 안에서** 일어났다. "단일 클라 재시작에서는 안 물린다"는
당시 판단은 **틀렸다** — 새 Room 재활용 없이도, 같은 dequeue 배치의 잔여 틱 잡과 execute 루프
자체가 파괴된 메모리를 만져 클라 종료 한 번으로 크래시했다(`PhysicsWorld::solveConstraints`
warm-cache 루프 / `try_dequeue_bulk`에서 발현, 지점은 잡 순서에 따라 유동).

**지연 파괴(reaper)로 수정**: `removeRoom`은 맵에서만 제거, `RoomManager::sweepPendingRooms`가
`JobQueue::idle()` 연속 2회 관측 후 반납. `Room::closed_`가 틱 재예약·입장·미아 leave를 차단,
`JobTimer::distribute`는 `RoomManager::postJob`으로 조회+push를 한 락에서 수행.
`removeRoom`/`Room::move`의 `operator[]` → `find`도 처리.
상세: `docs/serverHandoff.md` §4-D. 아래 감시 장치(§5)는 회귀 알람으로 계속 유지한다.

### H4. `Room::enter` 스냅샷이 죽은 몬스터를 그대로 보낸다 — 현 설계에선 발화 불가

`Room::enter`의 objInfos 수집에는 **hp 필터가 없다.** 반면 매 틱 `S_NpcMoveBatch`는
`if (npc.hp() > 0)`로 거른다.

→ 몬스터가 죽고 거점이 리스폰시키기 전에 접속한 클라는 그 몬스터를 **살아 보이는 오브젝트로
생성**하고, 서버는 이동을 절대 안 보내므로 **죽은 자리에 얼어붙은 채 서 있고, 서버 hp가 이미
0이라 때려도 반응이 없다.**

**단, 현재 게임 흐름은 "룸 생성 → 전원 집결 → Start"라 전투 시작 전에 모두 입장한다.**
전투 중 입장이라는 개념이 없으므로 이 경로는 발화하지 않는다 — 그래서 유령 몬스터의 원인
후보에서 기각됐고, 수정도 미뤘다. 탐지 로그도 제거했다(`Room::enter`에 취지 주석만 남김).

**재무장 조건: 재접속, 관전, 전투 중 난입 중 하나라도 도입하는 순간.** 그때 스냅샷에
`hp() > 0` 필터를 넣어야 한다.

연쇄 위험(같이 고칠 것): 클라가 모르는 id에 `S_NpcRespawn`이 오면 `onNpcRespawn`의
fresh-create 폴백이 `respawnKind_`/`monsterSpawnInfo_` 미스로 **기본값 Goblin +
scale(0,0,0) + orient(0,0,0,0)** 짜리 오브젝트를 만든다.

### 기타

- `reinitFromPool`에 `[EXPERIMENT] Bomber는 풀 재사용 금지` 임시 코드가 남아 있다.
- `GameSession::enterRoom`이 `findOrCreateRoomByCode` 반환 포인터를 락 밖에서 쓴다.
  → 2026-07-27: 지연 파괴 + `Room::enter`의 `closed_` 가드로 완화(닫힌 방 입장은 거부되고
  세션이 끊겨 재접속 시 새 방을 만든다). serverHandoff.md §4-D 참조.

---

## 5. 상시 유지되는 감시 장치

수정 후에도 남긴 것들. 전부 **실패할 때만** 출력하므로 평시에는 조용하다.

| 신호 | 위치 | 의미 |
|---|---|---|
| `[IdPool] STRAY PUSH / INVALID PUSH … REJECTED` | `ServerEngine/IdPool.cpp` | 다른 id 공간이 객체 풀로 새려다 차단됨. **새 누수 경로** |
| `[IdPool] DUPLICATE POP` | 〃 | 풀에 같은 값이 두 개 = 두 객체가 id 공유. push 방어를 우회한 경로가 있다는 뜻 |
| `[JobQueue] CONCURRENT EXECUTE` | `ServerEngine/JobQueue.cpp` | 한 잡 큐를 두 스레드가 동시 실행 = Room 상태 레이스 (**H3 발화**) |
| `[JobQueue] NEGATIVE jobCount` | 〃 | 파괴된 큐가 후임 인스턴스의 카운터를 깎았다 (**H3 발화**) |
| `[RoomManager] DOUBLE REMOVE` | `RoomServer/RoomManager.cpp` | 같은 룸 removeRoom 2회 (H3 2차 증상). 크래시 대신 보고 후 반환 |
| `[Skill] owner unresolved … resolved=0` | `Game::debugLogSkillOwnerResolution` | 스킬 owner 해석 실패 = H1 회귀. VFX/SFX가 원점에서 재생된다 |
| **F12** 정합성 감사 | `Game::debugAuditObjectRegistry` | `ORPHAN`(렌더는 되는데 id 맵에 없음=유령) / `STALE`(5초 이상 서버 move 없음) / `SKILL SLOT MISMATCH` / `PLAYER SLOT MISMATCH` / `DANGLING MAP ENTRY` |

`JobQueue::execute`는 잔량이 음수가 되면 1회 보고 후 루프를 빠져나온다. 원본은 `== 0`일 때만
탈출해서 그 상태에서 빈 큐를 무한 스핀했다(워커 1개 영구 점유).

### 유령 목격 시 판독

즉시 **F12** → 출력 캡처, 서버 콘솔의 같은 시각 로그를 캡처하고 감사에 나온 id로 grep.

| F12 결과 | 서버 로그 | 결론 |
|---|---|---|
| `STALE`(5초 이상 서버 move 없음), 맵 등록은 정상 | — | 서버가 그 개체를 이동 배치에서 뺐다 = 서버에선 이미 사망 (H4 계열) |
| `ORPHAN` | `DUPLICATE POP id=…` | 중복 id로 멱등 가드가 스폰을 삼킴 |
| `ORPHAN` | id 로그 정상 | 클라 컨테이너 정리 경로 결함 (신규) |
| `PLAYER SLOT MISMATCH` | — | H1 회귀 |
| 감사 전부 정상인데 유령이 보임 | `CONCURRENT EXECUTE` / `NEGATIVE jobCount` | **H3** |

### 오탐 주의

서버 종료 시의 `INVALID PUSH id=4294967295`는 **한 번도 accept되지 않은 대기 세션**
(`Session::id_` 기본값 -1)이 소멸한 것이다. 런타임 중에 뜨는 것만 문제로 본다.

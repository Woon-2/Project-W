# ToDo v2 — NPC AI 포팅 및 고블린 스포너 구현 기록

작성일: 2026-04-28

---

## 1. 이번 세션 작업 요약

### 1-1. NPC AI 포팅 (NPC-AI-Lab → RoomServer)

NPC AI Lab에서 검증한 8-state FSM + NpcGroup 시야 공유 시스템을 RoomServer로 이식했다.

**신규 파일:**

| 파일 | 역할 |
|------|------|
| `RoomServer/npc.hpp` | Npc 기반 클래스. 8-state FSM, NpcConfig, NpcUpdateResult 정의 |
| `RoomServer/npc.cpp` | 상태별 update 구현, 타겟 선택, 분리력 계산 |
| `RoomServer/goblin.hpp` | Goblin : Npc. 위치 히스토리 링 버퍼(lag-comp) 보유 |
| `RoomServer/goblin.cpp` | applyGoblinConfig(), recordSnapshot(), rewindPos() |
| `RoomServer/NpcGroup.hpp` | 그룹 공유 메모리 구조체, NpcGroup 인터페이스 |
| `RoomServer/NpcGroup.cpp` | reportSight(), getBestMemory(), update() 구현 |

**수정 파일:**

- `object.hpp/cpp` — Goblin을 별도 파일로 분리, 4-state FSM 제거
- `Room.hpp/cpp` — NPC AI 쿼리 인터페이스 추가 (getLivingPlayers, findNearbyNpcPositions, countNpcsTargeting, getNpcGroup, findLivingSessionByPlayerId), NpcGroup 벡터 추가
- `RoomServer.vcxproj` — 신규 .cpp/.hpp 6개 항목 수동 추가

**주요 타입 변경:**

- 시간 멤버: `float windupTimer_` 등 → `Seconds windupTimer_`  
  초기화는 `0s`, 비교는 `0s` / `0ms` (`.count()` 미사용)
- `targetId_`: `uint32(0)` 센티넬 → `int32(-1)` 센티넬  
  이유: `IdPool`이 0부터 발급하므로 0은 유효한 플레이어 ID → 0을 "없음"으로 쓸 수 없음
- `groupId_`: -1 = "그룹 없음", 0 이상 = 유효 그룹 (벡터 인덱스와 동일)
- `SharedTargetMemory::playerId`: `uint32` → `int32` (위와 동일 이유)
- `Room::aggroCount_`: `unordered_map<int32, int32>`

---

### 1-2. 고블린 스포너 배치 구현

**파일:** `Level.hpp`, `Level.cpp`, `Room.cpp`

GoblinSpawner 노드 1개당 고블린 N마리를 원형 활동 구역 안에 랜덤하고 겹치지 않게 생성하고,
같은 스포너 소속 고블린들을 하나의 NpcGroup으로 묶어 시야 공유 무리를 구성한다.

---

## 2. 고블린 생성 시 setModel이 필요한 이유

```cpp
void importGoblinSpawner(std::ifstream& ifs, const AssetManager& assetManager, Goblin& goblin) {
    goblin.setModel(assetManager.modelGoblin());
}
```

`Object::setModel`은 두 가지 일을 한다 (`object.cpp:6-13`):

1. 모델 포인터를 `pModel_`에 저장
2. `rebuildBodyBVH()` 호출 — 모델의 로컬 BVH를 월드 공간 BVH로 변환해서 `body_.worldBVH()`에 저장

**서버에서 BVH가 필요한 이유:**  
고블린은 `MotionType::Dynamic`으로 물리 시뮬레이션에 참여한다. 충돌 형상(BVH)이 없으면
물리 엔진이 고블린을 "형상 없는 점"으로 취급해 지형을 뚫고 지나간다.

**setModel 없이 가능한 경우:**  
서버가 고블린에 대해 물리 충돌을 쓰지 않고 XZ 이동 + 높이 고정 방식으로만 이동시킨다면
`setModel`은 불필요하다. 현재는 Dynamic 바디를 사용하므로 필요하다.

**참고:** `importCube`도 동일한 이유로 `cube.setModel(assetManager.modelCube())`를 호출한다.  
`rebuildBodyBVH()`는 `if (!pModel_ || pModel_->bvh.empty()) return;`으로 시작하므로
모델에 BVH 데이터가 없으면 실질적으로 아무 일도 하지 않는다.

---

## 3. 고블린 랜덤 위치 생성 알고리즘

**목표:** 반경 R인 원 안에 N개의 점을 균등하게 배치하되, 점 사이 최소 거리 d를 보장한다.

### 3-1. 원 안 균등 분포 (Disc Uniform Distribution)

```cpp
float r     = kActivityRadius * std::sqrt(distR(rng));   // distR: [0,1) 균등
float theta = distAngle(rng);                            // distAngle: [0, 2π) 균등
spawnPos = mu::Vec3(center.x() + r * std::cosf(theta),
                    baseY,
                    center.z() + r * std::sinf(theta));
```

**왜 `sqrt`를 쓰는가?**

단순히 `r = R * rand`를 쓰면 점이 중심에 몰린다.

반경 r, 두께 dr인 고리의 넓이는 `2πr·dr`이다. r이 작을수록 고리가 좁아지므로
균등하게 면적을 채우려면 작은 r이 뽑힐 확률을 낮춰야 한다.

균등 분포 u ∈ [0,1)에서 r = R·√u로 변환하면 CDF가 F(r) = (r/R)²가 되어
면적에 비례한 균등 분포를 얻는다:

```
P(r 이하) = (r/R)²  →  면적이 작은 내부보다 넓은 외부에 더 많은 점이 분포
```

### 3-2. 겹침 방지 (Rejection Sampling)

```cpp
int32 attempts = 0;
do {
    // 후보 위치 생성
    ...
    ++attempts;
} while (attempts < 100 &&
         std::any_of(placed.begin(), placed.end(), [&](const mu::Vec3& p) {
             return (spawnPos - p).len2() < kMinDist * kMinDist;
         }));
```

이미 배치된 점들과 `kMinDist`(3m) 이하로 겹치면 재추첨한다.  
최대 100회까지 시도하고 100회를 넘으면 겹치더라도 배치한다 (무한 루프 방지).

**상수 값 (현재 기준):**

| 상수 | 값 | 의미 |
|------|----|------|
| `kActivityRadius` | 28.f | 활동 구역 반경 (m) |
| `kCount` | 5 (테스트) / 20 (실전) | 스포너당 고블린 수 |
| `kMinDist` | 3.f | 고블린 간 최소 거리 (m) |

> 현재 Level.cpp에서 `kCount = 5`로 설정되어 있다 (테스트 목적).  
> 실전 배포 시 20으로 변경한다.

---

## 4. 고쳐야 할 버그 목록

### Bug-1. Return 상태에서 그룹 메모리 미체크 (우선순위 높음)

**파일:** `RoomServer/npc.cpp` — `updateReturn()`

**현상:** 어그로가 풀렸다가 플레이어가 재진입하면 일부 고블린(직접 감지 범위 15m 이내)만
반응하고, 나머지는 반응하지 않는다.

**원인:**  
`updateReturn`은 `selectBestTarget()`(직접 감지)만 체크하고 그룹 메모리를 보지 않는다.
두 번째 감지 시 일부 고블린이 아직 Return 상태라면, 직접 감지 범위 밖에 있는 고블린은
새로운 그룹 메모리를 무시하고 계속 귀환한다.

**수정 방향:** `updateReturn` 내부에서 직접 감지 실패 시 그룹 메모리 체크 추가:

```cpp
// 현재
if (canReAggroOnReturn_ && !isOutsideActivityZone()) {
    GameSession* candidate = selectBestTarget(room);
    if (candidate) { ... Chase ... }
}
// 그룹 메모리 체크 없음 → 귀환만 함

// 수정 후
if (!isOutsideActivityZone()) {
    if (canReAggroOnReturn_) {
        GameSession* candidate = selectBestTarget(room);
        if (candidate) {
            if (groupId_ >= 0) {
                NpcGroup* group = room.getNpcGroup(groupId_);
                if (group)
                    group->reportSight(getId(), candidate->id(),   // Bug-2도 동시 수정
                                       candidate->player()->pos(), room.getElapsedMs());
            }
            targetId_ = candidate->id();
            transitionTo(NpcState::Chase);
            return {};
        }
    }
    if (groupId_ >= 0) {
        NpcGroup* group = room.getNpcGroup(groupId_);
        if (group && group->getBestMemoryInsideActivityArea(room.getElapsedMs())) {
            transitionTo(NpcState::Investigate);
            return {};
        }
    }
}
```

---

### Bug-2. updateReturn의 reportSight에 uint32 캐스트 잔류

**파일:** `RoomServer/npc.cpp` — `updateReturn()` line 268

**코드:**
```cpp
// 현재 (버그)
group->reportSight(getId(), static_cast<uint32>(candidate->id()), ...);

// 수정
group->reportSight(getId(), candidate->id(), ...);
```

`reportSight`의 두 번째 파라미터는 `int32 playerId`인데 `uint32`로 캐스트하고 있다.  
양수 ID에서는 기능 문제가 없지만 타입 불일치이며, 이전 세션에서 `targetId_`를 `int32`로 바꿀 때 이 부분을 놓쳤다.

---

### Bug-3. evaluateTargetScore의 불필요한 uint32 캐스트

**파일:** `RoomServer/npc.cpp` — `evaluateTargetScore()` line 400

**코드:**
```cpp
// 현재 (코드 불일치)
if (static_cast<uint32>(s->id()) == targetId_ && aggro > 0) --aggro;

// 수정
if (s->id() == targetId_ && aggro > 0) --aggro;
```

`targetId_`가 `int32`로 바뀌었으므로 캐스트 불필요. 기능 버그는 아니나 (양수 ID 범위에서 동일하게 동작)
코드 일관성을 위해 수정한다.

---

## 5. 향후 작업 (기존 Plan.md에서 이전)

- [ ] **Bug-1, 2, 3 수정** — `npc.cpp` 단독 수정
- [ ] **`kCount = 20`으로 복구** — Level.cpp
- [ ] **공간 분할 그리드** — `findNearbyNpcPositions` O(N²) → O(1). NPC 30마리 초과 시 적용
- [ ] **setOrient 이중 BVH rebuild 제거** — NPC 수가 충분히 늘어난 시점에 별도 커밋
- [ ] **Dead NPC 조기 스킵** — `hp() <= 0`이면 omega/snapshot/update 세 줄 건너뜀
- [ ] **내 플레이어 사망 처리 완성** — `applyHit`에서 내 플레이어 분기에도 `setDead(true)` 추가

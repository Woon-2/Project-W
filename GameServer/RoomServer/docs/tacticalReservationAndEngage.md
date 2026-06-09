# 전술 NPC 공격권 예약 / squad 교전 배정 개선

NPCAI 시뮬레이터(`D:\source\repos\Project-W\NPCAI`)에서 검증된 4가지 전술 AI 개선을 RoomServer로 포팅한 기록.
기본 틀(플레이어당 동시 공격 NPC 최대 5명, NPC가 먼저 `PressureWait`로 진입해 공격권 예약)은 동일하며,
예약/배정 **알고리즘만** 옮겼다. 거리 크기 상수는 인게임 튜닝값을 유지한다.

## 1. 공격권 예약 — 거리 + 접근 진척 기반 (`Room`, `TacticalNpc`)

기존 `Room::tryReserveTacticalAttackSlot`은 단순 선착순(`size >= 5 → 거부`)이라 멀리 있는 NPC가 슬롯을
선점했다. 다음으로 교체:

- `Room::pruneTacticalAttackReservations()` — 죽은 플레이어/NPC, 타깃 불일치, 또는
  Chase/PressureWait/Flank/Windup/Recover 외 상태의 예약을 슬롯 풀에서 제거.
- `Room::tryReserveTacticalAttackSlot()` —
  1. 교전 중(Windup/Recover) NPC는 occupant로 슬롯 점유 보장
  2. Chase/PressureWait/Flank 후보를 플레이어 거리(`len2`)순 정렬
  3. 정원(5) 초과 시 **가장 먼 예약자부터 축출(eviction)**
  4. 빈 슬롯을 가까운 후보부터 채움
- `Room::findTacticalNpcById()` 헬퍼 추가(`tacticalNpcs_`는 vector라 id 조회용).
- `TacticalNpc::isEligibleForAttackReservation(targetId, targetPos)` — 거리 ≤ MAX_DIST이고,
  스테일로 막힌(blocked) 타깃이면 `PROGRESS_DIST`만큼 더 접근해야 재자격.
- `TacticalNpc::updateReservedAttackStaleTimer()` — 단순 시간 만료에서 **접근 진척 기반**으로 변경.
  사거리 내/교전 중이거나 직전보다 `PROGRESS_DIST` 이상 접근하면 리스 갱신, 진척 없이 정체 시에만 누적.
- `TacticalNpc::releaseStaleAttackReservation(dist)` — 스테일로 끊을 때 해당 타깃을
  `blockedAttackReservationTargetId_`로 표시해 즉시 재예약(진동)을 차단.

## 2. chase ↔ PressureWait 진동 제거

타이머 스태거(`reenterDelay = MIN_TIME + id%4*STAGGER`)와 표시 마스킹(`getDisplayState`)은 기존에 이미 존재.
여기에 #1의 **접근 진척 게이트**(`blockedAttackReservation*` + `PROGRESS_DIST`)가 더해져, 예약이 끊긴
NPC는 일정 거리 전진하기 전까지 재예약하지 않으므로 chase↔pressure 떨림이 사라진다.

## 3. 전술 종료 후 squad 타깃 균형 재배정 (`GoblinMidBossTactic`)

기존 `MidBossTacticBase::assignSquadsToPlayers`(거리 전용·매 틱 재계산)는 **제거**하고,
`GoblinMidBossTactic::issueStableEngage(room, liveSquads, resetAssignments)`로 대체:

- `engageTargetBySquad_`(squadId→playerId) 영속 맵으로 배정 고정.
- 미배정 squad는 **배정 수(count) 최소 → 거리 → id** 순으로 플레이어 선택(한쪽 쏠림 방지).
- `resetAssignments=true`(전술 실패/종료): 전면 재배정. `false`(정상 교전 틱): 생존 중 고정, 사망 시에만 재배정.
- `enterPhase()`는 전술 대형/솔로 phase(BoxAdvance/TacticalRetreat/Encircle/DivideAndConquer/BossSolo)
  진입 시 `engageTargetBySquad_`를 비운다 → 전술 종료 후 새 일반 교전이 깨끗한 균형 재배정으로 시작.

## 4. squad 교전 타깃 고정 + 동일 engage 중복 방지

`issueStableEngage`는 `squad->getEngageTargetId() == 배정타깃`이면 명령을 재발행하지 않는다.
`TacticalSquad::getEngageTargetId()` 게터 추가(현재 `currentOrder_`가 Engage일 때만 타깃 반환).
기존에 모든 squad로 무조건 `receiveOrder(Engage)`를 보내던 **일반 Engage 발행부 전부**를 이 함수
호출로 교체 → 타깃 흔들림/중복 명령 제거. 교체된 호출부:

- `evaluateTactics` 폴백, `enterTacticFailCooldown` (매 틱/실패 경로, `reset=false`/`true`)
- `update()`의 Encircle 완성·BoxAdvance→Engage 전환 (`reset=true`)
- `issueDivideEngage()` (`reset=true`). 기존 `selectReplacementTarget(divideTargetPlayerIds_)`
  단일 타깃 추격은 전 플레이어 대상 균형 배정으로 대체됨.

## 스케일 결정

양쪽 기본 스케일 동일(`moveSpeed=4.0`, `attackRange=2.8`). 시뮬레이터가 2D 가독성용으로 키운
`RESERVATION_MAX_DIST`(18→**8.0 유지**), `PRESSURE_EXTRA_RADIUS`(9→**4.0 유지**)는 적용하지 않았다.
신규 `TACTICAL_ATTACK_RESERVATION_PROGRESS_DIST = 0.4f`만 추가(attackRange 대비 작은 히스테리시스).
압박 링이 넓/좁게 느껴지면 이 값만 미세조정한다.

## 변경 파일

- `Room.hpp` / `Room.cpp` — prune + 거리/축출 예약, `findTacticalNpcById`
- `TacticalNpc.hpp` / `TacticalNpc.cpp` — PROGRESS_DIST 상수, blocked 필드, 예약 헬퍼 4종
- `TacticalSquad.hpp` — `getEngageTargetId()`
- `MidBossTactics.hpp` / `MidBossTactics.cpp` — `engageTargetBySquad_`, `issueStableEngage`,
  `isLivingPlayerTarget`; `assignSquadsToPlayers` 제거

## 검증

빌드 OK(RoomServer, Debug x64). 인게임 검증은 **client**(DummyClient 아님)로 미드보스 교전:
동시 공격 5명 상한·근접 우선, pressure 진동 없음, 다중 플레이어 squad 균등 분배·타깃 고정.

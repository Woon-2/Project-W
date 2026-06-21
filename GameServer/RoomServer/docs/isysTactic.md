# Isys(이시스) 중간보스 전술 — 설계/구현 문서

NPCAI 시뮬레이터(`D:\source\repos\Project-W\NPCAI\NPCAI\sim`)의 Isys 전술을 `RoomServer`로 포팅한
결과를 기록한다. Grandbaum(`grandbaumTactic.md`) / 홉고블린(`GoblinMidBossTactic`) 포팅 인프라를 재사용한다.

## 전술 개요

Isys는 Grandbaum(수동적 생존형)과 대비되는 **능동적 섬멸형** 패턴이다. 평소 4스쿼드 분산 교전 +
보스 자체 근접전을 하다가, **어느 한 스쿼드든 초기 인원의 80% 미만**으로 줄면 전군을 후방으로 빼
집결시킨 뒤 **Bomber로 1차 쐐기 → Buddy+보스로 2차 쐐기**를 *다른 군집*에 꽂는 "치고 빠지는
2연속 쐐기 협공"을 반복한다.

### 부대 구성 (4스쿼드 + 보스)

- `squad[0]` / `squad[1]` = **Buddy** 부대 (2차 돌격 + 보스 합류용)
- `squad[2]` / `squad[3]` = **Bomber** 부대 (1차 돌격용)
- 보스 본체(`PlatoonLeader`) = 평소 자체 근접전, 2차 쐐기에 직접 합류

스쿼드 인덱스 계약은 **스폰 순서**(`Room::spawnIsysEncounter`)로 보장한다.

### Phase 흐름

| Phase | 동작 | 전환 |
|---|---|---|
| `Engage` | 4스쿼드 균형 교전(`issueStableEngage`), 보스 근접 FSM | unlock & cooldown 만료 & bomber 생존 → `RetreatForPincer` |
| `Cooldown` | 교전 유지하며 대기 | 타이머 만료 → `Engage` |
| `RetreatForPincer` | 보스 고속 후퇴(모터), 4스쿼드 후방 슬롯 집결(`FormationHold`) | 보스 도착 & 슬롯 집결, 또는 `RETREAT_TIMEOUT(5s)` → `RegroupBombers` |
| `RegroupBombers` | Bomber 2스쿼드가 적 군집 측면에 돌격 라인 형성 | 슬롯 집결, 또는 `REGROUP_TIMEOUT(3.5s)` → `FirstBomberWedge` |
| `FirstBomberWedge` | Bomber 1차 쐐기(`WedgeCharge`) + 동시에 Buddy 재집결 착수 | charge 완료/`PINCER_TIMEOUT(7s)` & buddy 생존 → `RegroupBuddies` |
| `RegroupBuddies` | Buddy 라인 형성, 보스 본체가 쐐기 apex에 합류(자리 예약) | Buddy 슬롯 집결 & 보스 ready, 또는 4초 타임아웃 → `SecondBuddyWedge` |
| `SecondBuddyWedge` | Buddy 2차 쐐기 + 보스 직접 가세 → **피해 ×1.5** | charge 완료/타임아웃 → `Cooldown` → `Engage` |

## 핵심 메커니즘

### ① 전술 잠금 해제 (`checkUnlockCondition`)
첫 틱에 각 스쿼드 초기 인원을 캡처(`initialSquadSizes_`). 어느 한 스쿼드든
`current/initial < UNLOCK_SQUAD_RATIO(0.80)` 시 발동 → `Cooldown`으로 협공 사이클 진입.

### ② 2단 협공의 타겟 분리 (`selectStrikeClusters`)
`buildPlayerClusters(CLUSTER_RADIUS)` 위에 점수 `인원 ×1000 − 보스거리`. 2차(`applyRepeatPenalty=true`)는
1차 타깃(`firstStrikeTargetIds_`)과 겹치는 군집에 `SECOND_STRIKE_REPEAT_PENALTY(350)` 차감 →
1차에 안 맞은 다른 군집을 노려 분산 협공. 상위 2개 군집 반환.

### ③ 보스 본체 근접 FSM (`updateBossPersonalCombat`)
`EvaluateTarget → ChaseTarget → AttackWindup → AttackRecover` (Goblin `updateBossPersonalCombat` 미러,
score 기반 타깃 + `BOSS_TARGET_SWITCH_MARGIN`). 추가로 **피해 반응 Backstep/Retreat**:
`updateBossDamageReaction`이 HP 델타를 누적(`bossDamageSinceBackstep_`), `≥ BOSS_DAMAGE_REACTION_THRESHOLD(60)`
& 쿨다운(`3s`) 만료 시 `Backstep`(타깃 반대 `BOSS_BACKSTEP_DIST`) → `Retreat`(`BOSS_RETREAT_DIST` 이탈) →
재평가. **협공 phase 동안엔 `phase` 가드로 근접 루프 정지**(보스는 2차 쐐기에 직접 참여).

### ④ 보스의 2차 쐐기 합류 (`ensureBossBuddyWedgeJoin` 외)
`RegroupBuddies`에서 랜덤으로 합류할 Buddy 스쿼드 선택(`selectBossJoinedBuddySquad`). 해당 스쿼드 집결
완료 후 `setupBossBuddyWedgeJoin`이 apex prepare/exit 위치 산출. 보스는 prepare→exit로 모터 이동
(`updateBossBuddyWedgeJoin`). 그 스쿼드의 `WedgeCharge`에 `reserveWedgeApex=true`(보스 슬롯 비움) +
`wedgeDamageMult=1.5`(결정타) 세팅.

## 재사용한 기존 인프라 (신규 작성 없음)

| Isys 요구 | 재사용 |
|---|---|
| 쐐기 돌진 | `SquadOrderType::WedgeCharge` + `TacticalSquad` 쐐기 준비/release/charge (Goblin DivideAndConquer 경로) |
| **보스 합류 ×1.5 피해** | `SquadOrder::wedgeDamageMult` → `TacticalSquad.cpp` `impactDamage = WEDGE_CHARGE_DAMAGE × wedgeDamageMult`. **새 피해 메커니즘 불필요** |
| 보스 apex 슬롯 예약 | `SquadOrder::reserveWedgeApex` (쐐기 슬롯 1개 비움) |
| 후퇴/집결 대형 | `SquadOrderType::FormationHold` (+ `tacticCenter`,`formationTargetPos`,`slotColumnCount`,`speedMult`) |
| Engage 균형 배정 | `MidBossTacticBase::issueStableEngage` |
| 군집화 | `MidBossTacticBase::buildPlayerClusters` |
| 보스 근접 FSM | Goblin `updateBossPersonalCombat` 패턴 + `moveBossToward` 모터 |
| 쐐기 충돌 처리 | `Room::beginWedgeCharge/endWedgeCharge/tryApplyWedgeChargeHit` (charge당 플레이어 1회 피격) |
| 인카운터/존 | `Room::spawnGrandbaumEncounter` / `onArenaGrandbaumEnter` 미러 |

Grandbaum과 달리 Isys는 **새 패킷·넉백·피해경감·동적소환이 일절 없는** 순수 서버 AI 포팅이다.
`MidBossTactics.cpp/.hpp`는 이미 `.vcxproj`에 포함되어 신규 파일/프로젝트 수정도 없다.

### ⑤ 장애물 안전 슬롯과 교착 방지

Isys의 `FormationHold`/`WedgeCharge`만 정적 장애물 회피 옵션을 사용한다. 각 슬롯은 XZ 위치의 지형 높이로
Y를 맞춘 뒤 scatter prop과 아레나 벽/Static BVH 중첩을 검사한다. 막힌 슬롯은 1.5m 간격으로 최대 6m까지
가까운 빈 바닥을 탐색하며 다른 슬롯과 NPC 분리 반경을 유지한다. 나무·바위 위 높이는 사용하지 않는다.
쐐기 준비가 2.5초를 넘으면 현재 인원으로 강제 돌진하고, Buddy 재집결은 4초 후 2차 쐐기로 넘어가므로
일부 NPC가 이동 경로에서 막혀도 전술 전체가 정지하지 않는다.

## 보스 고속 이동 — 모터 변환 (중요)

시뮬은 보스를 `setPosition` 직접 적분(후퇴 ×15.5, 쐐기 돌진 ×28, 백스텝 ×20)으로 움직이지만,
GameServer 보스는 **물리 속도 모터(`setDesiredVel`)** 로 움직인다(`moveBossToward`). 모터는 시뮬 배율을
그대로 못 쓰므로 **인게임값으로 캡**했다(주석에 시뮬 원본 병기, M3 실검증 후 미세조정):

| 용도 | 인게임 캡 | 시뮬 원본 |
|---|---|---|
| 근접 chase | `BOSS_CHASE_SPEED_MULT` 1.0 | 5.35 |
| 후퇴 | `RETREAT_LEADER_SPEED_MULT` 6.0 | 15.5 |
| 쐐기 합류 이동 | `ISIS_BOSS_WEDGE_JOIN_SPEED_MULT` 6.0 | 15.5 |
| 쐐기 돌진 | `ISIS_BOSS_WEDGE_CHARGE_SPEED_MULT` 10.0 | 28.0 |
| 백스텝 | `BOSS_BACKSTEP_SPEED_MULT` 4.0 | 20.0 |

## 스케일 결정

- **월드 거리/반경/슬롯 오프셋**은 인게임 스케일 **×~0.4** 적용(주석에 시뮬 원본 병기). 예:
  `ISIS_RETREAT_MIN_DIST 36`(시뮬 90), `RETREAT_BUDDY_SIDE_OFFSET 11`(시뮬 28), `CLUSTER_RADIUS 8`(시뮬 20),
  `BOSS_BACKSTEP_DIST 7`(시뮬 18).
- **시간/비율/카운트/점수/spacing**(0.80, ×1000−d, 350, ×1.5, 타이머 5/3.5/7,
  spacing/column scale)은 **시뮬 원본 유지**하며, 협공 사이클 쿨다운은 게임 템포에 맞춰 **15~21초**로 조정한다.
- `WEDGE_PREP_APEX_DISTANCE`/`WEDGE_EXIT_DISTANCE`는 `TacticalSquad`의 GameServer 값(4/14, 인게임 스케일)을
  자동 사용.

## 주요 파일

| 파일 | 내용 |
|---|---|
| `IsysMidBossTactic.hpp/.cpp` | `IsysMidBossTactic`(보스별 전용 파일). NPCAI 구조 미러. 공용 유틸은 `MidBossTacticBase.hpp/.cpp`. |
| `Room.hpp/.cpp` | `spawnIsysEncounter`(Buddy 2 + Bomber 2 + Isys 보스), `onArenaIsysEnter`, zone 바인딩(`Arena_Isys`). |

(프로토콜/PacketManager/client/`.vcxproj` 변경 없음.)

## 트리거

- zone 태그 `"Arena_Isys"` + 마커 `WallIsys_0/1/2`(후방벽, 선택), `IsysSpawner`(없으면 Wall 중점 fallback).
  해당 아레나 진입만으로 트리거된다.
- 과거 한자리 비교용 디버그 트리거(`HOBGOBLIN_DEBUG_TACTIC`, `Arena_Hobgoblin` 재사용 + any-Wall/플레이어
  위치 fallback)는 **제거됨**. `Arena_Hobgoblin`은 다시 홉고블린 전용이며, Isys는 전용 마커가 저작된
  `Arena_Isys`에서만 스폰된다.

## 스탯/상수 (인게임)

- config(스폰): Buddy 80HP/4spd, Bomber 45HP/5spd, 보스 2000HP/4spd(홉고블린/Grandbaum 보스 선례).
  부대 인원 시뮬 원본 유지 — **Buddy 12/12 + Bomber 40/40 = 104기 + 보스(총 105기/방)**. → M3 성능 확인/튜닝 대상.
- Isys 쐐기 부대는 `chargeSpeedMult 2.0`으로 Buddy 24m/s, Bomber 30m/s를 목표로 하며, 부대와 보스 모두
  돌진 중 모터 가속도를 60m/s²로 일시 상향하고 종료 시 원래 값으로 복원한다.
- 모델: 전용 Isys/Buddy/Bomber 에셋 추가 전까지 `modelGoblin()`/`ObjectType::Goblin` 재사용(Grandbaum 선례).

## 빌드 상태

- **M1**: 전술 골격 + Engage 분산교전 + 보스 근접 FSM(백스텝 포함) + 80% unlock + 인카운터 스폰.
- **M2**: 협공 풀 사이클(후퇴→Bomber 집결→1차 쐐기→Buddy 집결+보스 합류→2차 쐐기 ×1.5→Cooldown).
- **M3**: 거리 인게임 스케일(×~0.4) + 보스 속도 모터 캡 + 본 문서/메모리. **실제 client 검증·밸런싱은
  `Arena_Isys` 레벨 마커 저작 후** 진행.

RoomServer Debug/x64 빌드 통과(오류 0/경고 0).

## 검증 방법

실행 검증은 `client`로(DummyClient 아님). RoomServer + LobbyServer + client 기동 → 방 입장 →
`Arena_Isys` zone 진입:
1. **Engage**: 4스쿼드 분산 교전, 보스 근접 FSM(추적→공격), 누적 피해 60↑ 시 백스텝 이탈.
2. **Unlock**: 한 스쿼드 인원 20%↑ 처치 → 협공 사이클 진입.
3. **1차 쐐기**: 전군 후퇴·집결 → Bomber가 최대·최근접 군집에 쐐기 돌진(피해).
4. **2차 쐐기**: Buddy 라인 + 보스 합류 → **다른 군집**(1차 페널티로 분산)에 쐐기 + **×1.5 피해**.
5. **루프**: Cooldown 후 Engage 복귀, 재차 80% 게이트로 재발동.

> **주의**: 디버그 fallback이 제거됐으므로 `Arena_Isys` zone과 `WallIsys_*`/`IsysSpawner` 마커가 레벨에
> 저작돼 있어야 Isys가 스폰된다(미저작 시 인카운터 스킵). 밸런싱(거리/속도 상수, 105기/방 성능,
> config 스탯/부대인원)은 후속.

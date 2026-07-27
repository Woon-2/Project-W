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

피해 프로파일은 `Engage/Cooldown=1.0`, `RetreatForPincer`부터 `SecondBuddyWedge` 종료까지
보스와 전 부대원 `0.0`이다. 부대가 먼저 전멸하면 진행 중 협공을 취소하고 `Engage`를 보스 단독 전투
상태로 재사용하며, 보스가 먼저 사망하면 모든 생존 부대원을 `1.0`으로 복구한다.

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

Isys의 전술 무적은 공용 `damageTakenMultiplier`만 사용하므로 **새 패킷·동적소환 없이** 서버 phase에서 처리한다.
`MidBossTactics.cpp/.hpp`는 이미 `.vcxproj`에 포함되어 신규 파일/프로젝트 수정도 없다.

### ⑤ 장애물 안전 슬롯과 교착 방지

Isys의 `FormationHold`/`WedgeCharge`만 정적 장애물 회피 옵션을 사용한다. 각 슬롯은 XZ 위치의 지형 높이로
Y를 맞춘 뒤 scatter prop과 아레나 벽/Static BVH 중첩을 검사한다. 막힌 슬롯은 1.5m 간격으로 최대 6m까지
가까운 빈 바닥을 탐색하며 다른 슬롯과 NPC 분리 반경을 유지한다. 나무·바위 위 높이는 사용하지 않는다.
쐐기 준비가 `WEDGE_PREP_FORCE_TIMEOUT`(4초)을 넘으면 현재 인원으로 강제 돌진하고, Buddy 재집결은 4초 후
2차 쐐기로 넘어가므로 일부 NPC가 이동 경로에서 막혀도 전술 전체가 정지하지 않는다.

### ⑥ 쐐기 준비와 돌진 명령 상태 분리

`TacticalSquad`는 쐐기 대형의 준비 완료(`wedgePrepared_`)와 실제 `ChargeThrough` 명령 발행 완료
(`wedgeChargeCommandIssued_`)를 별도로 관리한다. 실제 돌진 명령이 한 번 이상 발행되기 전에는
`areChargeMembersComplete()`가 완료를 반환하지 않으므로, 집결만 마친 Bomber가 돌진 없이 Engage로
넘어가지 않는다.

자연 준비 완료와 강제 시작은 동일한 돌진 시작 경로를 사용한다. 강제 시작 시 준비 슬롯 캐시가
비어 있으면 현재 명령으로 캐시를 다시 만든 뒤 즉시 돌진을 발행한다. 기존 대표 플레이어가 사망하거나
이탈한 경우에는 같은 군집의 다른 생존 플레이어를 우선 사용하고, 모두 유효하지 않으면 스쿼드에서 가장
가까운 생존 플레이어를 향한다. 슬롯 배치, 85% 준비·완료 기준, 돌진 속도와 피해는 기존 값을 유지한다.

### ⑦ 집결 앵커와 쐐기 완성 판정 (2026-07-27)

**두 돌격 라인은 모두 후퇴 집결지(`retreatTargetPos_`) 기준으로 세운다.** 1차
`issueBomberRegroup`이 예전엔 시뮬 잔재로 *군집 centroid* 기준(`cluster.centroid − 플레이어 평균
시선 × 11m`)이었다. `ISIS_RETREAT_MIN_DIST`(36m)까지 후퇴시켜 놓고 Bomber만 플레이어 11~14m
지점으로 되돌려 대형을 짜는 셈이라, "후퇴 → 대형 → 장거리 돌진"이라는 연출이 무너졌다. 지금은
2차 `issueBuddyColumn`과 동일하게 `retreatTargetPos_ + attackDir × RETREAT_BOMBER_FRONT_OFFSET
± right × RETREAT_BOMBER_SIDE_OFFSET`을 쓴다. **새 집결 대형을 추가할 때도 이 앵커 계약을 지킬 것** —
쐐기 준비 apex가 `스쿼드 centroid + forward × WEDGE_PREP_APEX_DISTANCE`라, 집결 위치가 곧 쐐기가
펼쳐지는 위치다.

**쐐기 준비 완성(`wedgePrepared_`)은 `SquadOrder::strictWedgeFormation` 옵트인으로 안착 래치를 쓴다**
(`TacticalSquad::areMembersSettledAtSlotsFraction`). 공용 `areMembersAtSlots()`의 허용 오차는
`separationRadius_ × 1.5`(≈4.5m)인데 쐐기 슬롯 간격은 가로 ~2.25m / 행 ~1.65m라, **허용 오차가
두 행을 덮어써서** 뭉친 덩어리가 그대로 "대형 완성"으로 통과해 V자가 화면에 나오지 않았다. 안착
래치(`isSettledAtSlot`, 히스테리시스 inner 0.25×sepRad / outer 0.7×sepRad)를 85% 비율로 집계해
실제로 자리를 잡은 뒤 출발한다.

> ⚠ 이 플래그는 **강제 돌진 타임아웃이 있는 전술에서만** 켤 것. 준비가 그만큼 느려지므로,
> 안전장치가 없으면 쐐기가 영영 출발하지 않을 수 있다. Isys는 `WEDGE_PREP_FORCE_TIMEOUT`이
> 보증한다. Goblin `DivideAndConquer`의 회랑 쐐기는 같은 `WedgeCharge` 경로를 공유하지만
> 플래그를 켜지 않아 기존 판정 그대로다.

### ⑧ 집결지 배치 규약 — 겹치면 안착이 느려진다 (2026-07-27)

`retreatTargetPos_`를 원점, forward를 타깃 군집 방향으로 놓고 **Bomber는 전방, Buddy는 후방**에
세운다. 두 대형은 forward 축으로 분리하며, 수정 후 범위는
Buddy `[−20.4, −7.6]` · Bomber 집결 `[0.25, 13.75]` · Bomber 쐐기 준비 `[−2.2, 11]`이다.

과거 두 가지가 겹쳐 80기가 서로 밀어내며 대형 준비가 지연됐다.
- `issueBuddyColumn`이 전진 오프셋에 **`RETREAT_BOMBER_FRONT_OFFSET`(Bomber용 상수)** 을 써서
  2차 대기열이 1차 쐐기 한복판에 섰다. Bomber가 군집 쪽으로 떠나던 시절엔 자리가 비어
  드러나지 않던 복사 실수다. 지금은 후퇴 배치와 같은 `RETREAT_BUDDY_BACK_OFFSET`(후방)을 쓰며,
  같은 상수를 공유하는 덕에 Buddy는 두 단계 사이에 사실상 제자리다.
- `BOMBER_REGROUP_COLUMN_SCALE`이 1.8이라 40기 블록이 29.7m 폭이 되어, 부대 중심 간격
  `RETREAT_BOMBER_SIDE_OFFSET × 2`(16m) 안에서 두 Bomber 부대가 서로 파고들었다.

> ⚠ **대형 폭은 부대 간 중심 간격보다 좁게 유지할 것.** 폭은
> `cols = ⌈√N × columnScale⌉` × `separationRadius × slotSpacingScale`로 결정된다.
> `findSafeFormationSlot`은 **같은 부대 슬롯끼리만** 간격을 검사하므로(부대별 호출) 교차 부대
> 침투는 아무도 막아주지 않는다. 인원(N)을 늘릴 때 폭이 함께 커진다는 점에 주의.

**집결 위치는 공용 후퇴 축(`retreatForwardDir_`) 하나로만 계산한다.** 예전엔
`issueBomberRegroup`/`issueBuddyColumn`이 각자 *자기 타깃 군집 방향*을 축으로 썼는데, 1·2차가
서로 다른 군집을 노리면 두 축이 벌어져(θ≈40°) 위 forward 구간의 5.4m 마진이 약 4.3m 겹침으로
뒤집혔다. 지금은 `issueRetreatForPincer`가 계산한 축을 저장해 세 배치가 공유하고, 부대별 타깃
방향은 바라보는 방향(`formationTargetPos`)과 쐐기 전진축에만 쓴다.

### ⑨ 슬롯 격자 간격 ≥ `findSafeFormationSlot`의 `minSpacing` (2026-07-27)

`findSafeFormationSlot`(`TacticalSquad.cpp`)은 후보 지점이 **이미 배치된 슬롯과 `minSpacing` 이상**
떨어져야 통과시킨다. 예전엔 호출부가 전부 `memberSeparationRadius_`(3.0m)를 넘겼는데, Isys 대형의
실제 격자 간격은 그보다 좁았다(집결 2.55~2.70m, Bomber 쐐기 가로 2.25 / 행 1.65m). 그래서

1. 첫 멤버를 뺀 **전원이 자기 슬롯에서 탈락**해 6m 링 탐색(4링 × 12샘플)으로 빠지고,
2. 밀집 블록 *안쪽* 슬롯을 받은 멤버는 48샘플이 전부 실패해 **"현재 위치"** 를 슬롯으로 받았다.

결과가 **후퇴하지 않고 제자리에 남는 NPC**였고, 이동 거리 0이라 `isAtSlot()`/`isSettledAtSlot()`이
모두 참이 되어 85% 도착 게이트를 오히려 더 쉽게 통과 — **아무 로그 없이 은폐**됐다. 그렇게 플레이어
군집(=1차 쐐기 관통점)에 남은 Buddy는 30 m/s·70kg Bomber 40기에 물리적으로 떠밀려 함께 돌진했다
(전술 NPC끼리 충돌 ON, 모터는 외력이 누적되는 소프트 P제어).

수정: 간격 계산을 `denseSlotSpacing` / `wedgeColSpacing` / `wedgeRowSpacing`(`TacticalSquad.hpp`)으로
단일화해 **생성기와 회피 보정이 같은 값을 보게** 했고, 링 탐색 실패 시 곧장 현재 위치로 떨어지는
대신 **이웃 간격 조건만 빼고 원래 슬롯을 재시도**하도록 폴백을 2단으로 완화했다.

> ⚠ **새 대형을 추가할 때 `slotSpacingScale`/`wedgeSpacingMult`를 낮추면 반드시 위 헬퍼를 통해
> `minSpacing`도 함께 내려갈 것.** 보정 쪽이 격자보다 크면 대형이 통째로 무너지는데, 증상이
> "일부 NPC만 안 움직임"으로 나타나 원인 추적이 어렵다.

또한 `FormationHold` 커맨드는 `useHoldFacing`을 켜서 발행한다. 이게 없으면 `ord.targetId`(특정
플레이어 1명)가 죽는 순간 `TacticalNpc::updateHoldSlot`이 슬롯을 포기하고 **부대 전원이 그 자리에서
`Idle`** 이 된다 — 대형 유지에 타깃이 필요 없는데도 생기는 프리즈다. `FormationGuard`(Goblin 회랑
차단선)는 같은 case를 공유하므로 플래그를 켜지 않아 기존 거동 그대로다.

**남은 구조적 결함(미수정):** `pushCommandsToMembers`가 대상 조회 실패로 early return 해도
`TacticalSquad::update`는 `orderDirty_`를 무조건 클리어해 **명령이 영구 소실**된다. 위 `useHoldFacing`
덕에 Isys 경로의 실피해는 사라졌지만 경로 자체는 남아 있다. 그리고 `selectStrikeClusters`의 정렬
1순위가 인원수라 `SECOND_STRIKE_REPEAT_PENALTY`가 **인원 동수 군집에서만** 작동한다 — 1·2차 타깃
분산이 설계 의도만큼 되지 않을 수 있다(밸런스 판단 필요).

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

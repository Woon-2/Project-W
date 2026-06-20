# Grandbaum(그랜드밤) 중간보스 전술 — 설계/구현 문서

NPCAI 프로토타입(`D:\source\repos\Project-W\NPCAI\NPCAI\sim`)의 Grandbaum 전술을 `RoomServer`로
포팅한 결과를 기록한다. 홉고블린(`GoblinMidBossTactic`) 포팅 인프라를 재사용한다.

## 전술 개요

Grandbaum의 전술은 **단 하나(ShieldWall)**. 보스는 평소 표적 우선순위 melee만 하고, 보스 HP가
**66% / 33%** 에 도달하면 각 1회 ShieldWall이 발동한다.

- **발동 게이트** (둘 다 충족해야 발동, 하나라도 불충족 시 스킵→Cooldown):
  - 살아있는 슬라임 ≥ 10 (`MIN_SHIELD_WALL_SLIME_COUNT`)
  - 살아있는 원본 뱀 ≥ 1 (뱀이 없으면 매복이 성립 안 함)
- **발동 효과**: 슬라임이 보스를 원형으로 둘러싸 **하드 블로커(플레이어 통과 불가)** 가 되고,
  **보스 + 슬라임이 받는 피해 90% 감소(×0.1)**. 슬라임을 뚫고 보스에게 접근 불가.
- **파훼(보스를 다시 취약하게)**: 오직 뱀.
  - 예방: 발동 전(해당 HP 도달 전) 원본 뱀 부대 전멸 → 발동 스킵.
  - 해제: 발동 후 등장하는 증원 뱀 웨이브를 전부 처치 → ShieldWall 종료.
- 루프: `Engage` ⇄ `ShieldWall` → `Cooldown(8s)` → `Engage`. 두 발동(66/33)은 독립적이며 NPC가
  이월되지 않는다.

### 보스 표적 우선순위 (`selectBossMeleeTarget`)

원본 뱀 보존 중(`shieldWallTriggerStage_ < 2`)에 한해 **SnakeThreat > SlimeThreat**, 그 외엔 항상
**Nearest**. 자기 자원(뱀/슬라임)을 위협하는 플레이어를 우선 응징한다.

### 뱀 매복 4단계 (`SnakeAmbushStage`)

`Evasion`(원본 뱀 개별 회피·산개) → `RetreatingOriginal`(원본 뱀 외곽 후퇴 →
외곽 도착/타임아웃 시 웨이브 소환 직후 **퇴장(숨김)**) →
`WaveActive`(증원 웨이브 = 원본수×10, 최대 60, 4의 배수 / `DistributedEngage`) →
`ReturningOriginal`(웨이브 전멸 시 종료 + 원본 뱀 부활/복귀).

**원본 뱀 퇴장(숨김)**: 후퇴만 시키면 원본 뱀이 외곽에 정지한 채 웨이브 내내 플레이어에게 노출되므로,
`despawnOriginalSnakeSquad`가 후퇴 완료한 원본 뱀을 서버 상태상 사망(hp0+물리제거, 객체는 시체로
유지 → roster 부활 가능)으로 전환하고 클라엔 `S_NpcHide`로 **시체/죽는 연출 없이 즉시 숨김**.
복귀는 `reviveOriginalSnakeSquad`(`hp<=0` 분기 → `reviveTacticalNpc`→`S_NpcRespawn`)가 hidden 해제.

## 클라-서버 핵심 과제

### 1. 데미지 경감 (ShieldWall 90%)
`Object::damageTakenMultiplier_`(기본 1.0). 스킬 피격 적용부(`Room::updateSkillSystem`)에서
`hit->damage × tgt->damageTakenMultiplier()`. `applyShieldWallProtection`이 보스+슬라임에 0.1 적용/해제.

### 2. 슬라임 하드 블로커
평소 전술 trooper는 `setCollisionMask(~(Player|Boss))`로 플레이어를 통과. ShieldWall 중에는
`Room::setShieldWallBlockers`가 슬라임 마스크를 `~Boss`로 바꿔 **플레이어와 충돌(벽)**, 종료 시
`clearShieldWallBlockers`가 원복. 슬라임은 클라에도 존재하므로 클라 로컬 물리도 자연히 막힌다.

### 3. 플레이어 넉백 + 이동잠금 (이동 권한 = 클라)
플레이어 이동은 **클라이언트 권한**(클라가 위치 계산→`C_Move`, 서버는 신뢰+클램프). 서버가
`player->setPos()`로 밀어도 클라가 덮어쓰므로 무효 → **서버→클라 명령 패킷** 방식.
- 신규 패킷 `S_PlayerKnockback{ playerId, dirX, dirZ, speed, knockMs, postLockMs }`.
- 서버 `Room::knockPlayersOutOfShieldWall`: 링 안쪽 플레이어에게 발행 + `GameSession`에 클램프 면제 부여.
- 클라 `Game::onPlayerKnockback`/`processInputGame`: `knockMs`(0.32s) 강제 이동 → `postLockMs`(1.2s)
  입력잠금. 두 구간 모두 위치를 `C_Move`로 서버에 반영(권한 유지). 서버는 그동안 클램프 면제.

### 4. 전투 중 동적 소환/디스폰 (증원 웨이브)
`Room::spawnTacticalWaveNpc`(바디 등록) / `addDynamicTacticalSquad` / `removeTacticalNpcById` /
`removeTacticalSquadById` / `broadcastTacticalNpcSpawn`(클라 통지 `S_NpcSpawnBatch`) /
`reviveTacticalNpc`(부활 시 물리 재등록 + `S_NpcRespawn`).
`tacticalNpcs_`는 `unique_ptr` 벡터라 재할당돼도 객체 주소(raw 포인터)는 불변 → tactic 실행 중
(분대/NPC 순회 이전)에 즉시 push/erase해도 안전. 살아있는 웨이브 강제 정리는 `setHp(0)+S_Hit`로 대체한다.

### 5. NPC 숨김 (원본 뱀 퇴장)
신규 패킷 `S_NpcHide{ npcId[] }`. `Room::despawnTacticalNpcHidden`이 서버 상태를 사망(hp0+물리제거,
객체 유지)으로 두고, 호출부(`despawnOriginalSnakeSquad`)가 살아있던 id를 묶어 `S_NpcHide` broadcast.
클라(`Object::hidden_`, 공통 베이스)는 사망(`isDead_`/시체/래그돌)과 별개로 **렌더/업데이트/HP바에서
완전 제외**(`Game::hideNpcs`). 복귀는 `S_NpcRespawn`이 `hidden_`을 해제(`Game::onNpcRespawn`).
`hidden_`은 타입 독립이라 전용 NPC 타입 도입 시 `hideNpcs`의 id 조회만 통합하면 됨.

## 주요 파일

| 파일 | 내용 |
|---|---|
| `GrandbaumMidBossTactic.hpp/.cpp` | `GrandbaumMidBossTactic`(보스별 전용 파일). NPCAI 구조 미러. 공용 유틸은 `MidBossTacticBase.hpp/.cpp`. |
| `Room.hpp/.cpp` | `spawnGrandbaumEncounter`(이종 4부대), `onArenaGrandbaumEnter`, ShieldWall/넉백/동적소환 헬퍼, 데미지 훅, `move` 클램프 면제. |
| `object.hpp` | `damageTakenMultiplier_`. |
| `GameSession.hpp` | 넉백 클램프 면제 타이머. |
| `ServerEngine/protocol.hpp` | `S_PlayerKnockback` 패킷. |
| `RoomServer/PacketManager.*` | `makeSPlayerKnockbackPacket`. |
| `client/PacketManager.*`, `client/online/onlineGame.*` | 넉백 핸들러 + 로컬 상태머신. |

## 교전 배정 / 공격권 예약

- **공격권 예약**은 `TacticalNpc` 자체 기능. 상태 핸들러가 `canEnterAttackSlot()` →
  `Room::tryReserveTacticalAttackSlot`(targetId당 최대 5슬롯, `tacticalNpcs_` 전체 후보)를 직접 호출 →
  전술 무관. Grandbaum 슬라임/뱀/웨이브 모두 자동 적용(보스는 1기라 불필요).
- **슬라임 부대(0,1,2) Engage**는 `MidBossTacticBase::issueStableEngage`(균형배정 + 생존중 고정,
  Goblin/Grandbaum 공용)를 재사용. 원본 뱀(3)은 personal 회피(HoldSlot), 증원 웨이브는
  `DistributedEngage`. (issueStableEngage는 원래 GoblinMidBossTactic private였으나 base로 승격.)

## 트리거

Zone 태그 `"Arena_Grandbaum"` + 마커 `WallGrandbaum_0/1/2`(후방벽), `GrandbaumSpawner`(없으면 Wall
중점 fallback). 해당 아레나 진입만으로 트리거된다. (과거 한자리 비교용 디버그 트리거
`HOBGOBLIN_DEBUG_TACTIC`는 제거됨 — `Arena_Hobgoblin`은 다시 홉고블린 전용.)

## 스탯/상수 (인게임 스케일)

- 거리/반경/슬롯간격 상수는 **인게임 스케일 ×~0.4 적용 완료**(코드에 시뮬 원본 병기, 예:
  `SNAKE_OUTER_RADIUS 26`(시뮬 64), 링 반경 `3~5`(시뮬 7~12)). 실검증 후 미세조정.
  시간/비율(경감 0.1, 66/33, ×10/최대60, 락 타이머)·카운트는 시뮬 원본 유지.
- config(스폰): 슬라임 60HP/4spd, 뱀 45HP/8spd, 보스 2000HP/4spd (홉고블린 보스 2000 선례).
  부대 A12/B12/C48/D10. → 모두 M3 튜닝 대상.
- 모델: 전용 슬라임/뱀/보스 에셋 추가 전까지 `modelGoblin()`/`ObjectType::Goblin` 재사용(홉고블린과 동일).

## 빌드 순서(완료 상태)

- **M1**: 데미지 훅, 전술 골격(표적우선순위/뱀회피/ShieldWall 발동·링·경감·블로커), 넉백. → 예방 경로 동작.
- **M2**: 동적 소환 인프라 + 뱀 매복 풀 루프(후퇴→웨이브→전멸→종료→부활). → 해제 경로 동작.
- **M3**: 거리 상수 인게임 스케일(×~0.4) 적용 + 본 문서/메모리 작성 완료. **실제 client 검증·밸런싱은
  `Arena_Grandbaum` 레벨 마커 저작 후** 진행(미완).

RoomServer · client 모두 Debug/x64 빌드 통과.

## 검증 방법

실행 검증은 `client`로(DummyClient 아님). RoomServer+LobbyServer+client 기동 → 방 입장 →
`Arena_Grandbaum` zone 진입.
1. 예방: 66% 전 뱀 부대(D) 전멸 → 66% 도달 시 ShieldWall **스킵** 확인.
2. 해제: 뱀 남긴 채 66% 도달 → 슬라임 링(통과 불가)+보스 90% 경감 → 증원 웨이브 스폰 → 전멸 시
   링 해제·재취약. 33%에서 1회 재현(독립).
3. 넉백: 링 형성 시 안쪽 플레이어 0.32s 밀려남 + 1.2s 입력잠금.
4. 표적 우선순위: 뱀 근처 플레이어 우선 추적.

> **주의**: 디버그 fallback이 제거됐으므로 `Arena_Grandbaum` zone과 `WallGrandbaum_*`/`GrandbaumSpawner`
> 마커가 레벨에 저작돼 있어야 Grandbaum이 스폰된다(미저작 시 인카운터 스킵).

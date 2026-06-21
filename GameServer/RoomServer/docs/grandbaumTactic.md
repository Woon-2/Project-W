# Grandbaum(그랜드밤) 중간보스 전술 — 설계/구현 문서

## 재설계(2026-06-21): "피해경감(DR) 토글" 방식

기존 뱀 매복(후퇴→증원 웨이브 소환→숨김→전멸→부활) 설계는 **전투 중 NPC/스쿼드를 런타임 생성·삭제**
하는 구조라 ShieldWall 발동 즈음 크래시가 났다(특히 `Room::removeTacticalNpcById`가 NPC를 free하면서
`TacticalSquad::memberCache_`의 raw 포인터를 스크럽하지 않아 use-after-free 가능). 이를 **전부 제거**하고,
처음 스폰된 NPC만으로 돌아가는 단일 메커니즘으로 재설계했다.

**핵심: 슬라임이 받는 피해 경감(DR)을 상황에 따라 토글한다.** 뱀 부대는 슬라임으로 교체(뱀 특수로직 제거).

| 상태 | 보스 DR | 슬라임 DR | 의도 |
|---|---|---|---|
| 평상시(Engage/Cooldown) | 1.0 (취약) | **0.0 (무적)** | 플레이어가 보스에 집중하고 2페이즈용 슬라임을 보존 |
| ShieldWall 발동 중 | **0.1 (90% 경감)** | **0.5 (50% 경감)** | 보스는 보호되고 벽 슬라임을 처치해 파훼 |
| 보스 사망 후(혼란) | (사망) | **1.0 (취약)** | 잔여 슬라임 정리 → 아레나 벽 해제 |

### 전투 흐름

1. **평상시**: 슬라임 무적(0.0)·보스 취약 → 플레이어가 보스 집중. 슬라임은 `issueStableEngage`로 교전.
2. **66% / 33% 도달 → ShieldWall 발동**: 플레이어 넉백 + 전 슬라임 부대가 보스를 원형 벽(RingGuard)으로
   감싸 **하드 블로커**가 되고 **보스 DR 0.1(보호)**. 이때 슬라임 DR을 0.5로 풀어 파훼 가능하게 한다.
   HP가 66%와 33%를 서로 다른 시점에 통과하면 각 단계에서 한 번씩 발동한다. 한 번의 전술 업데이트에서
   66%를 건너뛰고 바로 33% 이하가 되면 현재 HP 단계 2를 한 번만 소비하므로 ShieldWall도 한 번만 발동한다.
   첫 ShieldWall 중 33% 이하가 되면 현재 벽 파훼 직후 Cooldown 없이 두 번째로 발동한다. Cooldown 중 33%에
   도달하면 남은 Cooldown을 중단하고 즉시 발동해, 보스가 취약한 동안 사망해 2차 발동이 유실되지 않게 한다.
3. **파훼**: 벽 슬라임을 형성 시점 대비 `SHIELD_BREAK_KILL_FRACTION`(0.2) 이상 처치해야만
   ShieldWall 종료 → 평상시 DR 복귀 → `Cooldown(8s)` → `Engage`.
4. **보스 사망**: `onLeaderDead`에서 잔여(혼란 상태) 슬라임 DR을 영구 1.0으로 해제 → 깔끔히 정리.

발동 게이트: **살아있는 슬라임 ≥ `MIN_SHIELD_WALL_SLIME_COUNT`(10)** 만 충족하면 발동, 아니면 스킵→Cooldown.
2페이즈가 가능한 이유: 평상시 슬라임이 무적이므로 33% 발동 시에도 벽을 세울 슬라임이 남는다.

## 핵심 메커니즘

### 1. 데미지 경감 적용부 (필수 — 기존엔 비활성)
`Object::damageTakenMultiplier_`(기본 1.0). **기존엔 적용부가 주석 처리돼 동작하지 않았다.** 재설계의 토대라
두 플레이어→NPC 경로 모두에서 활성화:
- 스킬 피격: `Room::updateSkillSystem`에서 `hit->damage × tgt->damageTakenMultiplier()`.
- 레거시 평타: `tryMeleeTactical`(스킬과 별도 경로)에서 동일하게 `kDamage × o->damageTakenMultiplier()`.

DR 토글은 `GrandbaumMidBossTactic`이 소유: `applyNormalDamageProfile`(보스1.0/슬라임0.0+블로커해제) /
`applyShieldWallProtection`(보스0.1/슬라임0.5+블로커) / `setAllSlimeDamageMultiplier`. 스폰 시 슬라임 0.0 초기화.

### 2. 슬라임 차단벽 (전적으로 클라 권위)
차단은 **클라 barrier가 전담**한다(goblin divide와 동일). `Room::setShieldWallBlockers`는 **서버 충돌을 일절
건드리지 않고**(슬라임은 스폰 기본 mask `~(Player|Boss)`=플레이어 통과 유지) `S_NpcBarrier(true, ids)` 패킷만
broadcast → 클라 `Game::setNpcBarrier`가 barrier 등록 → `Game::resolveBarrierSeparation`(매 프레임)이 인접 살아있는
슬라임을 선분 캡슐로 이어 로컬 플레이어를 밀어낸다(하드 월).
> **하지 말 것**: 서버에서 슬라임 mask에 Player 충돌을 켜면, 플레이어 바디(Kinematic)가 슬라임(Dynamic)을
> **밀어내** 벽이 무너진다(클라 슬라임은 Kinematic이라 안 밀리지만 서버 변위가 broadcast됨). 패킷 전용이 정답.

ShieldWall 활성 중에는 벽 슬라임과 그랜드밤 보스 모두 스킬 `OnHit` impulse를 무시한다. 보스 ID는
`S_NpcBarrier.impulseOnlyNpcId`로 별도 전달하므로 보스는 플레이어 차단벽에는 포함되지 않으며, 피해·피격 연출과
슬라임의 RingGuard 이동은 그대로 유지된다. ShieldWall 종료 시 양쪽 면역을 즉시 해제한다.
> **버그였던 부분**: `setNpcBarrier`가 `idGoblinMap_`(goblin/hobgoblin 전용)에서 조회해 **슬라임이 안 잡혀
> barrier가 아예 비활성**이었다(플레이어가 그냥 통과). 전 몬스터를 담는 `idMonsterMap_`(Object*) 조회로 수정.
> 또 연속 벽이 되려면 인접 슬라임이 `kBarrierLinkDist 2.9` 이내여야 해 슬라임 `separationRadius`를 3.0→2.0으로 낮춤.
> **한계(설계상)**: 죽은 슬라임은 연결에서 제외돼 그 자리에 **틈**이 생기고, 빠른 대시는 순간 관통 여지가
> 있다. 즉 벽은 완전 불가침이 아니다. 그래서 **ShieldWall 중 보스 DR 0.1을 유지**해, 틈으로 진입해 보스를
> 직접 녹이는 우회(치즈)를 막고 파훼가 반드시 "슬라임 처치 수"로 일어나게 한다.

### 3. 플레이어 넉백 + 이동잠금 (이동 권한 = 클라)
링 형성 순간 안쪽 플레이어를 밖으로 밀어낸다. 신규 패킷 `S_PlayerKnockback{ playerId, dirX, dirZ, speed,
knockMs, postLockMs }`. 서버 `Room::knockPlayersOutOfShieldWall`(살아있는 플레이어만 순회 — NPC 포인터
미접촉, 크래시 무관) + `GameSession` 클램프 면제. 클라 `Game::onPlayerKnockback`이 로컬 넉백+입력잠금.
형성 완료 또는 `SHIELD_WALL_FORM_KNOCK_MAX` 6초 경과 시 **반복 넉백만** 중단한다. 6초는 ShieldWall
종료 시간이 아니며, 슬라임 barrier·보스 DR·ShieldWall 페이즈는 파훼 전까지 계속 유지된다.

### 4. 밸런스
평상시 슬라임은 무적이므로 "못 잡는 딜러"가 되지 않도록 위협을 보스로 집중. **공격력 레버는
`attackDamageScale`** — 등록 스킬을 쓰는 NPC의 데미지는 `skillSystem.cpp:933`에서 `lua damage × damageCoeff ×
attackDamageScale`로 산정되며, 레거시 `TacticalNpcConfig::attackDamage`는 스킬 없을 때만 쓰이는 폴백(미사용).
- 슬라임 공격↓: `TacticalSlime::trooperConfig()` `attackDamageScale 0.3`(슬라임 = Slime_Attack1 lua 9 × 0.3).
- 보스 공격↑: `bossCfg`(spawnGrandbaumEncounter) `attackDamageScale 5.0`(보스 = Treant 스킬 × 5.0).
- 수치는 실검증 튜닝용 플레이스홀더.

## 주요 파일

| 파일 | 내용 |
|---|---|
| `GrandbaumMidBossTactic.hpp/.cpp` | DR 토글 전술 전체(보스 melee, ShieldWall 발동/형성/파훼, DR 프로파일). |
| `Room.cpp` | `spawnGrandbaumEncounter`(슬라임 4부대 20명씩 + 초기 DR 0.0), `onArenaGrandbaumEnter`, 데미지 경감 적용부(2곳), ShieldWall 넉백/블로커 헬퍼. |
| `TacticalSlime.cpp` | 슬라임 trooper config(공격력 하향). |
| `object.hpp` | `damageTakenMultiplier_`. |
| `ServerEngine/protocol.hpp`, `PacketManager.*` | `S_PlayerKnockback`, `S_NpcBarrier`. |
| `client/online/onlineGame.cpp` | 넉백 핸들러, `setNpcBarrier`/`resolveBarrierSeparation`(벽 차단). |

> 제거됨: `spawnTacticalWaveNpc`/`addDynamicTacticalSquad`/`removeTacticalNpcById`/`removeTacticalSquadById`/
> `broadcastTacticalNpcSpawn`/`reviveTacticalNpc`/`despawnTacticalNpcHidden`(런타임 스폰·디스폰 인프라, 전부
> 미사용·크래시 원인). 전술의 뱀 매복/웨이브/숨김·부활/SnakeAmbushStage 일체. `TacticalSnake`는 현재 미사용.
> `S_NpcHide`/`S_NpcRespawn`/`S_NpcSpawnBatch`(전투 중)도 더 이상 발생하지 않음(초기 스폰 배치는 유지).

## 트리거

Zone 태그 `"Arena_Grandbaum"` + 마커 `WallGrandbaum_0/1/2`(후방벽), `GrandbaumSpawner`(없으면 Wall 중점
fallback). 해당 아레나 진입만으로 트리거. (마커는 `resources/terrains/chunks_index.bin`에 저작돼 있음.)

## 상수 (`GrandbaumMidBossTactic.hpp`)

- HP 임계 66%/33%, `MIN_SHIELD_WALL_SLIME_COUNT 10`, 링 반경 `3~8`, `SHIELD_BREAK_KILL_FRACTION 0.2`,
  `SHIELD_WALL_FORM_KNOCK_MAX 6s`, `TACTIC_COOLDOWN_DURATION 8s`,
  ShieldWall 보스 DR `0.1`, ShieldWall 슬라임 DR `0.5`, 슬라임 평상시 DR `0.0`. 거리/반경/비율은 실검증 튜닝 대상.
- **방패벽 연속성**: RingGuard는 전역 고유 슬롯을 다겹(multi-lane) 링으로 생성한다. ShieldWall 중에는
  슬라임 상호 물리 충돌을 끄고 원주·방사 슬롯 간격을 1.4로 유지한다. 클라 barrier 연결거리
  (`kBarrierLinkDist 2.9`)보다 충분히 짧아 연속 벽이 유지된다. 80마리 기준 지름 16m,
  `32@8.0 / 28@6.6 / 20@5.2`의 3겹을 형성한다.
- **생존 인원 기반 수축**: 다음 ShieldWall은 살아있는 슬라임 수로 반경을 다시 계산한다. 모든 링의 원주
  간격을 2.8 이하로 제한하므로, 64마리는 기존 `32/28/4@8m`의 성긴 안쪽 링 대신 지름 약 12.8m,
  `28@6.4 / 20@5.0 / 16@3.6`의 닫힌 3겹을 형성한다. 넉백 안전 반경도 함께 수축한다.

## 검증 방법

실행 검증은 `client`로(DummyClient 아님). RoomServer+LobbyServer+client 기동 → 방 입장 → `Arena_Grandbaum` 진입.
1. **DR 동작**: 평상시 슬라임은 HP가 감소하지 않고 보스는 풀데미지를 받는다.
2. **발동/파훼**: 66% 도달 → 넉백 + 슬라임 벽(통과 불가, 죽은 자리 틈) + 보스 90% 경감 + 슬라임 50% 경감, **크래시 없음**.
   벽 슬라임 일정 수 처치 → 실드 해제·보스 재취약. 30초 이상 대기만 해서는 해제되지 않음. 33% 재현(보존된 슬라임으로 벽).
   `100%→60%→30%`는 총 2회, 한 번에 `100%→30%`는 총 1회만 발동하고 HP 회복 후 재하락해도 중복 발동하지 않는다.
   첫 ShieldWall 중 33% 도달 시 파훼 직후 연속 발동하며, Cooldown 중 도달 시 다음 전술 틱에 즉시 발동한다.
3. **치즈 방지**: 틈으로 보스 접근해도 보스 DR로 직접 처치 비효율 → 결국 슬라임 처치로 파훼.
4. **보스 사후**: 잔여 슬라임 취약 전환 → 정리 → 아레나 벽 해제.
5. **엣지**: 슬라임 임계 미만이면 ShieldWall 스킵.

RoomServer Debug/x64 빌드 통과(경고 0/오류 0). 클라 코드 변경 없음(기존 바이너리 호환).

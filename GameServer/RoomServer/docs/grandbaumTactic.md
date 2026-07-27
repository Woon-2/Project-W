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
   보스는 근접을 멈추고 **원거리 흙 기둥 포격**으로 전환한다(§5).
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

### 5. ShieldWall 중 원거리 흙 기둥 포격 (2026-07-27)

**해결한 문제**: 벽이 서면 플레이어는 링 바깥(반경 최대 8m)으로 넉백되는데 보스 melee 사거리는
3.5m다. `enterPhase(ShieldWall)`이 `resetBossMelee`로 보스를 세워 놓기까지 해서, 이 페이즈 동안
보스는 **문자 그대로 아무것도 하지 않았다**. 파훼 조건도 시간이 아니라 벽 슬라임 20% 처치뿐이라
페이즈 전체가 위협 없는 노가다 구간이 됐다.

**패턴**: 예고 후 작렬. `Treant_Clap` 모션 → 대상 발밑에 앰버 마법진 예고(t=300ms) → 0.8초 뒤
갈색 흙 기둥 융기 + 히트박스(t=1100ms). **앵커는 시전 시점에 고정되고 추적하지 않으므로,
예고를 보고 걸어 나가면 회피된다** — 그 창이 이 패턴의 유일한 대응 수단이므로 텔레그래프 간격을
줄이지 말 것.

**타겟팅**: 살아있는 플레이어를 **playerId 오름차순 순환**(인덱스가 아니라 id 기준이라 도중에
누가 죽어도 순번이 어긋나지 않는다). 앵커 = `Player::estimatedPos(room.getElapsedMs())`(lag comp).

**구현**:
- `GrandbaumMidBossTactic::updateShieldWallBarrage` — `Phase::ShieldWall` 블록에서 매 틱 호출.
  페이즈를 벗어나면(파훼·보스 사망) 호출 자체가 끊기므로 별도 정지 처리가 없다.
- `PlatoonLeader::castSkillAt(room, skillId, clipKey, anchorPos, damageScale)` — 기존
  `castSkillAttack`은 `pickAttack()` **랜덤**이라 지정 시전에 못 쓴다. 앵커 + 명시 damageScale용 별도 경로.
- 스킬: `resources/skills/grandbaum_earth_spike.lua` (`skill.name = "Grandbaum_EarthSpike"`).
  VFX 전용 인스턴스 2종 — **vfxId 19 = 앰버 마법진 예고**(`magic_circle.dds`, World 정렬 빌보드),
  **vfxId 20 = 갈색 흙 기둥**(`IceSpikes2` 메시 + `MatTwoSides`, spikes 스킬과 같은 소재).

  > **⚠ 갈색을 얻으려면 소재 선택이 먼저다 (1차 시도 실패의 원인).** 처음엔 crystal 아트
  > (`CrystalFree1.dds`)에 갈색 `ps::MatUnlit::color`를 곱했는데 **결과가 거의 검정**이었다.
  > tint는 곱셈이라 색을 뺄 수만 있고 더할 수 없는데, 그 텍스처는 평균 RGB (0.03, 0.36, 0.73)의
  > **완전 채도 파랑**이라 R이 0.03이다 — 어떤 갈색 배율을 곱해도 R이 살아나지 않는다.
  > (원본이 `.color = {1.15, 1.18, 1.41}`처럼 1을 넘는 값을 쓰는 것도 이 텍스처를 *밝히려는* 것이다.)
  > **곱셈 틴트는 채도 0(그레이스케일) 텍스처에서만 임의 색을 낸다.** 채도 확인:
  > `Stone/Circle/magic_circle/Noise*` = sat 0.00(틴트 가능), `CrystalFree1` = sat 0.99(불가).
  > 메시 소재(`MatTwoSides`)는 색이 전부 코드 값(`frontFaces/backFaces/fresnel`)이라 이 문제가 없다.

  > **⚠ 지면에 눕는 빌보드**: `RendererModule::Alignment::World`면 빌보드가 카메라를 향하지 않고
  > 파티클 회전을 쓴다. 단 그 회전은 **`billboardRotation3D`(= `main.startRotation3D` 오일러)에서만**
  > 오고 이펙트 play 방향(`baseRotation`)은 안 탄다 — 그래서 lua의 `orient`/`groundAlign`이 아니라
  > **C++ cfg에서 X축 -90°로 눕혀야 한다**. 반대로 메시 파티클은 `baseRotation`을 쓰므로
  > `particleConform="SnapAndAlign"`의 슬로프 정렬이 실제로 먹는다.
- 앵커 배선은 신규 엔진 프리미티브 2개에 얹혀 있다 — `client/docs/terrainInteractingSkills.md`
  "앵커 오버라이드" / "PlayVFX의 Ground attach".

**함정 / 불변식**:
- ⚠ `SHIELDWALL_BARRAGE_INTERVAL(1.6s)` > lua `totalDurationMs(1400)`. `skillStartInternal`이
  `hasActiveSkill` 가드로 중복 캐스트를 **조용히** 버리므로, 간격을 줄이거나 스킬을 늘리면
  포격이 소리 없이 절반만 나간다.
- ⚠ `SHIELDWALL_BARRAGE_DAMAGE_SCALE = 1.0`을 **명시로** 넘긴다. 보스의 `attackDamageScale`은
  5.0(`Room.cpp` bossCfg)이라 그대로 두면 lua의 50이 250이 된다. 전용 스킬은 lua 숫자가 곧 실 데미지다.
- 히트박스가 `GroundAttach`(정적 OBB)라 **파티클 결정론 계약(`addVFX{systems=...}`)이 불필요**하다.
  VFX는 순수 연출, 판정은 앵커에 고정된 OBB가 담당한다.
- 에어본은 `OnHit.impulseDir.y`로 **클라에서** 걸린다. 서버 플레이어는 Kinematic이라 임펄스가
  no-op이지만, 플레이어 위치 권위는 어차피 클라에 있고 로컬 플레이어 바디만 Dynamic이다.

**밸런스 현황(미검증 플레이스홀더)**: damage 50 = 플레이어 최대 HP 5000의 **1%**. 회피까지
가능하므로 실질 위협은 사실상 0이다(30초 내내 맞아도 -375, HP 재생이 일부 상쇄). 사용자가
원안 수치를 유지하기로 선택한 값이며, `grandbaum_earth_spike.lua`의 `damage` 한 줄이라
플레이 테스트 후 올리기 쉽다.

### 6. 발동 순간 넉백 잔여 속도 제거 (2026-07-27)

링 중심(`shieldWallRingCenter_`)은 `issueShieldWall`에서 보스 위치를 **1회 스냅샷**하고 슬롯도
절대 좌표로 박제된다. 그런데 미드보스는 평상시 넉백 면역이 아니라(Dynamic mass 70), 임계
(66%/33%)를 깨는 바로 그 히트의 OnHit 임펄스(350~1200 → 5~17 m/s)가 ShieldWall 진입 시 켜지는
impulse 면역보다 먼저 실린다. 면역은 이후 임펄스만 막고 잔여 `linearVel`은 안 지우므로
(linearDamping 0.1), 보스가 캡처된 중심에서 수 미터 미끄러져 **벽 밖에 서 있는** 버그가 있었다.

수정: `issueShieldWall`이 중심을 캡처하기 직전에 보스 XZ `setLinearVel(0)` (Y는 중력 보존).
면역이 같은 틱에 먼저 켜지므로 이후 새 임펄스도 없다 → 보스는 링 중심에 정지 유지.
66%/33% 두 발동 모두 이 경로를 지난다. Engage/Cooldown 중 넉백 카운터플레이는 그대로다.

## 주요 파일

| 파일 | 내용 |
|---|---|
| `GrandbaumMidBossTactic.hpp/.cpp` | DR 토글 전술 전체(보스 melee, ShieldWall 발동/형성/파훼, DR 프로파일, ShieldWall 원거리 포격). |
| `PlatoonLeader.hpp/.cpp` | `castSkillAt`(지정 스킬 + 앵커 시전). |
| `resources/skills/grandbaum_earth_spike.lua` | 포격 스킬(예고→융기→히트박스). |
| `client/online/onlineGame.cpp`, `client/standalone/game.cpp` | 예고 마법진 + 갈색 흙 기둥 이펙트 + vfxId 19/20 (**양쪽 미러 필수**). |
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
6. **ShieldWall 포격**: 1.6초마다 보스가 Clap을 재생하고 **자기 발밑이 아니라 대상 플레이어 발밑**에
   갈색 글로우 → 결정이 솟는가. 순환하는가(P1→P2→P3→P4→P1, 중간에 죽어도 순번 유지).
   예고를 보고 걸어 나가면 0 데미지인가. 맞으면 50 데미지 + 살짝 뜨는가(접지 중력 게이팅이
   상승을 막지 않는지 확인). 파훼 즉시 포격이 멈추는가. 2인 이상에서 원격 클라도 같은 위치인가.
7. **회귀**: 플레이어 `spikes`/`crystals_front_attack`/`crystals_cross_fade`, 일반 몬스터 근접,
   최종 보스 4종이 종전 위치 그대로인가(앵커 오버라이드는 opt-in이라 영향이 없어야 한다).

RoomServer Debug/x64 빌드 통과(경고 0/오류 0). 클라 코드 변경 없음(기존 바이너리 호환).

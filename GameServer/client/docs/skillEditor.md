# Skill / Monster-Pattern Editor (standalone mode)

## 목적

이 게임의 모든 공격(플레이어·몬스터)은 SkillSystem을 통한다. 스킬은
`resources/skills/*.lua`로 작성되고 부팅 시 `SkillCompiler`가 **불변** `SkillAsset`으로
컴파일한다. 손으로 lua를 고치고 재빌드/재실행하는 반복이 느리고, 히트박스의 위치/크기/
타이밍을 눈으로 맞추기 어렵다.

→ **standalone 실행 모드를 스킬 에디터로 전환**해, 캐릭터→스킬을 드롭다운으로 고르고
일시정지/슬로모션·free-fly 카메라·히트박스 클릭 선택을 동원해 값을 키보드로 조정하며
즉시 확인한다. lua는 직접 수정하지 않고, 조정값의 original/current/delta와 lua 수정
힌트를 단축키로 콘솔에 덤프한다.

## 구조

- `StandAlone::Game` — 월드(에셋/씬/물리/gfx/파티클) 셋업 호스트. setup 끝에서
  `editor_.init(...)`로 `skillSystem_/skillCtx_/camera_/uiManager_/debugBVView_/player_/goblin_/ghWnd`
  참조를 주입한다. 게임플레이 입력(이펙트 공격/몬스터 AI)은 제거되고 입력은 `editor_`에 위임.
- `client/editor/`
  - `characterSkillMap.hpp` — 전역 캐릭터→스킬 매핑 상수.
  - `skillDraft.hpp/.cpp` — `original_`/`draft_` 사본, 편집 필드 목록, diff 덤프.
  - `editorController.hpp/.cpp` — 드롭다운·피킹·nudge·slow-mo·free-fly·패널.

## 핵심 메커니즘

- **불변 레지스트리 미손상**: 에디터가 소유하는 `SkillDraft::draft_`(SkillAsset 깊은 복사)를
  `SkillSystem::startSkillAsset(&draft, casterId, ctx)`로 재생. `SkillInstance.asset`이
  `const SkillAsset*`라 draft 값이 그대로 반영된다.
- **pause 중 라이브 편집**: `SkillSystem::update`의 `updateHitboxes()`는 dt와 무관하게
  매 프레임 worldOBB를 localOBB+본 변환으로 재계산한다. nudge가 활성 히트박스의 localOBB를
  `setHitboxLocalOBBs`로 덮으면 timeScale=0에서도 즉시 반영된다.
- **slow-mo/pause**: `Game::update`에서 `simDt = realDt * editor.timeScale()`을 물리/스킬/
  애니메이션에 적용. 입력·카메라·UI는 realDt(반응성 유지). timeScale 0 = pause.
  - **스킬 VFX 파티클도 simDt로 구동**(설계 결정): 스킬 이펙트(`*Effect_.update()`)를 realDt로
    돌리면 pause 중에도 파티클이 계속 시뮬레이션돼, `VFXParticleAttach` 히트박스가 파티클을
    따라 움직이다 소멸한다(BoneAttach 히트박스는 simDt에 묶여 정상 정지). 따라서 스킬 이펙트는
    simDt로 업데이트해 pause 시 함께 멈춘다. `dt=0`이면 `ParticleSystem::update`가 방출/이동/수명
    모두 0으로 깔끔히 정지. 환경 파티클(`flame/smoke/dustParticleSystem_`)은 씬 생동감을 위해 realDt 유지.
- **회전 round-trip**: lua `OBB(...)`의 euler(yaw/pitch/roll)는 quaternion으로만 저장돼
  역추출이 모호하므로, 컴파일러가 `SkillHitboxDef::localOBBEulerDeg`에 authoring euler를
  함께 보관(클라이언트 한정, 런타임 무해). 에디터는 이를 읽어 절대 euler를 편집·덤프한다.
- **피킹**: `camera.hpp::screenToRay`(inverse view-proj) → 활성 worldOBB에 `RaycastOBB`(정확
  교차) + **화면-공간 근접 폴백**(OBB 중심을 화면 투영해 커서와 70px 이내면 선택). 얇은/회전된
  히트박스를 정확히 맞히기 어려우므로 "근처 클릭"으로도 선택된다. 선택 박스는
  `renderDebugHitboxes(bv, selectedIdx)` 호박색 하이라이트, `Esc`로 선택 해제.
- **테스트 더미**: reset(캐릭터 선택/`R`) 시 `positionDummyInFront()`가 타깃(=caster 반대편)을
  caster 정면 3.5m·지형 높이(`InitRefs::terrainHeightAt`)에 배치하고 caster를 바라보게 한다.
- **UI**: 캐릭터/스킬 드롭다운 좌우 배치, 조작법=우상단 helpLabel, 상태=좌상단 statusLabel.
  기존 standalone HUD(도움말/HP바/HiZ 라벨)는 제거.

## 입력 맵

| 입력 | 동작 |
|---|---|
| 캐릭터 드롭다운 | caster 선택 + 양쪽 리셋, 반대편이 타깃 더미 |
| 스킬 드롭다운 | 해당 캐릭터의(레지스트리에 존재하는) 스킬 → draft 로드 |
| `Space` | 선택 스킬 t=0부터 재생/재시작 |
| `LMB` | 히트박스 피킹 선택 (UI 위/ RMB 룩 중 제외) |
| `Esc` | 현재 히트박스 편집 종료(선택 해제) |
| `RMB` 드래그 | 카메라 회전(follow=orbit / free=look) |
| `WASD` | follow=caster 이동 / free=카메라 이동, `Q`/`E`=free 상하 |
| `↑/↓` | 패널 편집 필드 이동 |
| `←/→` | 현재 필드 증감 (`Shift`=coarse) |
| `[` / `]` | timeScale 감/증, `0` = pause 토글 |
| `F` | 카메라 follow↔free 토글 |
| `P` | 조정값 diff 콘솔 덤프 (original/current/delta + lua 힌트) |
| `R` | 캐릭터 위치/HP 리셋 |

## 편집 대상 / 덤프

선택한 히트박스 def 기준: 각 OBB의 center/halfExtents/euler, onHit(damage/impulseStrength/
impulseDir), 해당 def의 SpawnHitbox/DestroyHitbox 이벤트 시각, 그리고 전역 totalDuration.
`P` 덤프는 변경 필드만 `original → current (delta)`와 lua 재구성 힌트(`OBB(...) -- was OBB(...)`,
`addEvent(<ms>, ...)`)를 출력한다.

## 이펙트 ↔ 스킬 1:1 기반 (foundation)

기존 standalone/online의 "이펙트 드롭다운 + LMB 재생"에서 쓰던 18종 ParticleEffect를
기반으로, 각 이펙트와 1:1 대응하는 **기본 스킬 lua**를 `resources/skills/`에 추가했다.
`SkillCompiler::compileAll`이 디렉터리를 스캔하므로 lua 추가만으로 자동 등록되고,
`kCharacterSkillMap`의 Player 스킬 목록에 이름을 넣어 드롭다운에 노출한다.

VFX 바인딩은 경로 문자열이 아니라 **vfxId → `StandAlone::Game::skillVfxById_[vfxId]`**
(ParticleEffect 포인터 배열) 인덱스다. lua `PlayVFX{vfxId=N}`이 곧 게임 배열의 N번이며,
아래 표대로 짝지었다(인덱스 0은 hit/blood 예약 = 에디터에선 nullptr no-op).

| vfxId | ParticleEffect | Skill / lua | 아키타입 |
|---|---|---|---|
| 1 | swordSlash1Effect_ | SwordSlash / sword_slash.lua | 근접 아크(기준 템플릿) |
| 2 | slashWaveEffect_ | SlashWave / slash_wave.lua | 근접 아크 |
| 3 | swordSlashComboEffect_ | SlashCombo / slash_combo.lua | 근접 아크 |
| 4 | swordSlash7Effect_ | Slash7 / slash_7.lua | 근접 아크 |
| 5 | spikesAttackEffect_ | Spikes / spikes.lua | 지면 AoE |
| 6 | crystalsFrontAttackEffect_ | CrystalsFrontAttack / crystals_front_attack.lua | 지면 AoE |
| 7 | aoESlashGreenEffect_ | AoESlashGreen / aoe_slash_green.lua | 근접 아크(광역) |
| 8 | redEnergyExplosionEffect_ | RedEnergyExplosion / red_energy_explosion.lua | 지면 AoE |
| 9 | crystalsCrossFadeEffect_ | CrystalsCrossFade / crystals_cross_fade.lua | 지면 AoE |
| 10 | arrowEffect_ | Arrow / arrow.lua | 투사체 |
| 11 | arrowVolleyEffect_ | ArrowVolley / arrow_volley.lua | 투사체(광역) |
| 12 | arrowRainEffect_ | ArrowRain / arrow_rain.lua | 지면 AoE(낙하) |
| 13 | energyExplosionArrowEffect_ | EnergyExplosionArrow / energy_explosion_arrow.lua | 투사체 |
| 14 | tornadoShotEffect_ | TornadoShot / tornado_shot.lua | 이동 투사체 |
| 15 | piercingEffect_ | Piercing / piercing.lua | 투사체 |
| 16 | piercingSlashEffect_ | PiercingSlash / piercing_slash.lua | 근접 아크 |
| 17 | piercingCircleSlashEffect_ | PiercingCircleSlash / piercing_circle_slash.lua | 근접 아크(주위) |
| 18 | piercingMultiEffect_ | PiercingMulti / piercing_multi.lua | 투사체(다중) |

각 lua는 PlayAnimation(`Player_Attack`) + PlayVFX(해당 vfxId) + 단일 SpawnHitbox/DestroyHitbox +
OnHit(damage/impulse)로 구성된 **시작값**일 뿐이며, 타이밍·히트박스·VFX offset은 에디터로
다듬어 `P` 덤프 가이드대로 lua에 반영하는 것을 전제로 한다. 신규 lua는 ASCII/영어 주석만 사용.

> 스킬 lua 작성법(이벤트별 파라미터, 유형별 레시피, VFXParticleAttach 등)은 `skillCreationGuide.md` 참조.

## 범위 / 후속

- 캐릭터 "재스폰"은 Player/Goblin 상주 상태에서 **caster 선택 + 양쪽 리셋 + 타깃 지정**으로
  구현. 실제 모델 스왑(Anubis/Fishman)은 해당 에셋이 AssetManager에 명시 로드된 뒤 spawn
  분기 추가(매핑 구조는 마련됨). 새 캐릭터는 `kCharacterSkillMap`에 추가.
- 타깃 더미 ragdoll의 완전한 부활은 best-effort(위치/HP/플래그 리셋). 사망 연출 후 깔끔한
  복원이 필요하면 EvDeath/respawn 경로 연동이 후속 과제.
- 타임라인 스크럽/프레임스텝/루프는 범위 외(슬로모션 + Space 재시작으로 대체).
- lua 자동 저장 없음(콘솔 가이드만) — 의도된 결정.

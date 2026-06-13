# 결정론적 파티클 히트박스 (VFXParticle Hitbox Determinism)

> 2026-06-11 도입. 관련 커밋: `client_particle` 브랜치.
> 공유 모듈: `common/particleGameplay.hpp` / 서버 구현: `RoomServer/skill/skillSystem.cpp`

## 1. 문제

`VFXParticleAttach` 히트박스는 파티클 시스템이 **랜덤 생성한 파라미터**(부채꼴 각도,
크기, 수명 등)를 그대로 따라간다. 클라이언트와 서버가 각자 `mt19937`을 돌리면
파라미터가 달라져 **이펙트(클라 비주얼)와 판정(서버 히트박스)이 어긋난다**
(맞았는데 안 맞은 것처럼 보임). 시드만 공유해도 해결되지 않는다:

- `mt19937`은 스트림이라 draw 호출 횟수·순서가 프레임 dt 분할에 묶임
  (조건부 draw, swap-remove 사망 순서, 프레임별 burst 윈도우).
- `vel *= (1 - drag·dt)` 류 per-frame 적분은 틱레이트가 다르면 갈라짐.

## 2. 설계 요약

게임플레이에 영향을 주는 랜덤만 분리해 **카운터 기반 PRNG**로 바꾸고, 서버는
파티클 시뮬레이션 전체 대신 **해석적 샘플러**를 돌린다.

```
캐스터 클라                      서버                         원격 클라
seed 생성(random_device)
startSkill(seed) ───C_SkillStart{seed}──▶ startSkill(elapsed, seed)
PlayVFX: 이펙트에 seed 주입          PlayVFX: VFX 앵커 기록      ◀─S_SkillStart{seed}─
ParticleSystem det 모드:            updateParticleHitboxSources: startSkill(elapsed, seed)
  pg::sampleSpawn(seed,stream,id)     pg::evaluateParticles(...)  동일 경로로 비주얼 재현
  → 비주얼 = 게임플레이               → 히트박스 = 캐스터 비주얼
```

핵심 불변식: **값은 (seed, stream, id, drawCursor) 키의 순수 함수**다. 호출
시점·프레임 분할·틱레이트와 무관하므로 양측이 같은 키로 같은 값을 얻는다.

### 시드 체인

```
inst.seed (per-cast, 캐스터 생성, C_SkillStart/S_SkillStart로 공유)
 └─ effectSeed = pg::mixSeed(inst.seed, vfxId)        ← SkillSystem PlayVFX
     └─ systemSeed = pg::mixSeed(effectSeed, systemIdx) ← ParticleEffect / 서버 소스
```

### 스폰 식별 (stream, id)

| stream | 용도 | id |
|---|---|---|
| `kStreamPlayEmit` | `play()`의 `emit(1)` (PlayMode::Emit) | emit 순번 (재생당 0부터) |
| `kStreamRate` | rate-over-time | k번째 파티클 (단조 증가), 스폰시각 = (k+1)/rate |
| `kStreamBurstMeta` | burst 확률/개수 roll | `burstKey(loop, burstIdx, cycle)` |
| `kStreamBurstSpawn` | burst 파티클 draw | `burstParticleId(key, i)` |

`pg::sampleSpawn()` 내부의 draw 순서가 곧 wire format이다 — 클라/서버가 같은
함수를 실행하므로 구조적으로 어긋날 수 없다. **순서를 바꾸면 호환이 깨진다.**

## 3. 구성 요소

| 파일 | 역할 |
|---|---|
| `common/particleGameplay.hpp` | DetRng(SplitMix64), GameplayConfig, sampleSpawn, evaluateParticles(해석적 평가), importGameplayConfig(JSON 게임플레이 서브셋 파서) |
| `common/simpleJson.{hpp,cpp}` | (client/에서 이동) 양측이 공유하는 JSON 파서 |
| `client/particleSystem.{hpp,cpp}` | det 모드: 스케줄 스폰(burst/rate/emit)의 게임플레이 draw를 pg 경로로 전환. 비주얼 전용 draw(trail, custom data 등)는 기존 rng_ 유지 |
| `client/particleEffect.{hpp,cpp}` | `setDeterministicSeed(seed)` → 비서브이미터 시스템 i에 `mixSeed(seed, i)` 전파 |
| `client/skill/*` | SkillInstance.seed, startSkill(seed), PlayVFX에서 이펙트 시딩, addVFX systems 테이블 컴파일 |
| `RoomServer/skill/*` | PlayVFX 앵커(월드변환+이벤트시각), SpawnHitbox에서 샘플러 바인딩, updateParticleHitboxSources를 evaluateParticles 기반으로 교체, 게임플레이 설정 캐시 |
| `ServerEngine/protocol.hpp` | C/S_SkillStart에 `uint32 skillSeed` |

### 이펙트 마이그레이션 (per-effect opt-in)

서버가 시스템을 재구성하려면 Lua `addVFX`에 구성 테이블이 필요하다:

```lua
skill:addVFX(6, "effects/Crystals front attack_ParticleSystems.json", {
    systems = {
        -- index = 클라 game.cpp의 addSystem 순서와 일치해야 함
        { name = "Crystals front attack",          mode = "Continuous", looping = false },
        { name = "Crystals front attack/Crystals", mode = "Emit" },
    }
})
```

- `name` = JSON 내 relativePath (**생략 = 코드 빌드 시스템**: 기본값 + 오버라이드만으로 구성),
  `mode` = 클라 PlayMode.
- **게임플레이 오버라이드** (`pg::VfxSystemOverrides`; game.cpp의 cfg 코드 tweak 미러링):
  `looping`, `duration`, `speed`(수 또는 `{min,max}`), `lifetime`, `startSize`,
  `maxParticles`, `shapeType`, `shapePosition=Vec3`, `shapeEuler=Vec3(도)`,
  `meshEuler=Vec3(도)`, `direction=Vec3`, `boxSize=Vec3`, `coneRadius`,
  `coneAngle(도)`, `radiusThickness`, `emitRate`, `volLinear=Vec3`(로컬 등속),
  `bursts = { { time=, count=, cycleCount=, interval=, probability= } }`.
  예시: `piercing_multi.lua`(JSON 베이스+코드 tweak 미러 20시스템),
  `arrow_rain.lua`(랜덤 분산 cone).
  **주의: JSON이 게임플레이 값(startSize3D/startRotation3D 랜덤 등)을 제공하는
  시스템은 반드시 `name`으로 JSON을 베이스로 깔고 코드 tweak만 오버라이드할 것.**
  (`name` 생략 = 기본값 베이스 — 완전 코드 빌드 시스템 전용.)
- **서브이미터 체인** (`parent = N`, `parentEvent = "Birth"|"Death"`; 기본 Death):
  부모 시스템 N의 파티클 탄생/사망 이벤트가 이 시스템의 burst를 재생한다
  (`bindSubEmitter` + `PendingSubEmitterBurst` 재현, 재귀 깊이 4). 클라의 emitAt
  스폰은 비주얼 rng를 쓰므로 **체인 시스템의 게임플레이 파라미터는 상수여야
  정확히 일치**한다 (burst probability<1, count 범위도 피할 것).
  예시: `energy_explosion_arrow.lua`(Charge→Arrow→Hit 2단 사망 체인),
  `crystals_cross_fade.lua`(탄생 체인).
- 설정 빌드는 **부팅/초기화 시 1회**: 서버 `AssetManager`와 클라 game 초기화가
  `buildVfxGameplayConfigs(assets, "../resources")`를 호출해 JSON+오버라이드를
  `SkillAsset::vfxDefs[..].gameplayCfg`(shared_ptr, 불변)로 굽는다. 클라는
  이펙트 구성 완료 후 `SkillSystem::bindVfxGameplayConfigs()`가 각
  ParticleEffect 시스템에 자동 주입한다 (per-effect 수동 wiring 불필요).
- 테이블이 없는 기존 스킬은 **서버 히트박스 비활성 + 경고 로그** (기존 no-op과
  동일 동작, 점진적 마이그레이션).

### 스킬별 적용 현황 (2026-06-11, 8종 전체 마이그레이션 완료)

| 스킬 (vfx, sys) | 상태 |
|---|---|
| CrystalsFrontAttack (6,0) | ✅ JSON + looping 오버라이드 (런타임 검증 완료) |
| Arrow (10,0) / ArrowVolley (11,0..8) / ArrowRain (12,0) | ✅ 코드 빌드 → 오버라이드 전체 기술 (런타임 검증 완료) |
| SlashWave (2,1) | ✅ JSON + direction/looping |
| PiercingMulti (18,0..19) | ✅ JSON 베이스 + 코드 tweak 오버라이드 |
| EnergyExplosionArrow (13,2) | ✅ 사망 체인 (Charge→Arrow→Hit, 전부 상수 → 정확) |
| CrystalsCrossFade (9,1) | ✅ 탄생 체인. **주의**: 필러의 velocityOverLifetime 곡선은 미지원(경고 로그) — 서버 히트박스가 필러의 상승 모션을 따라가지 못해 수직 오프셋 가능. 런타임 확인 필요 |

주의: ArrowVolley의 히트박스 슬롯 수는 10→9로 수정됨(10번째 슬롯이 공유
ArrowHit 자식 시스템에 잘못 부착되어 있었음).

## 4. 시드를 캐스터가 만드는 이유

캐스터는 `S_SkillStart`를 받지 않고 즉시 로컬 재생한다(서버는 `broadcastExcept`).
서버 생성 시드는 캐스터에게 RTT 후에야 도달해 첫 VFX(150ms)에 못 맞출 수 있다.
판정은 어차피 서버가 같은 시드로 수행하므로, 시드 조작으로 얻을 이득은 조준
보정 수준에 그친다. 경쟁 모드가 필요해지면 서버 생성+회신으로 전환 가능.

## 5. 정밀도 한계 / 미지원 (로그로 표면화됨)

- **비트 동일이 아니라 허용오차 동일**: /fp:fast 하에 양 바이너리의 부동소수점
  계약이 1ulp 수준에서 다를 수 있음 — 히트박스 크기 대비 무시 가능.
- **±1 클라 프레임 양자화**: 스폰/사망/회전적분이 클라에서는 프레임 경계에
  맞춰지고 서버는 연속 시간 평가 (~16ms, 속도 30 기준 ≤0.5u).
- 미지원(설정 로드 시 경고): `velocityOverLifetime` 곡선/랜덤 채널(상수만 지원),
  `rotationOverLifetime` 곡선 모드, drag, 파티클 지형 충돌(GroundKill 조기 사망).
  서브이미터 체인은 지원되나(`parent`/`parentEvent`) **체인 시스템의 게임플레이
  파라미터가 상수일 때만 정확** (클라 emitAt은 비주얼 rng로 draw).
- 한 프레임에 duration을 2회 이상 감는 루프(거대 dt)는 클라 레거시와 동일하게 미보정.
- ParticleEffect 인스턴스가 캐스트 간 공유되는 기존 한계는 그대로다(동시 시전 시
  마지막 시드가 승리). 서버는 캐스트별 소스라 영향 없음.

## 5.5 디버깅 이력

- **2026-06-11: CrystalsFrontAttack 서버 무판정 원인** — 서버 스킬 컴파일러가
  PlayVFX 페이로드를 파싱하지 않아(`default: break`) 앵커의 vfxId가 0으로
  남았고 `findVfxAnchor()`가 항상 실패 → 히트박스 0개. 서버 컴파일러에 클라와
  동일한 PlayVFX 파싱(vfxId/offset/orient/ground 플래그/conform)을 이식해 해결.
  교훈: **서버 컴파일러가 "no-op이라 파싱 생략"한 이벤트는 새 기능이 그 페이로드를
  소비하기 시작할 때 0-초기화 값으로 조용히 깨진다.**
- 같은 날 `evaluateParticles`의 volLinear가 이미터 회전 미적용(raw 가산)이던
  버그 수정 — 클라 `calcVelocityOverLifetime`은 `inWorldSpace=false`일 때
  `emitterTransformRotation`으로 회전시킨다.
- **2026-06-11: PiercingMulti 비주얼 회귀** — lua systems 엔트리를 `name` 없이
  (기본값 베이스) 작성하자 det 모드가 JSON의 startSize3D/startRotation3D 랜덤을
  기본값으로 대체해 찌르기 메시가 망가짐. JSON을 베이스(`name`)로 깔고 코드
  tweak만 오버라이드하도록 수정. 교훈: **det 모드는 비주얼 스폰 파라미터도
  대체하므로, lua 구성은 "클라 최종 cfg"와 같아야 한다 (JSON 유래 값 포함).**

## 6. 검증 방법

1. RoomServer 실행 후 온라인 모드에서 CrystalsFrontAttack(vfxId 6) 시전.
2. `S_DebugHitbox` 오버레이로 서버 OBB를 클라에서 렌더 → 얼음기둥 seeker 경로와
   일치 확인 (기존 디버그 채널 재사용).
3. 미마이그레이션 스킬은 서버 콘솔에 "VFXParticle hitbox has no gameplay config"
   경고가 떠야 정상.

## 7. 유지보수 주의 (SYNC 계약)

- `pg::sampleSpawn` / burst·rate 스케줄 키 산정은 클라 det 경로와 서버 샘플러가
  공유한다. `client/particleSystem.cpp`의 셰이프 샘플링·스폰 로직을 바꾸면
  `common/particleGameplay.hpp`에 반영해야 한다 (헤더 상단 SYNC WARNING 참조).
- `client/particleImporter.cpp`의 게임플레이 관련 필드 해석을 바꾸면
  `pg::importGameplayConfig`도 같이 바꿔야 한다.
- 히트박스가 붙는 시스템의 게임플레이 필드를 game.cpp 코드로 tweak하지 말 것
  (looping처럼 필요하면 Lua systems 테이블로 내려서 양측에 공급).

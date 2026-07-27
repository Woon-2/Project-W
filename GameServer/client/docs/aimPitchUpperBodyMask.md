# 플레이어 공격 조작감: 상하체 분리 마스크 + 카메라 pitch 조준

> 2026-07 구현. 관련 소스: `client/object.{hpp,cpp}`(AnimBlenderPlayer), `client/animation.{hpp,cpp}`(onPostDress 훅),
> `client/skill/skillSystem.cpp` · `RoomServer/skill/skillSystem.cpp`(PlayVFX aim), `RoomServer/object.{hpp,cpp}`(스파인 재합성),
> `ServerEngine/protocol.hpp`(pitch 필드).

## 1. 해결한 문제

1. **이동 중 공격 시 발 미끄러짐** — 공격 오버레이가 전신 lerp(`tAttack_` 스칼라)여서 공격 클립이 다리 본까지
   덮어써 run 보폭이 사라졌다.
2. **공격이 xz평면에 고정** — 카메라 pitch가 `Game::cameraPitch_`(카메라 전용)에서 끊기고 body orient/스킬/패킷
   어디에도 흐르지 않아 화살·멜리 모두 수평으로만 나갔다.

## 2. 상하체 분리 마스크 (클라 전용)

오버레이 클립의 lerp 가중치에 본별 마스크를 곱한다:

```
wOverlay(i) = t * ( mask[i] + (1 - mask[i]) * tIdle_ )
```

- **정지(tIdle_=1)**: 전 본 가중치 = `t` → 종전 전신 오버레이와 프레임 단위 동일(회귀 0).
- **이동(tIdle_→0)**: 하체(mask=0)는 로코모션 클립 유지 → 발 미끄러짐 소멸. 상체(mask=1)만 오버레이.
- 마스크: **`AnimBlender::buildUpperBodyMask(logTag)`**(`client/animation.cpp`)가 init 시 1회 구축.
  `spine_01` 서브트리=1, 그 외=0, 경계 소프트 가중치 `spine_01=0.5, spine_02=0.85`
  (힙-스파인 시어 방지, `kBoundaryWeights` 튜닝 지점). 읽기는 `upperBodyWeight(boneIdx)`
  (범위 밖이면 1 반환하므로 호출부 크기 검사 불필요).
- `spine_01` 미발견 시 전부 1(전신 오버레이)로 폴백 + 경고 로그 → 마스크를 지원하지 않는 리그에서도
  안전하게 호출할 수 있다.
- Baked 분기(원거리 원격 LOD)는 종전 유지(마스크 미적용).

### 적용 대상

| 블렌더 | 마스킹하는 오버레이 | 비고 |
|---|---|---|
| `AnimBlenderPlayer` | 공격 | hit/death는 넉백·입력잠금 상태라 접지 정확도가 무의미해 제외 |
| `AnimBlenderBoss` | 공격 + **피격** | 보스는 추격 중 피격이 잦다. 서버가 hit 클립을 `switchClip`하지 않으므로(순수 클라 연출) 동기화 리스크 없음. death는 전신 유지 |

플레이어 리그(`spine_01..03`)와 보스 리그(`spine_01..05`)는 둘 다 UE 마네킹 계열 네이밍이라
같은 규칙·같은 경계 가중치가 성립한다. 리그별로 값을 달리해야 할 필요가 생기면 그때 인자로 뽑는다.

로코모션 배속(`solveLocomotionRate`)에 넘기는 `locoWeight`는 마스크 몫을 반영해야 한다 —
양쪽 다 `wLegs = t로코모션 * (1 - tAttack_ * tIdle_)`.

- **서버는 전신 공격 클립 그대로 판정** — 이동+공격 시 클라(하체 로코모션)와 서버(전신 attack)의
  본 앵커 위치 오차는 히트박스 OBB 크기 대비 소량으로 허용 오차(§5).

## 2.5 마스크의 부작용: 상체 yaw 끌림과 그 상쇄 (2026-07-27)

### 증상
좌우로 이동하며 공격하면 **상체가 이동 방향으로 통째로 끌려간다.** 활에서 가장 두드러진다 —
왼쪽으로 이동하며 쏘면 몸이 왼쪽을 보고, 활을 든 왼손이 몸 **뒤**로 빠지는데 화살은 캐스터 정면
(`arrow.lua`의 `offset=(0,1,0.8)`)에서 나가므로 "옆구리에서 화살이 나가는" 그림이 된다.

### 원인 (플레이어 애니메이션 세트 전량 덤프로 확증)
`Run_*_Left/Right` 클립은 스트레이프(게걸음)가 아니라 **골반을 이동 방향으로 ±90° 돌리고
상체를 반대로 비틀어(counter-rotation) 정면을 유지**하도록 저작돼 있다. bind 대비 실측:

| 상태 (speed 5, 발사 시점 t=120ms) | 골반 | 어깨 | 왼손(활) 로컬 x/z |
|---|---|---|---|
| 정지 + 공격 (기준 자세) | +25° | +41° | 0.07 / **+0.57** |
| 좌 이동, 공격 없음 | −91° | −22° | −0.51 / +0.22 |
| 좌 이동 + 공격 | −89° | **−64°** | −0.72 / **−0.10** |
| 우 이동 + 공격 | +85° | **+82°** | +0.57 / +0.40 |

마스크가 상체를 공격 클립으로 덮으면 클립이 갖고 있던 **counter-rotation만 사라지고 골반의 ±90°는
하체 몫으로 남는다.** 그래서 어깨가 ±22° → ±64~82°로 벌어진다. 좌우 모두 생기는 결함이지만,
활 조준 자세 자체가 어깨 +41°(궁수가 몸을 오른쪽으로 트는 자세)라 **오른쪽 이동은 기준과 같은
방향으로 41° 더 트는 것**이라 자연스럽게 읽히고, 왼쪽 이동은 기준에서 **105° 반대로** 벗어나
결함이 한쪽에서만 보이는 것처럼 느껴진다. 2H/1H도 같은 구조(그쪽은 오른쪽이 더 크게 벗어난다).

### 해법 — `AnimBlenderPlayer::cancelAttackYawDrift()`
**공격 클립을 단독 재생했을 때의 상체 방향**을 목표로 삼아, 차이의 **yaw만** 상체 루트(`spine_01`)
피벗에서 되돌린다. pitch와 같은 피벗-공액 규약(`dress[b] * K`, 서브트리 우측 곱)이다.

```
목표  = root→pelvis→spine_01..03 을 공격 클립 프레임만으로 누적 (spineAncestry_)
delta = wrapPi( yaw(목표) − yaw(현재) ) * strength
strength = tAttack_ * (1 − tIdle_) * (1 − tDeath_)
```

- yaw 측정은 `boneForwardYaw()` = `toLocal * dress`(bind 대비 변형)를 본 전방 `+Z`에 적용해 `atan2`.
  **본 로컬 축 규약과 무관**하므로 리그가 달라도 성립한다.
- `(1 − tIdle_)` 항이 **정지 시 보정을 정확히 0으로** 만든다 — §2의 "정지 시 회귀 0" 불변식 유지.
- **골반·다리를 건드리지 않으므로 로코모션 접지는 그대로다**(마스크의 원래 목적인 발 미끄러짐 해결이
  훼손되지 않는다). 보정 후 골반↔어깨 비틀림은 44°로, 오히려 원본 클립(63°)보다 작다.
- 순서: `onPostDress()`에서 **yaw 보정 → pitch** 순. pitch가 본 전방을 기울인 뒤엔 yaw 측정이 흐려진다.

보정 후 어깨 yaw는 정지/전진/좌/우 전부 **39~44°**(기준 41°)로 수렴하고, 활 손은 전 방향에서
로컬 z ≈ +0.6(몸 앞)으로 돌아온다.

### 서버 미러가 불필요한 이유
서버 `AnimController`는 단일 클립 재생이라 애초에 이 드리프트가 없다(항상 전신 공격 클립).
플레이어 근접은 `AttachType::Body`(rest 포즈 기준, §6.5), 활은 `VFXParticleAttach`로 둘 다
애니 포즈와 무관하다. 즉 이 보정은 **클라 시각 전용**이며, 클라를 오히려 서버 포즈에 더 가깝게 만든다.

## 3. 조준 pitch 파이프라인

### 상태 채널 (body orient와 분리)
pitch를 body orient에 넣으면 캐릭터가 굴러버리므로 **별도 채널**로 관리한다:

| 위치 | 저장소 | 갱신 |
|---|---|---|
| 로컬 클라 | `Object::aimPitch_` | `processInputGame`에서 매 프레임 `cameraPitch_` 복사 |
| 서버 | `Object::cameraPitch_`(기존 휴면 필드 재활용) | `Room::rotate`(C_MouseMove) + `Room::skillStart`(시전 스냅) |
| 원격 클라 | `Object::aimPitch_` | `rotatePlayer`(S_MouseMove) + `onSkillStart`(시전 스냅) |

부호: **+ = 아래를 봄**(카메라 클램프 [−0.16π, +0.3π]). `rotateXH(+θ)`가 전방을 아래로 기울인다.

### 프로토콜 (`ServerEngine/protocol.hpp`)
- `C/S_MouseMove` + `pitchRadian` — 연속 스트림(원격 상체 굽힘 시각, 서버 live 상태).
  **송신 조건 확장**: yaw 불변이어도 pitch가 0.01rad 이상 변하면 송신(`lastSentAimPitch_`).
- `C/S_SkillStart` + `aimPitchRadian` — **시전 시점 스냅샷**. `Room::skillStart`가 `startSkill` 전에
  `setCameraPitch` → t=0 타임라인 이벤트가 캐스터 예측과 동일 pitch로 판정(skillSeed와 동일한 결정론 원리).
  NPC 캐스트(`skillStartInternal`)는 0.
- `SkillDispatchContext`/`startSkill`/`SkillInstance` 시그니처는 무변경(yaw와 대칭인 live 의미론).

### 시각: 스파인 피벗-공액 (클라)
`AnimBlender::onPostDress()` virtual 훅(onCalcDress 누적 직후, Keyframe 한정)에서
`AnimBlenderPlayer`가 드레스 공간 행렬에 주입:

```
k = spine_01, 02, 03 순서로 (pitch/3씩):
  pivot = translation(dress[k])          // 앞 단계 K 반영된 행렬에서
  K = translate(-pivot) · rotateXH(pitch/3) · translate(pivot)
  subtree(k)의 모든 본 b: dress[b] = dress[b] · K
```

- 본 로컬 축 규약과 무관(피벗-공액), 자식 본은 서브트리 곱으로 함께 회전.
- 사망 페이드: 적용 각 = `aimPitch * (1 - tDeath_)`.
- 체인 데이터(`spineChainIdx_`/`spineDepth_`)는 `AnimBlenderPlayer::buildSpineChain()`에서 1회 구축.

### 판정: 서버 미러
1. **멜리 본 히트박스**: `RoomServer/object.cpp Object::applySpinePitch()` — `updateAnimBones`에서
   `computeBoneModelXforms` 직후·entityWorld 곱 전에 **클라와 동일 수식**을 모델 공간에 적용.
   `BoneAttach("spine_01")+applyAttachRotation` 히트박스가 자동으로 기울어짐. NPC는 pitch 0 → no-op.
   클라 예측 히트박스는 finalXform 경유라 자동 일치.
2. **투사체/이펙트**: 양측 PlayVFX(미러 diff) — `aim = eulerOff · rotateXH(aimPitch) · baseRot`.
   **제외**: `kPlayVFXFlagYawOnly`(groundLock)·`kPlayVFXFlagGroundSnap`(지면 이펙트 평탄 유지),
   본 attach(본 행렬에 이미 pitch 포함 — 이중 적용 방지). emitter frame이 클라/서버 동일 입력이라
   파티클 히트박스 결정론 유지(`particleHitboxDeterminism.md`).

## 4. 튜닝 지점
- 마스크 경계 가중치: `client/animation.cpp buildUpperBodyMask()` `kBoundaryWeights`
  (플레이어·보스 공용이므로 한쪽만 바꿀 수 없다).
- 상체 yaw 보정 강도: `cancelAttackYawDrift()`의 `strength` 식(§2.5). 완전 고정이 뻣뻣하면
  상수 배율을 곱해 약하게 줄 수 있으나, 정지 시 0이 되는 `(1 - tIdle_)` 항은 유지해야 한다.
- 스파인 체인 분산: 현재 3관절 균등(pitch/3). 필요 시 가중 분산으로 변경 가능.
- 원격 pitch 스냅이 거슬리면 `rotatePlayer` 수신부에 지수 평활 추가 검토.

## 5. 허용 오차 (의도된 비대칭)
- 이동+공격 시 클라 하체는 로코모션, 서버는 전신 공격 클립 → 본 앵커 위치 소량 불일치(합의된 허용 오차).
  서버 `AnimController`는 단일 클립 재생(블렌딩 없음)이라 서버 마스크는 존재할 수 없다.
  - 플레이어: 근접 8종이 `AttachType::Body`(rest 포즈 기준)로 이관되어 **이 오차가 소멸**했다(§6.5).
  - **보스**: 히트박스가 `BoneAttach("weapon_r")`(애니 추종)라 구조적 해소책이 없다. 다만 보스는
    **시전 중 완전 정지**하므로(§7) 히트박스가 살아 있는 동안에는 `tIdle_=1`이라 마스크가
    붕괴하고 클라/서버가 같은 전신 공격 포즈를 본다 — 이 오차 자체가 발생하지 않는다.
- 서버는 사망 페이드 없음(사망자는 판정 대상 아님 — 실질 영향 없음).
- Baked(원거리 원격) LOD는 마스크/pitch 미적용 — 거리상 시인 불가.

## 6.5 애니메이션 독립 히트박스: `AttachType::Body` (2026-07-25)

### 문제
플레이어 근접 스킬 8종의 히트박스는 전부 `BoneAttach("spine_01")`였다. 런타임 배치는
`worldOBB = localOBB · (bone.toDress · finalXformData[spine_01] · world)`인데, `finalXformData[spine_01]`은
**현재 재생 중인 클립의 애니 포즈**다. 스킬을 무기 없던 시절 클립 기준으로 저작했는데 무기별로 `attackClips_`가
달라지면서 같은 spine_01의 애니 포즈가 클립마다 달라져 히트박스가 어긋났다.

### 해법
새 `AttachType::Body`: OBB는 여전히 **본-로컬 공간**(BoneAttach와 동일)에 저작하지만, 런타임은 애니 포즈 대신
**본의 rest(bind) 프레임(`toDress`)**을 쓴다 — 재생 클립과 무관. 여기에 조준 pitch를 본 원점 기준으로 주입한다.

```
pivot  = Vec4(0,0,0,1) · rest            // 본 rest 원점(dress 공간), rest = bone.toDress
K      = translate(-pivot) · rotateXH(aimPitch) · translate(pivot)
xform  = rest · K · world                // pitch=0이면 rest · world = BoneAttach의 bind 포즈와 동일 위치
```
- 클라 `SkillSystem::computeAttachTransform`(`AttachType::Body` 분기, `owner.aimPitch()`),
  서버 미러(`owner.cameraPitch()`, `entityWorld` = `Object::updateAnimBones`와 동일 합성). 둘 다 `toDress`+동기화된
  pitch만 쓰므로 **클라 예측 = 서버 판정**이 구조적으로 성립 → §5의 이동+공격 허용 오차가 **소멸**한다.
- 스윕(sword arc)은 종전처럼 타임라인의 다중 SpawnHitbox로 구동(본 추종 불필요).
- pitch는 spine_01 base 기준 **전체 aimPitch** 회전(멜리 볼륨이 무기 시각과 함께 상하로 기욺). 종전 서버는
  `applySpinePitch`로 spine_01에 pitch/3만 실렸었다(약한 틸트) — Body가 더 강하고 직관적.

### Lua / 마이그레이션
- `BodyAttach(boneName, { pitch = true|false })`(`resources/skills/lua/skill_api.lua`). `pitch=false`면 yaw 전용 평탄
  (예: PBAoE 링 `spikes`).
- 8종 이관: `BoneAttach("spine_01")` → `BodyAttach("spine_01")`. **OBB 값 불변**(같은 본-로컬 공간, rest 포즈에서
  종전과 동일 위치). 무기별 미세조정은 StandAlone 에디터에서. 에디터 picking/편집/렌더는 타입 무관하게 그대로 동작.
- NPC 공격 스킬은 무기 교체가 없어 `BoneAttach` 유지(애니 추종).

### StandAlone/에디터 조작 Online 동기화
standalone(=스킬 에디터, `standalone/game.cpp`가 `editor_.handleInput`/`updateCamera`에 위임)은 종전에 카메라
pitch를 캐스터에 연결하지 않아 이 기능을 테스트할 수 없었다. `client/editor/editorController.cpp handleInput`을
Online `processInputGame`과 동기화:
- 매 프레임 `casterObj()->setAimPitch(camPitch_)`(플레이어 한정) → `AnimBlenderPlayer::update`가 캐시 → 스파인 굽힘
  + Body attach 히트박스 틸트 동작.
- 이동(WASD) 시 궤도 `camYaw_`를 body orient에 접기(`!rmb && |camYaw_|>eps`, Online 5954–5958과 동일) → 캐스터가
  시선 방향을 바라봄. 카메라 리그·pitch 클램프는 이미 Online과 동일.
- 커서 캡처 없는 RMB-look은 에디터 UI(드롭다운·히트박스 픽)를 위해 의도적 존치(Online 커서캡처 mouselook과 다름).

## 6. 검증 이력 (임시 장치는 제거됨)
- Phase 1: M키 마스크 A/B 토글 — 이동 공격 발 고정/정지 공격 동일 확인.
- Phase 2: `[AimPitch]` 서버/클라 로그 — 아래=+0.94, 위=−0.50 도달 확인.
- Phase 3: N키 스파인 시각 토글 — 부호/원격 재현/자연스러움 확인.
- Phase 4: `kBroadcastDebugHitboxes` 임시 활성화 — 화살 궤적=서버 히트박스 일치, 멜리 상하 판정,
  지면 스킬 평탄 유지 확인. 이후 전부 원복.
- 유지된 진단: `buildUpperBodyMask` init 요약 로그(`[UpperBodyMask] <rig> built: ... upperBones=N`,
  리그별로 한 줄씩)와 spine_01 미발견 폴백 경고, `buildSpineChain`의 `[AimPitch] ... spineChain=3`.

## 7. FinalBoss 확장 (2026-07-27, 인게임 검증 완료)

마스크를 `AnimBlender` 기반 클래스로 올리고 `AnimBlenderBoss`(공격 + 피격)에 적용했다.

**보스는 시전 중 완전 정지한다.** 걸으며 휘두르는 안을 넣었다가 되돌렸다 — 공격 클립의 windup이
크고 무거워 발이 움직이는 채로 재생하면 부자연스러웠다. 따라서 보스에서 마스크가 실제로 일하는
구간은 **시전 중이 아니라** 다음 둘이다:

1. **시전 종료 후 오버레이 잔여 구간** — 서버 스킬이 끝나면 보스는 즉시 다시 움직이는데
   클라 오버레이는 클립이 끝날 때까지 남는다. 마스크가 없으면 그동안 다리가 공격 포즈로 굳는다.
   (함께 고친 것: 오버레이 길이가 `3000ms` 하드코딩이라 Smite(서버 1200ms) 뒤 약 1.8초를 굳어
   있었다 → 플레이어와 동일하게 선택 클립 길이로 교체.)
2. **이동 중 피격** — 보스는 선회/돌진 중 피격이 잦다. 전신 hit(최대 0.75)이면 걷다가 다리까지
   움찔한다. 서버는 hit 클립을 `switchClip`하지 않으므로 동기화 리스크가 없다.

조준 pitch(스파인 굽힘)는 보스에 **미적용**. 적용하려면 pitch 소스 정의 + `S_NpcMoveBatch`에
pitch 필드 추가(서버 `applySpinePitch`가 `weapon_r` 히트박스까지 기울이므로 클/서 미러 필수)가 필요하다.

보스의 교전 패턴(선회/돌진/압박), 속도 램프, walk↔run 위상 동기는 `RoomServer/docs/bossCombat.md`.

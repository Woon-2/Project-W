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

`AnimBlenderPlayer::onCalcLocal`의 공격 lerp 가중치에 본별 마스크를 곱한다:

```
wAttack(i) = tAttack_ * ( mask[i] + (1 - mask[i]) * tIdle_ )
```

- **정지(tIdle_=1)**: 전 본 가중치 = `tAttack_` → 종전 전신 공격과 프레임 단위 동일(회귀 0).
- **이동(tIdle_→0)**: 하체(mask=0)는 run 클립 유지 → 발 미끄러짐 소멸. 상체(mask=1)만 공격.
- 마스크: `buildAttackMask()`가 init 시 1회 구축. `spine_01` 서브트리=1, 그 외=0,
  경계 소프트 가중치 `spine_01=0.5, spine_02=0.85`(힙-스파인 시어 방지, `kBoundaryWeights` 튜닝 지점).
- `spine_01` 미발견 시 전부 1(종전 전신 공격)로 폴백 + 경고 로그.
- hit/death lerp와 Baked 분기(원거리 원격 LOD)는 종전 유지.
- **서버는 전신 공격 클립 그대로 판정** — 이동+공격 시 클라(하체 run)와 서버(전신 attack)의 spine_01 앵커
  위치 오차는 히트박스 OBB 크기 대비 소량으로 허용 오차(§5).

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
- 체인 데이터(`spineChainIdx_`/`spineDepth_`)는 `buildAttackMask()`에서 1회 구축.

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
- 마스크 경계 가중치: `client/object.cpp buildAttackMask()` `kBoundaryWeights`.
- 스파인 체인 분산: 현재 3관절 균등(pitch/3). 필요 시 가중 분산으로 변경 가능.
- 원격 pitch 스냅이 거슬리면 `rotatePlayer` 수신부에 지수 평활 추가 검토.

## 5. 허용 오차 (의도된 비대칭)
- 이동+공격 시 클라 하체는 run, 서버는 전신 공격 클립 → spine_01 앵커 위치 소량 불일치(합의된 허용 오차).
- 서버는 사망 페이드 없음(사망자는 판정 대상 아님 — 실질 영향 없음).
- Baked(원거리 원격) LOD는 마스크/pitch 미적용 — 거리상 시인 불가.

## 6. 검증 이력 (임시 장치는 제거됨)
- Phase 1: M키 마스크 A/B 토글 — 이동 공격 발 고정/정지 공격 동일 확인.
- Phase 2: `[AimPitch]` 서버/클라 로그 — 아래=+0.94, 위=−0.50 도달 확인.
- Phase 3: N키 스파인 시각 토글 — 부호/원격 재현/자연스러움 확인.
- Phase 4: `kBroadcastDebugHitboxes` 임시 활성화 — 화살 궤적=서버 히트박스 일치, 멜리 상하 판정,
  지면 스킬 평탄 유지 확인. 이후 전부 원복.
- 유지된 진단: `buildAttackMask` init 요약 로그(`spineChain=3` 확인용)와 spine_01 미발견 폴백 경고.

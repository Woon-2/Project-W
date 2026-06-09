# 전투 피드백 UI — Damage Number / Kill Count

4인 협동 핵앤슬래시 MORPG의 타격감·처치감을 위한 데미지 숫자 + Kill Count HUD.
성공 기준은 "숫자가 표시된다"가 아니라 **타격감이 즉시 읽히고 킬 누적이 기분 좋게 남는 것**.
넓은 AoE·군집 전투를 고려해 **읽힘 > 화려함**을 동점 처리 규칙으로 둔다.

## 구성

```
DigitAtlas (공유 헬퍼)  ──┬─▶ DamageNumberSystem (월드앵커 떠오르는 숫자 풀)
                          └─▶ KillCountWidget   (HUD: 스컬 + 누적/streak)
```

- 새 셰이더/파이프라인 없음. 기존 `UIPipeline`에 `DrawEvent`를 직접 제출한다.
- 데미지 숫자와 킬 카운트 숫자 모두 같은 `DigitAtlas`로 렌더해 룩 일관성 확보.
- Kill Count HUD 배치: 화면 **하단 우측에서 2번째 슬롯**(맨 오른쪽은 스킬 UI 예약), `BottomRight` 앵커 + 좌측 오프셋.

## 아틀라스 레이아웃 계약

`resources/UI/damage_digits.dds` — **가로 10칸 균일**, 칸 인덱스 = 숫자값(0..9).

```
640×96 권장, 각 칸 64×96 (cellAspect ≈ 0.667)
┌──┬──┬──┬──┬──┬──┬──┬──┬──┬──┐
│0 │1 │2 │3 │4 │5 │6 │7 │8 │9 │
└──┴──┴──┴──┴──┴──┴──┴──┴──┴──┘
```

- UV: 한 칸 폭 = `1/10 = 0.1`. 숫자 `d` → `uvScaleBias = (0.1, 1.0, d*0.1, 0)`.
- 코드 상수: `DigitAtlas::kCellCount = 10`, `DigitAtlas::kCellAspect = 64/96`. **계약과 1:1 일치 필수**.
- 샘플러는 `Samplers::BilinearClamp`(셀 경계 UV 번짐 방지).
- 화이트 베이스 + 검은 외곽선으로 제작 → `colorMul` 틴트로 색 변환.

## 렌더 좌표 규약

- 앵커는 월드 좌표(`obj->renderState().pos + headOffset`). 스폰 시점에 고정(맞은 지점에서 떠오름).
- 매 프레임 **투영을 인라인**(`clip = Vec4(anchor,1)*(view*proj)`)해 화면 픽셀(top-left origin) sx,sy와 `clip.w`(뷰 깊이)를 얻는다. `clip.w<=0`(카메라 뒤)/NDC 화면 밖이면 skip. (`worldToScreen`과 동일 변환을 인라인 — 원근 스케일에 clip.w 필요.)
- **원근 크기**(HP바와 다름): `perspScale = clamp(perspRefDepth/clip.w, perspScaleMin, perspScaleMax)` → 가까운 적 크게, 먼 적 작게. jitterX도 `× perspScale`.
- 쿼드 world 행렬: `scale(gw/2, gh/2, 1) * translate(centerX, screenH - centerY, 0)` (UI 셰이더는 bottom-origin이라 Y 뒤집음). `UIElement::buildWorldMatrix`와 동일 규약.
- 글자 높이: `baseSize = clamp(baseH * perspScale, glyphHeightMin, glyphHeightMax)` (정상 크기만 제한) → `glyphH = baseSize * punch * endScale`.
  `punch`=scale punch(시간 함수, 아래 키프레임), `perspScale`=거리 함수 → **분리**. **clamp는 base에만 적용해 punch를 삼키지 않음**(punch spike는 항상 보임).

## 데미지 값 / 이벤트 훅

- 패킷에는 `newHp`만 옴 → **클라이언트 계산**: `dmg = prevHp - newHp`.
- `onlineGame.cpp` 이벤트 디스패치 루프에서 `obj->eventBus()->receive()` **직전**에 `prevHp = obj->hp()` 캡처(핸들러가 hp/dead를 바꾸기 전).
  - `EvHit`: `dmg = prevHp - EvHit::hp`.
  - `EvDeath`: `dmg = prevHp`(잔여 = 막타 데미지). 대상이 고블린이고 `!isDead()`면 `KillCountWidget::addKill()`.
- 대상이 로컬 플레이어면 `PlayerHit`(빨강), 아니면 `EnemyHit`(웜 화이트).

## 동시성

온라인 모드는 `InGameScene` 시작 `SleepEx(1,true)` alertable wait에서 패킷이 **게임 스레드 APC**로 처리된다.
따라서 `applyHit`→`holdEvent`→디스패치 루프→`spawn`/`addKill`/`update`/`render`가 모두 게임 스레드 단독.
**락·동기화 불필요.** IOCP 워커와 공유 상태 없음.

## 연출 v1 사양 (튜닝 상수)

> 아래는 첫 구현의 초기값. 플레이 테스트에서 조정 후 **최종값을 이 문서에 갱신**한다(임시 매직넘버가 아닌 사양).

### DamageNumberTuning (`damageNumberSystem.hpp`)

| 값 | 초기값 | 의미 |
|----|--------|------|
| `lifetime` | 0.75s | 화면 체류 |
| `fadeOutDuration` | 0.20s | 끝 페이드 |
| `floatUpPx` | 44px | 총 떠오름(easeOutCubic) |
| `scaleEnd` | 0.90 | 소멸 시 수축 스케일 |
| `impactScale` | 1.00 | **scale punch 강도**(키프레임 편차 배수, 1.0=원본). punch에만 적용 |
| `glyphHeightSmall/Big` | 32 / 44px | 히트 강도 차등 높이 |
| `bigHitThreshold` | 40 | 이 값 이상이면 큰 글자(단순 상수, 통계 시스템 아님) |
| `perspRefDepth` | 12 | 이 뷰 깊이(clip.w)에서 perspScale=1 |
| `perspScaleMin/Max` | 0.55 / 1.80 | 원근 스케일 클램프 |
| `glyphHeightMin/Max` | 14 / 72px | **정상 base 크기(×persp) 클램프** — punch는 이 위에 곱(punch는 제한하지 않음) |
| `mergeWindow` | 0.25s | 동일 대상 누적 병합 창 |
| `jitterXMax` / `startYJitterMax` | 45 / 8px | 다중 히트 분산 (jitterX는 ×perspScale) |
| `worldHeadOffsetY` | 1.2 | 앵커 오프셋(월드, 몸통/머리 아래에서 떠오름 시작) |

#### Scale Punch 키프레임 (`damageNumberSystem.cpp` `kPunchKeys`, 하드코딩)

생성 직후 ~170ms 안에 끝나는 뾰족한 entrance spike. 구간 선형보간, `popAge`(병합 시 리셋) 기준. `impactScale`로 편차 강도 조절.

| ms | scale |
|----|-------|
| 0   | 1.00 |
| 16  | 1.80 |
| 50  | 1.15 |
| 90  | 1.00 |
| 130 | 0.97 |
| 170 | 1.00 |

### KillCountTuning (`ui/widgets/KillCountWidget.hpp`)

| 값 | 초기값 | 의미 |
|----|--------|------|
| `popDuration` / `popScale` | 0.18s / 1.25 | 킬 증가 팝 |
| `skullDelay` / `skullBouncePx` | 0.016s / 6px | 스컬 1프레임 늦은 바운스 |
| `streakTimeout` | 4.0s | streak 리셋 |
| `streakShowMin` / `streakShowTime` | 3 / 1.2s | streak 표시 조건/노출 |
| `milestoneFlash` | 0.35s | 10/25/50/100 금색 플래시 |

## 연출 검수 루프

1. **단일 히트**: 맞은 위치에서 "툭" 솟는지(첫 0.12s scale·Y).
2. **AoE 다중**: 숫자가 많아도 중심 정보가 읽히는지(병합/jitter/풀 상한). 안 읽히면 숫자를 늘리지 말고 정리.
3. **처치 순간**: 데미지 숫자 팝과 Kill Count 팝이 같은 박자로 시작되는지(`EvDeath` 프레임).

## 후속 작업 (TODO)

- **크리티컬 색/연출**: 패킷에 crit 플래그 없음 → v1은 단색 + 대형값 강조만. 정식 crit은 서버 플래그(`SHitPacket`/`SSkillHitPacket`) 추가 후.
- **streak `xN` 표기**: 현재 아틀라스는 0~9뿐이라 streak는 숫자만 금색 표시. `x` 글리프 추가(아틀라스 12칸 확장) 시 `xN` 표기.
- **거점(Stronghold) 데미지 숫자**: `applyHit` early-return 경로라 현재 제외. 필요 시 별도 훅.
- **standalone 모드 미러링**: 본 작업은 온라인 우선. `standalone/game`에도 동일 훅 적용 가능.

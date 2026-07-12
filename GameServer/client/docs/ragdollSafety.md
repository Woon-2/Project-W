# 래그돌/물리 안전장치 (Ragdoll Safety Net)

특정 캐릭터(보스·뱀처럼 스케일이 크거나 cone/twist 허용각이 좁은 리그)에서 래그돌 활성 시
솔버가 폭주/NaN으로 깨지는 것을 막기 위한 **엔진측 안전장치 모음**이다. 디버그 출력/토글은
모두 제거했고, 실제 안정화에 기여하는 코드만 남겼다.

> **근본 원인 메모**: 이 사태의 1차 원인은 보스 ragdoll body가 추출 단계에서 BV 대비 4× 크게
> 추출된 데이터 버그였고 그건 추출기에서 수정됨. 다만 cone/twist 허용각이 좁은 리그(뱀 등)는
> 솔버 여유가 작아 안전장치가 없으면 여전히 불안정 → 안전장치를 유지한다.

각 항목은 소스에 `[SAFETY n]` 주석 태그가 있다(grep으로 위치 검색 가능). 아래에 **의도 / 동작 /
개별 비활성화(A·B 테스트) 방법 / 제거 시 예상 영향**을 정리한다. 하나씩 꺼 보며 영향을 확인할 것.

```
grep -rn "\[SAFETY" client/
```

---

## [SAFETY 1] 적분기 선형 속도 클램프 + NaN 가드
- **위치**: `client/physicsWorld.cpp` — `PhysicsWorld::integrate()` Dynamic 분기 (`[SAFETY 1]` 태그)
  + 파일 상단 익명 namespace의 `isFiniteV` / `isFiniteQ` 헬퍼.
- **의도**: 기존에 각속도 클램프(`kMaxAngularSpeed=50`)만 있었고 **선형 속도 클램프가 없었다**.
  미해결 조인트/접촉 체인이 큰 분리속도를 주입하면 위치가 한 스텝에 폭주→발산. 또한 단일 NaN이
  바디 상태를 오염시켜 렌더 시 device-removed 크래시로 번지는 것을 막는다.
- **동작**:
  - 선형 속도가 `kMaxLinearSpeed=40 m/s` 초과 시 방향 유지하며 크기 클램프.
  - 적분 전: `linearVel`/`omega`가 비유한이면 0으로.
  - 적분 후: `pos`/`orient`가 비유한이면 직전(prev) 유한 상태로 복원 + 속도 0.
- **테스트(비활성화)**:
  - 클램프만 끄기: `kMaxLinearSpeed`를 매우 크게(예: `1e9f`).
  - NaN 가드만 끄기: 해당 `if (!isFiniteV...)` 3블록 주석 처리.
- **제거 시 예상 영향**: 깊은 침투/특이 상황에서 선형 속도가 무제한 폭주 → 위치 점프 → NaN →
  (가드까지 없으면) 화면 깨짐/크래시.
- **튜닝 메모**: 40 m/s는 정상 낙하/넉백보다 충분히 높다. 매우 빠른 발사체/연출이 필요하면 상향.

## [SAFETY 2] 활성 시 초기 겹침 쌍 자동 무시
- **위치**: `client/ragdoll.cpp` — `Ragdoll::activate()` ignore 구성 블록 (`[SAFETY 2]` 태그).
- **의도**: ragdoll def는 흔히 rest 포즈에서 박스가 서로 파고든다(쇄골 vs 가슴/목 등). 1/2-hop
  무시만으로는 3-hop 이상 겹침이 남아, Dynamic 전환 즉시 큰 접촉 임펄스가 터진다(스케일에 비례).
- **동작**: 1-hop·2-hop 무시 쌍을 구성한 뒤, **seed 포즈에서 OBB가 실제로 겹치는 쌍**을
  `collides(OBB,OBB)`로 찾아 추가로 ignore 집합에 넣는다. (활성 시 1회, O(n²), n≈19.)
- **테스트(비활성화)**: `[SAFETY 2]` 블록 전체를 주석 처리 → 1/2-hop만 적용.
- **제거 시 예상 영향**: rest에서 겹쳐 있던 비인접 박스들이 활성 즉시 충돌 → 사지가 튀어오름
  (특히 큰 모델). 정상 추출이면 영향이 작지만, 좁은 몸통 리그에선 체감될 수 있다.
- **주의**: "처음부터 겹친" 쌍만 무시한다(정상 충돌해야 할 쌍은 영향 없음). 활성 중 새로 생기는
  겹침은 [SAFETY 6]가 담당.

## [SAFETY 3] ConeTwist 각도 rest(refOrient) 활성 포즈 재설정
- **위치**: `client/jointConstraint.cpp` — `ConeTwistJoint::resetAnchors()` (`[SAFETY 3]` 태그).
  (`resetAnchors()`는 `Ragdoll::activate()`에서 전 조인트에 호출됨.)
- **의도**: `build()`가 cone/twist의 각도 기준(`refOrientA_/B_`)을 **bind 포즈(T-pose)** 기준으로
  굳혀둔다. 활성 포즈(예: idle 팔 내림)가 bind와 다르면 첫 스텝부터 큰 swing/twist 위반이 생겨
  솔버가 limb을 bind로 끌어당기며 fighting → 좁은 한계 리그에서 jitter/불안정.
- **동작**: `resetAnchors()`에서 선형 피벗을 seed 포즈로 맞춘 뒤, `refOrientA_/B_`를 **현재 바디
  방향**으로 재설정 → 활성 시점이 곧 rest(위반 0)가 되어 한계는 그 포즈로부터 측정.
- **테스트(비활성화)**: `[SAFETY 3]`의 `refOrientA_ = ...; refOrientB_ = ...;` 두 줄 주석 처리 →
  bind 포즈 기준으로 복귀.
- **제거 시 예상 영향**: 사망/활성 포즈가 bind와 다른 캐릭터에서 활성 직후 팔이 bind 쪽으로 튕기는
  스냅 + 한계 fighting. 정상 추출이어도 idle≠bind면 체감.
- **부작용 주의**: 한계가 "활성 포즈 기준"으로 바뀌므로, 극단적 사망 포즈에서 활성하면 그 포즈로부터의
  이탈만 제한한다(해부학적 bind 기준이 아님). 보통은 더 자연스럽다.

## [SAFETY 4] 관성(inertia) half-extent 바닥값
- **위치**: `client/ragdoll.cpp` — `Ragdoll::build()` 바디 생성부 (`[SAFETY 4]` 태그).
- **의도**: `computeBoxInertia`는 half-extent 2개가 ~0인 "막대형" 박스에서 한 축 관성=0을 낸다.
  `setInertia`가 이를 역행렬로 만들면 **Inf invInertia** → 각임펄스에서 `0*Inf=NaN` → 조인트
  그래프를 통해 전 래그돌이 한 스텝에 오염.
- **동작**: **관성 계산용** half-extent를 각 성분 최대값의 5%로 바닥 처리(`floorH = maxH*0.05`).
  **충돌 박스(`rb.halfExtents`)는 원본 유지** — 모양/충돌엔 영향 없음.
- **테스트(비활성화)**: `[SAFETY 4]` 블록을 지우고 `setInertia(computeBoxInertia(bd.mass, scaledHalf))`로
  되돌림.
- **제거 시 예상 영향**: degenerate 박스가 있는 def에서만 즉시 전신 NaN. 정상 def면 영향 0
  (보스 로그상으로도 이 케이스는 없었음 → 예방용).
- **튜닝 메모**: 5%가 과하게 두꺼우면(관성이 너무 커서 둔하면) 1~2%로 낮춰도 무방.

## [SAFETY 5] 조인트 warm-start NaN 자가복구
- **위치**: `client/jointConstraint.cpp` — `finite0` 헬퍼(파일 상단) + 각 조인트 `prepare()` 시작
  (BallSocket/Hinge/ConeTwist, `[SAFETY 5]` 태그).
- **의도**: 한 번 비유한이 된 warm-start 누적 임펄스(`*AccImp`)는 매 `prepare()`에서 재적용되어
  영구 지속·전파된다. 적분기 NaN 가드([SAFETY 1])는 바디 속도만 0화할 뿐 **조인트 캐시는 못 건드린다**.
- **동작**: 각 `prepare()` 진입 시 누적 임펄스가 비유한이면 0으로 리셋 → 일회성 NaN이 다음 스텝에
  복구.
- **테스트(비활성화)**: 각 prepare의 `cache_.*AccImp = finite0(...)` 줄들을 주석 처리.
- **제거 시 예상 영향**: 다른 원인으로 NaN이 한 번 발생하면 그 조인트가 복구 불가 상태로 고착
  (래그돌이 영구히 깨진 채 유지). 단독으로는 NaN을 만들지 않으므로 평상시 영향 0.

## [SAFETY 6] 접촉 보정 침투깊이 상한
- **위치**: `client/contactConstraint.hpp` 상수 `kMaxCorrectionDepth=0.2f` +
  `client/contactConstraint.cpp` `ContactConstraint::prepare()` (`[SAFETY 6]` 태그).
- **의도**: 접촉 보정속도 `bias = beta·invDt·penetration`은 침투깊이에 비례한다. 비정상적으로 깊은
  겹침(과대 바디·스폰 겹침·터널링)이면 분리속도가 Inf로 폭주 → 속도반복 중 NaN. **이번 보스
  사태의 직접 메커니즘**이었다.
- **동작**: 보정에 쓰는 침투깊이를 `min(depth-slop, 0.2m)`로 상한. 깊은 침투는 폭발 대신 여러
  스텝에 걸쳐 점진 해소. (Box2D `b2_maxLinearCorrection`/Bullet의 표준 기법, 기존
  `staticDepenetration.kMaxCorrect=0.2`와 동일 철학.) **전 접촉(지형 포함)에 적용**.
- **테스트(비활성화)**: `penetration = std::max(0.f, cp.depth - kSlop)`로 되돌리거나
  `kMaxCorrectionDepth`를 크게.
- **제거 시 예상 영향**: 0.2m 이하 일반 침투엔 변화 없음. 깊은 겹침이 생기는 상황(과대 바디·스폰
  겹침)에서만 폭발 재발. → **가장 일반적이고 영향이 광범위한 안전장치이므로 마지막에 끌 것**.

---

## 권장 A/B 테스트 순서
좁은 한계 리그(뱀)에서 하나씩 꺼 보며 어느 것이 실제로 안정성에 기여하는지 확인:
1. [SAFETY 4] (degenerate 박스 없으면 영향 0일 것)
2. [SAFETY 2] (정상 추출이면 영향 작을 것)
3. [SAFETY 1] 클램프 / [SAFETY 5] 자가복구 (평상시 영향 0, NaN 시에만)
4. [SAFETY 3] (idle≠bind 캐릭터에서 jitter 여부)
5. [SAFETY 6] (가장 광범위 — 마지막)

각 항목을 끈 뒤 StandAlone에서 **K**로 해당 캐릭터를 래그돌화해 안정성(튐/깨짐) 관찰.
(K 토글은 `Game::toggleCasterRagdoll`, 중력 토글은 Z.)

## 정공법(엔진 밖)
안전장치는 안전망일 뿐, 근본은 **추출 데이터 품질**이다:
- ragdoll body 크기/스케일이 BV·메시와 정합되게 추출(이번 4× 버그처럼).
- cone/twist 허용각·twist축을 각 리그에 맞게 저작(좁은 한계가 fighting의 1차 트리거).

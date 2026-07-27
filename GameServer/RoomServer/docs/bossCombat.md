# 최종 보스 1:1 전투 (Boss)

미드보스(Hobgoblin/Grandbaum/Isys)는 `TacticalNpc` + platoon **전술 전투**지만,
**최종 보스는 전술 전투가 아닌 단순 1:1 전투**다. 클래스는 `class FinalBoss : public Npc`
(`RoomServer/finalBoss.{hpp,cpp}`)로, 중간 보스(`TacticalNpc`)와 구분하기 위해 `FinalBoss`로
명명한다. `Npc`를 상속하지만 **FSM이 아니라 자체 BehaviorTree로 AI를 구동**한다(`Npc::update`는
`virtual`, `FinalBoss::update`가 오버라이드해 FSM `switch` 대신 BT를 틱). 공격 실행은 일반
몬스터와 **동일한 스킬 경로**(`Room::skillStartInternal` → 권위 히트박스 + `S_SkillStart`)를
재사용하며, 차이는 FSM 균등 랜덤(`pickAttack`) → **BT 상황 기반(거리/쿨다운) 선택**이라는 점이다.

> 와이어 enum `ObjectType::Boss`와 클라 `class Boss : public Goblin`는 프로토콜/클라 호환을 위해
> 이름을 유지한다. 서버 클래스/파일/멤버(`finalBoss_`)/메서드(`setupFinalBoss`)만 `FinalBoss`로 변경.

## BehaviorTree AI (`RoomServer/finalBoss.cpp`)
- **프레임워크**: `RoomServer/BehaviorTree.{hpp,cpp}`(`BtSelector`/`BtSequence`/`BtCondition`/
  `BtCooldown`). `BtContext = { FinalBoss& boss; Room& room; }`(blackboard는 `FinalBoss` 멤버가 겸함).
- **타깃 평가(`evaluateTarget`)**: 보스 전용 Zone이라 **감지 거리 없음** — `Room::getLivingPlayers()`
  전원을 점수로 평가해 매 0.5s 또는 타깃 소실 시 재선택. `targetId_`=session id.
  - **점수(`scoreTarget`)** = 거리·저HP·위협을 각 [0,1]로 정규화한 **가중 합**:
    - 근접 `1/(dist+1)` × `TARGET_W_PROXIMITY`(1.0)
    - 저HP `1 - hp/kPlayerMaxHp` × `TARGET_W_LOWHP`(0.6) — 약한 플레이어 마무리 유도
    - 위협 `최근 보스를 때린 플레이어?1:0` × `TARGET_W_THREAT`(0.8). 위협 목록은 보스가
      chargeable이라 이미 채워지는 `Object::collectRecentDamagers(now, TARGET_THREAT_WINDOW)`를
      평가당 1회 수집해 공유. damager objId == session id이므로 `s->id()`로 매칭.
  - **히스테리시스**: 현재 타깃에 `TARGET_STICKY_BONUS`(0.15) 가산 → 비슷한 점수에서 0.5s마다
    타깃이 튀는 현상 방지(점수 차가 크면 정상 전환).
- **트리 구조**:
  ```
  Root(Selector)
   +- [예약] 페이즈/Rage 우선순위 가지 (미구현)
   +- EngageSeq[Cond:HasTarget] -> Combat(Selector)
        +- BossSkillBusyGuard                              // npcSkillActive면 Running 유지(시전 중 선점 방지)
        +- Cooldown(6s):[justCharged && d<=commit] -> Smite(3)      // 돌진 마무리 강타
        +- Cooldown(5s):[d<=commit]                -> Combo(1)      // 강타
        +- Cooldown(7s):[d<=commit]                -> BackAttack(2) // 변형타
        +- Cooldown(2.5s):[d<=commit]              -> Swings(0)     // 경타 필러
        +- BossEngageAction                                // 폴백: 선회 → 돌진 → 압박
   +- BossIdleAction                                       // 타깃 없음(드묾)
  ```
  `commit` = `attackCommitRange()` = `attackRange(3.0) × ATTACK_COMMIT_FRACTION(0.6)` = **1.8m**.
  종전엔 전부 `attackRange` 끝자락(3.0)에서 발동해, 정지 시전이다 보니 플레이어가 한 발만 물러나도
  쉽게 회피됐다. 히트박스는 4종 모두 같은 `weapon_r` OBB라 **실제 리치가 동일**하므로 거리 조건도
  하나로 통일했다 — Smite는 더 이상 "갭 클로저"가 아니라(그건 이동 시전이 필요한데 롤백했다)
  **돌진이 닿은 직후에만 고르는 마무리 강타**다(`justCharged()`).
- **공격 리프**: `BossSkillAttackAction(idx)`가 타깃 조준 + `switchClip` + `skillStartInternal`
  (`damageScale_`)로 1회 시전 후 `Success`(`BtCooldown`이 시전 시점부터 카운트). 이후 `BossSkillBusyGuard`가
  스킬 종료까지 트리를 Running으로 잡아 다른 공격이 끼어들지 못하게 한다(스킬 lua 타임라인이
  windup/hit/recover를 담당하므로 멀티페이즈 노드 불필요).
- **시전 중에는 완전 정지한다**(`BossSkillBusyGuard` → `FinalBoss::halt()`).
  걸으며 휘두르는 안을 한 번 넣었다가 되돌렸다 — 공격 클립의 windup이 크고 무거워서 발이 아직
  움직이는 채로 재생하면 부자연스러웠다. 뒤로 빠지는 상대에 대한 답은 "이동 공격"이 아니라
  **휘두르기 전에 충분히 파고드는 것**이다(`attackCommitRange` + 아래 교전 패턴).
### 교전 패턴: 선회 → 돌진 → 압박 (`FinalBoss::engage`, 2026-07-27 인게임 검증 완료)

무조건 플레이어를 향해 직진하던 `BossChaseAction`을 3상태 기계 `BossEngageAction`으로 교체했다.

| 상태 | 동작 | 전이 |
|---|---|---|
| **circling** | 타깃을 **계속 바라본 채**(`faceToward` 매 틱) 걷기 속도로 옆걸음, 반경 `STRAFE_RADIUS(6.0)` 유지. 방향은 랜덤 타이머(`STRAFE_FLIP_MIN/MAX` 1.2~2.8s)로 반전 | `chargeTimer_`(2.0~4.5s 랜덤) 만료 또는 `dist > CHARGE_FORCE_DISTANCE(9.0)` → charging |
| **charging** | 타깃 예측 위치로 **직선 질주**(8.75 m/s), 진행 방향으로 회전 | `dist <= commit` → pressing |
| **pressing** | 사거리 안에 눌러앉아 근접 쿨다운이 한 번 더 터지게 함. `commit × PRESS_HOLD_FRACTION(0.7)` 밖이면 걸어서 붙고, 안이면 정지 | `POST_CHARGE_WINDOW(3.0s)` 만료 → circling |

- **옆걸음의 핵심은 `moveToward(dest, mult, dt, reorient=false)`** — 속도만 접선 방향으로 주고
  회전은 `faceToward(타깃)`가 따로 잡는다. 그래야 속도 벡터가 보스 forward와 어긋나고,
  클라의 4방향 blend space가 이를 `Boss_Walk_Left/Right`로 렌더한다(그래서 4방향 walk가 필요하다).
- **방향 반전은 0을 지나며 연속으로 슬루한다**(`STRAFE_TURN_RATE`). 부호를 즉시 뒤집으면 클라의
  Left↔Right가 한 프레임에 뒤바뀌어 튄다. 접선이 0에 가까워지는 구간에서 잠깐 멈춰 서는 것이
  발을 딛고 반대로 도는 것처럼 보인다.
- **돌진 거리 = 선회 반경**이다. 6.0m는 램프(2.7m) 뒤에도 최고 속도 구간이 남도록 고른 값 —
  4.5m로 낮추면 램프 도중에 도착해 클라가 `Boss_Run`을 제대로 보여주지 못한다.
- **`charging_ → pressing` 전이는 `FinalBoss::update`가 소유한다.** Combat selector가 공격 리프를
  engage보다 **먼저** 평가하므로, 돌진이 사거리에 닿는 틱은 대개 공격이 engage를 선점하는 틱이다.
  전이를 engage 안에 두면 `charging_`이 영원히 true로 남고 post-charge 창이 열리지 않는다.

### 속도 램프 (walk ↔ run 전환)
`moveToward`는 목표 속도로 **즉시 점프하지 않고** `MOVE_ACCEL(12.0)`/`MOVE_DECEL(14.0)` m/s²로 램프한다.
클라는 브로드캐스트되는 velocity로 gait를 추론하는데, 3.5 → 8.75를 한 프레임에 뛰면 클라의
walk↔run 블렌드 밴드(4.0~7.0)를 통째로 건너뛰어 **크로스페이드가 아예 재생되지 않는다** —
전환이 툭툭 끊겨 보이던 원인. 램프하면 밴드를 0.3~0.4s에 걸쳐 훑고 지나간다.
`halt()`(시전/대기/사망)만 즉시 정지하며 램프 상태(`curSpeed_`)도 함께 0으로 접는다.

- **속도와 클립을 반드시 같이 바꾼다** — 클라는 state를 안 받고 velocity로 gait를 추론하므로
  `switchClip("Run")`만 해서는 클라가 계속 걷는 모션을 섞는다(종전 버그). 클립 전환은
  서버 본(피격 BVH)용, 클라 gait를 바꾸는 건 브로드캐스트되는 속도다.
- **알려진 비대칭**: 옆걸음 중 서버는 `"Walk"`(=`Boss_Walk_Forward`) 하나만 재생한다. 서버
  `AnimController`는 블렌딩이 없고, 방향별로 `switchClip`을 갈아끼우면 경계에서 클립이 계속
  0부터 재시작해 오히려 서버 본이 떨린다. 보스 피격 BVH 박스는 대부분 몸통
  (`Root/Spine/LowerChest/UpperChest/Neck`)이라 다리 포즈 차이의 영향이 작다 — 합의된 오차.
- **빌드 시점**: `Room::setupFinalBoss`가 `addAttack` 4종 등록 **직후** `buildBehaviorTree()` 호출
  (리프가 인덱스 0~3으로 skillId/clipKey 참조).
- **튜닝 포인트**: 공격 확정 거리(`ATTACK_COMMIT_FRACTION`), 선회(`STRAFE_RADIUS`/`STRAFE_RADIAL_GAIN`/
  `STRAFE_FLIP_MIN|MAX`/`STRAFE_TURN_RATE`), 돌진(`RUN_SPEED_MULT`/`CHARGE_INTERVAL_MIN|MAX`/
  `CHARGE_FORCE_DISTANCE`), 압박(`POST_CHARGE_WINDOW`/`PRESS_HOLD_FRACTION`),
  속도 램프(`MOVE_ACCEL`/`MOVE_DECEL`),
  스킬별 쿨다운, `damageScale_`, 타깃 점수 가중치
  (`TARGET_W_PROXIMITY`/`TARGET_W_LOWHP`/`TARGET_W_THREAT`/`TARGET_STICKY_BONUS`/`TARGET_THREAT_WINDOW`).
  로코모션 재생 배속(`animRefSpeed`/`animBandEnd`)은 `client/docs/gameArchitecture.md` 참조.

## 리소스
- 모델: 클라 `resources/boss/boss.bin`, 서버 `resources/boss/bossServer.bin`
- 애니: `resources/boss/bossAnimations.anim` (클라/서버 공용 소스), 14클립:
  `Boss_Idle`, `Boss_Walk_Forward/Backward/Left/Right`, `Boss_Run`, `Boss_Hit1`, `Boss_Hit2`,
  `Boss_Death`, `Boss_Rage`, 공격 4종 `Boss_Swings`/`Boss_Combo`/`Boss_BackAttack`/`Boss_Smite`
- AssetManager: 클라 `modelBoss()`/`bossAnimations()`, 서버 동일 추가

## 애니메이션 렌더링 (클라 `AnimBlenderBoss`)
- **이동**: 플레이어식 blend space. `velocity·right/forward`로 4방향 가중치 산출.
  속력 저대역=4방향 walk(`Boss_Walk_*`) 블렌딩, 고대역=`Boss_Run`. 서버는 velocity만 보내고
  클라가 블렌딩(state 미전송, 다른 몬스터와 동일). 옆걸음(`engage`의 circling)이 실제로
  `Boss_Walk_Left/Right`를 태우는 경로다.
- **walk/run 위상 동기(2026-07-27)**: 두 클립을 **하나의 정규화 stride 위상**(`locoPhase_`)에서
  구동한다. 종전엔 각자 진행했고 `Boss_Run`은 관여할 때마다 0부터 재시작해서, 크로스페이드가
  **디딘 발과 뻗은 발을 평균내는** 상태였다 — gait 전환마다 다리가 움찔한 원인. 위상은 가중
  블렌드된 클립 길이로 진행하므로 밴드를 지나는 동안 케이던스도 연속이다.
  두 클립이 같은 발에서 시작하는 1 스트라이드 사이클이라고 가정한다 — 반 발짝 어긋나 보이면
  여기 고정 오프셋을 주면 된다(블렌드 버그가 아님).
- **다중 공격**: `attackClips_`=[Swings,Combo,BackAttack,Smite]. `EvAttack.attackIndex`로 선택.
  attackIndex는 각 스킬 lua의 `PlayAnimation.attackIndex`(0~3)에서 옴(스킬 시스템이 EvAttack로 전파).
  오버레이 길이 = **min(선택 클립 길이, 스킬 잔여 시간)** — 아래 "공격 후 미끄러짐" 참조.
- **피격**: `hitClips_`=[Hit1,Hit2]. `EvHit.hitAnimIndex`로 선택.
- **상하체 분리 마스크(2026-07-27)**: 공격·피격 **두 오버레이 모두** 본별 상체 가중치를 곱한다 —
  `w = t * (mask + (1-mask)*tIdle_)`. 정지 시(`tIdle_=1`) 종전 전신 오버레이와 프레임 단위 동일,
  이동 시 하체는 로코모션 클립 유지. 마스크는 `AnimBlender::buildUpperBodyMask`(기반 클래스 공용,
  플레이어와 동일 규칙: `spine_01` 서브트리=상체)가 init 시 1회 구축한다. death는 전신 유지.
  보스 리그는 UE 마네킹 계열(`pelvis`→`spine_01`..`spine_05`, `weapon_r`)이라 규칙이 그대로 성립.
  상세: `client/docs/aimPitchUpperBodyMask.md` §2, §7.
  - **허용 오차**: 이동 시전 중 서버는 전신 공격 클립으로 판정하고 클라 하체는 로코모션이라
    `weapon_r` 히트박스 앵커에 소량 오차가 생긴다. 보스 히트박스는 `BoneAttach`(애니 추종)라
    플레이어의 `AttachType::Body` 같은 구조적 해소책이 없다 — 합의된 오차.
- **Rage**: 등록만(트리거 미연결 — BT 구현 시 연결).

### 플레이어에게 밀리지 않는다 (2026-07-27 인게임 검증 완료)

"보스가 캐릭터와 충돌해 너무 쉽게 밀려나고, 한 발자국씩 순간이동하는 것처럼 보인다" 대응.
보스는 거대한 나무이므로 **플레이어에게는 밀리지 않는다.** 밀림 경로가 두 개였고 둘 다 막았다.

**① 접촉 depenetration.** 서버 플레이어 바디는 `Kinematic`이고 `Room::move`가 C_Move(20Hz)마다
`setPos`로 **텔레포트**한다 — 10 m/s면 한 패킷에 최대 50cm를 파고든다. 그 깊이가
`kMaxCorrectionDepth(0.2m)`로 잘린 뒤 `bias = kBaumgarteBeta·penetration/dt = 0.2·0.2·60 = 2.4 m/s`로
보스를 밀어냈고, 패킷 단위로 몰려 들어오니 뚝뚝 끊겨 보였다.
> **질량을 올려도 해결되지 않는다.** 무한 질량(Kinematic) 상대와의 접촉에서 보스가 얻는 속도 변화는
> `j·invMass = (effMass·bias)·invMass = bias`로 **질량이 약분된다.** 200을 20000으로 바꿔도 동일하다.
> 이 관계를 모르면 질량 튜닝으로 시간을 버리게 된다.

→ `setCollisionMask(~CollisionLayer::Player)`로 **플레이어↔보스 쌍을 broad phase 필터에서 제외**
(`PhysicsWorld::generateContacts`의 category/mask 검사). 카테고리도 미드보스와 같이
`CollisionLayer::Boss`로 맞췄다. 지형은 `TerrainCollider`(별도 `worldColliders_` 경로)라 영향 없음 —
중력·접지·직립은 그대로 동작한다.

**② 스킬 OnHit impulse.** 플레이어 근접 스킬의 `impulseStrength`는 350~1200이고 질량이 200이라
**한 방에 1.75~6 m/s**가 실린다(①의 상한보다 크다). 임펄스는 `updateSkillSystem`에서 실리고
다음 틱 AI(`halt`/`moveToward`)가 속도를 덮어쓰므로 **정확히 한 물리 step만 이동** → 히트마다
6~10cm씩 튀는, 딱 "한 발자국 순간이동"의 모양새. → `setHitImpulseImmune(true)`
(ShieldWall 슬라임과 같은 기존 opt-out). 피격 리액션은 `hitAnimIndex` 경로라 타격감은 유지된다.

**③ 그런데 플레이어를 막는 건 누구였나 — 클라로 이관.** 클라는 몬스터 바디를 PhysicsWorld에
**등록하지 않는다**(`configureNetMonster`). 즉 종전에 플레이어를 막아준 유일한 힘이 ①의 반작용이었고,
①을 끄면 보스가 유령이 된다. → 클라 `Game::resolveBossSeparation`을 추가했다.
`resolveBarrierSeparation`과 동일한 규칙(움직이지 않는 서버 권위 객체 → 침투량 100%를 플레이어가
위치 보정으로 받음, 임펄스 없음 → 튕김 없음), 물리 sub-step마다 호출.

> ⚠ **불변식**: `kPlayerSeparationRadius(0.4) + kBossSeparationRadius(0.7) = 1.1m` 는 보스가 접근해
> 멈추려는 거리 `attackCommitRange(1.8) × PRESS_HOLD_FRACTION(0.7) = 1.26m`보다 **작아야 한다.**
> 크면 보스가 목표 거리까지 붙지 못해 플레이어를 계속 밀어붙이며 배회한다. 몸통을 굵게 하려면
> 서버의 두 상수도 같이 올릴 것.

미드보스(`platoonLeader_`)는 종전대로 플레이어 충돌·임펄스를 유지한다(같은 증상이 있지만 별건).

### 공격 후 "미끄러짐" — 원인 3가지 (2026-07-27, 전부 수정 · 인게임 검증 완료)

"공격 이후 이동할 때 이동 애니메이션이 곧바로 재생되지 않고 미끄러진다"의 원인. 측정값 기준.

**① 공격 오버레이가 스킬보다 오래 산다 (주 원인).** 저작된 클립 길이가 lua `totalDurationMs`보다 길다:

| 공격 | 서버 스킬 | 클립 | 초과 |
|---|---|---|---|
| Smite | 1200ms | 2267ms | **+1067ms** |
| Combo | 3200ms | 4200ms | **+1000ms** |
| Swings | 2400ms | 2733ms | +333ms |
| BackAttack | 2600ms | 2500ms | −100ms |

보스가 정지하는 건 **스킬 길이만큼**이다(`BossSkillBusyGuard`). 스킬이 끝나면 AI가 즉시 걸어나가는데
클라 오버레이는 클립 끝까지 `tAttack_=1`로 남아, 상하체 마스크의 하체 가중치(`tAttack_ × tIdle_`)가
살아 있다 — 특히 pressing 상태는 걷기/정지를 반복하므로 `tIdle_`이 커지는 순간이 많아 다리가 공격
포즈에 붙는다. → **오버레이 상한 = 스킬 잔여 시간**(`EvAttack::skillRemaining`, 스킬 시스템이
`asset->totalDuration - elapsed`로 실어 보낸다) + 잘린 스윙이 한 프레임에 튀지 않게 **200ms 페이드아웃**.
오버레이 수명이 "보스가 다시 움직일 수 있는 시점"과 같은 권위에서 나오므로 조용히 재발할 수 없다.
(플레이어 블렌더는 이 필드를 아직 쓰지 않는다 — 2026-07-24 검증된 감각을 건드리지 않기 위해 단계적 이관.)

**② 몬스터 네트워크 보간 구간이 전송 주기와 불일치.** `client/docs/gameArchitecture.md` 8단계 참조 —
`netInterpDuration_` 50ms(20Hz) vs `S_NpcMoveBatch` 60Hz. 서버 step의 1/3만 보간되고 2/3는 점프로
도착해, 평균 속도는 맞는데 렌더가 스터터였다(애니 배속은 진짜 velocity 기준이라 발이 어긋난다).
→ `configureNetMonster`가 몬스터 한정 `kNpcMoveInterval(1/60s)`로 설정. **문서에 "미해결"로 남아 있던 항목.**

**③ `locoRate_`를 정지 중 1.0으로 리셋.** `solveLocomotionRate`의 지수 평활 시드가 되므로, 걷기 정상
상태(0.73)와 먼 1.0에서 출발해 이동 재개마다 ~0.3s를 과속 재생했다(보스는 `halt()`로 수시로 멈춘다).
→ 리셋 제거(정지 중에는 읽히지 않는 값이라 유지해도 무해하다).

## 공격 패턴
- 공격 4종은 각각 Skill 스크립트(`resources/skills/boss_{swings,combo,backattack,smite}.lua`).
  길이: Smite 1200 / Swings 2400 / BackAttack 2600 / Combo 3200ms.
- 선택은 **BT 상황 기반**이다(위 트리 구조 참조). `Npc::pickAttack()`의 균등 랜덤은 일반 몬스터용이고
  FinalBoss는 쓰지 않는다. 선택된 공격은 `skillStartInternal`로 서버 권위 히트박스 시전 +
  `S_SkillStart` 브로드캐스트 → 클라는 같은 스킬을 재생(히트박스/VFX 결정론 동기).
- 등록: `Room::setupFinalBoss`가 `addAttack(skillIdByName("Boss_X"), "X")` 4종 → 직후 `buildBehaviorTree()`.
- 히트박스는 4종 모두 `BoneAttach("weapon_r")` **플레이스홀더**(에디터 튜닝 대기).

## Hit 애니메이션 동기화 (서버 권위)
- 보스가 피격되면 서버가 `Boss_Hit1`/`Boss_Hit2` 중 랜덤 선택 → `SSkillHitPacket.hitAnimIndex`로
  전달(정확한 충돌 처리를 위해 전 클라 동일 재생). 단일 hit 몬스터는 index=0(무시).
- 경로: `Room::updateSkillSystem`(보스 타깃이면 rand 0/1) → `makeSSkillHitPacket(...,hitAnimIndex)`
  → 클라 `onSkillHit`→`applyHit`→`EvHit(...,hitAnimIndex)` → `AnimBlenderBoss`.

## 스폰 (보스 아레나 zone 트리거 → 런타임)
- 레벨 청크에 zone **`"Arena_Boss"`**(factionMask=Players)과 마커 `type=="BossSpawner"` 저작.
  (`Room.cpp:571`의 실제 태그. 코드 주석과 이 문서가 한동안 "ArenaZone"으로 잘못 적혀 있었다.)
- 서버 `Room::bindZoneHandlers`가 `Arena_Boss` Enter → `onArenaBossEnter`:
  `BossSpawner` 마커 위치(없으면 진입 플레이어 위치 fallback)에 보스 1마리 런타임 생성
  → 물리/`objectById_`/`npcBodyOwner_` 등록 → `S_NpcSpawnBatch`(type=`ObjectType::Boss`) 브로드캐스트.
  platoon/아레나 벽 없음. 1회성(zone disarm). 보스 AI home=spawn, activityZone 반경 60.
- 클라 디스패치: `PacketManager` S_Enter/S_NpcSpawnBatch 두 switch에 `ObjectType::Boss → createBoss`.

## 서버 구조
- `RoomServer/finalBoss.{hpp,cpp}`: `class FinalBoss : public Npc` + `applyBossConfig()`(HP 2000,
  range/속도 등) + BT(`update` 오버라이드/`buildBehaviorTree`/리프 헬퍼). (구 `boss.{hpp,cpp}`에서 rename.)
- `Room`: `std::unique_ptr<FinalBoss> finalBoss_`(단일, 런타임 스폰, 주소 안정). `updateMonsterAI`에서
  인라인 틱(FinalBoss는 Goblin의 lag-comp `recordSnapshot`이 없어 tickPool 미사용). `makeSEnterPacket`에
  중도 입장자용 포함. `~Room`에서 body unregister + id 반납. 스킬 경로로 데미지가 나가므로 `update`의
  `result.hit`는 빈값 → 인라인 틱이 레거시 `S_NpcAttack`/`S_Hit`를 쏘지 않음(스킬 NPC와 동일).

## 클라 구조
- `class Boss : public Goblin`(EventBus/ragdoll 재사용, `setAnimBlender`만 오버라이드 → `AnimBlenderBoss`).
- `Online::Game`: 전용 `bosses_`/`bossHpBars_`/`bossPool_` + `MonsterKind::Boss`. 시체/리스폰/렌더/
  업데이트/BVH/컬링/Hi-Z 루프 전부에 보스 통합(다른 몬스터와 동일 파이프라인).

## 디버그: StandAlone 래그돌 테스트
- StandAlone(스킬 에디터)에서 **K 키**로 현재 컨트롤 중인 객체(에디터 caster)를 래그돌화/되돌리기 토글.
  보스 래그돌 검증용 — 캐스터를 Boss로 핫스왑(setMonsterCaster) 후 K로 collapse/복원.
- 구현: `client/standalone/game.cpp::toggleCasterRagdoll`(현재 모델 기준 ragdoll 재빌드 → seed/passenger/
  activate, 되돌리기는 deactivate/destroy+body 재등록). 프레임 sync는 `casterObj`까지 일반화,
  caster 노출은 `Editor::Controller::controlledObject()`. (중력 토글은 Z)
- 전제: 보스 모델(`boss.bin`)에 `ragdollDef`가 있어야 함.

### 래그돌 폭발("튀다가 깨짐") — 원인과 안전장치 (2026-06-21)
- **1차 원인(데이터)**: 보스 ragdoll body가 추출 단계에서 BV 대비 **4× 크게 잘못 추출**됨. 과대 바디가
  몸통/사지끼리 수 m 깊이로 겹쳐 접촉 Baumgarte 보정속도가 폭주→NaN. → **추출기 수정으로 해결**. 뱀도 동일.
- **그래도 안전장치 필요**: cone/twist 허용각이 좁은 리그(뱀 등)는 솔버 여유가 작아, 추출을 고쳐도
  안전장치가 없으면 불안정. → 추적 중 만든 엔진측 안전장치 6종을 **디버그 출력만 빼고 모두 유지**.
- **안전장치 6종 + 개별 A/B 테스트 가이드**: `client/docs/ragdollSafety.md` (소스에 `[SAFETY 1..6]` 태그).
  요약: ①적분기 선형클램프+NaN가드 ②활성 시 초기겹침 자동무시 ③ConeTwist refOrient 활성포즈 재설정
  ④관성 half-extent 바닥값 ⑤조인트 warm-start NaN 자가복구 ⑥접촉 침투깊이 상한(0.2m, 표준 기법).
- 진단 코드(`Ragdoll::diagnose`/`Constraint::diagnose`/`[PhysNaN]` 단계검출/`[RagdollDiag]` 로그/J·L 토글)는
  원인 확정 후 **제거**. (서버는 래그돌 없어 무관.)

## 향후
- **Rage/페이즈 전이**(미구현): BT Root 최상단의 예약 우선순위 가지 + 클라 `AnimBlenderBoss` Rage
  트리거 + 전용 패킷이 필요. `Boss_Rage` 클립은 등록만 돼 있음.
- 공격 거리 밴드/쿨다운/`damageScale` 콘텐츠 튜닝(스킬 lua 히트박스 사거리와 정합).
- ~~타깃 점수에 저HP/위협 가중치 추가~~ → **구현됨**(거리+저HP+위협 가중 합 + 히스테리시스).
  돌진 등 신규 패턴은 전용 스킬 lua 저작 후 BT 리프 추가.

# 최종 보스 1:1 전투 (Boss)

미드보스(Hobgoblin/Grandbaum/Isys)는 `TacticalNpc` + platoon **전술 전투**지만,
**최종 보스는 전술 전투가 아닌 단순 1:1 전투**다. 따라서 보스는 platoon 인프라가 아니라
필드 몬스터 FSM인 `Npc`(Goblin/Snake의 베이스)를 기반으로 한다. 보스 전용 AI(BehaviorTree)는
추후 별도 구현 예정이며, 그 전까지는 기본 `Npc` FSM(Idle/Patrol → Chase → AttackWindup →
AttackRecover)으로 1:1 전투가 동작한다.

## 리소스
- 모델: 클라 `resources/boss/boss.bin`, 서버 `resources/boss/bossServer.bin`
- 애니: `resources/boss/bossAnimations.anim` (클라/서버 공용 소스), 14클립:
  `Boss_Idle`, `Boss_Walk_Forward/Backward/Left/Right`, `Boss_Run`, `Boss_Hit1`, `Boss_Hit2`,
  `Boss_Death`, `Boss_Rage`, 공격 4종 `Boss_Swings`/`Boss_Combo`/`Boss_BackAttack`/`Boss_Smite`
- AssetManager: 클라 `modelBoss()`/`bossAnimations()`, 서버 동일 추가

## 애니메이션 렌더링 (클라 `AnimBlenderBoss`)
- **이동**: 플레이어식 blend space. `velocity·right/forward`로 4방향 가중치 산출.
  속력 저대역=4방향 walk(`Boss_Walk_*`) 블렌딩, 고대역=`Boss_Run`. 서버는 velocity만 보내고
  클라가 블렌딩(state 미전송, 다른 몬스터와 동일).
- **다중 공격**: `attackClips_`=[Swings,Combo,BackAttack,Smite]. `EvAttack.attackIndex`로 선택.
  attackIndex는 각 스킬 lua의 `PlayAnimation.attackIndex`(0~3)에서 옴(스킬 시스템이 EvAttack로 전파).
- **피격**: `hitClips_`=[Hit1,Hit2]. `EvHit.hitAnimIndex`로 선택.
- **Rage**: 등록만(트리거 미연결 — BT 구현 시 연결).

## 확률적 공격 패턴
- 공격 4종은 각각 Skill 스크립트(`resources/skills/boss_{swings,combo,backattack,smite}.lua`).
- 서버 `Npc::pickAttack()`(균등 랜덤)이 매 스윙마다 하나 선택 → `skillStartInternal`로 서버 권위
  히트박스 시전 + `S_SkillStart` 브로드캐스트. 클라는 같은 스킬을 재생(히트박스/VFX 결정론 동기).
- 등록: `Room::setupBoss`가 `addAttack(skillIdByName("Boss_X"), "X")` 4종.

## Hit 애니메이션 동기화 (서버 권위)
- 보스가 피격되면 서버가 `Boss_Hit1`/`Boss_Hit2` 중 랜덤 선택 → `SSkillHitPacket.hitAnimIndex`로
  전달(정확한 충돌 처리를 위해 전 클라 동일 재생). 단일 hit 몬스터는 index=0(무시).
- 경로: `Room::updateSkillSystem`(보스 타깃이면 rand 0/1) → `makeSSkillHitPacket(...,hitAnimIndex)`
  → 클라 `onSkillHit`→`applyHit`→`EvHit(...,hitAnimIndex)` → `AnimBlenderBoss`.

## 스폰 (ArenaZone 트리거 → 런타임)
- 레벨 청크에 zone `"ArenaZone"`(factionMask=Players)과 마커 `type=="BossSpawner"` 저작.
- 서버 `Room::bindZoneHandlers`가 `ArenaZone` Enter → `onArenaBossEnter`:
  `BossSpawner` 마커 위치(없으면 진입 플레이어 위치 fallback)에 보스 1마리 런타임 생성
  → 물리/`objectById_`/`npcBodyOwner_` 등록 → `S_NpcSpawnBatch`(type=`ObjectType::Boss`) 브로드캐스트.
  platoon/아레나 벽 없음. 1회성(zone disarm). 보스 AI home=spawn, activityZone 반경 60.
- 클라 디스패치: `PacketManager` S_Enter/S_NpcSpawnBatch 두 switch에 `ObjectType::Boss → createBoss`.

## 서버 구조
- `RoomServer/boss.{hpp,cpp}`: `class Boss : public Npc` + `applyBossConfig()`(HP 2000, range/속도 등).
- `Room`: `std::unique_ptr<Boss> boss_`(단일, 런타임 스폰, 주소 안정). `updateMonsterAI`에서 인라인 틱
  (Boss는 Goblin의 lag-comp `recordSnapshot`이 없어 tickPool 미사용). `makeSEnterPacket`에 중도
  입장자용 포함. `~Room`에서 body unregister + id 반납.

## 클라 구조
- `class Boss : public Goblin`(EventBus/ragdoll 재사용, `setAnimBlender`만 오버라이드 → `AnimBlenderBoss`).
- `Online::Game`: 전용 `bosses_`/`bossHpBars_`/`bossPool_` + `MonsterKind::Boss`. 시체/리스폰/렌더/
  업데이트/BVH/컬링/Hi-Z 루프 전부에 보스 통합(다른 몬스터와 동일 파이프라인).

## 디버그: StandAlone 래그돌 테스트
- StandAlone(스킬 에디터)에서 **K 키**로 현재 컨트롤 중인 객체(에디터 caster)를 래그돌화/되돌리기 토글.
  보스 래그돌 검증용 — 캐스터를 Boss로 핫스왑(setMonsterCaster) 후 K로 collapse/복원.
- 구현: `client/standalone/game.cpp::toggleCasterRagdoll`(현재 모델 기준 ragdoll 재빌드 → seed/passenger/
  activate, 되돌리기는 deactivate/destroy+body 재등록). 프레임 sync는 `casterObj`까지 일반화.
  caster 노출은 `Editor::Controller::controlledObject()`. (중력 토글은 Z)
- 전제: 보스 모델(`boss.bin`)에 `ragdollDef`가 있어야 함(없으면 로그 후 무동작).

### 래그돌 폭발("튀다가 깨짐") 진단
- `Ragdoll::diagnose()`(`client/ragdoll.{hpp,cpp}`): 매 프레임 호출 가능한 읽기전용 스캔 —
  바디별 NaN/Inf, 최대 선속도/각속도, 조인트 앵커 분리(stretch)/limit 위반(rad)을 본 인덱스와 함께 보고.
- 조인트 위반 노출: `Constraint::diagnose()`(가상) → `ConstraintDiag{linearError, angViolation}`.
  `ConeTwistJoint`은 `linearError`/`max(cone,|twist|)`, BallSocket/Hinge는 월드 앵커 분리(`jointConstraint.cpp`).
- StandAlone 로깅(`game.cpp`): K로 활성화 시 `[RagdollDiag/ON]` 초기 스냅샷, 매 프레임 불안정(속도/각속도/
  stretch/각위반 임계 초과 또는 NaN) 전이 시 `[RagdollDiag/UNSTABLE]` 1회. NaN 발생 시 자동 deactivate(렌더
  크래시 방지 + 마지막 상태 로그). 본 이름은 `controlledObject` 모델 스켈레톤으로 해석.
- **읽는 법**: `ON` 스냅샷의 `maxStretch`/`maxAngViol`이 이미 크면 → 보스 `ragdollDef`(앵커/limit/축)가 보스
  스켈레톤·bind 포즈와 불일치(가장 유력). `numBodies`가 기대보다 적으면 → def 본 이름 불일치로 build 누락.
  활성 직후엔 작다가 수 프레임 내 `maxStretch`/속도가 폭증하면 → solver 발산(질량/관성·scale·limit 부적합).
  `activate()`는 `resetAnchors()`로 선형 피벗만 seed 포즈에 맞추고, 각도 limit refOrient는 build(bind 포즈)
  기준이라 死 포즈가 bind와 크게 다르면 각위반이 큼.

### 래그돌 폭발 원인 규명 & 수정 (2026-06-21)
진단 로그로 3중 원인 확인(보스 modelScale=4×가 모두 증폭):
1. **self-collision 부족**: 기존 무시 범위가 1/2-hop뿐 → 보스 상체 박스가 3-hop+에서 겹쳐 활성 즉시 거대한
   접촉 임펄스(스케일 비례). 로그에서 self-collision OFF 시 62.7→2.4 m/s로 급감해 확정.
2. **선형 속도 클램프 부재**: 적분기에 omega 클램프(50rad/s)만 있고 선형 클램프가 없어 62 m/s 폭주가
   무제한 → 위치 점프 → NaN.
3. **각도 기준 불일치**: `activate()`의 `resetAnchors()`가 선형 피벗만 seed 포즈로 맞추고 ConeTwist
   `refOrient`(각도 rest)는 build 시 bind(T-pose) 그대로 → 활성 포즈(idle arms-down)와 어긋나 cone/twist가
   첫 스텝부터 ~2.5rad(143°) 위반 → 솔버가 bind로 끌어당기며 fighting → 저속에서도 NaN.

엔진측 일반 안정화로 수정(모든 래그돌 공통, 보스 한정 아님):
- `client/physicsWorld.cpp integrate()`: 선형 속도 클램프(40 m/s) + 비유한(NaN/Inf) 가드(속도 0화, 위치는
  직전 유한 상태로 복원) — 폭주·NaN 전파 차단(렌더 device-removed 방지).
- `client/ragdoll.cpp activate()`: seed 포즈에서 이미 겹친 바디 쌍을 `collides(OBB,OBB)`로 자동 무시
  (1/2-hop에 더해). def 박스 간 초기 침투로 인한 접촉 폭발 제거.
- `client/jointConstraint.cpp ConeTwistJoint::resetAnchors()`: 각도 rest(`refOrient`)를 활성(seed) 포즈로
  재설정 → 활성 시 초기 cone/twist 위반 0, bind와의 어긋남으로 인한 fighting 제거.
- 진단: `Constraint::diagnose()` + `Ragdoll::diagnose()` + StandAlone K/J 토글·`[RagdollDiag/Detail]` 로그 유지.
> 잔여 과제: 보스 `ragdollDef`의 cone/twist limit·twist축이 보스 리그에 맞게 저작됐는지(현재는 활성 포즈
> 기준 재설정으로 회피). 정식으로는 Unity 익스포트에서 보스 전용 limit 재작성 권장.

### 2차: 잔존 NaN(폭발은 멈췄으나 NaN 주입) — 자가복구 + limit 노브 (2026-06-21)
위 수정 후 **폭발은 사라짐**(stretch<0.1m, 속도<4m/s)이나, cone/twist **위반이 0인(=limit 비활성)** 바디에서도
NaN이 발생·체인 전파. 원인: 적분기는 바디 속도를 0화하지만 **조인트 warm-start 누적(`*AccImp`)에 남은 NaN이
매 prepare()에서 재적용**되어 영구 지속. (snake·boss 공통점은 좁은 cone/twist 허용각 → 한계가 거의 항상
활성 → 강한 보정 누적이 한 번 비유한이 되면 고착.)
- **NaN 자가복구**: `jointConstraint.cpp` 각 `prepare()` 시작에서 비유한 누적 임펄스를 0으로(`finite0`).
  Ball/Hinge/ConeTwist 전부. 일회성 NaN이 다음 스텝에 복구됨(전파 차단).
- **limit 배율 노브(L키)**: `Ragdoll::setLimitScale`가 build에서 cone/twist(및 hinge) 허용각을 ×배(코ーン은
  π·0.85 클램프). StandAlone L=1→2→4 순환, 다음 K부터 적용. 좁은 한계가 원인인지 즉시 A/B.
> **허용각 확대 검토 결론**: 현 NaN은 limit 비활성 바디에서도 나므로 확대가 *직접* 제거하진 않음(자가복구가
> 근본 차단). 단 좁은 한계가 **최초 트리거**일 가능성이 높아, L×2~4로 안정화되면 보스/뱀 `ragdollDef`의
> 한계를 Unity 익스포트에서 넓히는 것이 정공법.

### 3차: 진짜 원인 — degenerate inertia 박스 (boss 고유) (2026-06-21)
snake는 안정화됐으나 boss는 limitScale ×1/×4 **둘 다 NaN**(허용각 무관 확정). UNSTABLE 로그 특징: maxSpeed=0인데
**전신이 한 스텝에 동시 NaN**, 시작점은 분기 허브 spine_01/spine_03, NaN 바디의 angViol=0. → 점진 발산이 아니라
**수치 특이점이 조인트 그래프로 1-sweep 전파**. 원인: `computeBoxInertia`는 half-extent 2개가 ~0인 박스(stick)에서
**한 축 관성=0** → `setInertia`의 역행렬이 **Inf invInertia** → 각임펄스에서 `0*Inf=NaN` → spine 박스 하나가 전
래그돌을 즉시 오염. snake엔 그런 박스가 없어 무사.
- (degenerate inertia 가드는 예방용으로 유지하나, 보스 로그상 degenerate 박스는 없었음 → 원인 아님.)

### 4차: 진짜 근본 원인 — 깊은 self-contact Baumgarte 폭발 (해결) (2026-06-21)
단계별 NaN 검출로 확정: **NaN은 `velocityIters`에서 태어남**(spine_01), **activeContacts=9**, prepare/조인트는 깨끗
(stretch 작음). 즉 조인트가 아니라 **동적-동적 self-contact 솔버**가 원천. 원인: `contactConstraint.cpp prepare`의
`c.bias = baumgarteBeta * invDt * penetration`에서 **침투깊이 무제한**. 보스 4× 스케일 → 사지 박스가 몸통에 수 m
깊이로 겹침 → `invDt × penetration`이 거대한 분리속도 → Inf → 속도반복 중 NaN(→조인트로 전파). integrate의 속도
클램프는 step *후*라 mid-solve를 못 막음.
- **수정**: `contactConstraint.cpp`에서 보정 침투깊이를 `kMaxCorrectionDepth=0.2m`로 상한
  (`penetration = min(depth-slop, 0.2)`). 깊은 침투는 폭발 대신 여러 스텝에 걸쳐 점진 해소. staticDepenetration의
  kMaxCorrect=0.2와 동일 철학. 전 접촉(지형 포함) 공통 견고성 향상. (서버는 래그돌 없어 무관.)
> 이로써 boss 래그돌 NaN 해결. 함께 도입된 진단(`[PhysNaN]` 단계 검출, K/J/L 토글)은 추후 디버깅용으로 유지.

## 향후
- 보스 BehaviorTree AI(공격 페이즈/Rage 전이 등)는 별도 작업. `Boss_Rage` 트리거는 그때 연결.

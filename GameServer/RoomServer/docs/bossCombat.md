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
        +- BossSkillBusyGuard                  // npcSkillActive면 Running 유지(시전 중 선점 방지)
        +- Cooldown(6s):[dist∈(atk,gap]] -> Smite(3)      // 갭 클로저
        +- Cooldown(5s):[dist<=atk]      -> Combo(1)       // 강타
        +- Cooldown(7s):[dist<=atk]      -> BackAttack(2)  // 변형타
        +- Cooldown(2.5s):[dist<=atk]    -> Swings(0)      // 경타 필러
        +- BossChaseAction                                 // 폴백 추격
   +- BossIdleAction                                       // 타깃 없음(드묾)
  ```
- **공격 리프**: `BossSkillAttackAction(idx)`가 타깃 조준 + `switchClip` + `skillStartInternal`
  (`damageScale_`)로 1회 시전 후 `Success`(`BtCooldown`이 시전 시점부터 카운트). 이후 `BossSkillBusyGuard`가
  스킬 종료까지 트리를 Running으로 잡아 다른 공격이 끼어들지 못하게 한다(스킬 lua 타임라인이
  windup/hit/recover를 담당하므로 멀티페이즈 노드 불필요).
- **추격 gait**: `BossChaseAction`이 거리로 걷기/질주를 고른다 —
  `updateChaseGait(dist)`가 `RUN_ENTER_DISTANCE(6.0)` 초과에서 질주로 래치하고
  `RUN_EXIT_DISTANCE(4.5)` 미만에서 걷기로 복귀(히스테리시스, 경계 플랩 방지).
  질주 속도 = `moveSpeed(3.5) × RUN_SPEED_MULT(2.5)` = 8.75 m/s.
  **속도와 클립을 반드시 같이 바꾼다** — 클라는 state를 안 받고 velocity로 gait를 추론하므로
  `switchClip("Run")`만 해서는 클라가 계속 걷는 모션을 섞는다(종전 버그). 클립 전환은
  서버 본(피격 BVH)용, 클라 gait를 바꾸는 건 브로드캐스트되는 속도다.
- **빌드 시점**: `Room::setupFinalBoss`가 `addAttack` 4종 등록 **직후** `buildBehaviorTree()` 호출
  (리프가 인덱스 0~3으로 skillId/clipKey 참조).
- **튜닝 포인트**: 거리 밴드(`gapRange_`), 추격 gait(`RUN_SPEED_MULT`/`RUN_ENTER_DISTANCE`/`RUN_EXIT_DISTANCE`),
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

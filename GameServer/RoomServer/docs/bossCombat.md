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
- 보스 BehaviorTree AI(공격 페이즈/Rage 전이 등)는 별도 작업. `Boss_Rage` 트리거는 그때 연결.

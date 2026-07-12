# Tactical NPC Variants & Mid-Boss Diversification

미드보스 인카운터(Hobgoblin / Grandbaum / Isys)가 고블린 외의 몬스터를 외형 그대로 배치할 수 있도록 한
변경 사항을 기록한다. Zone 트리거·인카운터·MidBoss 전술은 이전에 이미 구현되어 있었고, 본 변경은
"모든 tactical NPC가 goblin placeholder로 보이던" 문제를 해소한다.

## 핵심: per-NPC ObjectType

`TacticalNpc`에 `ObjectType objType_`(+ `objType()`/`setObjType()`)를 추가했다. 인카운터가 NPC를 스폰할 때
부대별로 이 값을 지정하고, spawn 통지(`broadcastEncounterSpawn` / `broadcastTacticalNpcSpawn`)가
`o.objType()`를 `ObjectInfo.type`으로 실어 보낸다 → 클라가 NPC별로 올바른 모델을 렌더한다.

이후 어떤 인카운터에든 임의 몬스터를 배치하려면 `setObjType` + 적절한 config만 주면 된다(일반화 완료).

## 몬스터별 Tactical config 클래스

`TacticalGoblin` 패턴을 따라 `TacticalSlime/Snake/Birdy/Bomber/Treant`(.hpp/.cpp)를 추가했다. 각 클래스는
`static TacticalNpcConfig trooperConfig()` + `static constexpr ObjectType objType()`를 제공(스탯 단일 출처).
기존 인카운터의 인라인 config(slimeCfg/snakeCfg/buddyCfg/bomberCfg)를 이 클래스로 이전했다.
주의: Isys 부대의 "Buddy"는 Birdy로 렌더(Isys=Birdy 변종).

## 공용 바디 등록 헬퍼

`Room::registerTacticalNpcBody(TacticalNpc&, ObjectType)`가 type별로 모델/애니셋/클립이름을 선택한다(3개 인카운터에
중복돼 있던 registerBody 람다를 통합). 보스 변종은 같은 리그를 공유: Hobgoblin→Goblin 애니, Grandbaum→Treant
애니, Isys→Birdy 애니. Idle/Walk/Die를 정확한 이름으로 등록해 본-부착 피격 BVH가 null 클립으로 동결되는 것을
방지한다. 공격 클립·스킬은 아래 "확률적 다중공격"에서 로스터로 일괄 등록한다.

## 확률적 다중공격 (스킬 시스템 전환, 2026-06-21)

이전엔 모든 TacticalNpc(부대원·보스)가 레거시 단일 `setHp`/`applyHitToSession`으로 **항상 같은 공격**만 했다
(일반 `Npc`는 이미 스킬 기반 확률 다중공격 보유). 본 변경으로 TacticalNpc도 일반 Npc와 동일하게 **스킬 기반
확률 공격**을 하도록 전환했다.

- **TacticalNpc 공격 메커니즘 복제**(`TacticalNpc.hpp/.cpp`): `TacticalNpcAttack{skillId, clipKey}` +
  `attacks_`/`addAttack`/`pickAttack`(균등 랜덤, thread_local mt19937)/`hasSkillAttacks` + `attackCast_`
  (windup당 1회 시전, `transitionTo(AttackWindup)`에서 리셋). `updateAttackWindup`이 윈드업 시작 시
  `pickAttack`→`switchClip(clipKey)`→`room.skillStartInternal(id, skillId, seed, attackDamageScale_)`로 시전
  (히트박스가 권위적 데미지, 레거시 setHp 제거). `updateAttackRecover`는 `npcSkillActive`인 동안 recover 유지
  (늦은 히트박스 본 정확). 스킬 미등록(`attacks_` 비면) 시 레거시 setHp 폴백 유지. (사용자 결정: 공용 헬퍼
  추출 대신 per-class 복제.)
- **공격 로스터 데이터**(`Room.cpp` 익명 namespace): `AttackDef{skillName, clipSrc, clipKey}` +
  `attackRosterFor(ObjectType)` 정적 테이블(Goblin 3 / Mushroom·Birdy 2 / Snake·Bomber·Slime 1 / Treant 3).
  `attackBaseType`가 보스→기본 몬스터 매핑(Hobgoblin→Goblin, Grandbaum→Treant, Isys→Birdy). `setupX`(일반 Npc)와
  내용 동일(향후 통합 가능). `registerTacticalNpcBody`가 이 로스터로 공격 클립 등록 + `addAttack`.
- **보스 스킬 전환**(`PlatoonLeader` + 3 MidBossTactic): 보스는 `TacticalNpc::updateAttackWindup`가 아니라
  `IMidBossTactic::update`로 공격하므로 별도 배선. `PlatoonLeader::castSkillAttack(room)`(pickAttack→switchClip→
  skillStartInternal, 미등록 시 false) 추가. `GoblinMidBossTactic`/`IsysMidBossTactic`의 `updateBossPersonalCombat`,
  `GrandbaumMidBossTactic`의 `updateBossMelee`에서 윈드업 종료 시 `applyHitToSession` 대신
  `if(!leader.castSkillAttack(room)) applyHitToSession(...)`.
- **캐스터별 데미지 배율**(스킬 시스템): `SkillInstance.damageScale`(기본 1.0) + `startSkill(... seed, damageScale)`
  오버로드 + `Room::skillStartInternal(..., damageScale)`. 데미지 = `oh.damage * damageCoeff * damageScale`
  (`RoomServer/skill/skillSystem.cpp`). `TacticalNpcConfig.attackDamageScale`(트루퍼 1.0, 보스 3.0)로 보스가 기본
  몬스터 스킬을 재사용하면서도 강한 데미지(레거시 boss 40 / trooper 12 ≈ 3.3x 근사). 일반 Npc 호출은 기본값 1.0.
- **클라 무변경**: tactical NPC도 동일 `createX`로 스폰돼 `skillObjectById_`/`idMonsterMap_`에 등록되므로
  `onSkillStart(ownerId)`가 id로 해석해 스킬 VFX + AnimBlender 공격 클립을 자동 렌더.
- **한계**: Snake/Bomber/Slime은 공격 lua가 1개뿐이라 다양화 불가(콘텐츠 추가 필요). 보스는 전용 스킬 없이 기본
  몬스터 로스터 재사용(전용 보스 스킬은 lua 저작 + objType별 로스터 분기 필요).

## 처치 시 스킬 차지 지급 (필드 몬스터 패리티)

이전엔 tactical NPC(부대원·보스) 처치 시 플레이어가 스킬 차지를 못 얻었다 — 일반 몬스터는
`setupX`에서 `setKillChargeReward`를 호출하지만 tactical 경로(`registerTacticalNpcBody`)가
이 값을 설정하지 않아 `killChargeReward_`가 0이었고, `Room::noteAndMaybeReward`가 `<=0`이면
조기 반환하기 때문이다. `registerTacticalNpcBody`에 `obj.setKillChargeReward(chargeConfig().monsterCharge(type))`
한 줄을 추가해 트루퍼·보스(PlatoonLeader도 TacticalNpc 파생)를 한 경로로 일괄 커버한다. 차지는
objType 기준이라 변종별로 해당 몬스터 값을 쓴다. 보스 objType(Hobgoblin/Grandbaum/Isys)은
`chargeConfig.lua`의 `monsters`에 항목을 추가(트루퍼 10 대비 50, 재빌드 없이 튜닝). 분배는
기존 플레이어 스킬 히트 경로(`EvSkillHit → noteAndMaybeReward → distributeKillCharge`)를 그대로
재사용하므로 별도 배선 없음(플레이어 데미저만 적립, HP 0 전이 1회만 분배).

## 전용 보스 모델 (Grandbaum / Isys)

- 서버: `assetManager_->modelGrandbaum()`(GrandbaumServer.bin) / `modelIsys()`(IsysServer.bin) 사용,
  `platoonLeaderObjType_ = ObjectType::Grandbaum/Isys`.
- 클라: `AssetManager`에 `modelGrandbaum_`(treant/Grandbaum.bin) / `modelIsys_`(birdy/Isys.bin) 로드.
  `Grandbaum : public Treant`, `Isys : public Birdy` 서브클래스가 `setAnimBlender`만 오버라이드(전용 모델 +
  베이스 애니 재사용). `createGrandbaum/createIsys`는 corpse/래그돌/에너지오브 정합을 위해
  `configureNetMonster(..., MonsterKind::Treant/Birdy, ...)`로 라우팅(newMonsters_ corpse 파이프라인이 처리).
  `PacketManager`의 ObjectType→create switch 두 곳에 Grandbaum/Isys 케이스 추가.

## 디버그 텔레포트 (F5/F6/F7)

서버 `move()`가 7m/패킷 클램프를 적용하므로 단순 setPos로는 원거리 zone 트리거가 안 된다. 전용 패킷
`C_DebugTeleport`(`CDebugTeleportPacket{ pos }`)를 추가했다. 서버 `Room::debugTeleport`가 클램프 없이
권위 위치를 옮기고 S_Move로 전파 → 다음 `zoneSystem_.update`에서 해당 아레나 Enter 발동.
클라 `processInputGame`의 F5→Arena_Hobgoblin, F6→Arena_Grandbaum, F7→Arena_Isys. 각 키가
`findZoneCenter`(chunks_index.bin의 zone volume center)로 좌표를 찾아 로컬 예측 setPos + 패킷 송신.

## 알려진 제한

- 보스는 일회성이라 corpse 풀 respawn 시 plain Treant/Birdy로 재생성될 수 있다(보스 재생성은 비대상).
- `C_DebugTeleport`는 디버그 전용(인증/권한 검사 없음). 릴리스 빌드에서 비활성화 고려.

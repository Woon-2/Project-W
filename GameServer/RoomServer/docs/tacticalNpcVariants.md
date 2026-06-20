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

`Room::registerTacticalNpcBody(Object&, ObjectType)`가 type별로 모델/애니셋/클립이름을 선택한다(3개 인카운터에
중복돼 있던 registerBody 람다를 통합). 보스 변종은 같은 리그를 공유: Hobgoblin→Goblin 애니, Grandbaum→Treant
애니, Isys→Birdy 애니. 라이브 클립은 "Idle" 고정(전술 NPC는 switchClip 미사용, 클라가 속도로 모션 추론)이나
Idle/Walk/Die/Attack을 정확한 이름으로 등록해 본-부착 피격 BVH가 null 클립으로 동결되는 것을 방지한다.

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

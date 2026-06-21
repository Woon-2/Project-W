# 스택형 스킬 충전 시스템 (Stack-Charge Skill System)

무기별 기본 공격 + 3 스킬을, **몬스터 처치로 모은 charge**로 사용하는 시스템.
GUI 시안: `docs/skill_hud_mockup/radial_dial.html` (120° 회전 다이얼 + 글로우 재충전).

> 이 문서는 설계/구현 기준 문서다. 밸런스 수치는 placeholder이며 `resources/data/chargeConfig.lua`
> 와 `resources/skills/*.lua`에서 재빌드 없이 조정한다.

## 1. 핵심 규칙

- **귀속**: 몬스터가 죽을 때, *최근 `damageWindowMs`(기본 15s) 내에 그 몬스터에게 데미지를 준* 플레이어
  전원이 charge를 얻는다. 기본 공격·스킬 모두 데미지로 인정(스킬킬도 충전).
- **선택 스킬만 충전**: charge는 그 플레이어의 **현재 선택 슬롯(0..2)** 에만 들어간다. 슬롯별 독립 누적.
- **2중 게이트**: 사용하려면 `스택 ≥ 1` **and** `쿨다운 경과`. 스택 = `floor(charge / chargeCost)`(상한 없음,
  소프트캡으로 감속). 쿨다운 ≈ 스킬 지속시간 + α.
- **콤보 → HP 회복 가속**: `comboWindowMs` 안에 연속 처치 시 콤보가 누적된다. 콤보는 더 이상 charge 획득량을
  가속하지 않고(charge는 `reward × softCap`만 적용), 대신 **플레이어의 초당 HP 회복 속도**를 끌어올린다.
  회복 속도는 S자(Hill) 커브 `regen(combo) = base + (cap−base)·xⁿ/(xⁿ + halfComboⁿ)`(기본 1/s, 점근선 25/s,
  높은 문턱: `halfCombo=10, exponent=3` → 콤보 ~7까지 미미하다가 콤보10에서 중간 13/s, 고콤보에서 25 수렴).
  회복은 **완전 서버 책임**: `Room::updatePlayerRegen`이 60fps로 적분(`hpRegenAccum_` carry, `kPlayerMaxHp` 상한),
  변경분만 ~10Hz `S_PlayerHp`로 브로드캐스트. 클라는 예측 없이 `setHp`만 반영(피격 애니/혈흔 없음).
- **권위**: charge·쿨다운·콤보·HP는 **서버 권위**. 클라는 동기화된 사본으로 즉시 시전(예측)하고 서버가
  재검증한다. 드물게 거부되면 `S_SkillUseReject`로 로컬 시전을 롤백한다(charge 값 자체는 예측하지 않음).
- **사망/리스폰**: 현재 리스폰 없음(해당 없음). 추후 리스폰 시 50% 소실 예정.

## 2. 데이터 / Lua

### 스킬 메타데이터 — `resources/skills/*.lua` (스킬 시스템 스키마 확장)
```lua
skill.weapon     = "sword"   -- sword|bow|wand|spear  (PlayerWeaponType 매핑)
skill.isBasic    = true      -- 좌클릭 기본 공격: 코스트/쿨 게이트 면제
skill.dialSlot   = 0         -- 0..2 다이얼 슬롯(스킬에만)
skill.chargeCost = 3         -- 1회 시전 charge
skill.cooldownMs = 1400      -- 시전 쿨다운
```
컴파일러(`client|RoomServer/skill/skillCompiler.cpp`)가 `SkillAsset`(`skillTypes.hpp`)의
`weaponType/loadoutSlot/isBasic/chargeCost/cooldown`로 채운다. 컴파일 후
`SkillLoadout::build()`(`skill/skillLoadout.hpp`, 클라/서버 각 1)가 무기→{기본, 3슬롯 assetId/코스트/쿨}을 만든다.

무기별 로드아웃: 검(sword_slash + slash_wave/slash_7/slash_combo), 활(arrow + arrow_volley/arrow_rain/
energy_explosion_arrow), 완드(spikes + crystals_front_attack/crystals_cross_fade/red_energy_explosion),
창(piercing + piercing_slash/piercing_circle_slash/piercing_multi).

### 충전 경제 튜닝 — `resources/data/chargeConfig.lua` (스킬과 별개)
`monsters`(ObjectType→charge), `damageWindowMs`, `combo{windowMs}`, `regen{basePerSec,capPerSec,halfCombo,exponent}`,
`softCap{startStacks,decay}`. `RoomServer/chargeConfig.{hpp,cpp}`(`ChargeConfig`)가 부팅 시 sol2로 로드,
`AssetManager`가 전 룸 공유. `comboMult`/`maxMult`/`mult[]`는 콤보가 charge 가속에서 HP 회복으로 전환되며 제거됨.

## 3. 서버 (RoomServer)

- 상태(`object.hpp`): `Player`에 `selectedSlot_/skillCharge_[3]/cooldownEnd_[3]/comboCount_/lastCreditMs_`
  + HP 회복용 `hpRegenAccum_`(분수 HP carry)/`lastSyncedHp_`(전송 변경 감지). `Object`에 `killChargeReward_`
  (스폰 시 `setupGoblin`에서 ChargeConfig로 설정) + 데미저 로그(`noteDamager`/`collectRecentDamagers`, 파티 크기만큼만 유지).
- 귀속/분배(`Room.cpp`): 모든 플레이어→몬스터 데미지를 `noteAndMaybeReward()`로 경유(주 경로 = `updateSkillSystem`
  의 `EvSkillHit` 루프, 보조 = `attack()`/C_Attack). HP 0 전이 시 `distributeKillCharge()` → 최근 데미저
  전원의 선택 슬롯에 `reward × softCapFactor(stacks)` 가산(콤보 가속 제거) → `S_SkillCharge`(전원) +
  `S_ComboState`(해당) 송신. 콤보 만료는 `updateComboExpiry()`(매 틱).
- HP 회복(`Room.cpp`): `updatePlayerRegen()`이 60fps 틱마다 살아있는 플레이어의 콤보로 회복 속도를 구해 적분
  (`hpRegenAccum_`, `hp<kPlayerMaxHp`만), 변경된 HP만 ~10Hz로 `S_PlayerHp` 브로드캐스트(`regenSyncAccum_` throttle).
- 선택/사용(`Room.cpp`): `selectSkill()`(C_SelectSkill, `S_SkillSelect` 중계). `skillStart()`에 게이트 추가 —
  자산이 `isBasic`이면 통과, 아니면 슬롯·무기·스택·쿨 검사 후 소모+쿨 설정+`S_SkillCharge`, 실패 시
  `S_SkillUseReject`.

## 4. 프로토콜 (`ServerEngine/protocol.hpp`)

신규 타입/구조체: `C_SelectSkill{slot}`, `S_SkillSelect{playerId,slot}`, `S_SkillCharge{playerId,slot,charge}`,
`S_SkillUseReject{slot}`, `S_ComboState{playerId,comboCount,windowMs}`, `S_PlayerHp{playerId,newHp}`(서버 권위 회복
HP 푸시 — 클라는 이벤트·애니 없이 `setHp`만). 사용 요청은 기존 `C_SkillStart` 재사용.

## 5. 클라이언트 (client)

- HUD(`ui/skillDialHUD.{hpp,cpp}`): 우하단 **소형** 120° 회전 휠(선택은 꼭대기 중앙). 3 아이콘은 **반투명
  회색 도넛**(effectMode 3, 절차적·텍스처 미샘플, `cRoughness`=안쪽 구멍 반지름) 위에 120° 간격으로 놓인다.
  크기/도넛 상수는 `.cpp` 상단(`kRadius/kSelSize/kSideSize/kRingPad/kRingHole`). 충전은 effect-mode로
  **아래서부터 일렁이는 액체**(`ui.hlsl` PS, `Material.cRoughness`=fill / `cMetallic`=mode / `PerFrameData.time`).
  mode 1=충전(어두운 base+밝은 fill), 2=준비(밝은 base+다음 스택 글로우 tide). 스택 ×N(≥2) 배지는 DigitAtlas.
  0→1 전이 시 scale pop + `skill_ready` 사운드.
- z-order: 다이얼+콤보는 `renderInGame`에서 `uiManager_.render` **이전**에 제출 → 설정 패널(`settingsPanel_`,
  uiManager 오버레이)이 항상 다이얼 위에 그려진다(UI는 제출 순서 = 그리기 순서).
- 아이콘(`AssetManager`): 12 스킬 아이콘 명시 멤버 + `skillIconByAssetName()`. 무기 아이콘은 기존 멤버.
- 입력(`onlineGame.cpp processInputGame`/`receiveWndMsg`): 휠 = 선택(다이얼 회전 + `C_SelectSkill`),
  휠클릭 = 선택 스킬 사용(자체 게이트 후 `castSkillByName` + 예측 쿨), 좌클릭 = 기본 공격(스킬 시전).
- 수신(`onlineGame.cpp`): `onSkillCharge/onSkillSelect/onSkillUseReject/onComboState/onPlayerHp`. 콤보 수는 다이얼
  위에 DigitAtlas로 표시(윈도우 잔량에 따라 크기 감쇠). `onPlayerHp`는 `idPlayerMap_` 대상에 `setHp`만 적용
  (이벤트·애니 없음) → 기존 매 프레임 HP 바/텍스트·파티 HUD 읽기로 회복이 자동 반영.

## 6. 변경 파일

- 서버: `ServerEngine/protocol.hpp`, `RoomServer/{chargeConfig.*, object.hpp, Room.*, PacketManager.*,
  AssetManager.*, skill/skillTypes.hpp, skill/skillCompiler.cpp, skill/skillLoadout.hpp}`
- 클라: `client/{ui.hlsl, uiPipeline.*, shader.hpp, AssetManager.*, PacketManager.*, sound/soundCatalog.cpp,
  ui/skillDialHUD.*, skill/{skillTypes.hpp, skillCompiler.cpp, skillLoadout.hpp, skillSystem.hpp},
  online/onlineGame.*}`
- 데이터: `resources/skills/*.lua`(16개), `resources/data/chargeConfig.lua`

## 7. 확장 방법 (How to extend)

대부분의 콘텐츠/밸런스 변경은 **재빌드 없이 lua만** 수정하면 된다.

- **기존 무기에 스킬 교체/추가**: 새 `resources/skills/<name>.lua`를 만들고 `weapon`+`dialSlot`(0..2)+
  `chargeCost`+`cooldownMs` 지정. 같은 무기·슬롯의 기존 스킬은 슬롯을 비우거나 옮긴다. 아이콘은
  `resources/UI/<name>.dds` 추가 후 클라 `AssetManager::skillIconByAssetName()`에 `이름→텍스처` 한 줄
  추가(스킬 자산 이름 기준). 코스트/쿨은 lua만으로 반영.
- **새 몬스터에 charge 부여**: `chargeConfig.lua`의 `monsters`에 `ObjectType` 항목 추가. 스폰 코드에서
  `obj.setKillChargeReward(chargeConfig.monsterCharge(type))` 설정(보스 등은 스폰 시 개별 값 override 가능).
  `ObjectType` 이름 매핑은 `RoomServer/chargeConfig.cpp::parseObjectType`.
- **HP 회복 커브 변경**: `chargeConfig.lua`의 `regen.basePerSec`(콤보 0 회복)·`capPerSec`(점근선)·
  `halfCombo`(중간점 콤보)·`exponent`(가파름, 클수록 문턱↑). 커브 형태 자체를 바꾸려면 `ChargeConfig::hpRegenPerSec`만
  수정(호출부 불변). 콤보 윈도우는 `combo.windowMs`.
- **소프트캡 커브 변경**: `chargeConfig.lua`의 `softCap.startStacks`·`decay`. 형태 변경은 `softCapFactor`만 수정.
- **새 무기 추가**: `PlayerWeaponType`(protocol.hpp) enum + 무기 아이콘 + 해당 무기의 기본/3슬롯 스킬 lua
  (`weapon="..."`). `SkillLoadout`/다이얼/게이트는 자동으로 따라온다(무기 ordinal 0..3 가정 주의 —
  4개 초과 시 `byWeapon` 크기·`forWeapon` 범위 확장 필요).
- **새 동기화 필드/패킷**: `ServerEngine/protocol.hpp`에 추가 → 서버 `PacketManager::makeS*`/디스패치,
  클라 `PacketManager::handleS*`+`onlineGame::on*` 양쪽 배선. 충전 값은 서버 확정만 보낸다는 원칙 유지.

## 8. 남은 폴리시 (plumbing 완료, 마감 필요)

- **파티원 HP HUD 스택 표기**: 팀원 charge/선택은 클라에 이미 동기화됨(`teammateCharge_`/`teammateSelected_`).
  기존 파티 HP HUD(`createOtherPlayerHud`/`updatePartyHpHudValues`)에 선택 스킬 스택 소형 표기 추가만 남음.
- **콤보 윈도우 잔량 바**: 현재 콤보 수만 표시. 바 비주얼 추가 가능(솔리드 텍스처 필요).
- **사운드 파일**: `resources/audio/sfx/skill_ready.wav` 미존재 시 무해 경고. 파일만 추가하면 동작.
- **밸런스 수치**: 모든 코스트/쿨/콤보/회복/소프트캡은 placeholder.

# 스킬 제작 가이드 (Lua API + 유형별 레시피)

스킬은 `resources/skills/*.lua` 파일로 정의하고, 부팅 시 `SkillCompiler`(클라) / `ServerSkillCompiler`(서버)가
`SkillAsset`으로 컴파일한다. 이 문서는 ① Lua API 레퍼런스와 ② 대표 스킬 유형별 제작 레시피를 제공한다.

> 구조/런타임 동작은 `skillArchitecture.md` 참조. 이 문서는 **스킬을 만드는 작성자 관점**의 가이드다.

---

## 1. 기본 골격

```lua
-- resources/skills/my_skill.lua
local skill = Skill()
skill.name            = "MySkill"   -- 식별용 이름
skill.totalDurationMs = 600         -- 스킬 총 길이(ms). 끝나면 인스턴스 종료
skill.interruptible   = true        -- 다른 동작으로 캔슬 가능 여부

skill:addVFX(1, "effects/my_effect.json")   -- vfxId 레지스트리에 이펙트 경로 등록

skill:addEvent(0,   "PlayAnimation", { clipName = "Player_Attack", blendTime = 0.1 })
skill:addEvent(120, "PlayVFX",       { vfxId = 1, offset = Vec3(0, 1, 0.8) })
skill:addEvent(140, "SpawnHitbox",   { slot = 0, localOBBs = { OBB(...) }, attach = BoneAttach("spine_02"), onHit = OnHit{...} })
skill:addEvent(400, "DestroyHitbox", { slot = 0 })

return skill   -- 반드시 skill 테이블을 반환
```

- `skill:addEvent(timeMs, type, params)` — 타임라인에 이벤트 추가. `timeMs`는 스킬 시작 기준 발동 시각.
  내부적으로 시간순 정렬되므로 작성 순서는 자유.
- `skill:addVFX(id, path)` — `id`를 PlayVFX/OnHit의 `vfxId`로 참조한다.

### 스택형 충전 메타데이터 (선택, 무기 로드아웃용)

스킬을 무기 다이얼(우하단 HUD)의 한 슬롯이나 기본 공격으로 편입하려면 아래 필드를 추가한다.
지정하지 않으면 로드아웃 외 스킬(에디터/디버그용)로 취급된다. 시스템 전체 설계는
`skillChargeSystem.md` 참조.

```lua
skill.weapon     = "sword"   -- sword|bow|wand|spear (PlayerWeaponType 매핑). 무기에 편입할 때 필수
skill.isBasic    = true      -- 좌클릭 기본 공격. charge/쿨다운 게이트 면제(코스트 무시)
-- 또는 다이얼 슬롯 스킬:
skill.dialSlot   = 0         -- 0..2. 휠로 선택되는 다이얼 슬롯
skill.chargeCost = 3         -- 1회 시전에 필요한 charge(스택 1칸). 정수 권장
skill.cooldownMs = 1400      -- 시전 쿨다운. 권장: totalDurationMs보다 살짝 길게
```

- 한 무기당 `isBasic` 1개 + `dialSlot` 0/1/2 각각 1개를 채운다(`skill/skillLoadout.hpp`가 자동 수집).
- 아이콘은 `resources/UI/<스킬파일명>.dds`를 두고, 클라 `AssetManager::skillIconByAssetName()`의
  이름→텍스처 매핑에 한 줄 추가하면 다이얼에 표시된다(스킬 자산 이름 기준, 예: `"SlashWave"`).
- 무기·슬롯·코스트·쿨다운은 **재빌드 없이** lua만 고치면 반영된다(아이콘 추가만 C++ 한 줄).

### vfxId 바인딩 (필수 선결 조건)

`vfxId`는 런타임에 실제 `ParticleEffect`로 매핑되어야 한다. 이 매핑은 lua가 아니라 C++에서 한다:
`standalone/game.cpp`와 `online/onlineGame.cpp`의 `skillVfxById_[vfxId] = &someEffect_` (현재 1~18 사용).
**새 이펙트를 쓰려면** 해당 `ParticleEffect`를 빌드하고 `skillVfxById_`에 바인딩해야 PlayVFX가 동작한다.
바인딩되지 않은 `vfxId`는 조용히 무시된다(no-op). 서버는 VFX를 항상 무시한다.

---

## 2. 좌표계 규약 (중요)

모든 로컬 공간은 **right = +X, up = +Y, forward = +Z**.

| 부착 대상 | 기준 |
|-----------|------|
| `BoneAttach("bone")` | 해당 뼈의 월드 변환(애니메이션 추종). 스킬 대부분이 `spine_02` 사용 |
| `BoneAttach("")` / attach 생략 (PlayVFX) | 시전자 루트(캐릭터 위치+방향) |
| `VFXParticleAttach(vfxId, sysIdx)` (히트박스) | 해당 이펙트의 파티클들 — 파티클마다 히트박스 1개 |
| `GroundAttach{}` (히트박스) | **시전자 상대 지점을 지면에 스냅해 정적 고정**(시전자 추종 X). 지면 AoE·솟구치는 기둥용. center.x/z=시전자 전방/우측 오프셋, center.y=표면 위 높이. 기본은 OBB별 독립 스냅(분산 융기). `anchor=N`이면 `SetGroundAnchor`로 등록한 앵커 프레임에 강체 배치(점 충돌). `align=true`면 지면 노멀로 기울임(분산 모드) |

- **OBB 회전(`orient`)** 과 **PlayVFX `orient`** 는 모두 `{yaw, pitch, roll}` (도) 동일 규약.
- 뼈에 붙은 박스는 `applyAttachRotation`으로 뼈 회전 추종 여부를 정한다:
  - `true`  — 뼈 방향을 따라감(전방 오프셋이 시전 방향을 따라감). 검격·전방 AoE에 적합.
  - `false` — 위치만 추종, 회전 무시(월드 축 정렬 유지). 흔들림 없는 지면 AoE에 유용.

---

## 3. API 레퍼런스

### 3.1 헬퍼 (skill_api.lua)

| 헬퍼 | 의미 |
|------|------|
| `Vec3(x, y, z)` | 벡터 |
| `OBB(cx, cy, cz, hx, hy, hz, yaw, pitch, roll)` | 박스: 중심(c), 반치수(h, 반지름), 회전(도, 선택) |
| `BoneAttach(name)` | 뼈 부착 대상 |
| `VFXParticleAttach(vfxId, systemIdx)` | 이펙트의 파티클 시스템 부착(파티클별 히트박스) |
| `OnHit{ damage, vfxId, impulseStrength, impulseDir }` | 피격 응답 |
| `deepCopy(t)` | 테이블 깊은 복사(여러 OnHit 변형을 만들 때) |

### 3.2 이벤트 타입별 파라미터

#### PlayAnimation
| 키 | 기본 | 설명 |
|----|------|------|
| `clipName` | "" | 애니메이션 클립 이름 |
| `blendTime` | 0.1 | 블렌드 시간(초) |

#### PlayVFX — 이펙트 배치·방향·진행 제어
| 키 | 기본 | 설명 |
|----|------|------|
| `vfxId` | 0 | 재생할 이펙트(레지스트리 인덱스) |
| `offset` | Vec3(0,0,0) | attach-local 배치. **`z` = 전방 N미터** |
| `orient` | {0,0,0} | attach-local 방향 오프셋 {yaw,pitch,roll}. 부채꼴 등을 옆으로 조준 |
| `advance` | (없음) | attach-local 파티클 진행 방향. 생략 시 orient에서 유도 |
| `groundLock` | false | 지면 평면 배치(시전자 pitch/roll 무시) — 바닥 원형/부채꼴 AoE에 사용 |
| `groundSnap` | false | **이펙트를 지면 표면에 스냅**(`offset.y`=표면 위 lift). 화살비 드롭존·낙하 마법구 착탄점 등 |
| `groundAlign` | false | 지면 노멀로 정렬(yaw 보존). `groundSnap`과 함께 사용 |
| `particleCollision` | (없음) | 이펙트 **파티클**의 지면 충돌: `"GroundStop"`/`"GroundKill"`/`"GroundBounce"`. GroundKill은 착탄 시 Death 서브이미터로 임팩트 버스트(JSON 무수정, sub-emitter 제외 top-level에만 적용) |
| `particleConform` | (없음) | 이펙트 **파티클**의 스폰 지면 컨폼: `"SnapY"`/`"SnapAndAlign"`(면적 emitter가 슬로프 따라 안착) |
| `attach` | 루트 | `BoneAttach(name)`; 생략 시 시전자 루트 |

> **형상 크기(원형 반지름 M, 부채꼴 반지름 R·각도 A)는 PlayVFX가 아니라 이펙트 프리팹 `.json`의
> shape config에 작성한다** (`shapeType`: Cone/Circle/Sphere…, `radius`, `angle`, `arc`).
> PlayVFX는 **배치·방향·진행만** 제어한다. 즉 같은 형상을 여러 스킬이 위치/방향만 달리해 공유한다.

> **지면 연계(얼음 기둥/화살비/낙하 마법구):** PlayVFX `groundSnap` + `particleCollision`/
> `particleConform` + 히트박스 `attach={type="Ground"}`를 조합한다. 지면 정보는 **전부 lua에서**
> 구동하며 effect `.json`은 수정하지 않는다. 상세 설계·예시는 `docs/terrainInteractingSkills.md`.

#### SpawnHitbox — 피해 판정 박스 생성
| 키 | 기본 | 설명 |
|----|------|------|
| `slot` | 0 | DestroyHitbox가 참조할 식별자 |
| `localOBBs` | {} | `OBB(...)` 배열(여러 개 가능) |
| `attach` | Bone | `BoneAttach` / `VFXParticleAttach` / `{type="Ground", align=}` (지면 고정) |
| `applyAttachRotation` | true | §2 참조 |
| `hitGroup` | 0 | 같은 그룹끼리 중복 피격 제거 공유 |
| `hitGroupCooldownMs` | 0 | 0 = 대상당 1회 / >0 = N ms 후 재피격 |
| `useParticleSize` | false | VFXParticle 전용: 파티클 시각 크기로 반치수 스케일 |
| `penetrate` | true | VFXParticle 전용: false = 비관통(첫 피격 시 소스 파티클 소멸). §3.4 참조 |
| `onHit` | — | `OnHit{...}` |

#### DestroyHitbox
| 키 | 설명 |
|----|------|
| `slot` | 제거할 히트박스 슬롯 |

#### OnHit (헬퍼)
| 키 | 기본 | 설명 |
|----|------|------|
| `damage` | 0 | 피해량(서버는 BVH 부위 배율 `damageCoeff`를 곱함) |
| `vfxId` | 255 | 피격 지점 이펙트(255 = 없음) |
| `impulseStrength` | 0 | 넉백 세기 |
| `impulseDir` | Vec3(0,0,1) | 넉백 방향(공격자 로컬) |

#### ApplyImpulse / CameraShake / ModifyStat
| 타입 | 키 |
|------|----|
| `ApplyImpulse` | `strength`, `dir = Vec3` (공격자 로컬, 기본 forward) |
| `CameraShake` | `magnitude`, `durationMs` |
| `ModifyStat` | `hpDelta`, `speedMultiplier`, `durationMs` |
| `SetGroundAnchor` | `id`(0~3), `offset = Vec3(우,상,전)`, `align`(bool) — 점 충돌 앵커 프레임 등록(§3.5 (3)) |

> **참고:** `SpawnProjectile`, `SendGameplayEvent`는 타입만 존재하고 페이로드 파싱은 미구현(스텁)이다.
> 현재 "투사체"는 별도 이동 오브젝트가 아니라 **날아가는 파티클 + 그 파티클에 부착한 히트박스**로 표현한다
> (§3.4 파티클 부착, 레시피 4-2).

### 3.3 히트박스 형상에 대한 핵심 원칙

히트 판정 형상은 **OBB(박스)뿐**이다. 박스 하나로 안 되는 형상(원형·부채꼴·이동 투사체)은 두 가지로 표현한다:

- **(A) 박스 근사** — 큰 박스 하나, 또는 여러 박스를 배열. 간단하고 비용 저렴. 정적 범위에 적합.
- **(B) 파티클 부착(`VFXParticleAttach`)** — 이펙트의 파티클마다 박스를 붙여 **파티클을 따라다니게** 한다.
  파티클이 움직이면(투사체) 박스도 같이 날아가고, 파티클이 원형/콘으로 퍼지면 박스도 그 형상을 이룬다.
  **이 프로젝트의 화살 등 투사체는 파티클로 구현**되어 있으므로 투사체 판정의 1순위 방법이다(§3.4).

### 3.4 파티클 부착 히트박스 (VFXParticleAttach) — 상세

`attach = VFXParticleAttach(vfxId, systemIdx)`로 SpawnHitbox를 만들면, 런타임이 매 프레임 다음을 수행한다:

1. `vfxId` 이펙트의 `systemIdx`번째 서브 파티클 시스템을 찾는다(`ParticleEffect::system(systemIdx)`).
2. 그 시스템의 **활성 파티클 수만큼 히트박스를 생성**한다(핸들 재사용으로 증감). 파티클이 새로 방출되면
   박스가 늘고, 소멸하면 준다.
3. 각 박스의 월드 위치 = **`파티클 위치 + (회전된) 템플릿 OBB center`**. 즉 `localOBBs`는 **파티클 로컬 공간**
   기준이고, 보통 `center = 0`인 작은 박스 하나를 템플릿으로 둔다.
4. `useParticleSize = true`면 박스 반치수가 **파티클의 현재 시각 크기**(sizeBegin→sizeEnd 보간)에 비례해 커진다.
5. `applyAttachRotation`(파티클 부착 시 회전 추종):
   - `true`  — 박스가 파티클의 회전(spin, baseRotation)을 따라감. 길쭉한 투사체 박스에 적합.
   - `false` — 위치만 추종, 회전 고정.
6. `penetrate`(관통 여부, 기본 `true`):
   - `true`  — 관통. 파티클이 유지되며 다중 피격은 `hitGroupCooldownMs`로 조절.
   - `false` — 비관통. 첫 피격 시 소스 파티클이 `ParticleSystem::killParticle`로 소멸한다.
     그 파티클에 Death 서브이미터(예: 폭발)가 있으면 충돌 지점에서 자식 이펙트가 재생된다
     (`energy_explosion_arrow`: 화살=데미지 0 비관통 트리거, 폭발=데미지). 클라는 예측 소멸,
     서버는 권위적으로 소비 처리 — 상세는 `particleHitboxDeterminism.md` §8.

**전제 조건 / 주의:**
- 해당 `vfxId`가 **PlayVFX로 재생되어 파티클이 살아 있어야** 박스가 생긴다. 보통 PlayVFX → (약간 뒤) SpawnHitbox 순.
- `systemIdx`는 이펙트 안의 **서브 시스템 인덱스**다. 이펙트가 본체/트레일/스파크 등 여러 시스템으로
  구성된 경우, **투사체 본체에 해당하는 시스템**을 골라야 한다(트레일에 붙이면 판정이 꼬리에 생긴다).
- 비용은 **활성 파티클 1개당 박스 1개**다. 투사체는 본체 파티클 수를 적게(이상적으로 1~소수) 유지하라.
  원형/콘 AoE를 파티클 정합으로 만들면 파티클 수가 그대로 박스 수가 되므로 방출량에 유의.
- SpawnHitbox로 소스를 만들고 DestroyHitbox(같은 `slot`)로 정리한다. 정리 전까지 박스는 파티클을 계속 추종한다.

### 3.5 지면 연계 (Terrain interaction) — 데이터 타입·사용법

얼음 기둥 솟구침, 화살비, 낙하 마법구처럼 **지면(terrain)과 상호작용**하는 스킬은 아래 세 가지 lua 도구를
조합한다. **지면 정보는 전부 스킬 lua에서 구동**하며 effect `.json`(Unity 익스포트 아트 에셋)은 수정하지
않는다. 지형 높이/노멀 질의는 엔진이 결정론적으로 제공한다(클라/서버 동일).

#### (1) 이펙트 지면 배치 — `PlayVFX`

| 키 | 타입 | 의미 |
|----|------|------|
| `groundSnap` | bool | 이펙트 `worldPos.y`를 **지면 표면으로 스냅**. 이때 `offset.y`는 표면 위로 띄우는 높이(lift)가 된다 |
| `groundAlign` | bool | 이펙트를 **지면 노멀로 기울임**(yaw 보존). `groundSnap`과 함께 사용 |

> **`groundLock` vs `groundSnap` (혼동 주의):**
> - `groundLock` = **방향**만 평탄화(시전자 pitch/roll 무시). 위치는 그대로. 시전자 발밑 바닥 원형 등.
> - `groundSnap` = **위치 Y**를 지면으로 이동. 시전자에서 떨어진 지점(전방·상공)을 지면에 떨어뜨릴 때.
> - 둘은 독립이며 함께 쓸 수 있다(예: 전방 지면 원형 = `groundSnap` + `groundLock`).

#### (2) 이펙트 파티클의 지면 거동 — `PlayVFX`

이펙트가 방출하는 **파티클**이 지면과 어떻게 상호작용할지. 문자열 열거값.

| 키 | 값(문자열) | 의미 |
|----|-----------|------|
| `particleCollision` | `"GroundStop"` | 표면에서 정지(속도·중력 0). 쌓이는 잔해/얼음 파편 |
| | `"GroundKill"` | 표면에 닿으면 소멸. **이펙트에 Death 서브이미터가 있으면 착탄 지점에서 임팩트 버스트가 자동 발생**. 화살비·낙하물 착탄 먼지 |
| | `"GroundBounce"` | 수직 반사(기본 반발 0.3). 튀는 파편 |
| `particleConform` | `"SnapY"` | 스폰 시 각 파티클 Y를 지면으로 스냅(면적 emitter가 굴곡 지면에 안착) |
| | `"SnapAndAlign"` | 위 + 메시 파티클을 지면 노멀로 기울임(슬로프 따라 솟는 기둥) |

> - **적용 범위:**
>   - `particleConform`은 **모든 시스템(서브이미터 포함)** 에 적용된다. 얼음 기둥·잔해처럼 시각 본체가
>     Birth 서브이미터로 부모 파티클 위치에 스폰되는 경우가 많으므로(예: `crystals_front_attack`의
>     "Crystals" 서브 시스템), 자식까지 컨폼해야 각 조각이 제 XZ의 지면에 안착한다.
>   - `particleCollision`은 **top-level 시스템에만** 적용된다(서브이미터=임팩트 버스트가 착탄점에서
>     스폰되자마자 죽지 않도록 제외).
> - 세부 수치(반발 계수, 표면 오프셋)는 lua로 노출하지 않고 엔진 기본값을 쓴다(필요 시 차후 확장).
> - `SnapAndAlign`의 노멀 정렬(기울임)은 **메시 파티클**에만 보인다. Billboard/StretchedBillboard는
>   카메라를 향하므로 기울지 않지만, `SnapY`(지면 높이 스냅)는 빌보드에도 적용된다.
> - **클라 전용(시각).** 서버는 파티클이 없으므로 무시한다.
> - 비용: 충돌은 낙하 파티클당 프레임 1회 지형 질의(`vel.y<0` 게이트). 수천 개 방출 emitter는 주의.

#### (3) 지면 고정 히트박스 — `SpawnHitbox`의 `attach`

```lua
attach = { type = "Ground", align = false }
```

시전자를 따라다니지 않고 **시전 시점에 지면에 박혀 정적**으로 남는 판정 박스. 지면 AoE·솟구치는 기둥용.

- **앵커** = 시전 순간의 시전자 위치 + yaw(바라보는 방향). 시전 후 이동해도 박스는 그 자리에 남는다.
- `localOBBs`의 각 `OBB(cx, cy, cz, hx, hy, hz, ...)`:
  - `cx` = 시전자 기준 **우측(+)/좌측(−)** 오프셋(m)
  - `cz` = 시전자 기준 **전방(+)** 오프셋(m) — XZ는 시전 yaw로 회전되어 "전방 6m"가 시전 방향을 따른다
  - `cy` = **지면 표면 위 높이**(m). `0`이면 박스 중심이 표면 높이에 옴
  - `hx/hy/hz` = 반치수(반지름)
- `align = true`: OBB를 지면 노멀로 기울임(급경사에서 박스가 기우니 보통 `false`). **분산 모드에서만** 의미가 있다(점 충돌 모드는 등록 앵커의 align이 결정).
- **결정론:** 클라/서버가 같은 시전자·같은 지형에서 같은 위치를 계산하므로, 신규 패킷 없이 서버 권위
  판정과 클라 예측이 일치한다(서버도 Ground attach를 미러).

**두 배치 모드 — 분산 융기 vs 점 충돌**

| `anchor` | 동작 | 용도 |
|----------|------|------|
| 없음(기본) | **OBB마다 독립적으로** 자기 XZ에서 지면 스냅. `center.y`=각자 발밑 표면 위 높이 | 굴곡 지면에 솟는 **기둥 그리드**(분산 융기) — 각 기둥이 제 발밑 높이에 맞음 |
| `= N` | **등록된 지면 앵커 N 프레임에 강체 배치**(스냅 안 함, 앵커가 1회 스냅됨). `center`=앵커 프레임 내 오프셋 | **점 충돌**(메테오 폭발 링, 크레이터) — 여러 개의 **별도 히트박스**가 한 충돌점을 공유 |

점 충돌은 **`SetGroundAnchor` 이벤트로 앵커 프레임을 먼저 등록**하고, 각 `SpawnHitbox`가 `anchor=N`으로 참조한다.
**히트박스를 합칠 필요 없이** 박스마다 자기 `onHit`(예: 방사 방향 넉백)을 유지한 채 한 충돌점을 공유한다:

```lua
-- ① 충돌 앵커 프레임 등록(전방 5m, 지면 스냅). id 0~3
skill:addEvent(1150, "SetGroundAnchor", GroundAnchor{ id = 0, offset = Vec3(0, 0, 5.0), align = false })

-- ② 링 박스 각각이 앵커 0을 참조 — center는 앵커 프레임 기준 오프셋, 박스별 onHit 유지
for i = 0, 7 do
    local deg = i * 45.0
    local s, c = math.sin(math.rad(deg)), math.cos(math.rad(deg))
    local onHit = deepCopy(onHitBase); onHit.impulseDir = Vec3(s, 0, c)  -- 방사 넉백
    skill:addEvent(1200, "SpawnHitbox", {
        slot = i,
        localOBBs = { OBB(1.65*s, 0, 1.65*c, 1.1, 1.2, 1.1, deg, 0, 0) },  -- 앵커 기준
        attach = GroundAttach{ anchor = 0 },
        onHit = onHit,
    })
end
```

- `SetGroundAnchor` = `{ id, offset=Vec3(우,상,전), align }`. 시전자 상대 오프셋을 시전 yaw로 회전 후 지면 스냅,
  `align=true`면 프레임을 지면 노멀로 기울임. 앵커 슬롯은 0~3(`kMaxGroundAnchors`).
- **반드시 참조 히트박스보다 먼저(같은/이전 타임)** 이벤트를 둘 것. 미등록 앵커를 참조하면 분산 모드로 폴백한다.
- **이펙트와의 일치:** 점 충돌은 단일 원점 폭발 VFX(`groundSnap`, `groundAlign=false`)와 분포가 맞아떨어진다.
  VFX를 `groundAlign`하지 않으면 앵커도 `align=false`(똑바로)로 두는 것이 자연스럽다.

#### 조합 요약

| 원하는 효과 | 사용 도구 |
|-------------|-----------|
| 이펙트를 지면 위 특정 지점에 배치 | PlayVFX `groundSnap` (+ `offset.z`=전방거리, `offset.y`=lift) |
| 떨어지는 파티클이 땅에서 소멸·폭발 | PlayVFX `particleCollision="GroundKill"` + 이펙트에 Death 서브이미터 |
| 굴곡 지면에 솟는 기둥/면적 안착 | PlayVFX `particleConform="SnapAndAlign"` |
| 분산 기둥 그리드(각자 발밑 스냅) | SpawnHitbox `attach=GroundAttach{}`(기본) |
| 점 충돌 클러스터(메테오 폭발 링) | `SetGroundAnchor`로 앵커 등록 + 각 SpawnHitbox `attach=GroundAttach{anchor=N}` |

> 전체 설계·내부 동작은 `docs/terrainInteractingSkills.md` 참조.

---

## 4. 스킬 유형별 제작 레시피

각 레시피는 **개념 → 구성 → 예시 → 튜닝 팁** 순. 수치는 인게임 스킬 에디터(standalone)로 시각 조정 권장.

### 4-1. 검격 (근접 슬래시)

**개념:** 무기 궤적을 따라 짧은 시간 동안 박스가 호(arc)를 그리며 지나간다.
**구성:** `spine_02` 부착 + `applyAttachRotation=true`. 여러 박스를 시간차로 생성해 휘두름을 흉내내거나,
방향별 `onHit.impulseDir`로 넉백 방향을 다르게 준다. (`sword_slash.lua` 참고)

```lua
local skill = Skill()
skill.name = "SwordSlash"; skill.totalDurationMs = 400

skill:addVFX(1, "effects/sword_slash_1.json")
skill:addEvent(0,   "PlayAnimation", { clipName = "Player_Attack", blendTime = 0.1 })
skill:addEvent(100, "PlayVFX",       { vfxId = 1, offset = Vec3(0, 0.8, 1.0) })

local onHit = OnHit{ damage = 25, vfxId = 0, impulseStrength = 700, impulseDir = Vec3(0,0.1,1) }

-- 휘두름을 3구간으로: 왼→앞→오른쪽 (yaw 각도로 박스 회전)
skill:addEvent(110, "SpawnHitbox", { slot=0, localOBBs={ OBB(0.3,-0.9,-0.75, 0.15,0.7,0.9, -48,0,0) },
    attach=BoneAttach("spine_02"), applyAttachRotation=true, hitGroup=0, hitGroupCooldownMs=600, onHit=onHit })
skill:addEvent(130, "SpawnHitbox", { slot=1, localOBBs={ OBB(0.3,-1.0, 0.0, 0.15,0.9,0.8,   0,0,0) },
    attach=BoneAttach("spine_02"), applyAttachRotation=true, hitGroup=0, hitGroupCooldownMs=600, onHit=onHit })
skill:addEvent(150, "SpawnHitbox", { slot=2, localOBBs={ OBB(0.3,-0.9, 0.75, 0.15,0.7,0.9,  48,0,0) },
    attach=BoneAttach("spine_02"), applyAttachRotation=true, hitGroup=0, hitGroupCooldownMs=600, onHit=onHit })

skill:addEvent(300, "DestroyHitbox", { slot=0 })
skill:addEvent(310, "DestroyHitbox", { slot=1 })
skill:addEvent(320, "DestroyHitbox", { slot=2 })
skill:addEvent(180, "CameraShake",   { magnitude=0.3, durationMs=100 })
return skill
```

**튜닝 팁:** `hitGroup`을 같게 두면 한 번 휘두름에 적이 중복 피격되지 않는다. 다단히트를 원하면
`hitGroupCooldownMs`를 두거나 그룹을 분리. 박스가 너무 빨리 사라지면 적을 놓치니 생성~파괴 간격을 충분히.

---

### 4-2. 화살 발사 (날아가는 투사체)

**개념:** 화살은 **앞으로 날아가는 파티클**이다. 그 파티클에 작은 박스를 붙이면(§3.4) 박스가 화살을 따라
이동하며 적중 지점에서 판정된다 — 정지된 긴 박스 근사보다 정확하고 자연스럽다.
**구성:** PlayVFX로 화살 파티클을 `advance` 방향으로 발사 → 같은 `vfxId`에 `VFXParticleAttach`로 박스 부착.

```lua
local skill = Skill()
skill.name = "Arrow"; skill.totalDurationMs = 600

skill:addVFX(10, "effects/arrow.json")
skill:addEvent(0,   "PlayAnimation", { clipName = "Player_Attack", blendTime = 0.1 })
-- 화살 파티클을 전방으로 발사 (advance = 진행 방향)
skill:addEvent(120, "PlayVFX", { vfxId = 10, offset = Vec3(0,1,0.8), advance = Vec3(0,0,1) })

local onHit = OnHit{ damage = 35, impulseStrength = 500, impulseDir = Vec3(0,0.1,1) }

-- 화살 본체 파티클 시스템(systemIdx=0)에 박스 부착 → 박스가 화살을 따라 날아감.
-- 템플릿 OBB는 파티클 로컬 기준의 작은 박스(center=0). applyAttachRotation으로 화살 방향 추종.
skill:addEvent(130, "SpawnHitbox", {
    slot                = 0,
    localOBBs           = { OBB(0,0,0, 0.25,0.25,0.6, 0,0,0) },
    attach              = VFXParticleAttach(10, 0),
    applyAttachRotation = true,
    useParticleSize     = false,
    hitGroup            = 0,
    hitGroupCooldownMs  = 0,     -- 화살 하나가 적당 1회 피격
    onHit               = onHit
})
skill:addEvent(550, "DestroyHitbox", { slot = 0 })
return skill
```

**튜닝 팁:**
- 박스가 화살 본체가 아니라 트레일/스파크를 따라간다면 `systemIdx`를 본체 시스템 번호로 바꾼다(§3.4).
- SpawnHitbox는 PlayVFX **직후**(여기선 130ms)에 둬야 발사 직후부터 판정이 따라붙는다.
- 다수 화살 동시 발사(volley)는 파티클이 여러 개 방출되는 이펙트면 자동으로 박스도 여러 개 생긴다.
- **간단 대안(정적 근사):** 이동 추종이 필요 없고 즉발 직선 판정이면, `BoneAttach("spine_02")` +
  전방으로 긴 박스(`OBB(0,-0.2,2.5, 0.4,0.4,2.5)`)로도 충분하다. 비용이 가장 싸다.

---

### 4-3. 부채꼴형 발사 (전방 콘)

**개념:** 시전자 전방의 부채꼴 범위. VFX는 콘 형상(.json `shapeType=Cone`, `angle`, `radius`)으로 만들고
스킬은 `orient`로 조준한다. 판정은 (A) 부채꼴로 배열한 박스 또는 (B) 콘 파티클 정합.

```lua
local skill = Skill()
skill.name = "ConeBlast"; skill.totalDurationMs = 500

-- 이 이펙트의 .json shape: shapeType=Cone, angle=A(반각), radius=R 로 부채꼴을 정의
skill:addVFX(8, "effects/aoe_slash_green.json")
skill:addEvent(0,   "PlayAnimation", { clipName = "Player_Attack", blendTime = 0.1 })
-- 전방 2m 지점에 부채꼴 VFX 재생 (정면). 좌측으로 틀려면 orient = {45,0,0}
skill:addEvent(120, "PlayVFX", { vfxId = 8, offset = Vec3(0,1,2.0), orient = {0,0,0} })

local onHit = OnHit{ damage = 30, impulseStrength = 600, impulseDir = Vec3(0,0.2,1) }

-- (A) 박스 근사: 정면 + 좌우로 yaw를 벌린 3개의 박스로 부채꼴 흉내
skill:addEvent(160, "SpawnHitbox", { slot=0, localOBBs={
        OBB(0.0,-0.5,2.0, 1.0,1.0,2.0,  0,0,0),
        OBB(0.0,-0.5,2.0, 1.0,1.0,2.0, 30,0,0),
        OBB(0.0,-0.5,2.0, 1.0,1.0,2.0,-30,0,0) },
    attach=BoneAttach("spine_02"), applyAttachRotation=true, hitGroup=0, hitGroupCooldownMs=0, onHit=onHit })
skill:addEvent(300, "DestroyHitbox", { slot=0 })

-- (B) 콘 파티클 정합(§3.4): 위 SpawnHitbox 대신 파티클마다 박스를 붙여 부채꼴에 정확히 맞춤
-- skill:addEvent(160, "SpawnHitbox", { slot=0, localOBBs={ OBB(0,0,0, 0.3,0.3,0.3) },
--     attach = VFXParticleAttach(8, 0), useParticleSize = true, hitGroup=0, onHit = onHit })
return skill
```

**튜닝 팁:** 부채꼴 각도/반지름은 lua가 아니라 `effects/*.json`의 `angle`/`radius`/`arc`에서 바꾼다.
시전 방향과 무관하게 부채꼴을 옆이나 뒤로 쏘려면 PlayVFX `orient`의 yaw를 조절(예: 후방 = {180,0,0}).

---

### 4-4. 플레이어 주위 원형 공격 (PBAoE)

**개념:** 시전자를 중심으로 한 원형 범위 즉발. VFX는 중심(offset 0)에 `groundLock=true`로 바닥에 깐다.
**구성:** 판정은 (A) 시전자 중심의 큰 박스 또는 (B) 원형 파티클 정합. 방향 무관하므로 `applyAttachRotation=false` 권장.

```lua
local skill = Skill()
skill.name = "Nova"; skill.totalDurationMs = 700

-- 이 이펙트의 .json shape: shapeType=Circle/Sphere, radius = 원하는 반지름 M
skill:addVFX(2, "effects/slash_wave.json")
skill:addEvent(0,   "PlayAnimation", { clipName = "Player_Attack", blendTime = 0.1 })
-- 시전자 발밑 중심에 바닥 정렬로 원형 이펙트
skill:addEvent(120, "PlayVFX", { vfxId = 2, offset = Vec3(0,0,0), groundLock = true })

local onHit = OnHit{ damage = 28, impulseStrength = 650, impulseDir = Vec3(0,0.2,1) } -- impulseDir은 공격자 로컬

-- (A) 박스 근사: 시전자 중심의 넓고 낮은 박스(반지름 M ≈ hx,hz)
skill:addEvent(160, "SpawnHitbox", { slot=0, localOBBs={ OBB(0,-0.5,0, 3.0,1.0,3.0, 0,0,0) },
    attach=BoneAttach("spine_02"), applyAttachRotation=false, hitGroup=0, hitGroupCooldownMs=0, onHit=onHit })
skill:addEvent(400, "DestroyHitbox", { slot=0 })
return skill
```

**튜닝 팁:** 박스 근사는 모서리가 실제 원보다 약간 더 멀리 맞는다. 정확한 원형이 필요하면 (B) 파티클 정합(§3.4) 사용 —
원형으로 방출되는 파티클마다 박스가 붙어 링/디스크 형상에 맞는다.
넉백을 "바깥 방향"으로 주려면 단일 `impulseDir`로는 부족하므로, 방향별 박스(섹터)로 나눠 각기 다른 `impulseDir`을 준다.

---

### 4-5. 전방 원형 메테오형 (앞쪽 지면 원형 강타)

**개념:** 시전자 앞 N미터 지점의 원형 지면 범위에 강타. 메테오 낙하/충격 연출.
**구성:** VFX `offset.z = N` + `groundLock=true`. 판정은 그 지점의 박스/원형. (`spikes.lua` 가 전방 지면 AoE 원형의 베이스)

```lua
local skill = Skill()
skill.name = "Meteor"; skill.totalDurationMs = 900

-- .json shape: shapeType=Circle, radius = 착탄 반경
skill:addVFX(5, "effects/spikes.json")
skill:addEvent(0,   "PlayAnimation", { clipName = "Player_Attack", blendTime = 0.1 })
-- 전방 4m 바닥 지점에 원형 이펙트(낙하 지연 후 폭발 타이밍에 맞춰 timeMs 조정)
skill:addEvent(150, "PlayVFX", { vfxId = 5, offset = Vec3(0,0,4.0), groundLock = true })

local onHit = OnHit{ damage = 50, impulseStrength = 500, impulseDir = Vec3(0,0.8,0.4) } -- 위로 띄우는 넉백

-- 전방 4m 지점의 착탄 박스. 전방 추종을 위해 applyAttachRotation=true, cz로 전방 배치
skill:addEvent(300, "SpawnHitbox", { slot=0, localOBBs={ OBB(0,-0.8,4.0, 2.0,1.2,2.0, 0,0,0) },
    attach=BoneAttach("spine_02"), applyAttachRotation=true, hitGroup=0, hitGroupCooldownMs=0, onHit=onHit })
skill:addEvent(600, "DestroyHitbox", { slot=0 })
skill:addEvent(300, "CameraShake",  { magnitude=0.5, durationMs=200 })
return skill
```

**튜닝 팁:** "낙하 후 폭발"은 VFX 재생(150ms)과 히트박스 생성(300ms)의 시간차로 연출한다 — 이펙트가 떨어지는
동안은 판정이 없다가 착탄 순간 박스가 켜진다. 착탄 반경은 박스 `hx/hz`(근사) 또는 이펙트 `radius`(정합).
4-5는 `BoneAttach`라 박스가 시전자를 따라간다 — **지면에 고정**하려면 §4-6의 `attach={type="Ground"}`를 쓴다.

---

### 4-6. 지면 연계 (얼음 기둥 / 화살비 / 낙하 마법구)

§3.5의 세 도구(PlayVFX `groundSnap`·`particleCollision`/`particleConform`, 히트박스 `Ground` attach)를
조합한다. 기존 4-5(전방 메테오)와의 차이는 **판정·이펙트가 지면에 박혀 시전자를 따라가지 않는다**는 점.

#### (a) 얼음 기둥 솟구침 (지면에 박힌 다중 기둥)

**개념:** 시전자 전방 지면에서 얼음 기둥 여러 개가 솟아오른다. 굴곡 지면이면 각 기둥이 제 위치 높이에서
솟고, 기둥 박스는 시전자를 따라가지 않고 그 자리에 박힌다.
**구성:** PlayVFX `groundSnap`(+`particleConform="SnapAndAlign"`으로 메시 기둥이 슬로프 따라 솟음) +
`Ground` attach 히트박스(한 def에 여러 OBB를 배열, OBB별 독립 지면 스냅).

```lua
local skill = Skill()
skill.name = "IcePillars"; skill.totalDurationMs = 900

skill:addVFX(5, "effects/spikes.json")   -- mesh 파티클 기둥(지면 정보는 json에 없음 — lua가 구동)
skill:addEvent(0,   "PlayAnimation", { clipName = "Player_Attack", blendTime = 0.1 })
-- 전방 4m 지점 지면에 이펙트 배치 + 파티클이 슬로프 따라 솟도록 컨폼
skill:addEvent(150, "PlayVFX", {
    vfxId = 5, offset = Vec3(0, 0, 4.0),
    groundSnap = true, groundAlign = true,
    particleConform = "SnapAndAlign",
})

local onHit = OnHit{ damage = 40, impulseStrength = 600, impulseDir = Vec3(0, 1.0, 0.3) } -- 위로 띄움

-- 전방 일렬(또는 격자)로 기둥 3개. cz로 전방 거리, cx로 좌우 폭, cy=표면 위 높이(기둥 절반).
-- 한 def의 OBB 3개가 각자 자기 XZ의 지면 높이에 독립 스냅된다.
skill:addEvent(280, "SpawnHitbox", {
    slot = 0,
    localOBBs = {
        OBB(-1.2, 1.2, 3.2, 0.5, 1.4, 0.5, 0,0,0),
        OBB( 0.0, 1.2, 4.0, 0.5, 1.4, 0.5, 0,0,0),
        OBB( 1.2, 1.2, 4.8, 0.5, 1.4, 0.5, 0,0,0),
    },
    attach = { type = "Ground", align = false },  -- 지면 고정(시전자 비추종)
    hitGroup = 0, hitGroupCooldownMs = 0, onHit = onHit,
})
skill:addEvent(650, "DestroyHitbox", { slot = 0 })
skill:addEvent(280, "CameraShake",   { magnitude = 0.4, durationMs = 150 })
return skill
```

**튜닝 팁:** "솟아오름"은 PlayVFX(150ms)→히트박스(280ms) 시간차 + 기둥 메시의 size-over-lifetime(.json)으로
연출. 기둥 높이는 `cy`(표면 위 중심 높이)와 `hy`(반높이)로 맞춘다. 급경사에서 기둥을 수직으로 세우려면
`align=false`(노멀 정렬 끔)를 유지.

#### (b) 화살비 (상공에서 낙하 → 지면 착탄)

**개념:** 드롭존 상공에서 화살 파티클이 쏟아져 지면에 닿으면 사라지며 먼지가 튄다. 그동안 드롭존에는
평평한 지면 AoE 판정이 깔린다.
**구성:** PlayVFX `groundSnap`(드롭존 중심을 지면에)+`particleCollision="GroundKill"`(화살이 땅에서 소멸→
Death 서브이미터 착탄 버스트) + `Ground` attach 평면 AoE 박스.

```lua
local skill = Skill()
skill.name = "ArrowRain"; skill.totalDurationMs = 1200

skill:addVFX(12, "effects/arrow_rain.json")  -- 중력으로 낙하 + Death 서브이미터(착탄 먼지)
skill:addEvent(0,   "PlayAnimation", { clipName = "Player_Attack", blendTime = 0.1 })
-- 전방 6.5m 드롭존을 지면에 스냅하고, 그 위 상공(offset.y)에서 방출되도록 이펙트 배치.
-- 화살 파티클은 닿으면 소멸(GroundKill) → Death 서브이미터로 착탄 버스트.
skill:addEvent(120, "PlayVFX", {
    vfxId = 12, offset = Vec3(0, 8.0, 6.5),
    groundSnap = true,
    particleCollision = "GroundKill",
})

local onHit = OnHit{ damage = 8, impulseStrength = 150, impulseDir = Vec3(0, -0.2, 0.3) }

-- 드롭존에 깔리는 평평한 지면 AoE 박스. 화살비가 지속되는 동안 다단히트(cooldown).
skill:addEvent(300, "SpawnHitbox", {
    slot = 0,
    localOBBs = { OBB(0.0, 0.6, 6.5, 2.5, 1.2, 2.5, 0,0,0) }, -- cz=6.5 전방, cy=표면 위 낮게
    attach = { type = "Ground", align = false },
    hitGroup = 0, hitGroupCooldownMs = 300,  -- 0.3s마다 재피격(비처럼 지속 피해)
    onHit = onHit,
})
skill:addEvent(1000, "DestroyHitbox", { slot = 0 })
return skill
```

**튜닝 팁:** 낙하 높이는 `offset.y`(드롭존 표면 위), 드롭존 위치는 `offset.z`(전방거리). 지속 피해는
`hitGroupCooldownMs`로 조절. 착탄 먼지는 이펙트 `.json`의 Death 서브이미터가 담당하므로 lua는
`particleCollision="GroundKill"`만 켜면 된다.

#### (c) 낙하 마법구 (공중에서 한 발 떨어져 착탄 폭발)

**개념:** 마법구가 대상 지점 상공에서 떨어져 지면에 닿는 순간 폭발한다.
**구성:** PlayVFX `groundSnap`(착탄점)+`particleCollision="GroundStop"`(또는 Kill) + 착탄 타이밍에 맞춘
`Ground` attach 폭발 박스.

```lua
local skill = Skill()
skill.name = "FallingOrb"; skill.totalDurationMs = 800

skill:addVFX(8, "effects/red_energy_explosion.json")  -- 낙하 orb + 착탄 폭발(Death 서브이미터)
skill:addEvent(0,   "PlayAnimation", { clipName = "Player_Attack", blendTime = 0.1 })
-- 전방 5m 착탄점을 지면에 스냅하고 그 위 12m에서 orb 방출. 닿으면 정지(또는 GroundKill로 즉소멸).
skill:addEvent(100, "PlayVFX", {
    vfxId = 8, offset = Vec3(0, 12.0, 5.0),
    groundSnap = true,
    particleCollision = "GroundStop",
})

local onHit = OnHit{ damage = 55, impulseStrength = 800, impulseDir = Vec3(0, 0.6, 0.5) }

-- 착탄 예상 시각(400ms)에 폭발 판정. 지면에 박혀 한 번 터진다.
skill:addEvent(400, "SpawnHitbox", {
    slot = 0,
    localOBBs = { OBB(0.0, 0.8, 5.0, 2.2, 1.4, 2.2, 0,0,0) },
    attach = { type = "Ground", align = true },  -- 폭발 디스크를 지면에 눕힘
    hitGroup = 0, hitGroupCooldownMs = 0, onHit = onHit,
})
skill:addEvent(600, "DestroyHitbox", { slot = 0 })
skill:addEvent(400, "CameraShake",   { magnitude = 0.6, durationMs = 200 })
return skill
```

**튜닝 팁:** 낙하 시간은 `offset.y`(높을수록 오래 떨어짐)와 히트박스 생성 시각(400ms)을 시각적으로 맞춘다 —
파티클이 땅에 닿는 순간과 박스가 켜지는 순간이 일치하도록 에디터에서 조정. 착탄 폭발 연출은 이펙트의
Death 서브이미터가 담당한다.

---

## 5. 체크리스트 / 디버깅

- [ ] `return skill` 빠뜨리지 않았는가.
- [ ] `vfxId`가 `skillVfxById_`(standalone/online)에 바인딩되어 있는가. 안 되면 VFX 무음(no-op).
- [ ] SpawnHitbox `slot`과 DestroyHitbox `slot`이 짝이 맞는가. 안 맞으면 박스가 안 사라진다.
- [ ] 전방 배치가 시전 방향을 따라야 하면 `applyAttachRotation=true`(히트박스) / PlayVFX는 기본 추종.
- [ ] 바닥 평면 고정이 필요하면 PlayVFX `groundLock=true`, 히트박스는 `applyAttachRotation=false`.
- [ ] 원형/부채꼴 **크기**는 lua가 아니라 `effects/*.json`의 shape(`radius`/`angle`/`arc`)에서 조정.
- [ ] `VFXParticleAttach`(투사체·정합)는 ① 같은 `vfxId`가 PlayVFX로 먼저 재생되고 ② `systemIdx`가
      투사체 본체 시스템을 가리키는가. 파티클이 없으면 박스도 생기지 않는다(§3.4).
- [ ] **지면 연계(§3.5):** 지면 위 배치는 PlayVFX `groundSnap`(위치) — `groundLock`(방향)과 혼동 금지.
- [ ] 떨어지는 파티클을 땅에서 소멸·폭발시키려면 PlayVFX `particleCollision="GroundKill"` + 이펙트에
      **Death 서브이미터**가 있어야 착탄 버스트가 나온다(없으면 그냥 사라짐).
- [ ] 지면 AoE/기둥 판정은 `BoneAttach`(시전자 추종)가 아니라 `attach={type="Ground"}`(지면 고정)인가.
      `Ground`에서 `cy`는 **표면 위 높이**, `cx/cz`는 시전자 상대 오프셋이다.
- [ ] 낙하물(화살비·마법구)은 파티클 착지 순간과 `Ground` 히트박스 생성 `timeMs`를 맞췄는가
      (이펙트 `offset.y` 높이 ↔ 낙하 시간 ↔ SpawnHitbox 시각).
- **튜닝 도구:** standalone 스킬 에디터에서 박스를 시각적으로 잡고 euler/offset을 round-trip 편집할 수 있다
  (Space 재생, LMB 박스 선택, ↑/↓/←/→ 넛지, P diff 덤프). 자세한 키맵은 `CODE_INDEX.md` 스킬 에디터 절 참조.

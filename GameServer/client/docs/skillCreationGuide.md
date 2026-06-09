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
| `attach` | 루트 | `BoneAttach(name)`; 생략 시 시전자 루트 |

> **형상 크기(원형 반지름 M, 부채꼴 반지름 R·각도 A)는 PlayVFX가 아니라 이펙트 프리팹 `.json`의
> shape config에 작성한다** (`shapeType`: Cone/Circle/Sphere…, `radius`, `angle`, `arc`).
> PlayVFX는 **배치·방향·진행만** 제어한다. 즉 같은 형상을 여러 스킬이 위치/방향만 달리해 공유한다.

#### SpawnHitbox — 피해 판정 박스 생성
| 키 | 기본 | 설명 |
|----|------|------|
| `slot` | 0 | DestroyHitbox가 참조할 식별자 |
| `localOBBs` | {} | `OBB(...)` 배열(여러 개 가능) |
| `attach` | Bone | `BoneAttach` 또는 `VFXParticleAttach` |
| `applyAttachRotation` | true | §2 참조 |
| `hitGroup` | 0 | 같은 그룹끼리 중복 피격 제거 공유 |
| `hitGroupCooldownMs` | 0 | 0 = 대상당 1회 / >0 = N ms 후 재피격 |
| `useParticleSize` | false | VFXParticle 전용: 파티클 시각 크기로 반치수 스케일 |
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

**전제 조건 / 주의:**
- 해당 `vfxId`가 **PlayVFX로 재생되어 파티클이 살아 있어야** 박스가 생긴다. 보통 PlayVFX → (약간 뒤) SpawnHitbox 순.
- `systemIdx`는 이펙트 안의 **서브 시스템 인덱스**다. 이펙트가 본체/트레일/스파크 등 여러 시스템으로
  구성된 경우, **투사체 본체에 해당하는 시스템**을 골라야 한다(트레일에 붙이면 판정이 꼬리에 생긴다).
- 비용은 **활성 파티클 1개당 박스 1개**다. 투사체는 본체 파티클 수를 적게(이상적으로 1~소수) 유지하라.
  원형/콘 AoE를 파티클 정합으로 만들면 파티클 수가 그대로 박스 수가 되므로 방출량에 유의.
- SpawnHitbox로 소스를 만들고 DestroyHitbox(같은 `slot`)로 정리한다. 정리 전까지 박스는 파티클을 계속 추종한다.

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
- **튜닝 도구:** standalone 스킬 에디터에서 박스를 시각적으로 잡고 euler/offset을 round-trip 편집할 수 있다
  (Space 재생, LMB 박스 선택, ↑/↓/←/→ 넛지, P diff 덤프). 자세한 키맵은 `CODE_INDEX.md` 스킬 에디터 절 참조.

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
> 현재 "투사체"는 별도 이동 오브젝트가 아니라 **전방으로 길쭉한 히트박스 + 전진 VFX**로 표현한다(아래 4-2).

### 3.3 히트박스 형상에 대한 핵심 원칙

히트 판정 형상은 **OBB(박스)뿐**이다. 원형/부채꼴은 두 가지로 표현한다:

- **(A) 박스 근사** — 큰 박스 하나, 또는 여러 박스를 부채꼴로 배열. 간단하고 비용 저렴. 대부분 충분.
- **(B) 파티클 정합** — `VFXParticleAttach`로 이펙트의 파티클마다 박스를 붙인다. 이펙트의 shape가
  원형/콘이면 히트박스가 자연히 그 형상을 따른다. `useParticleSize=true`면 파티클 크기에 맞춰 박스가 커진다.
  시각과 판정이 정확히 일치하지만 파티클 수만큼 박스가 생기므로 비용에 유의.

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

### 4-2. 화살 발사 (직선 투사체)

**개념:** 전방으로 길쭉한 박스를 깔아 일직선 관통을 판정하고, VFX는 앞으로 전진시킨다.
(별도 이동 오브젝트는 아직 없음 — §3.2 SpawnProjectile 스텁 참고)
**구성:** 전방으로 긴 OBB(`hz` 큼, `cz`를 전방으로). VFX는 `advance`로 진행 방향을 줘 날아가는 느낌. (`arrow.lua` 참고)

```lua
local skill = Skill()
skill.name = "Arrow"; skill.totalDurationMs = 600

skill:addVFX(10, "effects/arrow.json")
skill:addEvent(0,   "PlayAnimation", { clipName = "Player_Attack", blendTime = 0.1 })
-- 화살 VFX를 전방으로 진행시킴
skill:addEvent(120, "PlayVFX", { vfxId = 10, offset = Vec3(0,1,0.8), advance = Vec3(0,0,1) })

local onHit = OnHit{ damage = 35, impulseStrength = 500, impulseDir = Vec3(0,0.1,1) }
-- 전방으로 긴 박스(길이 5m = hz 2.5, 중심 z 2.5)
skill:addEvent(140, "SpawnHitbox", { slot=0, localOBBs={ OBB(0,-0.2,2.5, 0.4,0.4,2.5, 0,0,0) },
    attach=BoneAttach("spine_02"), applyAttachRotation=true, hitGroup=0, hitGroupCooldownMs=600, onHit=onHit })
skill:addEvent(400, "DestroyHitbox", { slot=0 })
return skill
```

**튜닝 팁:** 관통 사거리는 `hz`(반길이)와 `cz`(전방 오프셋)로 조절. 단일 적만 맞히려면 `hitGroupCooldownMs=0`.
VFX가 캐릭터 자세를 따라 기울지 않게 하려면 PlayVFX에 `groundLock=true` 또는 `orient`로 보정.

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

-- (B) 정합을 원하면 위 SpawnHitbox 대신:
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

**튜닝 팁:** 박스 근사는 모서리가 실제 원보다 약간 더 멀리 맞는다. 정확한 원형이 필요하면 (B) 파티클 정합 사용.
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
- **튜닝 도구:** standalone 스킬 에디터에서 박스를 시각적으로 잡고 euler/offset을 round-trip 편집할 수 있다
  (Space 재생, LMB 박스 선택, ↑/↓/←/→ 넛지, P diff 덤프). 자세한 키맵은 `CODE_INDEX.md` 스킬 에디터 절 참조.

# 지면 연계 Particle Effect / 스킬 설계

> 얼음 기둥 솟구침, 화살비, 공중에서 지면으로 떨어지는 마법구처럼 **지면(terrain)과 상호작용하는**
> 스킬/이펙트를 **스킬 lua만으로** 저작할 수 있게 하는 엔진 기반 설계.
> (effect JSON은 Unity 익스포트 아트 에셋이라 수정하지 않는다 — 지면 정보는 전부 lua에서 구동한다.)

## 배경 / 문제

도입 전에는 두 가지 이유로 지면 연계 스킬을 만들 수 없었다.

1. **파티클 시스템에 지면 인식이 전혀 없었다.** 스폰 위치는 스폰 시점에 1회 계산되고
   (`particleSystem.cpp` `spawnParticle`/`sampleShapeOrigin`), 시뮬레이션 중 지면 충돌도 없었다.
2. **데이터 주도 스킬 `PlayVFX` 경로에 지면 스냅이 없었다.** 지면 높이 스냅은 `onlineGame.cpp`의
   하드코딩 디버그 경로(`SwordEffect::ArrowRain/RedEnergyExplosion`)에만 존재했고, 신규 `PlayVFX`
   이벤트는 지면을 몰랐다. 게다가 지면 AOE 히트박스를 `BoneAttach("spine_..")`로 저작해 시전자
   척추를 따라다니는 잘못된 동작을 했다.

지면 높이 질의는 이미 결정론적으로 존재했다(`TerrainChunkManager::heightAtWorld/normalAtWorld`,
서버 `Room::groundHeightAtWorld`). 본 작업은 이 질의를 GFX/스킬 레이어가 디커플링된 방식으로 소비하게
하고, 지면 연계를 위한 재사용 가능한 프리미티브 4종을 도입한다.

## 레이어 개요

```
 Layer 0  GroundSampler   콜백 번들(height/normal). 순수, terrain 의존 없음
 Layer 1  GFX(파티클)      ShapeModule 지면 컨폼 스폰 + ParticleCollisionModule(지면 충돌)
 Layer 2  Skill(데이터)    PlayVFX 지면 스냅 플래그 + AttachType::Ground 히트박스
 Layer 3  Anchor          SkillInstance::castAnchor = 시전자 위치+yaw (프로토콜 변경 없음)
```

GFX 레이어와 스킬 레이어는 **동일한 `GroundSampler` 타입을 각자 보관**한다. 공유 소유자가 없어
GFX/game 레이어 분리를 지킨다.

---

## Primitive 1 — GroundSampler (디커플링 플러밍)

`client/groundSampler.hpp` — `std::function` 두 개(height/normal)를 담는 경량 번들. 미바인딩 시
height=0, normal=up을 반환하며 `operator bool()`로 "지면 없음(예: 청크 미로드)"을 판별한다. 이 경우
호출부는 **스냅을 건너뛴다**(0으로 스냅 금지).

배선:
- **standalone** `standalone/game.cpp` — `groundSampler_`를 `chunkManager_`에 바인딩, `skillCtx_.ground` 설정.
  에디터의 `terrainHeightAt`도 `groundSampler_.heightAt` 재사용.
- **online** `online/onlineGame.cpp` — 동일하게 바인딩, `skillCtx_.ground` 설정.
- **server** `Room::bindGroundQueries()` — 스킬 디스패치 컨텍스트에 `groundHeight`/`groundNormal` 콜백 주입
  (`updateSkillSystem`/`skillStart` 양쪽).

`ParticleSystem::setGroundSampler` / `ParticleEffect::setGroundSampler`로 GFX 레이어에 전달된다.

---

## Primitive 2 — 파티클 지면 컨폼 스폰 (GFX)

`ShapeModule`(`particleModules.hpp`):
```cpp
enum class GroundConform { None, SnapY, SnapAndAlign };
GroundConform groundConform = GroundConform::None;
float         groundOffset  = 0.f;   // surface 위로 띄우기(z-fight 방지)
```
스폰 시 origin의 Y를 지면으로 스냅한다. `SnapAndAlign`은 추가로 메시 파티클의 `baseRotation`을 지면
노멀에 맞춰 기울인다(얼음 기둥/데칼이 슬로프를 따름). 면적 emitter(Circle/Box/Edge)가 울퉁불퉁한
지면 위에 안착한다.

**구동은 effect JSON이 아니라 스킬 lua에서 한다** — JSON은 Unity 익스포트 아트 에셋이라 스키마를
바꿀 수 없으므로, 지면 정보는 PlayVFX 이벤트(`particleConform`)로 주입한다 → Primitive 4 참조.

> **⚠ 서브이미터 본체 패턴(실전 주의):** 얼음 기둥·잔해 등 시각 본체가 **Birth 서브이미터**로 부모
> 파티클 위치에 스폰되는 이펙트가 많다(예: `crystals_front_attack`의 system 0=Cone 부모, system 1=
> "Crystals" 서브이미터=실제 기둥). `emitAt`이 자식 스폰 시 `shape.position`을 부모 파티클 위치로
> 잡으므로, **conform은 서브이미터 시스템에도 적용해야** 각 조각이 제 XZ의 지면에 안착한다. 따라서
> `setGroundBehavior`는 conform을 전 시스템에 적용한다(collision만 top-level 한정).
>
> **빌보드 vs 메시:** `SnapAndAlign`의 노멀 정렬(기울임)은 **메시 파티클**에서만 보인다.
> Billboard/StretchedBillboard는 카메라를 향하므로 기울지 않으나 `SnapY`(높이 스냅)는 적용된다.
> 즉 빌보드 기둥은 "지면 높이는 따라가되 똑바로 선다".

---

## Primitive 3 — 파티클 지면 충돌 (GFX)

`ParticleCollisionModule`(`particleModules.hpp`):
```cpp
enum class Mode { None, GroundStop, GroundKill, GroundBounce };
bool enabled; Mode mode; float bounce; float radiusOffset;
```
시뮬 루프에서 낙하/정적 파티클을 지면과 교차 검사한다(`vel.y<0` 게이트).
- `GroundStop`: 표면에 정지(속도/중력 0)
- `GroundBounce`: 수직 반사(`bounce`)
- `GroundKill`: `lifetime=0` → **기존 Death 서브이미터 경로가 충돌 지점에서 임팩트 버스트 자동 발생**

화살비/마법구의 착탄 먼지가 추가 코드 없이 나온다.

**구동은 스킬 lua에서 한다**(JSON 아님): PlayVFX `particleCollision`으로 주입 → Primitive 4 참조.
`ParticleEffect::setGroundBehavior()`가 적용하되 **collision은 top-level 시스템에만**(sub-emitter
임팩트 버스트가 착탄점에서 스폰되자마자 죽지 않게). **conform은 sub-emitter 포함 전 시스템**에 적용 —
얼음 기둥처럼 시각 본체가 Birth 서브이미터인 경우가 많기 때문(아래 Primitive 2 참조).

---

## Primitive 4 — PlayVFX 지면 배치 (데이터 주도)

`PlayVFX::flags` 1바이트에 모두 패킹(56바이트 페이로드 유지, 페이로드 증가 없음):
```
bit0  yawOnly       지면 평면 배치(pitch/roll 무시)
bit1  groundSnap    worldPos.y를 지면으로 스냅(localOffset.y = lift)
bit2  groundAlign   지면 노멀로 정렬(yaw 보존)
bits3-4  particleCollision  이펙트 파티클 충돌 모드(Primitive 3 ordinal)
bits5-6  particleConform    이펙트 파티클 컨폼 모드(Primitive 2 ordinal)
```
디스패치 시 ① worldPos.y를 지면+offset.y로 스냅, ② `fx->setGroundSampler(ctx.ground)`,
③ 비트3-6을 디코드해 `fx->setGroundBehavior(collision, conform)`로 **이펙트 파티클의 P2·P3을 lua에서
구동**한다(JSON 무수정). vfxId↔이펙트 1:1이라 재생 직전 config 덮어쓰기가 안전하다.
이로써 `onlineGame.cpp`의 하드코딩 지면 스냅 경로(`SwordEffect::ArrowRain/RedEnergyExplosion`)가
**완전히 제거**되었다.

**lua 사용법(지면 정보 전부 lua, json 무관):**
```lua
skill:addEvent(120, "PlayVFX", {
    vfxId = 12, offset = Vec3(0, 8.0, 6.5),  -- 드롭존 8m 상공, 6.5m 전방
    groundSnap = true,                        -- 드롭존 중심을 지면에 스냅(배치)
    particleCollision = "GroundKill",         -- 화살 파티클이 지면에 닿으면 소멸→Death 버스트
    -- particleConform = "SnapAndAlign",      -- (면적 emitter가 슬로프 따라 안착할 때)
})
```
`particleCollision`: `"GroundStop"|"GroundKill"|"GroundBounce"`, `particleConform`: `"SnapY"|"SnapAndAlign"`.

---

## Primitive 5 — 지면 고정 히트박스 `AttachType::Ground`

bone을 따라가지 않고 **SpawnHitbox 시점에 1회 지면 스냅 후 세계에 정적으로 박히는** 히트박스.
`updateHitboxes`는 비-Bone을 스킵하므로 Ground는 자동으로 정적이다.

- 앵커 = 시전자 XZ + yaw(시전 시점 캡처, `SkillInstance::castAnchor`).
- lua OBB의 `center.x/z`는 시전자 전방/우측 오프셋, `center.y`는 표면 위 높이.
- **OBB별 독립 스냅** → 지면 컨폼 기둥 그리드 지원.
- `align=true`면 각 OBB를 지면 노멀로 기울임(기본 false=yaw만; 급경사 leaning 방지).

클라/서버 양쪽에서 동일하게 OBB를 생성하므로 권위적 판정이 클라 예측과 일치한다.

**lua 사용법:**
```lua
skill:addEvent(250, "SpawnHitbox", {
    slot = 0,
    localOBBs = { OBB(0.0, 0.8, 6.5, 2.0, 1.5, 2.0, 0, 0, 0) }, -- 6.5m 전방, 지면에 평면 AOE
    attach = { type = "Ground", align = false },  -- 척추 추종이 아닌 지면 고정
    onHit = onHit,
})
```

---

## Primitive 6 — 앵커 스레딩 (프로토콜 변경 없음)

조준 방식은 **시전자 상대(전방 고정)**. 양측이 동일 시전자 엔티티에서 앵커(pos+yaw)를 유도하고
지면 높이는 XZ에 대해 결정론적이므로, 신규 패킷 없이 클라/서버 스냅 위치가 일치한다. 잔여 차이는
시전자 XZ 드리프트뿐이며 `kHitboxAABBMargin=0.2`가 완충한다.

> **향후 확장**: 레티클(클릭) 조준으로 바꾸려면 `castAnchor`를 채우는 위치만 패킷
> (C_/S_SkillStart에 targetX/Z/yaw 추가)로 교체하면 된다. 현재 구조는 이를 위해 `castAnchor`
> 구조체로 추상화되어 있다.

---

## 작업 예시 (저작은 에디터/lua로 사용자가 진행)

- **얼음 기둥**: PlayVFX `groundSnap=true, groundAlign=true, particleConform="SnapAndAlign"` +
  각 기둥 `attach={type="Ground", align=true}`, OBB center=시전자 상대 링 좌표 + 기둥 base 높이.
  effect json은 mesh 파티클만(지면 정보 없음).
- **화살비**: PlayVFX `offset=Vec3(0,8,6.5), groundSnap=true, particleCollision="GroundKill"`;
  effect json은 `main.gravity` 하강 + Death 서브이미터(착탄 버스트)만; 히트박스 `attach={type="Ground"}`(평면 AOE).
- **낙하 마법구**: PlayVFX `offset=Vec3(0,12,5), groundSnap=true, particleCollision="GroundStop"`(또는 Kill);
  orb 파티클 하강 + Death 폭발; 착탄 타이밍 `attach={type="Ground", align=true}` 폭발 히트박스.

> 마이그레이션 노트: 기존 `spikes.lua`/`arrow_rain.lua`의 히트박스는 `BoneAttach("spine_..")`로
> 시전자를 따라다닌다. 위 프리미티브로 `attach={type="Ground"}` + PlayVFX `groundSnap`으로 교체하면
> 지면에 고정된다. 콘텐츠 수정은 스킬 에디터에서 진행한다.

---

## 수정 파일 맵

| 영역 | 파일 |
|------|------|
| GroundSampler | `client/groundSampler.hpp` (신규) |
| 파티클 GFX | `client/particleModules.hpp`(ShapeModule/ParticleCollisionModule 필드), `particleSystem.hpp/.cpp`(spawn/충돌 hook), `particleEffect.hpp/.cpp`(`setGroundSampler`/`setGroundBehavior`) |
| 스킬 타입(클라) | `client/skill/skillTypes.hpp` (flags, AttachType::Ground, AttachTarget::groundAlign) |
| 스킬 런타임(클라) | `client/skill/skillSystem.hpp/.cpp` (ctx.ground, castAnchor, PlayVFX/Ground dispatch) |
| 스킬 컴파일(클라) | `client/skill/skillCompiler.cpp` (groundSnap/groundAlign, "Ground" attach) |
| 게임 배선(클라) | `client/standalone/game.cpp`, `client/online/onlineGame.cpp` (groundSampler_ 바인딩, 레거시 제거) |
| 스킬(서버) | `RoomServer/skill/skillTypes.hpp`, `skillSystem.hpp/.cpp`, `skillCompiler.cpp` (미러) |
| 게임 배선(서버) | `RoomServer/Room.hpp/.cpp` (`bindGroundQueries`) |

## 리스크 / 주의

- **per-particle 지면 질의 비용**: 스폰당 1회(P2) + 낙하 파티클 프레임당 1회(P3, `vel.y<0` 게이트).
  수천 파티클 emitter는 핫패스 → 필요한 emitter만 활성화.
- **멀티청크 경계**: 미로드 청크는 `!ground`로 처리해 스냅 스킵(0으로 스냅 금지).
- **바이너리 레이아웃**: 플래그는 기존 `flags` 바이트 재사용 → **PlayVFX 56바이트 유지**. AttachType/
  AttachTarget 변경은 `SkillAsset`(힙, 프로세스별 컴파일)에만 영향 → 와이어 무관.
- **groundAlign 히트박스**: 급경사에서 OBB leaning 가능 → 기본 false(yaw만), align은 VFX 위주.

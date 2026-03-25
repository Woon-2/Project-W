# ParticleSystem TODO

Stage 1~10 모두 완료. 불꽃 파티클 렌더링 완성.

## 완료된 추가 작업
- **Texture Sheet Animation 구현** (스프라이트 시트 방식으로 전환)
  - `SpriteAnimFrame`: 개별 `Texture` → `uvOffset` / `uvScale` 방식으로 교체
  - `SpriteAnimationClip`: 프레임별 텍스처 N개 → 공유 `spriteSheet` 텍스처 1개
  - `loadSpriteSheetAnimation()`: 그리드 파라미터(rows, cols, frameCount) 기반 로더
  - `billboard.hlsl` GS: 로컬 UV → 스프라이트 시트 UV 변환 (`uv = baseUV * uvScale + uvOffset`)
  - `PerDrawcallData` cbuffer: `uvOffset` / `uvScale` 필드 추가
  - 유니티 익스포트 방식 변경: PNG N장 + 바이너리 → 스프라이트 시트 PNG 1장
  - flame 설정: `a_VFX_flame.dds`, 3×3 그리드, 9프레임, 80ms/frame

- **파티클 lifetime ↔ 애니메이션 속도 동기화**
  - `emit()` 시 `SpriteAnimType::Loop` → lifetime 내 정수 N번 완전 루프 재생되도록 speed 자동 계산
  - `SpriteAnimType::Once` → lifetime과 동시에 애니메이션 완료되도록 speed 자동 계산
  - 파티클이 임의의 프레임에서 갑자기 사라지는 현상 수정

## 남은 작업

---

### Stage 11: ParticleEffect (멀티 이미터 복합 이펙트)

**목표**: 여러 EmitterConfig를 하나의 이펙트로 묶어 동시 재생

**배경**: 현재 `ParticleSystem`은 continuous emitter를 하나만 지원한다.
불꽃+연기처럼 여러 이미터를 조합할 경우 `Game`이 `ParticleSystem` 인스턴스를 여러 개 직접 보유해야 하는데,
이펙트가 늘어날수록 `Game` 클래스가 비대해진다.

**설계**:
- 신규: `client/particleEffect.hpp` / `particleEffect.cpp`
  - `EmitterEntry { EmitterConfig config; float delay=0; float stopAt=-1; }`
    - `delay`: 이펙트 시작 후 이 이미터가 활성화되기까지의 지연(초)
    - `stopAt`: -1이면 끝까지, 양수이면 이 시간 이후 이미터 emit 중단
  - `ParticleEffect` API:
    - `addEmitter(EmitterEntry)` — 이미터 정의 추가
    - `play(ParticleSystem&, float duration=-1)` — 재생 시작. -1이면 무한, 양수이면 duration 후 자동 정지
    - `stop()` — emit 중단 (살아있는 파티클은 수명까지 유지)
    - `update(Seconds dt)` — 매 프레임 호출
    - `isPlaying() / isFinished()` — 상태 조회
  - 내부: per-emitter `emitAccum` 누산, delay/stopAt 타이밍 제어
- `ParticleSystem` 리팩토링:
  - `startContinuous` / `stopContinuous` / `continuousConfig_` / `emitAccum_` / `continuous_` 제거
  - continuous emit 로직을 `ParticleEffect`로 이관
  - `update(dt)` 는 물리/수명 갱신만 담당
- `Game` 교체:
  - `flameParticleSystem_` + `smokeParticleSystem_` (인스턴스 2개)
    → `particleSystem_` (단일 전역 풀) + `campfireEffect_` (ParticleEffect)

---

### Stage 12: VFXManager (이름/ID 기반 이펙트 관리)

**목표**: 이펙트를 이름으로 등록하고 play/stop 핸들로 수명 관리

**배경**: Stage 11 이후에도 게임 이벤트마다 `ParticleEffect`를 `Game`이 직접 생성·관리하면 구조가 비대해진다.
이펙트 등록/재생/수명 관리를 분리된 레이어로 캡슐화한다.

**설계**:
- 신규: `client/vfxManager.hpp` / `vfxManager.cpp`
  - `VFXDef { vector<EmitterEntry> emitters; float duration=-1; }` — 이펙트 정의
  - `using VFXId = uint32_t;` — 등록 시 반환되는 ID
  - `struct VFXHandle` — play 시 반환되는 핸들 (stop/상태 조회용)
  - `VFXManager` API:
    - `registerEffect(name, VFXDef)` → `VFXId`
    - `findEffect(name)` → `VFXId`
    - `play(id, pos, ParticleSystem&)` → `VFXHandle` (일회성, duration 경과 후 자동 제거)
    - `playPersistent(id, pos, ParticleSystem&)` → `VFXHandle` (stop() 명시 필요)
    - `stop(VFXHandle)`
    - `update(Seconds dt)` / `render(GFX&)`
  - 내부: 128개 고정 풀 + free list + generation 카운터 (stale handle 방지)
- `Game` 교체:
  - 파티클 관련 모든 멤버 → `ParticleSystem particleSystem_` + `VFXManager vfxManager_`
  - `setupStage()`에서 이펙트 등록, `update()`/`render()`에서 단일 호출

**Stage 11+12는 세트로 구현**: 두 클래스가 갖춰져야 `Game` 클래스 정리가 의미 있음

---

### Stage 13: World Attachment (오브젝트 추종 파티클)

**목표**: 이동하는 오브젝트를 따라가는 연속 이펙트 지원

**배경**: 현재 `EmitterConfig.position`은 emit 시점의 고정 좌표다.
무기 트레일, 버프 오라처럼 캐릭터에 붙는 이펙트는 매 프레임 위치를 갱신해야 한다.

**설계**:
- `ParticleEffect`에 attachment 추가:
  - `enum class AttachMode { None, Follow, Inherit }`
    - `Follow`: 위치만 추종
    - `Inherit`: 위치 + 방향(direction) 모두 오브젝트 기준으로 갱신
  - `struct AttachTarget { const Object* pObj; Vec3 offset; AttachMode mode; }`
  - `ParticleEffect::setAttachTarget(AttachTarget)` 추가
  - `update()` 내부에서 매 프레임 `config.position` (Inherit이면 `config.direction`도) 갱신
- `VFXManager::playAttached(id, AttachTarget, ParticleSystem&)` → `VFXHandle` 추가

**구현 시점**: attachment가 필요한 구체적인 이펙트(트레일, 오라 등)가 기획될 때 구현

---

### Stage 14: 게임 이벤트 연결

**목표**: Hit, Death, Attack 등 게임 이벤트 발생 시 이펙트 자동 재생

**배경**: Stage 11+12+13이 갖춰지면 이벤트 처리 블록에 `vfxManager_.play(...)` 한 줄 삽입으로 완성된다.

**설계**:
- `Game::setupStage()`에서 이펙트 사전 등록:
  ```cpp
  hitEffectId_   = vfxManager_.registerEffect("hit",   { ..., .duration=0.4f });
  deathEffectId_ = vfxManager_.registerEffect("death", { ..., .duration=1.5f });
  ```
- `Game::update()` 이벤트 처리 블록에 트리거 삽입:
  - `EventType::Hit`   → `vfxManager_.play(hitEffectId_,   targetPos, particleSystem_)`
  - `EventType::Death` → `vfxManager_.play(deathEffectId_, victimPos, particleSystem_)`
- `event.hpp`의 `Blood` 이벤트 활성화 검토 (혈흔 파티클)
- 위치 조회 헬퍼 `getObjectPos(id)` private 함수 추가 (반복 패턴 정리)

**구현 순서**: Stage 11+12 → Stage 14 → Stage 13

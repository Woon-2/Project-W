# ParticleSystem 재설계 TODO

현재 `particleSystem.hpp/cpp`를 완전히 폐기하고 단계적으로 새로 구축한다.
각 단계가 끝날 때마다 **화면에서 직접 확인**하고 다음 단계로 넘어간다.

---

## Stage 1 — 단일 파티클 이미지 출력 확인

**목표:** `SpriteAnimation`, `EmitterConfig`, 풀 없이, 딱 하나의 billboard 텍스처가 화면에 나오는지 확인한다.

- [x] `particleSystem.hpp` / `particleSystem.cpp` 기존 코드 전부 삭제
- [x] 새 `ParticleSystem` 클래스 작성 (멤버: 위치 하나, 텍스처 인덱스 하나)
- [x] `render(GFX&)` 에서 해당 위치에 billboard drawcall 한 번만 제출
- [x] `game.cpp`에서 `ParticleSystem` 인스턴스 생성 후 `render()` 호출
- [x] 빌드 & 실행 → Flame 텍스처 quad가 화면에 보이는지 확인

**확인 기준:** Flame_0.dds 이미지가 3D 공간의 지정 위치에 카메라를 향해 렌더링된다. ✓

---

## Stage 2 — Particle Pool + 기본 물리

**목표:** 여러 파티클을 pool에 넣고, 중력/속도로 움직이다가 lifetime이 다하면 사라지게 한다.

- [x] `Particle` 구조체 정의
  ```cpp
  struct Particle {
      mu::Vec3 pos, vel;
      float    lifetime, maxLifetime;
      int      texIdx;
      bool     active;
  };
  ```
- [x] `static constexpr kMaxParticles = 64` (작게 시작)
- [x] `std::array<Particle, kMaxParticles> pool_` + ring-buffer `cursor_`
- [x] `emit(pos, count)` — 위 방향 고정, 속도 1~3 랜덤, lifetime 0.5~1.5s
- [x] `update(dt)` — `vel += gravity * dt`, `pos += vel * dt`, lifetime 감소 → 0 이하면 `active = false`
- [x] `render()` — active 파티클마다 billboard 제출
- [x] `game.cpp`에서 키 입력시 `emit()` 호출
- [x] 빌드 & 실행 → 파티클들이 위로 솟구쳤다가 중력으로 떨어지는지 확인

**확인 기준:** emit 후 파티클들이 퍼지며 날아가다 사라진다.

---

## Stage 3 — SpriteAnimation 연동 (프레임 애니메이션)

**목표:** 정적 이미지 대신 `SpriteAnimation`으로 Flame 프레임 애니메이션을 재생한다.

- [x] `Particle`에 `SpriteAnimation anim` 추가, `texIdx` 제거
- [x] `emit()` 에서 `p.anim.init(pClip)`, `p.anim.setPos(pos)` 초기화
- [x] `update()` 에서 `p.anim.update(dt)`, `p.anim.setPos(p.pos)` 호출
- [x] `anim.done()` 이면 `active = false`
- [x] `render()` 에서 `p.anim.render(gfx)` 호출
- [x] `EmitterConfig` 없이 `pClip` 을 `emit()` 인자로 직접 받아도 됨
- [x] 빌드 & 실행 → Flame 애니메이션 프레임이 순서대로 재생되는지 확인

**확인 기준:** 파티클마다 Flame 스프라이트가 프레임 순으로 재생되고 끝나면 소멸한다. ✓

---

## Stage 4 — EmitterConfig + 방향/속도 분산

**목표:** 발사 방향, 속도, 수명을 `EmitterConfig`로 제어한다.

- [x] `EmitterConfig` 구조체 정의
  ```cpp
  struct EmitterConfig {
      mu::Vec3 position;
      mu::Vec3 direction  = {0, 1, 0};
      float    spread     = 0.3f;        // cone half-angle (radians)
      float    speedMin   = 1.f, speedMax   = 3.f;
      float    lifetimeMin= 0.5f, lifetimeMax= 1.5f;
      const SpriteAnimationClip* pClip = nullptr;
  };
  ```
- [x] `sampleConeDirection(axis, spread)` 헬퍼 복원
- [x] `emit(const EmitterConfig&, int count)` 시그니처로 변경
- [x] 빌드 & 실행 → 방향과 spread 값을 바꾸며 cone 모양이 달라지는지 확인

**확인 기준:** spread=0이면 일직선, spread=π/2이면 반구형으로 퍼진다. ✓

---

## Stage 4-1 — 파티클 재사용 시 애니메이션 상태 초기화 버그 수정 ✓

**문제:** 풀에서 파티클이 재사용될 때 `SpriteAnimation::init()`이 클립 포인터만 설정하고
`currFrameIdx_`, `timeAcc_`, `done_`을 초기화하지 않아, 이전 애니메이션의 마지막 프레임이
새 파티클에 잔상으로 남는 현상 (F키 연타 시 재현).

**수정:** `spriteAnimation.cpp`의 `init()` 첫 줄에 상태 초기화 3줄 추가.

```cpp
currFrameIdx_ = 0u;
timeAcc_      = 0ms;
done_         = false;
```

**확인 기준:** F키 연타 시 파티클이 항상 첫 프레임부터 재생된다. ✓

---

## Stage 5 — 시각적 보간 (tint / size / drag)

**목표:** 파티클 생성부터 소멸까지 색과 크기를 부드럽게 보간한다.

- [x] `EmitterConfig`에 추가
  ```cpp
  mu::Vec3 tintBegin = {1,1,1}, tintEnd = {1,1,1};
  float    sizeBegin = 1.f,    sizeEnd  = 0.f;
  float    drag      = 0.f;
  mu::Vec3 gravity   = {0, -9.8f, 0};
  ```
- [x] `Particle`에 `tintBegin, tintEnd, sizeBegin, sizeEnd, drag` 저장
- [x] `update()` 에서 `t = 1 - lifetime/maxLifetime`으로 보간
  - `size = lerp(sizeBegin, sizeEnd, t)`
  - `tint = lerp(tintBegin, tintEnd, t)`
  - `vel *= max(0, 1 - drag * dt)`
  - `anim.setScale`, `anim.setTint` 적용
- [x] 빌드 & 실행 → 파티클이 점점 작아지고 색이 변하는지 확인

**확인 기준:** 불꽃이 타오르다가 점점 투명해지며 사라지는 느낌이 난다. ✓

---

## Stage 6 — 풀 크기 & 최종 정리

**목표:** pool 크기를 늘리고 코드를 정리한다.

- [x] `kMaxParticles` 4096으로 증가
- [x] `emit()` 이 null clip이나 count<=0일 때 no-op 보장 (이미 구현됨)
- [x] `kMaxParticles` 를 헤더 전역이 아닌 클래스 내 `static constexpr`로 이동 (ODR 위반 방지)
- [x] `particleSystem.hpp/cpp` 코드 최종 리뷰
- [x] `docs/TODO.md`에 파티클 시스템 완료 체크

> `game.cpp` 테스트 코드(F키 emit)는 게임 이벤트 연결 전 유지.
> 실제 이벤트(타격·폭발 등) 연결은 게임플레이 시스템이 갖춰진 이후 별도 작업.

---

## 불꽃 파티클 고도화 (Flame Particle Polish)

불꽃 이미지에 걸맞는 시각적 완성도를 높이는 단계.
Unity Particle System의 Shape, Start Rotation, Additive Blending을 참고하여 구현한다.
불필요한 범용 기능은 구현하지 않고 불꽃 연출에 필요한 것만 추린다.

---

### Stage 7 — Additive Blending (가산 혼합)

**목표:** 겹친 파티클이 더 밝게 보이도록 블렌드 스테이트를 가산 혼합으로 변경한다.

Unity에서 불꽃 파티클은 Rendering Mode → Additive로 설정되어 겹칠수록 밝아지는 효과를 낸다.

- [x] billboard 렌더 패스에 additive blend PSO 옵션 추가
  - `SrcBlend = ONE, DestBlend = ONE` (가산 혼합)
  - 기존: `AlphaToCoverageEnable = true`, `BlendEnable = false`
  - additive PSO: `DepthWriteMask = ZERO` (depth read 유지, write 비활성)
- [x] `EmitterConfig`에 `bool additiveBlend = true` 추가
- [x] 렌더 시 해당 플래그에 따라 PSO 선택 (alpha / additive 분기)
  - `DrawEvent`에 `bool additive` 추가, 정렬 시 non-additive → additive 순
  - `BillboardPipeline::Dispatcher`가 두 PSO 보유, 경계에서 `SetPipelineState` 전환
- [x] 빌드 & 실행 → 불꽃이 겹치는 곳이 밝게 타오르는지 확인

**확인 기준:** 파티클이 많이 겹치는 중심부가 더 밝고 강렬하게 보인다. ✓

**구현 요약:**
- `shader.cpp`: `createBillboardShaderAdditive()` 추가 — `SrcBlend = ONE, DestBlend = ONE`, `AlphaToCoverageEnable = false`, `DepthWriteMask = ZERO` (depth test 유지, write 비활성으로 파티클끼리 서로 가리지 않음)
- `billboardPipeline.hpp/cpp`: `DrawEvent`에 `bool additive` 추가. `operator<=>` 수정으로 non-additive → additive 순 정렬 보장. `Dispatcher`가 두 PSO를 보유하고 경계에서 `SetPipelineState()` 전환 (싱글/멀티스레드 모두 대응)
- `spriteAnimation.hpp/cpp`: `additive_` 필드 및 `setAdditive()` 추가, `DrawEvent` 생성 시 전달
- `particleSystem.hpp/cpp`: `EmitterConfig`에 `bool additiveBlend = true` 추가, `Particle`에 `bool additive` 저장, `emit()`/`render()`에서 반영

---

### Stage 8 — Start Rotation (초기 회전)

**목표:** 파티클 생성 시 랜덤한 초기 회전값을 부여해 시각적 다양성을 높인다.

- [x] `EmitterConfig`에 추가
  ```cpp
  float startRotationMin = 0.f;   // radians
  float startRotationMax = 0.f;
  ```
- [x] `Particle`에 `float rotation` 저장
- [x] `emit()` 에서 `[startRotationMin, startRotationMax]` 범위 랜덤 샘플링
- [x] `SpriteAnimation::setRotation(float rad)` 추가, billboard GS에서 quad 생성 시 회전 반영
- [x] **billboard.hlsl / billboardPipeline.cpp 회전 미적용 버그 수정** (아래 상세 참조)
- [ ] 빌드 & 실행 → 파티클마다 방향이 다른지 확인

**발견된 문제점:**

- [x] **[BUG] rotation 미적용** *(수정 완료)* — 원인 분석 및 수정 완료. 아래 "Stage 8-1" 참조.
- [ ] **[BUG] Skybox에 파티클이 가려짐** — additive 파티클임에도 skybox depth가 파티클보다 앞에 써지는 문제. Stage 7에서 `DepthWriteMask = ZERO`로 설정했으나 skybox depth pass 순서 또는 far-plane 설정 확인 필요.

**확인 기준:** 파티클이 각각 다른 각도로 출력되어 자연스러운 불꽃 모양이 된다.

---

### Stage 8-1 — billboard 회전 미적용 버그 분석 및 수정 ✓

**핵심 원인:** 회전 수식이 잘못된 것이 아니라, 대부분의 파티클이 **GS에 도달하기 전에 하드웨어에 의해 컬링**되고 있었음. 회전이 안 보인 게 아니라 파티클 자체가 렌더링되지 않았던 것.

#### [CRITICAL] `SV_Position`에 world-space 좌표 저장 (`billboard.hlsl` L17, L48)
```hlsl
// 수정 전 (버그)
struct VSOutput { float4 pos : SV_Position; ... };
ret.pos = mul(float4(position, 1.0f), instance.world);  // VP 변환 없음
```

`SV_Position`은 GPU 하드웨어가 frustum clipping에 사용하는 특수 semantic이다.
VS에서 `SV_Position`에 쓰는 순간 하드웨어는 그것이 clip-space 좌표라고 가정한다.
DX12 clip-space 유효 범위: `-w ≤ x,y ≤ w`, `0 ≤ z ≤ w` (w=1일 때 z는 [0,1]).
world-space 위치 `(10, 0, 5, 1)`은 z=5 > w=1이므로 near/far clipping에 걸려 제거된다.
결과적으로 원점 근처(world xyz가 [-1,1] 이내)의 파티클만 GS에 도달하고 나머지는 모두 컬링.

```hlsl
// 수정 후
struct VSOutput { float3 worldPos : TEXCOORD0; ... };
ret.worldPos = mul(float4(position, 1.0f), instance.world).xyz;
```

GS가 있는 파이프라인에서 VS의 역할은 world-space 변환까지만이고,
clip-space 변환(`matViewProj`)은 반드시 GS 출력에서 수행해야 한다.

#### [MEDIUM] `_m00`만으로 scale 추출 (`billboard.hlsl` L49)

```hlsl
// 수정 전 (버그)
ret.size = size.x * instance.world._m00;
// world matrix에 rotation이 있으면 _m00 = scaleX * cos(θ) ≠ scaleX
```

```hlsl
// 수정 후
float scaleX = length(float3(instance.world._m00, instance.world._m10, instance.world._m20));
ret.size = size * scaleX;
```

#### [LOW] `cross(vUP, vLook)` NaN 위험 (`billboard.hlsl` L61)

카메라가 파티클 바로 위/아래에 있을 때 `cross((0,1,0), (0,±1,0)) = (0,0,0)` → `normalize` → NaN.

```hlsl
// 수정 후
float3 worldUp = (abs(vLook.y) < 0.999f) ? float3(0,1,0) : float3(1,0,0);
```

#### [BUG] 멀티스레드 업데이트 경로에서 rotation 누락 (`billboardPipeline.cpp` L494)

`addJobUpdate()`의 람다에서 `PerInstanceData` 초기화 시 `.rotation` 필드가 빠져있었음.
싱글스레드 경로는 정상이었고 멀티스레드 경로에서만 rotation이 항상 0.

```cpp
// 수정 전 (버그)
return BillboardShader::PerInstanceData{
    .world = mu::transpose( drawEvent.world ).getXmf(),
    // .rotation 누락
};

// 수정 후
return BillboardShader::PerInstanceData{
    .world    = mu::transpose( drawEvent.world ).getXmf(),
    .rotation = drawEvent.rotation,
};
```

#### [NAMING] `cameraPosV` → `cameraPosW` (`shader.hpp`, `billboardPipeline.cpp`)

"V" suffix는 view-space 관행적 표기이지만 실제로는 world-space 카메라 위치를 담고 있었음.
혼동 방지를 위해 `cameraPosW`로 rename.

**수정된 파일:**
- `billboard.hlsl` — VSOutput semantic, VS 로직, GS 로직 수정
- `shader.hpp` — `BillboardShader::PerFrameData::cameraPosV` → `cameraPosW`
- `billboardPipeline.cpp` — 필드명 rename, `addJobUpdate` rotation 누락 수정

---

### Stage 9 — Shape: 발사 영역 제어

**목표:** Emitter가 파티클을 방출하는 영역 형태를 설정할 수 있게 한다.
Unity Shape 컴포넌트 중 불꽃에 필요한 타입만 구현한다.

```cpp
enum class EmitterShape { Point, Edge };
```

- **Point** (기본값): 현재 구현과 동일, 한 점에서 emit
- **Edge**: 선분 위의 랜덤 위치에서 emit — 불꽃 바닥이 넓게 퍼지는 효과

- [ ] `EmitterShape` enum 추가
- [ ] `EmitterConfig`에 추가
  ```cpp
  EmitterShape shape      = EmitterShape::Point;
  float        edgeLength = 1.f;        // Edge 타입일 때 선분 길이
  mu::Vec3     edgeDir    = {1, 0, 0};  // Edge 방향
  ```
- [ ] `emit()` 에서 shape에 따라 spawn 위치 계산
  - `Point`: `config.position`
  - `Edge`: `config.position + edgeDir * rand(-edgeLength/2, edgeLength/2)`
- [ ] 빌드 & 실행 → Edge 타입에서 선분 위에 고르게 파티클이 생성되는지 확인

**확인 기준:** Edge 타입으로 설정하면 불꽃 바닥이 선형으로 퍼지며 타오르는 모양이 된다.

---

### Stage 10 — Continuous Emit + 불꽃 프리셋 튜닝

**목표:** 자동으로 계속 방출하는 연속 emit을 구현하고, 불꽃다운 파라미터 기본값을 잡는다.

**Continuous Emit:**

- [ ] `EmitterConfig`에 `float emitRate = 0.f` 추가 (particles/sec, 0이면 수동 emit)
- [ ] `ParticleSystem::update(dt)` 에서 `emitRate > 0`이면 누적 시간으로 자동 emit
  ```cpp
  emitAccum_ += emitRate * dt;
  int count = (int)emitAccum_;
  emitAccum_ -= count;
  if (count > 0) emit(config_, count);
  ```
- [ ] F키는 수동 burst emit으로 유지, 별도 emitter로 continuous emit 테스트

**불꽃 프리셋 (EmitterConfig 튜닝 목표값):**

- [ ] `gravity = {0, -1.f, 0}` — 중력 거의 없음, 불꽃은 위로 타오름
- [ ] `drag = 0.8f` — 공기 저항으로 자연스럽게 감속
- [ ] `direction = {0, 1, 0}`, `spread = 0.2f` — 좁은 cone으로 위쪽 집중
- [ ] `speedMin = 0.5f, speedMax = 2.f` — 천천히 위로 올라가는 속도
- [ ] `sizeBegin = 1.f, sizeEnd = 0.3f` — 타오르다 작아지는 느낌
- [ ] `tintBegin = {1, 1, 1}, tintEnd = {1, 0.3f, 0}` — 흰→주황 계열 fade
- [ ] 빌드 & 실행 → 불꽃이 자연스럽게 타오르는지 확인

**확인 기준:** 횃불처럼 위로 타오르며 자연스럽게 소멸하는 불꽃이 연출된다.

---

## 메모

- billboard 셰이더: `billboard.hlsl` (VS→GS→PS, GS에서 camera-facing quad 생성)
- Flame 리소스: `resources/Sprites/Flame/Flame_0~8.dds`, clip: `AssetManager::flameAnimation()`
- `SpriteAnimation::done()` — 애니메이션 클립이 끝나면 true (non-looping 가정)
- Stage 1~2는 `SpriteAnimation` 없이 raw GFX drawcall로 직접 확인하는 게 디버깅에 유리함

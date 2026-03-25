# ParticleSystem 클래스 레퍼런스

**파일:** `client/particleSystem.hpp` / `particleSystem.cpp`

---

## 클래스 구조

```
EmitterConfig        — emit 동작 전체를 기술하는 파라미터 묶음
Particle             — 풀 내 파티클 1개의 런타임 상태
ParticleSystem       — 풀 관리 + update + render
```

---

## EmitterConfig

| 필드 | 타입 | 기본값 | 설명 |
|---|---|---|---|
| `position` | `Vec3` | — | 발사 원점 |
| `direction` | `Vec3` | `(0,1,0)` | 발사 축 (world-space) |
| `spread` | `float` | `0.3f` | cone 반각 (rad). 0=직선, π/2=반구 |
| `speedMin/Max` | `float` | `1 / 3` | 초기 속도 범위 |
| `lifetimeMin/Max` | `float` | `0.5 / 1.5` | 수명 범위 (초) |
| `tintBegin/End` | `Vec3` | `(1,1,1)` | 생성→소멸 색 보간 |
| `sizeBegin/End` | `float` | `1 / 0` | 생성→소멸 크기 보간 |
| `drag` | `float` | `0` | 공기 저항 (속도에 매 프레임 곱함) |
| `gravity` | `Vec3` | `(0,-9.8,0)` | 가속도 (m/s²) |
| `startRotationMin/Max` | `float` | `0 / 0` | 초기 회전 랜덤 범위 (rad) |
| `pClip` | `const SpriteAnimationClip*` | `nullptr` | 스프라이트 애니메이션 클립 |
| `shape` | `EmitterShape` | `Point` | 발사 영역 형태 |
| `edgeLength` | `float` | `1` | Edge 타입일 때 선분 길이 |
| `edgeDir` | `Vec3` | `(1,0,0)` | Edge 방향 |
| `additiveBlend` | `bool` | `true` | 가산 혼합 여부 |
| `emitRate` | `float` | `0` | 연속 emit 속도 (particles/sec). 0=수동 |

### EmitterShape

```cpp
enum class EmitterShape { Point, Edge };
```

- **Point**: 단일 점에서 emit
- **Edge**: `position` 기준으로 `edgeDir` 방향 선분 위 랜덤 위치에서 emit

---

## Particle (내부 구조체)

```cpp
struct Particle {
    Vec3            pos, vel;
    float           lifetime, maxLifetime;
    Vec3            tintBegin, tintEnd;
    float           sizeBegin, sizeEnd, drag;
    Vec3            gravity;
    SpriteAnimation anim;
    float           rotation;
    bool            additive;
    // active 필드 없음: pool_[0..activeCount_-1] 이 항상 활성 상태
};
```

compact array 방식으로 관리된다. `pool_[0..activeCount_-1]`이 항상 활성 파티클이며,
만료 시 swap-remove로 마지막 요소와 교체 후 `activeCount_`를 감소한다.

---

## ParticleSystem API

```cpp
class ParticleSystem {
public:
    static constexpr int kMaxParticles = 4096;

    // 즉시 count개 파티클 emit.
    // emit 시 SpriteAnimType::Once / Loop 클립의 애니메이션 속도(speed)를
    // 파티클 lifetime에 맞게 자동 조정한다.
    void emit(const EmitterConfig& config, int count);

    // 연속 emit 시작/중지 (config.emitRate particles/sec)
    void startContinuous(const EmitterConfig& config);
    void stopContinuous();

    // 매 프레임 호출
    void update(Seconds dt);
    void render(GFX& gfx) const;

    // 현재 활성 파티클 수 (디버깅/프로파일링용)
    int activeCount() const;
};
```

### 동작 흐름

```
update(dt)
  └─ continuous_ && emitRate > 0  →  누적 후 emit()
  └─ pool_[0..activeCount_-1] 순회 (compact array):
       vel  *= max(0, 1 - drag * dt)      // drag
       vel  += gravity * dt               // 중력
       pos  += vel * dt                   // 이동
       t     = 1 - lifetime/maxLifetime   // 0→1 진행도
       size  = lerp(sizeBegin, sizeEnd, t)
       tint  = lerp(tintBegin, tintEnd, t)
       anim.update / setPos / setScale / setTint
       ※ emit 시 speed 자동 설정:
           Once  → speed = duration / lifetime
           Loop  → speed = N * duration / lifetime  (N = round(lifetime/duration), min 1)
       lifetime 소진 or anim.done()
         → swap-remove: pool_[i] ← move(pool_[activeCount_-1])
         → --activeCount_, i 재처리

render(gfx) const
  └─ pool_[0..activeCount_-1] 순회
  └─ 각 Particle: anim.render(gfx)
       non-additive 먼저, additive 나중에 정렬 (BillboardPipeline 내부)
```

---

## 사용 예시

### 수동 burst emit

```cpp
EmitterConfig cfg;
cfg.position      = firePos;
cfg.pClip         = AssetManager::flameAnimation();
cfg.additiveBlend = true;
cfg.emitRate      = 0.f;  // 수동
particleSystem_.emit(cfg, 20);
```

### 연속 emit (불꽃 프리셋)

```cpp
EmitterConfig flame;
flame.position       = torchPos;
flame.pClip          = AssetManager::flameAnimation();
flame.direction      = {0, 1, 0};
flame.spread         = 0.2f;
flame.speedMin       = 0.5f;   flame.speedMax    = 2.f;
flame.gravity        = {0, -1.f, 0};
flame.drag           = 0.8f;
flame.sizeBegin      = 1.f;    flame.sizeEnd     = 0.3f;
flame.tintBegin      = {1,1,1};flame.tintEnd     = {1,0.3f,0};
flame.startRotationMin = 0.f;  flame.startRotationMax = 6.28f;
flame.shape          = EmitterShape::Edge;
flame.edgeLength     = 0.5f;
flame.additiveBlend  = true;
flame.emitRate       = 30.f;   // 30 particles/sec
particleSystem_.startContinuous(flame);

// 매 프레임
particleSystem_.update(dt);
particleSystem_.render(gfx);

// 중지
particleSystem_.stopContinuous();
```

---

## 관련 파일

| 파일 | 역할 |
|---|---|
| `spriteAnimation.hpp/cpp` | 개별 파티클의 스프라이트 재생, tint/scale/rotation 제어 |
| `billboardPipeline.hpp/cpp` | DrawEvent 정렬 (non-additive→additive), PSO 전환 |
| `billboard.hlsl` | VS(world변환) → GS(camera-facing quad 생성, rotation 적용) → PS |
| `shader.cpp` | `createBillboardShaderAdditive()` — additive blend PSO 생성 |

---

## 주의사항

- `pClip`이 `nullptr`이거나 `count <= 0`이면 `emit()`은 no-op.
- 풀 크기(4096)를 초과하면 `overwriteCursor_`가 round-robin으로 기존 활성 파티클 슬롯을 덮어씀.
- `SpriteAnimation::init()`은 호출 시 `currFrameIdx_`, `timeAcc_`, `done_`을 초기화한다 (재사용 시 잔상 방지).
- `additive` 플래그는 `emit()` 시 한 번 설정되며 수명 동안 불변이다.
- billboard VS는 world-space까지만 변환; clip-space 변환은 GS에서 수행.
- 카메라가 파티클 바로 위/아래에 있을 때 NaN 방지: `worldUp`을 `(1,0,0)`으로 fallback.
- `emit()` 시 `SpriteAnimType::Once` / `Loop` 클립의 `speed`는 lifetime에 맞게 자동 설정된다.
  `Loop` 타입은 lifetime 동안 정수 N번의 완전한 사이클이 재생되도록 계산하여,
  파티클 소멸 시 애니메이션이 루프 경계 근처에서 끝나도록 한다.
  `RandomAdvance` 타입은 랜덤 특성상 자동 설정 없음 (speed = 1.f 유지).

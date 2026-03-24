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

- [ ] `EmitterConfig`에 추가
  ```cpp
  mu::Vec3 tintBegin = {1,1,1}, tintEnd = {1,1,1};
  float    sizeBegin = 1.f,    sizeEnd  = 0.f;
  float    drag      = 0.f;
  mu::Vec3 gravity   = {0, -9.8f, 0};
  ```
- [ ] `Particle`에 `tintBegin, tintEnd, sizeBegin, sizeEnd, drag` 저장
- [ ] `update()` 에서 `t = 1 - lifetime/maxLifetime`으로 보간
  - `size = lerp(sizeBegin, sizeEnd, t)`
  - `tint = lerp(tintBegin, tintEnd, t)`
  - `vel *= max(0, 1 - drag * dt)`
  - `anim.setScale`, `anim.setTint` 적용
- [ ] 빌드 & 실행 → 파티클이 점점 작아지고 색이 변하는지 확인

**확인 기준:** 불꽃이 타오르다가 점점 투명해지며 사라지는 느낌이 난다.

---

## Stage 6 — 풀 크기 & 최종 정리

**목표:** pool 크기를 늘리고 코드를 정리한다.

- [ ] `kMaxParticles` 4096으로 증가 (단계적으로 128 → 1024 → 4096 테스트)
- [ ] `game.cpp` 테스트 코드 정리, 실제 게임 이벤트(타격, 폭발 등)와 연결
- [ ] `emit()` 이 null clip이나 count<=0일 때 no-op 보장
- [ ] `particleSystem.hpp/cpp` 코드 최종 리뷰
- [ ] `docs/TODO.md`에 파티클 시스템 완료 체크

---

## 메모

- billboard 셰이더: `billboard.hlsl` (VS→GS→PS, GS에서 camera-facing quad 생성)
- Flame 리소스: `resources/Sprites/Flame/Flame_0~8.dds`, clip: `AssetManager::flameAnimation()`
- `SpriteAnimation::done()` — 애니메이션 클립이 끝나면 true (non-looping 가정)
- Stage 1~2는 `SpriteAnimation` 없이 raw GFX drawcall로 직접 확인하는 게 디버깅에 유리함

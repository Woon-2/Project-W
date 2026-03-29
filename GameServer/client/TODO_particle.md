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

## 완료된 추가 작업

- **Color over Lifetime 구현 (Unity 모델 기반)**

  **설계 원칙**
  ```
  finalColor(t) = startColor * colorOverLifetime.evaluate(t)  // RGBA component-wise
  최종 픽셀    = finalColor × texture.rgba
  ```
  - `startColor` : 파티클 생성 시 고정 RGBA (베이스 색)
  - `colorOverLifetime` : startColor에 곱해지는 RGBA multiplier 커브 (절대 색이 아님)
  - 기본값 `ColorGradient::constant({1,1,1,1})` → 곱해도 변화 없음

  **구현 내용**
  - `gfxUtil.hpp/cpp`: `ColorKey` + `ColorGradient` 신설
    - `evaluate(float t)` — piecewise linear RGBA 보간, keys 비어있으면 `{1,1,1,1}` 반환
    - `static constant(Vec4)` / `static linear(Vec4 begin, Vec4 end)`
  - `particleSystem.hpp`: `EmitterConfig` / `Particle` 재설계
    - `tintBegin` / `tintEnd` (Vec3) 제거
    - `startColor: mu::Vec4` + `colorOverLifetime: ColorGradient` 추가
  - `particleSystem.cpp`: `emit()` / `update()` 수정
    - `emit()`: `p.startColor` / `p.colorOverLifetime` 복사
    - `update()`: `finalColor = p.startColor * p.colorOverLifetime.evaluate(t)` → `anim.setTint(finalColor)`
  - `spriteAnimation.hpp`: `tint_` / `setTint()` / `tint()` Vec3 → Vec4
  - `billboardPipeline.hpp`: `DrawEvent::tint` Vec3 → Vec4, 기본값 `{1,1,1,1}`
  - `shader.hpp`: `BillboardShader::Material::tint` XMFLOAT3 → XMFLOAT4
    - cbuffer 레이아웃: `int4 idxTex`(16B) + `float4 tint`(16B) = 32B
  - `billboard.hlsl`: `Material.tint` float3 → float4
    - PSMain: `return float4(src.xyz * material.tint.rgb, src.a * material.tint.a);`
  - `standalone/game.cpp`: 마이그레이션
    - 불꽃: `tintBegin/tintEnd` → `startColor = {1, 0.4, 0, 1}` (기본 gradient 사용)
    - 연기: `startColor = {0.8, 0.8, 0.8, 1}` + 3-key gradient (밝은 회색→어두운 회색, alpha 0→1→0)

  **smoke colorOverLifetime 파라미터**
  ```cpp
  smokeEmitterConfig_.startColor = {0.8f, 0.8f, 0.8f, 1.f};
  smokeEmitterConfig_.colorOverLifetime = ColorGradient{
      .keys = {
          {0.0f, {1.f,  1.f,  1.f,  0.f}},   // RGB*1 (밝음),  A*0 (투명)
          {0.5f, {0.5f, 0.5f, 0.5f, 1.f}},   // RGB*0.5 (중간), A*1 (불투명)
          {1.0f, {0.f,  0.f,  0.f,  0.f}},   // RGB*0 (검정),  A*0 (투명)
      }
  };
  ```
  기대 결과: 안 보임 → 밝은 회색(반투명) → 중간 회색(불투명) → 어두운 회색(반투명) → 안 보임

  **검증 완료**
  - [x] 불꽃 파티클 regression 확인 — 시각 변화 없음
  - [x] smoke 파티클 시각 결과 확인 — 밝은 회색→어두운 회색 페이드 동작 확인

- **cbuffer 레이아웃 버그 수정**
  - `shader.hpp` `BillboardShader::PerDrawcallData`: `pad_` 필드 추가
  - `billboard.hlsl` cbuffer: `float pad0` 명시적 패딩 추가
  - 원인: `Material::tint`를 XMFLOAT3(12B)→XMFLOAT4(16B)로 확장하면서 `uvScale` offset이
    C++(44) vs HLSL auto-pad(48)로 4바이트 불일치 → 파티클이 회색 직사각형으로 깨지는 현상
  - 결과 레이아웃: `Material`(32B) + `firstInstanceOffset`(4B) + `uvOffset`(8B) + `pad_`(4B) + `uvScale`(8B) = 56B

---

## 완료된 추가 작업

- **additiveBlend PSO 버그 수정**
  - `shader.cpp` `createBillboardShader()`: non-additive PSO 블렌드 설정 수정
    - `AlphaToCoverageEnable=true, BlendEnable=false` → `BlendEnable=true, SrcAlpha/InvSrcAlpha, DepthWriteMask=ZERO`
    - 기존 설정은 알파를 무시하고 이진 컷으로 처리하여 가산 혼합과 구별이 불가능했음
  - `billboardPipeline.cpp` `drawMultiThreaded()`: `additiveStart` 계산 버그 수정
    - `std::ranges::lower_bound(events, false, {}, [](e){ return !e.additive; })` → `std::ranges::partition_point(events, [](e){ return !e.additive; })`
    - 내림차순 프로젝션을 `lower_bound`에 넘겨 항상 0을 반환했던 UB 수정
    - 결과: 멀티스레드 모드에서 non-additive 파티클도 항상 additive PSO로 렌더링되던 버그 해결

---

## 완료된 추가 작업: Mesh Particle — 검기 이펙트 MVP

자세한 구현 내용은 `docs/meshParticleSystem.md` 참조.

**완료된 항목:**
- `.meshbin` v1 포맷 정의 및 `tools/MeshBinExporter.cs` (Unity Editor 익스포터)
- `loadMeshBin()` (`mesh.hpp/.cpp`): magic/version 검증, Position(Slot0) + UV(Slot1) VB 분리
- `MeshParticleShader` 네임스페이스 + `meshParticle.hlsl` + `createMeshParticleShader()` (alpha blend PSO)
- `MeshParticlePipeline::Dispatcher`: DrawEvent당 1 drawcall, alpha blend 전용
- `GFX` 통합: `drawEventsMeshParticlePipeline_`, `render()` 내 Dispatcher 호출
- `MeshParticleSystem`: emit/update/render, 최대 256 파티클, colorOverLifetime 지원
- `game.cpp`: 좌클릭 시 검기 이펙트 emit, alpha fade 동작 확인

**미완료 (후속):**
- [ ] Unity 3D Start Rotation (X:80°, Y:180°, Z:180°) 보정 회전 적용
- [ ] depth test / 반투명 정렬 검증
- [ ] Additive PSO, DrawEvent 배칭 최적화

---

## Unity Particle System과의 비교 분석

### 구현된 기능 현황

| 모듈 | 현재 상태 | Unity 대비 수준 |
|------|-----------|-----------------|
| **Main (기본)** | startColor/gravity/maxParticles 있음, duration/looping 없음 | ~40% |
| **Emission** | emitRate (time-based), 수동 버스트 | ~50% |
| **Shape** | Point, Edge | ~15% |
| **Color over Lifetime** | 완전 구현 (Unity 모델 동일) | ~100% |
| **Size over Lifetime** | 선형 lerp만 | ~60% |
| **Texture Sheet Animation** | Loop/Once/RandomAdvance 완전 구현 | ~90% |
| **Renderer** | Billboard + Additive/Alpha blend | ~40% |
| **Physics** | 중력(상수), 드래그 | ~30% |
| **Rotation** | 스폰 시 랜덤 각도만, 생애 중 회전 없음 | ~20% |

---


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

- **additiveBlend PSO 버그 수정**
  - `shader.cpp` `createBillboardShader()`: non-additive PSO 블렌드 설정 수정
    - `AlphaToCoverageEnable=true, BlendEnable=false` → `BlendEnable=true, SrcAlpha/InvSrcAlpha, DepthWriteMask=ZERO`
    - 기존 설정은 알파를 무시하고 이진 컷으로 처리하여 가산 혼합과 구별이 불가능했음
  - `billboardPipeline.cpp` `drawMultiThreaded()`: `additiveStart` 계산 버그 수정
    - `std::ranges::lower_bound(events, false, {}, [](e){ return !e.additive; })` → `std::ranges::partition_point(events, [](e){ return !e.additive; })`
    - 내림차순 프로젝션을 `lower_bound`에 넘겨 항상 0을 반환했던 UB 수정
    - 결과: 멀티스레드 모드에서 non-additive 파티클도 항상 additive PSO로 렌더링되던 버그 해결


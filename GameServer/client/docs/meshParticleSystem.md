# Mesh Particle System

방향이 고정된 3D 메시를 파티클로 렌더링하는 시스템.
Billboard(카메라 facing Quad)로는 표현할 수 없는 검기(sword slash) 같은 방향성 이펙트에 사용한다.

---

## 관련 파일

| 파일 | 역할 |
|------|------|
| `meshParticle.hlsl` | Vertex/Pixel Shader |
| `meshParticlePipeline.hpp/.cpp` | DrawEvent → GPU 제출 |
| `meshParticleSystem.hpp/.cpp` | 파티클 생명주기 관리 |
| `mesh.hpp/.cpp` | `.meshbin` 로더 (`loadMeshBin()`) |
| `shader.hpp/.cpp` | PSO 생성 (`createMeshParticleShader()`) |
| `gfx.hpp/.cpp` | 파이프라인 통합 |
| `tools/MeshBinExporter.cs` | Unity Editor 익스포터 |

---

## .meshbin 포맷 (v1)

Unity 메시를 경량 바이너리로 익스포트하는 커스텀 포맷.

```
[u8×8]    magic = "MESHBIN\0"
[u8]      version = 1
[u32]     vertexCount
[u32]     indexCount
[float3 + float2] × vertexCount   (position 12B + uv 8B, stride=20B)
[u16] × indexCount
[u8]      texturePathLen
[char × texturePathLen]            (resources/Textures/ 기준 상대경로, / 구분자)
```

**익스포터 처리 규칙 (`tools/MeshBinExporter.cs`)**
- vertexCount > 65535 → export 실패 처리
- UV y-flip: `uv.y = 1 - uv.y` (Unity 좌하단 → DX12 좌상단)
- 좌표계 반전: 적용 안 함 (뒤집혀 보이면 `pos.z = -pos.z` + winding 반전)
- SubMesh: 첫 번째만 export

**로더 (`loadMeshBin()` in `mesh.hpp/.cpp`)**
- magic / version 검증
- VB 분리: `"{name}_VB_Position"` (float3, Slot0), `"{name}_VB_UV"` (float2, Slot1)
- 기존 `createBufferResource()` 재사용

---

## 셰이더 / PSO

### cbuffer 레이아웃

```
PerInstanceData (StructuredBuffer t0): float4x4 world(64B) + float4 tint(16B) = 80B
PerDrawcallData (b0):                  uint4 idxTex(16B) + uint firstInstanceOffset(4B) + uint3 pad(12B) = 32B
PerFrameData    (b1):                  float4x4 matViewProj(64B)
```

### VS / PS 처리

```hlsl
// VS
PerInstanceData inst = gInstances[firstInstanceOffset + input.instID];
float4 worldPos = mul(float4(input.position, 1.0f), inst.world);  // row-vector * row-major
ret.pos = mul(worldPos, matViewProj);

// PS
float4 src = sampleBindless(idxTex, input.uv);
return float4(src.rgb * input.tint.rgb, src.a * input.tint.a);
```

### PSO 설정 (`createMeshParticleShader()`)

- Topology: `D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST`
- CullMode: `NONE`
- DepthWriteMask: `ZERO`, DepthFunc: `LESS`
- Blend: SrcAlpha / InvSrcAlpha (alpha blend, additive 없음)

---

## MeshParticlePipeline

### DrawEvent

```cpp
struct DrawEvent {
    mu::Mat4x4     world;
    const Mesh*    pMesh;
    const SubMesh* pSubMesh;
    const Texture* pTex;
    mu::Vec4       tint     = { 1, 1, 1, 1 };
    int            renderOrder = 0;
};
```

- DrawEvent당 1 drawcall: `DrawIndexedInstanced(indexCount, 1, 0, 0, firstInstanceOffset)`
- Additive PSO 없음 — alpha blend 전용
- Multithreaded variants는 현재 single-threaded로 위임

### GPU 리소스

```cpp
struct Resources {
    StructuredBuffer    perInstanceData;   // t0
    ConstantBufferArray perDrawcallData;   // b0 (DrawEvent당 1슬롯)
    ConstantBuffer      perFrameData;      // b1
};
```

---

## MeshParticleSystem

### MeshEmitterConfig

```cpp
struct MeshEmitterConfig {
    mu::Vec3       position;
    mu::Mat4x4     rotation;         // 모든 파티클에 적용되는 고정 방향
    float          sizeBegin  = 1.f;
    float          sizeEnd    = 0.f;
    float          lifetimeMin = 0.3f;
    float          lifetimeMax = 0.5f;
    mu::Vec4       startColor = { 1, 1, 1, 1 };
    ColorGradient  colorOverLifetime = ColorGradient::constant({ 1, 1, 1, 1 });
    const Mesh*    pMesh    = nullptr;
    const SubMesh* pSubMesh = nullptr;
    const Texture* pTex     = nullptr;
    int            renderOrder = 0;
    float          emitRate = 0.f;   // particles/sec (0 = 수동 emit만)
};
```

### 파티클 풀

- 최대 256 파티클 (`kMaxParticles`)
- Compact array: `pool_[0..activeCount_-1]`이 항상 활성 상태
- 풀 초과 시: `overwriteCursor_`로 라운드로빈 덮어쓰기

### tint 계산 (Billboard 파티클과 동일한 정책)

```cpp
float t = 1.f - p.lifetime / p.maxLifetime;  // 0=스폰, 1=사망
mu::Vec4 tint = p.startColor * p.colorOverLifetime.evaluate(t);  // RGBA component-wise
```

### world 행렬 계산

```cpp
// world = Scale * Rotation * Translation
auto world = mu::scaleH({ size, size, size }) * p.rotation * mu::translate(p.pos);
```

각속도 없음 — `rotation`은 emit 시점에 고정.

### emit / update / render

- `emit(config, count)`: 파티클 생성, lifetime은 `[lifetimeMin, lifetimeMax]` 균등분포 샘플링
- `update(dt)`: lifetime 감소, 만료된 파티클은 swap-and-pop으로 제거
- `render(gfx)`: 활성 파티클마다 `gfx.addDrawEvent(MeshParticlePipeline::DrawEvent{...})` 제출

---

## GFX 통합

- `drawEventsMeshParticlePipeline_`: Billboard 섹션 바로 다음
- `addDrawEvent(MeshParticlePipeline::DrawEvent)` 오버로드
- `render()`: BillboardDispatcher 실행 직후 MeshParticleDispatcher 실행

---

## 검기 이펙트 연동 (standalone/game.cpp)

- `AssetManager::RequestMeshBinLoad`로 `swordSlashMesh_` / `swordSlashTex_` 로드
- 좌클릭(LButton) 시 `swordSlashSystem_.emit(config, 1)` 호출
- 이펙트 위치: `player_->renderState().pos + forward * 1.f + (0, 1, 0)` (허리 높이)
- colorOverLifetime: alpha 1→0 페이드아웃

---

## 미완료 / 후속

- [ ] **Unity 3D Start Rotation 보정 회전** (X:80°, Y:180°, Z:180°)
  - `swordSlashConfig_.rotation`에 보정 행렬 설정 후 emit 시 플레이어 방향과 합성
- [ ] depth test / 반투명 정렬 검증 (캐릭터·지형과 겹칠 때)
- [ ] 기존 불꽃/연기 파티클과 동일 프레임 렌더링 검증
- [ ] Additive PSO 추가
- [ ] DrawEvent 배칭 최적화
- [ ] Quaternion 각속도 적분
- [ ] `buildFanMesh()` 절차적 생성

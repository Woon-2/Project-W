- [X] 모델 메시별로 셰이더 별도 적용
- [X] 공격을 위한 충돌처리 구현 (AABB 기반, CombatSystem 서브시스템)
- [X] 몬스터 단순 AI 구현 (쿨타임 기반 AABB 교차 공격)
- [X] 공격에 해당하는 바운딩 볼륨 렌더링 가능하도록 구현
- [X] OBB 충돌처리 지원, 캐릭터 오브젝트들에 대해 기존 AABB 전부 OBB로 교체 (AABB는 특정 단순 사물에만 사용할 예정)
- [X] 유니티에서 추출한 바이너리 리소스를 로드해 Bounding Volume Hierarchy 구축 및 그를 통한 충돌처리로 업그레이드
  - 유니티에서 어떻게 추출했는지는 `unityScripts/ExtractUtil.cs`, `unityScripts/ModelExtractor.cs`, `unityScripts/MultiBoundingVolume.cs` 참조
  - BVH 노드가 bone에 종속된 경우 `bone.toDress * finalXformData()[i] * world` 체인으로 월드 변환
- [X] Height map 기반 Terrain 구현/Terrain Splat까지 (Unity에서 맵 추출)
  - TerrainExtractor.cs로 추출된 height.raw + terrain_meta.bin + terrain_manifest.bin + DDS 텍스처 로드
  - N×N 그리드 메시 생성 (중앙차분 법선, 32-bit IB), RGBA splat map 기반 레이어 블렌딩
  - terrain.hlsl: Lambertian + globalAmbient + PCF shadow, terrainPipeline.hpp/cpp: Dispatcher
- [X] level 바이너리에서 Terrain WorldTRS 읽어 월드 변환 적용
  - TerrainObject(Object 상속)와 TerrainData 분리: Object/Model 패턴과 동일
  - importNode() "Terrain" 분기 → TerrainObject 생성 → importTerrain() → update(0ms, 1.f)
  - TerrainPipeline::DrawEvent에 world 필드 추가, mainPass()에서 ev.world로 WVP 계산
- [X] Terrain Shadow 구현 (지형이 PBR 객체 위에 그림자를 드리움, PBR 객체의 그림자가 지형 위에 드리움)
  - terrainShadowMap.hlsl + TerrainShadowMapShader PSO 추가 (position-only, depth-only, NumRenderTargets=0)
  - TerrainPipeline::Dispatcher에 shadowPass/shadowPassMT/shadowUpdate/shadowDraw 추가
  - 공유 shadow map("ShadowMap") DSV에 지형 기하를 기록 → PBR mainPass에서 샘플링
- [X] Terrain roughness metallic도 unity에서 추출 및 렌더링 시 반영하도록 수정
  - 현재는 셰이더에 하드코딩되어 있음.
- [ ] Cascaded Shadow Mapping 구현
- [ ] 몬스터 AI 시스템 초안 구현(주변 배회, 피격 시 어그로)
- [ ] 장비 장착: 공격 모션에 무기도 같이 움직이도록 (필요하면 IK 구현)
- [ ] Rigid Body Physics 구현: 중력, 공기 저항, 마찰력 등 반영
- [ ] Software Occlusion(Culling)을 통한 최적화
- [ ] Active Ragdoll 시뮬레이션, 몬스터들의 움직임에 적용
- [ ] 시분할 애니메이션 제대로 적용
- [ ] Deferred Shading을 위한 GBuffer 설계
- [ ] Deferred Shading 구현
- [ ] 청크 구현 및 리소스 멀티스레드 동적 로딩 구현 (Seamless Openworld가 가능하도록)
- [ ] Image Based Lighting 구현


## Cascaded Shadow Mapping 구현안

### 기술 결정

**Cascade 개수: 4**
- 미터 단위 씬 (near=0.1m, far≈500m) 에서 근거리~원거리 품질 차이를 커버하려면 4개 필요
- 3개 시 원거리 cascade 하나가 ~25m~500m를 담당해 texel 밀도 급락

**Shadow Map 저장: Texture2DArray**
- `bindless.hlsli`에 이미 `gTex2DArrays[]` 및 `sampleCmpBindless` TextureArray 분기 구현됨
- `gfx.hpp`의 `srvTexArrayPool_` 그대로 활용 → 추가 bindless 인프라 불필요
- Atlas 대비 UV 누수 없음, 개별 Texture2D×4 대비 SRV 슬롯 1개, DSV 1개
- D3D12에서 array의 모든 slice는 동일 해상도 → `AssetConfigs` 단일 값으로 관리

**Draw Call 전략: Geometry Shader (`SV_RenderTargetArrayIndex`)**
- VS는 world-space `posW`만 출력
- GS가 cascade 루프로 각 삼각형을 N개 slice로 증폭 → `SV_RenderTargetArrayIndex = cascadeIdx`
- `DrawIndexedInstanced(idxCount, groupSize, ...)` — instanceCount에 cascade 배수 없음
- C++ 측 cascade 루프·DSV 재바인딩 없음, 단일 full-array DSV 바인딩
- D3D12 feature level 11.1 이상 필요 (현 프로젝트 12.0이므로 지원됨)

**Cascade Split 공식: Practical Split (λ=0.75)**
- 균등 분할만 쓰면 원거리 cascade가 너무 넓어짐
- 로그 분할만 쓰면 중거리 cascade가 과도하게 촘촘해짐
- `λ=0.75`가 게임용 표준 권장값

**Shadow Map Swimming 방지: Texel Snapping 필수**
- 카메라가 이동하면 light-space AABB 경계가 서브텍셀 단위로 흔들려 shadow edge shimmer 발생
- ortho 경계를 texel 크기로 round-to-texel 처리해 고정

---

### 수학적 계산

#### Cascade Split Plane

```
n = camera near,  f = camera far,  N = cascade count,  λ = split factor

C_log[i] = n * (f/n)^(i/N)
C_uni[i] = n + (f-n) * (i/N)
split[i]  = λ*C_log[i] + (1-λ)*C_uni[i]
```

예시 (near=0.1m, far=500m, N=4, λ=0.75):
```
split[0] = 0.1m   (near)
split[1] ≈ 3.5m
split[2] ≈ 25m
split[3] ≈ 120m
split[4] = 500m   (far)
```

split 값들은 view-space depth로 변환 후 `cascadeSplitsFarV[4]`로 shader에 전달.

#### 각 Cascade의 Light-Space Orthographic Frustum

```
1. split[i-1], split[i]로 한정한 카메라 frustum 8 코너 점 계산 (world space)
2. lightView = LookAt(origin, lightDir, worldUp) 로 변환
3. light-view AABB 계산
     left, right = AABB.minX, AABB.maxX
     bottom, top = AABB.minY, AABB.maxY
     nearZ, farZ = AABB.minZ - padding, AABB.maxZ
4. Texel Snapping (swimming 방지)
     texelSize = (right - left) / shadowMapResolution
     left   = floor(left   / texelSize) * texelSize
     right  = ceil (right  / texelSize) * texelSize
     bottom = floor(bottom / texelSize) * texelSize
     top    = ceil (top    / texelSize) * texelSize
5. XMMatrixOrthographicOffCenterLH(left, right, bottom, top, nearZ, farZ)
```

---

### 구조 변경 목록

#### AssetConfigs 신설 (gfx.hpp)

앞으로 `GFX::loadAssets`에 전달할 필요가 있는 모든 인자는 `AssetConfigs`에 모아서 전달한다.

```cpp
struct AssetConfigs {
    struct ShadowMapConfig {
        std::string key          = "ShadowMap";
        u32t        resolution   = 2048u;   // 권장: POT, texel snapping 계산 단순화
        u32t        cascadeCount = 4u;
        DXGI_FORMAT format       = DXGI_FORMAT_D32_FLOAT;
    } shadowMap;
    struct CascadeConfig {
        float nearZ  = 0.1f;
        float farZ   = 500.f;
        float lambda = 0.75f;
    } cascade;
};
void loadAssets(const AssetConfigs& configs = AssetConfigs{});
```

#### SharedResources::ShadowMapData

```cpp
struct ShadowMapData {
    Texture     tex;        // Texture2DArray SRV (CSM) 또는 Texture2D SRV (non-CSM)
    DXGI_FORMAT format;
    u32t        width, height;
    // sliceCount 없음: DSV/SRV 생성 시에만 필요, 런타임 불필요
    D3D12_CPU_DESCRIPTOR_HANDLE dsv;            // full-array DSV (단일 핸들)
    D3D12_RESOURCE_STATES       curState;
};
// addShadowMap(key,...,roomCnt): non-CSM Texture2D
// addCSMShadowMap(key,...,sliceCount,roomCnt): CSM Texture2DArray  ← 별도 함수, 통합 금지
// shadowMapData: unordered_map<string, vector<ShadowMapData>>  ← roomIdx로 인덱싱
// clearShadowMap(key, roomIdx, cmdList): 단일 full-array DSV로 ClearDepthStencilView
```

#### shader.hpp PerFrameData

PBRShader / TerrainShader 공통 변경:
```cpp
constexpr int MAX_CSM_CASCADES = 4;
struct PerFrameData {
    XMFLOAT3  globalAmbient;
    float     padding0;
    u32t      lightCnt;
    u32t      cascadeCount;                        // 추가
    XMUINT2   padding1;
    BindlessIndex idxShadowMap;
    float     cascadeSplitsFarV[MAX_CSM_CASCADES]; // 추가 (view-space far per cascade)
    XMFLOAT4X4 lightVP[MAX_CSM_CASCADES];          // 단일 → 배열
};

// ShadowMapShader::PerFrameData: cascadeIndex 제거, lightVP 배열화
struct PerFrameData {
    XMFLOAT4X4 lightVP[MAX_CSM_CASCADES];
};
```

#### Shadow VS+GS 변경 (terrainShadowMap.hlsl, shadowMap.hlsl, shadowMapSkinned.hlsl)

```hlsl
// VS: world-space position 출력만
struct VSOut { float4 posW : POSITION_W; };
VSOut VSMain(float3 position : POSITION /*, uint idxInst : SV_InstanceID (PBR만)*/) {
    VSOut ret;
    ret.posW = mul(float4(position, 1.f), worldOrInstance);
    return ret;
}

// GS: cascade별 증폭 + SV_RenderTargetArrayIndex
struct GSOut { float4 pos : SV_Position; uint sliceIdx : SV_RenderTargetArrayIndex; };
[maxvertexcount(3 * MAX_CSM_CASCADES)]
void GSMain(triangle VSOut input[3], inout TriangleStream<GSOut> stream) {
    for (uint cascade = 0; cascade < cascadeCount; ++cascade) {
        [unroll] for (uint v = 0; v < 3; ++v) {
            GSOut o;
            o.pos      = mul(input[v].posW, lightVP[cascade]);
            o.sliceIdx = cascade;
            stream.Append(o);
        }
        stream.RestartStrip();
    }
}
```

#### pbrLighting.hlsli — 기존 함수 변경 없이 CSM 전용 함수 추가

기존 `PCF`, `calcSingleShadow`는 일절 변경하지 않는다. 신규 함수만 추가:

```hlsl
// PCF_CSM: Texture2DArray slice용 9-tap PCF
// idx.z = cascade slice index (Texture2DArray 분기: bindless.hlsli gTex2DArrays[])
float PCF_CSM(int4 idx, float4 posL) { ... }  // 기존 PCF와 동일 패턴

// calcCSMShadow: cascade 선택 후 PCF_CSM 호출 (calcSingleShadow와 별개)
float calcCSMShadow(float3 posV) {
    uint  cascadeIdx = cascadeCount - 1;
    float depth      = posV.z;  // LH: +z is forward
    [unroll]
    for (uint i = 0; i < cascadeCount; ++i) {
        if (depth < cascadeSplitsFarV[i]) { cascadeIdx = i; break; }
    }
    float4 posL = mul(float4(posV, 1.f), lightVP[cascadeIdx]);
    int4   idx  = idxShadowMap;
    idx.z       = (int)cascadeIdx;
    return PCF_CSM(idx, posL);
}
```

호출부: `calcSingleShadow(posV, posL)` → `calcCSMShadow(posV)` 로 교체.
terrain.hlsl은 pbrLighting.hlsli를 TERRAIN_SHADER define으로 include 중이므로 자동 적용됨.

#### TerrainPipeline LightData 확장 (완료)

```cpp
struct LightData {
    mu::Vec3   dir;
    mu::Vec3   color;
    float      intensity = 1.f;
    // 단일 view/proj → CSM cascade 배열
    std::array<mu::Mat4x4, MAX_CSM_CASCADES> cascadeViews = {};
    std::array<mu::Mat4x4, MAX_CSM_CASCADES> cascadeProjs = {};
    XMFLOAT4   cascadeSplitsFarV = {};  // view-space far per cascade
    u32t       cascadeCount = MAX_CSM_CASCADES;
};
```

`gfx.hpp`: `std::vector<TerrainPipeline::LightData> lightDataTerrainPipeline_{}` (단일 값 금지).
`gfx.cpp::addLightData(const TerrainPipeline::LightData&)`: `.push_back()`.
`terrainPipeline.cpp` 내부에서 `lightData_[0]`을 shadow/main pass 에 사용.
shadow map SRV: `terrainPipeline.cpp::mainUpdate()`에서 `shadowMapData.at("ShadowMap")[roomIdx_].tex.idxSrv` 직접 조회 → game.cpp에서 별도 전달 불필요.

#### Light 클래스 — `updateCSMCascades` 추가 (완료)

```cpp
// light.hpp (실제 구현 시그니처)
void MU_CALLCONV updateCSMCascades(
    FXMMATRIX view, CXMMATRIX proj,
    const float* cascadeFarDistances, u32t cascadeCount, u32t shadowResolution
);
// 출력 accessor:
const std::array<mu::Mat4x4, MAX_CSM_CASCADES>& cascadeViews() const;
const std::array<mu::Mat4x4, MAX_CSM_CASCADES>& cascadeProjs() const;
XMFLOAT4 cascadeSplitsFarV() const;  // view-space far per cascade (XMFLOAT4로 packing)
u32t cascadeCount() const;
```

내부 구현: cascadeFarDistances 기반 frustum 8 코너 계산 → light-view AABB → texel snapping → ortho proj.

---

### 렌더 루프 다이어그램

```
GFX::loadAssets(AssetConfigs)
  |
  +-- SharedResources::ShadowMap::addShadowMap(key, ..., sliceCount=4)
        Texture2DArray [2048 x 2048 x 4 slices]
        DSV: D3D12_DSV_DIMENSION_TEXTURE2DARRAY (full array, 단일 핸들)
        SRV: srvTexArrayPool_ → gTex2DArrays[arrayIdx]

GFX::render()
  |
  +-- [Shadow Pass]
  |     getReadyAsDepthWrite("ShadowMap", roomIdx)     // full array → depth write
  |     clearShadowMap("ShadowMap", roomIdx)           // 단일 full-array DSV로 전체 clear
  |     PBRPipeline::Dispatcher::shadowPass()
  |       OMSetRenderTargets(0, nullptr, dsvFullArray)
  |       PerFrameData.lightVP[4] = cascadeVPs
  |       DrawIndexedInstanced(idxCnt, groupSize)      // cascade 배수 없음
  |         VS: posW 출력
  |         GS: cascade 루프 → SV_RenderTargetArrayIndex = cascadeIdx
  |     PBRSkinnedPipeline / TerrainPipeline  (동일 패턴)
  |
  +-- [Transition]
  |     getReadyAsShaderResource("ShadowMap")
  |
  +-- [Main Pass]
        PerFrameData {
          idxShadowMap         : gTex2DArrays[arrayIdx]  (.z = cascadeIdx in shader)
          cascadeCount         : 4
          cascadeSplitsFarV[4] : [3.5, 25, 120, 500]  (view-space)
          lightVP[4]           : cascadeVPs[0..3]
        }
        PS: calcCSMShadow(posV)
          cascade 선택: posV.z vs cascadeSplitsFarV
          PCF_CSM(idxShadowMap with .z=cascadeIdx, posL)

Light::updateCSMCascades(cam, splits, 4)
  for i in [0..4):
    frustum 8 corners → light-view → AABB → texel snapping
    cascadeVPs[i] = {lightView, orthoProj[i], splitFarV[i]}
```

---

### 디버그 시각화 ('c' 키)

셰이더 매크로 방식. 별도 pass/pipeline 불필요.

```hlsl
// pbrLighting.hlsli (calcCSMShadow 내에서 cascadeIdx 확정 후)
#ifdef CSM_DEBUG_VIS
static const float3 gCascadeColors[4] = {
    float3(1,0,0),  // cascade 0: 빨강 (≤ 3.5m)
    float3(0,1,0),  // cascade 1: 초록 (≤ 25m)
    float3(0,0,1),  // cascade 2: 파랑 (≤ 120m)
    float3(1,1,0),  // cascade 3: 노랑 (≤ 500m)
};
// illuminate() 최종 color에:
color.rgb = lerp(color.rgb, color.rgb * gCascadeColors[cascadeIdx], 0.5f);
#endif
```

C++ 측:
- `GFX::csmDebugVisualization_` (bool) + `GFX::toggleCsmDebugVisualization()`
- `game.cpp::processInput()`에서 `'C'` 키 토글
- Dispatcher: `csmDebugVisualization_` 시 `PBRShader_CsmDebug` / `TerrainShader_CsmDebug` PSO 사용
  (CSM_DEBUG_VIS macro define된 별도 컴파일 shader)

---

### 성능 참고

해상도 **2048×2048** 권장 (기존 2000×2000 → POT 정렬):
- 4 cascade × 2048² × 4B = 64MB

```
Cascade | 범위 (λ=0.75)  | Texel 밀도
   0    | 0.1 ~ 3.5m     | ~0.34 cm/texel
   1    | 3.5 ~ 25m      | ~2.4  cm/texel
   2    | 25 ~ 120m      | ~11.7 cm/texel
   3    | 120 ~ 500m     | ~49   cm/texel  (원거리, 시각 허용)
```

---

### 구현 순서

```
[완료] Step  1. AssetConfigs 구조체 신설 + GFX::loadAssets 시그니처 변경  (gfx.hpp/cpp)
[완료] Step  2. SharedResources: addCSMShadowMap + shadowMapData vector화  (sharedResources.hpp/cpp)
[완료] Step  3. Light::updateCSMCascades 추가  (light.hpp/cpp)
[완료] Step  4. shader.hpp PerFrameData 변경  (PBR / Terrain / ShadowMap 공통)
[완료] Step  5. pbrLighting.hlsli: PCF_CSM, calcCSMShadow, illuminateCSM 추가
[완료] Step  6. Shadow 셰이더 GS 추가 + NumRenderTargets=0  (shadowMap.hlsl, shadowMapSkinned.hlsl, terrainShadowMap.hlsl, shader.cpp)
[완료] Step  7. PBRPipeline::Dispatcher::shadowPass() — roomIdx, GS 방식 적용  (pbrPipeline.cpp)
[완료] Step  8. PBRSkinnedPipeline::Dispatcher::shadowPass() 동일  (pbrSkinnedPipeline.cpp)
[완료] Step  9. TerrainPipeline::LightData vector화, mainDirectionalLight 구조화  (gfx.hpp/cpp, terrainPipeline.hpp/cpp)
[완료] Step 10. gfx.cpp render() shadow pass 변경  (addCSMShadowMap, roomIdx 전달)
[완료] Step 11. game.cpp terrain 라이트 데이터 연결  (cascadeViews/cascadeProjs/cascadeSplitsFarV 전달)
[ 미완] Step 12. CSM 디버그 시각화  (CSM_DEBUG_VIS PSO 추가, 'c' 키, GFX::toggleCsmDebugVisualization)
[ 미완] Step 13. CODE_INDEX.md 갱신
```



// UI(hp, inventory, login, loading), Effect, goal-based AI, clustered AI: 5월 초 게임 시작->집단 전투 컨텐츠 완성
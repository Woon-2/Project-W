# 경로 안내 연출 (Path Guidance)

플레이어가 다음 목적지로 향하는 경로를 알려주는 **클라이언트 전용 연출**.
검토/선정 과정은 `pathGuidanceReview.html` 참조. 두 가지 연출을 결합한다.

- **C — 흐르는 빛의 리본**: 지면을 따라 흐르는 한 줄기 발광 빛. UV 흐름이 목적지 방향으로 스크롤되어 진행 방향을 전달.
- **D — 길잡이 위습(Wisp)**: 밝게 빛나는 구체가 플레이어 앞을 날아감(움직임=방향).

경로는 게임플레이가 아닌 연출 데이터라 `ZoneSystem`처럼 **클라 단독**(서버 무관)으로 처리한다.

---

## 1. 데이터 (Unity → 엔진)

경로 = **순서가 있는 점들의 폴리라인**. v1은 익스포터 무변경으로 착수한다.

- **저작**: Unity에서 경로 점마다 `LevelMarker` 배치. `markerType="PathPt"`,
  `markerName="<pathId>_<index>"` (예: `main_0`, `main_1`, …).
- **추출**: 기존 Marker 섹션이 `chunks_index.bin`으로 베이크됨(`TerrainExtractor` 변경 없음).
- **파싱**: `TerrainChunkManager::markers()`에서 `type=="PathPt"` 필터 → name을 마지막 `_` 기준으로
  `pathId`+`index` 분리 → pathId별 그룹화 → index 정수 정렬 → 등호장(等弧長) 리샘플.
- **후속(②/③)**: Path 컴포넌트/Spline 등호장 베이크. 산출물이 동일 폴리라인이라 연출 코드 재사용.

---

## 2. 구성 요소

| 파일 | 역할 |
|------|------|
| `client/pathGuideSystem.{hpp,cpp}` | `PathGuideSystem` — 마커 파싱·리샘플·지면 conform·리본/위습 제출. 게임 스레드 단독 |
| `client/mesh.{hpp,cpp}` | `buildOrbProxyMesh()` — free-orb(위습)용 N-포인트 프록시 메시 |
| `client/trail.hlsl` · `trailPipeline.*` · `shader.hpp` | Trail `flowSpeed`/`alignMode` 추가(하위호환) |
| `client/gfx.{hpp,cpp}` · `shader.{hpp,cpp}` | 프리블룸 HDR 트레일 채널(`addHDRTrailDrawEvent`, `TrailShaderHDR`, 별도 리소스셋) |
| `client/online/onlineGame.{hpp,cpp}` | `pathGuide_` 멤버 + build/update/submit 훅 + 프록시 메시 생성 |

### PathGuideSystem 흐름

- `build(markers)` — PathPt 그룹화·정렬·등호장 리샘플(기본 0.5m). XZ만 확정, Y는 매 프레임 conform.
- `update(dt, playerPos, terrain)` — 가장 가까운 path 선택(`activateRadius` 내) → 플레이어를 폴리라인에
  투영(`sPlayer`) → **가시 윈도우**(뒤 `windowBehind`, 앞 `windowAhead`)만 `heightAtWorld`로 Y conform
  → 위습 전진(ease) → `flowTime_ += dt`.
- `submitDrawEvents(gfx, ribbonTex, orbProxy)` — 리본을 `≤31정점 + 1정점 오버랩`으로 세그먼트 체이닝하여
  `addHDRTrailDrawEvent`, 위습 1개를 `addDrawEvent(EnergyOrbPipeline::DrawEvent)`.

---

## 3. 렌더링 핵심

### 발광(블룸) 패스 순서 — 설계의 결정 근거

- `EnergyOrb`는 **블룸 전 SceneColorHDR**에 그려짐 → 발광 보장 (`gfx.cpp` energy orb 블록).
- 기존 `TrailPipeline`은 **톤매핑 resolve 이후 LDR 백버퍼**에 그려짐 → 블룸 미적용.
- 따라서 리본 발광을 위해 **프리블룸 HDR 트레일 채널**을 신규 추가:
  - `TrailShaderHDR` PSO(`RTVFormats[0]=R16G16B16A16_FLOAT`, additive, reverse-Z GREATER, depth-write off).
  - `drawEventsTrailPipelineHDR_` + `resourcesTrailPipelineHDR_`(**별도 리소스셋** — 두 디스패처가 같은
    StructuredBuffer를 덮어쓰는 충돌 방지, per-backbuffer 더블버퍼링).
  - 2번째 `TrailPipeline::Dispatcher`를 **energy orb 직후**(SceneColorHDR RTV + scene DSV)에 draw.

### trail.hlsl 하위호환 확장

`PerDrawcallData`의 말단 `float2 pad0` → `float flowSpeed; uint alignMode;`(크기 불변).
- **흐름**: Tile 모드 V = `cumulativeDist/tileLength − currentSystemTime*flowSpeed`.
- **지면정렬**: `alignMode==1`이면 `sideDir = cross(tangent, worldUp)`(평지 가정), 아니면 기존 카메라-페이싱.
- 기본값 0 → **기존 파티클 트레일 동작 불변**(회귀 없음).

### 위습 = EnergyOrb free-orb 일반화

`energyOrb.hlsl`은 `morphT=1`에서 최종 위치 = `sphereCenter + pointInUnitSphere(vid)*radius` →
**시작 POSITION/본 무관, 정점 개수만 의미**. `buildOrbProxyMesh`(N=128 더미 정점 + 항등 1-bone)로
시체 비종속 발광 구체를 렌더. (B "빛나는 구체"에도 재사용 가능.)

---

## 4. 동시성 / 엣지케이스

- build/update/submit 모두 **게임 스레드 단독**. submit은 `gfx.addDrawEvent` push_back뿐 → 신규 락 없음.
- HDR 트레일 리소스는 per-backbuffer 더블버퍼링(GPU in-flight 덮어쓰기 방지).
- **지형 스트리밍**: 미로드 청크에서 `heightAtWorld`는 0 반환 → conform 시 authored Y로 폴백. 단 가시 윈도우는
  항상 플레이어 인근(로드된 청크)이라 안전, 지형 후로드에도 추종.
- 점<2/마커 없음→연출 숨김; 경로 끝 너머→윈도우 클램프 + 양끝 페이드(정점 `age`로 재사용); 다중 path→
  가장 가까운 1개만 활성(클러터 방지); path 전환 시 위습 `sWisp` 스냅(fly-across 방지).
- 용량: 윈도우 리본 ≤ 십수 draw, 위습 1 orb → `kMaxDrawEvents`/`kMaxOrbDrawcalls` 대비 충분(HDR 트레일은
  128 draw 전용 풀).

---

## 5. 튜닝 (`PathGuideSystem::Config`)

리본: `sampleSpacing/windowAhead/windowBehind/ribbonWidth/ribbonYOffset/flowSpeed/tileLength/endFade/ribbonColor`.
위습: `wispLead/wispEaseRate/wispHoverY/wispBobAmp/wispBobFreq/wispRadius/wispPointSize/wispColor`.
활성: `activateRadius`. 색은 HDR(>1)로 두어 블룸을 유도. 리본 텍스처는 `AssetManager::trail62Tex()` 재사용.

---

## 6. 검증

자동 테스트 없음 → 실행 후 시각 검증.

1. Unity에 `PathPt` `LevelMarker` 배치 후 추출(또는 테스트 `chunks_index.bin` 수동 추가).
2. Online 클라 실행 → 보행: 리본이 지면 밀착·목적지 방향 스크롤·블룸 발광, 위습이 앞서 날며 발광·bob,
   윈도우가 플레이어 추종, 언덕 뒤 가림 정상, 양끝 페이드.
3. 회귀: 몬스터 처치→에너지 오브 발광 유지; 스킬 파티클 트레일→기존과 동일(카메라-페이싱, 흐름 없음).

## 7. 후속(v1 범위 외)

위습 혜성 꼬리, 다중 경로/목적지 전환, Unity ②/③ 정식 파이프, 가파른 지형용 per-vertex 노멀 정렬,
화면 밖 목적지용 UI 나침반.

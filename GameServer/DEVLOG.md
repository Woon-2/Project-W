# DEVLOG

RoomServer 전술 전투(미드보스) 시스템 정리 작업 로그.

---

## 2026-06-17 — 전술 전투 분리 & PlatoonLeader 디커플링 (branch: `temp_server`)

미드보스 전술(홉고블린/GrandBaum/Isis)을 **독립 모듈로 분리**하고, 검증용 임시 코드를 제거했다.
또한 `PlatoonLeader`가 특정 보스 전술에 결합돼 있던 부분을 끊어 **인카운터별 명시 주입**으로 통일했다.
공통 전투 엔진(`TacticalNpc`/`TacticalSquad`/`PlatoonLeader`/`MidBossTacticBase`)은 세 전술이 함께 쓰는
기반이라 **그대로 유지**했다. 두 작업 모두 `RoomServer`(Debug|x64) 빌드 통과 검증 완료.

---

### 1. 전술 전투 분리 (스캐폴딩 제거 + 보스별 파일 분리)

#### 배경
그간 GrandBaum·Isis 전술을 검증하려고 **홉고블린 아레나(`Arena_Hobgoblin`)에 다른 보스 전투를 임시로
띄워** 한 자리에서 비교 테스트해 왔다. 이제 각 보스가 전용 zone/마커(`Arena_Grandbaum` + `WallGrandbaum_*`
+ `GrandbaumSpawner`, `Arena_Isys` + `WallIsys_*` + `IsysSpawner`)를 갖췄으므로, 테스트 전용 코드를
제거하고 각 전술을 자기 아레나에서만 트리거되는 독립 모듈로 정리했다.

> 참고: "전략 계층"은 이미 분리돼 있었다 — `IMidBossTactic` 인터페이스 + 폴리모픽 구현 3개. 이번 작업은
> ① 테스트 스캐폴딩 제거 ② 한 파일에 동거하던 전술 구현을 보스별로 물리 분리 두 가지다.

#### A. 테스트 스캐폴딩 제거
- `Room.cpp`의 `HOBGOBLIN_DEBUG_TACTIC` 매크로 + `bindZoneHandlers`의 `#if/#elif/#else` 제거 →
  `Arena_Hobgoblin`이 다시 **홉고블린 전용**으로 복귀. (매크로가 `1`이라 그동안 Arena_Hobgoblin이
  GrandBaum을 스폰하던 상태였음.)
- `onArenaGrandBaumEnter`/`onArenaIsisEnter`의 `[디버그 트리거]` fallback(전용 마커가 없을 때
  any-Wall 중점 → 진입 플레이어 위치로 스폰) 제거. 보스 **자기 벽** 기반 fallback은 정식 경로라 유지.
- 사장 코드 `TacticalGoblin::spawnEncounter()`(호출부 없는 래퍼) 제거 + 불필요해진 `Room.hpp` 인클루드·
  `class Room` 전방선언 정리. `trooperConfig()`/`bossConfig()`는 `Room::spawnTacticalGoblinEncounter`에서
  사용 중이라 그대로 유지.

#### B. `MidBossTactics` 보스별 분리
단일 파일(`MidBossTactics.hpp` 551줄 / `MidBossTactics.cpp` 3688줄)을 아래 5쌍으로 분리. **로직 변경 0**
(함수 본문을 그대로 이동).

| 새 파일 | 내용 |
|---|---|
| `MidBossTacticBase.{hpp,cpp}` | 공용 유틸 (clusters, engage, centroid, `issueStableEngage`, `isLivingPlayerTarget`) |
| `GoblinMidBossTactic.{hpp,cpp}` | 홉고블린 전술 |
| `GrandBaumMidBossTactic.{hpp,cpp}` | GrandBaum 전술 (ShieldWall/뱀 매복/넉백) |
| `IsisMidBossTactic.{hpp,cpp}` | Isis 전술 (2연속 쐐기 협공) |

- 파일-로컬 `static` 헬퍼는 내부 링키지 보존을 위해 필요한 .cpp에만 복제: `norm3`(전부), `lenXZ`(Goblin/Isis).
  `isisRng()`은 Isis에 귀속.
- 인클루드 갱신: `Room.cpp` → GrandBaum + Isis 헤더(Goblin은 §2에서 추가), `PlatoonLeader.cpp` → Goblin 헤더(§2에서 재조정).
- 원본 2파일 삭제, `RoomServer.vcxproj` / `RoomServer.vcxproj.filters` 등록 교체.

> **이슈/해결 (UTF-8 BOM)**: 새로 만든 `.hpp` 4개에 UTF-8 BOM이 빠져 있어, MSVC가 한글 주석을 시스템
> 코드 페이지(cp949)로 오해석(C4819) → 헤더 파싱이 깨지면서 `constexpr` 상수가 미선언으로 처리됨
> (`CAPTURE_CHARGE_STANDOFF`, `RETREAT_TIMEOUT` C2065). 기존 소스가 모두 UTF-8 **BOM** 포함이었던 것을
> 확인하고 4개 헤더에 BOM을 추가해 해결.

#### C. 문서 갱신
`RoomServer/docs/`의 `grandBaumTactic.md`, `isisTactic.md`, `tacticalReservationAndEngage.md`에서
파일명 참조·"(Goblin 전술과 동거)" 표현·제거된 디버그 트리거 안내를 실제 zone 태그(`Arena_Grandbaum`/
`Arena_Isys`)와 현 파일 구조에 맞게 정리.

#### 검증
`RoomServer`(Debug|x64) 빌드 통과 — 오류 0, 새 파일 경고 0.

---

### 2. PlatoonLeader 기본 tactic 디커플링

#### 배경
의도는 **실행되는 전술 전투에 맞춰** 해당 `MidBossTactic`(Goblin/GrandBaum/Isis)이 주입되는 것이었다.
GrandBaum·Isis는 이미 3-arg 생성자로 명시 주입했지만, 홉고블린 인카운터만 `PlatoonLeader`의 2-arg 생성자
**기본값(`GoblinMidBossTactic`)**에 의존했다. 이 때문에 공유 인프라인 `PlatoonLeader.cpp`가 특정 보스 모듈
(`GoblinMidBossTactic.hpp`)을 하드 의존하는 잔여 결합이 있었다.

#### 변경
- `PlatoonLeader.hpp`: 2-arg 생성자(`cfg = {}`) 선언 제거 → 전술을 받는 **3-arg 생성자만** 유지
  (전술 없이는 보스를 만들 수 없게 강제).
- `PlatoonLeader.cpp`: 기본 생성자 정의 + `GoblinMidBossTactic.hpp` 의존 제거 → **`IMidBossTactic`에만 의존**.
- `Room.cpp`: `spawnTacticalGoblinEncounter`가 GrandBaum/Isis와 대칭으로
  `std::make_unique<GoblinMidBossTactic>()`를 명시 전달하고 `GoblinMidBossTactic.hpp`를 인클루드.

#### 결과 / 검증
세 인카운터 모두 자기 전술을 **동일한 방식(명시 주입)**으로 전달. 동작은 동일(홉고블린 인카운터는 여전히
`GoblinMidBossTactic` 실행). `RoomServer`(Debug|x64) 빌드 통과.

---

### 영향받은 파일

- **신규(8)**: `MidBossTacticBase.{hpp,cpp}`, `GoblinMidBossTactic.{hpp,cpp}`,
  `GrandBaumMidBossTactic.{hpp,cpp}`, `IsisMidBossTactic.{hpp,cpp}`
- **삭제(2)**: `MidBossTactics.{hpp,cpp}`
- **수정**: `Room.cpp`, `PlatoonLeader.{hpp,cpp}`, `TacticalGoblin.{hpp,cpp}`,
  `RoomServer.vcxproj`, `RoomServer.vcxproj.filters`,
  `docs/{grandBaumTactic,isisTactic,tacticalReservationAndEngage}.md`

### 남은 작업 / 주의
- 디버그 fallback을 제거했으므로 GrandBaum/Isis는 전용 마커(`WallGrandbaum_*`/`GrandbaumSpawner`,
  `WallIsys_*`/`IsysSpawner`)가 레벨에 저작돼 있어야 스폰된다(미저작 시 인카운터 스킵).
- 실행 검증은 실제 `client`로(DummyClient 아님): `Arena_Hobgoblin` → 홉고블린, `Arena_Grandbaum` →
  GrandBaum, `Arena_Isys` → Isis 가 각각 뜨는지 확인.
- 향후 `TacticalSlime`/`TacticalSnake` 등 per-type NPC 클래스 도입은 이번 작업과 독립적인 후속 가산 작업
  (`TacticalGoblin` 패턴을 템플릿으로 사용).

### 해야 할 일
1. 모든 전술 전투 잘 되는지 체크
	- 홉고블린 전술 전투 [O]
	- 그랜드밤 전술 전투 [X] -> 다른 2번째 생성된 room에서 넉백이 안되는 버그가 있음
	- 이시스 전술 전투 [O] -> 한 번 확인했음. 약간 예상으로는 정상 동작하는 거 같음. 여러 번 체크 해봐야 함.

2. 영어 철차 맞추기
	- GrandBaum -> Grandbaum
	- Isis -> Isys

3. 클라이언트 및 서버 최적화
	- 우선적으로 클라이언트 병목 찾아보기

4. 스트레스 테스트
5. DB 연동

* 2 -> 3 -> 4 - > 1 -> 5 순으로 처리할 예정

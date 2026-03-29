### Client
Entry: `client/main.cpp` (WinMain)

DirectX 12 game client. Supports two modes selected at compile/runtime:
- `ClientApp` — top-level app; switches between `StandAlone` and `Online` modes
- `standalone/game` — singleplayer mode; simulates all at the client-side
- `online/onlineGame` — multiplayer mode; owns the network session and interpolates server state for rendering

### 핵심 지침
시니어 게임 엔진 프로그래머의 관점에서 구현한다.

목표:
- 현재 구조를 유지하는 것이 아니라, 더 나은 구조로 한 단계씩 개선한다.
- 가능한 임시 코드가 아니라 production-quality 코드를 작성한다.

허용:
- 클래스 구조 변경 가능
- 새로운 abstraction 도입 가능
- 기존 코드 삭제/리팩토링 가능

현재 구조가 잘못되어 있다면 과감하게 재설계해도 된다.
기존 구조 유지보다 코드 품질을 우선한다.

### 아키텍처
그래픽스 아키텍처 참조 - `docs/graphicsArchitecture.md`
물리 아키텍처 참조 - `docs/physicsArchitecture.md`
게임 아키텍처 참조 - `docs/gameArchitecture.md`

### 파일 인코딩 주의
프로젝트의 기존 파일은 **UTF-8 with BOM** (utf-8-sig)으로 저장되어 있다.
새 파일을 생성할 때 자동 도구가 BOM 없이 저장하는 경우, 한국어 Windows에서 MSVC가
해당 파일을 cp949로 해석해 한국어 주석이 빌드 오류를 일으킨다.
**새 파일에는 한국어 주석 사용 금지.** 기존 파일 편집은 무방.

### 외부 참조
`d3dx12`, `texloader` 디렉터리는 외부 참조입니다.
해당 디렉터리의 내용은 가급적 수정 및 참조하지 마세요.

### 코드 인덱스 (필수 참조)
- `docs/CODE_INDEX.md` — 기능별 소스 파일 + 라인 번호 인덱스

**탐색 규칙:**
1. 특정 기능/클래스의 위치를 찾을 때는 `CODE_INDEX.md`를 먼저 조회한다.
2. 코드를 수정한 후에는 해당 항목의 라인 번호를 `CODE_INDEX.md`에 반드시 갱신한다.
3. 새 클래스/함수/구조체를 추가하면 `CODE_INDEX.md`에 항목을 추가한다.

### TODO 참조
- `docs/TODO.md`
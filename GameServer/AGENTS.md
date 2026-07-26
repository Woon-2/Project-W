# AGENTS.md

- 이 프로젝트에서 사용자에게 답변할 때는 한국어로 작성한다.
- 사용자는 클라이언트를 Visual Studio에서 직접 실행하며 확인한다. 온라인 모드 검증에는
  `LobbyServer` + `RoomServer` + `client`를 함께 띄운다.
- **전체 솔루션 빌드는 정상 통과한다(2026-07-26 확인).** 빌드 실패를 "기존 환경 이슈"로
  넘기지 말고 원인을 찾을 것. 자주 걸리는 함정 두 가지:
  - `ServerEngine`을 고쳤으면 **단독으로 먼저 빌드**해야 한다. 다른 프로젝트는
    `#pragma comment(lib, ...)`로 `.lib`를 링크하므로(프로젝트 참조 아님) 구 `.lib`를 물고
    `LNK2019`로 죽는다.
  - 솔루션을 `/m`으로 빌드하면 공유 `ServerEngine.pdb` 경합으로 `C1090`이 난다.
    ServerEngine을 `/m` 없이 빌드한 뒤 솔루션을 빌드한다.
  - 개별 `.vcxproj` 단독 빌드에는 `/p:SolutionDir`가 **필수**다(없으면 `sepch.hpp`를 못 찾는다).
  - 툴셋은 **v145** — VS18 MSBuild를 쓴다(구 VS2022 MSBuild는 `MSB8020`).
- 렌더링/클라이언트 작업 검증은 수정 파일의 정적 확인, HLSL 직접 컴파일, 관련 코드 경로 점검을
  우선하되, **빌드는 실제로 통과시킨다.**
- `DummyClient/` 디렉터리는 솔루션에서 빠졌다. 빌드 대상은
  `ServerEngine` / `LobbyServer` / `RoomServer` / `client` 4개뿐이다.

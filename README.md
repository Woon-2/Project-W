# Project-W
한국공학대학교 게임공학과 2025 졸업작품

### 팀 구성
- 장명운: *클라이언트, 서버
- 우상훈: 클라이언트, *서버
- 이종진: 클라이언트, 기획

### How to Build

- cmake 3.18 버전 이상이 필요합니다.
- vscode의 터미널 혹은 Windows powershell에서 다음 두 명령어를 실행합니다.

> cmake -S. -Bbuild   

> cmake --build build --config [Release|Debug]

- `--config` 옵션으로 설정한 빌드 설정에 따라 `build/Release/client.exe` 또는 `build/Debug/client.exe`가 생성됩니다.   
- Visual Studio에서 실행시키고 싶은 경우, 다음을 따릅니다.
  - `build/PZolzak.sln`을 엽니다.
  - `client` 서브프로젝트를 마우스 오른쪽 클릭하고, `시작 프로젝트로 설정`을 클릭합니다.
  - 해당 서브프로젝트를 실행할 수 있게 되었습니다.
  - cmake generator로 Visual Studio가 선택되었을 때에만 `build/PZolzak.sln`이 존재합니다.
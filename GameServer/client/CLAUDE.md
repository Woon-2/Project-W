### Client
Entry: `client/main.cpp` (WinMain)

DirectX 12 game client. Supports two modes selected at compile/runtime:
- `online/onlineGame.hpp` — Networked mode
- `standalone/game.hpp` — Single-player mode

### 아키텍처
그래픽스 아키텍처 참조 - `docs/graphicsArchitecture.md`
물리 아키텍처 참조 - `docs/physicsArchitecture.md`
게임 아키텍처 참조 - `docs/gameArchitecture.md`

### 외부 참조
`d3dx12`, `texloader` 디렉터리는 외부 참조입니다.
해당 디렉터리의 내용은 가급적 수정 및 참조하지 마세요.

### TODO 참조
- `docs/TODO.md`
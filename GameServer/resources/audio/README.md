# resources/audio

게임 사운드 자산 폴더. `client/sound/soundCatalog.cpp`의 카탈로그가 여기를 참조합니다.

```
audio/
├── bgm/   배경 음악 (스트리밍). 권장 포맷: .mp3 또는 .flac
└── sfx/   효과음 (메모리 디코딩). 권장 포맷: .wav
```

## 사용법

1. 오디오 파일을 위 폴더에 넣습니다.
2. `client/sound/soundCatalog.cpp`의 테이블에 `{ 논리이름, 경로, 버스, loop, stream, 기본볼륨 }` 항목을 추가합니다.
3. 게임 코드에서 논리이름으로 재생합니다: `ClientApp::sound().playBgm("lobby")`, `playSfx("ui_click")`, `playSfx3D("hit", worldPos)`.

## 현재 카탈로그가 기대하는 파일 (플레이스홀더)

| 논리이름 | 경로 | 용도 |
|----------|------|------|
| lobby | bgm/lobby.mp3 | 로비 BGM |
| ingame | bgm/ingame.mp3 | 인게임 BGM |
| ui_click | sfx/ui_click.wav | 버튼 클릭 |
| ui_hover | sfx/ui_hover.wav | 버튼 호버 |
| hit | sfx/hit.wav | 피격 |
| death | sfx/death.wav | 사망 |
| attack | sfx/attack.wav | 공격 |
| skill_hit | sfx/skill_hit.wav | 스킬 적중 |

파일이 없으면 재생 시 1회 경고 로그만 남고 크래시는 발생하지 않습니다.

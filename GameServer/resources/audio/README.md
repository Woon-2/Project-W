# resources/audio

게임 사운드 자산 폴더. `client/sound/soundCatalog.cpp`의 카탈로그가 여기를 참조합니다.

```
audio/
├── bgm/   배경 음악 (스트리밍). 권장 포맷: .mp3 또는 .flac
└── sfx/   효과음 (메모리 디코딩). 포맷: .wav / .mp3
```

## 사용법

1. 오디오 파일을 위 폴더에 넣습니다.
2. `client/sound/soundCatalog.cpp`의 테이블에 `{ 논리이름, 경로, 버스, loop, stream, 기본볼륨 }` 항목을 추가합니다.
3. 게임 코드에서 논리이름으로 재생합니다: `ClientApp::sound().playBgm("lobby")`, `playSfx("ui_click")`, `playSfx3D("sword_slash_1", worldPos)`. 스킬 연출음은 직접 호출 대신 lua `PlaySound` 타임라인 이벤트로 재생합니다.

## 현재 카탈로그가 기대하는 파일

| 논리이름 | 경로 | 용도 |
|----------|------|------|
| lobby | bgm/lobby.wav | 로비 BGM |
| ingame | bgm/Action 5 (Loop).wav | 인게임 BGM |
| ui_click | sfx/ui_click.wav | 버튼 클릭 |
| sword_slash_1 | sfx/sword/sword_slash_1.mp3 | SlashCombo 스윙음(레이어 1~3) |
| sword_slash_finish | sfx/sword/sword_slash_finish.mp3 | SlashCombo 마무리음 |

> 2026-06-19 정리: ui_click을 제외한 플레이스홀더 SFX(hit/death/attack/skill_hit/ui_hover/skill_ready)는 카탈로그·파일·코드에서 모두 제거됨.

파일이 없으면 재생 시 1회 경고 로그만 남고 크래시는 발생하지 않습니다.

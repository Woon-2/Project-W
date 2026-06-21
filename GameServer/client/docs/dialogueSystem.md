# 대화/독백 창 제작 파이프라인

## 목표

대화창의 화면 위치, 크기, 배경색과 투명도, 글자색, 폰트, 페이지 텍스트, 페이드 시간을 하나의 JSON에서 관리한다. HTML 미리보기와 게임 런타임이 `resources/UI/dialogues/dialogues.json`을 함께 사용하므로 미리보기에서 조정한 수치를 변환 없이 게임에 적용할 수 있다.

## 빠른 미리보기

PowerShell에서 다음 파일을 실행한다.

```powershell
client\docs\dialogue_preview\preview.ps1
```

브라우저 편집기에서 다음 작업을 할 수 있다.

- 이벤트별 대화창 선택
- X/Y 위치와 폭/높이 조정
- 배경색, 투명도, 글자색 조정
- 폰트 패밀리와 크기 조정
- 페이지별 문장 편집
- Enter 또는 마우스 왼쪽 클릭으로 페이지 진행 확인
- 마지막 페이지 이후 페이드아웃 확인
- 수정 결과를 `dialogues.json`으로 저장

저장한 파일을 `resources/UI/dialogues/dialogues.json`에 덮어쓴 뒤 게임을 다시 실행하면 동일한 설정이 적용된다.

## JSON 구조

모든 좌표와 크기는 기존 UI와 같은 1024×768 기준 픽셀이다.

```json
{
  "schemaVersion": 1,
  "canvas": { "width": 1024, "height": 768 },
  "dialogues": [
    {
      "eventId": "sample_intro",
      "rect": { "x": 128, "y": 504, "width": 768, "height": 176 },
      "background": [0.03, 0.05, 0.08, 0.82],
      "textColor": [0.96, 0.98, 1.0, 1.0],
      "padding": 24,
      "fontFamily": "Malgun Gothic",
      "fontSize": 24,
      "fadeOutSeconds": 0.45,
      "pages": ["첫 페이지", "두 번째 페이지"]
    }
  ]
}
```

색상 배열은 `[R, G, B, A]`이며 각 값의 범위는 0~1이다.

## 게임에서 이벤트 연결

이벤트가 발생한 코드에서 다음처럼 문자열 ID를 전달한다.

```cpp
dialogueSystem_.show("sample_intro");
```

`eventId`가 JSON에 있으면 해당 창이 표시된다. 표시 중에는 Enter와 마우스 왼쪽 클릭이 페이지 진행에 사용되며 게임 플레이 입력보다 우선한다. 마지막 페이지에서 한 번 더 진행하면 설정된 시간 동안 페이드아웃한다.

standalone에서는 F8 키로 `sample_intro` 이벤트를 다시 실행할 수 있다.

개발 중 JSON을 다시 읽어야 할 때는 `dialogueSystem_.reload()`를 호출할 수 있다.

## 주요 코드

- `ui/dialogue/DialogueSystem.hpp/.cpp`: JSON 로딩, 이벤트 매핑, 페이지 진행, 페이드
- `standalone/game.cpp`: 입력 우선순위와 샘플 F8 트리거
- `ui/widgets/Panel.cpp`: 반투명 단색 배경
- `ui/widgets/Label.cpp`: 지정 폰트와 페이드 알파 적용
- `docs/dialogue_preview/`: 브라우저 편집 및 미리보기

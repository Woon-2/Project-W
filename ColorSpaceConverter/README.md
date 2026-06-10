# Color Space Converter

이미지(`.png`, `.tga`)의 color space 를 변환하는 도구입니다.

- **sRGB → Linear** (gamma decode)
- **Linear → sRGB** (gamma encode)

단순 2.2 감마가 아니라 정확한 sRGB transfer function(piecewise)을 사용합니다.
알파 채널은 변환하지 않고 그대로 보존하며, 8bit/16bit 비트뎁스도 유지합니다.

## 설치

```
pip install -r requirements.txt
```

(`tkinter` 는 표준 라이브러리라 별도 설치 불필요. numpy / Pillow 만 필요합니다.)

## 사용법

### GUI

```
python run.py
```

입력/출력 디렉터리를 고르고, 변환 방향을 선택한 뒤 **변환 시작**을 누르면 됩니다.
기본 입력/출력 디렉터리는 `input/`, `output/` 입니다.

### CLI

```
python run.py --to linear              # input/ -> output/, sRGB -> Linear
python run.py --to srgb                # Linear -> sRGB
python run.py -i tex_in -o tex_out --to linear --recursive
python run.py --to srgb --no-overwrite # 기존 출력 파일은 건너뜀
```

| 옵션 | 설명 |
| --- | --- |
| `-i, --input` | 입력 디렉터리 (기본 `input`) |
| `-o, --output` | 출력 디렉터리 (기본 `output`) |
| `--to` | `linear`(sRGB→Linear) 또는 `srgb`(Linear→sRGB) — 필수 |
| `-r, --recursive` | 하위 폴더까지 처리 (폴더 구조 유지) |
| `--no-overwrite` | 출력 파일이 이미 있으면 건너뜀 |

## 파일 구성

| 파일 | 역할 |
| --- | --- |
| `colorspace.py` | 변환 핵심 로직 (transfer function, 파일/디렉터리 처리) |
| `gui.py` | tkinter GUI |
| `cli.py` | 명령줄 인터페이스 |
| `run.py` | 진입점 (인자 없으면 GUI, 있으면 CLI) |

## 동작 메모

- 출력 파일명/확장자는 입력과 동일하게 유지됩니다.
- 팔레트(P) 이미지는 RGBA 로 풀어서 변환합니다.
- 한 파일 변환이 실패해도 나머지 파일 처리는 계속됩니다 (요약에 실패 건수 표시).

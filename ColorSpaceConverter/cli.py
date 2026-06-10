"""
cli.py — 명령줄(headless) 인터페이스

사용 예:
  python cli.py --to linear            # ./input -> ./output, sRGB->Linear
  python cli.py --to srgb              # Linear->sRGB
  python cli.py -i tex_in -o tex_out --to linear --recursive
"""

from __future__ import annotations

import argparse
import sys

import colorspace as cs


def _direction_from_arg(value: str) -> str:
    v = value.strip().lower()
    if v in ("linear", "srgb_to_linear", "s2l", "tolinear"):
        return cs.SRGB_TO_LINEAR
    if v in ("srgb", "linear_to_srgb", "l2s", "tosrgb"):
        return cs.LINEAR_TO_SRGB
    raise argparse.ArgumentTypeError(
        "--to 값은 linear (sRGB->Linear) 또는 srgb (Linear->sRGB) 여야 합니다."
    )


def main(argv: list[str] | None = None) -> int:
    # Windows 콘솔(cp949)에서도 유니코드(화살표/em-dash 등) 출력이 깨지지 않도록 재설정
    for stream in (sys.stdout, sys.stderr):
        try:
            stream.reconfigure(encoding="utf-8", errors="replace")
        except (AttributeError, ValueError):
            pass

    parser = argparse.ArgumentParser(
        description="이미지(.png/.tga) color space 변환기 (sRGB <-> Linear)"
    )
    parser.add_argument("-i", "--input", default="input", help="입력 디렉터리 (기본: input)")
    parser.add_argument("-o", "--output", default="output", help="출력 디렉터리 (기본: output)")
    parser.add_argument(
        "--to",
        required=True,
        type=_direction_from_arg,
        help="변환 대상 color space: 'linear'(sRGB->Linear) 또는 'srgb'(Linear->sRGB)",
    )
    parser.add_argument("-r", "--recursive", action="store_true", help="하위 폴더까지 처리")
    parser.add_argument(
        "--no-overwrite",
        action="store_true",
        help="출력 파일이 이미 있으면 건너뜀",
    )
    args = parser.parse_args(argv)

    def progress(idx, total, src, status, message):
        tag = {"ok": "OK   ", "skip": "SKIP ", "error": "ERROR"}.get(status, status)
        print(f"[{idx}/{total}] {tag} {src}" + (f"  -> {message}" if status != "ok" else ""))

    try:
        summary = cs.convert_directory(
            args.input,
            args.output,
            args.to,
            recursive=args.recursive,
            overwrite=not args.no_overwrite,
            progress=progress,
        )
    except Exception as e:  # noqa: BLE001
        print(f"실패: {e}", file=sys.stderr)
        return 2

    print(
        f"\n완료 — 성공 {summary['ok']} / 건너뜀 {summary['skip']} / 실패 {summary['error']}"
    )
    for path, msg in summary["errors"]:
        print(f"  - {path}: {msg}", file=sys.stderr)
    return 0 if summary["error"] == 0 else 1


if __name__ == "__main__":
    raise SystemExit(main())

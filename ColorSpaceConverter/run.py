"""
run.py — 기본 진입점

인자 없이 실행하면 GUI 를 띄운다.
인자가 있으면 CLI 로 동작한다.

  python run.py                    # GUI
  python run.py --to linear        # CLI (sRGB -> Linear)
"""

import sys

if __name__ == "__main__":
    if len(sys.argv) > 1:
        import cli
        raise SystemExit(cli.main())
    else:
        import gui
        gui.main()

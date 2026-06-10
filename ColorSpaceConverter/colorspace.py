"""
colorspace.py — 이미지 color space 변환 핵심 로직

지원 변환:
  - sRGB  -> Linear (gamma decode)
  - Linear -> sRGB  (gamma encode)

설계 원칙:
  - 정확한 sRGB transfer function(piecewise) 사용. 단순 2.2 감마가 아님.
  - 알파 채널은 color space 변환 대상이 아니므로 그대로 보존한다.
  - 8bit / 16bit 비트뎁스를 자동 감지하여 정밀도 손실 없이 처리한다.
  - 팔레트(P) 이미지는 RGBA로 풀어서 처리한다.

이 모듈은 GUI/CLI 양쪽에서 공통으로 사용한다.
"""

from __future__ import annotations

import os
from typing import Iterable

import numpy as np
from PIL import Image

# 변환 방향 식별자
SRGB_TO_LINEAR = "srgb_to_linear"
LINEAR_TO_SRGB = "linear_to_srgb"

# 지원 입력 확장자
SUPPORTED_EXTS = (".png", ".tga")


# ---------------------------------------------------------------------------
# transfer function (정규화된 0..1 float 입력 기준)
# ---------------------------------------------------------------------------
def srgb_to_linear(c: np.ndarray) -> np.ndarray:
    """sRGB(감마 인코딩) -> Linear. 입력/출력 모두 0..1 float."""
    c = np.clip(c, 0.0, 1.0)
    return np.where(c <= 0.04045, c / 12.92, ((c + 0.055) / 1.055) ** 2.4)


def linear_to_srgb(c: np.ndarray) -> np.ndarray:
    """Linear -> sRGB(감마 인코딩). 입력/출력 모두 0..1 float."""
    c = np.clip(c, 0.0, 1.0)
    return np.where(c <= 0.0031308, c * 12.92, 1.055 * (c ** (1.0 / 2.4)) - 0.055)


_TRANSFER = {
    SRGB_TO_LINEAR: srgb_to_linear,
    LINEAR_TO_SRGB: linear_to_srgb,
}


# ---------------------------------------------------------------------------
# 이미지 단위 변환
# ---------------------------------------------------------------------------
def _split_color_alpha(arr: np.ndarray, mode: str):
    """(color, alpha) 로 분리. alpha 가 없으면 alpha 는 None."""
    if mode in ("LA", "RGBA"):
        return arr[..., :-1], arr[..., -1:]
    return arr, None


def convert_image(img: Image.Image, direction: str) -> Image.Image:
    """PIL Image 한 장을 변환하여 새로운 PIL Image 로 반환한다.

    - 비트뎁스(8/16)와 채널 구성(L/LA/RGB/RGBA)을 보존한다.
    - 알파 채널은 변환하지 않고 그대로 둔다.
    """
    if direction not in _TRANSFER:
        raise ValueError(f"알 수 없는 변환 방향: {direction}")
    transfer = _TRANSFER[direction]

    original_mode = img.mode

    # 팔레트 이미지는 RGBA 로 변환해 처리
    if original_mode == "P":
        img = img.convert("RGBA")
    # 1bit 등 비표준 모드는 RGB 로 승격
    elif original_mode in ("1", "I", "F"):
        # I(32bit int) / F(float) / 1(bilevel) 은 일반 처리가 까다로워 RGB 로 승격
        img = img.convert("RGB")

    mode = img.mode
    arr = np.asarray(img)

    # 정수 dtype 기준 최대값(정규화 분모). float 이미지면 1.0 으로 간주.
    if np.issubdtype(arr.dtype, np.integer):
        max_val = float(np.iinfo(arr.dtype).max)
    else:
        max_val = 1.0
    out_dtype = arr.dtype

    color, alpha = _split_color_alpha(arr, mode)

    # 0..1 정규화 -> transfer -> 역정규화
    color_f = color.astype(np.float64) / max_val
    color_f = transfer(color_f)
    color_f = np.clip(color_f, 0.0, 1.0) * max_val

    if np.issubdtype(out_dtype, np.integer):
        # 반올림 후 캐스팅(정밀도 손실 최소화)
        color_out = np.rint(color_f).astype(out_dtype)
    else:
        color_out = color_f.astype(out_dtype)

    if alpha is not None:
        result = np.concatenate([color_out, alpha], axis=-1)
    else:
        result = color_out

    return Image.fromarray(result, mode=mode)


def convert_file(src_path: str, dst_path: str, direction: str) -> None:
    """파일 하나를 변환하여 dst_path 로 저장한다."""
    os.makedirs(os.path.dirname(dst_path) or ".", exist_ok=True)
    with Image.open(src_path) as img:
        img.load()
        out = convert_image(img, direction)
        # 원본 확장자에 맞는 포맷으로 저장 (PIL 이 확장자로 포맷 결정)
        out.save(dst_path)


# ---------------------------------------------------------------------------
# 디렉터리 단위 일괄 처리
# ---------------------------------------------------------------------------
def find_images(input_dir: str, recursive: bool = False) -> list[str]:
    """input_dir 안의 지원 이미지 파일 경로 목록을 반환한다."""
    results: list[str] = []
    if recursive:
        for root, _dirs, files in os.walk(input_dir):
            for name in files:
                if name.lower().endswith(SUPPORTED_EXTS):
                    results.append(os.path.join(root, name))
    else:
        for name in os.listdir(input_dir):
            full = os.path.join(input_dir, name)
            if os.path.isfile(full) and name.lower().endswith(SUPPORTED_EXTS):
                results.append(full)
    return sorted(results)


def convert_directory(
    input_dir: str,
    output_dir: str,
    direction: str,
    recursive: bool = False,
    overwrite: bool = True,
    progress=None,
) -> dict:
    """input_dir 의 이미지를 모두 변환하여 output_dir 로 출력한다.

    progress: 선택. progress(index, total, src_path, status, message) 콜백.
              status 는 "ok" | "skip" | "error".
    반환: {"ok": n, "skip": n, "error": n, "errors": [(path, msg), ...]}
    """
    if not os.path.isdir(input_dir):
        raise NotADirectoryError(f"입력 디렉터리가 없습니다: {input_dir}")

    files = find_images(input_dir, recursive=recursive)
    total = len(files)
    summary = {"ok": 0, "skip": 0, "error": 0, "errors": []}

    for i, src in enumerate(files):
        # 입력 기준 상대 경로를 유지하여 출력 (recursive 시 하위 구조 보존)
        rel = os.path.relpath(src, input_dir)
        dst = os.path.join(output_dir, rel)

        try:
            if (not overwrite) and os.path.exists(dst):
                summary["skip"] += 1
                if progress:
                    progress(i + 1, total, src, "skip", "이미 존재함")
                continue

            convert_file(src, dst, direction)
            summary["ok"] += 1
            if progress:
                progress(i + 1, total, src, "ok", dst)
        except Exception as e:  # noqa: BLE001 - 한 파일 실패가 전체를 멈추지 않도록
            summary["error"] += 1
            summary["errors"].append((src, str(e)))
            if progress:
                progress(i + 1, total, src, "error", str(e))

    return summary

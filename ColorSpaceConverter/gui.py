"""
gui.py — tkinter 기반 GUI

기능:
  - 입력/출력 디렉터리 선택 (찾아보기)
  - 변환 방향 선택 (sRGB->Linear / Linear->sRGB)
  - 하위 폴더 포함, 덮어쓰기 옵션
  - 진행률 바 + 실시간 로그
  - 변환은 별도 스레드에서 수행하여 UI 가 멈추지 않음

tkinter 는 표준 라이브러리라 추가 설치가 필요 없다.
"""

from __future__ import annotations

import os
import queue
import threading
import tkinter as tk
from tkinter import filedialog, ttk

import colorspace as cs

APP_TITLE = "Color Space Converter — sRGB <-> Linear"


class ConverterGUI:
    def __init__(self, root: tk.Tk):
        self.root = root
        root.title(APP_TITLE)
        root.minsize(620, 480)

        base = os.path.dirname(os.path.abspath(__file__))
        self.input_var = tk.StringVar(value=os.path.join(base, "input"))
        self.output_var = tk.StringVar(value=os.path.join(base, "output"))
        self.direction_var = tk.StringVar(value=cs.SRGB_TO_LINEAR)
        self.recursive_var = tk.BooleanVar(value=False)
        self.overwrite_var = tk.BooleanVar(value=True)

        # 워커 스레드 -> UI 로 메시지를 넘기는 큐
        self._msg_queue: queue.Queue = queue.Queue()
        self._worker: threading.Thread | None = None

        self._build_ui()
        self.root.after(80, self._drain_queue)

    # ------------------------------------------------------------------ UI
    def _build_ui(self):
        pad = {"padx": 8, "pady": 4}
        frm = ttk.Frame(self.root, padding=12)
        frm.pack(fill="both", expand=True)
        frm.columnconfigure(1, weight=1)

        # 입력 디렉터리
        ttk.Label(frm, text="입력 디렉터리").grid(row=0, column=0, sticky="w", **pad)
        ttk.Entry(frm, textvariable=self.input_var).grid(row=0, column=1, sticky="ew", **pad)
        ttk.Button(frm, text="찾아보기", command=self._pick_input).grid(row=0, column=2, **pad)

        # 출력 디렉터리
        ttk.Label(frm, text="출력 디렉터리").grid(row=1, column=0, sticky="w", **pad)
        ttk.Entry(frm, textvariable=self.output_var).grid(row=1, column=1, sticky="ew", **pad)
        ttk.Button(frm, text="찾아보기", command=self._pick_output).grid(row=1, column=2, **pad)

        # 변환 방향
        dir_frame = ttk.LabelFrame(frm, text="변환 방향", padding=8)
        dir_frame.grid(row=2, column=0, columnspan=3, sticky="ew", **pad)
        ttk.Radiobutton(
            dir_frame, text="sRGB  →  Linear  (gamma decode)",
            variable=self.direction_var, value=cs.SRGB_TO_LINEAR,
        ).pack(anchor="w")
        ttk.Radiobutton(
            dir_frame, text="Linear  →  sRGB  (gamma encode)",
            variable=self.direction_var, value=cs.LINEAR_TO_SRGB,
        ).pack(anchor="w")

        # 옵션
        opt_frame = ttk.Frame(frm)
        opt_frame.grid(row=3, column=0, columnspan=3, sticky="ew", **pad)
        ttk.Checkbutton(opt_frame, text="하위 폴더 포함", variable=self.recursive_var).pack(side="left", padx=(0, 16))
        ttk.Checkbutton(opt_frame, text="기존 파일 덮어쓰기", variable=self.overwrite_var).pack(side="left")

        # 실행 버튼
        self.run_btn = ttk.Button(frm, text="변환 시작", command=self._start)
        self.run_btn.grid(row=4, column=0, columnspan=3, sticky="ew", **pad)

        # 진행률
        self.progress = ttk.Progressbar(frm, mode="determinate")
        self.progress.grid(row=5, column=0, columnspan=3, sticky="ew", **pad)
        self.status_var = tk.StringVar(value="대기 중")
        ttk.Label(frm, textvariable=self.status_var).grid(row=6, column=0, columnspan=3, sticky="w", **pad)

        # 로그
        log_frame = ttk.LabelFrame(frm, text="로그", padding=4)
        log_frame.grid(row=7, column=0, columnspan=3, sticky="nsew", **pad)
        frm.rowconfigure(7, weight=1)
        self.log = tk.Text(log_frame, height=10, wrap="none", state="disabled")
        self.log.pack(side="left", fill="both", expand=True)
        sb = ttk.Scrollbar(log_frame, command=self.log.yview)
        sb.pack(side="right", fill="y")
        self.log.configure(yscrollcommand=sb.set)

    def _pick_input(self):
        d = filedialog.askdirectory(initialdir=self.input_var.get() or ".")
        if d:
            self.input_var.set(d)

    def _pick_output(self):
        d = filedialog.askdirectory(initialdir=self.output_var.get() or ".")
        if d:
            self.output_var.set(d)

    # --------------------------------------------------------------- worker
    def _start(self):
        if self._worker and self._worker.is_alive():
            return

        input_dir = self.input_var.get().strip()
        output_dir = self.output_var.get().strip()
        if not os.path.isdir(input_dir):
            self._append_log(f"[오류] 입력 디렉터리가 없습니다: {input_dir}\n")
            return

        self._set_running(True)
        self.progress.configure(value=0, maximum=100)
        self._clear_log()

        direction = self.direction_var.get()
        recursive = self.recursive_var.get()
        overwrite = self.overwrite_var.get()

        def progress_cb(idx, total, src, status, message):
            self._msg_queue.put(("progress", (idx, total, src, status, message)))

        def run():
            try:
                summary = cs.convert_directory(
                    input_dir, output_dir, direction,
                    recursive=recursive, overwrite=overwrite,
                    progress=progress_cb,
                )
                self._msg_queue.put(("done", summary))
            except Exception as e:  # noqa: BLE001
                self._msg_queue.put(("fatal", str(e)))

        self._worker = threading.Thread(target=run, daemon=True)
        self._worker.start()

    def _drain_queue(self):
        """워커 스레드 메시지를 UI 스레드에서 처리."""
        try:
            while True:
                kind, payload = self._msg_queue.get_nowait()
                if kind == "progress":
                    idx, total, src, status, message = payload
                    if total > 0:
                        self.progress.configure(maximum=total, value=idx)
                    self.status_var.set(f"{idx}/{total} 처리 중...")
                    tag = {"ok": "OK", "skip": "건너뜀", "error": "오류"}.get(status, status)
                    name = os.path.basename(src)
                    line = f"[{tag}] {name}"
                    if status == "error":
                        line += f"  →  {message}"
                    self._append_log(line + "\n")
                elif kind == "done":
                    s = payload
                    self.status_var.set(
                        f"완료 — 성공 {s['ok']} / 건너뜀 {s['skip']} / 실패 {s['error']}"
                    )
                    self._append_log(
                        f"\n=== 완료: 성공 {s['ok']}, 건너뜀 {s['skip']}, 실패 {s['error']} ===\n"
                    )
                    self._set_running(False)
                elif kind == "fatal":
                    self.status_var.set("실패")
                    self._append_log(f"[치명적 오류] {payload}\n")
                    self._set_running(False)
        except queue.Empty:
            pass
        self.root.after(80, self._drain_queue)

    # ----------------------------------------------------------------- util
    def _set_running(self, running: bool):
        self.run_btn.configure(state="disabled" if running else "normal",
                               text="변환 중..." if running else "변환 시작")

    def _clear_log(self):
        self.log.configure(state="normal")
        self.log.delete("1.0", "end")
        self.log.configure(state="disabled")

    def _append_log(self, text: str):
        self.log.configure(state="normal")
        self.log.insert("end", text)
        self.log.see("end")
        self.log.configure(state="disabled")


def main():
    root = tk.Tk()
    ConverterGUI(root)
    root.mainloop()


if __name__ == "__main__":
    main()

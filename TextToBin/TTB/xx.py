import tkinter as tk
from tkinter import filedialog, messagebox, ttk
from PIL import Image, ImageTk
import json
import os
import struct  # 바이너리 패킹을 위한 모듈
import random

class ProductionSpriteViewer:
    def __init__(self, root):
        self.root = root
        self.root.title("Pro Sprite Viewer & Binary Exporter")
        self.root.geometry("450x600")
        
        # 데이터 상태
        self.current_frames_data = [] # [(x, y, w, h), ...]
        self.pil_frames = []
        self.fps = 12
        self.duration = 1000
        self.frame_cnt = 0

        self.setup_ui()

    def setup_ui(self):
        main_frame = ttk.Frame(self.root, padding="20")
        main_frame.pack(fill="both", expand=True)

        ttk.Label(main_frame, text="Sprite Sheet Exporter", font=("Arial", 16, "bold")).pack(pady=10)

        # 1. 파일 선택
        file_group = ttk.LabelFrame(main_frame, text="1. File Selection", padding=10)
        file_group.pack(fill="x", pady=5)
        
        self.img_path_var = tk.StringVar(value="")
        ttk.Entry(file_group, textvariable=self.img_path_var, state='readonly').pack(side="left", fill="x", expand=True, padx=5)
        ttk.Button(file_group, text="찾기", command=self.load_image_path).pack(side="right")

        # 2. 애니메이션 타입 선택 및 설정
        self.type_var = tk.StringVar(value="loop")
        type_group = ttk.LabelFrame(main_frame, text="2. Play Type Settings", padding=10)
        type_group.pack(fill="x", pady=5)
        
        ttk.Radiobutton(type_group, text="Loop", variable=self.type_var, value="loop").grid(row=0, column=0)
        ttk.Radiobutton(type_group, text="Once", variable=self.type_var, value="once").grid(row=0, column=1)
        ttk.Radiobutton(type_group, text="RandomAdvance", variable=self.type_var, value="randomAdvance").grid(row=0, column=2)

        # 3. 모드 선택 및 설정
        self.mode_var = tk.StringVar(value="grid")
        mode_group = ttk.LabelFrame(main_frame, text="3. Slicing Settings", padding=10)
        mode_group.pack(fill="x", pady=5)
        
        ttk.Radiobutton(mode_group, text="그리드", variable=self.mode_var, value="grid", command=self.toggle_mode).grid(row=0, column=0)
        ttk.Radiobutton(mode_group, text="JSON", variable=self.mode_var, value="atlas", command=self.toggle_mode).grid(row=0, column=1)

        self.settings_frame = ttk.Frame(mode_group)
        self.settings_frame.grid(row=1, column=0, columnspan=2, sticky="ew", pady=5)
        self.show_grid_settings()

        # 4. 재생 및 내보내기
        play_group = ttk.LabelFrame(main_frame, text="4. Control", padding=10)
        play_group.pack(fill="x", pady=5)
        
        ttk.Label(play_group, text="FPS:").pack(side="left")
        self.fps_scale = ttk.Scale(play_group, from_=1, to=60, orient="horizontal", command=self.update_fps)
        self.fps_scale.set(12)
        self.fps_scale.pack(side="left", fill="x", expand=True, padx=10)
        self.fps_label = ttk.Label(play_group, text="12")
        self.fps_label.pack(side="left")

        ttk.Label(play_group, text="Duration:").pack(side="left")
        self.duration_entry = ttk.Entry(play_group)
        self.duration_entry.pack(side="left", fill="x", expand=True, padx=10)

        play_optional_group = ttk.LabelFrame(main_frame, text="5. Random Advance Option", padding=10)
        play_optional_group.pack(fill="x", pady=5)

        ttk.Label(play_optional_group, text="FrameCnt(RA):").pack(side="left")
        self.frame_cnt_entry = ttk.Entry(play_optional_group)
        self.frame_cnt_entry.pack(side="left", fill="x", expand=True, padx=10)

        ttk.Button(main_frame, text="프리뷰 재생하기", command=self.process_and_open).pack(fill="x", pady=5)
        ttk.Button(main_frame, text="바이너리(.anim) 추출", command=self.export_binary, style="Accent.TButton").pack(fill="x", pady=5)

    def toggle_mode(self):
        for widget in self.settings_frame.winfo_children(): widget.destroy()
        if self.mode_var.get() == "grid": self.show_grid_settings()
        else: self.show_atlas_settings()

    def show_grid_settings(self):
        f = self.settings_frame
        self.grid_inputs = {}
        fields = [("W", "64"), ("H", "64"), ("OffX", "0"), ("OffY", "0")]
        for i, (label, default) in enumerate(fields):
            ttk.Label(f, text=label).grid(row=0, column=i*2, padx=2)
            ent = ttk.Entry(f, width=5)
            ent.insert(0, default)
            ent.grid(row=0, column=i*2+1, padx=2)
            self.grid_inputs[label] = ent

    def show_atlas_settings(self):
        self.json_path_var = tk.StringVar(value="")
        ttk.Entry(self.settings_frame, textvariable=self.json_path_var, width=30).pack(side="left", padx=5)
        ttk.Button(self.settings_frame, text="JSON 선택", command=lambda: self.json_path_var.set(filedialog.askopenfilename())).pack(side="right")

    def load_image_path(self):
        path = filedialog.askopenfilename()
        if path: self.img_path_var.set(path)

    def update_fps(self, e):
        val = int(float(self.fps_scale.get()))
        self.fps = val
        self.fps_label.config(text=str(val))

    def process_and_open(self):
        try:
            self.pil_frames, self.current_frames_data = self.slice_logic()
            PreviewWindow(self.root, self.pil_frames, self.type_var.get(), self.fps, self.duration)
        except Exception as e:
            messagebox.showerror("Error", str(e))

    def slice_logic(self):
        self.duration = int(self.duration_entry.get())
        if self.type_var.get() == "randomAdvance":
            self.frame_cnt = int(self.frame_cnt_entry.get())
        img_path = self.img_path_var.get()
        full_img = Image.open(img_path).convert("RGBA")
        frames_imgs = []
        frames_coords = []
        frame_time = 1000 // self.fps

        if self.mode_var.get() == "grid":
            w = int(self.grid_inputs["W"].get())
            h = int(self.grid_inputs["H"].get())
            ox = int(self.grid_inputs["OffX"].get())
            oy = int(self.grid_inputs["OffY"].get())
            i = 0
            for y in range(oy, full_img.height - h + 1, h):
                for x in range(ox, full_img.width - w + 1, w):
                    box = (x, y, x + w, y + h)
                    frames_imgs.append(full_img.crop(box))
                    if (self.type_var.get() == "randomAdvance"):
                        frames_coords.append((x, y, w, h, self.fps))
                    else:
                        frames_coords.append((x, y, w, h, self.fps * i))
                    i += 1
                    if (self.type_var.get() != "randomAdvance" and
                        self.duration <= frame_time * i):
                        break
                    if (self.type_var.get() == "randomAdvance" and
                        len(frames_imgs) == self.frame_cnt):
                        break
                if (self.type_var.get() != "randomAdvance" and
                    self.duration <= frame_time * i):
                    break
                if (self.type_var.get() == "randomAdvance" and
                    len(frames_imgs) == self.frame_cnt):
                    break
        else:
            with open(self.json_path_var.get(), 'r') as f:
                data = json.load(f)
                frames_dict = data['frames']
                source = frames_dict.values() if isinstance(frames_dict, dict) else frames_dict
                i = 0
                for item in source:
                    f_info = item['frame']
                    x, y, w, h = f_info['x'], f_info['y'], f_info['w'], f_info['h']
                    frames_imgs.append(full_img.crop((x, y, x + w, y + h)))
                    if (self.type_var.get() == "randomAdvance"):
                        frames_coords.append((x, y, w, h, self.fps))
                    else:
                        frames_coords.append((x, y, w, h, self.fps * i))
                    i += 1
                    if (self.type_var.get() != "randomAdvance" and
                        self.duration <= frame_time * i):
                        break
                    if (self.type_var.get() == "randomAdvance" and
                        len(frames_imgs) == self.frame_cnt):
                        break
        
        return frames_imgs, frames_coords
    
    def write_string(self, f, s):
        f.write(struct.pack("B", len(s)))
        f.write(s)

    def export_binary(self):
        """데이터를 커스텀 바이너리 형식으로 추출"""
        if not self.current_frames_data:
            messagebox.showwarning("주의", "먼저 프리뷰를 실행하여 데이터를 로드하세요.")
            return

        save_path = filedialog.asksaveasfilename(defaultextension=".anim", filetypes=[("Animation File", "*.anim")])
        if not save_path: return

        try:
            with open(save_path, "wb") as f:
                # 1. 파일 시작 태그
                self.write_string(f, b"<SpriteAnimation:>")

                # 2. 이미지 파일명 (uint8 길이 + 문자열)
                self.write_string(f, b"<SpriteSheet:>")
                img_name = os.path.basename(self.img_path_var.get()).encode('utf-8')
                self.write_string(f, img_name)
                self.write_string(f, b"</SpriteSheet>")

                # 3. 애니메이션 메타데이터
                self.write_string(f, b"<Type:>")
                self.write_string(f, self.type_var.get().encode('utf-8'))
                self.write_string(f, b"</Type>")

                self.write_string(f, b"<FrameTime:>")
                f.write(struct.pack("I", 1000 // self.fps))
                self.write_string(f, b"</FrameTime>")

                self.write_string(f, b"<Duration:>")
                f.write(struct.pack("I", self.duration))
                self.write_string(f, b"</Duration>")

                self.write_string(f, b"<FrameCnt:>")
                f.write(struct.pack("I", len(self.current_frames_data)))
                self.write_string(f, b"</FrameCnt>")

                # 4. 프레임 데이터 반복
                for x, y, w, h, t in self.current_frames_data:
                    f.write(b"<Frame:>")
                    f.write(struct.pack("IIIII", x, y, w, h, t)) # x, y, w, h, t를 각각 uint32로
                    f.write(b"</Frame>")

                # 5. 파일 종료 태그
                f.write(b"</SpriteAnimation>")

            messagebox.showinfo("성공", f"바이너리 파일이 성공적으로 추출되었습니다:\n{save_path}")
        except Exception as e:
            messagebox.showerror("추출 실패", f"오류 발생: {e}")

class PreviewWindow(tk.Toplevel):
    def __init__(self, parent, frames, type_str, fps, duration):
        super().__init__(parent)
        self.title("Animation Preview")
        self.frames = [ImageTk.PhotoImage(f.resize((f.width*2, f.height*2), Image.NEAREST)) for f in frames]
        self.fps = fps
        self.frame_time = 1000 / fps
        self.idx = 0
        self.lbl = tk.Label(self, bg="black")
        self.lbl.pack(padx=20, pady=20)
        self.type_str = type_str
        self.duration = duration
        self.animate()

    def animate(self):
        print(self.idx)
        print(self.frame_time)
        print(self.duration)
        if not self.winfo_exists(): return
        self.lbl.config(image=self.frames[self.idx])
        if self.type_str == "loop":
            self.idx = (self.idx + 1) % len(self.frames)
        elif self.type_str == "once":
            self.idx = min(self.idx + 1, len(self.frames) - 1)
        else:   # RandomAdvance
            m = len(self.frames) * self.frame_time / self.duration
            print(m)
            self.idx += int(random.gauss(m, m))
            self.idx = min(self.idx, len(self.frames) - 1)
            self.idx = max(self.idx, 0)
        self.after(int(self.frame_time), self.animate)

if __name__ == "__main__":
    root = tk.Tk()
    # 스타일 개선 (선택사항)
    style = ttk.Style()
    style.configure("Accent.TButton", foreground="blue", font=('Arial', 10, 'bold'))
    app = ProductionSpriteViewer(root)
    root.mainloop()
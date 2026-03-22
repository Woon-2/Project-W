import tkinter as tk
from tkinter import filedialog, messagebox
from PIL import Image
import numpy as np
import os

# ----------------------------
# Color Space Conversion
# ----------------------------

def srgb_to_linear(c):
    """sRGB -> Linear"""
    threshold = 0.04045
    below = c <= threshold
    above = c > threshold
    result = np.zeros_like(c)

    result[below] = c[below] / 12.92
    result[above] = ((c[above] + 0.055) / 1.055) ** 2.4

    return result

def linear_to_srgb(c):
    """Linear -> sRGB"""
    threshold = 0.0031308
    below = c <= threshold
    above = c > threshold
    result = np.zeros_like(c)

    result[below] = c[below] * 12.92
    result[above] = 1.055 * (c[above] ** (1 / 2.4)) - 0.055

    return result

# ----------------------------
# Image Processing
# ----------------------------

def process_image(input_path, output_path, mode):
    try:
        img = Image.open(input_path).convert("RGBA")
        arr = np.array(img).astype(np.float32) / 255.0

        rgb = arr[..., :3]
        alpha = arr[..., 3:]

        if mode == "srgb_to_linear":
            rgb = srgb_to_linear(rgb)
        elif mode == "linear_to_srgb":
            rgb = linear_to_srgb(rgb)
        else:
            raise ValueError("Invalid mode")

        result = np.concatenate([rgb, alpha], axis=-1)
        result = np.clip(result * 255.0, 0, 255).astype(np.uint8)

        Image.fromarray(result).save(output_path)

        messagebox.showinfo("완료", f"변환 완료!\n저장 위치:\n{output_path}")

    except Exception as e:
        messagebox.showerror("오류", str(e))

# ----------------------------
# GUI
# ----------------------------

class App:
    def __init__(self, root):
        self.root = root
        self.root.title("PNG Color Space Converter")

        self.input_path = tk.StringVar()
        self.output_path = tk.StringVar()
        self.mode = tk.StringVar(value="srgb_to_linear")

        # Input
        tk.Label(root, text="입력 파일").grid(row=0, column=0)
        tk.Entry(root, textvariable=self.input_path, width=50).grid(row=0, column=1)
        tk.Button(root, text="찾기", command=self.browse_input).grid(row=0, column=2)

        # Output
        tk.Label(root, text="출력 파일").grid(row=1, column=0)
        tk.Entry(root, textvariable=self.output_path, width=50).grid(row=1, column=1)
        tk.Button(root, text="저장 위치", command=self.browse_output).grid(row=1, column=2)

        # Mode
        tk.Label(root, text="변환 방식").grid(row=2, column=0)
        tk.Radiobutton(root, text="sRGB → Linear", variable=self.mode, value="srgb_to_linear").grid(row=2, column=1, sticky="w")
        tk.Radiobutton(root, text="Linear → sRGB", variable=self.mode, value="linear_to_srgb").grid(row=3, column=1, sticky="w")

        # Run button
        tk.Button(root, text="변환 실행", command=self.run, height=2).grid(row=4, column=1, pady=10)

    def browse_input(self):
        path = filedialog.askopenfilename(filetypes=[("PNG files", "*.png")])
        if path:
            self.input_path.set(path)

    def browse_output(self):
        path = filedialog.asksaveasfilename(defaultextension=".png", filetypes=[("PNG files", "*.png")])
        if path:
            self.output_path.set(path)

    def run(self):
        if not self.input_path.get() or not self.output_path.get():
            messagebox.showwarning("경고", "입력/출력 경로를 모두 지정하세요.")
            return

        process_image(self.input_path.get(), self.output_path.get(), self.mode.get())


# ----------------------------
# Main
# ----------------------------

if __name__ == "__main__":
    root = tk.Tk()
    app = App(root)
    root.mainloop()
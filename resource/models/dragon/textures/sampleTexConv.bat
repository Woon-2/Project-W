@echo off
setlocal

REM set path of input/output dierectories
set "input_folder=%cd%"
set "output_folder=%cd%"

REM set texconv path
set "texconv_path=texconv.exe"

REM transform all images
for %%f in ("%input_folder%\*.png" "%input_folder%\*.jpg" "%input_folder%\*.jpeg" "%input_folder%\*.bmp" "%input_folder%\*.tga") do (
    "%texconv_path%" -o "%output_folder%" "%%f" -f BC1_UNORM -y
)

echo transformation complete!
pause
@echo off
setlocal EnableExtensions EnableDelayedExpansion

set "texture_dir=%~dp0"
set "texture_dir=%texture_dir:~0,-1%"
set "texconv_path=texconv.exe"
set /a converted=0

if "%~1"=="" goto convert_all

:convert_args
if "%~1"=="" goto done
call :convert_one "%~f1"
if errorlevel 1 exit /b 1
shift
goto convert_args

:convert_all
echo No input file was specified.
echo Converting all PNG files in "%texture_dir%".
echo.
for %%F in ("%texture_dir%\*.png") do (
    call :convert_one "%%~fF"
    if errorlevel 1 exit /b 1
)
goto done

:convert_one
set "src_png=%~1"
if not exist "!src_png!" (
    echo Missing input: "!src_png!"
    exit /b 1
)

set "src_ext=%~x1"
if /I not "!src_ext!"==".png" (
    echo Skipping non-PNG input: "!src_png!"
    exit /b 0
)

set "src_name=%~n1"
set "base_name=!src_name:_UnityImported=!"
set "dst_dir=%~dp1"
set "dst_dir=!dst_dir:~0,-1!"
set "dst_dds=!dst_dir!\!base_name!.dds"
set "tmp_stem=!base_name!_dds_tmp_!RANDOM!!RANDOM!"
set "tmp_png=!dst_dir!\!tmp_stem!.png"
set "tmp_dds=!dst_dir!\!tmp_stem!.dds"

echo Converting "!src_png!" -^> "!dst_dds!"

copy /Y "!src_png!" "!tmp_png!" > nul
if errorlevel 1 exit /b 1

"%texconv_path%" "!tmp_png!" -o "!dst_dir!" -f R8G8B8A8_UNORM -y
if errorlevel 1 (
    del /F /Q "!tmp_png!" > nul 2> nul
    exit /b 1
)

if not exist "!tmp_dds!" (
    echo texconv did not create "!tmp_dds!".
    del /F /Q "!tmp_png!" > nul 2> nul
    exit /b 1
)

if exist "!dst_dds!" (
    del /F /Q "!dst_dds!"
    if exist "!dst_dds!" (
        echo Failed to delete existing "!dst_dds!".
        echo Close any process using the DDS file, or run this batch from an elevated terminal.
        del /F /Q "!tmp_png!" > nul 2> nul
        del /F /Q "!tmp_dds!" > nul 2> nul
        exit /b 1
    )
)

move /Y "!tmp_dds!" "!dst_dds!" > nul
if errorlevel 1 (
    del /F /Q "!tmp_png!" > nul 2> nul
    exit /b 1
)

del /F /Q "!tmp_png!" > nul 2> nul
set /a converted+=1
echo Wrote "!dst_dds!".
exit /b 0

:usage
echo Usage:
echo   convertEffectdds.bat Smoke24.png
echo   convertEffectdds.bat Smoke24.png Noise43b.png
echo   convertEffectdds.bat
echo.
echo Output format:
echo   R8G8B8A8_UNORM
echo.
echo Notes:
echo   With no input arguments, all PNG files in this folder are converted.
echo   If the input name ends with _UnityImported.png, the output removes that suffix.
echo   Example: Smoke24_UnityImported.png -^> Smoke24.dds
exit /b 1

:done
echo Converted %converted% file(s).
exit /b 0

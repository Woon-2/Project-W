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
shift
goto convert_args

:convert_all
for %%F in ("%texture_dir%\*_UnityImported.png") do (
    call :convert_one "%%~fF"
)
goto done

:convert_one
set "src_png=%~1"
if not exist "!src_png!" (
    echo Missing input: "!src_png!"
    exit /b 1
)

set "src_name=%~n1"
set "base_name=!src_name:_UnityImported=!"
if "!base_name!"=="!src_name!" (
    echo Skipping "!src_png!" because the file name does not end with _UnityImported.png.
    exit /b 0
)

set "dst_dds=%texture_dir%\!base_name!.dds"
set "tmp_stem=!base_name!_dds_tmp_!RANDOM!!RANDOM!"
set "tmp_png=%texture_dir%\!tmp_stem!.png"
set "tmp_dds=%texture_dir%\!tmp_stem!.dds"

echo Converting "!src_png!" -^> "!dst_dds!"

copy /Y "!src_png!" "!tmp_png!" > nul
if errorlevel 1 exit /b 1

"%texconv_path%" "!tmp_png!" -o "%texture_dir%" -f R8G8B8A8_UNORM -y
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
        exit /b 1
    )
)

move /Y "!tmp_dds!" "!dst_dds!"
if errorlevel 1 (
    del /F /Q "!tmp_png!" > nul 2> nul
    exit /b 1
)

del /F /Q "!tmp_png!" > nul 2> nul
set /a converted+=1
echo Wrote "!dst_dds!".
exit /b 0

:done
if %converted% EQU 0 (
    echo No Unity imported PNG files were converted.
    echo Expected names like Smoke12_UnityImported.png in "%texture_dir%".
)
exit /b 0

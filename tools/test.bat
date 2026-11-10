@echo off
setlocal
set CONFIG=%~1
if "%CONFIG%"=="" set CONFIG=Release

REM The OpenCV DLLs must be on PATH, or every test fails with 0xc0000135
REM (DLL not found). This does not affect the build, only the run.
set "PATH=C:\Program Files\opencv\build\x64\vc16\bin;%PATH%"

call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >NUL 2>&1
if errorlevel 1 (
    echo Failed to initialise the MSVC environment.
    exit /b 1
)

cd /d "%~dp0.."
ctest --test-dir build -C %CONFIG% --output-on-failure
exit /b %ERRORLEVEL%

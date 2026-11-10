@echo off
setlocal
REM Usage: tools\build.bat [Debug|Release]
set CONFIG=%~1
if "%CONFIG%"=="" set CONFIG=Release

call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >NUL 2>&1
if errorlevel 1 (
    echo Failed to initialise the MSVC environment.
    exit /b 1
)

cd /d "%~dp0.."
cmake -S . -B build -DCMAKE_BUILD_TYPE=%CONFIG%
if errorlevel 1 exit /b 1

cmake --build build --config %CONFIG% --parallel
exit /b %ERRORLEVEL%

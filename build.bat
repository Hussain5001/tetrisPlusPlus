@echo off
rem Builds Tetris++ on Windows and starts it with hand control.
rem Needs Git, CMake and the Visual Studio C++ build tools; see README.md,
rem "Play on Windows". Run it again after changing the code.
setlocal
pushd "%~dp0"

where cmake >nul 2>nul
if errorlevel 1 (
    echo CMake was not found. Install the tools once, in PowerShell:
    echo     winget install Kitware.CMake
    echo     winget install Microsoft.VisualStudio.2022.BuildTools --override "--add Microsoft.VisualStudio.Workload.VCTools --includeRecommended --passive"
    echo then open a new terminal and run build.bat again.
    pause
    exit /b 1
)

if not exist "vendor\raylib\src\raylib.h" (
    echo Downloading raylib and json...
    git submodule update --init --recursive
    if errorlevel 1 (
        echo Could not download the libraries. Is Git installed? winget install Git.Git
        pause
        exit /b 1
    )
)

if not exist "build\CMakeCache.txt" (
    cmake -B build
    if errorlevel 1 (
        echo CMake could not find a C++ compiler. Install the Visual Studio build tools, see above.
        pause
        exit /b 1
    )
)

cmake --build build --config Release
if errorlevel 1 (
    pause
    exit /b 1
)

set "EXE=build\Release\Tetris.exe"
if not exist "%EXE%" set "EXE=build\Tetris.exe"
start "" "%EXE%" --gestures

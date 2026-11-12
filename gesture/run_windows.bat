@echo off
rem Runs the Tetris++ hand tracking (camera -> gestures -> game). The game
rem starts this for you; you can also double-click it while the game is open.
rem Extra arguments are passed on to hand_control.py, e.g.:
rem     run_windows.bat --mode point
setlocal
pushd "%~dp0"

set "VENV=%LOCALAPPDATA%\tetrisplusplus\venv"
set "STAMP=%VENV%\requirements.installed.txt"

rem The packages are installed once, and again whenever requirements.txt
rem changes: the environment is then rebuilt so no old package is left over.
fc /b requirements.txt "%STAMP%" >nul 2>nul
if errorlevel 1 (
    echo Setting up Python for hand tracking, this takes a minute...
    if exist "%VENV%" rmdir /s /q "%VENV%"
    py -3.12 -m venv "%VENV%" 2>nul || py -3 -m venv "%VENV%" 2>nul || python -m venv "%VENV%"
    if not exist "%VENV%\Scripts\python.exe" (
        echo Could not create a Python environment. Install Python 3.10-3.12 from python.org first.
        pause
        exit /b 1
    )
    "%VENV%\Scripts\python.exe" -m pip install --upgrade pip
    "%VENV%\Scripts\python.exe" -m pip install -r requirements.txt
    if errorlevel 1 (
        echo Could not install the packages. Check your internet connection and try again.
        pause
        exit /b 1
    )
    copy /y requirements.txt "%STAMP%" >nul
)

"%VENV%\Scripts\python.exe" hand_control.py %*
if errorlevel 1 pause
popd

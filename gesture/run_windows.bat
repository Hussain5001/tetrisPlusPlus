@echo off
rem Runs the Tetris++ hand tracking on Windows. Use this when the game runs in
rem WSL, which can't access the webcam. Works when opened from the WSL folder
rem (\\wsl$\...) too. Extra arguments are passed on, e.g.:
rem     run_windows.bat --host 172.20.1.5
setlocal
pushd "%~dp0"

set "VENV=%LOCALAPPDATA%\tetrisplusplus\venv"
if not exist "%VENV%\Scripts\python.exe" (
    echo Setting up Python for hand tracking, this takes a minute the first time...
    py -3.12 -m venv "%VENV%" 2>nul || py -3 -m venv "%VENV%" 2>nul || python -m venv "%VENV%"
    if not exist "%VENV%\Scripts\python.exe" (
        echo Could not create a Python environment. Install Python 3.10-3.12 from python.org first.
        pause
        exit /b 1
    )
    "%VENV%\Scripts\python.exe" -m pip install --upgrade pip
    "%VENV%\Scripts\python.exe" -m pip install -r requirements.txt
)

"%VENV%\Scripts\python.exe" hand_control.py %*
if errorlevel 1 pause
popd

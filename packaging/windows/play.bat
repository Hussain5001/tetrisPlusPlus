@echo off
rem Starts Tetris++. Hand control needs Python 3.10-3.12 from python.org
rem (tick "Add python.exe to PATH"); without it you can still play with
rem the keyboard.
setlocal
pushd "%~dp0"

where py >nul 2>nul
if errorlevel 1 where python >nul 2>nul
if errorlevel 1 (
    echo Python was not found, so hand control is off - keyboard only.
    echo Install Python 3.12 from https://www.python.org/downloads/ to play hands-free.
    start "" Tetris.exe
    timeout /t 6 >nul
    exit /b 0
)

rem The first start installs MediaPipe for the camera (a minute or two);
rem the game's HAND CONTROL box turns on when it's ready.
start "" Tetris.exe --gestures

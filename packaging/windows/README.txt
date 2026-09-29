Tetris++ for Windows
====================

Play:          double-click play.bat
Keyboard only: double-click Tetris.exe

Hand control (camera) needs Python 3.10-3.12 from https://www.python.org/downloads/
(tick "Add python.exe to PATH" in the installer). The first start downloads
MediaPipe and a hand model, which takes a minute or two; a minimised
"Tetris++ hand control" window does this. When it's ready, a camera preview
opens and the game's HAND CONTROL box turns on.

Gestures
  Point at the camera with your index finger (other fingers curled in):
  flick finger left/right move one column
  flick the hand down     hard drop
  pinch (thumb to tip)    rotate
  hold a fist             pause / select in menus
  Press m in the camera window to switch to point-and-hold, palm or
  position mode.

Keyboard
  left / right            move (hold to repeat)
  up or X                 rotate
  down                    soft drop
  space                   hard drop
  esc or P                pause
  enter                   select in menus

Your saved Zen game and scores are stored in this folder.
Source code: https://github.com/Hussain5001/tetrisPlusPlus

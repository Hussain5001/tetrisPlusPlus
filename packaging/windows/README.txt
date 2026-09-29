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
  Point your index finger up (other fingers curled in):
  swipe finger left/right move one column (bringing it back doesn't count)
  dip finger down and up  hard drop / select in menus
  pinch (thumb to tip)    rotate
  open palm, hold still   pause / resume
  fist or relaxed hand    nothing, rest any time
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

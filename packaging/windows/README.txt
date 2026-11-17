Tetris++ for Windows
====================

Play: double-click Tetris.exe

Hand control (camera)
  Press H in the game to switch hand control on, and H again to switch it
  off. The keyboard works the whole time, so you can change between the two
  whenever you like; nothing has to be restarted or closed.

  It needs Python 3.10-3.12 from https://www.python.org/downloads/ (tick
  "Add python.exe to PATH" in the installer). The first time, a minimised
  "Tetris++ hand control" window downloads MediaPipe and a hand model, which
  takes a minute or two. Then a camera preview opens and the status line at
  the bottom of the game changes from "hand: starting" to "hand: ready".
  The game remembers the choice, and the camera closes when the game does.

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
  H                       hand control on / off
  M                       sound on / off
  F11                     fullscreen

Your saved Zen game and scores are stored in this folder.
Source code: https://github.com/Hussain5001/tetrisPlusPlus

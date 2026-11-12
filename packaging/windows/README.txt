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
  Hold your open hand in front of the camera:
  move it left / right    the piece goes to the column under your hand
  pinch (thumb to tip)    rotate
  close it into a fist    hard drop (open it again after)
  in menus                point at a line, make a fist to choose it
  take it away            the game pauses after a moment
  The grey box in the camera window is the zone your hand moves in; press c
  there to centre it on your hand.

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

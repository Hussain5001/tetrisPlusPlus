#include <cstring>
#include <iostream>

#include "App.h"
#include "UnitTestBoard.h"
#include "UnitTestModes.h"
#include "UnitTestTetromino.h"

// Usage:
//   Tetris              play the game (gestures work if the sidecar is running)
//   Tetris --gestures   also start gesture/hand_control.py
//   Tetris --test       run the unit tests instead of the game
int main(int argc, char** argv) {
  bool run_tests = false;
  App tetr;
  for (int i = 1; i < argc; i++) {
    if (std::strcmp(argv[i], "--test") == 0) run_tests = true;
    if (std::strcmp(argv[i], "--gestures") == 0) tetr.launch_gesture_sidecar = true;
  }

  if (run_tests) {
    UnitTestBoard test_board;
    test_board.run_test_board();

    UnitTestTetromino test_tetromino;
    test_tetromino.run_test_tetromino();

    UnitTestModes test_modes;
    test_modes.run_test_modes();
    return 0;
  }

  tetr.run_menu();
  return 0;
}

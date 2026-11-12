#include <cstring>
#include <iostream>

#include "App.h"
#include "tests/TestData.h"
#include "tests/TestGame.h"
#include "tests/TestInput.h"
#include "tests/TestLegacy.h"
#include "tests/TestModes.h"

// Usage:
//   Tetris              play the game. Hand control is switched on and off
//                       inside it (the H key) and remembered in profile.json
//   Tetris --test       run the unit tests instead of the game
int main(int argc, char** argv) {
  bool run_tests = false;
  App tetr;
  for (int i = 1; i < argc; i++) {
    if (std::strcmp(argv[i], "--test") == 0) run_tests = true;
  }

  if (run_tests) {
    // Every check counts; any failure sets the exit code
    TestLegacy().run();
    TestGame().run();
    TestModes().run();
    TestData().run();
    TestInput().run();
    std::cout << "\n" << test::stats().passed << " checks passed, " << test::stats().failed
              << " failed" << std::endl;
    return test::stats().failed > 0 ? 1 : 0;
  }

  tetr.run_menu();
  return 0;
}

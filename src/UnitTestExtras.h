#include <cmath>
#include <iostream>
#include <string>

#include "Tetromino/Block_I.h"
#include "zen_mode.h"

// Tests for the features added in Tetris++: Zen levels, wall kicks and the
// lock delay. Same style as the other unit tests: each prints passed/FAILED.
class UnitTestExtras {
 public:
  void run_test_extras() {
    run_test_zen_levels();
    run_test_zen_level_up();
    run_test_wall_kick();
    run_test_lock_delay();
  }

 private:
  void report(const std::string& name, bool ok) {
    std::cout << "Test for " << name << (ok ? " passed!" : " FAILED") << std::endl;
  }

  static bool near(double a, double b) { return std::fabs(a - b) < 1e-9; }

  // The chosen start level sets the speed and the score multiplier
  void run_test_zen_levels() {
    ZenMode classic;
    report("zen default level", classic.level == 1 &&
                                    near(classic.drop_interval, 0.80) &&
                                    near(classic.score_multiplier, 1));
    ZenSettings settings;
    settings.start_level = 5;
    ZenMode fast(settings);
    report("zen start level", fast.level == 5 && near(fast.drop_interval, 0.48) &&
                                  near(fast.score_multiplier, 3));
    report("zen speed keeps increasing after level 10",
           ZenMode::drop_interval_for(15) < ZenMode::drop_interval_for(10) &&
               near(ZenMode::drop_interval_for(99), 0.05));
  }

  // Levels go up according to the chosen pace
  void run_test_zen_level_up() {
    ZenSettings brutal;
    brutal.start_level = 2;
    brutal.lines_per_level = 3;
    ZenMode game(brutal);
    game.total_lines_cleared = 2;
    game.update_level();
    bool before = game.level == 2;
    game.total_lines_cleared = 7;  // two more levels
    game.update_level();
    report("zen level up with pace", before && game.level == 4 &&
                                         near(game.drop_interval, 0.55) &&
                                         near(game.score_multiplier, 2.5));
  }

  // Rotating an I block next to the wall kicks it back inside the board
  void run_test_wall_kick() {
    ZenMode game;
    game.current_block = BlockI();
    game.current_block.move(5, 0);
    game.rotate_and_bound_chk();  // vertical
    for (int i = 0; i < 10; i++) game.move_right();
    bool rotated = game.rotate_and_bound_chk();  // back to horizontal
    bool inside = true;
    for (Position cell : game.current_block.get_current_position()) {
      if (cell.column < 0 || cell.column > 9) inside = false;
    }
    report("wall kick", rotated && inside);
  }

  // A landed block waits lock_delay seconds before locking, and moving it
  // restarts the wait
  void run_test_lock_delay() {
    ZenMode game;
    game.lock_delay = 0.5;
    game.drop_interval = 0.01;
    double t = 0;
    int id = game.piece_id;
    int row = -100;
    // Fall until the block stops moving (it has landed)
    while (game.current_block.get_row_offset() != row && t < 5) {
      row = game.current_block.get_row_offset();
      t += 0.02;
      game.tick(t);
    }
    bool still_falling_piece = game.piece_id == id;
    game.tick(t + 0.3);
    bool waiting = game.piece_id == id;
    game.move_left();  // restarts the lock delay at t + 0.3
    game.tick(t + 0.7);
    bool reset = game.piece_id == id;
    game.tick(t + 0.85);
    bool locked = game.piece_id != id;
    report("lock delay", still_falling_piece && waiting && reset && locked);
  }
};

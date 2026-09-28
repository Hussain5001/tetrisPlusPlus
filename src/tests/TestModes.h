#pragma once
#include <fstream>

#include "../first_forty_mode.h"
#include "../time_attack_mode.h"
#include "../zen_mode.h"
#include "TestRunner.h"

// Tests for the three modes: Zen levels, pace, multiplier and save files;
// the timer used by the timed modes; Time Attack scoring; First 40 Lines.
class TestModes {
 public:
  void run() {
    zen_levels();
    zen_level_up();
    zen_multiplier_on_line_clears();
    zen_save_and_load();
    zen_old_save_still_loads();
    zen_missing_save();
    timer_pause_and_end();
    time_attack_scoring();
    first_forty_finish();
  }

 private:
  void zen_levels() {
    ZenMode classic;
    test::check(classic.level == 1 && test::near(classic.drop_interval, 0.80) &&
                    test::near(classic.score_multiplier, 1),
                "zen default level");
    ZenSettings settings;
    settings.start_level = 5;
    ZenMode fast(settings);
    test::check(fast.level == 5 && test::near(fast.drop_interval, 0.48) &&
                    test::near(fast.score_multiplier, 3),
                "zen start level sets speed and multiplier");
    bool faster = true;
    for (int level = 1; level < 30; level++) {
      if (ZenMode::drop_interval_for(level + 1) > ZenMode::drop_interval_for(level)) faster = false;
    }
    test::check(faster && test::near(ZenMode::drop_interval_for(99), 0.05) &&
                    test::near(ZenMode::drop_interval_for(0), 0.80),
                "zen speed never slows down and stops at 0.05s");
    ZenSettings bad;
    bad.start_level = -3;
    bad.lines_per_level = 0;
    ZenMode fixed(bad);
    test::check(fixed.level == 1 && fixed.settings.lines_per_level == 1,
                "zen rejects invalid settings");
  }

  void zen_level_up() {
    ZenSettings brutal;
    brutal.start_level = 2;
    brutal.lines_per_level = 3;
    ZenMode game(brutal);
    game.total_lines_cleared = 2;
    game.update_level();
    bool before = game.level == 2;
    game.total_lines_cleared = 7;  // two more levels
    game.update_level();
    test::check(before && game.level == 4 && test::near(game.drop_interval, 0.55) &&
                    test::near(game.score_multiplier, 2.5),
                "zen levels up with the chosen pace");
  }

  void zen_multiplier_on_line_clears() {
    ZenSettings settings;
    settings.start_level = 3;  // x2
    ZenMode game(settings);
    for (int c = 0; c < 10; c++) {
      game.game_grid.grid[18][c] = 1;
      game.game_grid.grid[19][c] = 1;
    }
    game.block_attach();  // attaches the block at the top, clears 2 lines
    test::check(test::near(game.score, 600 * 2.0) && game.total_lines_cleared == 2,
                "zen multiplier applies to line clears");
  }

  void zen_save_and_load() {
    std::string path = test::temp_file("save.json");
    ZenSettings settings;
    settings.start_level = 4;
    settings.lines_per_level = 3;
    ZenMode game(settings);
    game.game_grid.grid[19][0] = 5;
    game.game_grid.grid[10][9] = 7;
    game.score = 1234.5;
    game.level = 6;
    game.total_lines_cleared = 7;
    game.lines_counter = 6;
    game.drop_interval = ZenMode::drop_interval_for(6);
    game.score_multiplier = ZenMode::multiplier_for(6);
    game.save_game_state(path);

    ZenMode loaded;
    loaded.load_game_state(path);
    std::remove(path.c_str());
    test::check(loaded.game_grid.grid[19][0] == 5 && loaded.game_grid.grid[10][9] == 7 &&
                    loaded.game_grid.grid[0][0] == 0,
                "zen save keeps the board");
    test::check(test::near(loaded.score, 1234.5) && loaded.level == 6 &&
                    loaded.settings.start_level == 4 && loaded.settings.lines_per_level == 3 &&
                    loaded.total_lines_cleared == 7 && loaded.lines_counter == 6 &&
                    test::near(loaded.drop_interval, 0.38) &&
                    test::near(loaded.score_multiplier, 3.5),
                "zen save keeps score, level and pace");
  }

  // Saves from the original game have no level data and random colour ids
  void zen_old_save_still_loads() {
    std::string path = test::temp_file("old_save.json");
    {
      std::ofstream f(path);
      f << "{\"game_grid\": [";
      for (int r = 0; r < 20; r++) {
        f << (r ? "," : "") << "[";
        for (int c = 0; c < 10; c++) f << (c ? "," : "") << (r == 19 && c == 0 ? 10 : 0);
        f << "]";
      }
      f << "], \"score\": 50, \"score_multiplier\": 1.5, \"drop_interval\": 0.4}";
    }
    ZenMode game;
    game.load_game_state(path);
    std::remove(path.c_str());
    test::check(game.game_grid.grid[19][0] == 10 && test::near(game.score, 50) &&
                    test::near(game.score_multiplier, 1.5) && test::near(game.drop_interval, 0.4) &&
                    game.level == 1 && game.settings.lines_per_level == 5,
                "zen loads saves from the original game");
  }

  void zen_missing_save() {
    ZenMode game;
    game.score = 7;
    game.load_game_state(test::temp_file("does_not_exist.json"));
    test::check(test::near(game.score, 7) && game.level == 1,
                "zen ignores a missing save file");
  }

  // The clock stops while paused and when the game ends
  void timer_pause_and_end() {
    TimeAttackMode game;
    game.game_start();
    test::sleep_ms(30);
    game.pause_timer();
    test::sleep_ms(200);
    double paused = game.elapsed_seconds();
    game.resume_timer();
    test::sleep_ms(30);
    double resumed = game.elapsed_seconds();
    game.game_end();
    double ended = game.elapsed_seconds();
    test::sleep_ms(60);
    test::check(paused < 0.15 && resumed > paused && resumed < 0.2,
                "timer doesn't count paused time");
    test::check(test::near(game.elapsed_seconds(), ended, 1e-6), "timer stops when the game ends");
  }

  void time_attack_scoring() {
    TimeAttackMode game;
    int expected[] = {0, 200, 600, 1200, 2400};
    bool ok = true;
    for (int lines = 0; lines <= 4; lines++) {
      game.lines_cleared = lines;
      if (!test::near(game.get_score(), expected[lines])) ok = false;
    }
    test::check(ok, "time attack scores 200/300/400/600 per line");
    game.game_start();
    test::check(!game.is_game_finished(), "time attack runs for 120 seconds");
  }

  void first_forty_finish() {
    FirstFortyMode game;
    game.game_start();
    game.total_lines_cleared = 39;
    bool not_yet = !game.is_game_finished();
    game.total_lines_cleared = 40;
    test::check(not_yet && game.is_game_finished(), "first 40 lines ends at 40 lines");
    test::sleep_ms(20);
    test::check(game.get_score() > 0.01, "first 40 lines scores by time");
  }
};

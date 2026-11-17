#pragma once
#include <vector>

#include "../Tetromino/Block_I.h"
#include "../Tetromino/Block_O.h"
#include "../Tetromino/Tetromino.h"
#include "../board.h"
#include "../time_attack_mode.h"
#include "../zen_mode.h"
#include "TestRunner.h"

// The unit tests of the original university project (UnitTestBoard,
// UnitTestTetromino and UnitTestModes), with the same checks. They used to
// print only when a check succeeded; they now go through test::check, so a
// failure is counted and fails the run.
class TestLegacy {
 public:
  void run() {
    board_bounds();
    board_row_clearance();
    board_clear_grid();
    board_is_row_full();
    tetromino_rotation_state();
    tetromino_position();
    tetromino_initial_position();
    zen_moves();
    zen_collision();
    zen_block_attach();
    time_attack_score();
  }

 private:
  // Note: despite its name, is_cell_within_bounds() returns true for a cell
  // that lies OUTSIDE the grid
  void board_bounds() {
    Board board;
    test::check(board.is_cell_within_bounds(5, 10), "bound checking_1");
    test::check(board.is_cell_within_bounds(30, 50), "bound checking_2");
  }

  void board_row_clearance() {
    // By default the board has 20 rows and 10 columns
    Board board;
    int full_rows = board.row_clearance();
    test::check(full_rows == 0 && board.get_num_rows() == 20, "row_clearance");
  }

  void board_clear_grid() {
    Board board;
    for (int c = 0; c < 10; c++) board.grid[19][c] = 1;
    board.clear_grid();
    int zero_count = 0;
    for (int r = 0; r < 20; r++) {
      for (int c = 0; c < 10; c++) {
        if (board.get_grid_value(r, c) == 0) zero_count++;
      }
    }
    test::check(zero_count == 200, "clear_grid()");
  }

  void board_is_row_full() {
    Board board;
    for (int c = 0; c < 10; c++) board.grid[19][c] = 1;
    test::check(board.is_row_full(19), "is_row_full()");
  }

  void tetromino_rotation_state() {
    Tetromino block;
    test::check(block.current_rotation == 0, "checking rotation state");
  }

  void tetromino_position() {
    Tetromino block;
    std::vector<Position> cells = block.get_current_position();
    test::check(cells.empty() || cells[0].row == 0 || cells[0].column == 0,
                "checking the position initialization");
  }

  // Blocks start at column 3; the O and I blocks have their own start
  void tetromino_initial_position() {
    BlockO block_o;
    block_o.set_initial_position();
    test::check(block_o.get_col_offset() != 3, "checking the starting position of O block");

    BlockI block_i;
    block_i.set_initial_position();
    test::check(block_i.get_row_offset() != 0 && block_i.get_col_offset() != 3,
                "checking the starting position of I block");
  }

  void zen_moves() {
    ZenMode game;
    int row = game.current_block.get_row_offset();
    int col = game.current_block.get_col_offset();
    game.move_left();
    test::check(game.current_block.get_row_offset() == row &&
                    game.current_block.get_col_offset() == col - 1,
                "move_left function");

    row = game.current_block.get_row_offset();
    col = game.current_block.get_col_offset();
    game.move_right();
    test::check(game.current_block.get_row_offset() == row &&
                    game.current_block.get_col_offset() == col + 1,
                "move_right function");

    row = game.current_block.get_row_offset();
    col = game.current_block.get_col_offset();
    game.move_down();
    test::check(game.current_block.get_col_offset() == col &&
                    game.current_block.get_row_offset() == row + 1,
                "move_down function");
  }

  void zen_collision() {
    ZenMode game;
    // Cells under the new block: a collision
    game.game_grid.grid[1][2] = 4;
    game.game_grid.grid[1][3] = 4;
    test::check(game.is_collision(), "is_collision function (1)");
    game.game_grid.grid[1][2] = 0;
    game.game_grid.grid[1][3] = 0;
    test::check(!game.is_collision(), "is_collision function (2)");
  }

  void zen_block_attach() {
    // Two full lines
    ZenMode two_lines;
    for (int c = 0; c < 10; c++) two_lines.game_grid.grid[19][c] = 1;
    for (int c = 0; c < 10; c++) two_lines.game_grid.grid[18][c] = 2;
    two_lines.block_attach();
    two_lines.lines_cleared = 2;
    test::check(two_lines.get_score() == 600, "score_block_attach() function (1)");
    test::check(two_lines.total_lines_cleared == 2, "lines_block_attach() function (1)");

    // No full lines
    ZenMode no_lines;
    no_lines.block_attach();
    test::check(no_lines.get_score() == 0, "score_block_attach() function (2)");
    test::check(no_lines.total_lines_cleared == 0, "lines_block_attach() function (2)");
  }

  void time_attack_score() {
    // One line cleared
    TimeAttackMode game;
    for (int c = 0; c < 10; c++) game.game_grid.grid[19][c] = 20;
    game.block_attach();
    game.lines_cleared = 1;
    test::check(game.get_score() == 200, "get_score() function (1)");

    // No lines to clear
    game.block_attach();
    test::check(game.get_score() == 0, "get_score() function (2)");

    // Ten lines at once
    TimeAttackMode ten_lines;
    for (int r = 10; r < 20; r++) {
      for (int c = 0; c < 10; c++) ten_lines.game_grid.grid[r][c] = 50;
    }
    ten_lines.block_attach();
    ten_lines.lines_cleared = 10;
    test::check(ten_lines.get_score() == 6000, "get_score() function (3)");
  }
};

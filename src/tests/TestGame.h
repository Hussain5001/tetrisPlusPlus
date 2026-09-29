#pragma once
#include <algorithm>
#include <set>

#include "../Tetromino/Block_I.h"
#include "../board.h"
#include "../zen_mode.h"
#include "TestRunner.h"

// Tests for the Game base class (via ZenMode, the simplest concrete mode)
// and the Board: movement, the piece bag, drops, ghost piece, gravity,
// lock delay, wall kicks and game over.
class TestGame {
 public:
  void run() {
    piece_bag();
    moves_and_walls();
    soft_drop_points();
    hard_drop_and_ghost();
    ghost_does_not_move_block();
    step_towards_column();
    gravity_tick();
    wall_kick();
    lock_delay();
    lock_delay_reset_limit();
    hard_drop_ignores_lock_delay();
    blocked_rotation_is_undone();
    game_over_stops_input();
    board_row_clearance_events();
  }

 private:
  static bool inside_board(const Tetromino& block) {
    Tetromino copy = block;
    for (Position p : copy.get_current_position()) {
      if (p.row < 0 || p.row > 19 || p.column < 0 || p.column > 9) return false;
    }
    return true;
  }

  // Every group of 7 blocks contains each of the 7 shapes exactly once
  // (the original game had O twice and no S)
  void piece_bag() {
    ZenMode game;
    std::set<int> first = {game.current_block.color_id};
    for (int i = 0; i < 6; i++) first.insert(game.random_block().color_id);
    std::set<int> second;
    for (int i = 0; i < 7; i++) second.insert(game.random_block().color_id);
    std::set<int> all = {1, 2, 3, 4, 5, 6, 7};
    test::check(first == all && second == all, "piece bag has all 7 blocks");

    int id = game.piece_id;
    game.random_block();
    test::check(game.piece_id == id + 1, "piece_id counts new blocks");
  }

  void moves_and_walls() {
    ZenMode game;
    int col = game.current_block.get_col_offset();
    game.apply(Action::Left);
    bool left = game.current_block.get_col_offset() == col - 1;
    game.apply(Action::Right);
    game.apply(Action::Right);
    bool right = game.current_block.get_col_offset() == col + 1;
    for (int i = 0; i < 12; i++) game.apply(Action::Left);
    bool wall_left = inside_board(game.current_block);
    for (int i = 0; i < 12; i++) game.apply(Action::Right);
    bool wall_right = inside_board(game.current_block);
    test::check(left && right, "apply left/right moves one column");
    test::check(wall_left && wall_right, "blocks can't leave the board");
  }

  void soft_drop_points() {
    ZenMode game;
    int rows = 0;
    int id = game.piece_id;
    while (game.piece_id == id && rows < 30) {
      int before = game.current_block.get_row_offset();
      game.apply(Action::SoftDrop);
      if (game.piece_id == id && game.current_block.get_row_offset() == before + 1) rows++;
    }
    test::check(rows > 10 && test::near(game.score, rows),
                "soft drop gives 1 point per row");
  }

  void hard_drop_and_ghost() {
    ZenMode game;
    int color = game.current_block.color_id;
    std::vector<Position> landing = game.ghost_position();
    int id = game.piece_id;
    game.apply(Action::HardDrop);
    bool placed = true;
    for (Position p : landing) {
      if (game.game_grid.grid[p.row][p.column] != color) placed = false;
    }
    int max_row = 0;
    for (Position p : landing) max_row = std::max(max_row, p.row);
    test::check(placed && max_row == 19, "hard drop lands where the ghost shows");
    test::check(game.piece_id == id + 1 && test::near(game.score, 20),
                "hard drop locks at once and scores 20");
  }

  void ghost_does_not_move_block() {
    ZenMode game;
    int row = game.current_block.get_row_offset();
    int col = game.current_block.get_col_offset();
    game.ghost_position();
    test::check(game.current_block.get_row_offset() == row &&
                    game.current_block.get_col_offset() == col,
                "ghost position leaves the block where it is");
  }

  void step_towards_column() {
    ZenMode game;
    game.current_block = BlockI();
    int moves = 0;
    while (game.step_towards_column(0) && moves < 20) moves++;
    int min_col = 99;
    for (Position p : game.current_block.get_current_position()) min_col = std::min(min_col, p.column);
    bool left = min_col == 0 && moves > 0;
    while (game.step_towards_column(9) && moves < 40) moves++;
    int max_col = 0;
    for (Position p : game.current_block.get_current_position()) max_col = std::max(max_col, p.column);
    test::check(left && max_col == 9, "position mode steps the block to a column");
  }

  void gravity_tick() {
    ZenMode game;
    game.drop_interval = 0.5;
    game.fall_start = 0;
    int row = game.current_block.get_row_offset();
    bool early = !game.tick(0.2) && game.current_block.get_row_offset() == row;
    bool fell = game.tick(0.6) && game.current_block.get_row_offset() == row + 1;
    bool waits_again = !game.tick(0.9);
    test::check(early && fell && waits_again, "gravity moves one row per interval");

    // Without a lock delay a landed block locks on the next gravity step
    ZenMode instant;
    instant.drop_interval = 0.01;
    int id = instant.piece_id;
    double t = 0;
    while (instant.piece_id == id && t < 2) instant.tick(t += 0.02);
    test::check(instant.piece_id == id + 1, "no lock delay locks straight away");
  }

  // Rotating an I block next to the wall kicks it back inside the board
  void wall_kick() {
    ZenMode game;
    game.current_block = BlockI();
    game.current_block.move(5, 0);
    game.rotate_and_bound_chk();  // vertical
    for (int i = 0; i < 10; i++) game.move_right();
    bool rotated = game.rotate_and_bound_chk();  // back to horizontal
    test::check(rotated && inside_board(game.current_block), "wall kick");
  }

  // A landed block waits lock_delay seconds before locking, and moving it
  // restarts the wait
  void lock_delay() {
    ZenMode game;
    game.lock_delay = 0.5;
    game.drop_interval = 0.01;
    double t = 0;
    int id = game.piece_id;
    int row = -100;
    while (game.current_block.get_row_offset() != row && t < 5) {
      row = game.current_block.get_row_offset();
      game.tick(t += 0.02);
    }
    bool landed = game.piece_id == id;
    game.tick(t + 0.3);
    bool waiting = game.piece_id == id;
    game.move_left();  // restarts the lock delay at t + 0.3
    game.tick(t + 0.7);
    bool reset = game.piece_id == id;
    game.tick(t + 0.85);
    bool locked = game.piece_id != id;
    test::check(landed && waiting && reset && locked, "lock delay");
  }

  // Moving a landed block restarts the lock delay, but only 15 times
  void lock_delay_reset_limit() {
    ZenMode game;
    game.lock_delay = 0.5;
    game.drop_interval = 0.01;
    int id = game.piece_id;
    double t = 0;
    int row = -100;
    while (game.current_block.get_row_offset() != row && t < 5) {
      row = game.current_block.get_row_offset();
      game.tick(t += 0.02);
    }
    int locked_at = -1;
    for (int i = 1; i <= 30 && locked_at < 0; i++) {
      t += 0.1;
      game.apply(i % 2 ? Action::Left : Action::Right);
      game.tick(t);
      if (game.piece_id != id) locked_at = i;
    }
    test::check(locked_at > 15 && locked_at <= 22, "lock delay resets at most 15 times");
  }

  void hard_drop_ignores_lock_delay() {
    ZenMode game;
    game.lock_delay = 5;
    int id = game.piece_id;
    game.apply(Action::HardDrop);
    test::check(game.piece_id == id + 1, "hard drop ignores the lock delay");
  }

  // If a rotation can't fit anywhere, the block stays exactly as it was
  void blocked_rotation_is_undone() {
    ZenMode game;
    game.current_block = BlockI();
    game.current_block.move(8, 0);  // horizontal on rows 9, columns 3-6
    std::vector<Position> cells = game.current_block.get_current_position();
    for (int r = 0; r < 20; r++) {
      for (int c = 0; c < 10; c++) game.game_grid.grid[r][c] = 1;
    }
    for (Position p : cells) game.game_grid.grid[p.row][p.column] = 0;
    int rotation = game.current_block.current_rotation;
    bool rotated = game.rotate_and_bound_chk();
    std::vector<Position> after = game.current_block.get_current_position();
    bool same = true;
    for (size_t i = 0; i < cells.size(); i++) {
      if (cells[i].row != after[i].row || cells[i].column != after[i].column) same = false;
    }
    test::check(!rotated && same && game.current_block.current_rotation == rotation,
                "blocked rotation is undone");
  }

  void game_over_stops_input() {
    ZenMode game;
    // Fill the board up to row 1, leaving column 0 open so no line clears
    for (int r = 1; r < 20; r++) {
      for (int c = 1; c < 10; c++) game.game_grid.grid[r][c] = 2;
    }
    game.apply(Action::HardDrop);  // lands at the top; the next block can't spawn
    int col = game.current_block.get_col_offset();
    double score = game.score;
    game.apply(Action::Left);
    game.apply(Action::HardDrop);
    test::check(game.game_over, "game over when a new block can't spawn");
    test::check(game.current_block.get_col_offset() == col && test::near(game.score, score),
                "no moves or points after game over");
  }

  void board_row_clearance_events() {
    Board board;
    for (int c = 0; c < 10; c++) {
      board.grid[5][c] = 1;
      board.grid[19][c] = 1;
    }
    board.grid[4][2] = 3;   // should fall two rows
    board.grid[18][7] = 4;  // should fall one row
    int cleared = board.row_clearance();
    bool rows = board.last_cleared_rows.size() == 2 && board.last_cleared_rows[0] == 19 &&
                board.last_cleared_rows[1] == 5;
    bool shifted = board.grid[6][2] == 3 && board.grid[19][7] == 4 && board.grid[4][2] == 0;
    test::check(cleared == 2 && rows && board.clear_events == 1 && shifted,
                "clearing split rows shifts the rest down");
    int again = board.row_clearance();
    test::check(again == 0 && board.clear_events == 1 && board.last_cleared_rows.empty(),
                "no clear event when nothing is full");
  }
};

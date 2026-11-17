#include "game.h"

#include <time.h>

#include <algorithm>

// Constructor for the Game class
Game::Game() {
  lines_cleared=0;
  fall_start = 0;
  drop_interval=0.48;
  score=0;
  // Initialize the block list
  block_list = get_blocks();
  // Set the current block to a random Tetromino
  current_block = random_block();
  // Set the game over state to false
  game_over=false;
}


// Function to generate a random Tetromino block
Tetromino Game::random_block() {
    if (block_list.empty()){
        block_list=get_blocks();
    }
  // Randomly select a block from the block list
  int block_index = rand() % block_list.size();
  Tetromino blk =block_list[block_index];
  block_list.erase(block_list.begin()+block_index);

  // A new block starts with a fresh lock delay
  piece_id++;
  grounded = false;
  lock_resets = 0;

  // Each block type keeps its own colour (set in the Block_* constructors)
  return (blk);
}

// Function to get the list of Tetromino blocks
std::vector<Tetromino> Game::get_blocks(){
    return {BlockI(), BlockJ(), BlockL(), BlockO(),
                BlockS(), BlockT(), BlockZ()};
}

// Function to attach the current block to the grid and bring in the next one
void Game::block_attach() {
  for (Position cell : current_block.get_current_position()) {
    game_grid.grid[cell.row][cell.column] = current_block.color_id;
  }
  // Clear the full rows before the next block appears: a block that
  // completes a line near the top makes room for it
  on_lines_cleared(game_grid.row_clearance());
  current_block = random_block();
  if (is_collision()) {
    game_over = true;
    on_top_out();
  }
}

// Function to apply a single player action to the game
void Game::apply(Action action) {
  switch (action) {
    case Action::Left: move_left(); break;
    case Action::Right: move_right(); break;
    case Action::SoftDrop: {
      int id = piece_id;
      int row = current_block.get_row_offset();
      move_down();
      if (piece_id == id && current_block.get_row_offset() > row) {
        score += soft_drop_points;
      }
      break;
    }
    case Action::Rotate: rotate_and_bound_chk(); break;
    case Action::HardDrop: hard_drop(); break;
    default: break;  // Pause/Confirm/Back are handled by the App
  }
}

// Function to drop the current block to the bottom and attach it
void Game::hard_drop() {
  if(!game_over){
    score+=20;
    while(true){
      current_block.move(1, 0);
      if (!is_within_grid()|| is_collision()) {
        current_block.move(-1, 0);
        block_attach();
        break;
      }
    }
  }
}

// Function to move the block one column towards the target column. The
// block's centre column is compared with the target.
bool Game::step_towards_column(int column) {
  if (game_over) return false;
  std::vector<Position> cells = current_block.get_current_position();
  int min_col = cells[0].column, max_col = cells[0].column;
  for (Position cell : cells) {
    min_col = std::min(min_col, cell.column);
    max_col = std::max(max_col, cell.column);
  }
  int centre = (min_col + max_col) / 2;
  int before = current_block.get_col_offset();
  if (centre < column) {
    move_right();
  } else if (centre > column) {
    move_left();
  }
  return current_block.get_col_offset() != before;
}

// Function to move the block to the target column in one go
void Game::move_to_column(int column) {
  for (int steps = 0; steps < game_grid.get_num_cols(); steps++) {
    if (!step_towards_column(column)) break;
  }
}

// Function to find the cells the current block would occupy if dropped
std::vector<Position> Game::ghost_position() {
  Tetromino original = current_block;
  while (true) {
    current_block.move(1, 0);
    if (!is_within_grid() || is_collision()) {
      current_block.move(-1, 0);
      break;
    }
  }
  std::vector<Position> landing = current_block.get_current_position();
  current_block = original;
  return landing;
}
// Function to move the current block to the left
void Game::move_left() {
  if(!game_over){
    current_block.move(0, -1);
    if (!is_within_grid()||is_collision()) {
      current_block.move(0, 1);
    } else {
      on_player_move();
    }
  }
}
// Function to move the current block to the right
void Game::move_right() {
  if(!game_over){
    current_block.move(0, 1);
    if (!is_within_grid()||is_collision()) {
      current_block.move(0, -1);
    } else {
      on_player_move();
    }
  }
}
// Function to move the current block down
void Game::move_down() {
  if(!game_over){
    current_block.move(1, 0);
    if (!is_within_grid()|| is_collision()) {
      current_block.move(-1, 0);
      block_attach();
    }
  }
}
// Function to check if the block is within the grid
bool Game::is_within_grid() {
  std::vector<Position> block_structure = current_block.get_current_position();
  for (Position cell : block_structure) {
    if (game_grid.is_cell_within_bounds(cell.row, cell.column)) {
      return false;
    }
  }
  return true;
}
// Function to rotate the current block and check for collisions
bool Game::rotate_and_bound_chk() {
  if(game_over){
    return false;
  }
  current_block.rotate();
  if (is_within_grid() && !is_collision()) {
    on_player_move();
    return true;
  }
  // Wall kicks: try nudging the rotated block left/right, then up
  const int kicks[][2] = {{0, -1}, {0, 1}, {0, -2}, {0, 2}, {-1, 0}};
  for (const auto& kick : kicks) {
    current_block.move(kick[0], kick[1]);
    if (is_within_grid() && !is_collision()) {
      on_player_move();
      return true;
    }
    current_block.move(-kick[0], -kick[1]);
  }
  // Nothing fits: undo the rotation
  current_block.current_rotation -= 1;
  if (current_block.current_rotation == -1) {
    current_block.current_rotation = (int)current_block.cells.size() - 1;
  }
  return false;
}
// Function to make the current block fall
void Game::fall_block() {
    tick(GetTime());
}

// Function to check whether the current block could fall one row
bool Game::can_fall() {
  current_block.move(1, 0);
  bool fits = is_within_grid() && !is_collision();
  current_block.move(-1, 0);
  return fits;
}

// Function to restart the lock delay when a landed block is moved, so the
// player can still slide it into place (limited to kMaxLockResets)
void Game::on_player_move() {
  if (grounded && lock_resets < kMaxLockResets) {
    lock_resets++;
    lock_start = last_tick;
  }
}

// Function to apply gravity and the lock delay
bool Game::tick(double now) {
  last_tick = now;
  if (game_over) {
    return false;
  }
  if (grounded) {
    if (can_fall()) {
      grounded = false;  // slid off a ledge, keep falling
    } else if (now - lock_start >= lock_delay) {
      grounded = false;
      block_attach();
      fall_start = now;
      return false;
    }
  }
  if (now - fall_start < drop_interval) {
    return false;
  }
  fall_start = now;
  if (can_fall()) {
    current_block.move(1, 0);
    return true;
  }
  if (lock_delay <= 0) {
    block_attach();
  } else if (!grounded) {
    grounded = true;
    lock_start = now;
  }
  return false;
}

// Function to check for collisions with other blocks
bool Game::is_collision(){
    std::vector<Position> block_structure=current_block.get_current_position();
    for(Position cell:block_structure){

        if(!game_grid.is_cell_empty(cell.row,cell.column)){
            return true;
        }
    }
    return false;
}
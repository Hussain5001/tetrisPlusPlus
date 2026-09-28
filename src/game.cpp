#include "game.h"

#include <time.h>

#include <iostream>
#include <random>

// Constructor for the Game class
Game::Game() {
  lines_cleared=0;
  fall_start = 0;
  drop_interval=0.48;
  score=0;
  // Display the initial state of the game grid
  game_grid.show_state();
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

  // Each block type keeps its own colour (set in the Block_* constructors)
  return (blk);
}

// Function to get the list of Tetromino blocks
std::vector<Tetromino> Game::get_blocks(){
    return {BlockI(), BlockJ(), BlockL(), BlockO(),
                BlockS(), BlockT(), BlockZ()};
}
// Function to display the game grid and the current block
void Game::display() {
  display(0, 0);
}

// Function to display the grid, the ghost piece and the current block at (x, y)
void Game::display(int x, int y) {
  game_grid.draw(x, y);
  if (!game_over) {
    Tetromino ghost = current_block;
    std::vector<Position> landing = ghost_position();
    ghost.move(landing[0].row - current_block.get_current_position()[0].row, 0);
    ghost.draw_ghost(x, y);
  }
  current_block.draw(x, y);
}
// Function to handle user input
void Game::handle_input() {
  switch (GetKeyPressed()) {
    case KEY_LEFT: apply(Action::Left); break;
    case KEY_RIGHT: apply(Action::Right); break;
    case KEY_DOWN: apply(Action::SoftDrop); break;
    case KEY_UP: apply(Action::Rotate); break;
    case KEY_SPACE: apply(Action::HardDrop); break;
  }
}

// Function to apply a single player action to the game
void Game::apply(Action action) {
  switch (action) {
    case Action::Left: move_left(); break;
    case Action::Right: move_right(); break;
    case Action::SoftDrop: move_down(); break;
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
    }
  }
}
// Function to move the current block to the right
void Game::move_right() {
  if(!game_over){
    current_block.move(0, 1);
  if (!is_within_grid()||is_collision()) {
    current_block.move(0, -1);
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
void Game::rotate_and_bound_chk() {
  if(!game_over){
    current_block.rotate();
    if (!is_within_grid()||is_collision()) {
      current_block.current_rotation -= 1;
      if (current_block.current_rotation == -1) {
        current_block.current_rotation = (int)current_block.cells.size() - 1;
      }
    }
  }
}
// Function to make the current block fall
void Game::fall_block() {
    double current_t=GetTime();
    if(current_t-fall_start>=drop_interval){
        move_down();
        fall_start=current_t;
    }
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
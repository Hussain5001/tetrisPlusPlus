#pragma once
#include "board.h"
#include "Tetromino/Tetromino.h"
#include "Tetromino/Block_I.h"
#include "Tetromino/Block_J.h"
#include "Tetromino/Block_L.h"
#include "Tetromino/Block_O.h"
#include "Tetromino/Block_S.h"
#include "Tetromino/Block_T.h"
#include "Tetromino/Block_Z.h"
#include "Position.h"
#include "Color.h"
#include "input/Action.h"
#include <vector>

// Game class declaration
class Game{
public:
    // Constructor for the Game class
    Game();
    virtual ~Game() = default;
    
    // Board for the game grid
    Board game_grid;

    // Function to generate a random Tetromino block
    Tetromino random_block();

    // Function to get the list of Tetromino blocks
    std::vector<Tetromino> get_blocks();

    // Function to display the game
    void display();

    // Function to display the game with the board's top-left corner at (x, y)
    void display(int x, int y);

    // Function to handle keyboard input directly (kept for compatibility)
    void handle_input();

    // Function to apply one player action (keyboard, mouse or gesture)
    void apply(Action action);

    // Function to drop the current block straight to the bottom
    void hard_drop();

    // Function to step the current block one column towards `column`
    // (used by the gesture position mode). Returns false when it can't move.
    bool step_towards_column(int column);

    // Function to get where the current block would land (ghost piece)
    std::vector<Position> ghost_position();

    // Function to move the current block to the left
    void move_left();

    // Function to move the current block to the right
    void move_right();

    // Function to move the current block down
    void move_down();

    // Function to rotate the current block and check boundaries
    void rotate_and_bound_chk();

    // Function to make the current block fall
    virtual void fall_block();

    // Function to check for collisions
    bool is_collision();

    // Variables for game timing and scoring
    double fall_start;
    double drop_interval;
    double score;
    bool game_over;
    int lines_cleared;

    // Function to attach the block to the game grid
    virtual void block_attach() = 0;

    // Function to check if the game is finished
    virtual bool is_game_finished() = 0;

    // Function to get the current score
    virtual double get_score() = 0;

    // Current Tetromino block
    Tetromino current_block;

   protected:
    // List of Tetromino blocks
    std::vector<Tetromino> block_list;

    // Function to check if the block is within the game grid
    bool is_within_grid();
};

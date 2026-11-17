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

    // Function to bring in the next block: returns next_block and takes a
    // new one out of the bag (every 7 blocks hold each shape once)
    Tetromino random_block();

    // The block that comes after the current one (shown as "next")
    Tetromino next_block;

    // Function to get the list of Tetromino blocks
    std::vector<Tetromino> get_blocks();

    // Function to apply one player action (keyboard, mouse or gesture)
    void apply(Action action);

    // Function to drop the current block straight to the bottom
    void hard_drop();

    // Function to step the current block one column towards `column`
    // (used by the gesture position mode). Returns false when it can't move.
    bool step_towards_column(int column);

    // Function to move the current block all the way to `column` (as far as
    // walls and other blocks allow), so a drop lands under the player's hand
    void move_to_column(int column);

    // Function to get where the current block would land (ghost piece)
    std::vector<Position> ghost_position();

    // Function to move the current block to the left
    void move_left();

    // Function to move the current block to the right
    void move_right();

    // Function to move the current block down
    void move_down();

    // Function to rotate the current block and check boundaries. If the
    // rotated block doesn't fit, it is nudged sideways/up (a "wall kick").
    // Returns true if the block rotated.
    bool rotate_and_bound_chk();

    // Function to make the current block fall
    virtual void fall_block();

    // Function to apply gravity and the lock delay at time `now` (seconds).
    // Returns true if the block moved down one row.
    bool tick(double now);

    // Function to check for collisions
    bool is_collision();

    // Variables for game timing and scoring
    double fall_start;
    double drop_interval;
    double score;
    bool game_over;
    int lines_cleared;

    // Seconds a landed block can still be moved before it locks. 0 locks
    // immediately (the original behaviour); the App uses 0.5.
    double lock_delay = 0;

    // Increases every time a new block spawns
    int piece_id = 0;

    // Points for each row moved with soft drop (the down key)
    double soft_drop_points = 1;

    // Function to attach the block to the game grid: fills its cells, clears
    // the full rows (reported to on_lines_cleared() for scoring), then spawns
    // the next block. The game is over if that block has no room.
    void block_attach();

    // Function to check if the game is finished
    virtual bool is_game_finished() = 0;

    // Function to get the current score
    virtual double get_score() = 0;

    // Current Tetromino block
    Tetromino current_block;

   protected:
    // List of Tetromino blocks
    std::vector<Tetromino> block_list;

    // Function to take a random block out of the bag
    Tetromino take_from_bag();

    // Function to check if the block is within the game grid
    bool is_within_grid();

    // Function called by block_attach() with the number of rows it just
    // cleared (0-4), so each mode can keep its own score
    virtual void on_lines_cleared(int lines) = 0;

    // Function called by block_attach() when the next block has no room
    virtual void on_top_out() {}

    // Lock delay state
    bool grounded = false;
    double lock_start = 0;
    double last_tick = 0;
    int lock_resets = 0;
    static const int kMaxLockResets = 15;

    // Function to restart the lock delay after the player moves a landed block
    void on_player_move();

    // Function to check whether the current block could fall one row
    bool can_fall();
};

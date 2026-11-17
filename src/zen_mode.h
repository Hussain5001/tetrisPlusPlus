#pragma once

#include <string>

#include "game.h" // including the header file for the base Game class

// Choices made on the Zen setup screen
struct ZenSettings {
    int start_level = 1;       // 1-10, sets the starting speed
    int lines_per_level = 5;   // how many lines until the next level (pace)
};

// ZenMode class inheriting from Game class
class ZenMode: public Game{
public:
    // Constructor
    ZenMode();
    explicit ZenMode(ZenSettings settings);

    // Current level (speeds up every settings.lines_per_level lines)
    int level;
    ZenSettings settings;

    // Seconds between rows falling at a given level (1 = slowest)
    static double drop_interval_for(int level);

    // Score multiplier at a given level
    static double multiplier_for(int level);

    // Member variables
    int total_lines_cleared; // variable to keep track of the total lines cleared
    double score_multiplier; // variable to manage the score multiplier
    int lines_counter; // variable to count the lines

    // Function to check if the game is finished
    bool is_game_finished();

    // Function to calculate the score based on lines cleared (lines_cleared
    // rows in one move, before the multiplier)
    double get_score();

    // Function to manage the falling of blocks
    void fall_block();

    // Function to go up a level for every settings.lines_per_level lines
    void update_level();

    // Function to save the game state to a JSON file
    void save_game_state(const std::string& path = "game_state.json");

    // Function to load the game state from a JSON file. Returns false if the
    // file is missing or unreadable.
    bool load_game_state(const std::string& path = "game_state.json");

protected:
    // Function to score the rows cleared by the block that just landed
    void on_lines_cleared(int lines) override;
};

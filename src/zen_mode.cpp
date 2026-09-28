// Including necessary headers
#include "zen_mode.h"
#include <iostream>
#include <fstream>
#include "../vendor/json/single_include/nlohmann/json.hpp"

using json = nlohmann::json;
// Constructor for ZenMode class, inheriting from Game class
ZenMode::ZenMode():ZenMode(ZenSettings()) {}

ZenMode::ZenMode(ZenSettings chosen):Game(), settings(chosen) {
    total_lines_cleared=0;
    lines_counter=0;
    lines_cleared=0;
    if (settings.start_level < 1) settings.start_level = 1;
    if (settings.lines_per_level < 1) settings.lines_per_level = 1;
    level=settings.start_level;
    drop_interval=drop_interval_for(level);
    score_multiplier=multiplier_for(level);
}

// Seconds between rows falling: levels 1-10 follow a table, after that the
// game keeps speeding up slowly until 0.05 seconds
double ZenMode::drop_interval_for(int level) {
    static const double table[] = {0.80, 0.72, 0.63, 0.55, 0.48,
                                   0.38, 0.30, 0.22, 0.15, 0.10};
    if (level < 1) level = 1;
    if (level <= 10) return table[level - 1];
    double interval = 0.10 - 0.005 * (level - 10);
    return interval < 0.05 ? 0.05 : interval;
}

// Higher levels are worth more: x1 at level 1, +0.5 per level
double ZenMode::multiplier_for(int level) {
    return 1 + 0.5 * (level - 1);
}

// Function to check if the game is finished
bool ZenMode::is_game_finished() {
    if (is_collision()) {
        std::cout<<"Game Over"<<std::endl;
        return true;
    } else {
        return false;
    }
}
// Function to attach the block to the grid
void ZenMode::block_attach() {
    std::vector<Position> block_structure = current_block.get_current_position();
    for (Position cell : block_structure) {
        game_grid.grid[cell.row][cell.column] = current_block.color_id;
    }
    current_block = random_block();
    game_over = is_game_finished();
    lines_cleared = game_grid.row_clearance();
    total_lines_cleared += lines_cleared;
    score += get_score() * score_multiplier;
    lines_cleared = 0;
    std::cout << "score: " << score << std::endl;
}
// Function to calculate score based on the number of lines cleared
double ZenMode::get_score() {
    if (lines_cleared >= 4) {
        return 500 * lines_cleared;
    } else {
        return 300 * lines_cleared;
    }
}
// Function to make the block fall
void ZenMode::fall_block() {
    update_level();
    // Every row the block falls on its own is worth the multiplier
    if (tick(GetTime()) && !game_over) {
        score += score_multiplier;
    }
}

// Function to go up a level for every settings.lines_per_level lines
void ZenMode::update_level() {
    while (total_lines_cleared - lines_counter >= settings.lines_per_level) {
        lines_counter += settings.lines_per_level;
        level++;
        drop_interval = drop_interval_for(level);
        score_multiplier = multiplier_for(level);
        std::cout << "level " << level << std::endl;
    }
}
// Function to save the current game state to a JSON file
void ZenMode::save_game_state() {
    try {
        json game_state;
        json grid_array;

        
        for (int i = 0; i < 20; i++) {
            json row;
            for (int j = 0; j < 10; j++) {
                row.push_back(game_grid.grid[i][j]);
            }
            grid_array.push_back(row);
        }

        game_state["game_grid"] = grid_array;
        game_state["score"] = score;
        game_state["score_multiplier"]=score_multiplier;
        game_state["drop_interval"]=drop_interval;
        game_state["level"]=level;
        game_state["start_level"]=settings.start_level;
        game_state["lines_per_level"]=settings.lines_per_level;
        game_state["total_lines_cleared"]=total_lines_cleared;
        game_state["lines_counter"]=lines_counter;

        std::ofstream file("game_state.json");
        if (!file.is_open()) {
            throw std::runtime_error("Failed to open file: game_state.json");
        }
        file << game_state.dump(4);
    } catch (const std::exception &e) {
        std::cerr << "An error occurred: " << e.what() << std::endl;
    }
}
 // Loading other game state data from the JSON file
void ZenMode::load_game_state() {
    try {
        std::ifstream file("game_state.json");
        if (!file.is_open()) {
            throw std::runtime_error("Failed to open file: game_state.json");
        }

        json game_state;
        file >> game_state;

        std::cout << "Reading from file: " << game_state << std::endl;

        if (game_state.contains("game_grid") && game_state["game_grid"].is_array()) {
            json grid_array = game_state["game_grid"];
            for (int i = 0; i < 20; i++) {
                if (grid_array[i].is_array()) {
                    for (int j = 0; j < 10; j++) {
                        if (grid_array[i][j].is_number()) {
                            game_grid.grid[i][j]=grid_array[i][j];
                        }
                    }
                    std::cout << std::endl;
                }
            }
        }

        // Loading other game state data from the JSON file
        if (game_state.contains("score") && game_state["score"].is_number()) {
            score = game_state["score"];
        }

        if (game_state.contains("score_multiplier") && game_state["score_multiplier"].is_number_float()) {
            score_multiplier = game_state["score_multiplier"];
        }

        if (game_state.contains("drop_interval") && game_state["drop_interval"].is_number_float()) {
            drop_interval = game_state["drop_interval"];
        }

        // Level data (saves from the original game don't have it)
        auto read_int = [&](const char* key, int& target) {
            if (game_state.contains(key) && game_state[key].is_number_integer()) {
                target = game_state[key];
            }
        };
        read_int("level", level);
        read_int("start_level", settings.start_level);
        read_int("lines_per_level", settings.lines_per_level);
        read_int("total_lines_cleared", total_lines_cleared);
        read_int("lines_counter", lines_counter);
        if (settings.lines_per_level < 1) settings.lines_per_level = 5;

    } catch (const std::exception &e) {
        std::cerr << "An error occurred: " << e.what() << std::endl;
    }
}




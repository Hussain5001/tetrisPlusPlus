#include "first_forty_mode.h"

FirstFortyMode::FirstFortyMode():TimeDependentMode(){}

//Function to check if the game is finished! (Total lines cleared are above 40)
bool FirstFortyMode::is_game_finished() {
    return total_lines_cleared>=40;
}

//Function to return the score in elapsed seconds
double FirstFortyMode::get_score() {
    return elapsed_seconds();
}

//Function to count the rows cleared by the block that just landed
void FirstFortyMode::on_lines_cleared(int lines) {
  total_lines_cleared += lines;
}

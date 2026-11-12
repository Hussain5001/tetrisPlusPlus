#include "time_attack_mode.h"

// Constructor for TimeAttackMode, inheriting from TimeDependentMode
TimeAttackMode::TimeAttackMode():TimeDependentMode() {}
// Function to check if the game is finished based on elapsed time
bool TimeAttackMode::is_game_finished() {
  if (elapsed_seconds() >= 120) {
    return true;
  } else {
    return false;
  }
}
// Function to score the rows cleared by the block that just landed
void TimeAttackMode::on_lines_cleared(int lines){
    lines_cleared=lines;
    score+=get_score();
    lines_cleared=0;
}
// Function to calculate the score based on the number of lines cleared
double TimeAttackMode::get_score() { 
    switch (lines_cleared)
    {
    case 1:
        return 200*lines_cleared;
    case 2:
        return 300*lines_cleared;
    case 3:
        return 400*lines_cleared;
    default:
        return 600*lines_cleared;
    }
}

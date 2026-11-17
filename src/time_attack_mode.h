#pragma once

#include "time_dependent_mode.h" // including the header file for the base TimeDependentMode class

// TimeAttackMode class inheriting from TimeDependentMode
class TimeAttackMode: public TimeDependentMode{
public:
    // Constructor
    TimeAttackMode();

    // Function to check if the game is finished based on time
    bool is_game_finished();

    // Function to calculate the score based on the lines cleared
    double get_score();

protected:
    // Function to score the rows cleared by the block that just landed
    void on_lines_cleared(int lines) override;
};

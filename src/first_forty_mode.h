#pragma once
#include "time_dependent_mode.h"

class FirstFortyMode: public TimeDependentMode{
    public:
        FirstFortyMode();
        int total_lines_cleared=0;
        bool is_game_finished();
        double get_score();

    protected:
        // Function to count the rows cleared by the block that just landed
        void on_lines_cleared(int lines) override;
};

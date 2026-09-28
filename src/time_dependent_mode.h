#pragma once
#include "game.h"
#include <chrono>
#include <ctime>
class TimeDependentMode: public Game{
    public:
        TimeDependentMode();
        void game_start();
        void game_end();
        double elapsed_seconds();
        // Stop / restart the clock while the game is paused
        void pause_timer();
        void resume_timer();
        bool timer_on=false;
    protected:
        std::chrono::time_point<std::chrono::system_clock> game_start_time;
        std::chrono::time_point<std::chrono::system_clock> game_end_time;
        std::chrono::time_point<std::chrono::system_clock> pause_start_time;
        std::chrono::duration<double> paused_duration{0};
        bool paused=false;

};


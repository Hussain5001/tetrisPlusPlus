#pragma once
#include "game.h"
#include <chrono>
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
        // A steady clock never jumps, unlike the wall clock (time sync,
        // daylight saving), so the game timer can't skip or run backwards
        using Clock = std::chrono::steady_clock;
        Clock::time_point game_start_time;
        Clock::time_point game_end_time;
        Clock::time_point pause_start_time;
        std::chrono::duration<double> paused_duration{0};
        bool paused=false;

        // The clock stops when the board fills up
        void on_top_out() override;
};

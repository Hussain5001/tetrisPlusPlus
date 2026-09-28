#include "time_dependent_mode.h"


TimeDependentMode::TimeDependentMode():Game() {}

//Storing the time for the start of the game
void TimeDependentMode::game_start() {
  game_start_time = std::chrono::system_clock::now();
  paused_duration = std::chrono::duration<double>(0);
  paused = false;
  timer_on = true;
}

//Storing the ending time
void TimeDependentMode::game_end(){
    resume_timer();
    game_end_time=std::chrono::system_clock::now();
    timer_on=false;
}

//Keeping the track of elapsed seconds
double TimeDependentMode::elapsed_seconds(){
    std::chrono::time_point<std::chrono::system_clock> end_t;
    if(timer_on){
        end_t =std::chrono::system_clock::now(); //Current Time
    }else{
        end_t=game_end_time;
    }
    if(paused){
        end_t=pause_start_time;
    }
    std::chrono::duration<double> elapsed_time = end_t-game_start_time-paused_duration;
    return elapsed_time.count();
}

//Freezing the clock while the game is paused
void TimeDependentMode::pause_timer(){
    if(timer_on && !paused){
        paused=true;
        pause_start_time=std::chrono::system_clock::now();
    }
}

//Restarting the clock, not counting the time spent paused
void TimeDependentMode::resume_timer(){
    if(paused){
        paused_duration+=std::chrono::system_clock::now()-pause_start_time;
        paused=false;
    }
}

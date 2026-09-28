//Class for manipulating the tetrominos

#pragma once
#include "../Position.h"
#include <map>
#include <string>
#include <vector>
#include "../Color.h"

class Tetromino{

    private:
    int cell_size;
    int row_pos;
    int col_pos;

    public:
    Tetromino();
    int color_id;
    int current_rotation;
    std::map<int, std::vector<Position>> cells;
    std::vector<Position> get_current_position();
    void draw(int x = 0, int y = 0);
    void draw_ghost(int x = 0, int y = 0);
    void move(int row, int col);
    void rotate();
    virtual void set_initial_position();
    int get_row_offset();
    int get_col_offset();

};





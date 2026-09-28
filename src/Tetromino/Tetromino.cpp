#include"Tetromino.h"
#include<raylib.h>
#include <iostream>
#include "../ui/Draw.h"

//Default constructor for initializing all the attributes
Tetromino::Tetromino(){
    cell_size=30;
    current_rotation=0;
    color_id=0;
    row_pos=0;
    col_pos=0;
};

std::vector<Position> Tetromino::get_current_position() {
    std::vector<Position> block_structure = cells[current_rotation];
    std::vector<Position> block_with_offset;
    for(Position cell: block_structure){
        Position moved{cell.row + row_pos, cell.column + col_pos};
        block_with_offset.push_back(moved);
    }

    return block_with_offset;
}

void Tetromino::draw(int x, int y) {
  // getting the position of block in grid
  for (Position cell : get_current_position()) {
    if (cell.row < 0) continue;  // not visible yet
    ui::draw_cell(x + cell.column * cell_size, y + cell.row * cell_size,
                  cell_size, cell_color(color_id));
  }
}

// Draws the block as an outline, used to show where it will land
void Tetromino::draw_ghost(int x, int y) {
  for (Position cell : get_current_position()) {
    if (cell.row < 0) continue;
    ui::draw_ghost_cell(x + cell.column * cell_size, y + cell.row * cell_size,
                        cell_size, cell_color(color_id));
  }
}

void Tetromino::move(int row, int col){

    //Updating the row offset and the column offset
    row_pos=row_pos+row;
    col_pos=col_pos+col;
};

//Virtual Function for defining the initial position of a Tetromino block
void Tetromino::set_initial_position(){
    move(0,3);
}

//Function for rotating a Tetromino block
void Tetromino::rotate(){

    current_rotation=current_rotation+1;
    if(current_rotation==(int)cells.size()){
        current_rotation=0;
    }
};

//Function for providing the row offset of a tetromino (For testing)
int Tetromino::get_row_offset(){

    return row_pos;
}

//Function for providing the column offset of a tetromino (For testing)
int Tetromino::get_col_offset(){

    return col_pos;
}

#include "Block_I.h"
#include "Tetromino.h"


BlockI::BlockI(){
    color_id = 1;

    cells[0] = {Position{1, 0}, Position{1, 1}, Position{1, 2}, Position{1, 3}};
    cells[1] = {Position{0, 2}, Position{1, 2}, Position{2, 2}, Position{3, 2}};
    cells[2] = {Position{2, 0}, Position{2, 1}, Position{2, 2}, Position{2, 3}};
    cells[3] = {Position{0, 1}, Position{1, 1}, Position{2, 1}, Position{3, 1}};
    set_initial_position();
}
//Changing the initial position of the I block to make it start its movement from the centre of the grid
void BlockI::set_initial_position(){
    move(-1,3);
}

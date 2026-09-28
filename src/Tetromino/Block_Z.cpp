#include "Block_Z.h"
#include "Tetromino.h"


BlockZ::BlockZ(){
    color_id = 7;
    cells[0] = {Position{0, 0}, Position{0, 1}, Position{1, 1}, Position{1, 2}};
    cells[1] = {Position{0, 2}, Position{1, 1}, Position{1, 2}, Position{2, 1}};
    cells[2] = {Position{1, 0}, Position{1, 1}, Position{2, 1}, Position{2, 2}};
    cells[3] = {Position{0, 1}, Position{1, 0}, Position{1, 1}, Position{2, 0}};
    set_initial_position();
}


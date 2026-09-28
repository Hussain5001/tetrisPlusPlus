// colours for the tetrimino's
#include "Color.h"

// Index 0 is an empty cell; 1-7 are the I, J, L, O, S, T and Z blocks
std::vector<Color> color_vector(){
    return {
        {22, 24, 38, 255},    // empty
        {64, 214, 230, 255},  // I - cyan
        {66, 110, 235, 255},  // J - blue
        {245, 150, 50, 255},  // L - orange
        {245, 215, 65, 255},  // O - yellow
        {95, 210, 95, 255},   // S - green
        {175, 85, 225, 255},  // T - purple
        {235, 70, 85, 255},   // Z - red
    };
}

// Colour for a cell value. Saves from the original game used random ids up
// to 10, so any id outside 1-7 is wrapped back into the palette.
Color cell_color(int id){
    static const std::vector<Color> colors = color_vector();
    if (id <= 0) return colors[0];
    return colors[1 + (id - 1) % 7];
}

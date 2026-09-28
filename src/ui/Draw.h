#pragma once
#include "raylib.h"

// Shared look for Tetris++: an old terminal / CRT monitor. Everything is
// drawn in one phosphor colour (amber or green) on near-black, or muted
// colours per block with the "multi" phosphor.
namespace ui {

enum Phosphor { kAmber = 0, kGreen = 1, kMulti = 2 };

struct Theme {
  Color background;  // the dark glass
  Color bright;      // highlighted text, focused items
  Color text;        // normal text
  Color dim;         // secondary text
  Color faint;       // grid dots, frames
  Color alert;       // warnings, game over
};

void set_phosphor(int phosphor);
int phosphor();
const Theme& theme();
const char* phosphor_name(int phosphor);

// Colour of a block cell for the current phosphor (id 1-7)
Color block_color(int color_id);

// A single block cell: glowing edge with a darker core
void draw_cell(int x, int y, int size, int color_id, float alpha = 1.0f);
// Where the falling block will land: dim [] brackets
void draw_ghost_cell(int x, int y, int size, int color_id);
// An empty board cell: a faint dot, like the 1984 original
void draw_empty_cell(int x, int y, int size);

// The font loaded by the App (falls back to raylib's default font)
Font& font();
void load_font();
void unload_font();

// Text helpers using the shared font
void text(const char* str, float x, float y, float size, Color color);
void text_centered(const char* str, float cx, float y, float size, Color color);
void text_right(const char* str, float right_x, float y, float size, Color color);
float text_width(const char* str, float size);

// "TETRIS++" in big letters made of block cells
void draw_logo(float cx, float y, int cell, float time);

// Blinking cursor state (on half the time)
bool cursor_on();

// raylib puts the cursor in the middle of the window at start, so hover
// effects only start once the mouse has really moved. Call once per frame.
void update_mouse();
bool mouse_active();

}  // namespace ui

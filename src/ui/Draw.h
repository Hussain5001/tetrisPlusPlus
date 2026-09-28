#pragma once
#include "raylib.h"

// Shared drawing helpers so the board, the falling block and the menus all
// use the same look.
namespace ui {

// Theme colours
const Color kBackground = {12, 13, 24, 255};
const Color kPanel = {26, 28, 46, 255};
const Color kPanelLight = {42, 45, 72, 255};
const Color kAccent = {110, 200, 255, 255};
const Color kText = {235, 238, 250, 255};
const Color kTextDim = {140, 146, 180, 255};

// A single bevelled block cell
void draw_cell(int x, int y, int size, Color color);

// Outline of a cell, used for the ghost piece
void draw_ghost_cell(int x, int y, int size, Color color);

// The font loaded by the App (falls back to raylib's default font)
Font& font();
void load_font();
void unload_font();

// Text helpers using the shared font
void text(const char* str, float x, float y, float size, Color color);
void text_centered(const char* str, float cx, float y, float size, Color color);
float text_width(const char* str, float size);

}  // namespace ui

#pragma once
#include <string>
#include <vector>

#include "../input/Action.h"
#include "raylib.h"

namespace ui {

// Draws a rounded button and returns true if it was clicked this frame.
// `focused` highlights it for keyboard / gesture navigation.
bool button(Rectangle r, const char* label, bool focused, Color accent,
            float font_size = 32);

// A vertical list of buttons that can be driven by the mouse, the keyboard
// (up/down/left/right + enter/space) or hand gestures (swipe left/right to
// move, swipe down or open palm to select).
class Menu {
 public:
  Menu() = default;
  explicit Menu(std::vector<std::string> items) : items(std::move(items)) {}

  // Draws the menu centred on cx starting at y and returns the index of the
  // item chosen this frame, or -1.
  int run(const std::vector<Action>& actions, float cx, float y,
          float width = 320, float height = 56, float gap = 14);

  std::vector<std::string> items;
  std::vector<Color> accents;  // optional per-item colours
  int focus = 0;
};

}  // namespace ui

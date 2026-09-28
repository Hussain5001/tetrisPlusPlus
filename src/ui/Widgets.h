#pragma once
#include <string>
#include <vector>

#include "../input/Action.h"
#include "raylib.h"

namespace ui {

// A terminal-style text button, drawn as "[ label ]". Returns true if clicked.
bool text_button(Rectangle r, const char* label, bool focused, float font_size = 26);

// A list of lines like a terminal menu:
//     > zen mode_
//       time attack
//       start level        < 5 >
// Keyboard: up/down (or left/right on plain items) move, enter/space select,
// left/right change values. Gestures: swipe left/right/up move or change
// values, swipe down or a held fist selects. Mouse: hover and click.
class Menu {
 public:
  Menu() = default;
  explicit Menu(std::vector<std::string> items) : items(std::move(items)) {}

  // Draws the menu with its first line at (x, y) and returns the index of
  // the item chosen this frame, or -1. After the call, `changed_row` is the
  // row whose value was changed this frame (or -1) and `change` is -1/+1.
  int run(const std::vector<Action>& actions, float x, float y, float width,
          float line_height = 40, float font_size = 32);

  // The keyboard/gesture part of run(), without drawing or the mouse:
  // moves the focus, sets changed_row/change and returns the chosen item
  int handle(const std::vector<Action>& actions);

  std::vector<std::string> items;
  std::vector<std::string> values;  // optional value shown on the right
  std::vector<bool> disabled;       // optional; disabled rows are skipped
  int focus = 0;
  int changed_row = -1;
  int change = 0;

 private:
  bool has_value(int i) const { return i < (int)values.size() && !values[i].empty(); }
  bool is_disabled(int i) const { return i < (int)disabled.size() && disabled[i]; }
  void move_focus(int step);
};

}  // namespace ui

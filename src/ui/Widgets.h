#pragma once
#include <string>
#include <vector>

#include "../input/Action.h"
#include "raylib.h"

namespace ui {

// Where the player's hand points, in screen coordinates (follow mode)
struct Pointer {
  float x;
  float y;
};

// A terminal-style text button, drawn as "[ label ]". Returns true if clicked.
bool text_button(Rectangle r, const char* label, bool focused, float font_size = 26);

// A list of lines like a terminal menu:
//     > zen mode_
//       time attack
//       start level        < 5 >
// Keyboard: up/down (or left/right on plain items) move, enter/space select,
// left/right change values. Hand (follow mode): the line under the hand is
// focused, a fist or a pinch clicks it (Action::Click: selects a plain line,
// cycles a value). Other gestures: swipe left/right/up move or change values,
// a drop selects. Mouse: hover and click.
class Menu {
 public:
  Menu() = default;
  explicit Menu(std::vector<std::string> items) : items(std::move(items)) {}

  // Draws the menu with its first line at (x, y) and returns the index of
  // the item chosen this frame, or -1. After the call, `changed_row` is the
  // row whose value was changed this frame (or -1) and `change` is -1/+1.
  // hand: where the player's hand points, or nullptr if it doesn't.
  int run(const std::vector<Action>& actions, float x, float y, float width,
          float line_height = 40, float font_size = 32, const Pointer* hand = nullptr);

  // The line at screen point (px, py) for a menu drawn at (x, y), or -1
  int row_at(float px, float py, float x, float y, float width, float line_height) const;

  // Focuses the line under the hand (if it is an enabled line). Returns true
  // if the focus moved.
  bool point_at(const Pointer& hand, float x, float y, float width, float line_height);

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

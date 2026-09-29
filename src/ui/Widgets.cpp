#include "Widgets.h"

#include "../Sound.h"
#include "Draw.h"

namespace ui {

bool text_button(Rectangle r, const char* label, bool focused, float font_size) {
  bool hover = mouse_active() && CheckCollisionPointRec(GetMousePosition(), r);
  const Theme& t = theme();
  Color c = focused ? t.bright : hover ? t.text : t.dim;
  if (focused || hover) {
    DrawRectangleRec(r, ColorAlpha(t.text, focused ? 0.16f : 0.08f));
  }
  std::string s = std::string("[ ") + label + " ]";
  text_centered(s.c_str(), r.x + r.width / 2, r.y + (r.height - font_size) / 2,
                font_size, c);
  bool clicked = hover && IsMouseButtonReleased(MOUSE_BUTTON_LEFT);
  if (clicked) sound::play(Sfx::MenuSelect);
  return clicked;
}

void Menu::move_focus(int step) {
  int count = (int)items.size();
  for (int tries = 0; tries < count; tries++) {
    focus = (focus + step + count) % count;
    if (!is_disabled(focus)) break;
  }
  sound::play(Sfx::MenuMove);
}

int Menu::handle(const std::vector<Action>& actions) {
  int count = (int)items.size();
  changed_row = -1;
  change = 0;
  if (count == 0) return -1;
  if (is_disabled(focus)) move_focus(1);
  int chosen = -1;

  for (Action a : actions) {
    bool value_row = has_value(focus);
    if (a == Action::Rotate) {
      move_focus(-1);
    } else if (a == Action::SoftDrop) {
      move_focus(1);
    } else if (a == Action::Left || a == Action::Right) {
      int dir = a == Action::Left ? -1 : 1;
      if (value_row) {
        changed_row = focus;
        change = dir;
        sound::play(Sfx::MenuMove);
      } else {
        move_focus(dir);
      }
    } else if (a == Action::Confirm || a == Action::HardDrop) {
      if (value_row) {
        move_focus(1);  // "ok, next" on option rows
      } else {
        chosen = focus;
      }
    }
  }
  return chosen;
}

int Menu::run(const std::vector<Action>& actions, float x, float y, float width,
              float line_height, float font_size) {
  int count = (int)items.size();
  int chosen = handle(actions);
  if (count == 0) return -1;

  const Theme& t = theme();
  Vector2 mouse = GetMousePosition();
  for (int i = 0; i < count; i++) {
    Rectangle r = {x - 10, y + i * line_height - 4, width + 20, line_height};
    bool hover = mouse_active() && !is_disabled(i) && CheckCollisionPointRec(mouse, r);
    bool focused = i == focus;
    if (focused) DrawRectangleRec(r, ColorAlpha(t.text, 0.10f));
    else if (hover) DrawRectangleRec(r, ColorAlpha(t.text, 0.05f));

    Color c = is_disabled(i) ? t.faint : focused ? t.bright : hover ? t.text : t.dim;
    float ty = y + i * line_height;
    std::string label = (focused ? "> " : "  ") + items[i];
    if (focused && !has_value(i) && cursor_on()) label += "_";
    text(label.c_str(), x, ty, font_size, c);
    if (has_value(i)) {
      std::string v = focused ? "< " + values[i] + " >" : values[i];
      text_right(v.c_str(), x + width, ty, font_size, c);
    }

    if (hover && IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
      focus = i;
      if (has_value(i)) {
        changed_row = i;
        change = 1;  // clicking a value cycles it
        sound::play(Sfx::MenuMove);
      } else {
        chosen = i;
      }
    }
  }
  if (chosen >= 0) sound::play(Sfx::MenuSelect);
  return chosen;
}

}  // namespace ui

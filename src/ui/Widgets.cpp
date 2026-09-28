#include "Widgets.h"

#include "Draw.h"

namespace ui {

bool button(Rectangle r, const char* label, bool focused, Color accent,
            float font_size) {
  Vector2 mouse = GetMousePosition();
  bool hover = CheckCollisionPointRec(mouse, r);
  bool pressed = hover && IsMouseButtonDown(MOUSE_BUTTON_LEFT);
  bool active = focused || hover;

  Rectangle face = r;
  if (pressed) {
    face.y += 2;
  }
  // Drop shadow
  DrawRectangleRounded({r.x, r.y + 4, r.width, r.height}, 0.3f, 8,
                       ColorAlpha(BLACK, 0.4f));
  DrawRectangleRounded(face, 0.3f, 8, active ? ColorBrightness(accent, -0.2f) : kPanel);
  DrawRectangleRoundedLines(face, 0.3f, 8, 2, active ? ColorBrightness(accent, 0.3f) : kPanelLight);
  if (focused) {
    // Small arrow marker on the focused item
    float cy = face.y + face.height / 2;
    DrawTriangle({face.x - 18, cy - 9}, {face.x - 18, cy + 9}, {face.x - 6, cy},
                 accent);
  }
  text_centered(label, face.x + face.width / 2,
                face.y + (face.height - font_size) / 2, font_size,
                active ? WHITE : kText);
  return hover && IsMouseButtonReleased(MOUSE_BUTTON_LEFT);
}

int Menu::run(const std::vector<Action>& actions, float cx, float y,
              float width, float height, float gap) {
  int count = (int)items.size();
  if (count == 0) return -1;
  int chosen = -1;

  for (Action a : actions) {
    if (a == Action::Left || a == Action::Rotate) {
      focus = (focus + count - 1) % count;
    } else if (a == Action::Right || a == Action::SoftDrop) {
      focus = (focus + 1) % count;
    } else if (a == Action::Confirm || a == Action::HardDrop) {
      chosen = focus;
    }
  }

  for (int i = 0; i < count; i++) {
    Rectangle r = {cx - width / 2, y + i * (height + gap), width, height};
    if (CheckCollisionPointRec(GetMousePosition(), r) &&
        (GetMouseDelta().x != 0 || GetMouseDelta().y != 0)) {
      focus = i;  // mouse movement moves the focus too
    }
    Color accent = i < (int)accents.size() ? accents[i] : kAccent;
    if (button(r, items[i].c_str(), i == focus, accent)) chosen = i;
  }
  return chosen;
}

}  // namespace ui

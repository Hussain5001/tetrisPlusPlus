#include "InputManager.h"

#include "raylib.h"

InputManager::InputManager(bool any_interface) {
  repeat_keys_ = {{KEY_LEFT, Action::Left, 0, 0},
                  {KEY_RIGHT, Action::Right, 0, 0},
                  {KEY_DOWN, Action::SoftDrop, 0, 0}};
  gestures_.open(kGesturePort, any_interface);
}

const std::vector<Action>& InputManager::poll() {
  actions_.clear();
  double dt = GetFrameTime();

  // Movement keys: fire on press, then auto-repeat while held
  for (RepeatKey& k : repeat_keys_) {
    if (IsKeyPressed(k.key)) {
      actions_.push_back(k.action);
      k.held_for = 0;
      k.next_repeat = kDelayedAutoShift;
    } else if (IsKeyDown(k.key)) {
      k.held_for += dt;
      while (k.held_for >= k.next_repeat) {
        actions_.push_back(k.action);
        k.next_repeat += kAutoRepeatRate;
      }
    }
  }

  // One-shot keys
  if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_X)) actions_.push_back(Action::Rotate);
  if (IsKeyPressed(KEY_SPACE)) actions_.push_back(Action::HardDrop);
  if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_P)) actions_.push_back(Action::Pause);
  if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER)) actions_.push_back(Action::Confirm);
  if (IsKeyPressed(KEY_BACKSPACE)) actions_.push_back(Action::Back);

  gestures_.poll(actions_);
  return actions_;
}

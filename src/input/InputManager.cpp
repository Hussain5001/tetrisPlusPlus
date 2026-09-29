#include "InputManager.h"

#include "raylib.h"

InputManager::InputManager(bool any_interface) {
  repeat_keys_ = {{KEY_LEFT, Action::Left, AutoRepeat()},
                  {KEY_RIGHT, Action::Right, AutoRepeat()},
                  {KEY_DOWN, Action::SoftDrop, AutoRepeat()}};
  gestures_.open(kGesturePort, any_interface);
}

const std::vector<Action>& InputManager::poll() {
  actions_.clear();
  double dt = GetFrameTime();

  // Movement keys: fire on press, then auto-repeat while held
  for (RepeatKey& k : repeat_keys_) {
    int count = k.repeat.update(IsKeyPressed(k.key), IsKeyDown(k.key), dt);
    for (int i = 0; i < count; i++) actions_.push_back(k.action);
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

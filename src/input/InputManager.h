#pragma once
#include <vector>

#include "Action.h"
#include "AutoRepeat.h"
#include "GestureSource.h"

// Collects actions from the keyboard and the gesture sidecar each frame.
// Holding a movement key auto-repeats after a short delay (DAS/ARR), like
// modern Tetris games, instead of needing one key press per cell.
class InputManager {
 public:
  static const int kGesturePort = 5005;

  // any_interface: accept gestures from other machines / Windows (WSL)
  explicit InputManager(bool any_interface = false);

  // Call once per frame; returns the actions triggered this frame.
  const std::vector<Action>& poll();

  const GestureSource& gestures() const { return gestures_; }

 private:
  struct RepeatKey {
    int key;
    Action action;
    AutoRepeat repeat;
  };

  std::vector<Action> actions_;
  std::vector<RepeatKey> repeat_keys_;
  GestureSource gestures_;
};

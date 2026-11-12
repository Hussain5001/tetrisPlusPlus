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

  InputManager();

  // Call once per frame; returns the actions triggered this frame.
  const std::vector<Action>& poll();

  // True if a key did something this frame (not a gesture)
  bool keyboard_used() const { return keyboard_used_; }

  // True if the left/right keys moved the piece this frame
  bool keyboard_moved() const { return keyboard_moved_; }

  const GestureSource& gestures() const { return gestures_; }
  GestureSource& gestures() { return gestures_; }

 private:
  struct RepeatKey {
    int key;
    Action action;
    AutoRepeat repeat;
  };

  std::vector<Action> actions_;
  bool keyboard_used_ = false;
  bool keyboard_moved_ = false;
  std::vector<RepeatKey> repeat_keys_;
  GestureSource gestures_;
};

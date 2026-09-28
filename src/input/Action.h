#pragma once

// Everything the player can ask the game to do, independent of whether the
// request came from the keyboard, the mouse or a hand gesture.
enum class Action {
  Left,
  Right,
  SoftDrop,
  HardDrop,
  Rotate,
  Pause,
  Confirm,
  Back
};

// Human readable name, used by the HUD to show the last gesture received
inline const char* action_name(Action a) {
  switch (a) {
    case Action::Left: return "LEFT";
    case Action::Right: return "RIGHT";
    case Action::SoftDrop: return "DOWN";
    case Action::HardDrop: return "DROP";
    case Action::Rotate: return "ROTATE";
    case Action::Pause: return "PAUSE";
    case Action::Confirm: return "SELECT";
    case Action::Back: return "BACK";
  }
  return "?";
}

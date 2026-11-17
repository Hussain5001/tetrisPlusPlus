#pragma once

// Small decisions the App makes, kept free of raylib so the tests can check
// them without a window.
namespace rules {

// The game pauses itself when its window loses focus, unless a hand is
// playing: the camera preview window takes the focus when it opens, and
// gestures reach the game whether it has the focus or not.
inline bool pause_on_focus_loss(bool was_focused, bool focused, bool playing,
                                bool hand_connected) {
  return was_focused && !focused && playing && !hand_connected;
}

}  // namespace rules

#pragma once

// Holding a movement key: one action on the press, then after `delay`
// seconds one action every `rate` seconds (DAS/ARR in Tetris terms).
// Kept separate from raylib so it can be unit tested.
struct AutoRepeat {
  double delay = 0.17;
  double rate = 0.05;

  // Returns how many actions to fire this frame
  int update(bool pressed, bool down, double dt) {
    if (pressed) {
      held_ = 0;
      next_ = delay;
      active_ = true;
      return 1;
    }
    if (!down) {
      active_ = false;
      return 0;
    }
    if (!active_) return 0;  // held since before we started watching
    held_ += dt;
    int count = 0;
    while (held_ >= next_) {
      count++;
      next_ += rate;
    }
    return count;
  }

 private:
  double held_ = 0;
  double next_ = 0;
  bool active_ = false;
};

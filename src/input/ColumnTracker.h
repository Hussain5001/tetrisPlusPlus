#pragma once
#include <cmath>

// Turns the hand's sideways position (0 = left edge of its zone, 1 = right
// edge) into a board column. A hand resting on the border between two
// columns would make the piece flicker between them, so the column only
// changes once the hand is clearly inside the next one.
class ColumnTracker {
 public:
  explicit ColumnTracker(int columns = 10) : columns_(columns) {}

  // Returns the column for position u; call once per frame while the hand is
  // in view.
  int update(float u) {
    int raw = (int)std::floor(u * columns_);
    if (raw < 0) raw = 0;
    if (raw > columns_ - 1) raw = columns_ - 1;
    if (column_ < 0) {
      column_ = raw;
    } else if (raw != column_) {
      float centre = (column_ + 0.5f) / columns_;
      if (std::fabs(u - centre) >= kLeave / columns_) column_ = raw;
    }
    return column_;
  }

  // The hand has left: start fresh when it comes back
  void reset() { column_ = -1; }

  int column() const { return column_; }

 private:
  // How far from the middle of its column (in column widths) the hand must
  // be before the column changes: 0.5 would be the border itself
  static constexpr float kLeave = 0.8f;
  int columns_;
  int column_ = -1;
};

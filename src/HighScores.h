#pragma once
#include <string>
#include <vector>

// One finished game
struct ScoreEntry {
  double value = 0;   // points, or seconds for First 40 Lines
  int lines = 0;
  int level = 0;      // Zen start level (0 for other modes)
  std::string date;   // YYYY-MM-DD
};

// Best five results and the most recent result for each mode, kept in a
// JSON file (scores.json) so they survive restarts.
// Modes use the App's numbering: 1 Zen, 2 Time Attack, 3 First 40 Lines.
class HighScores {
 public:
  static const int kModes = 3;
  static const int kTableSize = 5;

  explicit HighScores(std::string path = "scores.json");

  // Reads the file; a missing or broken file just means no scores yet
  void load();
  // Writes the file; returns false if it couldn't be written
  bool save() const;

  // Stores a finished game as the mode's last run and, if `eligible` and good
  // enough, in the top five. Returns its rank (1-5) or 0. Saves immediately.
  int record(int mode, ScoreEntry entry, bool eligible = true);

  const std::vector<ScoreEntry>& top(int mode) const;
  const ScoreEntry* best(int mode) const;  // nullptr if none
  const ScoreEntry* last(int mode) const;  // nullptr if none

  // First 40 Lines is a race, so a lower time is better
  static bool lower_is_better(int mode) { return mode == 3; }

  int total_entries() const;

  static std::string today();

 private:
  struct ModeScores {
    std::vector<ScoreEntry> top;
    ScoreEntry last;
    bool has_last = false;
  };
  std::string path_;
  ModeScores modes_[kModes];
  ModeScores& slot(int mode);
  const ModeScores& slot(int mode) const;
};

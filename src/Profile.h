#pragma once
#include <string>

// The player's last choices on the Zen setup screen, kept in profile.json
struct Profile {
  int zen_level = 5;      // 1-10
  int zen_pace = 2;       // index into App's pace list (chill/steady/rising/brutal)
  int phosphor = 0;       // 0 amber, 1 green, 2 multi
  bool ghost = true;      // show where the block will land
  bool crt = true;        // CRT screen effect
  bool sound = true;

  void load(const std::string& path = "profile.json");
  bool save(const std::string& path = "profile.json") const;
};

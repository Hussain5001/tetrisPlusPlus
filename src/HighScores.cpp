#include "HighScores.h"

#include <algorithm>
#include <ctime>
#include <fstream>
#include <iostream>

#include "../vendor/json/single_include/nlohmann/json.hpp"

using json = nlohmann::json;

namespace {
const char* kKeys[] = {"zen", "time_attack", "first_40"};

json to_json(const ScoreEntry& e) {
  return {{"value", e.value}, {"lines", e.lines}, {"level", e.level}, {"date", e.date}};
}

ScoreEntry from_json(const json& j) {
  ScoreEntry e;
  if (!j.is_object()) return e;
  e.value = j.value("value", 0.0);
  e.lines = j.value("lines", 0);
  e.level = j.value("level", 0);
  e.date = j.value("date", std::string());
  return e;
}
}  // namespace

HighScores::HighScores(std::string path) : path_(std::move(path)) {}

HighScores::ModeScores& HighScores::slot(int mode) {
  return modes_[std::min(std::max(mode, 1), kModes) - 1];
}

const HighScores::ModeScores& HighScores::slot(int mode) const {
  return modes_[std::min(std::max(mode, 1), kModes) - 1];
}

void HighScores::load() {
  for (ModeScores& m : modes_) m = ModeScores();
  std::ifstream file(path_);
  if (!file.is_open()) return;
  try {
    json data = json::parse(file);
    for (int i = 0; i < kModes; i++) {
      if (!data.contains(kKeys[i])) continue;
      const json& mode = data[kKeys[i]];
      if (mode.contains("top") && mode["top"].is_array()) {
        for (const json& e : mode["top"]) modes_[i].top.push_back(from_json(e));
        if ((int)modes_[i].top.size() > kTableSize) modes_[i].top.resize(kTableSize);
      }
      if (mode.contains("last") && mode["last"].is_object()) {
        modes_[i].last = from_json(mode["last"]);
        modes_[i].has_last = true;
      }
    }
  } catch (const std::exception& e) {
    std::cerr << "Ignoring unreadable " << path_ << ": " << e.what() << std::endl;
    for (ModeScores& m : modes_) m = ModeScores();
  }
}

bool HighScores::save() const {
  json data;
  for (int i = 0; i < kModes; i++) {
    json top = json::array();
    for (const ScoreEntry& e : modes_[i].top) top.push_back(to_json(e));
    data[kKeys[i]]["top"] = top;
    if (modes_[i].has_last) data[kKeys[i]]["last"] = to_json(modes_[i].last);
  }
  std::ofstream file(path_);
  if (!file.is_open()) return false;
  file << data.dump(2);
  return file.good();
}

int HighScores::record(int mode, ScoreEntry entry, bool eligible) {
  if (entry.date.empty()) entry.date = today();
  ModeScores& m = slot(mode);
  m.last = entry;
  m.has_last = true;

  int rank = 0;
  if (eligible) {
    bool lower = lower_is_better(mode);
    auto better = [lower](const ScoreEntry& a, const ScoreEntry& b) {
      return lower ? a.value < b.value : a.value > b.value;
    };
    // Insert after equal scores so older results keep their place
    auto pos = std::upper_bound(m.top.begin(), m.top.end(), entry,
                                [&](const ScoreEntry& a, const ScoreEntry& b) {
                                  return better(a, b);
                                });
    int index = (int)(pos - m.top.begin());
    if (index < kTableSize) {
      m.top.insert(pos, entry);
      if ((int)m.top.size() > kTableSize) m.top.resize(kTableSize);
      rank = index + 1;
    }
  }
  save();
  return rank;
}

const std::vector<ScoreEntry>& HighScores::top(int mode) const { return slot(mode).top; }

const ScoreEntry* HighScores::best(int mode) const {
  const ModeScores& m = slot(mode);
  return m.top.empty() ? nullptr : &m.top.front();
}

const ScoreEntry* HighScores::last(int mode) const {
  const ModeScores& m = slot(mode);
  return m.has_last ? &m.last : nullptr;
}

int HighScores::total_entries() const {
  int n = 0;
  for (const ModeScores& m : modes_) n += (int)m.top.size();
  return n;
}

std::string HighScores::today() {
  std::time_t now = std::time(nullptr);
  char buf[16];
  std::strftime(buf, sizeof(buf), "%Y-%m-%d", std::localtime(&now));
  return buf;
}

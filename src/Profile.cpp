#include "Profile.h"

#include <algorithm>
#include <fstream>
#include <iostream>

#include "../vendor/json/single_include/nlohmann/json.hpp"

using json = nlohmann::json;

void Profile::load(const std::string& path) {
  std::ifstream file(path);
  if (!file.is_open()) return;  // first run: keep the defaults
  try {
    json j = json::parse(file);
    zen_level = std::min(std::max(j.value("zen_level", zen_level), 1), 10);
    zen_pace = std::min(std::max(j.value("zen_pace", zen_pace), 0), 3);
    phosphor = std::min(std::max(j.value("phosphor", phosphor), 0), 2);
    ghost = j.value("ghost", ghost);
    crt = j.value("crt", crt);
    sound = j.value("sound", sound);
  } catch (const std::exception& e) {
    std::cerr << "Ignoring unreadable " << path << ": " << e.what() << std::endl;
  }
}

bool Profile::save(const std::string& path) const {
  json j = {{"zen_level", zen_level}, {"zen_pace", zen_pace}, {"phosphor", phosphor},
            {"ghost", ghost},         {"crt", crt},           {"sound", sound}};
  std::ofstream file(path);
  if (!file.is_open()) return false;
  file << j.dump(2);
  return file.good();
}

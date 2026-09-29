#pragma once
#include <chrono>
#include <cmath>
#include <cstdio>
#include <iostream>
#include <string>
#include <thread>

// Minimal test helpers: every check prints "passed"/"FAILED" like the
// original unit tests, and main() returns a failing exit code if any check
// failed, so CI notices.
namespace test {

struct Stats {
  int passed = 0;
  int failed = 0;
};

inline Stats& stats() {
  static Stats s;
  return s;
}

inline void check(bool ok, const std::string& name) {
  if (ok) {
    stats().passed++;
    std::cout << "Test for " << name << " passed!" << std::endl;
  } else {
    stats().failed++;
    std::cout << "Test for " << name << " FAILED" << std::endl;
  }
}

inline bool near(double a, double b, double eps = 1e-9) { return std::fabs(a - b) < eps; }

inline void sleep_ms(int ms) { std::this_thread::sleep_for(std::chrono::milliseconds(ms)); }

// Files written by tests go here and are removed afterwards
inline std::string temp_file(const std::string& name) { return "tetris_test_" + name; }

}  // namespace test

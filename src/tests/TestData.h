#pragma once
#include <fstream>

#include "../Color.h"
#include "../HighScores.h"
#include "../Profile.h"
#include "../ui/Draw.h"
#include "TestRunner.h"

// Tests for what's saved between runs (high scores, profile) and colours
class TestData {
 public:
  void run() {
    high_score_ranking();
    high_score_ties_and_fields();
    high_score_first_forty();
    high_score_persistence();
    profile_round_trip();
    profile_clamps_and_broken_file();
    colours();
  }

 private:
  static ScoreEntry entry(double value, int lines = 0, int level = 0) {
    ScoreEntry e;
    e.value = value;
    e.lines = lines;
    e.level = level;
    return e;
  }

  void high_score_ranking() {
    std::string path = test::temp_file("scores.json");
    std::remove(path.c_str());
    HighScores scores(path);
    scores.load();
    bool empty = scores.best(1) == nullptr && scores.last(1) == nullptr &&
                 scores.total_entries() == 0;
    int ranks[7];
    double zen[] = {500, 900, 100, 700, 300, 800, 50};
    for (int i = 0; i < 7; i++) ranks[i] = scores.record(1, entry(zen[i]));
    const std::vector<ScoreEntry>& top = scores.top(1);
    bool sorted = top.size() == 5 && top[0].value == 900 && top[1].value == 800 &&
                  top[2].value == 700 && top[3].value == 500 && top[4].value == 300;
    test::check(empty && sorted, "high scores keep the best 5, best first");
    test::check(ranks[1] == 1 && ranks[5] == 2 && ranks[6] == 0, "high scores report the rank");
    test::check(scores.last(1)->value == 50, "last run is kept even when it's not a high score");
    test::check(scores.top(2).empty() && scores.last(2) == nullptr,
                "each mode has its own high scores");
    std::remove(path.c_str());
  }

  void high_score_ties_and_fields() {
    std::string path = test::temp_file("scores_ties.json");
    std::remove(path.c_str());
    HighScores scores(path);
    scores.record(2, entry(400, 3));
    int rank = scores.record(2, entry(400, 9));
    bool ties = rank == 2 && scores.top(2)[0].lines == 3 && scores.top(2)[1].lines == 9;
    test::check(ties, "an equal score ranks below the older one");
    scores.record(1, entry(1000, 12, 7));
    const ScoreEntry* best = scores.best(1);
    test::check(best && best->lines == 12 && best->level == 7 && best->date.size() == 10,
                "high scores keep lines, level and date");
    std::remove(path.c_str());
  }

  void high_score_first_forty() {
    std::string path = test::temp_file("scores_forty.json");
    std::remove(path.c_str());
    HighScores scores(path);
    scores.record(3, entry(95.0, 40));
    int fast = scores.record(3, entry(71.5, 40));
    int unfinished = scores.record(3, entry(12.0, 9), false);
    test::check(fast == 1 && unfinished == 0 && scores.best(3)->value == 71.5 &&
                    scores.top(3).size() == 2 && scores.last(3)->lines == 9,
                "first 40 lines: fastest wins, unfinished runs aren't ranked");
    std::remove(path.c_str());
  }

  void high_score_persistence() {
    std::string path = test::temp_file("scores_saved.json");
    std::remove(path.c_str());
    {
      HighScores scores(path);
      scores.record(1, entry(900));
      scores.record(3, entry(12.0, 9), false);
    }
    HighScores reloaded(path);
    reloaded.load();
    test::check(reloaded.best(1) && reloaded.best(1)->value == 900 && reloaded.last(3) &&
                    reloaded.last(3)->value == 12.0 && !reloaded.best(1)->date.empty(),
                "high scores are saved and reloaded");

    std::ofstream(path) << "{ not json";
    HighScores broken(path);
    broken.load();
    test::check(broken.total_entries() == 0 && broken.last(1) == nullptr,
                "a broken scores file starts empty");

    std::ofstream(path) << "{\"zen\": {\"top\": [1, {\"value\": 5}], \"last\": 3}}";
    HighScores odd(path);
    odd.load();
    test::check(odd.top(1).size() == 2 && odd.top(1)[1].value == 5 && odd.last(1) == nullptr,
                "unexpected values in the scores file are ignored");
    std::remove(path.c_str());
  }

  void profile_round_trip() {
    std::string path = test::temp_file("profile.json");
    Profile p;
    p.zen_level = 8;
    p.zen_pace = 3;
    p.phosphor = 1;
    p.ghost = false;
    p.crt = false;
    p.sound = false;
    p.save(path);
    Profile q;
    q.load(path);
    std::remove(path.c_str());
    test::check(q.zen_level == 8 && q.zen_pace == 3 && q.phosphor == 1 && !q.ghost && !q.crt &&
                    !q.sound,
                "profile is saved and reloaded");
    Profile fresh;
    fresh.load(test::temp_file("no_profile.json"));
    test::check(fresh.zen_level == 5 && fresh.zen_pace == 2 && fresh.ghost && fresh.crt &&
                    fresh.sound,
                "profile defaults on first run");
  }

  void profile_clamps_and_broken_file() {
    std::string path = test::temp_file("profile_bad.json");
    std::ofstream(path) << "{\"zen_level\": 99, \"zen_pace\": -3, \"phosphor\": 7}";
    Profile p;
    p.load(path);
    test::check(p.zen_level == 10 && p.zen_pace == 0 && p.phosphor == 2,
                "profile values are kept in range");
    std::ofstream(path) << "garbage";
    Profile q;
    q.load(path);
    std::remove(path.c_str());
    test::check(q.zen_level == 5 && q.sound, "a broken profile keeps the defaults");
  }

  static bool same(Color a, Color b) {
    return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a;
  }

  void colours() {
    std::vector<Color> palette = color_vector();
    test::check(palette.size() == 8 && same(cell_color(0), palette[0]) &&
                    same(cell_color(8), cell_color(1)) && same(cell_color(10), cell_color(3)),
                "old saves' colour ids wrap into the palette");

    bool distinct = true;
    for (int phosphor = 0; phosphor < 3; phosphor++) {
      ui::set_phosphor(phosphor);
      for (int a = 1; a <= 7; a++) {
        for (int b = a + 1; b <= 7; b++) {
          if (same(ui::block_color(a), ui::block_color(b))) distinct = false;
        }
      }
    }
    test::check(distinct, "every block has its own shade in each phosphor");
    ui::set_phosphor(1);
    bool green = ui::theme().text.g > ui::theme().text.r;
    ui::set_phosphor(42);
    bool fallback = ui::phosphor() == ui::kAmber && ui::theme().text.r > ui::theme().text.g;
    test::check(green && fallback && same(ui::block_color(8), ui::block_color(1)),
                "phosphor themes and fallback");
  }
};

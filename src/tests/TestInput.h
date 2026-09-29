#pragma once
#include <vector>

#include "../input/AutoRepeat.h"
#include "../input/GestureSource.h"
#include "../ui/Widgets.h"
#include "TestRunner.h"

// Tests for input: key auto-repeat, the UDP gesture protocol and menu
// navigation (the keyboard/gesture part of ui::Menu)
class TestInput {
 public:
  void run() {
    auto_repeat();
    gesture_protocol();
    gesture_position_mode();
    gesture_port_busy();
    menu_navigation();
    menu_disabled_rows();
    menu_value_rows();
  }

 private:
  static const int kPort = 5099;

  void auto_repeat() {
    AutoRepeat r;
    bool press = r.update(true, true, 0.016) == 1;
    bool waits = r.update(false, true, 0.10) == 0;
    bool first = r.update(false, true, 0.08) == 1;       // 0.18s held
    bool rate = r.update(false, true, 0.05) == 1;        // every 0.05s
    bool catch_up = r.update(false, true, 0.20) == 4;    // a slow frame
    bool release = r.update(false, false, 0.016) == 0;
    bool stale = r.update(false, true, 1.0) == 0;        // held without a new press
    test::check(press && waits && first && rate && catch_up && release && stale,
                "holding a key repeats after a delay");
  }

  static std::vector<Action> receive(GestureSource& g, const std::string& data) {
    std::vector<Action> out;
    GestureSource::send_to_local(kPort, data);
    for (int i = 0; i < 50; i++) {  // wait up to 0.5s for the datagram
      g.poll(out);
      if (!out.empty() || g.connected()) break;
      test::sleep_ms(10);
    }
    for (int i = 0; i < 5; i++) g.poll(out);
    return out;
  }

  void gesture_protocol() {
    GestureSource g;
    bool opened = g.open(kPort);
    bool quiet = !g.connected() && g.target_column() == -1;
    std::vector<Action> none;
    g.poll(none);
    std::vector<Action> got = receive(g, "LRDHUPCB");
    std::vector<Action> want = {Action::Left,  Action::Right, Action::SoftDrop,
                                Action::HardDrop, Action::Rotate, Action::Pause,
                                Action::Confirm, Action::Back};
    test::check(opened && g.is_open() && quiet && none.empty(), "gesture socket opens");
    test::check(got == want, "gesture letters become actions in order");
    test::check(g.connected() && g.last_action() == Action::Back &&
                    g.seconds_since_gesture() < 1,
                "gesture link shows the last gesture");
    std::vector<Action> junk = receive(g, "xyz 123 K\n");
    test::check(junk.empty(), "keep-alives and unknown letters are ignored");
  }

  void gesture_position_mode() {
    GestureSource g;
    g.open(kPort);
    receive(g, "@7");
    bool seven = g.target_column() == 7;
    std::vector<Action> mixed = receive(g, "@12R");
    bool twelve = g.target_column() == 12 && mixed.size() == 1 && mixed[0] == Action::Right;
    receive(g, "@");
    bool kept = g.target_column() == 12;
    test::sleep_ms(350);
    bool expired = g.target_column() == -1;
    test::check(seven && twelve && kept, "position mode reads the target column");
    test::check(expired, "position mode stops when the hand is gone");
  }

  void gesture_port_busy() {
    GestureSource first;
    first.open(kPort);
    GestureSource second;
    bool busy = !second.open(kPort) && !second.is_open();
    std::vector<Action> out;
    second.poll(out);  // must be safe on a closed socket
    test::check(busy && out.empty(), "a busy gesture port is reported");
  }

  static std::vector<Action> acts(std::initializer_list<Action> list) { return list; }

  void menu_navigation() {
    ui::Menu menu({"a", "b", "c", "d"});
    menu.handle(acts({Action::Right}));
    bool right = menu.focus == 1;
    menu.handle(acts({Action::Left, Action::Left}));
    bool wraps = menu.focus == 3;
    menu.handle(acts({Action::Rotate}));
    bool up = menu.focus == 2;
    menu.handle(acts({Action::SoftDrop}));
    bool down = menu.focus == 3;
    int chosen = menu.handle(acts({Action::HardDrop}));
    int confirmed = menu.handle(acts({Action::Confirm}));
    int nothing = menu.handle(acts({Action::Pause}));
    test::check(right && wraps && up && down, "menu focus moves and wraps");
    test::check(chosen == 3 && confirmed == 3 && nothing == -1,
                "menu selects with enter, a drop or a fist");
    ui::Menu empty;
    test::check(empty.handle(acts({Action::Confirm})) == -1, "an empty menu selects nothing");
  }

  void menu_disabled_rows() {
    ui::Menu menu({"a", "b", "c"});
    menu.disabled = {false, true, false};
    menu.handle(acts({Action::SoftDrop}));
    bool skipped = menu.focus == 2;
    menu.focus = 1;
    int chosen = menu.handle(acts({Action::Confirm}));
    test::check(skipped && chosen == 2, "menu skips disabled rows");
  }

  void menu_value_rows() {
    ui::Menu menu({"level", "start"});
    menu.values = {"5", ""};
    menu.handle(acts({Action::Right}));
    bool inc = menu.changed_row == 0 && menu.change == 1 && menu.focus == 0;
    menu.handle(acts({Action::Left}));
    bool dec = menu.changed_row == 0 && menu.change == -1;
    menu.handle({});
    bool reset = menu.changed_row == -1 && menu.change == 0;
    int next = menu.handle(acts({Action::Confirm}));
    int start = menu.handle(acts({Action::Confirm}));
    test::check(inc && dec && reset, "left/right change a setting");
    test::check(next == -1 && start == 1, "select on a setting moves to the next row");
  }
};

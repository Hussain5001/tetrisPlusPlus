#pragma once
#include <vector>

#include "../AppRules.h"
#include "../input/AutoRepeat.h"
#include "../input/ColumnTracker.h"
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
    focus_pause_rule();
    gesture_keep_alive_answer();
    gesture_quit();
    hand_switch_on_and_off();
    hand_switch_while_starting();
    hand_switch_script_comes_and_goes();
    background_command();
    gesture_pointer();
    column_tracker();
    hand_follow_and_keys();
    hand_lost_pause();
    menu_hand_cursor();
    hand_clicks_in_menus();
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
    test::check(!g.inherited_by_children(),
                "the camera script doesn't inherit the game's socket (the port frees on exit)");
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

  // Polls until `done` is true (up to half a second)
  template <typename Done>
  static void poll_until(GestureSource& g, std::vector<Action>& out, Done done) {
    for (int i = 0; i < 50 && !done(); i++) {
      g.poll(out);
      if (!done()) test::sleep_ms(10);
    }
  }

  // The camera script sends "K" twice a second; the game answers "k" so the
  // script knows the game is still there
  void gesture_keep_alive_answer() {
    GestureSource g;
    g.open(kPort);
    std::vector<Action> out = receive(g, "L");
    bool script_only = g.connected() && !g.has_sidecar();  // a command alone is no sidecar

    UdpProbe script;
    script.send(kPort, "KR");
    out.clear();
    poll_until(g, out, [&] { return g.has_sidecar(); });
    std::string answer = script.receive();
    test::check(script_only, "a program that only sends commands is not the camera script");
    test::check(g.has_sidecar() && answer == "k" && out.size() == 1 && out[0] == Action::Right,
                "the game answers the camera script's keep-alive");
    test::check(script.receive(50).empty(), "one answer per keep-alive");
  }

  // Switching hand control off tells the script to close
  void gesture_quit() {
    GestureSource g;
    g.open(kPort);
    bool nobody = !g.quit_sidecar();
    UdpProbe script;
    script.send(kPort, "K");
    std::vector<Action> out;
    poll_until(g, out, [&] { return g.has_sidecar(); });
    script.receive();  // the "k"

    bool sent = g.quit_sidecar();
    std::string told = script.receive();
    bool forgotten = !g.has_sidecar() && !g.connected();
    test::check(nobody, "nothing to quit before a camera script shows up");
    test::check(sent && told == "Q" && forgotten, "quit tells the camera script to close");

    // It may still send for a moment: that is ignored and it is told again
    script.send(kPort, "KLH@4");
    out.clear();
    for (int i = 0; i < 20; i++) {
      g.poll(out);
      test::sleep_ms(5);
    }
    std::string again = script.receive();
    test::check(out.empty() && !g.connected() && g.target_column() == -1 && again == "Q",
                "a camera script that was told to quit can't move anything");
    // Somebody else can still play
    std::vector<Action> other = receive(g, "R");
    test::check(other.size() == 1 && other[0] == Action::Right,
                "other programs are still heard after a quit");
  }

  typedef rules::HandSwitch::Do Do;

  void hand_switch_on_and_off() {
    rules::HandSwitch hand;
    bool off = !hand.wanted() && hand.link(true, false, 0) == rules::HandLink::Off;
    bool launch = hand.toggle(0, false) == Do::Launch && hand.wanted();
    bool starting = hand.link(true, false, 10) == rules::HandLink::Starting;
    bool waits = hand.update(10, false) == Do::Nothing && hand.wanted();
    bool up = hand.update(20, true) == Do::Nothing &&
              hand.link(true, true, 20) == rules::HandLink::On;
    bool quit = hand.toggle(30, true) == Do::Quit && !hand.wanted();
    bool off_again = hand.update(31, false) == Do::Nothing &&
                     hand.link(true, false, 31) == rules::HandLink::Off;
    bool relaunch = hand.toggle(40, false) == Do::Launch;
    test::check(off && launch && starting && waits && up,
                "hand control: switching on starts the camera script");
    test::check(quit && off_again && relaunch,
                "hand control: switching off stops it, and it can be started again");
    test::check(hand.link(false, false, 0) == rules::HandLink::PortBusy,
                "hand control: a busy port is shown");
  }

  void hand_switch_while_starting() {
    // Switched off before the script is up: it is stopped when it arrives
    rules::HandSwitch hand;
    hand.toggle(0, false);
    bool off = hand.toggle(5, false) == Do::Nothing && !hand.wanted() &&
               hand.link(true, false, 6) == rules::HandLink::Off;
    bool stopped = hand.update(20, true) == Do::Quit;
    test::check(off && stopped, "hand control: switched off while starting stops the late script");

    // Switched off and on again before it is up: no second script
    rules::HandSwitch again;
    again.toggle(0, false);
    again.toggle(5, false);
    bool reused = again.toggle(6, false) == Do::Nothing && again.wanted();
    bool kept = again.update(20, true) == Do::Nothing && again.wanted();
    test::check(reused && kept, "hand control: never starts two camera scripts");

    // The script never shows up (no Python, no camera)
    rules::HandSwitch stuck;
    stuck.toggle(0, false);
    bool no_answer = stuck.link(true, false, rules::kHandStartTimeout + 1) ==
                     rules::HandLink::NoAnswer;
    stuck.toggle(200, false);
    bool retry = stuck.toggle(201, false) == Do::Launch;
    rules::HandSwitch failed;
    failed.toggle(0, false);
    failed.launch_failed();
    bool gave_up = !failed.wanted() && failed.link(true, false, 1) == rules::HandLink::Off;
    test::check(no_answer && retry && gave_up,
                "hand control: a script that doesn't start can be retried");
  }

  void hand_switch_script_comes_and_goes() {
    // Its window was closed: hand control is off
    rules::HandSwitch hand;
    hand.toggle(0, false);
    hand.update(10, true);
    bool closed = hand.update(20, false) == Do::Nothing && !hand.wanted() &&
                  hand.link(true, false, 20) == rules::HandLink::Off;
    test::check(closed, "hand control: closing the camera window switches it off");

    // Started by hand, not by the game: it counts as on, and H stops it
    rules::HandSwitch manual;
    bool adopted = manual.update(0, true) == Do::Nothing && manual.wanted();
    bool stops = manual.toggle(5, true) == Do::Quit;
    test::check(adopted && stops, "hand control: a script started by hand is taken over");
  }

  void background_command() {
    std::string command = rules::background_command("\"a b/run.bat\" --flag");
#ifdef _WIN32
    bool ok = command ==
              "start \"Tetris++ hand control\" /min cmd /c \"\"a b/run.bat\" --flag\"";
#else
    bool ok = command == "\"a b/run.bat\" --flag &";
#endif
    test::check(ok, "the camera script is started in the background");
  }

  // Follow mode: the camera script reports where the hand is in its zone
  void gesture_pointer() {
    GestureSource g;
    g.open(kPort);
    bool none = !g.pointer_live() && !g.pointer_seen();
    std::vector<Action> out = receive(g, "X250Y999H");
    bool placed = g.pointer_live() && g.pointer_seen() && test::near(g.pointer_x(), 250 / 999.0, 1e-6) &&
                  test::near(g.pointer_y(), 1.0, 1e-6);
    bool with_drop = out.size() == 1 && out[0] == Action::HardDrop;
    receive(g, "X5000");  // out of range: the edge
    bool clamped = test::near(g.pointer_x(), 1.0, 1e-6) && test::near(g.pointer_y(), 1.0, 1e-6);
    receive(g, "XY");     // no numbers: nothing changes
    bool kept = test::near(g.pointer_x(), 1.0, 1e-6);
    test::sleep_ms(350);
    bool gone = !g.pointer_live() && g.pointer_seen();
    test::check(none && placed && with_drop, "the hand's position arrives with the drop");
    test::check(clamped && kept, "a pointer outside the zone stops at its edge");
    test::check(gone, "the pointer goes when the hand leaves the picture");
  }

  void column_tracker() {
    ColumnTracker tracker;
    bool first = tracker.column() == -1 && tracker.update(0.05f) == 0;
    bool middle = tracker.update(0.55f) == 5;
    // 0.6 is the border between columns 5 and 6
    bool holds = tracker.update(0.61f) == 5 && tracker.update(0.59f) == 5 &&
                 tracker.update(0.62f) == 5;
    bool moves = tracker.update(0.64f) == 6;
    bool back = tracker.update(0.58f) == 6 && tracker.update(0.56f) == 5;
    bool edges = tracker.update(1.0f) == 9 && tracker.update(-0.2f) == 0 &&
                 tracker.update(1.7f) == 9;
    tracker.reset();
    bool fresh = tracker.column() == -1 && tracker.update(0.61f) == 6;
    test::check(first && middle && moves && edges, "the hand's position picks the column");
    test::check(holds && back && fresh, "a hand on the border between two columns doesn't flicker");
  }

  void hand_follow_and_keys() {
    rules::HandFollow follow;
    bool steers = follow.column(4, false) == 4 && follow.column(5, false) == 5;
    // an arrow key moves the piece: the hand (still over column 5) lets it be
    bool keys_win = follow.column(5, true) == -1 && follow.column(5, false) == -1;
    // the hand moves to another column: it steers again
    bool hand_back = follow.column(6, false) == 6 && follow.column(6, false) == 6;
    bool no_hand = follow.column(-1, false) == -1 && follow.column(-1, true) == -1;
    bool fresh = follow.column(5, false) == 5;
    test::check(steers && no_hand && fresh, "the piece goes to the column under the hand");
    test::check(keys_win && hand_back,
                "after an arrow key the piece stays put until the hand moves on");
  }

  void hand_lost_pause() {
    rules::HandLostPause lost;
    bool keyboard_game = !lost.update(0, false, false) && !lost.update(60, false, false);
    lost.update(61, true, false);  // a hand plays
    bool short_gap = !lost.update(61.5, false, false) && !lost.update(62.4, false, false) &&
                     !lost.update(62.5, true, false) && !lost.update(63.9, false, false);
    bool pauses = lost.update(63.9 + rules::kHandLostDelay, false, false);
    bool once = !lost.update(70, false, false);
    test::check(keyboard_game && short_gap, "a short gap in hand tracking doesn't pause");
    test::check(pauses && once, "the game pauses when the hand has left for a while");

    rules::HandLostPause keys;
    keys.update(0, true, false);
    bool taken_over = !keys.update(0.5, false, true) && !keys.update(5, false, false);
    test::check(taken_over, "going on with the keyboard doesn't pause");
    keys.update(6, true, false);
    keys.update(6.1, false, false);
    keys.reset();  // a new game or a resume
    test::check(!keys.update(20, false, false), "a new game forgets the old hand");
  }

  // In menus the hand is a cursor: it focuses the line it points at, and a
  // click selects a plain line or cycles a value
  void menu_hand_cursor() {
    ui::Menu menu({"level", "start", "saved", "back"});
    menu.values = {"5", "", "", ""};
    menu.disabled = {false, false, true, false};
    // drawn at (100, 200), 300 wide, 40 per line: line i spans y 196+40i .. 236+40i
    bool hits = menu.row_at(150, 200, 100, 200, 300, 40) == 0 &&
                menu.row_at(150, 275, 100, 200, 300, 40) == 1 &&
                menu.row_at(395, 355, 100, 200, 300, 40) == 3;
    bool misses = menu.row_at(150, 190, 100, 200, 300, 40) == -1 &&
                  menu.row_at(150, 400, 100, 200, 300, 40) == -1 &&
                  menu.row_at(50, 220, 100, 200, 300, 40) == -1 &&
                  menu.row_at(420, 220, 100, 200, 300, 40) == -1;
    test::check(hits && misses, "the hand points at a menu line");

    bool moved = menu.point_at({150, 250}, 100, 200, 300, 40) && menu.focus == 1;
    bool same = !menu.point_at({160, 260}, 100, 200, 300, 40);
    bool skips_disabled = !menu.point_at({150, 300}, 100, 200, 300, 40) && menu.focus == 1;
    bool outside = !menu.point_at({10, 10}, 100, 200, 300, 40) && menu.focus == 1;
    test::check(moved && same && skips_disabled && outside,
                "the line under the hand is focused (not a disabled one)");

    int chosen = menu.handle(acts({Action::Click}));
    menu.focus = 0;
    int on_value = menu.handle(acts({Action::Click}));
    bool cycles = on_value == -1 && menu.changed_row == 0 && menu.change == 1 && menu.focus == 0;
    test::check(chosen == 1 && cycles, "a hand click selects a line or changes a setting");
  }

  void hand_clicks_in_menus() {
    // keyboard: up, space; then gestures: pinch, fist, a swipe
    std::vector<Action> in = {Action::Rotate, Action::HardDrop, Action::Rotate, Action::HardDrop,
                              Action::Left};
    std::vector<Action> live = rules::hand_clicks(in, 2, true);
    std::vector<Action> want = {Action::Rotate, Action::HardDrop, Action::Click, Action::Click,
                                Action::Left};
    test::check(live == want, "in menus the hand's pinch and fist click, the keys don't");
    test::check(rules::hand_clicks(in, 2, false) == in,
                "without a hand in view nothing turns into a click");
  }

  void focus_pause_rule() {
    bool keyboard = rules::pause_on_focus_loss(true, false, true, false);
    bool hand = rules::pause_on_focus_loss(true, false, true, true);
    bool in_menu = rules::pause_on_focus_loss(true, false, false, false);
    bool still_focused = rules::pause_on_focus_loss(true, true, true, false);
    bool stays_away = rules::pause_on_focus_loss(false, false, true, false);
    test::check(keyboard && !in_menu && !still_focused && !stays_away,
                "losing the window focus pauses a keyboard game once");
    test::check(!hand, "losing the window focus doesn't pause while a hand plays");
  }
};

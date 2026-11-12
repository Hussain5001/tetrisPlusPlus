#pragma once
#include <string>

// Small decisions the App makes, kept free of raylib so the tests can check
// them without a window.
namespace rules {

// The game pauses itself when its window loses focus, unless a hand is
// playing: the camera preview window takes the focus when it opens, and
// gestures reach the game whether it has the focus or not.
inline bool pause_on_focus_loss(bool was_focused, bool focused, bool playing,
                                bool hand_connected) {
  return was_focused && !focused && playing && !hand_connected;
}

// How long the camera script may take to show up after it was started. The
// very first start installs Python packages, which takes a minute or two.
const double kHandStartTimeout = 180;

// What the hand control status shows
enum class HandLink { PortBusy, Off, Starting, NoAnswer, On };

// Decides when the camera script (the "sidecar") is started and stopped, so
// the player can switch between hand and keyboard inside the game. Pure
// logic: the App reports what happened and carries out what comes back.
class HandSwitch {
 public:
  enum class Do { Nothing, Launch, Quit };

  // The player asked to switch hand control on or off (the H key, the menu,
  // or the saved choice at start-up). now: seconds; sidecar: a camera script
  // is talking to the game right now.
  Do toggle(double now, bool sidecar) {
    if (wanted_) {
      wanted_ = false;
      // One that is still starting is stopped by update() when it shows up
      return sidecar ? Do::Quit : Do::Nothing;
    }
    wanted_ = true;
    if (sidecar) return Do::Nothing;    // one is already there
    if (starting(now)) return Do::Nothing;  // one is on its way: don't start a second
    launched_at_ = now;
    return Do::Launch;
  }

  // Call every frame.
  Do update(double now, bool sidecar) {
    if (sidecar) {
      bool ours = launched_at_ >= 0;
      launched_at_ = -1;
      if (!wanted_) {
        if (ours) return Do::Quit;  // it was switched off while still starting
        wanted_ = true;             // the player started the script by hand
      }
    } else if (wanted_ && launched_at_ < 0) {
      wanted_ = false;  // the script has closed (its window was closed)
    }
    (void)now;
    return Do::Nothing;
  }

  // The script could not be started: hand control stays off.
  void launch_failed() {
    wanted_ = false;
    launched_at_ = -1;
  }

  // True if hand control is switched on (what the profile remembers)
  bool wanted() const { return wanted_; }

  HandLink link(bool port_open, bool sidecar, double now) const {
    if (!port_open) return HandLink::PortBusy;
    if (sidecar) return HandLink::On;
    if (!wanted_ || launched_at_ < 0) return HandLink::Off;
    return starting(now) ? HandLink::Starting : HandLink::NoAnswer;
  }

 private:
  bool wanted_ = false;
  double launched_at_ = -1;  // when a script was started that hasn't shown up yet

  bool starting(double now) const {
    return launched_at_ >= 0 && now - launched_at_ < kHandStartTimeout;
  }
};

// Hand and keyboard share the piece. The piece goes to the column under
// the hand, but when the player moves it with the arrow keys the keys win:
// the piece stays where they put it until the hand moves to another column.
class HandFollow {
 public:
  // hand_column: the column under the hand, or -1 if no hand is in view.
  // key_moved: the left/right keys moved the piece this frame. Returns the
  // column the piece should go to, or -1 to leave it where it is.
  int column(int hand_column, bool key_moved) {
    if (hand_column < 0) {
      hold_ = -1;
      return -1;
    }
    if (key_moved) hold_ = hand_column;
    if (hold_ >= 0) {
      if (hand_column == hold_) return -1;
      hold_ = -1;  // the hand has moved on: it steers again
    }
    return hand_column;
  }

 private:
  int hold_ = -1;  // the hand's column when the keys took over
};

// Seconds the hand may be out of the camera's view before the game pauses
const double kHandLostDelay = 1.5;

// A game played by hand pauses by itself when the hand has gone, so a lost
// hand (or a tracking hiccup) costs a pause instead of the game. If the
// player goes on with the keyboard instead, nothing pauses.
class HandLostPause {
 public:
  void reset() {
    had_hand_ = false;
    lost_since_ = -1;
  }

  // Call every frame of play. Returns true when the game should pause.
  bool update(double now, bool hand_in_view, bool key_used) {
    if (hand_in_view) {
      had_hand_ = true;
      lost_since_ = -1;
      return false;
    }
    if (!had_hand_) return false;  // a keyboard game
    if (key_used) {
      reset();  // the player took over with the keys
      return false;
    }
    if (lost_since_ < 0) lost_since_ = now;
    if (now - lost_since_ < kHandLostDelay) return false;
    reset();
    return true;
  }

 private:
  bool had_hand_ = false;
  double lost_since_ = -1;
};

// The shell command that runs `command` in the background: in its own
// minimised window on Windows.
inline std::string background_command(const std::string& command) {
#ifdef _WIN32
  return "start \"Tetris++ hand control\" /min cmd /c \"" + command + "\"";
#else
  return command + " &";
#endif
}

}  // namespace rules

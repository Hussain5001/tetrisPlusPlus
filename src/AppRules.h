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

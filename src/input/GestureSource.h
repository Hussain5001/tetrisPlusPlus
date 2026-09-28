#pragma once
#include <string>
#include <vector>

#include "Action.h"

// Receives commands from the hand-tracking sidecar (gesture/hand_control.py)
// over a local UDP socket. The socket is non-blocking and drained once per
// frame, so gestures reach the game within one frame of arriving.
//
// Protocol: each datagram holds one or more single-letter commands
//   L = left, R = right, D = soft drop, H = hard drop, U = rotate,
//   P = pause, C = confirm, B = back, K = keep-alive (no action)
//   @N = position mode: move the falling piece towards board column N
//
// Note: this file deliberately does not include raylib.h, because raylib's
// names clash with <windows.h> / <winsock2.h> on Windows.
class GestureSource {
 public:
  GestureSource() = default;
  ~GestureSource();
  GestureSource(const GestureSource&) = delete;
  GestureSource& operator=(const GestureSource&) = delete;

  // Opens the socket on 127.0.0.1:port. Returns false if the port is taken.
  bool open(int port);

  // Appends every action received since the last call to `out`.
  void poll(std::vector<Action>& out);

  // True if the sidecar has sent anything (including keep-alives) recently.
  bool connected() const;

  bool is_open() const { return sock_ >= 0; }

  // Seconds since the last real (non keep-alive) gesture, or a large number.
  double seconds_since_gesture() const;

  Action last_action() const { return last_action_; }

  // Column the player's hand points at in position mode, or -1 if the
  // sidecar is not in position mode / the hand left the camera
  int target_column() const;

 private:
  long long sock_ = -1;  // SOCKET on Windows, int elsewhere
  double last_packet_ = -1e9;
  double last_gesture_ = -1e9;
  Action last_action_ = Action::Left;
  int target_column_ = -1;
  double last_target_ = -1e9;
  static double now();
};

#pragma once
#include <string>
#include <vector>

#include "Action.h"

// Receives commands from the hand-tracking sidecar (gesture/hand_control.py)
// over a local UDP socket. The socket is non-blocking and drained once per
// frame, so gestures reach the game within one frame of arriving.
//
// Protocol, sidecar -> game: each datagram holds one or more single-letter
// commands
//   L = left, R = right, D = soft drop, H = hard drop, U = rotate,
//   P = pause, C = confirm, B = back, K = keep-alive (no action)
//   @N = position mode: move the falling piece towards board column N
// Game -> sidecar (sent to whoever sends keep-alives):
//   k = "the game is here", the answer to every K
//   Q = quit: close the camera and exit
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

  // Appends every action received since the last call to `out`, and answers
  // the sidecar's keep-alives.
  void poll(std::vector<Action>& out);

  // True if anything (including keep-alives) has arrived recently.
  bool connected() const;

  bool is_open() const { return sock_ >= 0; }

  // True while a sidecar is sending keep-alives (they stop when it closes),
  // until quit_sidecar().
  bool has_sidecar() const;

  // Tells the sidecar to close ("Q") and forgets it, so connected() is false
  // straight away. What it still sends in the next two seconds is ignored
  // and answered with another "Q". Returns false if no sidecar is known.
  bool quit_sidecar();

  // Seconds since the last real (non keep-alive) gesture, or a large number.
  double seconds_since_gesture() const;

  Action last_action() const { return last_action_; }

  // Sends `data` to 127.0.0.1:port over UDP (used by the tests)
  static bool send_to_local(int port, const std::string& data);

  // Column the player's hand points at in position mode, or -1 if the
  // sidecar is not in position mode / the hand left the camera
  int target_column() const;

 private:
  long long sock_ = -1;  // SOCKET on Windows, int elsewhere
  double last_packet_ = -1e9;
  double last_keep_alive_ = -1e9;
  double last_gesture_ = -1e9;
  Action last_action_ = Action::Left;
  int target_column_ = -1;
  double last_target_ = -1e9;
  // Socket addresses (sockaddr_in), kept as bytes so this header needs no
  // network includes: the sidecar, and the one that was just told to quit
  unsigned char peer_[32] = {};
  int peer_len_ = 0;
  unsigned char quit_peer_[32] = {};
  int quit_peer_len_ = 0;
  double mute_until_ = -1e9;
  static double now();
};

// A plain UDP socket for the tests, standing in for the sidecar: it sends to
// the game's port and reads what the game sends back.
class UdpProbe {
 public:
  UdpProbe();
  ~UdpProbe();
  UdpProbe(const UdpProbe&) = delete;
  UdpProbe& operator=(const UdpProbe&) = delete;

  bool send(int port, const std::string& data);

  // Waits up to wait_ms for a datagram and returns its text ("" if none came)
  std::string receive(int wait_ms = 300);

 private:
  long long sock_ = -1;
};

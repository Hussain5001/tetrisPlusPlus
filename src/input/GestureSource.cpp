#include "GestureSource.h"

#include <chrono>
#include <cstring>
#include <iostream>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
typedef int socklen_t;
#else
#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace {
#ifdef _WIN32
typedef SOCKET Socket;
#else
typedef int Socket;
#endif

// Opens a UDP socket; returns -1 if that fails. Programs the game starts
// (the camera script) don't inherit it: an inherited socket would keep the
// port taken after the game has closed, and the next start would find it busy.
long long new_udp_socket() {
#ifdef _WIN32
  WSADATA wsa;
  if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) return -1;
  SOCKET s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (s == INVALID_SOCKET) {
    WSACleanup();
    return -1;
  }
  SetHandleInformation((HANDLE)s, HANDLE_FLAG_INHERIT, 0);
  return (long long)s;
#else
  int s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (s >= 0) fcntl(s, F_SETFD, FD_CLOEXEC);
  return s;
#endif
}

void close_udp_socket(long long s) {
  if (s < 0) return;
#ifdef _WIN32
  closesocket((SOCKET)s);
  WSACleanup();
#else
  close((int)s);
#endif
}

sockaddr_in local_address(int port) {
  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_port = htons((unsigned short)port);
  addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  return addr;
}

bool send_bytes(long long s, const std::string& data, const sockaddr* to, int to_len) {
  return sendto((Socket)s, data.data(), (int)data.size(), 0, to, to_len) ==
         (long)data.size();
}
}  // namespace

double GestureSource::now() {
  using namespace std::chrono;
  return duration<double>(steady_clock::now().time_since_epoch()).count();
}

GestureSource::~GestureSource() { close_udp_socket(sock_); }

bool GestureSource::open(int port) {
  long long s = new_udp_socket();
  if (s < 0) return false;
#ifdef _WIN32
  u_long non_blocking = 1;
  ioctlsocket((SOCKET)s, FIONBIO, &non_blocking);
#else
  fcntl((int)s, F_SETFL, fcntl((int)s, F_GETFL, 0) | O_NONBLOCK);
#endif

  sockaddr_in addr = local_address(port);
  if (bind((Socket)s, (sockaddr*)&addr, sizeof(addr)) != 0) {
    std::cerr << "Gesture input: could not bind UDP port " << port << std::endl;
    close_udp_socket(s);
    return false;
  }
  sock_ = s;
  std::cout << "Gesture input: listening on udp://127.0.0.1:" << port << std::endl;
  return true;
}

void GestureSource::poll(std::vector<Action>& out) {
  if (sock_ < 0) return;
  char buf[256];
  while (true) {
    sockaddr_in from{};
    socklen_t from_len = sizeof(from);
#ifdef _WIN32
    int n = recvfrom((SOCKET)sock_, buf, sizeof(buf), 0, (sockaddr*)&from, &from_len);
#else
    long n = recvfrom((int)sock_, buf, sizeof(buf), 0, (sockaddr*)&from, &from_len);
#endif
    if (n <= 0) break;  // nothing left (EWOULDBLOCK) or error
    double t = now();

    // A sidecar that was told to quit may still send for a moment: tell it
    // again and don't let it move anything
    if (t < mute_until_ && from_len == quit_peer_len_ &&
        std::memcmp(&from, quit_peer_, (size_t)from_len) == 0) {
      send_bytes(sock_, "Q", (sockaddr*)&from, from_len);
      continue;
    }

    last_packet_ = t;
    bool keep_alive = false;
    for (long i = 0; i < n; i++) {
      if (buf[i] == '@') {
        int column = 0;
        bool has_digit = false;
        while (i + 1 < n && buf[i + 1] >= '0' && buf[i + 1] <= '9') {
          column = column * 10 + (buf[++i] - '0');
          has_digit = true;
        }
        if (has_digit) {
          target_column_ = column;
          last_target_ = last_packet_;
        }
        continue;
      }
      if (buf[i] == 'X' || buf[i] == 'Y') {
        float& axis = buf[i] == 'X' ? pointer_x_ : pointer_y_;
        int value = 0;
        bool has_digit = false;
        while (i + 1 < n && buf[i + 1] >= '0' && buf[i + 1] <= '9') {
          if (value < 100000) value = value * 10 + (buf[i + 1] - '0');
          i++;
          has_digit = true;
        }
        if (has_digit) {
          axis = (value > 999 ? 999 : value) / 999.0f;
          last_pointer_ = last_packet_;
        }
        continue;
      }
      Action a;
      switch (buf[i]) {
        case 'L': a = Action::Left; break;
        case 'R': a = Action::Right; break;
        case 'D': a = Action::SoftDrop; break;
        case 'H': a = Action::HardDrop; break;
        case 'U': a = Action::Rotate; break;
        case 'P': a = Action::Pause; break;
        case 'C': a = Action::Confirm; break;
        case 'B': a = Action::Back; break;
        case 'K': keep_alive = true; continue;
        default: continue;  // whitespace, unknown
      }
      out.push_back(a);
      last_action_ = a;
      last_gesture_ = last_packet_;
    }

    // Only the sidecar sends keep-alives: remember where it is and answer,
    // so it knows the game is still running
    if (keep_alive && from_len <= (socklen_t)sizeof(peer_)) {
      std::memcpy(peer_, &from, (size_t)from_len);
      peer_len_ = (int)from_len;
      last_keep_alive_ = t;
      send_bytes(sock_, "k", (sockaddr*)&from, from_len);
    }
  }
}

bool GestureSource::quit_sidecar() {
  if (sock_ < 0 || peer_len_ == 0) return false;
  bool sent = send_bytes(sock_, "Q", (sockaddr*)peer_, peer_len_);
  std::memcpy(quit_peer_, peer_, sizeof(peer_));
  quit_peer_len_ = peer_len_;
  mute_until_ = now() + 2.0;
  peer_len_ = 0;
  last_packet_ = -1e9;
  last_target_ = -1e9;
  last_pointer_ = -1e9;
  return sent;
}

bool GestureSource::send_to_local(int port, const std::string& data) {
  long long s = new_udp_socket();
  if (s < 0) return false;
  sockaddr_in addr = local_address(port);
  bool ok = send_bytes(s, data, (sockaddr*)&addr, sizeof(addr));
  close_udp_socket(s);
  return ok;
}

bool GestureSource::connected() const { return now() - last_packet_ < 3.0; }

bool GestureSource::inherited_by_children() const {
  if (sock_ < 0) return false;
#ifdef _WIN32
  DWORD flags = 0;
  if (!GetHandleInformation((HANDLE)sock_, &flags)) return false;
  return (flags & HANDLE_FLAG_INHERIT) != 0;
#else
  return (fcntl((int)sock_, F_GETFD) & FD_CLOEXEC) == 0;
#endif
}

bool GestureSource::has_sidecar() const {
  return peer_len_ > 0 && now() - last_keep_alive_ < 3.0;
}

int GestureSource::target_column() const {
  return now() - last_target_ < 0.3 ? target_column_ : -1;
}

bool GestureSource::pointer_live() const { return now() - last_pointer_ < 0.3; }

double GestureSource::seconds_since_gesture() const {
  return now() - last_gesture_;
}

// ------------------------------------------------------------------ UdpProbe

UdpProbe::UdpProbe() : sock_(new_udp_socket()) {}

UdpProbe::~UdpProbe() { close_udp_socket(sock_); }

bool UdpProbe::send(int port, const std::string& data) {
  if (sock_ < 0) return false;
  sockaddr_in addr = local_address(port);
  return send_bytes(sock_, data, (sockaddr*)&addr, sizeof(addr));
}

std::string UdpProbe::receive(int wait_ms) {
  if (sock_ < 0) return "";
  fd_set readable;
  FD_ZERO(&readable);
  FD_SET((Socket)sock_, &readable);
  timeval timeout;
  timeout.tv_sec = wait_ms / 1000;
  timeout.tv_usec = (wait_ms % 1000) * 1000;
  if (select((int)sock_ + 1, &readable, nullptr, nullptr, &timeout) <= 0) return "";
  char buf[256];
#ifdef _WIN32
  int n = recv((SOCKET)sock_, buf, sizeof(buf), 0);
#else
  long n = recv((int)sock_, buf, sizeof(buf), 0);
#endif
  return n > 0 ? std::string(buf, (size_t)n) : "";
}

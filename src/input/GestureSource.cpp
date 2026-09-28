#include "GestureSource.h"

#include <chrono>
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
#include <sys/socket.h>
#include <unistd.h>
#endif

double GestureSource::now() {
  using namespace std::chrono;
  return duration<double>(steady_clock::now().time_since_epoch()).count();
}

GestureSource::~GestureSource() {
  if (sock_ < 0) return;
#ifdef _WIN32
  closesocket((SOCKET)sock_);
  WSACleanup();
#else
  close((int)sock_);
#endif
}

bool GestureSource::open(int port) {
#ifdef _WIN32
  WSADATA wsa;
  if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) return false;
  SOCKET s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (s == INVALID_SOCKET) return false;
  u_long non_blocking = 1;
  ioctlsocket(s, FIONBIO, &non_blocking);
#else
  int s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (s < 0) return false;
  fcntl(s, F_SETFL, fcntl(s, F_GETFL, 0) | O_NONBLOCK);
#endif

  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_port = htons((unsigned short)port);
  addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  if (bind(s, (sockaddr*)&addr, sizeof(addr)) != 0) {
    std::cerr << "Gesture input: could not bind UDP port " << port << std::endl;
#ifdef _WIN32
    closesocket(s);
    WSACleanup();
#else
    close(s);
#endif
    return false;
  }
  sock_ = (long long)s;
  std::cout << "Gesture input: listening on udp://127.0.0.1:" << port
            << std::endl;
  return true;
}

void GestureSource::poll(std::vector<Action>& out) {
  if (sock_ < 0) return;
  char buf[256];
  while (true) {
#ifdef _WIN32
    int n = recv((SOCKET)sock_, buf, sizeof(buf), 0);
#else
    long n = recv((int)sock_, buf, sizeof(buf), 0);
#endif
    if (n <= 0) break;  // nothing left (EWOULDBLOCK) or error
    last_packet_ = now();
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
        default: continue;  // 'K' keep-alive, whitespace, unknown
      }
      out.push_back(a);
      last_action_ = a;
      last_gesture_ = last_packet_;
    }
  }
}

bool GestureSource::connected() const { return now() - last_packet_ < 3.0; }

int GestureSource::target_column() const {
  return now() - last_target_ < 0.3 ? target_column_ : -1;
}

double GestureSource::seconds_since_gesture() const {
  return now() - last_gesture_;
}

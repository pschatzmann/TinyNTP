// The Arduino UDP API on top of a POSIX socket (Linux, macOS), IPv4 only.
// Used by the command line tool and by the tests.
#pragma once
#include <arpa/inet.h>
#include <netdb.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstdint>
#include <cstdio>
#include <cstring>

/// IPv4 address of a sender
struct IPv4 {
  uint32_t addr = 0;
};

/// The Arduino UDP API on top of a POSIX socket
class PosixUDP {
 public:
  /// Print error messages (e.g. if the port cannot be used)
  static inline bool debug = false;

  ~PosixUDP() { stop(); }

  uint8_t begin(uint16_t port) {
    stop();
    fd_ = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd_ < 0) return 0;
    int yes = 1;
    setsockopt(fd_, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof yes);
    sockaddr_in a = {};
    a.sin_family = AF_INET;
    a.sin_port = htons(port);
    a.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(fd_, reinterpret_cast<sockaddr*>(&a), sizeof a) < 0) {
      if (debug) perror("bind");
      stop();
      return 0;
    }
    return 1;
  }

  void stop() {
    if (fd_ >= 0) close(fd_);
    fd_ = -1;
  }

  int parsePacket() {
    if (fd_ < 0) return 0;
    pollfd p = {fd_, POLLIN, 0};
    if (poll(&p, 1, 10) <= 0) return 0;  // wait up to 10 ms: no busy loop
    socklen_t len = sizeof remote_;
    int n = recvfrom(fd_, in_, sizeof in_, MSG_DONTWAIT,
                     reinterpret_cast<sockaddr*>(&remote_), &len);
    if (n <= 0) return 0;
    inLen_ = n;
    inPos_ = 0;
    return n;
  }

  int available() { return inLen_ - inPos_; }

  int read(uint8_t* buffer, int len) {
    int n = available() < len ? available() : len;
    memcpy(buffer, in_ + inPos_, n);
    inPos_ += n;
    return n;
  }

  IPv4 remoteIP() {
    IPv4 ip;
    ip.addr = remote_.sin_addr.s_addr;
    return ip;
  }
  uint16_t remotePort() { return ntohs(remote_.sin_port); }

  int beginPacket(const char* host, uint16_t port) {
    addrinfo hints = {};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    addrinfo* res = nullptr;
    if (getaddrinfo(host, nullptr, &hints, &res) != 0 || res == nullptr) {
      return 0;
    }
    dest_ = *reinterpret_cast<sockaddr_in*>(res->ai_addr);
    freeaddrinfo(res);
    dest_.sin_port = htons(port);
    outLen_ = 0;
    return 1;
  }

  int beginPacket(IPv4 ip, uint16_t port) {
    dest_ = {};
    dest_.sin_family = AF_INET;
    dest_.sin_port = htons(port);
    dest_.sin_addr.s_addr = ip.addr;
    outLen_ = 0;
    return 1;
  }

  size_t write(const uint8_t* buffer, size_t len) {
    if (outLen_ + len > sizeof out_) return 0;
    memcpy(out_ + outLen_, buffer, len);
    outLen_ += len;
    return len;
  }

  int endPacket() {
    return sendto(fd_, out_, outLen_, 0, reinterpret_cast<sockaddr*>(&dest_),
                  sizeof dest_) == static_cast<ssize_t>(outLen_);
  }

 protected:
  int fd_ = -1;
  uint8_t in_[512];
  uint8_t out_[512];
  size_t outLen_ = 0;
  int inLen_ = 0, inPos_ = 0;
  sockaddr_in remote_ = {}, dest_ = {};
};

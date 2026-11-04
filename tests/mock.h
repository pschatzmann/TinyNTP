// A fake Arduino environment for the tests: millis() that you control, Serial
// and a UDP class that works on in-memory packets, so that tests are fast and
// deterministic.
#pragma once
#include <cstdint>
#include <deque>
#include <functional>
#include <utility>
#include <string>
#include <vector>

namespace mock {
/// The value of millis(). Starts at 1000: the library uses 0 as "not set"
inline uint32_t now = 1000;
/// Added to millis(): millis() returns an unsigned long, which has 64 bit on a
/// computer (set to 1 << 32 to simulate an uptime of more than 49 days)
inline uint64_t high = 0;
/// Everything that was printed to Serial
inline std::string serialOutput;
inline void reset() {
  now = 1000;
  high = 0;
  serialOutput.clear();
}
}  // namespace mock

inline unsigned long millis() {
  return static_cast<unsigned long>(mock::now + mock::high);
}

/// micros(): follows millis(), plus mock::microsExtra
namespace mock {
inline uint32_t microsExtra = 0;
}
inline unsigned long micros() {
  return static_cast<unsigned long>(static_cast<uint32_t>(mock::now * 1000UL + mock::microsExtra));
}

struct SerialMock {
  void print(const char* text) { mock::serialOutput += text; }
  void println(const char* text) {
    mock::serialOutput += text;
    mock::serialOutput += "\n";
  }
};
inline SerialMock Serial;

/// IP address of a sender
struct MockIP {
  uint32_t addr = 0;
  bool operator==(const MockIP& o) const { return addr == o.addr; }
  /// The bytes of the IPv4 address as IPAddress[]: addr is little endian
  /// (192.168.0.10 = 0x0a00a8c0)
  uint8_t operator[](int i) const { return static_cast<uint8_t>(addr >> (8 * i)); }
};

using Bytes = std::vector<uint8_t>;

/// UDP class with the Arduino API. The state is static: call reset() first.
class MockUDP {
 public:
  struct Incoming {
    Bytes data;
    MockIP ip;
    uint16_t port;
  };
  struct Sent {
    Bytes data;
    std::string host;  // set if the packet was sent to a host name
    MockIP ip;
    uint16_t port;
  };

  /// Packets that the next parsePacket() calls return
  static inline std::deque<Incoming> inbox;
  /// All packets that were sent
  static inline std::vector<Sent> sent;
  /// Ports of the begin() calls
  static inline std::vector<uint16_t> boundPorts;
  /// Number of stop() calls
  static inline int stopCount = 0;
  /// Called for each sent packet: returns the answer or an empty vector
  static inline std::function<Bytes(const Bytes&)> responder;
  /// millis() advances by this value when parsePacket() has nothing to return
  static inline uint32_t idleStep = 0;
  /// begin() returns this value
  static inline uint8_t beginResult = 1;
  /// An answer of the responder arrives after this number of parsePacket()
  /// calls that return 0 (the network needs some time)
  static inline int delayPolls = 0;
  static inline std::deque<std::pair<int, Incoming>> delayed;

  static void reset() {
    inbox.clear();
    sent.clear();
    boundPorts.clear();
    stopCount = 0;
    responder = nullptr;
    idleStep = 0;
    beginResult = 1;
    delayPolls = 0;
    delayed.clear();
  }

  /// Put a packet into the receive queue
  static void deliver(const Bytes& data, uint32_t ip = 0x0100007f,
                      uint16_t port = 40000) {
    inbox.push_back({data, MockIP{ip}, port});
  }

  uint8_t begin(uint16_t port) {
    boundPorts.push_back(port);
    return beginResult;
  }
  void stop() { stopCount++; }

  int parsePacket() {
    if (inbox.empty()) {
      mock::now += idleStep;
      for (auto& d : delayed) d.first--;
      while (!delayed.empty() && delayed.front().first <= 0) {
        inbox.push_back(delayed.front().second);
        delayed.pop_front();
      }
      return 0;
    }
    current_ = inbox.front();
    inbox.pop_front();
    pos_ = 0;
    return static_cast<int>(current_.data.size());
  }
  int available() { return static_cast<int>(current_.data.size()) - pos_; }
  int read(uint8_t* buffer, int len) {
    int n = available() < len ? available() : len;
    for (int i = 0; i < n; i++) buffer[i] = current_.data[pos_ + i];
    pos_ += n;
    return n;
  }
  MockIP remoteIP() { return current_.ip; }
  uint16_t remotePort() { return current_.port; }

  int beginPacket(const char* host, uint16_t port) {
    out_ = {};
    out_.host = host;
    out_.port = port;
    return 1;
  }
  int beginPacket(MockIP ip, uint16_t port) {
    out_ = {};
    out_.ip = ip;
    out_.port = port;
    return 1;
  }
  size_t write(const uint8_t* buffer, size_t len) {
    out_.data.insert(out_.data.end(), buffer, buffer + len);
    return len;
  }
  int endPacket() {
    sent.push_back(out_);
    if (responder) {
      Bytes answer = responder(out_.data);
      if (!answer.empty()) {
        Incoming in{answer, MockIP{0x0100007f}, out_.port};
        if (delayPolls > 0) {
          delayed.push_back({delayPolls, in});
        } else {
          inbox.push_back(in);
        }
      }
    }
    return 1;
  }

 protected:
  Incoming current_;
  int pos_ = 0;
  Sent out_;
};

// ---- helpers to build and read NTP packets --------------------------------
constexpr uint64_t NtpEpoch = 2208988800ULL;

inline void put32(Bytes& b, size_t at, uint32_t v) {
  for (int i = 0; i < 4; i++) b[at + i] = v >> (24 - 8 * i);
}
inline uint32_t get32(const Bytes& b, size_t at) {
  return (uint32_t(b[at]) << 24) | (uint32_t(b[at + 1]) << 16) |
         (uint32_t(b[at + 2]) << 8) | b[at + 3];
}
/// Write a Unix time in ms as NTP timestamp
inline void putTimestamp(Bytes& b, size_t at, uint64_t unixMs) {
  put32(b, at, static_cast<uint32_t>(unixMs / 1000 + NtpEpoch));
  put32(b, at + 4, static_cast<uint32_t>((unixMs % 1000) * 4294967296ULL / 1000));
}
/// Read an NTP timestamp as Unix time in ms
inline uint64_t getTimestamp(const Bytes& b, size_t at) {
  uint64_t sec = get32(b, at) - NtpEpoch;
  // rounded to the nearest ms
  return sec * 1000 +
         ((static_cast<uint64_t>(get32(b, at + 4)) * 1000 + (1ULL << 31)) >> 32);
}
/// A request of an NTP client (mode 3, version 4)
inline Bytes ntpRequest(uint64_t clientTimeMs = 1800000000000ULL, int version = 4) {
  Bytes b(48, 0);
  b[0] = (version << 3) | 3;
  b[2] = 6;  // poll
  putTimestamp(b, 40, clientTimeMs);
  return b;
}

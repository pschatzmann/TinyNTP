// Client and server talk over real UDP sockets on the loopback interface
#include <unistd.h>

#include <atomic>
#include <chrono>
#include <thread>

#include "test.h"

unsigned long millis() {
  using namespace std::chrono;
  return static_cast<uint32_t>(
      duration_cast<milliseconds>(steady_clock::now().time_since_epoch())
          .count());
}
struct SerialPort {
  void print(const char*) {}
} Serial;

#include "../tools/tinyntp/PosixUDP.h"
#include "TinyNTP/TinyClockTimeSource.h"
#include "TinyNTP/TinyNTPClient.h"
#include "TinyNTP/TinyNTPServer.h"
#include "TinyNTP/TinySystemTimeSource.h"

using namespace tiny_ntp;

static uint16_t basePort() { return 20000 + getpid() % 10000; }

static int64_t systemMs() {
  using namespace std::chrono;
  return duration_cast<milliseconds>(system_clock::now().time_since_epoch())
      .count();
}

/// Runs process() of a server in a thread
struct ServerThread {
  TinyNTPServer<PosixUDP>& server;
  std::atomic<bool> run{true};
  std::thread thread;
  explicit ServerThread(TinyNTPServer<PosixUDP>& s) : server(s) {
    thread = std::thread([this] {
      while (run) server.process();
    });
  }
  ~ServerThread() {
    run = false;
    thread.join();
  }
};

TEST(client_gets_the_time_from_a_server) {
  uint16_t port = basePort();
  TinySystemTimeSource source;
  TinyNTPServer<PosixUDP> server;
  CHECK(server.begin(source, port));
  ServerThread t(server);
  TinyNTPClient<PosixUDP> ntp("127.0.0.1", port, 1000);
  ntp.setLocalPort(port + 1);
  CHECK(ntp.begin());
  CHECK_NEAR(static_cast<double>(ntp.getTimeMs()),
             static_cast<double>(systemMs()), 50);
  // begin() does two exchanges. The server counts a request after it has sent
  // the answer: give its thread a moment
  for (int i = 0; i < 1000 && server.getRequestCount() < 2; i++) {
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  CHECK(server.getRequestCount() >= 2);
}

TEST(client_resolves_host_names) {
  uint16_t port = basePort() + 2;
  TinySystemTimeSource source;
  TinyNTPServer<PosixUDP> server;
  CHECK(server.begin(source, port));
  ServerThread t(server);
  TinyNTPClient<PosixUDP> ntp("localhost", port, 1000);
  ntp.setLocalPort(port + 1);
  CHECK(ntp.begin());
}

TEST(server_without_time_does_not_answer) {
  uint16_t port = basePort() + 6;
  TinyClockTimeSource clock;  // not set
  TinyNTPServer<PosixUDP> server;
  CHECK(server.begin(clock, port));
  ServerThread t(server);
  TinyNTPClient<PosixUDP> ntp("127.0.0.1", port, 300);
  ntp.setLocalPort(port + 1);
  CHECK(!ntp.begin());
  CHECK_EQ(server.getRequestCount(), 0u);  // nothing was answered
  clock.setTimeMs(static_cast<uint64_t>(systemMs()));
  CHECK(ntp.begin());  // now it answers
}

TEST(set_the_time_over_sockets) {
  using Msg = TinyTimeSetMessage;
  uint16_t port = basePort() + 8;
  TinyClockTimeSource clock;
  clock.setTime(1609459200);  // 2021: wrong
  TinyNTPServer<PosixUDP> server;
  CHECK(server.begin(clock, port));
  server.enableTimeSetting("topsecret");
  ServerThread t(server);

  PosixUDP udp;
  CHECK(udp.begin(port + 1));
  uint8_t out[64], in[64];
  size_t n = Msg::makeChallengeRequest(out);
  udp.beginPacket("127.0.0.1", port);
  udp.write(out, n);
  udp.endPacket();
  int len = 0;
  for (int i = 0; i < 300 && (len = udp.parsePacket()) == 0; i++) {}
  CHECK(len > 0);
  len = udp.read(in, len);
  uint8_t nonce[8];
  CHECK(Msg::parseChallengeResponse(in, len, nonce));
  uint64_t now = systemMs();
  n = Msg::makeSetRequest(out, reinterpret_cast<const uint8_t*>("topsecret"), 9,
                          nonce, now);
  udp.beginPacket("127.0.0.1", port);
  udp.write(out, n);
  udp.endPacket();
  for (int i = 0; i < 300 && (len = udp.parsePacket()) == 0; i++) {}
  CHECK(len > 0);
  len = udp.read(in, len);
  Msg::Status status = Msg::Rejected;
  uint64_t deviceMs = 0;
  CHECK(Msg::parseSetResponse(in, len, status, deviceMs));
  CHECK_EQ(static_cast<int>(status), static_cast<int>(Msg::Ok));
  CHECK_NEAR(static_cast<double>(clock.getTimeMs()),
             static_cast<double>(systemMs()), 50);
}

TEST_MAIN("loopback")

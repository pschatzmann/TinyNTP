// Tests with the Arduino emulator (https://github.com/pschatzmann/arduino-emulator):
// the library is used with the real Arduino API (Arduino.h, IPAddress, UDP
// classes), not with the simulation of the other tests. Client and server talk
// over real sockets on the loopback interface.
#include <atomic>
#include <chrono>
#include <thread>

#include "Arduino.h"
#include "WiFi.h"
#include "WiFiUdp.h"
#include "EthernetUdp.h"

#include "test.h"
#include "TinyNTP.h"

static uint16_t basePort() { return 22000 + getpid() % 10000; }

static int64_t systemMs() {
  using namespace std::chrono;
  return duration_cast<milliseconds>(system_clock::now().time_since_epoch())
      .count();
}

template <typename UDP>
struct ServerThread {
  TinyNTPServer<UDP>& server;
  std::atomic<bool> run{true};
  std::thread thread;
  explicit ServerThread(TinyNTPServer<UDP>& s) : server(s) {
    thread = std::thread([this] {
      while (run) {
        server.process();
        delay(1);
      }
    });
  }
  ~ServerThread() {
    run = false;
    thread.join();
  }
};

template <typename UDP>
static void exchange(uint16_t port) {
  // The millis() of the emulator is 0 at its first call, so the first response
  // arrives at millis() == 0 (the client used to fail in this case)
  TinySystemTimeSource source;
  TinyNTPServer<UDP> server;
  CHECK(server.begin(source, port));
  ServerThread<UDP> t(server);
  TinyNTPClient<UDP> ntp("127.0.0.1", port, 1000);
  ntp.setLocalPort(port + 1);
  CHECK(ntp.begin());
  CHECK_NEAR(static_cast<double>(ntp.getTimeMs()),
             static_cast<double>(systemMs()), 100);
  CHECK(ntp.setTimeZone(TZ_EUROPE_ZURICH));
  int hour = ntp.getLocalTm().tm_hour;
  CHECK(hour >= 0 && hour < 24);
}

TEST(client_and_server_with_WiFiUDP) { exchange<WiFiUDP>(basePort()); }

TEST(client_and_server_with_EthernetUDP) { exchange<EthernetUDP>(basePort() + 4); }

TEST(set_the_time_with_the_arduino_api) {
  using Msg = TinyTimeSetMessage;
  uint16_t port = basePort() + 8;
  TinyClockTimeSource clock;
  clock.setTime(1609459200);
  TinyNTPServer<WiFiUDP> server;
  CHECK(server.begin(clock, port));
  server.enableTimeSetting("topsecret");
  ServerThread<WiFiUDP> t(server);

  WiFiUDP udp;
  udp.begin(port + 1);
  uint8_t out[64], in[64];
  size_t n = Msg::makeChallengeRequest(out);
  udp.beginPacket("127.0.0.1", port);
  udp.write(out, n);
  udp.endPacket();
  int len = 0;
  for (int i = 0; i < 1000 && (len = udp.parsePacket()) == 0; i++) delay(1);
  CHECK(len > 0);
  len = udp.read(in, len);
  uint8_t nonce[8];
  CHECK(Msg::parseChallengeResponse(in, len, nonce));
  n = Msg::makeSetRequest(out, reinterpret_cast<const uint8_t*>("topsecret"), 9,
                          nonce, systemMs());
  udp.beginPacket("127.0.0.1", port);
  udp.write(out, n);
  udp.endPacket();
  for (int i = 0; i < 1000 && (len = udp.parsePacket()) == 0; i++) delay(1);
  CHECK(len > 0);
  len = udp.read(in, len);
  Msg::Status status = Msg::Rejected;
  uint64_t deviceMs = 0;
  CHECK(Msg::parseSetResponse(in, len, status, deviceMs));
  CHECK_EQ(static_cast<int>(status), static_cast<int>(Msg::Ok));
  CHECK_NEAR(static_cast<double>(clock.getTimeMs()),
             static_cast<double>(systemMs()), 100);
}

TEST(client_resolves_host_names) {
  uint16_t port = basePort() + 12;
  TinySystemTimeSource source;
  TinyNTPServer<EthernetUDP> server;
  CHECK(server.begin(source, port));
  ServerThread<EthernetUDP> t(server);
  TinyNTPClient<EthernetUDP> ntp("localhost", port, 1000);
  ntp.setLocalPort(port + 1);
  CHECK(ntp.begin());
}

TEST(server_without_time_does_not_answer) {
  uint16_t port = basePort() + 14;
  TinyClockTimeSource clock;  // not set
  TinyNTPServer<EthernetUDP> server;
  CHECK(server.begin(clock, port));
  ServerThread<EthernetUDP> t(server);
  TinyNTPClient<EthernetUDP> ntp("127.0.0.1", port, 300);
  ntp.setLocalPort(port + 1);
  CHECK(!ntp.begin());
  CHECK_EQ(server.getRequestCount(), 0u);  // nothing was answered
  clock.setTimeMs(static_cast<uint64_t>(systemMs()));
  CHECK(ntp.begin());  // now it answers
}

TEST_MAIN("emulator")

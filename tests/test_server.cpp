// Tests of the NTP part of TinyNTPServer with a simulated network
#include "mock.h"
#include "test.h"
#include "TinyNTP/TinyClockTimeSource.h"
#include "TinyNTP/TinyNTPServer.h"

using namespace tiny_ntp;
using Server = TinyNTPServer<MockUDP>;

static const uint64_t serverMs = 1782907200500ULL;  // 2026-07-01 12:00:00.500

/// A time source that moves 1 ms forward for each call
struct Ticking : TinyTimeSource {
  uint64_t t = serverMs;
  uint64_t getTimeMs() override { return t++; }
};

static void setup() {
  mock::reset();
  MockUDP::reset();
}

TEST(begin_binds_the_port) {
  setup();
  TinyClockTimeSource clock;
  Server s;
  CHECK(s.begin(clock));
  CHECK_EQ(MockUDP::boundPorts.back(), 123);
  CHECK(s.begin(clock, 12345));
  CHECK_EQ(MockUDP::boundPorts.back(), 12345);
}

TEST(begin_fails_if_the_port_cannot_be_opened) {
  setup();
  MockUDP::beginResult = 0;
  TinyClockTimeSource clock;
  Server s;
  CHECK(!s.begin(clock));
}

TEST(nothing_to_do_without_requests_or_source) {
  setup();
  Server s;
  CHECK(!s.process());  // no begin()
  TinyClockTimeSource clock;
  clock.setTime(1800000000);
  s.begin(clock);
  CHECK(!s.process());  // no request
  CHECK(MockUDP::sent.empty());
  CHECK_EQ(s.getRequestCount(), 0u);
}

TEST(answers_a_request) {
  setup();
  TinyClockTimeSource clock;
  clock.setTimeMs(serverMs);
  Server s;
  s.begin(clock);
  Bytes request = ntpRequest(1800000000123ULL);
  MockUDP::deliver(request, 0x0a00a8c0, 54321);
  CHECK(s.process());
  CHECK_EQ(MockUDP::sent.size(), 1u);
  const auto& r = MockUDP::sent[0];
  CHECK_EQ(r.data.size(), 48u);
  CHECK_EQ(r.data[0], 0x24);  // LI=0, version 4, mode 4 (server)
  CHECK_EQ(r.data[1], 3);     // stratum
  CHECK_EQ(r.data[2], 6);     // the poll of the client
  CHECK_EQ(r.data[3], 0xF6);  // precision: -10
  CHECK_EQ(get32(r.data, 4), 0u);      // root delay
  CHECK_EQ(get32(r.data, 8), 0x400u);  // root dispersion
  CHECK_EQ(get32(r.data, 12), 0u);     // reference id
  CHECK_NEAR(static_cast<double>(getTimestamp(r.data, 16)), serverMs, 1);
  CHECK_NEAR(static_cast<double>(getTimestamp(r.data, 32)), serverMs, 1);
  CHECK_NEAR(static_cast<double>(getTimestamp(r.data, 40)), serverMs, 1);
  // the answer goes to the sender of the request
  CHECK_EQ(r.ip.addr, 0x0a00a8c0u);
  CHECK_EQ(r.port, 54321);
  CHECK_EQ(s.getRequestCount(), 1u);
}

TEST(originate_time_is_the_transmit_time_of_the_client) {
  setup();
  TinyClockTimeSource clock;
  clock.setTimeMs(serverMs);
  Server s;
  s.begin(clock);
  Bytes request = ntpRequest(1800000000123ULL);
  for (int i = 0; i < 8; i++) request[40 + i] = 0x10 + i;  // any value
  MockUDP::deliver(request);
  CHECK(s.process());
  const auto& r = MockUDP::sent[0].data;
  for (int i = 0; i < 8; i++) CHECK_EQ(r[24 + i], 0x10 + i);
}

TEST(receive_time_is_before_the_transmit_time) {
  setup();
  Ticking source;
  Server s;
  s.begin(source);
  MockUDP::deliver(ntpRequest());
  CHECK(s.process());
  const auto& r = MockUDP::sent[0].data;
  uint64_t rx = getTimestamp(r, 32), tx = getTimestamp(r, 40);
  CHECK(tx > rx);
  CHECK(tx - rx < 5);
}

TEST(version_of_the_client_is_returned) {
  for (int version = 1; version <= 4; version++) {
    setup();
    TinyClockTimeSource clock;
    clock.setTimeMs(serverMs);
    Server s;
    s.begin(clock);
    MockUDP::deliver(ntpRequest(serverMs, version));
    CHECK(s.process());
    CHECK_EQ(MockUDP::sent[0].data[0], (version << 3) | 4);
  }
}

TEST(unknown_versions_are_answered_with_version_4) {
  for (int version : {0, 5, 6, 7}) {
    setup();
    TinyClockTimeSource clock;
    clock.setTimeMs(serverMs);
    Server s;
    s.begin(clock);
    MockUDP::deliver(ntpRequest(serverMs, version));
    CHECK(s.process());
    CHECK_EQ(MockUDP::sent[0].data[0], 0x24);
  }
}

TEST(stratum_can_be_set) {
  setup();
  TinyClockTimeSource clock;
  clock.setTimeMs(serverMs);
  Server s;
  s.begin(clock);
  s.setStratum(1);
  MockUDP::deliver(ntpRequest());
  CHECK(s.process());
  CHECK_EQ(MockUDP::sent[0].data[1], 1);
  s.setStratum(10);
  MockUDP::deliver(ntpRequest());
  CHECK(s.process());
  CHECK_EQ(MockUDP::sent[1].data[1], 10);
}

TEST(only_client_requests_are_answered) {
  for (int mode : {0, 1, 2, 4, 5, 6}) {
    setup();
    TinyClockTimeSource clock;
    clock.setTimeMs(serverMs);
    Server s;
    s.begin(clock);
    Bytes b = ntpRequest();
    b[0] = (4 << 3) | mode;
    MockUDP::deliver(b);
    CHECK(!s.process());
    CHECK(MockUDP::sent.empty());
  }
}

TEST(short_packets_are_ignored) {
  setup();
  TinyClockTimeSource clock;
  clock.setTimeMs(serverMs);
  Server s;
  s.begin(clock);
  for (size_t len : {1, 2, 10, 47}) {
    Bytes b = ntpRequest();
    b.resize(len);
    MockUDP::deliver(b);
    CHECK(!s.process());
  }
  MockUDP::deliver(Bytes());  // empty packet
  CHECK(!s.process());
  CHECK(MockUDP::sent.empty());
}

TEST(packets_with_extension_fields_are_answered) {
  setup();
  TinyClockTimeSource clock;
  clock.setTimeMs(serverMs);
  Server s;
  s.begin(clock);
  Bytes b = ntpRequest();
  b.resize(68, 0xEE);  // e.g. a MAC
  MockUDP::deliver(b);
  CHECK(s.process());
  CHECK_EQ(MockUDP::sent[0].data.size(), 48u);
  Bytes big = ntpRequest();
  big.resize(1000, 0x55);
  MockUDP::deliver(big);
  CHECK(s.process());
}

TEST(no_answer_while_the_time_is_not_known) {
  setup();
  TinyClockTimeSource clock;  // not set
  Server s;
  s.begin(clock);
  MockUDP::deliver(ntpRequest());
  CHECK(!s.process());
  CHECK(MockUDP::sent.empty());
  clock.setTimeMs(serverMs);  // now it is known
  MockUDP::deliver(ntpRequest());
  CHECK(s.process());
  clock.clear();  // and not any more
  MockUDP::deliver(ntpRequest());
  CHECK(!s.process());
  CHECK_EQ(s.getRequestCount(), 1u);
}

TEST(one_request_per_call_and_all_get_an_answer) {
  setup();
  TinyClockTimeSource clock;
  clock.setTimeMs(serverMs);
  Server s;
  s.begin(clock);
  for (int i = 0; i < 5; i++) MockUDP::deliver(ntpRequest(), 100 + i, 2000 + i);
  for (int i = 0; i < 5; i++) CHECK(s.process());
  CHECK(!s.process());
  CHECK_EQ(MockUDP::sent.size(), 5u);
  for (int i = 0; i < 5; i++) {  // in the order of the requests
    CHECK_EQ(MockUDP::sent[i].ip.addr, 100u + i);
    CHECK_EQ(MockUDP::sent[i].port, 2000 + i);
  }
  CHECK_EQ(s.getRequestCount(), 5u);
}

TEST(time_moves_on_between_requests) {
  setup();
  TinyClockTimeSource clock;
  clock.setTimeMs(serverMs);
  Server s;
  s.begin(clock);
  MockUDP::deliver(ntpRequest());
  s.process();
  mock::now += 60000;
  MockUDP::deliver(ntpRequest());
  s.process();
  uint64_t first = getTimestamp(MockUDP::sent[0].data, 40);
  uint64_t second = getTimestamp(MockUDP::sent[1].data, 40);
  CHECK_NEAR(static_cast<double>(second - first), 60000, 1);
}

TEST(end_stops_the_server_and_begin_resets_the_counters) {
  setup();
  TinyClockTimeSource clock;
  clock.setTimeMs(serverMs);
  Server s;
  s.begin(clock);
  MockUDP::deliver(ntpRequest());
  CHECK(s.process());
  CHECK_EQ(s.getRequestCount(), 1u);
  s.end();
  CHECK_EQ(MockUDP::stopCount, 1);
  MockUDP::deliver(ntpRequest());
  CHECK(!s.process());
  MockUDP::inbox.clear();
  s.begin(clock);
  CHECK_EQ(s.getRequestCount(), 0u);
}

TEST(time_sources_can_be_exchanged) {
  setup();
  TinyClockTimeSource a, b;
  a.setTimeMs(serverMs);
  b.setTimeMs(serverMs + 5000000);
  Server s;
  s.begin(a);
  MockUDP::deliver(ntpRequest());
  s.process();
  s.begin(b);
  MockUDP::deliver(ntpRequest());
  s.process();
  uint64_t t1 = getTimestamp(MockUDP::sent[0].data, 40);
  uint64_t t2 = getTimestamp(MockUDP::sent[1].data, 40);
  CHECK_NEAR(static_cast<double>(t2 - t1), 5000000, 1);
}

TEST(fraction_of_the_second_is_converted) {
  for (uint64_t ms : {0ULL, 1ULL, 250ULL, 500ULL, 999ULL}) {
    setup();
    TinyClockTimeSource clock;
    clock.setTimeMs(1800000000000ULL + ms);
    Server s;
    s.begin(clock);
    MockUDP::deliver(ntpRequest());
    s.process();
    uint64_t t = getTimestamp(MockUDP::sent[0].data, 40);
    CHECK_NEAR(static_cast<double>(t), 1800000000000.0 + ms, 1);
  }
}

TEST(udp_object_is_accessible) {
  Server s;
  MockUDP& udp = s.getUDP();
  (void)udp;
  CHECK(true);
}

TEST_MAIN("server")

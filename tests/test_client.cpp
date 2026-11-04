// Tests of TinyNTPClient with a simulated network and a simulated NTP server
#include "mock.h"
#include "test.h"
#include "TinyNTP/TinyNTPClient.h"
#include "TinyNTP/TinyNTPServer.h"

using namespace tiny_ntp;
using Client = TinyNTPClient<MockUDP>;

// 2026-07-01 12:00:00.500 UTC and 2026-01-15 12:00:00.000 UTC
static const uint64_t summerMs = 1782907200500ULL;
static const uint64_t winterMs = 1768478400000ULL;

/// A simulated NTP server. Its time runs with millis() times `rate`.
struct FakeServer {
  uint64_t baseMs;
  uint32_t baseMillis = 1000;
  double rate = 1.0;
  uint32_t processingMs = 0;  // the server needs this time to answer
  int requests = 0;
  explicit FakeServer(uint64_t ms) : baseMs(ms) {}
  uint64_t now() const {
    return baseMs + static_cast<uint64_t>((mock::now - baseMillis) * rate);
  }
  Bytes operator()(const Bytes& request) {
    requests++;
    Bytes r(48, 0);
    r[0] = 0x24;  // version 4, mode 4 (server)
    r[1] = 2;     // stratum
    for (int i = 0; i < 8; i++) r[24 + i] = request[40 + i];  // originate
    putTimestamp(r, 32, now());                               // receive
    putTimestamp(r, 40, now() + processingMs);                // transmit
    return r;
  }
};

static void setup(FakeServer& server) {
  mock::reset();
  MockUDP::reset();
  MockUDP::responder = [&server](const Bytes& b) { return server(b); };
  MockUDP::idleStep = 1;  // time passes while we wait for a response
}

TEST(begin_gets_the_time_of_the_server) {
  FakeServer server(summerMs);
  setup(server);
  Client ntp("192.0.2.1", 123, 1000);
  CHECK(!ntp);
  CHECK_EQ(ntp.getTimeMs(), 0u);  // not set yet
  CHECK(ntp.begin());
  CHECK(ntp);
  CHECK_EQ(ntp.getTimeMs(), summerMs);
  CHECK_EQ(ntp.getTimeSec(), 1782907200u);
  CHECK_EQ(ntp.millis(), summerMs);
  std::tm tm = ntp.getTm();
  CHECK_EQ(tm.tm_year + 1900, 2026);
  CHECK_EQ(tm.tm_mon + 1, 7);
  CHECK_EQ(tm.tm_mday, 1);
  CHECK_EQ(tm.tm_hour, 12);
  CHECK_EQ(tm.tm_min, 0);
  CHECK_EQ(tm.tm_sec, 0);
}

TEST(begin_does_two_exchanges) {
  FakeServer server(summerMs);
  setup(server);
  Client ntp("192.0.2.1", 123, 1000);
  CHECK(ntp.begin());
  CHECK_EQ(server.requests, 2);
  CHECK_EQ(MockUDP::sent.size(), 2u);
}

TEST(request_packets) {
  FakeServer server(summerMs);
  setup(server);
  Client ntp("192.0.2.1", 4123, 1000);
  CHECK(ntp.begin());
  const auto& first = MockUDP::sent[0];
  const auto& second = MockUDP::sent[1];
  CHECK_EQ(first.data.size(), 48u);
  CHECK_STR(first.host, "192.0.2.1");
  CHECK_EQ(first.port, 4123);
  CHECK_EQ(first.data[0], 0xDB);   // LI=3, version 3, mode 3 (client)
  CHECK_EQ(get32(first.data, 40), 0u);  // the first request has no time
  CHECK_EQ(second.data[0], 0xDB);
  // the second request contains the time of the client (NTP seconds)
  CHECK_EQ(get32(second.data, 40),
           static_cast<uint32_t>(summerMs / 1000 + NtpEpoch));
}

TEST(local_port_is_the_server_port_by_default) {
  FakeServer server(summerMs);
  setup(server);
  Client ntp("192.0.2.1", 4123, 1000);
  CHECK(ntp.begin());
  for (uint16_t p : MockUDP::boundPorts) CHECK_EQ(p, 4123);
}

TEST(local_port_can_be_changed) {
  FakeServer server(summerMs);
  setup(server);
  Client ntp("192.0.2.1", 123, 1000);
  ntp.setLocalPort(12300);
  CHECK(ntp.begin());
  CHECK(!MockUDP::boundPorts.empty());
  for (uint16_t p : MockUDP::boundPorts) CHECK_EQ(p, 12300);
  CHECK_EQ(MockUDP::sent[0].port, 123);  // the server port does not change
  ntp.setLocalPort(0);  // back to the server port
  MockUDP::boundPorts.clear();
  CHECK(ntp.update());
  CHECK_EQ(MockUDP::boundPorts.back(), 123);
}

TEST(time_runs_with_millis) {
  FakeServer server(summerMs);
  setup(server);
  Client ntp("192.0.2.1", 123, 1000);
  CHECK(ntp.begin());
  mock::now += 2500;
  CHECK_EQ(ntp.getTimeMs(), summerMs + 2500);
  CHECK_EQ(ntp.getTimeSec(), 1782907203u);
}

TEST(response_can_arrive_later) {
  FakeServer server(summerMs);
  setup(server);
  MockUDP::delayPolls = 50;  // 50 ms
  Client ntp("192.0.2.1", 123, 1000);
  CHECK(ntp.begin());
  CHECK(ntp);
}

TEST(response_after_the_timeout_is_too_late) {
  FakeServer server(summerMs);
  setup(server);
  MockUDP::delayPolls = 500;
  Client ntp("192.0.2.1", 123, 100);  // waits 100 ms
  CHECK(!ntp.begin());
}

// Some WiFi networks (e.g. with an ESP32) lose responses
TEST(request_is_sent_again_if_the_response_is_lost) {
  FakeServer server(summerMs);
  setup(server);
  int lost = 2;
  MockUDP::responder = [&](const Bytes& b) {
    return lost-- > 0 ? Bytes() : server(b);
  };
  Client ntp("192.0.2.1", 123, 1000);
  CHECK(ntp.begin());
  CHECK_EQ(MockUDP::sent.size(), 4u);  // 2 lost, then 2 exchanges
  CHECK_NEAR(static_cast<double>(ntp.getTimeMs()),
             static_cast<double>(server.now()), 2);
}

// The response to the first request arrives after the second one was sent:
// the round trip is measured from the first request
TEST(round_trip_is_measured_from_the_answered_request) {
  FakeServer server(summerMs);
  setup(server);
  MockUDP::delayPolls = 400;  // longer than the time until it is sent again
  Client ntp("192.0.2.1", 123, 1000);
  CHECK(ntp.begin());
  // 400 ms round trip: 200 ms behind the server (not 50 ms of 100 ms from
  // the request that was sent again)
  CHECK_NEAR(static_cast<double>(server.now()) -
                 static_cast<double>(ntp.getTimeMs()),
             200, 2);
}

TEST(response_to_an_earlier_update_is_ignored) {
  FakeServer server(summerMs);
  setup(server);
  Client ntp("192.0.2.1", 123, 1000);
  CHECK(ntp.begin());
  // a late response to the last request, with a wrong time
  FakeServer other(summerMs + 3600000);
  MockUDP::deliver(other(MockUDP::sent.back().data));
  CHECK(ntp.update());
  CHECK_NEAR(static_cast<double>(ntp.getTimeMs()),
             static_cast<double>(server.now()), 2);
}

TEST(socket_stays_open_between_updates) {
  FakeServer server(summerMs);
  setup(server);
  Client ntp("192.0.2.1", 123, 1000);
  CHECK(ntp.begin());
  CHECK(ntp.update());
  CHECK_EQ(MockUDP::boundPorts.size(), 1u);
  ntp.end();
  CHECK(ntp.begin());  // opened again after end()
  CHECK_EQ(MockUDP::boundPorts.size(), 2u);
}

// millis() returns an unsigned long, which has 64 bit on a computer. After an
// uptime of more than 49.7 days it is larger than any 32 bit value: the client
// gave up at the first poll, before the server could answer.
TEST(response_can_arrive_later_after_49_days_of_uptime_on_64_bit) {
  FakeServer server(summerMs);
  setup(server);
  mock::high = 1ULL << 32;
  MockUDP::delayPolls = 50;
  Client ntp("192.0.2.1", 123, 1000);
  CHECK(ntp.begin());
}

// The client used millis() == 0 for "never updated". A program that asks for
// the time at once (on a computer or with the Arduino emulator) got the first
// response at millis() == 0 and the result was twice the correct time.
TEST(first_response_when_millis_is_zero) {
  FakeServer server(summerMs);
  server.baseMillis = 0;
  setup(server);
  mock::now = 0;
  Client ntp("192.0.2.1", 123, 1000);
  CHECK(ntp.begin());
  CHECK_NEAR(static_cast<double>(ntp.getTimeMs()),
             static_cast<double>(summerMs), 1000);
}

TEST(first_response_when_millis_is_not_zero) {
  FakeServer server(summerMs);
  server.baseMillis = 1;
  setup(server);
  mock::now = 1;
  Client ntp("192.0.2.1", 123, 1000);
  CHECK(ntp.begin());
  CHECK_NEAR(static_cast<double>(ntp.getTimeMs()),
             static_cast<double>(summerMs), 1000);
}

TEST(time_is_corrected_by_half_of_the_network_delay) {
  FakeServer server(summerMs);
  setup(server);
  MockUDP::delayPolls = 50;  // the answer needs 50 ms
  Client ntp("192.0.2.1", 123, 1000);
  CHECK(ntp.begin());
  // The simulated server stamps its answer when the request arrives, and the
  // answer needs 50 ms to get to us: without a correction the client would be
  // 50 ms behind the server, with half of the delay added it is 25 ms behind
  CHECK_NEAR(static_cast<double>(server.now()) -
                 static_cast<double>(ntp.getTimeMs()),
             25, 2);
}

TEST(processing_time_of_the_server_is_not_network_delay) {
  FakeServer server(summerMs);
  server.processingMs = 20;  // 20 ms between receive and transmit
  setup(server);
  MockUDP::delayPolls = 100;  // 100 ms to get the answer: 80 ms network delay
  Client ntp("192.0.2.1", 123, 1000);
  CHECK(ntp.begin());
  // The transmit time was stamped 100 ms ago plus 20 ms processing time, and
  // half of the 80 ms network delay is added: 40 ms behind the server
  CHECK_NEAR(static_cast<double>(server.now()) -
                 static_cast<double>(ntp.getTimeMs()),
             40, 2);
}

TEST(a_server_that_needs_longer_than_the_round_trip_does_not_move_time_back) {
  FakeServer server(summerMs);
  server.processingMs = 500;  // not possible: more than the round trip
  setup(server);
  Client ntp("192.0.2.1", 123, 1000);
  CHECK(ntp.begin());
  CHECK_EQ(ntp.getTimeMs(), summerMs + 500);  // the transmit time, no change
}

TEST(timeout_and_response_work_across_the_overflow_of_millis) {
  FakeServer server(summerMs);
  server.baseMillis = 0xFFFFFE00u;
  setup(server);
  mock::now = 0xFFFFFE00u;  // 512 ms before the overflow
  MockUDP::delayPolls = 500;
  Client ntp("192.0.2.1", 123, 1000);  // the response arrives after the overflow
  CHECK(ntp.begin());
  CHECK(ntp);
  // and a timeout still ends after the time that was given
  FakeServer none(summerMs);
  setup(none);
  MockUDP::responder = nullptr;
  mock::now = 0xFFFFFF80u;
  Client lost("192.0.2.1", 123, 300);
  CHECK(!lost.begin());
  CHECK(mock::now - 0xFFFFFF80u >= 300);  // it waited for the timeout
  CHECK(mock::now - 0xFFFFFF80u < 400);
}

TEST(time_keeps_running_across_the_overflow_of_millis) {
  FakeServer server(summerMs);
  server.baseMillis = 0xFFFFFF00u;
  setup(server);
  mock::now = 0xFFFFFF00u;
  Client ntp("192.0.2.1", 123, 1000);
  CHECK(ntp.begin());
  mock::now = 0x00000100u;  // 512 ms later
  CHECK_EQ(ntp.getTimeMs(), summerMs + 512);
}

TEST(fraction_of_a_second) {
  FakeServer server(1782907200750ULL);
  setup(server);
  Client ntp("192.0.2.1", 123, 1000);
  CHECK(ntp.begin());
  CHECK_NEAR(static_cast<double>(ntp.getTimeMs() % 1000), 750, 1);
}

TEST(time_offset) {
  FakeServer server(summerMs);
  setup(server);
  Client ntp("192.0.2.1", 123, 1000);
  CHECK(ntp.begin());
  ntp.setTimeOffsetHours(2);
  CHECK_EQ(ntp.getTimeMs(), summerMs + 7200000);
  ntp.setTimeOffsetSeconds(-90);
  CHECK_EQ(ntp.getTimeMs(), summerMs - 90000);
  CHECK_EQ(ntp.getTm().tm_hour, 11);
  CHECK_EQ(ntp.getTm().tm_min, 58);
  ntp.setTimeOffsetSeconds(0);
  CHECK_EQ(ntp.getTimeMs(), summerMs);
}

TEST(a_server_that_uses_the_client_serves_its_time_offset) {
  // the offset is part of getTimeMs(): do not set it on such a client
  FakeServer server(summerMs);
  setup(server);
  Client ntp("192.0.2.1", 123, 1000);
  CHECK(ntp.begin());
  ntp.setTimeOffsetHours(1);
  TinyNTPServer<MockUDP> served;
  MockUDP::reset();
  CHECK(served.begin(ntp));
  MockUDP::deliver(ntpRequest());
  CHECK(served.process());
  CHECK_NEAR(static_cast<double>(getTimestamp(MockUDP::sent[0].data, 40)),
             static_cast<double>(summerMs + 3600000), 2);
}

TEST(local_time_with_summer_time) {
  FakeServer server(summerMs);
  setup(server);
  Client ntp("192.0.2.1", 123, 1000);
  CHECK(ntp.begin());
  CHECK(ntp.setTimeZone(TZ_EUROPE_ZURICH));
  std::tm tm = ntp.getLocalTm();
  CHECK_EQ(tm.tm_hour, 14);
  CHECK_EQ(tm.tm_isdst, 1);
  CHECK_STR(ntp.getTimeZoneName(), "CEST");
  CHECK_EQ(ntp.getTm().tm_hour, 12);  // UTC does not change
}

TEST(local_time_with_winter_time) {
  FakeServer server(winterMs);
  setup(server);
  Client ntp("192.0.2.1", 123, 1000);
  CHECK(ntp.begin());
  CHECK(ntp.setTimeZone(TZ_EUROPE_ZURICH));
  std::tm tm = ntp.getLocalTm();
  CHECK_EQ(tm.tm_hour, 13);
  CHECK_EQ(tm.tm_isdst, 0);
  CHECK_STR(ntp.getTimeZoneName(), "CET");
}

TEST(local_time_follows_the_switch_to_summer_time) {
  // 2026-03-29 00:59:50 UTC: 10 seconds before the switch
  FakeServer server(1774745990000ULL);
  setup(server);
  Client ntp("192.0.2.1", 123, 1000);
  CHECK(ntp.begin());
  ntp.setTimeZone(TZ_EUROPE_ZURICH);
  CHECK_EQ(ntp.getLocalTm().tm_hour, 1);
  CHECK_EQ(ntp.getLocalTm().tm_isdst, 0);
  mock::now += 10000;
  CHECK_EQ(ntp.getLocalTm().tm_hour, 3);
  CHECK_EQ(ntp.getLocalTm().tm_isdst, 1);
}

TEST(invalid_time_zone_is_refused_and_local_time_is_utc) {
  FakeServer server(summerMs);
  setup(server);
  Client ntp("192.0.2.1", 123, 1000);
  CHECK(ntp.begin());
  CHECK(!ntp.setTimeZone("nonsense"));
  CHECK_EQ(ntp.getLocalTm().tm_hour, 12);
  CHECK_STR(ntp.getTimeZoneName(), "UTC");
}

TEST(time_zone_survives_update_and_end) {
  FakeServer server(summerMs);
  setup(server);
  Client ntp("192.0.2.1", 123, 1000);
  ntp.setTimeZone(TZ_EUROPE_ZURICH);
  CHECK(ntp.begin());  // begin() calls end(): the time zone is kept
  CHECK_EQ(ntp.getLocalTm().tm_hour, 14);
}

TEST(timeout_without_response) {
  FakeServer server(summerMs);
  setup(server);
  MockUDP::responder = nullptr;  // nobody answers
  Client ntp("192.0.2.1", 123, 100);
  uint32_t start = mock::now;
  CHECK(!ntp.begin());
  CHECK(!ntp);
  CHECK_EQ(ntp.getTimeMs(), 0u);
  uint32_t waited = mock::now - start;
  CHECK(waited >= 100 && waited < 200);  // timeout of 100 ms
  CHECK(mock::serialOutput.find("timed out") != std::string::npos);
}

TEST(short_response_is_refused) {
  FakeServer server(summerMs);
  setup(server);
  MockUDP::responder = [](const Bytes&) { return Bytes(20, 0); };
  Client ntp("192.0.2.1", 123, 100);
  CHECK(!ntp.begin());
  CHECK(!ntp);
  CHECK(mock::serialOutput.find("too short") != std::string::npos);
}

TEST(failed_update_keeps_the_time) {
  FakeServer server(summerMs);
  setup(server);
  Client ntp("192.0.2.1", 123, 100);
  CHECK(ntp.begin());
  MockUDP::responder = nullptr;
  mock::now += 3000;
  CHECK(!ntp.update());
  CHECK(ntp);
  CHECK_NEAR(static_cast<double>(ntp.getTimeMs()),
             static_cast<double>(summerMs + 3000), 200);  // runs on millis()
}

// The clock offset must not be added to the time of the server (it was, and
// after a jump of +30 s the client was 60 s off)
TEST(update_follows_a_jump_of_the_server) {
  FakeServer server(summerMs);
  setup(server);
  Client ntp("192.0.2.1", 123, 1000);
  CHECK(ntp.begin());
  server.baseMs += 30000;  // the clock of the server is 30 s ahead
  CHECK(ntp.update());
  CHECK_NEAR(static_cast<double>(ntp.getTimeMs()),
             static_cast<double>(server.now()), 50);
}

TEST(update_follows_a_jump_back) {
  FakeServer server(summerMs);
  setup(server);
  Client ntp("192.0.2.1", 123, 1000);
  CHECK(ntp.begin());
  server.baseMs -= 3600000;  // one hour back
  CHECK(ntp.update());
  CHECK_NEAR(static_cast<double>(ntp.getTimeMs()),
             static_cast<double>(server.now()), 50);
}

// The offset of 10 s that millis() has built up after 1000 s was counted twice,
// so the factor was 1.02 and not 1.01
TEST(correction_factor_follows_the_speed_of_millis) {
  FakeServer server(summerMs);
  server.rate = 1.01;  // millis() is 1% slower than the real time
  setup(server);
  Client ntp("192.0.2.1", 123, 1000);
  CHECK(ntp.begin());
  CHECK_NEAR(ntp.getMillisCorrectionFactor(), 1.0, 1e-9);
  mock::now += 1000000;  // 1000 s later
  CHECK(ntp.update());
  CHECK_NEAR(ntp.getMillisCorrectionFactor(), 1.01, 0.001);
  // the time between the updates is now corrected
  uint64_t before = ntp.getTimeMs();
  mock::now += 100000;
  CHECK_NEAR(static_cast<double>(ntp.getTimeMs() - before), 101000, 1000);
}

TEST(correction_factor_is_not_changed_within_10_seconds) {
  FakeServer server(summerMs);
  server.rate = 1.5;
  setup(server);
  Client ntp("192.0.2.1", 123, 1000);
  CHECK(ntp.begin());
  mock::now += 5000;
  CHECK(ntp.update());
  CHECK_NEAR(ntp.getMillisCorrectionFactor(), 1.0, 1e-9);
}

TEST(end_clears_the_state) {
  FakeServer server(summerMs);
  setup(server);
  Client ntp("192.0.2.1", 123, 1000);
  CHECK(ntp.begin());
  ntp.setTimeOffsetHours(1);
  int stops = MockUDP::stopCount;
  ntp.end();
  CHECK(!ntp);
  CHECK_EQ(ntp.getTimeMs(), 0u);
  CHECK_EQ(MockUDP::stopCount, stops + 1);
  CHECK_NEAR(ntp.getMillisCorrectionFactor(), 1.0, 1e-9);
  CHECK(ntp.begin());  // can be started again
  CHECK_EQ(ntp.getTimeMs(), summerMs + (mock::now - 1000));
}

TEST(set_server) {
  FakeServer server(summerMs);
  setup(server);
  Client ntp("192.0.2.1", 123, 1000);
  ntp.setServer("198.51.100.7", 4444);
  CHECK(ntp.begin());
  CHECK_STR(MockUDP::sent[0].host, "198.51.100.7");
  CHECK_EQ(MockUDP::sent[0].port, 4444);
  ntp.setServer("203.0.113.9");  // the default port
  CHECK(ntp.update());
  CHECK_STR(MockUDP::sent.back().host, "203.0.113.9");
  CHECK_EQ(MockUDP::sent.back().port, 123);
}

TEST(default_server_is_the_pool) {
  FakeServer server(summerMs);
  setup(server);
  Client ntp;
  CHECK(ntp.begin());
  CHECK_STR(MockUDP::sent[0].host, "pool.ntp.org");
}

TEST(client_is_a_time_source) {
  FakeServer server(summerMs);
  setup(server);
  Client ntp("192.0.2.1", 123, 1000);
  CHECK(ntp.begin());
  TinyTimeSource& source = ntp;
  CHECK_EQ(source.getTimeMs(), summerMs);
  CHECK(!source.setTimeMs(1));  // the client cannot be set
}

TEST(udp_object_is_accessible) {
  Client ntp;
  MockUDP& udp = ntp.getUDP();
  (void)udp;
  CHECK(true);
}

TEST(big_values_do_not_overflow) {
  // 2100-01-01 00:00:00 UTC: the 32 bit NTP seconds are 2208988800 + ...
  FakeServer server(4102444800000ULL);
  setup(server);
  Client ntp("192.0.2.1", 123, 1000);
  CHECK(ntp.begin());
  CHECK_EQ(ntp.getTm().tm_year + 1900, 2100);
}


// --- details of the answer (getLastResult()) ---

TEST(last_result_has_the_header_of_the_answer) {
  FakeServer server(summerMs);
  server.processingMs = 2;
  setup(server);
  MockUDP::responder = [&server](const Bytes& b) {
    Bytes r = server(b);
    r[1] = 1;     // stratum 1
    r[3] = 0xEC;  // precision -20
    r[4] = 0;     // root delay 0x00008000 = 0.5 s
    r[5] = 0;
    r[6] = 0x80;
    r[7] = 0;
    r[8] = 0;  // root dispersion 0x00000100 = 1/256 s
    r[9] = 0;
    r[10] = 1;
    r[11] = 0;
    r[12] = 'G';
    r[13] = 'P';
    r[14] = 'S';
    r[15] = 0;
    return r;
  };
  Client ntp("192.0.2.1", 123, 1000);
  CHECK(ntp.begin());
  const TinyNTPResult& r = ntp.getLastResult();
  CHECK(r.valid);
  CHECK_EQ(r.stratum, 1);
  CHECK_EQ(r.precision, -20);
  CHECK_EQ(r.leapIndicator, 0);
  CHECK_EQ(r.version, 4);
  CHECK_NEAR(r.rootDelayMs, 500.0, 1e-6);
  CHECK_NEAR(r.rootDispersionMs, 1000.0 / 256, 1e-6);
  CHECK_STR(r.referenceText, "GPS");
  CHECK(r.roundTripMs >= r.delayMs);
  CHECK_EQ(r.receivedMillis, static_cast<uint32_t>(millis()));
  CHECK_EQ(r.serverTimeMs, ntp.getTimeMs());
}

TEST(last_result_reference_of_stratum_2_is_an_address) {
  FakeServer server(summerMs);
  setup(server);
  MockUDP::responder = [&server](const Bytes& b) {
    Bytes r = server(b);
    r[12] = 192;
    r[13] = 0;
    r[14] = 2;
    r[15] = 7;
    return r;
  };
  Client ntp("192.0.2.1", 123, 1000);
  CHECK(ntp.begin());
  CHECK_STR(ntp.getLastResult().referenceText, "192.0.2.7");
}

TEST(kiss_of_death_is_rejected) {
  FakeServer server(summerMs);
  setup(server);
  MockUDP::responder = [&server](const Bytes& b) {
    Bytes r = server(b);
    r[1] = 0;  // stratum 0: kiss-o'-death
    r[12] = 'R';
    r[13] = 'A';
    r[14] = 'T';
    r[15] = 'E';
    return r;
  };
  Client ntp("192.0.2.1", 123, 1000);
  CHECK(!ntp.begin());
  CHECK(!ntp);
  CHECK(!ntp.getLastResult().valid);
  CHECK_STR(ntp.getLastResult().referenceText, "RATE");
  CHECK(mock::serialOutput.find("kiss-o'-death RATE") != std::string::npos);
}

TEST(unsynchronised_server_is_rejected_by_default) {
  FakeServer server(summerMs);
  setup(server);
  MockUDP::responder = [&server](const Bytes& b) {
    Bytes r = server(b);
    r[0] = 0xE4;  // LI 3
    return r;
  };
  Client ntp("192.0.2.1", 123, 1000);
  CHECK(!ntp.begin());
  CHECK_EQ(ntp.getLastResult().leapIndicator, 3);
  ntp.setRejectUnsynchronized(false);
  CHECK(ntp.begin());
}

TEST(server_name_is_copied) {
  FakeServer server(summerMs);
  setup(server);
  char name[32];
  snprintf(name, sizeof(name), "%s", "time.example.com");
  Client ntp(name, 123, 1000);
  snprintf(name, sizeof(name), "%s", "changed");
  CHECK_STR(ntp.getServer(), "time.example.com");
  CHECK(ntp.begin());
  CHECK_EQ(MockUDP::sent[0].host, std::string("time.example.com"));
}

static std::string logged;
static void collect(const char* message) { logged += message; }

TEST(log_can_be_redirected_and_switched_off) {
  FakeServer server(summerMs);
  setup(server);
  MockUDP::responder = nullptr;  // no answers: timeout
  Client ntp("192.0.2.1", 123, 50);
  logged.clear();
  ntp.setLogger(collect);
  CHECK(!ntp.begin());
  CHECK(logged.find("timed out") != std::string::npos);
  CHECK(mock::serialOutput.empty());
  logged.clear();
  ntp.setLogEnabled(false);
  CHECK(!ntp.begin());
  CHECK(logged.empty());
}

TEST_MAIN("client")

// Tests of setting the time of a TinyNTPServer over the network
// (TinyNTPServer::enableTimeSetting() and TinyTimeSetMessage)
#include "mock.h"
#include "test.h"
#include "TinyNTP/TinyClockTimeSource.h"
#include "TinyNTP/TinyNTPServer.h"

using namespace tiny_ntp;
using Server = TinyNTPServer<MockUDP>;
using Msg = TinyTimeSetMessage;

static const char* secret = "topsecret";
static const uint8_t* key() { return reinterpret_cast<const uint8_t*>(secret); }
static const size_t keyLen = 9;

// a device with a clock that has the wrong time
struct Device {
  TinyClockTimeSource clock;
  Server server;
  Device() {
    mock::reset();
    MockUDP::reset();
    clock.setTime(1609459200);  // 2021-01-01
    server.begin(clock);
    server.enableTimeSetting(secret);
  }
  /// Send a message and return the answer (empty if there is none)
  Bytes send(const Bytes& message, bool* handled = nullptr) {
    size_t before = MockUDP::sent.size();
    MockUDP::deliver(message, 0x0100a8c0, 5555);
    bool h = server.process();
    if (handled) *handled = h;
    if (MockUDP::sent.size() == before) return Bytes();
    return MockUDP::sent.back().data;
  }
  Bytes challenge() {
    uint8_t b[2];
    size_t n = Msg::makeChallengeRequest(b);
    return send(Bytes(b, b + n));
  }
  Bytes setRequest(const Bytes& challengeResponse, uint64_t timeMs,
                   const uint8_t* k = nullptr, size_t kl = 0) {
    uint8_t nonce[8], b[64];
    Msg::parseChallengeResponse(challengeResponse.data(),
                                challengeResponse.size(), nonce);
    size_t n = Msg::makeSetRequest(b, k ? k : key(), k ? kl : keyLen, nonce,
                                   timeMs);
    return Bytes(b, b + n);
  }
  /// The status of a set response (255 if there is no valid response)
  static int status(const Bytes& r) {
    Msg::Status s;
    uint64_t t;
    return Msg::parseSetResponse(r.data(), r.size(), s, t) ? s : 255;
  }
};

static const uint64_t newTime = 1800000000000ULL;

TEST(challenge_is_answered_with_a_nonce) {
  Device d;
  Bytes r = d.challenge();
  uint8_t nonce[8];
  CHECK(Msg::parseChallengeResponse(r.data(), r.size(), nonce));
  CHECK_EQ(r.size(), 10u);
  CHECK_EQ(MockUDP::sent.back().ip.addr, 0x0100a8c0u);  // to the sender
  CHECK_EQ(MockUDP::sent.back().port, 5555);
}

TEST(nonces_are_different_even_at_the_same_time) {
  Device d;
  Bytes a = d.challenge(), b = d.challenge(), c = d.challenge();
  CHECK(a != b);
  CHECK(b != c);
  CHECK(a != c);
}

TEST(valid_request_sets_the_time) {
  Device d;
  Bytes c = d.challenge();
  bool handled = false;
  Bytes r = d.send(d.setRequest(c, newTime), &handled);
  CHECK(handled);
  CHECK_EQ(Device::status(r), Msg::Ok);
  CHECK_EQ(d.clock.getTimeMs(), newTime);
  CHECK_EQ(d.server.getTimeSetCount(), 1u);
  // the response contains the new time of the server
  Msg::Status s;
  uint64_t t = 0;
  CHECK(Msg::parseSetResponse(r.data(), r.size(), s, t));
  CHECK_EQ(t, newTime);
  CHECK_EQ(r.size(), 11u);
}

TEST(set_time_keeps_running) {
  Device d;
  d.send(d.setRequest(d.challenge(), newTime));
  mock::now += 3000;
  CHECK_EQ(d.clock.getTimeMs(), newTime + 3000);
}

TEST(ntp_clients_get_the_new_time) {
  Device d;
  d.send(d.setRequest(d.challenge(), newTime));
  MockUDP::deliver(ntpRequest());
  CHECK(d.server.process());
  CHECK_NEAR(static_cast<double>(getTimestamp(MockUDP::sent.back().data, 40)),
             static_cast<double>(newTime), 1);
}

TEST(wrong_secret_is_rejected) {
  Device d;
  Bytes c = d.challenge();
  const uint8_t wrong[] = "topsecreT";
  CHECK_EQ(Device::status(d.send(d.setRequest(c, newTime, wrong, 9))),
           Msg::Rejected);
  CHECK_EQ(d.clock.getTimeMs(), 1609459200000ULL);  // not changed
  CHECK_EQ(d.server.getTimeSetCount(), 0u);
}

TEST(a_request_without_challenge_is_rejected) {
  Device d;
  Bytes fake = d.challenge();
  d.server.enableTimeSetting(secret);  // forgets the nonce
  CHECK_EQ(Device::status(d.send(d.setRequest(fake, newTime))), Msg::Rejected);
  // random nonce
  uint8_t nonce[8] = {1, 2, 3, 4, 5, 6, 7, 8}, b[64];
  size_t n = Msg::makeSetRequest(b, key(), keyLen, nonce, newTime);
  CHECK_EQ(Device::status(d.send(Bytes(b, b + n))), Msg::Rejected);
  CHECK_EQ(d.clock.getTimeMs(), 1609459200000ULL);
}

TEST(a_request_cannot_be_replayed) {
  Device d;
  Bytes request = d.setRequest(d.challenge(), newTime);
  CHECK_EQ(Device::status(d.send(request)), Msg::Ok);
  d.clock.setTime(1609459200);  // the time is wrong again
  CHECK_EQ(Device::status(d.send(request)), Msg::Rejected);  // replay
  CHECK_EQ(d.clock.getTimeMs(), 1609459200000ULL);
  CHECK_EQ(d.server.getTimeSetCount(), 1u);
}

TEST(a_nonce_with_a_fresh_signature_is_used_up) {
  Device d;
  Bytes c = d.challenge();
  CHECK_EQ(Device::status(d.send(d.setRequest(c, newTime))), Msg::Ok);
  // even a correct signature does not help: the nonce was used already
  CHECK_EQ(Device::status(d.send(d.setRequest(c, newTime + 1000))),
           Msg::Rejected);
}

TEST(only_the_last_nonce_is_valid) {
  Device d;
  Bytes first = d.challenge();
  Bytes second = d.challenge();
  CHECK_EQ(Device::status(d.send(d.setRequest(first, newTime))), Msg::Rejected);
  CHECK_EQ(Device::status(d.send(d.setRequest(second, newTime))), Msg::Ok);
}

TEST(every_changed_byte_is_rejected_and_does_not_use_up_the_nonce) {
  Device d;
  Bytes good = d.setRequest(d.challenge(), newTime);
  for (size_t i = 1; i < good.size(); i++) {
    Bytes bad = good;
    bad[i] ^= 0x01;
    // byte 1 is the type: it becomes the type of a server message, which is
    // ignored. All other bytes are protected by the signature.
    CHECK_EQ(Device::status(d.send(bad)), i == 1 ? 255 : Msg::Rejected);
  }
  CHECK_EQ(d.clock.getTimeMs(), 1609459200000ULL);
  // an attacker cannot block a valid request with bad ones
  CHECK_EQ(Device::status(d.send(good)), Msg::Ok);
}

TEST(nonce_expires_after_10_seconds) {
  Device d;
  Bytes c = d.challenge();
  mock::now += 9999;
  Bytes request = d.setRequest(c, newTime);
  CHECK_EQ(Device::status(d.send(request)), Msg::Ok);

  Device e;
  Bytes c2 = e.challenge();
  mock::now += 10000;
  CHECK_EQ(Device::status(e.send(e.setRequest(c2, newTime))), Msg::Rejected);
  // a new challenge works again
  Bytes c3 = e.challenge();
  CHECK_EQ(Device::status(e.send(e.setRequest(c3, newTime))), Msg::Ok);
}

TEST(nonce_expiry_works_when_millis_overflows) {
  Device d;
  mock::now = 0xFFFFF000u;
  Bytes c = d.challenge();
  mock::now = 0x00000800u;  // 6.1 seconds later, after the overflow
  CHECK_EQ(Device::status(d.send(d.setRequest(c, newTime))), Msg::Ok);
  Device e;
  mock::now = 0xFFFFF000u;
  Bytes c2 = e.challenge();
  mock::now = 0x00003000u;  // 16 seconds later
  CHECK_EQ(Device::status(e.send(e.setRequest(c2, newTime))), Msg::Rejected);
}

TEST(times_before_2020_are_refused_and_the_nonce_is_used_up) {
  Device d;
  Bytes c = d.challenge();
  CHECK_EQ(Device::status(d.send(d.setRequest(c, 1000000000000ULL))),
           Msg::InvalidTime);
  CHECK_EQ(d.clock.getTimeMs(), 1609459200000ULL);
  CHECK_EQ(Device::status(d.send(d.setRequest(c, newTime))), Msg::Rejected);
  CHECK_EQ(Device::status(d.send(d.setRequest(d.challenge(), 0))),
           Msg::InvalidTime);
  // the first valid time
  CHECK_EQ(Device::status(d.send(d.setRequest(d.challenge(), 1577836800000ULL))),
           Msg::Ok);
  CHECK_EQ(Device::status(d.send(d.setRequest(d.challenge(), 1577836799999ULL))),
           Msg::InvalidTime);
}

struct FixedSource : TinyTimeSource {
  uint64_t getTimeMs() override { return 1800000000000ULL; }
};
struct RefusingSource : TinyClockTimeSource {
  bool refuse = false;
  bool setTimeMs(uint64_t ms) override {
    return refuse ? false : TinyClockTimeSource::setTimeMs(ms);
  }
};

TEST(source_that_cannot_be_set) {
  mock::reset();
  MockUDP::reset();
  FixedSource source;
  Server s;
  s.begin(source);
  s.enableTimeSetting(secret);
  MockUDP::deliver(Bytes{0x27, 1}, 1, 1);
  CHECK(s.process());
  Bytes c = MockUDP::sent.back().data;
  uint8_t nonce[8], b[64];
  CHECK(Msg::parseChallengeResponse(c.data(), c.size(), nonce));
  size_t n = Msg::makeSetRequest(b, key(), keyLen, nonce, newTime);
  MockUDP::deliver(Bytes(b, b + n), 1, 1);
  CHECK(s.process());
  CHECK_EQ(Device::status(MockUDP::sent.back().data), Msg::NotSupported);
  CHECK_EQ(s.getTimeSetCount(), 0u);
}

TEST(source_that_refuses_to_be_set) {
  mock::reset();
  MockUDP::reset();
  RefusingSource source;
  source.setTime(1609459200);
  source.refuse = true;
  Server s;
  s.begin(source);
  s.enableTimeSetting(secret);
  MockUDP::deliver(Bytes{0x27, 1}, 1, 1);
  s.process();
  uint8_t nonce[8], b[64];
  Msg::parseChallengeResponse(MockUDP::sent.back().data.data(), 10, nonce);
  size_t n = Msg::makeSetRequest(b, key(), keyLen, nonce, newTime);
  MockUDP::deliver(Bytes(b, b + n), 1, 1);
  CHECK(s.process());
  CHECK_EQ(Device::status(MockUDP::sent.back().data), Msg::NotSupported);
  CHECK_EQ(source.getTimeMs(), 1609459200000ULL);
}

TEST(time_setting_is_off_by_default) {
  mock::reset();
  MockUDP::reset();
  TinyClockTimeSource clock;
  clock.setTime(1609459200);
  Server s;
  s.begin(clock);  // no enableTimeSetting()
  MockUDP::deliver(Bytes{0x27, 1});
  CHECK(!s.process());
  CHECK(MockUDP::sent.empty());  // not even a challenge
  uint8_t nonce[8] = {}, b[64];
  size_t n = Msg::makeSetRequest(b, key(), keyLen, nonce, newTime);
  MockUDP::deliver(Bytes(b, b + n));
  CHECK(!s.process());
  CHECK(MockUDP::sent.empty());
  CHECK_EQ(clock.getTimeMs(), 1609459200000ULL);
}

TEST(time_setting_can_be_disabled_again) {
  Device d;
  CHECK(!d.challenge().empty());
  d.server.disableTimeSetting();
  CHECK(d.challenge().empty());
  d.server.enableTimeSetting(secret);
  CHECK(!d.challenge().empty());
}

TEST(an_empty_secret_does_not_enable_it) {
  Device d;
  d.server.enableTimeSetting("");
  CHECK(d.challenge().empty());
  d.server.enableTimeSetting(nullptr);
  CHECK(d.challenge().empty());
}

TEST(begin_resets_the_counter) {
  Device d;
  d.send(d.setRequest(d.challenge(), newTime));
  CHECK_EQ(d.server.getTimeSetCount(), 1u);
  d.server.begin(d.clock);
  CHECK_EQ(d.server.getTimeSetCount(), 0u);
}

TEST(garbage_gets_no_answer) {
  Device d;
  Bytes valid = d.setRequest(d.challenge(), newTime);
  size_t sent = MockUDP::sent.size();
  bool handled = true;
  d.send(Bytes{0x27}, &handled);        // too short for a message
  CHECK(!handled);
  d.send(Bytes{0x27, 9, 1, 2, 3}, &handled);  // unknown type
  CHECK(!handled);
  d.send(Bytes{0x27, Msg::ChallengeResponse, 1, 2, 3, 4, 5, 6, 7, 8}, &handled);
  CHECK(!handled);  // messages of a server are not accepted
  d.send(Bytes{0x27, Msg::SetResponse, 0, 0, 0, 0, 0, 0, 0, 0, 0}, &handled);
  CHECK(!handled);
  CHECK_EQ(MockUDP::sent.size(), sent);
  // a truncated set request is answered with "rejected", never accepted
  for (size_t len = 2; len < valid.size(); len++) {
    Bytes cut(valid.begin(), valid.begin() + len);
    CHECK_EQ(Device::status(d.send(cut)), Msg::Rejected);
  }
  CHECK_EQ(d.clock.getTimeMs(), 1609459200000ULL);
  CHECK_EQ(Device::status(d.send(valid)), Msg::Ok);  // still valid
}

TEST(ntp_requests_still_work_with_time_setting) {
  Device d;
  MockUDP::deliver(ntpRequest());
  CHECK(d.server.process());
  CHECK_EQ(MockUDP::sent.back().data.size(), 48u);
  CHECK_EQ(MockUDP::sent.back().data[0], 0x24);
  CHECK_EQ(d.server.getRequestCount(), 1u);
}

TEST(a_long_secret_works) {
  mock::reset();
  MockUDP::reset();
  std::string longSecret(200, 'k');  // longer than the block size of SHA-256
  TinyClockTimeSource clock;
  clock.setTime(1609459200);
  Server s;
  s.begin(clock);
  s.enableTimeSetting(longSecret.c_str());
  MockUDP::deliver(Bytes{0x27, 1}, 1, 1);
  s.process();
  uint8_t nonce[8], b[64];
  Msg::parseChallengeResponse(MockUDP::sent.back().data.data(), 10, nonce);
  size_t n = Msg::makeSetRequest(
      b, reinterpret_cast<const uint8_t*>(longSecret.data()), 200, nonce,
      newTime);
  MockUDP::deliver(Bytes(b, b + n), 1, 1);
  CHECK(s.process());
  CHECK_EQ(Device::status(MockUDP::sent.back().data), Msg::Ok);
  CHECK_EQ(clock.getTimeMs(), newTime);
}

TEST_MAIN("timesetting")

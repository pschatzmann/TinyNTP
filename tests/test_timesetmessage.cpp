// Tests of the messages that set the time of a server (TinyTimeSetMessage)
#include "TinyNTP/TinyTimeSet.h"
#include "test.h"

using namespace tiny_ntp;
using Msg = TinyTimeSetMessage;

static const uint8_t key[] = "secret";
static const size_t keyLen = 6;

TEST(sizes_and_header) {
  uint8_t b[64];
  CHECK_EQ(Msg::makeChallengeRequest(b), 2u);
  CHECK_EQ(b[0], 0x27);  // version 4, mode 7
  CHECK_EQ(b[1], 1);
  CHECK(Msg::isPrivate(b, 2));
  CHECK(!Msg::isPrivate(b, 1));
  uint8_t nonce[8] = {1, 2, 3, 4, 5, 6, 7, 8};
  CHECK_EQ(Msg::makeChallengeResponse(b, nonce), 10u);
  CHECK_EQ(Msg::makeSetRequest(b, key, keyLen, nonce, 1800000000000ULL), 50u);
  CHECK_EQ(Msg::makeSetResponse(b, Msg::Ok, 1), 11u);
}

TEST(ntp_packets_are_not_private) {
  uint8_t ntp[48] = {0x23};  // version 4, mode 3 (client)
  CHECK(!Msg::isPrivate(ntp, 48));
  ntp[0] = 0x24;  // mode 4 (server)
  CHECK(!Msg::isPrivate(ntp, 48));
  ntp[0] = 0xe7;  // other LI and version, mode 7
  CHECK(Msg::isPrivate(ntp, 48));
}

TEST(challenge_response_roundtrip) {
  uint8_t out[16], nonce[8] = {9, 8, 7, 6, 5, 4, 3, 2}, got[8] = {};
  size_t n = Msg::makeChallengeResponse(out, nonce);
  CHECK(Msg::parseChallengeResponse(out, n, got));
  CHECK(memcmp(nonce, got, 8) == 0);
  CHECK(!Msg::parseChallengeResponse(out, n - 1, got));  // too short
  out[1] = Msg::SetRequest;
  CHECK(!Msg::parseChallengeResponse(out, n, got));  // wrong type
}

TEST(set_request_verifies_with_the_right_secret_only) {
  uint8_t nonce[8] = {1, 2, 3, 4, 5, 6, 7, 8}, req[64];
  size_t n = Msg::makeSetRequest(req, key, keyLen, nonce, 1800000123456ULL);
  uint64_t t = 0;
  CHECK(Msg::verifySetRequest(req, n, key, keyLen, nonce, t));
  CHECK_EQ(t, 1800000123456ULL);
  const uint8_t other[] = "Secret";
  t = 0;
  CHECK(!Msg::verifySetRequest(req, n, other, 6, nonce, t));
  CHECK_EQ(t, 0u);  // no time is returned for an invalid request
  CHECK(!Msg::verifySetRequest(req, n, key, keyLen - 1, nonce, t));
  uint8_t otherNonce[8] = {1, 2, 3, 4, 5, 6, 7, 9};
  CHECK(!Msg::verifySetRequest(req, n, key, keyLen, otherNonce, t));
}

TEST(every_byte_of_a_set_request_is_protected) {
  uint8_t nonce[8] = {1, 2, 3, 4, 5, 6, 7, 8}, req[64];
  size_t n = Msg::makeSetRequest(req, key, keyLen, nonce, 1800000000000ULL);
  for (size_t i = 1; i < n; i++) {  // byte 0 only says: private message
    for (int bit = 0; bit < 8; bit++) {
      uint8_t copy[64];
      memcpy(copy, req, n);
      copy[i] ^= 1 << bit;
      uint64_t t;
      CHECK(!Msg::verifySetRequest(copy, n, key, keyLen, nonce, t));
    }
  }
}

TEST(truncated_and_wrong_messages_are_refused) {
  uint8_t nonce[8] = {}, req[64];
  size_t n = Msg::makeSetRequest(req, key, keyLen, nonce, 1800000000000ULL);
  uint64_t t;
  for (size_t len = 0; len < n; len++) {
    CHECK(!Msg::verifySetRequest(req, len, key, keyLen, nonce, t));
  }
  req[1] = Msg::ChallengeRequest;
  CHECK(!Msg::verifySetRequest(req, n, key, keyLen, nonce, t));
}

TEST(time_is_big_endian) {
  uint8_t nonce[8] = {}, req[64];
  Msg::makeSetRequest(req, key, keyLen, nonce, 0x0102030405060708ULL);
  for (int i = 0; i < 8; i++) CHECK_EQ(req[10 + i], i + 1);
}

TEST(set_response_roundtrip) {
  uint8_t out[16];
  Msg::Status s;
  uint64_t t;
  for (int st = 0; st <= 3; st++) {
    size_t n = Msg::makeSetResponse(out, static_cast<Msg::Status>(st),
                                    1800000000999ULL);
    CHECK(Msg::parseSetResponse(out, n, s, t));
    CHECK_EQ(static_cast<int>(s), st);
    CHECK_EQ(t, 1800000000999ULL);
  }
  CHECK(!Msg::parseSetResponse(out, 10, s, t));
  out[1] = Msg::ChallengeResponse;
  CHECK(!Msg::parseSetResponse(out, 11, s, t));
}

TEST(nonce_depends_on_all_inputs) {
  uint8_t base[8], n[8];
  Msg::makeNonce(key, keyLen, 1, 1000, 5, base);
  Msg::makeNonce(key, keyLen, 1, 1000, 5, n);
  CHECK(memcmp(base, n, 8) == 0);  // deterministic
  Msg::makeNonce(key, keyLen, 2, 1000, 5, n);
  CHECK(memcmp(base, n, 8) != 0);
  Msg::makeNonce(key, keyLen, 1, 1001, 5, n);
  CHECK(memcmp(base, n, 8) != 0);
  Msg::makeNonce(key, keyLen, 1, 1000, 6, n);
  CHECK(memcmp(base, n, 8) != 0);
  const uint8_t other[] = "secreT";
  Msg::makeNonce(other, 6, 1, 1000, 5, n);
  CHECK(memcmp(base, n, 8) != 0);  // cannot be predicted without the secret
}

TEST_MAIN("timesetmessage")

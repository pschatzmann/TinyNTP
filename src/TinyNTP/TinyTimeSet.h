/**
 * @file TinyTimeSet.h
 * @brief Messages to set the time of a TinyNTPServer over the network.
 */

#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "TinySHA256.h"

namespace tiny_ntp {

/**
 * @class TinyTimeSetMessage
 * @brief The messages of the protocol that sets the time of a TinyNTPServer
 * (e.g. with the command line tool `tinyntp set`).
 *
 * NTP has no way to set the time of a server, so the messages use the NTP
 * mode 7 (reserved for private use) on the port of the server. A normal NTP
 * client never sends them. The messages are authenticated with HMAC-SHA256 and
 * a shared secret; the secret itself is never sent:
 *
 * 1. Challenge request (client): `header, type=1`
 * 2. Challenge response (server): `header, type=2, nonce[8]`
 * 3. Set request (client): `header, type=3, nonce[8], timeMs[8],
 *    hmac[32]` where hmac = HMAC-SHA256(secret, type, nonce, timeMs)
 * 4. Set response (server): `header, type=4, status, timeMs[8]` (the time of
 *    the server after the request)
 *
 * The server accepts a nonce only once and for a short time, so a recorded
 * request cannot be replayed. All numbers are big endian.
 */
class TinyTimeSetMessage {
 public:
  /** First byte of all messages: LI=0, version 4, mode 7 (private use). */
  static constexpr uint8_t Header = 0x27;
  /** Type of a message. */
  enum Type : uint8_t {
    ChallengeRequest = 1,
    ChallengeResponse = 2,
    SetRequest = 3,
    SetResponse = 4
  };
  /** Result of a set request. */
  enum Status : uint8_t {
    Ok = 0,
    Rejected = 1,      ///< wrong secret, unknown or expired nonce
    NotSupported = 2,  ///< the time source cannot be set
    InvalidTime = 3    ///< the time is before 2020
  };
  /** Size of the nonce in bytes. */
  static constexpr size_t NonceSize = 8;
  /** Size of a challenge request in bytes. */
  static constexpr size_t ChallengeRequestSize = 2;
  /** Size of a challenge response in bytes. */
  static constexpr size_t ChallengeResponseSize = 2 + NonceSize;
  /** Size of a set request in bytes. */
  static constexpr size_t SetRequestSize = 2 + NonceSize + 8 + 32;
  /** Size of a set response in bytes. */
  static constexpr size_t SetResponseSize = 3 + 8;
  /** Smallest time that is accepted: 2020-01-01 in ms. */
  static constexpr uint64_t MinValidTimeMs = 1577836800000ULL;

  /** @brief Returns true if the message is in the private mode (7). */
  static bool isPrivate(const uint8_t* msg, size_t len) {
    return len >= 2 && (msg[0] & 0x07) == 7;
  }

  /** @brief Get the type of a message in the private mode. */
  static uint8_t type(const uint8_t* msg) { return msg[1]; }

  /** @brief Write a challenge request. Returns its size. */
  static size_t makeChallengeRequest(uint8_t* out) {
    out[0] = Header;
    out[1] = ChallengeRequest;
    return ChallengeRequestSize;
  }

  /** @brief Write a challenge response. Returns its size. */
  static size_t makeChallengeResponse(uint8_t* out, const uint8_t* nonce) {
    out[0] = Header;
    out[1] = ChallengeResponse;
    memcpy(out + 2, nonce, NonceSize);
    return ChallengeResponseSize;
  }

  /** @brief Read the nonce of a challenge response. */
  static bool parseChallengeResponse(const uint8_t* msg, size_t len,
                                     uint8_t* nonce) {
    if (len < ChallengeResponseSize || !isPrivate(msg, len) ||
        msg[1] != ChallengeResponse) {
      return false;
    }
    memcpy(nonce, msg + 2, NonceSize);
    return true;
  }

  /**
   * @brief Create a nonce that cannot be predicted without the secret: the
   * start of the HMAC of a counter and the current time.
   */
  static void makeNonce(const uint8_t* key, size_t keyLen, uint32_t counter,
                        uint64_t timeMs, uint32_t millis, uint8_t* nonce) {
    uint8_t data[16];
    put32(data, counter);
    put64(data + 4, timeMs);
    put32(data + 12, millis);
    uint8_t mac[TinySHA256::DigestSize];
    TinyHMACSHA256::calculate(key, keyLen, data, sizeof data, mac);
    memcpy(nonce, mac, NonceSize);
  }

  /** @brief Write a set request. Returns its size. */
  static size_t makeSetRequest(uint8_t* out, const uint8_t* key, size_t keyLen,
                               const uint8_t* nonce, uint64_t timeMs) {
    out[0] = Header;
    out[1] = SetRequest;
    memcpy(out + 2, nonce, NonceSize);
    put64(out + 2 + NonceSize, timeMs);
    // the HMAC covers the type, the nonce and the time
    TinyHMACSHA256::calculate(key, keyLen, out + 1, 1 + NonceSize + 8,
                              out + 2 + NonceSize + 8);
    return SetRequestSize;
  }

  /**
   * @brief Check a set request.
   * @param msg The received message.
   * @param len Size of the message in bytes.
   * @param key The shared secret.
   * @param keyLen Length of the secret in bytes.
   * @param nonce The nonce that the server has issued.
   * @param timeMs Receives the time of the request.
   * @return true if the message is valid, from a client with the secret and
   * for this nonce.
   */
  static bool verifySetRequest(const uint8_t* msg, size_t len,
                               const uint8_t* key, size_t keyLen,
                               const uint8_t* nonce, uint64_t& timeMs) {
    if (len < SetRequestSize || !isPrivate(msg, len) || msg[1] != SetRequest) {
      return false;
    }
    uint8_t mac[TinySHA256::DigestSize];
    TinyHMACSHA256::calculate(key, keyLen, msg + 1, 1 + NonceSize + 8, mac);
    bool ok = TinyHMACSHA256::equals(mac, msg + 2 + NonceSize + 8,
                                     TinySHA256::DigestSize);
    ok = TinyHMACSHA256::equals(nonce, msg + 2, NonceSize) && ok;
    if (ok) timeMs = get64(msg + 2 + NonceSize);
    return ok;
  }

  /** @brief Write a set response. Returns its size. */
  static size_t makeSetResponse(uint8_t* out, Status status, uint64_t timeMs) {
    out[0] = Header;
    out[1] = SetResponse;
    out[2] = status;
    put64(out + 3, timeMs);
    return SetResponseSize;
  }

  /** @brief Read a set response. */
  static bool parseSetResponse(const uint8_t* msg, size_t len, Status& status,
                               uint64_t& timeMs) {
    if (len < SetResponseSize || !isPrivate(msg, len) ||
        msg[1] != SetResponse) {
      return false;
    }
    status = static_cast<Status>(msg[2]);
    timeMs = get64(msg + 3);
    return true;
  }

 protected:
  /** Write a 32 bit number (big endian). */
  static void put32(uint8_t* out, uint32_t v) {
    for (int i = 0; i < 4; i++) out[i] = v >> (24 - 8 * i);
  }
  /** Write a 64 bit number (big endian). */
  static void put64(uint8_t* out, uint64_t v) {
    for (int i = 0; i < 8; i++) out[i] = v >> (56 - 8 * i);
  }
  /** Read a 64 bit number (big endian). */
  static uint64_t get64(const uint8_t* in) {
    uint64_t v = 0;
    for (int i = 0; i < 8; i++) v = (v << 8) | in[i];
    return v;
  }
};

}  // namespace tiny_ntp

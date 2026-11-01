/**
 * @file TinyNTPServer.h
 * @brief Simple NTP server for Arduino using a generic UDP class (e.g.
 * WiFiUDP, EthernetUDP). It answers the requests of NTP clients with the time
 * of a time source, e.g. a TinyNTPClient.
 */

#pragma once
#include <cstdint>
#include <cstring>

#include "TinyTimeSet.h"
#include "TinyTimeSource.h"

namespace tiny_ntp {

/**
 * @class TinyNTPServer
 * @brief A minimal Network Time Protocol (NTP) server (RFC 5905, mode 4) that
 * serves the time of a time source to the clients in your network.
 *
 * The time comes from a TinyTimeSource, e.g. a TinyNTPClient (without a time
 * offset or time zone), a TinySystemTimeSource or a TinyClockTimeSource. While
 * the time source returns 0 (time not known), requests are not answered.
 *
 * The server can optionally allow to set the time of the time source over the
 * network, see enableTimeSetting().
 *
 * The server does not block: call process() in your loop(). It does not
 * limit the request rate, so do not make it reachable from the internet.
 *
 * If the same device also runs a TinyNTPClient, the client must not use port
 * 123 as local port: call TinyNTPClient::setLocalPort().
 *
 * @tparam UDPAPI The UDP class to use (e.g: WiFiUDP)
 */
template <typename UDPAPI>
class TinyNTPServer {
 public:
  TinyNTPServer() = default;

  /**
   * @brief Start listening for requests.
   * @param source The time source. It must stay valid as long as the server
   * is used.
   * @param port UDP port (default: 123).
   * @return true if the UDP socket could be opened.
   */
  bool begin(TinyTimeSource& source, uint16_t port = 123) {
    _source = &source;
    _requests = 0;
    _timeSetCount = 0;
    return _udp.begin(port) != 0;
  }

  /** @brief Stop listening. */
  void end() {
    _udp.stop();
    _source = nullptr;
  }

  /**
   * @brief Answer a pending request. Call this regularly in loop().
   * @return true if a request was answered.
   */
  bool process() {
    if (_source == nullptr) return false;
    int size = _udp.parsePacket();
    if (size <= 0) return false;

    // the time when the request was received
    uint64_t rxMs = _source->getTimeMs();
    uint8_t request[MaxRequestSize];
    int total = 0;
    while (total < size && total < MaxRequestSize) {
      int len = _udp.read(request + total, MaxRequestSize - total);
      if (len <= 0) break;
      total += len;
    }
    // mode 7: request to set the time
    if (TinyTimeSetMessage::isPrivate(request, total)) {
      return processTimeSet(request, total);
    }
    if (total < PacketSize) return false;
    if ((request[0] & 0x07) != 3) return false;  // only client requests
    if (rxMs == 0) return false;                  // time not known yet

    uint8_t version = (request[0] >> 3) & 0x07;
    if (version < 1 || version > 4) version = 4;

    uint8_t response[PacketSize] = {};
    response[0] = (version << 3) | 4;  // LI=0, VN=version, Mode=4 (server)
    response[1] = _stratum;
    response[2] = request[2];                   // poll
    response[3] = static_cast<uint8_t>(-10);    // precision: 2^-10 s (~1 ms)
    putUint32(response + 8, 0x00000400);        // root dispersion: ~15 ms
    putTimestamp(response + 16, rxMs);          // reference time
    for (int i = 0; i < 8; i++) {
      response[24 + i] = request[40 + i];  // originate = client transmit time
    }
    putTimestamp(response + 32, rxMs);                  // receive time
    putTimestamp(response + 40, _source->getTimeMs());   // transmit time

    auto ip = _udp.remoteIP();
    auto port = _udp.remotePort();
    _udp.beginPacket(ip, port);
    _udp.write(response, PacketSize);
    _udp.endPacket();
    _requests++;
    return true;
  }

  /**
   * @brief Set the stratum that is reported to the clients: 1 for a server
   * with a reference clock (e.g. GPS), otherwise the stratum of the upstream
   * server plus one. Default: 3.
   */
  void setStratum(uint8_t stratum) { _stratum = stratum; }

  /**
   * @brief Allow to set the time of the time source over the network, e.g.
   * with the command line tool `tinyntp set`. The requests are authenticated
   * with HMAC-SHA256 and the secret, which is never sent over the network,
   * and cannot be replayed. It is off by default and needs a time source that
   * implements TinyTimeSource::setTimeMs() (e.g. TinyClockTimeSource).
   *
   * Keep in mind that everybody who knows the secret can change the time that
   * you serve: use this in a trusted network only.
   * @param secret The shared secret (a long random text). The text is not
   * copied: it must stay valid as long as the server is used.
   */
  void enableTimeSetting(const char* secret) {
    _secret = reinterpret_cast<const uint8_t*>(secret);
    _secretLen = secret != nullptr ? strlen(secret) : 0;
    _nonceValid = false;
  }

  /** @brief Do not allow to set the time (default). */
  void disableTimeSetting() {
    _secret = nullptr;
    _secretLen = 0;
    _nonceValid = false;
  }

  /** @brief Number of times that the time was set since begin(). */
  uint32_t getTimeSetCount() const { return _timeSetCount; }

  /** @brief Number of requests that were answered since begin(). */
  uint32_t getRequestCount() const { return _requests; }

  /** @brief Get a reference to the UDP API. */
  UDPAPI& getUDP() { return _udp; }

 protected:
  /** Size of an NTP packet in bytes. */
  static constexpr int PacketSize = 48;
  /** Largest request that is read. */
  static constexpr int MaxRequestSize = 64;
  /** Time in ms that a nonce can be used. */
  static constexpr uint32_t NonceLifetimeMs = 10000;
  /** Seconds between 1900-01-01 (NTP epoch) and 1970-01-01 (Unix epoch). */
  static constexpr uint32_t NtpEpochOffset = 2208988800UL;

  /** The UDP API (e.g., WiFiUDP, EthernetUDP). */
  UDPAPI _udp;
  /** The source of the time. */
  TinyTimeSource* _source = nullptr;
  /** Stratum that is reported to the clients. */
  uint8_t _stratum = 3;
  /** Number of answered requests. */
  uint32_t _requests = 0;
  /** The secret to set the time (nullptr: not allowed). */
  const uint8_t* _secret = nullptr;
  /** Length of the secret. */
  size_t _secretLen = 0;
  /** The nonce that was issued last. */
  uint8_t _nonce[TinyTimeSetMessage::NonceSize] = {};
  /** True if the nonce can be used. */
  bool _nonceValid = false;
  /** The value of millis() when the nonce was issued. */
  uint32_t _nonceMillis = 0;
  /** Counter for the nonces. */
  uint32_t _nonceCounter = 0;
  /** Number of times that the time was set. */
  uint32_t _timeSetCount = 0;

  /** Send a message to the sender of the last request. */
  void reply(const uint8_t* message, size_t size) {
    auto ip = _udp.remoteIP();
    auto port = _udp.remotePort();
    _udp.beginPacket(ip, port);
    _udp.write(message, size);
    _udp.endPacket();
  }

  /** Handle a challenge request or a set request. */
  bool processTimeSet(const uint8_t* request, int size) {
    if (_secretLen == 0) return false;  // not allowed
    uint8_t response[TinyTimeSetMessage::SetRequestSize];
    if (TinyTimeSetMessage::type(request) ==
        TinyTimeSetMessage::ChallengeRequest) {
      // a nonce that nobody can predict without the secret
      uint32_t now = ::millis();
      TinyTimeSetMessage::makeNonce(_secret, _secretLen, ++_nonceCounter,
                                    _source->getTimeMs(), now, _nonce);
      _nonceValid = true;
      _nonceMillis = now;
      reply(response,
            TinyTimeSetMessage::makeChallengeResponse(response, _nonce));
      return true;
    }
    if (TinyTimeSetMessage::type(request) != TinyTimeSetMessage::SetRequest) {
      return false;
    }
    TinyTimeSetMessage::Status status = TinyTimeSetMessage::Rejected;
    uint64_t timeMs = 0;
    // 32 bit arithmetic: also correct after the overflow of millis(), and
    // if millis() returns a 64 bit value (e.g. on a computer)
    bool fresh = _nonceValid && static_cast<uint32_t>(::millis() - _nonceMillis) <
                                    NonceLifetimeMs;
    // a failed attempt keeps the nonce: others cannot block a valid request
    if (fresh && TinyTimeSetMessage::verifySetRequest(
                     request, size, _secret, _secretLen, _nonce, timeMs)) {
      _nonceValid = false;  // use it only once
      if (timeMs < TinyTimeSetMessage::MinValidTimeMs) {
        status = TinyTimeSetMessage::InvalidTime;
      } else if (!_source->setTimeMs(timeMs)) {
        status = TinyTimeSetMessage::NotSupported;
      } else {
        status = TinyTimeSetMessage::Ok;
        _timeSetCount++;
      }
    }
    reply(response, TinyTimeSetMessage::makeSetResponse(
                        response, status, _source->getTimeMs()));
    return true;
  }

  /** Write a 32 bit value in network byte order (big endian). */
  static void putUint32(uint8_t* out, uint32_t value) {
    out[0] = value >> 24;
    out[1] = value >> 16;
    out[2] = value >> 8;
    out[3] = value;
  }

  /** Write a Unix time in ms as 64 bit NTP timestamp (seconds and fraction). */
  static void putTimestamp(uint8_t* out, uint64_t unixMs) {
    uint32_t seconds = static_cast<uint32_t>(unixMs / 1000) + NtpEpochOffset;
    uint32_t fraction = static_cast<uint32_t>(
        (static_cast<uint64_t>(unixMs % 1000) << 32) / 1000);
    putUint32(out, seconds);
    putUint32(out + 4, fraction);
  }
};

}  // namespace tiny_ntp

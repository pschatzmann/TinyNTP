/**
 * @file TinyNTPServer.h
 * @brief Simple NTP server for Arduino using a generic UDP class (e.g.
 * WiFiUDP, EthernetUDP). It answers the requests of NTP clients with the time
 * of a time source, e.g. a TinyNTPClient.
 */

#pragma once
#include <cstdint>

/**
 * @class TinyNTPServer
 * @brief A minimal Network Time Protocol (NTP) server (RFC 5905, mode 4) that
 * serves the time of a time source to the clients in your network.
 *
 * The time source is any object with a `uint64_t getTimeMs()` method that
 * returns the UTC time in milliseconds since the Unix epoch and 0 as long as
 * the time is not known, e.g. a TinyNTPClient (without a time offset or time
 * zone). While the time is not known, requests are not answered.
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
   * @param source Time source with `uint64_t getTimeMs()`. It must stay valid
   * as long as the server is used.
   * @param port UDP port (default: 123).
   * @return true if the UDP socket could be opened.
   */
  template <typename TimeSource>
  bool begin(TimeSource& source, uint16_t port = 123) {
    _source = &source;
    _getTimeMs = [](void* s) -> uint64_t {
      return static_cast<TimeSource*>(s)->getTimeMs();
    };
    _requests = 0;
    return _udp.begin(port) != 0;
  }

  /** @brief Stop listening. */
  void end() {
    _udp.stop();
    _source = nullptr;
    _getTimeMs = nullptr;
  }

  /**
   * @brief Answer a pending request. Call this regularly in loop().
   * @return true if a request was answered.
   */
  bool process() {
    if (_getTimeMs == nullptr) return false;
    int size = _udp.parsePacket();
    if (size < PacketSize) return false;

    // the time when the request was received
    uint64_t rxMs = _getTimeMs(_source);
    uint8_t request[PacketSize];
    int total = 0;
    while (total < PacketSize) {
      int len = _udp.read(request + total, PacketSize - total);
      if (len <= 0) break;
      total += len;
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
    putTimestamp(response + 40, _getTimeMs(_source));   // transmit time

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

  /** @brief Number of requests that were answered since begin(). */
  uint32_t getRequestCount() const { return _requests; }

  /** @brief Get a reference to the UDP API. */
  UDPAPI& getUDP() { return _udp; }

 protected:
  static constexpr int PacketSize = 48;
  /** Seconds between 1900-01-01 (NTP epoch) and 1970-01-01 (Unix epoch). */
  static constexpr uint32_t NtpEpochOffset = 2208988800UL;

  UDPAPI _udp;
  void* _source = nullptr;
  uint64_t (*_getTimeMs)(void*) = nullptr;
  uint8_t _stratum = 3;
  uint32_t _requests = 0;

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

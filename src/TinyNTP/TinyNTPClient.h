

/**
 * @file TinyNTPClient.h
 * @brief Simple NTP client for Arduino using a generic UDP Client (e.g.,
 * WiFiUdp, EthernetUdp). It provides the UTC time and, with a POSIX TZ string,
 * the local time including summer/winter time. The client is also a
 * TinyTimeSource for the TinyNTPServer.
 */

#pragma once
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <ctime>

#include "TinyTimeSource.h"
#include "TinyTimeZone.h"
#include "TinyNTPServers.h"
#include "TinyTimeZones.h"

namespace tiny_ntp {

/**
 * @class TinyNTPClient
 * @brief A Network Time Protocol (NTP) client for retrieving the current
 * date/time from an NTP server using an Arduino UDP API. The time of the
 * server is corrected by half of the network delay, and between the updates the
 * time is calculated with millis(), corrected by the measured speed of
 * millis() (see getMillisCorrectionFactor()).
 *
 * The client provides the UTC time (getTimeMs(), getTimeSec(), getTm()) and,
 * with a POSIX TZ string, the local time including summer/winter time
 * (setTimeZone(), getLocalTm()). It is also a TinyTimeSource, so a
 * TinyNTPServer can serve its time.
 *
 * The functions block until the server has answered or the timeout has
 * passed. Without a response, the request is sent again every 300 ms (some
 * WiFi networks lose responses or hold them back until the device sends). Call update() from time to time (e.g. once per hour).
 * @tparam UDPAPI The UDP class to use (e.g: WiFiUDP)
 */
template <typename UDPAPI>
class TinyNTPClient : public TinyTimeSource {
 public:
  /**
   * @brief Construct a new TinyNTPClient object.
   * @param server NTP server hostname or IP address. The text is not copied:
   * it must stay valid as long as the client is used (e.g. a string literal
   * or one of the NTP_HOST_* / NTP_IP_* defines).
   * @param port NTP server port (default: 123).
   * @param timeoutMs Time to wait for the response of the server in ms.
   */
  TinyNTPClient(const char* server = NTP_HOST_POOL, int port = 123,
                uint32_t timeoutMs = 6000)
      : _server(server), _port(port), _timeoutMs(timeoutMs) {}

  /**
   * @brief Initialize the NTP client and perform the first time update (two
   * requests to the server). This calls end() first, so a time offset that was
   * set before is cleared: set it after begin().
   * @return true if the update was successful, false otherwise.
   */
  bool begin() {
    end();
    return update();
  }

  /**
   * @brief Reset the client state: the time, the time offset and the correction
   * factor are cleared and the UDP socket is closed. The time zone that was set
   * with setTimeZone() is kept.
   */
  void end() {
    _timeOffsetSeconds = 0;
    _lastNtpTime = 0;
    _lastNtpTimeMs = 0;
    _hasTime = false;
    _hasBaseline = false;
    _lastUpdateMillis = 0;
    _baselineNtpTime = 0;
    _baselineNtpTimeMs = 0;
    _baselineUpdateMillis = 0;
    _millisCorrectionFactor = 1.0;
    _udp.stop();
    _openPort = 0;
  }

  /**
   * @brief Get the current time in seconds since Unix epoch (UTC, plus the
   * time offset if one was set).
   * @return Current time in seconds, or 0 as long as the time is not known.
   */
  uint32_t getTimeSec() {
    // Return time in seconds from getTimeMs()
    return static_cast<uint32_t>(getTimeMs() / 1000ULL);
  }

  /**
   * @brief Get the current time in milliseconds since Unix epoch (UTC, plus
   * the time offset if one was set).
   * @return Current time in milliseconds, or 0 as long as the time is not
   * known.
   */
  uint64_t getTimeMs() override {
    if (!_hasTime) {
      return 0;  // Time not yet initialized
    }
    // Calculate ms since last update with correction factor applied
    uint32_t raw_ms_since_update = ::millis() - _lastUpdateMillis;
    uint64_t corrected_ms_since_update = static_cast<uint64_t>(
        static_cast<double>(raw_ms_since_update) * _millisCorrectionFactor);
    // Return last NTP time in ms plus corrected elapsed ms, plus offset
    return (static_cast<uint64_t>(_lastNtpTime) * 1000ULL) +
           _lastNtpTimeMs +
           corrected_ms_since_update +
           static_cast<uint64_t>(_timeOffsetSeconds) * 1000ULL;
  }

  /**
   * @brief Alias for getTimeMs().
   * @return Current time in milliseconds.
   */
  uint64_t millis() { return getTimeMs(); }

  /**
   * @brief Get the current time as a std::tm structure (UTC, plus the time
   * offset if one was set). Use getLocalTm() for the local time.
   * @return std::tm structure representing the current time.
   */
  std::tm getTm() {
    time_t t = getTimeSec();
    std::tm tm_result;
    gmtime_r(&t, &tm_result);
    return tm_result;
  }

  /**
   * @brief Set a fixed time offset in seconds (e.g., for timezone adjustment).
   * It is added to getTimeMs(), getTimeSec() and getTm(). Do not set it on a
   * client that is the time source of a TinyNTPServer: the server would serve
   * the shifted time instead of UTC. The offset is cleared by begin() and
   * end(). There is no handling of summer
   * time: use setTimeZone() for that, and not both.
   * @param offset Time offset in seconds.
   */
  void setTimeOffsetSeconds(long offset) { _timeOffsetSeconds = offset; }

  /**
   * @brief Set a fixed time offset in hours, see setTimeOffsetSeconds().
   * @param hours Time offset in hours.
   */
  void setTimeOffsetHours(long hours) { _timeOffsetSeconds = hours * 3600; }

  /**
   * @brief Set the time zone with a POSIX TZ string, e.g.
   * "CET-1CEST,M3.5.0,M10.5.0/3" for Europe/Zurich (or the define
   * TZ_EUROPE_ZURICH). Summer/winter time is handled by getLocalTm(). Do not
   * combine with setTimeOffsetSeconds(). The time zone does not change the
   * time of getTimeMs(), which stays UTC.
   * @param tz POSIX TZ string. The text is not copied: it must stay valid.
   * @return true if the string was valid.
   */
  bool setTimeZone(const char* tz) { return _timeZone.setTimeZone(tz); }

  /**
   * @brief Get the current local time according to setTimeZone(). If no valid
   * time zone was set, this is the same as getTm() (UTC).
   * @return std::tm structure with the local time; tm_isdst is set during
   * summer time.
   */
  std::tm getLocalTm() { return _timeZone.toLocal(getTimeSec()); }

  /**
   * @brief Get the name of the active time zone (e.g. "CET" or "CEST"), or
   * "UTC" if no valid time zone was set.
   */
  const char* getTimeZoneName() {
    return _timeZone.name(getTimeSec());
  }

  /**
   * @brief Get the millis() correction factor.
   * @return Correction factor (real_time / millis_time). A value of 1.0 means
   * no correction. Values > 1.0 mean millis() runs slower than real time,
   * values < 1.0 mean millis() runs faster than real time.
   */
  double getMillisCorrectionFactor() const { return _millisCorrectionFactor; }

  /**
   * @brief Set the NTP server address and port for the next update.
   * @param server NTP server hostname or IP address. The text is not copied:
   * it must stay valid as long as the client is used.
   * @param port NTP server port (default: 123).
   */
  void setServer(const char* server, int port = 123) {
    _server = server;
    _port = port;
  }

  /**
   * @brief Set the local UDP port that is used to send the requests. By
   * default this is the same as the NTP server port (123). Use a different
   * port if the device also runs a TinyNTPServer on port 123.
   * @param port Local port or 0 to use the NTP server port.
   */
  void setLocalPort(uint16_t port) { _localPort = port; }

  /**
   * @brief Update the current time from the NTP server (one request, or two
   * if there is no time yet). If it fails, the time that is known stays valid
   * and keeps running. Each update also improves the correction factor of
   * millis().
   * @return true if the update was successful, false otherwise.
   */
  bool update() {
    return !_hasTime ? updateWithoutRTC() && updateWithRTC() : updateWithRTC();
  }

  /**  @brief Returns true if the time has been received from a server. */
  operator bool() const { return _hasTime; }

  /**  @brief Get a reference to the UDP API. */
  UDPAPI& getUDP() { return _udp; }

 protected:
  /**
   * @brief Get uncorrected time in seconds (without millis correction factor).
   * Used internally during NTP exchange to avoid circular dependency.
   * Returns UTC time without timezone offset.
   */
  uint32_t getUncorrectedTimeSec() {
    if (!_hasTime) {
      return 0;
    }
    uint32_t raw_ms_since_update = ::millis() - _lastUpdateMillis;
    return _lastNtpTime + (raw_ms_since_update / 1000);
  }

  /**
   * @brief Structure representing an NTP packet (RFC 5905, 48 bytes)
   */
  struct __attribute__((packed)) NTPPacket {
    // Leap Indicator, Version Number, Mode (packed in one byte)
    // Bit layout: LI(7-6) | VN(5-3) | Mode(2-0)
    uint8_t li_vn_mode = 0;  // Default: 0x00, set to 0xE3 for client request
    uint8_t stratum = 0;     // Stratum level of the local clock
    uint8_t poll = 0;        // Maximum interval between messages
    uint8_t precision = 0;   // Precision of the local clock
    uint32_t rootDelay =
        0;  // Total round trip delay to the primary reference source
    uint32_t rootDispersion =
        0;                 // Max error relative to the primary reference source
    uint32_t refId = 0;    // Reference clock identifier
    uint32_t refTm_s = 0;  // Reference timestamp (seconds)
    uint32_t refTm_f = 0;  // Reference timestamp (fraction)
    uint32_t origTm_s =
        0;  // Originate timestamp (seconds) - time request sent by client
    uint32_t origTm_f = 0;  // Originate timestamp (fraction)
    uint32_t rxTm_s =
        0;  // Receive timestamp (seconds) - time request received by server
    uint32_t rxTm_f = 0;  // Receive timestamp (fraction)
    uint32_t txTm_s =
        0;  // Transmit timestamp (seconds) - time reply sent by server
    uint32_t txTm_f = 0;  // Transmit timestamp (fraction)
  };

  /** Reference to the UDP API (e.g., WiFiUDP, EthernetUDP). */
  UDPAPI _udp;
  /** NTP server hostname or IP address. */
  const char* _server;
  /** NTP server port. */
  int _port;
  /** Local UDP port (0: same as the NTP server port). */
  uint16_t _localPort = 0;
  /** Time zone rules from a POSIX TZ string. */
  TinyTimeZone _timeZone;
  /** Time offset in seconds (for timezone adjustment). */
  long _timeOffsetSeconds = 0;
  /** Last received NTP time (seconds since Unix epoch). */
  uint32_t _lastNtpTime = 0;
  /** Last received NTP time fraction (milliseconds part, 0-999). */
  uint32_t _lastNtpTimeMs = 0;
  /** Milliseconds when the last NTP update was received. */
  uint32_t _lastUpdateMillis = 0;
  /** True if the time was received from a server. */
  bool _hasTime = false;
  /** True if the baseline for the correction factor was established. */
  bool _hasBaseline = false;
  /** Initial baseline NTP time (seconds) for correction factor calculation. */
  uint32_t _baselineNtpTime = 0;
  /** Initial baseline NTP time fraction (milliseconds). */
  uint32_t _baselineNtpTimeMs = 0;
  /** Initial baseline millis() value for correction factor calculation. */
  uint32_t _baselineUpdateMillis = 0;
  /** Timeout for NTP response in milliseconds. */
  uint32_t _timeoutMs = 0;
  /** Correction factor for millis() drift (real_time / millis_time). */
  double _millisCorrectionFactor = 1.0;
  /** Local port of the open UDP socket (0: not open). */
  uint16_t _openPort = 0;
  /** Counter for the numbers of the requests. */
  uint32_t _requestCounter = 0;
  /** Maximum number of requests that are sent in one exchange. */
  static constexpr int MaxRequests = 16;
  /** Time in ms until a request is sent again if there is no response. */
  static constexpr uint32_t ResendIntervalMs = 300;

  /** Returns true if the CPU is little endian. */
  constexpr bool isLittleEndian() {
    unsigned int x = 1;
    char* c = (char*)&x;
    return (int)*c;
  }

  /** Swap the byte order of a 32 bit value. */
  inline uint32_t swap32(uint32_t value) {
    return ((value & 0x000000FF) << 24) | ((value & 0x0000FF00) << 8) |
           ((value & 0x00FF0000) >> 8) | ((value & 0xFF000000) >> 24);
  }

  /**
   * @brief Convert a 32-bit value from host byte order to network byte order
   * (big-endian).
   * @param hostlong Value in host byte order.
   * @return Value in network byte order.
   */
  inline uint32_t l_htonl(uint32_t hostlong) {
    return isLittleEndian() ? swap32(hostlong) : hostlong;
  }

  /**
   * @brief Convert a 32-bit value from network byte order (big-endian) to host
   * byte order.
   * @param netlong Value in network byte order.
   * @return Value in host byte order.
   */
  inline uint32_t l_ntohl(uint32_t netlong) {
    return isLittleEndian() ? swap32(netlong) : netlong;
  }

  /**
   * @brief Convert NTP fractional timestamp to milliseconds.
   * NTP fractions are in units of 2^-32 seconds, so we need to convert to ms.
   * @param fraction NTP fractional part (32-bit).
   * @return Milliseconds (0-999).
   */
  inline uint32_t ntpFractionToMs(uint32_t fraction) {
    // Convert from 2^-32 seconds to milliseconds
    // fraction / 2^32 * 1000 = fraction * 1000 / 2^32
    return (uint32_t)(((uint64_t)fraction * 1000ULL) >> 32);
  }

  /**
   * @brief Read a received packet, including the bytes that do not fit into
   * an NTP packet (otherwise some UDP classes do not return the next packet).
   * @return true if it is a complete NTP packet.
   */
  bool readResponse(int packetSize, NTPPacket& response) {
    uint8_t* buffer = reinterpret_cast<uint8_t*>(&response);
    int totalRead = 0;
    while (totalRead < (int)sizeof(NTPPacket) && _udp.available()) {
      int bytesRead =
          _udp.read(buffer + totalRead, (int)sizeof(NTPPacket) - totalRead);
      if (bytesRead <= 0) break;
      totalRead += bytesRead;
    }
    uint8_t rest[16];
    while (_udp.available() > 0 && _udp.read(rest, sizeof(rest)) > 0) {
    }
    if (packetSize < (int)sizeof(NTPPacket)) {
      log("NTP: packet too short - Expected: %d, Got: %d\n",
          (int)sizeof(NTPPacket), packetSize);
      return false;
    }
    if (totalRead < (int)sizeof(NTPPacket)) {
      log("NTP: response read incomplete - Expected: %d, Got: %d\n",
          (int)sizeof(NTPPacket), totalRead);
      return false;
    }
    return true;
  }

  /**
   * @brief Perform the NTP request/response exchange and update the internal
   * time. The request is sent again every ResendIntervalMs until a response
   * arrives or the timeout has passed.
   *
   * @param txTm_s   Transmit timestamp (seconds since NTP epoch, 1900) to send
   * in the request packet.
   * @return true if the update was successful, false otherwise.
   */
  bool ntpExchange(uint32_t txTm_s) {
    NTPPacket packet = {};
    // Set LI=3 (clock not synchronized), VN=3 (NTPv3), Mode=3 (client)
    // Binary: 11 011 011 = 0xDB
    packet.li_vn_mode = 0xDB;
    // Convert transmit timestamp to network byte order
    packet.txTm_s = l_htonl(txTm_s);

    // The socket stays open between the updates, so that a late response is
    // still received (and ignored) and does not get lost with the socket
    uint16_t localPort = _localPort != 0 ? _localPort : _port;
    if (_openPort != localPort) {
      _openPort = _udp.begin(localPort) ? localPort : 0;
    }

    // The request is sent again until a response arrives: some WiFi networks
    // (e.g. with an ESP32) lose responses or hold them back until the device
    // sends the next packet. Each request has its own number in the transmit
    // fraction, which the server returns as originate time: the round trip is
    // measured from the request that was answered, and responses to the
    // requests of an earlier update are ignored.
    uint32_t sentMillis[MaxRequests];
    int sentCount = 0;
    uint32_t requestId = ++_requestCounter << 8;
    uint32_t startMillis = ::millis();
    uint32_t t0_millis = 0;  // the time when the answered request was sent
    uint32_t t3_millis = 0;  // the time when the response was received
    NTPPacket response = {};
    bool received = false;
    while (!received) {
      // 32 bit arithmetic: also correct after the overflow of millis(), and if
      // millis() returns a 64 bit value (e.g. on a computer)
      uint32_t now = ::millis();
      if (static_cast<uint32_t>(now - startMillis) > _timeoutMs) {
        log("NTP: request timed out\n");
        return false;
      }
      if (sentCount < MaxRequests &&
          (sentCount == 0 || static_cast<uint32_t>(
                                 now - sentMillis[sentCount - 1]) >=
                                 ResendIntervalMs)) {
        packet.txTm_f = l_htonl(requestId | sentCount);
        sentMillis[sentCount++] = now;
        _udp.beginPacket(_server, _port);
        _udp.write(reinterpret_cast<uint8_t*>(&packet), sizeof(NTPPacket));
        _udp.endPacket();
      }

      int packetSize = _udp.parsePacket();
      if (packetSize <= 0) continue;
      t3_millis = ::millis();
      if (!readResponse(packetSize, response)) continue;
      uint32_t originate = l_ntohl(response.origTm_f);
      int index = static_cast<int>(originate & 0xFF);
      if ((originate & ~0xFFu) != requestId || index >= sentCount) continue;
      t0_millis = sentMillis[index];
      received = true;
    }

    uint32_t receive = l_ntohl(response.rxTm_s);
    uint32_t receive_f = l_ntohl(response.rxTm_f);
    uint32_t transmit = l_ntohl(response.txTm_s);
    uint32_t transmit_f = l_ntohl(response.txTm_f);

    // The transmit time of the server is the time when it sent the response.
    // The response needed about half of the network delay to get to us, which
    // is the time of the round trip without the time that the server needed.
    uint32_t roundTripMs = t3_millis - t0_millis;
    int64_t serverMs =
        static_cast<int64_t>(static_cast<int32_t>(transmit - receive)) * 1000 +
        static_cast<int64_t>(ntpFractionToMs(transmit_f)) -
        static_cast<int64_t>(ntpFractionToMs(receive_f));
    int64_t delayMs = static_cast<int64_t>(roundTripMs) - serverMs;
    if (delayMs < 0) delayMs = 0;
    if (delayMs > roundTripMs) delayMs = roundTripMs;
    // Unix seconds, calculated with 32 bit (also if unsigned long has 64 bit):
    // correct until 2106, also after the rollover of the NTP time in 2036
    uint32_t transmitUnix = static_cast<uint32_t>(transmit - 2208988800UL);
    uint64_t newTimeMs = static_cast<uint64_t>(transmitUnix) * 1000ULL +
                         ntpFractionToMs(transmit_f) +
                         static_cast<uint64_t>(delayMs / 2);
    uint32_t newNtpTime = static_cast<uint32_t>(newTimeMs / 1000);
    uint32_t newNtpTimeMs = static_cast<uint32_t>(newTimeMs % 1000);

    if (!_hasBaseline) {
      // First update: establish baseline for correction factor calculation
      _baselineNtpTime = newNtpTime;
      _baselineNtpTimeMs = newNtpTimeMs;
      _baselineUpdateMillis = t3_millis;
      _hasBaseline = true;
    } else {
      // Calculate correction factor based on cumulative drift since baseline
      uint32_t raw_millis_elapsed = t3_millis - _baselineUpdateMillis;
      // Calculate actual time elapsed according to NTP (with millisecond
      // precision)
      uint64_t actual_time_elapsed_ms =
          ((uint64_t)(newNtpTime - _baselineNtpTime) * 1000ULL) +
          newNtpTimeMs - _baselineNtpTimeMs;

      // Only update correction factor if enough time has elapsed (at least 10
      // seconds). This avoids division by zero and ensures meaningful
      // calibration
      if (raw_millis_elapsed >= 10000 && actual_time_elapsed_ms >= 10000) {
        _millisCorrectionFactor = static_cast<double>(actual_time_elapsed_ms) /
                                  static_cast<double>(raw_millis_elapsed);
      }
    }

    _lastNtpTime = newNtpTime;
    _lastNtpTimeMs = newNtpTimeMs;
    _lastUpdateMillis = t3_millis;
    _hasTime = true;
    return true;
  }

  /**
   * @brief Request the current time from the NTP server and update the
   * internal time.
   * @return true if the update was successful, false otherwise.
   */
  bool updateWithoutRTC() {
    // No real time clock: set transmit timestamp to zero, use millis for
    // timeout
    return ntpExchange(0);
  }

  /**
   * @brief Request the current time from the NTP server and update the
   * internal time using a real time clock.
   * @return true if the update was successful, false otherwise.
   */
  bool updateWithRTC() {
    // Use system clock for transmit timestamp (uncorrected to avoid circular dependency)
    uint32_t t0_unix = getUncorrectedTimeSec();  // seconds since 1970
    uint32_t t0_ntp = t0_unix + 2208988800UL;     // seconds since 1900
    return ntpExchange(t0_ntp);
  }

  /**
   * @brief Log a formatted message (printf-style).
   * @param format Format string (printf-style).
   * @param ... Variable arguments.
   */
  void log(const char* format, ...) {
    char buffer[128];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    Serial.print(buffer);
  }
};

}  // namespace tiny_ntp

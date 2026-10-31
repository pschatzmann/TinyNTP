

/**
 * @file TinyNTPClient.h
 * @brief Simple NTP client for Arduino using a generic UDP Client (e.g.,
 * WiFiUdp, EthernetUdp).
 */

#pragma once
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <ctime>

#include "TinyTimeZone.h"
#include "TinyTimeZoneDB.h"

/**
 * @class TinyNTPClient
 * @brief A Network Time Protocol (NTP) client for retrieving the current
 * date/time from an NTP server using an Arduino UDP API. It returns the time
 * information from the NTP server and adjusts it based on the round-trip delay
 * calculation. The timezone offset can be set to account for local time
 * differences.
 * @tparam UDPAPI The UDP class to use (e.g: WiFiUDP)
 */
template <typename UDPAPI>
class TinyNTPClient {
 public:
  /**
   * @brief Construct a new TinyNTPClient object.
   * @param server NTP server hostname or IP address.
   * @param port NTP server port (default: 123).
   */
  TinyNTPClient(const char* server = "pool.ntp.org", int port = 123,
                uint32_t timeoutMs = 6000)
      : _server(server), _port(port), _timeoutMs(timeoutMs) {}

  /**
   * @brief Initialize the NTP client and perform the first time update.
   * @return true if the update was successful, false otherwise.
   */
  bool begin() {
    end();
    return update();
  }

  /**
   * @brief Reset the client state and clear the last received time.
   */
  void end() {
    _timeOffsetSeconds = 0;
    _lastNtpTime = 0;
    _lastNtpTimeMs = 0;
    _lastUpdateMillis = 0;
    _baselineNtpTime = 0;
    _baselineNtpTimeMs = 0;
    _baselineUpdateMillis = 0;
    _millisCorrectionFactor = 1.0;
    _udp.stop();
  }

  /**
   * @brief Get the current time in seconds since Unix epoch (UTC).
   * @return Current time in seconds.
   */
  uint32_t getTimeSec() {
    // Return time in seconds from getTimeMs()
    return static_cast<uint32_t>(getTimeMs() / 1000ULL);
  }

  /**
   * @brief Get the current time in milliseconds since Unix epoch (UTC).
   * @return Current time in milliseconds.
   */
  uint64_t getTimeMs() {
    if (_lastUpdateMillis == 0) {
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
   * @brief Get the current time as a std::tm structure (UTC).
   * @return std::tm structure representing the current time (UTC).
   */
  std::tm getTm() {
    time_t t = getTimeSec();
    std::tm tm_result;
    gmtime_r(&t, &tm_result);
    return tm_result;
  }

  /**
   * @brief Set the time offset in seconds (e.g., for timezone adjustment).
   * @param offset Time offset in seconds.
   */
  void setTimeOffsetSeconds(long offset) { _timeOffsetSeconds = offset; }

  /**
   * @brief Set the time offset in hours (e.g., for timezone adjustment).
   * @param hours Time offset in hours.
   */
  void setTimeOffsetHours(long hours) { _timeOffsetSeconds = hours * 3600; }

  /**
   * @brief Set the time zone with a POSIX TZ string, e.g.
   * "CET-1CEST,M3.5.0,M10.5.0/3" for Europe/Zurich. Summer/winter time is
   * handled by getLocalTm(). Do not combine with setTimeOffsetSeconds().
   * @param tz POSIX TZ string.
   * @return true if the string was valid.
   */
  bool setTimeZone(const char* tz) { return _timeZone.setTimeZone(tz); }

  /**
   * @brief Get the current local time according to setTimeZone(). If no valid
   * time zone was set, this is the same as getTm().
   * @return std::tm structure with the local time; tm_isdst is set during
   * summer time.
   */
  std::tm getLocalTm() { return _timeZone.toLocal(getTimeSec()); }

  /**
   * @brief Get the name of the active time zone (e.g. "CET" or "CEST").
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
   * @brief Set the NTP server address and port.
   * @param server NTP server hostname or IP address.
   * @param port NTP server port (default: 123).
   */
  void setServer(const char* server, int port = 123) {
    _server = server;
    _port = port;
  }

  /**
   * @brief Update the current time from the NTP server.
   * @return true if the update was successful, false otherwise.
   */
  bool update() {
    return (_lastUpdateMillis == 0) ? updateWithoutRTC() && updateWithRTC()
                                    : updateWithRTC();
  }

  /**  @brief Conversion operator to bool. */
  operator bool() const { return _lastUpdateMillis != 0; }

  /**  @brief Get a reference to the UDP API. */
  UDPAPI& getUDP() { return _udp; }

 protected:
  /**
   * @brief Get uncorrected time in seconds (without millis correction factor).
   * Used internally during NTP exchange to avoid circular dependency.
   * Returns UTC time without timezone offset.
   */
  uint32_t getUncorrectedTimeSec() {
    if (_lastUpdateMillis == 0) {
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

  constexpr bool isLittleEndian() {
    unsigned int x = 1;
    char* c = (char*)&x;
    return (int)*c;
  }

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
   * @brief Perform the NTP request/response exchange and update the internal
   * time.
   *
   * @param txTm_s   Transmit timestamp (seconds since NTP epoch, 1900) to send
   * in the request packet.
   * @return true if the update was successful, false otherwise.
   */
  bool ntpExchange(uint32_t txTm_s) {
    NTPPacket packet = {};
    // Set LI=3 (no warning), VN=3 (NTPv3), Mode=3 (client)
    // Binary: 11 011 011 = 0xDB
    packet.li_vn_mode = 0xDB;
    // Convert transmit timestamp to network byte order
    packet.txTm_s = l_htonl(txTm_s);
    packet.txTm_f = 0;

    _udp.begin(_port);
    _udp.beginPacket(_server, _port);
    _udp.write(reinterpret_cast<uint8_t*>(&packet), sizeof(NTPPacket));
    _udp.endPacket();

    // Wait for response with timeout
    uint32_t timeout_ms = ::millis() + _timeoutMs;
    int packetSize = 0;
    while ((packetSize = _udp.parsePacket()) == 0) {
      if (::millis() > timeout_ms) {
        log("NTP: request timed out\n");
        return false;
      }
    }

    // Read response
    uint32_t t3_millis = ::millis();
    NTPPacket response = {};
    uint8_t* buffer = reinterpret_cast<uint8_t*>(&response);

    // Check if packet size is correct
    if (packetSize < (int)sizeof(NTPPacket)) {
      _udp.read(buffer, packetSize);  // Read available data for logging
      log("NTP: packet too short - Expected: %d, Got: %d (%s)\n",
          sizeof(NTPPacket), packetSize, buffer);
      return false;
    }

    // Read the packet data - read in chunks if necessary
    int totalRead = 0;
    int bytesToRead = sizeof(NTPPacket);

    while (totalRead < bytesToRead && _udp.available()) {
      int bytesRead = _udp.read(buffer + totalRead, bytesToRead - totalRead);
      if (bytesRead > 0) {
        totalRead += bytesRead;
      } else {
        break;
      }
    }

    if (totalRead < (int)sizeof(NTPPacket)) {
      log("NTP: response read incomplete - Expected: %d, Got: %d (%s)\n",
          sizeof(NTPPacket), totalRead, buffer);
      return false;
    }

    bool useOffset = (txTm_s != 0);
    if (useOffset) {
      // Full NTP offset calculation
      uint32_t originate = l_ntohl(response.origTm_s);
      uint32_t receive = l_ntohl(response.rxTm_s);
      uint32_t transmit = l_ntohl(response.txTm_s);
      uint32_t transmit_f = l_ntohl(response.txTm_f);
      // log("NTP: originate=%u, receive=%u, transmit=%u\n", originate, receive,
      // transmit);
      uint32_t t3_unix = getUncorrectedTimeSec();
      uint32_t t3_ntp = t3_unix + 2208988800UL;
      long offset =
          ((long)(receive - originate) + (long)(transmit - t3_ntp)) / 2;
      uint32_t newNtpTime = transmit - 2208988800UL + offset;
      uint32_t newNtpTimeMs = ntpFractionToMs(transmit_f);
      
      // Establish baseline on first update, calculate correction factor on subsequent updates
      if (_baselineUpdateMillis == 0) {
        // First update: establish baseline for correction factor calculation
        _baselineNtpTime = newNtpTime;
        _baselineNtpTimeMs = newNtpTimeMs;
        _baselineUpdateMillis = t3_millis;
      } else {
        // Calculate correction factor based on cumulative drift since baseline
        uint32_t raw_millis_elapsed = t3_millis - _baselineUpdateMillis;
        // Calculate actual time elapsed according to NTP (with millisecond precision)
        uint64_t actual_time_elapsed_ms = 
            ((uint64_t)(newNtpTime - _baselineNtpTime) * 1000ULL) + 
            newNtpTimeMs - _baselineNtpTimeMs;
        
        // Only update correction factor if enough time has elapsed (at least 10 seconds)
        // This avoids division by zero and ensures meaningful calibration
        if (raw_millis_elapsed >= 10000 && actual_time_elapsed_ms >= 10000) {
          _millisCorrectionFactor = static_cast<double>(actual_time_elapsed_ms) / 
                                   static_cast<double>(raw_millis_elapsed);
          // log("NTP: correction factor = %.6f (actual=%llu ms, millis=%u ms from baseline)\n",
          //     _millisCorrectionFactor, actual_time_elapsed_ms, raw_millis_elapsed);
        } else {
          // log("NTP: skipping correction factor update - insufficient time from baseline (actual=%llu ms, millis=%u ms)\n",
          //     actual_time_elapsed_ms, raw_millis_elapsed);
        }
      }
      
      _lastNtpTime = newNtpTime;
      _lastNtpTimeMs = newNtpTimeMs;
      _lastUpdateMillis = t3_millis;
    } else {
      // Only use server transmit timestamp (first update without RTC)
      uint32_t transmit = l_ntohl(response.txTm_s);
      uint32_t transmit_f = l_ntohl(response.txTm_f);
      _lastNtpTime = transmit - 2208988800UL;
      _lastNtpTimeMs = ntpFractionToMs(transmit_f);
      _lastUpdateMillis = t3_millis;
      
      // Establish baseline if not yet set
      if (_baselineUpdateMillis == 0) {
        _baselineNtpTime = _lastNtpTime;
        _baselineNtpTimeMs = _lastNtpTimeMs;
        _baselineUpdateMillis = t3_millis;
      }
    }
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
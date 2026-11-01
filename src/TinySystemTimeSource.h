/**
 * @file TinySystemTimeSource.h
 * @brief Time source for the TinyNTPServer that uses the system clock (e.g.
 * set by the SNTP client of the ESP32/ESP8266 core).
 */

#pragma once
#include <sys/time.h>

#include <cstdint>

#include "TinyTimeSource.h"

namespace tiny_ntp {

/**
 * @class TinySystemTimeSource
 * @brief Serves the UTC time of the system clock (gettimeofday()). It is
 * available on the platforms that provide gettimeofday() (e.g. ESP32,
 * ESP8266, RP2040, STM32, desktop) but not on AVR.
 *
 * The system clock starts at 1970 when it has not been set: until the clock
 * has a time after the "minimum valid time", the time is reported as not
 * known (0).
 * @code
 * configTime(0, 0, "pool.ntp.org");  // ESP32: set the clock with SNTP (UTC)
 * TinySystemTimeSource source;
 * server.begin(source);
 * @endcode
 */
class TinySystemTimeSource : public TinyTimeSource {
 public:
  /**
   * @param minValidSeconds Times before this Unix time (default: 2023-11-14)
   * are treated as clock not set.
   */
  explicit TinySystemTimeSource(uint32_t minValidSeconds = 1700000000UL)
      : _minValid(minValidSeconds) {}

  /**
   * @brief Get the current UTC time in milliseconds since 1970-01-01.
   * @return The time or 0 if the system clock has not been set.
   */
  uint64_t getTimeMs() override {
    timeval tv;
    if (gettimeofday(&tv, nullptr) != 0) return 0;
    if (static_cast<uint64_t>(tv.tv_sec) < _minValid) return 0;
    return static_cast<uint64_t>(tv.tv_sec) * 1000ULL + tv.tv_usec / 1000;
  }

 protected:
  uint32_t _minValid;
};

}  // namespace tiny_ntp

#if defined(ARDUINO)
// Arduino sketches use the classes without the namespace
using namespace tiny_ntp;
#endif

/**
 * @file TinyClockTimeSource.h
 * @brief Time source for the TinyNTPServer that you set from any source (RTC,
 * GPS, ...) and that keeps running with millis().
 */

#pragma once
#include <cstdint>

#include "TinyTimeSource.h"

namespace tiny_ntp {

/**
 * @class TinyClockTimeSource
 * @brief A software clock: you set the UTC time with setTime() and it keeps
 * counting with millis(). Use it as time source of the TinyNTPServer when the
 * time comes from a device that has no ready made getTimeMs(), e.g. an RTC
 * chip or a GPS receiver:
 * @code
 * clock.setTime(rtc.now().unixtime());
 * server.begin(clock);
 * @endcode
 *
 * As long as the time has not been set, getTimeMs() returns 0 (not known).
 * millis() is not as exact as an RTC or a GPS: set the time regularly (e.g.
 * every few minutes). It must be set or read at least once every 49 days
 * because of the overflow of millis().
 */
class TinyClockTimeSource : public TinyTimeSource {
 public:
  /**
   * @brief Set the current UTC time.
   * @param unixSeconds Seconds since 1970-01-01 (UTC).
   * @param milliseconds Milliseconds within the second (0-999). Use this if
   * you know the exact moment, e.g. right after the second of an RTC changed.
   */
  void setTime(uint32_t unixSeconds, uint16_t milliseconds = 0) {
    setTimeMs(unixSeconds * 1000ULL + milliseconds);
  }

  /**
   * @brief Set the current UTC time in milliseconds since 1970-01-01.
   * Setting 0 clears the time.
   */
  void setTimeMs(uint64_t unixMs) {
    _timeMs = unixMs;
    _lastMillis = ::millis();
  }

  /** @brief Forget the time: getTimeMs() returns 0 until it is set again. */
  void clear() { _timeMs = 0; }

  /** @brief Returns true if the time has been set. */
  bool isSet() const { return _timeMs != 0; }

  /**
   * @brief Get the current UTC time in milliseconds since 1970-01-01.
   * @return The time or 0 if it has not been set.
   */
  uint64_t getTimeMs() override {
    if (_timeMs == 0) return 0;
    // add the time since the last call: this also handles the overflow of
    // millis() as long as it is called at least once every 49 days
    uint32_t now = ::millis();
    _timeMs += static_cast<uint32_t>(now - _lastMillis);
    _lastMillis = now;
    return _timeMs;
  }

 protected:
  uint64_t _timeMs = 0;
  uint32_t _lastMillis = 0;
};

}  // namespace tiny_ntp

#if defined(ARDUINO)
// Arduino sketches use the classes without the namespace
using namespace tiny_ntp;
#endif

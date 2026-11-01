/**
 * @file TinyTimeSource.h
 * @brief Interface for the time sources of the TinyNTPServer.
 */

#pragma once
#include <cstdint>

/**
 * @namespace tiny_ntp
 * @brief The classes of the TinyNTP library: the NTP client (TinyNTPClient),
 * the NTP server (TinyNTPServer), the time sources (TinyTimeSource) and the
 * time zone handling (TinyTimeZone). In Arduino sketches (ARDUINO is defined)
 * the namespace is used automatically.
 */
namespace tiny_ntp {

/**
 * @class TinyTimeSource
 * @brief Abstract base class of all time sources: an object that knows the
 * current UTC time. The TinyNTPServer serves the time of a TinyTimeSource.
 *
 * Provided time sources: TinyNTPClient (time from the internet),
 * TinySystemTimeSource (system clock) and TinyClockTimeSource (software clock
 * that you set from an RTC, a GPS, ...). To use another source, derive from
 * this class:
 * @code
 * class MyTimeSource : public TinyTimeSource {
 *  public:
 *   uint64_t getTimeMs() override { return myClock.unixMs(); }
 * };
 * @endcode
 */
class TinyTimeSource {
 public:
  /**
   * @brief Get the current UTC time in milliseconds since 1970-01-01. It is
   * called for each request, so it should be fast and must not block.
   * @return The time in ms, or 0 as long as the time is not known (e.g. not
   * synchronized yet). The server does not answer while the time is 0.
   */
  virtual uint64_t getTimeMs() = 0;

  /**
   * @brief Set the current UTC time, e.g. by the command line tool `tinyntp
   * set` (see TinyNTPServer::enableTimeSetting()). The default implementation
   * does not support this. Override it if the source can be set; you can also
   * write the time to an RTC here.
   * @param unixMs The new time in milliseconds since 1970-01-01.
   * @return true if the time was set.
   */
  virtual bool setTimeMs(uint64_t unixMs) {
    (void)unixMs;
    return false;
  }
};

}  // namespace tiny_ntp

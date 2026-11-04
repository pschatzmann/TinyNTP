/**
 * @file TinyGPSTimeSource.h
 * @brief Time from a GPS / GNSS receiver: a parser of the NMEA sentences that
 * carry the time (TinyNMEAParser) and a time source that keeps the time
 * between the sentences, optionally aligned to the pulse per second (PPS).
 */

#pragma once
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "TinyTimeSource.h"

namespace tiny_ntp {

/**
 * @class TinyNMEAParser
 * @brief Parser of the NMEA 0183 sentences of a GNSS receiver that carry the
 * time and the state of the fix: RMC and ZDA (date and time), GGA (fix
 * quality, satellites used) and GSV (satellites in view). Any talker (GP,
 * GN, GL, GA, GB, ...) is accepted; sentences with a wrong checksum are
 * ignored. It needs no hardware, so it can be fed from any source.
 */
class TinyNMEAParser {
 public:
  /**
   * @brief Feed one character.
   * @return true if a time sentence (RMC or ZDA with a valid time) was
   * completed: the time is then in getTimeMs().
   */
  bool feed(char c) {
    if (c == '$') {
      _len = 0;
      _active = true;
      return false;
    }
    if (!_active) return false;
    if (c == '\r' || c == '\n') {
      _active = false;
      _buf[_len] = 0;
      return parseSentence();
    }
    if (_len < sizeof(_buf) - 1) {
      _buf[_len++] = c;
    } else {
      _active = false;  // too long: not NMEA
    }
    return false;
  }

  /** @brief UTC ms since 1970 of the last time sentence (0: none yet). */
  uint64_t getTimeMs() const { return _timeMs; }
  /** @brief Fraction of the last time in ms (0: the full second). */
  int getFractionMs() const { return _fractionMs; }
  /** @brief The receiver has a fix (GGA quality > 0 or RMC status A). */
  bool hasFix() const { return _fixQuality > 0 || _rmcValid; }
  /** @brief Satellites used for the fix (GGA). */
  int getSatellitesUsed() const { return _satsUsed; }
  /** @brief Satellites in view of all systems (sum of the GSV of each talker). */
  int getSatellitesVisible() const {
    int sum = 0;
    for (int v : _satsInView) sum += v;
    return sum;
  }
  /** @brief Number of sentences with a wrong checksum. */
  uint32_t getChecksumErrors() const { return _checksumErrors; }

 protected:
  char _buf[96];
  size_t _len = 0;
  bool _active = false;
  int _fixQuality = 0;
  bool _rmcValid = false;
  int _satsUsed = 0;
  int _satsInView[6] = {0, 0, 0, 0, 0, 0};  // GP, GL, GA, GB, GQ, GI
  uint64_t _timeMs = 0;
  int _fractionMs = 0;
  uint32_t _checksumErrors = 0;

  static int hex(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return -1;
  }

  /** Splits _buf into fields (in place). Returns the number of fields. */
  int split(char** fields, int max) {
    int n = 0;
    char* p = _buf;
    fields[n++] = p;
    while (*p && n < max) {
      if (*p == ',') {
        *p = 0;
        fields[n++] = p + 1;
      }
      p++;
    }
    return n;
  }

  bool parseSentence() {
    char* star = strchr(_buf, '*');
    if (star == nullptr || hex(star[1]) < 0 || hex(star[2]) < 0) return false;
    uint8_t sum = 0;
    for (char* p = _buf; p < star; ++p) sum ^= static_cast<uint8_t>(*p);
    if (sum != hex(star[1]) * 16 + hex(star[2])) {
      _checksumErrors++;
      return false;
    }
    *star = 0;
    if (strlen(_buf) < 5) return false;
    const char* type = _buf + 2;  // after the talker
    const char talker[3] = {_buf[0], _buf[1], 0};
    char* f[24];
    const int n = split(f, 24);
    if (strncmp(type, "RMC", 3) == 0 && n >= 10) {
      _rmcValid = f[2][0] == 'A';
      return _rmcValid && setTime(f[1], f[9], nullptr, nullptr, nullptr);
    }
    if (strncmp(type, "ZDA", 3) == 0 && n >= 5) {
      return f[2][0] && f[4][0] && setTime(f[1], nullptr, f[2], f[3], f[4]);
    }
    if (strncmp(type, "GGA", 3) == 0 && n >= 8) {
      _fixQuality = atoi(f[6]);
      _satsUsed = atoi(f[7]);
    } else if (strncmp(type, "GSV", 3) == 0 && n >= 4) {
      static const char* const talkers[6] = {"GP", "GL", "GA", "GB", "GQ", "GI"};
      for (int i = 0; i < 6; ++i) {
        if (strcmp(talker, talkers[i]) == 0) _satsInView[i] = atoi(f[3]);
      }
    }
    return false;
  }

  static int two(const char* p) { return (p[0] - '0') * 10 + (p[1] - '0'); }

  /** time hhmmss.sss, date ddmmyy (RMC) or day, month, year (ZDA). */
  bool setTime(const char* time, const char* ddmmyy, const char* day, const char* month,
               const char* year) {
    if (strlen(time) < 6) return false;
    int d, m, y;
    if (ddmmyy != nullptr) {
      if (strlen(ddmmyy) < 6) return false;
      d = two(ddmmyy);
      m = two(ddmmyy + 2);
      y = 2000 + two(ddmmyy + 4);
    } else {
      d = atoi(day);
      m = atoi(month);
      y = atoi(year);
    }
    if (m < 1 || m > 12 || d < 1 || d > 31 || y < 2000) return false;
    const int hh = two(time), mm = two(time + 2), ss = two(time + 4);
    if (hh > 23 || mm > 59 || ss > 60) return false;
    _fractionMs = time[6] == '.' ? static_cast<int>(atof(time + 6) * 1000.0 + 0.5) : 0;
    const int64_t days = daysFromCivil(y, m, d);
    _timeMs = static_cast<uint64_t>(((days * 24 + hh) * 60 + mm) * 60 + ss) * 1000ULL + _fractionMs;
    return true;
  }

  static int64_t daysFromCivil(int y, unsigned m, unsigned d) {
    y -= m <= 2;
    const int64_t era = (y >= 0 ? y : y - 399) / 400;
    const unsigned yoe = static_cast<unsigned>(y - era * 400);
    const unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + static_cast<int64_t>(doe) - 719468;
  }
};

/**
 * @class TinyGPSTimeSource
 * @brief The UTC time of a GPS / GNSS receiver as time source, e.g. for a
 * stratum 1 TinyNTPServer.
 *
 * Feed the characters of the receiver with process() (or feed()). Each RMC or
 * ZDA sentence sets the time; between the sentences it runs with micros().
 *
 * - With PPS: call pps() from the interrupt of the PPS pin. The rising edge
 *   is the start of the second that the following sentence reports, so the
 *   time is exact to a few µs.
 * - Without PPS: the arrival of the first character of the sentence is used,
 *   corrected by setLatencyMs() (receiver specific, typically 50-500 ms):
 *   good to tens of ms.
 *
 * @code
 * TinyGPSTimeSource gps;
 * void IRAM_ATTR onPps() { gps.pps(micros()); }
 * void setup() {
 *   Serial1.begin(9600);
 *   attachInterrupt(digitalPinToInterrupt(PPS_PIN), onPps, RISING);
 *   server.begin(gps);
 *   server.setStratum(1);
 *   server.setReferenceId("GPS");
 * }
 * void loop() {
 *   gps.process(Serial1);
 *   server.process();
 * }
 * @endcode
 *
 * The time is 0 (not known) until the receiver has a fix. Each fix is also
 * available as pair of UTC time and micros() value (getLastFix()), so that
 * the offset of another clock can be measured.
 */
class TinyGPSTimeSource : public TinyTimeSource {
 public:
  /** @brief Reads all available characters of the receiver (any Stream). */
  template <typename StreamT>
  void process(StreamT& in) {
    while (in.available() > 0) {
      const int c = in.read();
      if (c < 0) break;
      feed(static_cast<char>(c), ::micros());
    }
  }

  /**
   * @brief Feed one character of the receiver.
   * @param c The character.
   * @param nowMicros micros() when it was received.
   * @return true if the time was set from a sentence.
   */
  bool feed(char c, uint32_t nowMicros) {
    if (c == '$') _sentenceMicros = nowMicros;
    if (!_parser.feed(c) || !_parser.hasFix()) return false;
    const uint64_t utcMs = _parser.getTimeMs();
    if (utcMs == _lastSentenceUtcMs) return false;  // RMC and ZDA of the same second
    _lastSentenceUtcMs = utcMs;
    const uint32_t pps = _ppsMicros;
    const uint32_t sincePps = _sentenceMicros - pps;
    _lastFixPps = _ppsCount != 0 && _parser.getFractionMs() == 0 && sincePps < 1000000UL;
    if (_lastFixPps) {
      _fixMicros = pps;  // the start of the reported second
      _fixUtcMs = utcMs;
    } else {
      _fixMicros = _sentenceMicros;
      _fixUtcMs = utcMs + _latencyMs;
    }
    _fixMillis = ::millis() - static_cast<uint32_t>((nowMicros - _fixMicros) / 1000);
    _fixCount++;
    return true;
  }

  /** @brief Call from the interrupt of the PPS pin (rising edge). */
  void pps(uint32_t nowMicros) {
    _ppsMicros = nowMicros;
    _ppsCount++;
  }

  /** @brief Time from the start of the second to the sentence (without PPS). */
  void setLatencyMs(uint32_t ms) { _latencyMs = ms; }

  /**
   * @brief The UTC time in ms since 1970, or 0 as long as there is no fix.
   */
  uint64_t getTimeMs() override {
    if (_fixCount == 0) return 0;
    const uint32_t elapsedMicros = ::micros() - _fixMicros;
    // micros() overflows after 71 minutes: use millis() for old fixes
    const uint32_t elapsedMillis = ::millis() - _fixMillis;
    const uint64_t elapsed = elapsedMillis < 60000UL ? elapsedMicros / 1000 : elapsedMillis;
    return _fixUtcMs + elapsed;
  }

  /**
   * @brief The last fix: the UTC time (ms) and the micros() value at that
   * moment. Returns false if there was none.
   */
  bool getLastFix(uint64_t& utcMs, uint32_t& atMicros, bool& withPps) const {
    if (_fixCount == 0) return false;
    utcMs = _fixUtcMs;
    atMicros = _fixMicros;
    withPps = _lastFixPps;
    return true;
  }

  /** @brief Number of fixes since the start (changes with each new fix). */
  uint32_t getFixCount() const { return _fixCount; }

  /** @brief PPS pulses arrived during the last 2 seconds. */
  bool isPpsActive() const { return _ppsCount != 0 && ::micros() - _ppsMicros < 2000000UL; }

  /** @brief The NMEA parser (satellites, fix). */
  const TinyNMEAParser& parser() const { return _parser; }

 protected:
  TinyNMEAParser _parser;
  uint32_t _sentenceMicros = 0;
  uint64_t _lastSentenceUtcMs = 0;
  volatile uint32_t _ppsMicros = 0;
  volatile uint32_t _ppsCount = 0;
  uint32_t _latencyMs = 0;
  uint64_t _fixUtcMs = 0;
  uint32_t _fixMicros = 0;
  uint32_t _fixMillis = 0;
  uint32_t _fixCount = 0;
  bool _lastFixPps = false;
};

}  // namespace tiny_ntp

/**
 * @file TinyTimeZone.h
 * @brief Portable parser for POSIX TZ strings (e.g.
 * "CET-1CEST,M3.5.0,M10.5.0/3") that converts UTC to local time, including
 * summer/winter time. It does not depend on tzset()/localtime_r().
 */

#pragma once
#include <cstdint>
#include <cstring>
#include <ctime>

/**
 * @class TinyTimeZone
 * @brief Parses a POSIX TZ string and converts UTC time to local time.
 *
 * Supported format: std offset [dst [offset] [,start[/time],end[/time]]]
 * - names: 3 or more letters, or any text in <...> (e.g. <+0330>)
 * - offset: [+-]hh[:mm[:ss]] (positive is west of UTC, as in POSIX)
 * - rules: Mm.w.d (month, week 1-5 where 5 is last, weekday 0=Sunday),
 *   Jn (day 1-365 without leap day) or n (day 0-365 with leap day)
 * - time: [+-]hh[:mm[:ss]], default 02:00:00
 * If a DST name is given without rules, the US rules (M3.2.0,M11.1.0) are used.
 */
class TinyTimeZone {
 public:
  TinyTimeZone() = default;

  /**
   * @brief Parse a POSIX TZ string.
   * @param tz The TZ string, e.g. "CET-1CEST,M3.5.0,M10.5.0/3".
   * @return true if the string was valid. On failure UTC is used.
   */
  bool setTimeZone(const char* tz) {
    clear();
    if (tz == nullptr) return false;
    const char* p = tz;
    if (!parseName(p, _stdName) || !parseTime(p, _stdOffset)) {
      clear();
      return false;
    }
    // POSIX offsets are positive west of UTC
    _stdOffset = -_stdOffset;
    if (*p == 0) {
      _valid = true;
      return true;
    }
    if (!parseName(p, _dstName)) {
      clear();
      return false;
    }
    _hasDst = true;
    _dstOffset = _stdOffset + 3600;
    if (*p != ',' && *p != 0) {
      int32_t off = 0;
      if (!parseTime(p, off)) {
        clear();
        return false;
      }
      _dstOffset = -off;
    }
    if (*p == 0) {
      // default rules: second Sunday in March to first Sunday in November
      const char* defaults = ",M3.2.0,M11.1.0";
      p = defaults;
    }
    if (*p++ != ',' || !parseRule(p, _start) || *p++ != ',' ||
        !parseRule(p, _end) || *p != 0) {
      clear();
      return false;
    }
    _valid = true;
    return true;
  }

  /** @brief Returns true if a TZ string was successfully parsed. */
  bool isValid() const { return _valid; }

  /** @brief Returns true if the time zone has summer time rules. */
  bool hasDst() const { return _hasDst; }

  /**
   * @brief Check if summer time is active at the given UTC time.
   * @param utc Seconds since the Unix epoch (UTC).
   */
  bool isDst(int64_t utc) const {
    if (!_hasDst) return false;
    int year = yearOf(utc + _stdOffset);
    int64_t start = transition(year, _start) - _stdOffset;
    int64_t end = transition(year, _end) - _dstOffset;
    if (start < end) return utc >= start && utc < end;
    // southern hemisphere: DST spans the new year
    return !(utc >= end && utc < start);
  }

  /**
   * @brief Get the offset to UTC in seconds (positive is east of UTC) that
   * applies at the given UTC time.
   */
  int32_t offsetSeconds(int64_t utc) const {
    return isDst(utc) ? _dstOffset : _stdOffset;
  }

  /** @brief Get the name of the zone that applies at the given UTC time. */
  const char* name(int64_t utc) const {
    return isDst(utc) ? _dstName : _stdName;
  }

  /**
   * @brief Convert UTC to local time.
   * @param utc Seconds since the Unix epoch (UTC).
   * @return std::tm with the local time, tm_isdst set accordingly.
   */
  std::tm toLocal(int64_t utc) const {
    bool dst = isDst(utc);
    time_t local = static_cast<time_t>(utc + (dst ? _dstOffset : _stdOffset));
    std::tm result = {};
    gmtime_r(&local, &result);
    result.tm_isdst = dst ? 1 : 0;
    return result;
  }

 protected:
  /** Rule describing when a DST transition happens. */
  struct Rule {
    char type = 'M';        // 'M', 'J' or 'N' (plain day number)
    int month = 0;          // M: 1-12
    int week = 0;           // M: 1-5
    int day = 0;            // M: weekday 0-6; J/N: day number
    int32_t time = 7200;    // local seconds after midnight
  };

  bool _valid = false;
  bool _hasDst = false;
  char _stdName[8] = "UTC";
  char _dstName[8] = "";
  int32_t _stdOffset = 0;  // seconds east of UTC
  int32_t _dstOffset = 0;  // seconds east of UTC
  Rule _start;
  Rule _end;

  void clear() {
    _valid = false;
    _hasDst = false;
    strcpy(_stdName, "UTC");
    _dstName[0] = 0;
    _stdOffset = 0;
    _dstOffset = 0;
    _start = Rule();
    _end = Rule();
  }

  static bool isAlpha(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
  }
  static bool isDigit(char c) { return c >= '0' && c <= '9'; }

  /** Parse a zone name; names longer than the buffer are truncated. */
  static bool parseName(const char*& p, char (&out)[8]) {
    size_t n = 0;
    if (*p == '<') {
      p++;
      while (*p && *p != '>') {
        if (n < sizeof(out) - 1) out[n++] = *p;
        p++;
      }
      if (*p != '>') return false;
      p++;
    } else {
      while (isAlpha(*p)) {
        if (n < sizeof(out) - 1) out[n++] = *p;
        p++;
      }
    }
    out[n] = 0;
    return n >= 3 || (n > 0 && out[0] != 0 && p[-1] == '>');
  }

  static bool parseInt(const char*& p, int& value) {
    if (!isDigit(*p)) return false;
    value = 0;
    while (isDigit(*p)) value = value * 10 + (*p++ - '0');
    return true;
  }

  /** Parse [+-]hh[:mm[:ss]] into seconds. */
  static bool parseTime(const char*& p, int32_t& seconds) {
    int sign = 1;
    if (*p == '+' || *p == '-') sign = (*p++ == '-') ? -1 : 1;
    int h = 0, m = 0, s = 0;
    if (!parseInt(p, h)) return false;
    if (*p == ':') {
      p++;
      if (!parseInt(p, m)) return false;
      if (*p == ':') {
        p++;
        if (!parseInt(p, s)) return false;
      }
    }
    seconds = sign * (h * 3600 + m * 60 + s);
    return true;
  }

  /** Parse Mm.w.d, Jn or n, optionally followed by /time. */
  static bool parseRule(const char*& p, Rule& r) {
    if (*p == 'M') {
      p++;
      r.type = 'M';
      if (!parseInt(p, r.month) || *p++ != '.' || !parseInt(p, r.week) ||
          *p++ != '.' || !parseInt(p, r.day))
        return false;
      if (r.month < 1 || r.month > 12 || r.week < 1 || r.week > 5 ||
          r.day > 6)
        return false;
    } else if (*p == 'J') {
      p++;
      r.type = 'J';
      if (!parseInt(p, r.day) || r.day < 1 || r.day > 365) return false;
    } else {
      r.type = 'N';
      if (!parseInt(p, r.day) || r.day > 365) return false;
    }
    r.time = 7200;
    if (*p == '/') {
      p++;
      if (!parseTime(p, r.time)) return false;
    }
    return true;
  }

  static bool isLeap(int y) {
    return (y % 4 == 0 && y % 100 != 0) || y % 400 == 0;
  }

  static int daysInMonth(int y, int m) {
    static const uint8_t d[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    return (m == 2 && isLeap(y)) ? 29 : d[m - 1];
  }

  /** Days since 1970-01-01 for a civil date (proleptic Gregorian). */
  static int64_t daysFromCivil(int y, int m, int d) {
    y -= m <= 2;
    int64_t era = (y >= 0 ? y : y - 399) / 400;
    int64_t yoe = y - era * 400;
    int64_t doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    int64_t doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + doe - 719468;
  }

  /** Year of a (local) time in seconds since the Unix epoch. */
  static int yearOf(int64_t t) {
    int64_t days = t >= 0 ? t / 86400 : -((-t + 86399) / 86400);
    int64_t z = days + 719468;
    int64_t era = (z >= 0 ? z : z - 146096) / 146097;
    int64_t doe = z - era * 146097;
    int64_t yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    int64_t doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    int64_t mp = (5 * doy + 2) / 153;
    int m = static_cast<int>(mp < 10 ? mp + 3 : mp - 9);
    return static_cast<int>(yoe + era * 400 + (m <= 2));
  }

  /** Local wall clock seconds since the Unix epoch when a rule fires. */
  static int64_t transition(int year, const Rule& r) {
    int64_t days;
    if (r.type == 'M') {
      int64_t first = daysFromCivil(year, r.month, 1);
      int wday = static_cast<int>(((first + 4) % 7 + 7) % 7);  // 1970-01-01 = Thu
      int day = 1 + (r.day - wday + 7) % 7 + (r.week - 1) * 7;
      while (day > daysInMonth(year, r.month)) day -= 7;
      days = first + day - 1;
    } else if (r.type == 'J') {
      // day 1-365, February 29 is never counted
      days = daysFromCivil(year, 1, 1) + r.day - 1 +
             ((isLeap(year) && r.day >= 60) ? 1 : 0);
    } else {
      days = daysFromCivil(year, 1, 1) + r.day;
    }
    return days * 86400 + r.time;
  }
};

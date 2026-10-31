/**
 * @file TinyTimeZoneDB.h
 * @brief Small dictionary that maps city names (e.g. "Europe/Zurich") to POSIX
 * TZ strings (e.g. "CET-1CEST,M3.5.0,M10.5.0/3").
 */

#pragma once
#include <cstddef>

/**
 * @struct TinyTimeZoneEntry
 * @brief A city and the POSIX TZ string of its time zone.
 */
struct TinyTimeZoneEntry {
  const char* city;  ///< IANA name, e.g. "Europe/Zurich"
  const char* tz;    ///< POSIX TZ string, e.g. "CET-1CEST,M3.5.0,M10.5.0/3"
};

/**
 * @class TinyTimeZoneDB
 * @brief Dictionary of cities and their POSIX TZ strings. The entries are
 * provided by the caller as an array, which is not copied: it must stay valid
 * for the lifetime of the object (e.g. a global const array).
 *
 * A city can be looked up with the full name ("Europe/Zurich") or just the
 * city part ("Zurich"). The search ignores the case and treats a space like
 * an underscore.
 */
class TinyTimeZoneDB {
 public:
  /**
   * @brief Construct the dictionary from an array.
   * @param entries Array of entries, e.g. `const TinyTimeZoneEntry cities[]`.
   */
  template <size_t N>
  TinyTimeZoneDB(const TinyTimeZoneEntry (&entries)[N])
      : _entries(entries), _size(N) {}

  /**
   * @brief Construct the dictionary from a pointer and a size.
   * @param entries Pointer to the first entry.
   * @param size Number of entries.
   */
  TinyTimeZoneDB(const TinyTimeZoneEntry* entries, size_t size)
      : _entries(entries), _size(size) {}

  /** @brief Number of entries. */
  size_t size() const { return _size; }

  /** @brief Get the entry at the given index (0 <= index < size()). */
  const TinyTimeZoneEntry& operator[](size_t index) const {
    return _entries[index];
  }

  /**
   * @brief Find the POSIX TZ string of a city.
   * @param city Full name ("Europe/Zurich") or city part ("Zurich").
   * @return The POSIX TZ string or nullptr if the city is not in the list.
   */
  const char* find(const char* city) const {
    if (city == nullptr) return nullptr;
    // a full name wins over a match of the city part only
    for (size_t i = 0; i < _size; i++) {
      if (equals(_entries[i].city, city)) return _entries[i].tz;
    }
    for (size_t i = 0; i < _size; i++) {
      const char* part = cityPart(_entries[i].city);
      if (part != _entries[i].city && equals(part, city)) return _entries[i].tz;
    }
    return nullptr;
  }

 protected:
  const TinyTimeZoneEntry* _entries;
  size_t _size;

  /** The text after the last '/' of an IANA name. */
  static const char* cityPart(const char* name) {
    const char* result = name;
    for (const char* p = name; *p; p++) {
      if (*p == '/') result = p + 1;
    }
    return result;
  }

  static char normalize(char c) {
    if (c >= 'A' && c <= 'Z') return c - 'A' + 'a';
    return c == ' ' ? '_' : c;
  }

  static bool equals(const char* a, const char* b) {
    while (*a && *b) {
      if (normalize(*a++) != normalize(*b++)) return false;
    }
    return *a == *b;
  }
};

// Time zone helpers of the command line tool: look up a time zone by name and
// find the time zone of this computer.
#pragma once
#include <string>
#include <vector>

/// A time zone of the table: IANA name and POSIX TZ string
struct Zone {
  std::string name;  // e.g. Europe/Zurich
  std::string tz;    // e.g. CET-1CEST,M3.5.0,M10.5.0/3
};

/// The time zone of this computer
struct LocalZone {
  std::string name;      // IANA name, empty if not known
  std::string nameFrom;  // where the name comes from
  std::string tz;        // POSIX TZ string, empty if not found
  std::string tzFrom;    // where the string comes from
  int samples = 0;       // number of times that the string was compared with the system
  int differences = 0;   // ...and how many of them were different
  std::vector<std::string> sameRules;  // other zones of the table with this string
  std::string tableTz;   // the string of the table for this zone, if it is another one
};

/// Looks for zones by name (Europe/Zurich), by city (zurich) or by a part of the
/// name (case is ignored). A name or city that matches wins over a part.
/// An empty text returns all zones.
std::vector<Zone> findZones(const std::string& text);

/// Finds the POSIX TZ string for an argument of -z: "local" (this computer), a
/// POSIX TZ string, or a name that identifies exactly one zone.
/// Returns false and an error message if this is not possible.
bool resolveZone(const std::string& arg, std::string& tz, std::string& error);

/// Finds the time zone of this computer: from the TZ variable, /etc/localtime
/// and the zoneinfo files, and by comparing the local time of this computer
/// with the zones of the table. The result is checked against the local time.
LocalZone findLocalZone();
